# l82 SDK prep: CP bulk register writes

Implementation by an Opus subagent on 2026-11-08 as SDK commit `c40db4b` on local branch `mcla/m11-l82` (parent `2aff82e`; ported in part from Xenia Canary, BSD-3, with attribution); cvar `cp_bulk_register_writes` default off; accepted by the primary agent as code: ctest 2195/2195 (4 skipped), 0 new warnings; not installed (goes into a later owner build with a host key and an A/B arm). Estimated saving (unmeasured): PC 0.3-0.8 ms/f, Deck 1-2.5 ms/f of `packets_other`.

---

# l82 SDK: CP bulk register writes (cvar cp_bulk_register_writes) - evidence draft

Date: 2026-11-08. Branch `mcla/m11-l82` (worktree `private/l82-sdk-wt`) from `mcla/m11-l81` 2aff82e.
Build `private/l82-sdk-b`, output `private/l82-sdk-o`; no install. Scripts:
`%TEMP%/claude/C--BDU-MCLA-Recomp/l82-scripts` (l81 scripts with paths changed).

## Problem

Deck l79 default arm: `packets_other` 10.8 ms/f (docs/evidence/M11-l79-deck-ab-20261108.md); PC 2.2-2.7 ms/f
(docs/evidence/M11-cp-cost-plan-20261106.md 2.4). The D3D12 `WriteRegistersFromMem` already copies whole
float/bool-loop/fetch constant ranges in bulk (Canary-derived), but every other range (SET_CONSTANT type 4
"REGISTERS" = context registers 0x2000+, type-0 ranges, SET_CONSTANT2 ranges that are not wholly inside one
constant block) fell back to `CommandProcessor::WriteRegistersFromMem`: one virtual `WriteRegister` per dword
(D3D12 override: fetch-range compare, base call: volatile store, static known-register table, scratch check,
switch; back in D3D12: three range checks).

## Change

- `include/rex/graphics/util/bulk_register_writes.h` (new, pure): `BulkRegisterWriteTable` (one byte per
  register, built once: single if base special - SCRATCH_REG0..7, COHER_STATUS_HOST, DC_LUT_RW_INDEX..DC_LUT_30_COLOR
  - or inside a backend range, and (second table) also if unknown to `RegisterFile::GetRegisterInfo`;
  indices >= kRegisterCount always single). FINDING: only 171 of the 2048 context registers 0x2000-0x27FF are in
  the register table; the per-register path logs writes to unknown ones at debug level (REXGPU_DEBUG, gated by
  should_log). The D3D12 caller therefore checks the GPU logger's debug level per range: debug off (normal) ->
  unknown registers are plain; debug on -> the table that sends them through WriteRegister (log identical). and `WriteRegisterRunsBulk`: walks the range in order, copy_and_swap
  of each maximal run of plain registers into the register file, every single register passed to the
  per-register callback at its position. Approach ported from Xenia Canary (BSD-3;
  `d3d12_command_processor.cc` WriteRegisterRangeFromMem_WithKnownBound / WritePossiblySpecialRegistersFromMem,
  `command_processor.cc` HandleSpecialRegisterWrite; attribution in the header). Difference to Canary: table
  instead of range macros, and unknown registers and constant blocks inside mixed ranges stay on the old path.
- `D3D12CommandProcessor::WriteRegistersFromMem`: after the three existing pure-constant-block branches
  (unchanged), with `cp_bulk_register_writes` latched true the range goes through `WriteRegisterRunsBulk` with
  the D3D12 constant blocks as backend single ranges and the virtual `WriteRegister` as the callback. With the
  cvar false the old path runs (only `regs_single += n` added).
- cvar `cp_bulk_register_writes` (bool, default false, GPU/D3D12, hot reload; latched with
  `LatchFetchIdentityOptions` at setup and every frame start).
- Perf split (MCLA_PERF_SPLIT v=1, appended after smp_memo_miss): `regs_bulk` (dwords stored by bulk copies),
  `regs_single` (range dwords through the per-register path; all range dwords outside pure constant blocks when
  the cvar is off), `bulk_runs` (bulk copies). Type-0 one-register packets, type-1 and direct WriteRegister calls
  are not counted; pure constant-block ranges (already bulk) are not counted.
- Vulkan backend unchanged (not built in MCLA).

## Exactness argument

Side effects exist only for: base special registers, D3D12 constant blocks (dirty bits, M11-016 compare),
unknown registers (debug log), extended indices. All of those take the identical per-register call. Plain
registers have no effect beyond the store. Runs are processed strictly in order, so every side effect sees the
same register file as on the old path (e.g. scratch write-back reading SCRATCH_UMSK/ADDR written earlier in the
same range). Pure constant-block ranges keep their existing bulk branches. The non-volatile copy matches the
existing constant bulk copies.

## Tests

`tests/unit/graphics/bulk_register_writes_test.cpp` (6 cases, tag [bulk-regs]): model processor with a test hook
recording each side-effect call (index, value, previous value, FNV hash of the whole register file at the call);
COHER and gamma-LUT side effects mutate the model register file; synthetic PM4 streams (type-0 ranges and
one-reg, SET_CONSTANT types 0-4, SET_CONSTANT2) through both paths; asserts identical register files, extended
registers, call sequences, and counter partition. Table test: every reference side-effect register is single;
extra singles only between the gamma LUT ports. Perf-split field order test.

RESULTS: commit c40db4b on mcla/m11-l82 (parent 2aff82e). Release build l82-sdk-b OK; normalized warning set
identical to l81 (219 lines, diff empty; d3d12 command_processor.cpp warnings unchanged apart from line shifts).
ctest: 100% passed, 0 failed of 2199 (2195 passed + 4 skipped BitStream; baseline 2189 + 4 = 2193; +6 new).
git-clang-format on changed lines: clean (the pre-existing unformatted parts of the d3d12 files were left alone).
First test run caught the unknown-register finding (628 bulk vs 8779 single dwords for random context ranges with
unknown registers treated single) -> led to the logger-level split above.

## Estimate (not measured)

No packet census of range dwords exists yet; the counters above provide it. Plan 2.4 estimate stands:
PC -0.3 to -0.8 ms/f of 2.2-2.7 ms/f packets_other; Deck -1 to -2.5 ms/f of 10.8 ms/f. Owner A/B
(cp_bulk_register_writes true vs false, perf split on) required; compare packets_other_ms and regs_* per frame.
