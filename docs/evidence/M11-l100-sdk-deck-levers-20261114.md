# l100 SDK + host: Deck command-processor levers and the guest spin census (2026-11-14)

Implemented by an Opus subagent for the primary agent, from the Deck census in
[l96 Deck analysis](M11-l96-deck-analysis-20261113.md), the levers in
[Deck CP budget](M11-deck-cp-budget-and-levers-20261112.md), the census definitions in
[l96 SDK measurement](M11-l96-sdk-measurement-20261113.md), the register write path in
[l82 bulk registers](M11-l82-sdk-bulk-registers-20261108.md) and item 8 of the
[macXenos survey](M11-macxenos-survey-20261113.md). SDK commit `1e81149`
(`1e81149a750ffb6b8b2d284aa906c4b98a74762f`) on the local branch `mcla/m11-l100`, parent
`d3d7b6c` (l99); worktree `private/l100-sdk-wt`; not pushed. Installed to
`private/l100-sdk-install` (CMake version `0.20.0.10-dev.g1e81149`). **Built and unit-tested
only: nothing has run in the game, there is no physical PASS, and no Deck or PC number in this
document is measured with l100.** Every switch is default off; the two new log lines are always
on. Nothing was taken from GPL forks or LARecomp; no code was ported from Xenia Canary or
macXenos for l100 (the macXenos spin census of commit b0b75d2b is the idea, read only through
the survey, not the code). Spark was not used: deterministic source work checked by unit tests.
Owner build: [l100 owner build](M11-l100-owner-build-20261114.md).

## 0. What shipped and what did not

| Item | Status | Switch (default) | Host key |
|---|---|---|---|
| A. register-write skip | **shipped**, with its verify | `cp_reg_write_skip_unchanged` (false), `cp_reg_write_skip_verify` (false) | `cp_reg_skip`, `cp_reg_skip_verify` |
| B. constants-only draw fast path | **verify only**, no fast path | `cp_draw_const_only_verify` (false) | `cp_draw_fast_verify` (no `cp_draw_fast` key, no `cp-fast` arm) |
| C. guest spin census | **shipped**, always on (host) | - | - |
| C. guest spin backoff | **shipped**, default off (host) | `mcla_guest_spin_backoff` (0) | `guest_spin_backoff_64` |

**Why B is verify-only.** A constants-only fast path would skip the render-target cache
update, the pipeline lookup, the shader-modification lookup, the texture binding refresh and
the primitive setup. Each of those reads state that is not register state: the render-target
cache's EDRAM ownership (changed by resolves and by other draws' updates), the texture cache's
outdated flags and residency (changed by guest CPU writes through the memory watch, by texture
loads and by eviction; `MarkAsUsed` also feeds the LRU), the descriptor heap pages (a new page
forces descriptor rewrites even for unchanged textures), the submission (a new command list
loses every binding) and guest memory (index data). The invariants below are cheap to check, but
whether they are *sufficient* cannot be proven from the source alone in one night, and the l96
IM_LOAD memo showed that a shortcut that looks safe can be wrong (131 mismatches in 3.4 M hits).
So l100 ships the measurement with every comparison the fast path would rely on; a fast path can
follow once a Deck run shows how many draws qualify and that the compared results never differ.

## 1. A: register-write skip (`cp_reg_write_skip_unchanged`)

### 1.1 What is skipped (include/rex/graphics/util/cp_l100_levers.h, `ClassifyRegisterSkip`)

| Class | Registers | Normal per-register path | Skipped when the value is unchanged |
|---|---|---|---|
| `kNever` | `< 0x2000` (scratch write-back, COHER dirty bit, gamma LUT ports, CP / MMIO registers the guest CPU also writes) | side effects / races | **never** |
| `kNever` | fetch constants `0x4800..0x48BF` | texture binding and vertex residency invalidation | **never** (the M11-016 switch `d3d12_fetch_identity_cache`, default on, already does this) |
| `kNever` | `> 0x4927`, `>= kRegisterCount` (extended map) | store / extended map | **never** |
| `kNever` | context registers unknown to the register table while GPU debug logging is on | debug log | **never** |
| `kPlain` | context registers `0x2000..0x3FFF` | store + draw identity census (`DrawStateDirty::NoteWrite`) | yes: no handler in `CommandProcessor::WriteRegister` (src/graphics/command_processor.cpp:531-689 handles only `< 0x2000`) or in `D3D12CommandProcessor::WriteRegister` (src/graphics/d3d12/command_processor.cpp:2251, constants only) |
| `kAluConstant` | float constants `0x4000..0x47FF` | store + census + the stage's float CB out of date when its constant map uses the constant | yes, see 1.2 |
| `kBoolLoopConstant` | bool / loop `0x4900..0x4927` | store + bool/loop CB out of date | yes, see 1.2 |

The table agrees with the l82 side-effect table (`BulkRegisterWriteTable`; unit test "skip
table agrees with the l82 side-effect table": every plain skippable register is an l82 "plain"
register; the only l82 singles that are skippable are the float and bool/loop constants).

### 1.2 Why an unchanged constant write never needs to mark a constant buffer

The float vertex / pixel and bool/loop constant buffers are pure copies of register values:
`UpdateBindings` (src/graphics/d3d12/command_processor.cpp:8249) packs `regs[0x4000 + ...]` for
every bit of the shader's constant map (vertex 8351-8386, pixel 8389-8425) and copies
`regs[0x4900..0x4927]` (bool/loop 8428-8446). A buffer is rebuilt when its `up_to_date` flag is
false. The flag is cleared by (a) a write to a used constant (per register 2251-2300; the bulk
branches 2507-2560 and the l96 branches after them), (b) a constant-map change (8308-8336: the
map is compared with the previous draw's map at every draw, independently of any write), (c)
every frame start (6926-6945: the per-frame upload pool is reset). With the skip on, (b) and (c)
are unchanged and (a) still fires for every write that changes a value. A write that leaves the
value unchanged leaves the bytes a rebuild would produce unchanged, so the bound buffer stays
correct. It could only fail through a register store that bypasses the write path (then the next
same-value write would have been the one marking the buffer). Two bypasses exist:
`RestoreRegisters` with `execute_callbacks=false` (trace playback only) and
`GraphicsSystem::WriteRegister` (guest MMIO, registers below 0x4000 only, never constants). The
verify detects any other.

Mechanics: `CommandProcessor::WriteRegisterSkipUnchanged`
(include/rex/graphics/command_processor.h:302) replaces the virtual `WriteRegister` call of
type-0 one-register packets (src/graphics/command_processor.cpp:1035), type-1 packets
(1056-1057), the base range loop (695) and the l82 bulk single callback
(src/graphics/d3d12/command_processor.cpp:2645); it compares with the register file through a
byte table pointer (`register_skip_table_`, null while off). The bulk constant branches
(2507-2560) store with `StoreConstantsCompare` and mark a buffer only for the changed dword span
(conservative: unchanged dwords inside the span still count). Latched at every frame start with
the other CP switches (`LatchL100Options`, 2359; the GPU debug-log state chooses the table).

Known side effect: with the skip on, skipped writes do not reach the l96 draw identity census, so
`draws_state_same` / `draws_const_only` see fewer writes (`state_changed`, used by B, is
unaffected: it tracks value changes only). `alu_dw_written` excludes the per-register skipped
writes (they are in `const_dw_skipped`).

### 1.3 Verify (`cp_reg_write_skip_verify`, wins over the skip)

The normal path always runs. Two checks:
- **Side state:** a write the skip would have skipped (`register_write_would_skip_`) must leave
  the register value and `L100SideState` (2318: system / fetch / descriptor-index CB flags,
  gamma table flags, frame open, root parameter bits) unchanged, else `reg_side_mismatch`.
- **Constant contents:** `ConstantSkipVerifier` keeps a shadow of the out-of-date state the
  skip would have: every reason (a)-(c) above marks it, except would-skip writes and, in the
  bulk branches, the unchanged dwords. At every upload of the three buffers (8380, 8419, 8439)
  the bytes are compared with the previous upload of that buffer when the shadow says "up to
  date": equal = `cb_saved` (an upload the skip saves), different = `cb_mismatch` (the skip would
  have drawn with stale constants). The upload heap is never read back: the bytes are gathered
  again from the register file into a scratch buffer.

Unit tests prove the model: random write / map / frame streams in normal, skip and verify mode
bind the same constants at every draw, `cb_mismatch == 0` and `cb_saved` equals the uploads the
skip avoided; a store that bypasses the write path is caught (`cb_mismatch == 1`).

## 2. B: constants-only draw measurement (`cp_draw_const_only_verify`)

`DrawFastVerifier` runs at the draw identity point (after `UpdateBindings`, 5235-5277, before
the l96 census resets its state). A draw **qualifies** when none of these rejection reasons hold
(the first one in this order is counted):

| Reason | Invariant | Hook |
|---|---|---|
| `first` | a previous identity draw exists | `Reset` when the verify is turned on |
| `state` | no value change of a draw-state register (0x2000..0x3FFF without the 4 draw-packet registers) | l96 `draw_identity_.dirty.state_changed` |
| `shader` | same active vertex and pixel shader | `active_vertex_shader()` / `active_pixel_shader()` |
| `fetch` | no fetch-constant invalidation (texture / vertex bindings) | `InvalidateFetchDwords` (2396) sets `draw_fast_fetch_changed_` |
| `copy` | no resolve between the draws (render-target ownership, scaled resolve, read-back) | `++draw_fast_copy_epoch_` in `IssueCopy` (5806) |
| `submission` | same submission (a new command list loses the bindings) | `submission_current_` |
| `initiator` | same primitive type, index format / endianness, major mode | `IssueDraw` arguments |
| `memexport` | neither this nor the previous draw uses memexport | `memexport_used` |
| `textures` | no texture became outdated and every used binding is in sync | `TextureCache::TexturesNeedRequest` (include/rex/graphics/pipeline/texture/cache.h:127), read before `RequestTextures` (5156) |

For a qualifying draw the full path's results are compared with the previous draw's
(mismatch kinds): `pso` (pipeline handle, `ConfigurePipeline` 5103), `root` (root signature),
`rt` (bound render-target bits, formats and the render-target cache key, `Update` 5074), `vs` /
`ps` (shader translations, i.e. the shader modifications, 5062-5090), `prim` (host primitive
type, host vertex shader type, tessellation mode, index buffer type, host index format and
endian, primitive reset; `Process` 5042), `tex_desc` (the bindings phase still wrote a texture or
sampler descriptor; a fast path would have to keep that part, e.g. after a descriptor-heap page
change).

Sub-steps a fast path could skip if `pso/root/rt/vs/ps/prim` stay 0: the shader-modification and
translation lookups (5062-5090), `ConfigurePipeline` and the async-pipeline checks (5103-5150),
the render-target `Update` (5074; only while `copy` = 0, since resolves change EDRAM ownership).
Not skippable: `primitive_processor_->Process` (reads index data and the per-draw `VGT_DMA_*`
range), the vertex-buffer residency loop (5285-5334), the constant buffer update, the
fixed-function state and system constants (5212-5221, cached on their own), the draw call. The
texture refresh (`RequestTextures` 5158) is a no-op in its base part when `textures` = 0, but the
D3D12 part marks every used texture as used (LRU) and pushes its transition barrier on every draw
(src/graphics/d3d12/texture_cache.cpp:770-833); a fast path would have to keep at least the
`MarkAsUsed` (eviction) and could drop the barrier only while the texture's resource state cannot
have changed (no copy, no load: the `textures` and `copy` reasons).

## 3. MCLA_CP_L100 v=1 (rexgpu-xenos.dll, CP thread)

After each MCLA_CP_PACKETS line (same reason and seq; perf split on, which the owner builds
default to), deltas of the window:

`MCLA_CP_L100 v=1 reason=<r> seq=<n> reg_skip=<0|1> reg_skip_verify=<0|1>
draw_fast_verify=<0|1> regs_skipped= const_dw_skipped= cb_uploads= reg_would_skip=
reg_side_mismatch= cb_saved= cb_mismatch= draws_eval= draws_qualify=
rej=first:N,state:N,shader:N,fetch:N,copy:N,submission:N,initiator:N,memexport:N,textures:N
mm=pso:N,root:N,rt:N,vs:N,ps:N,prim:N,tex_desc:N`

- `regs_skipped`: per-register writes skipped (skip mode); `const_dw_skipped`: of them the
  constant dwords, plus the unchanged dwords of bulk constant ranges (not allowed to mark a
  buffer); `cb_uploads`: the window's constant-buffer uploads (the MCLA_PERF_SPLIT field, all
  five kinds), the number the skip should lower.
- `reg_would_skip`, `reg_side_mismatch`, `cb_saved`, `cb_mismatch`: the verify (1.3).
- `draws_eval` / `draws_qualify` / `rej=` / `mm=`: B (section 2); `rej` sums to
  `draws_eval - draws_qualify`. All 0 while `draw_fast_verify=0`.

And at setup and at every change: `MCLA_CP_L100 v=1 kind=config reg_skip= reg_skip_verify=
draw_fast_verify= draw_fast=0 frame=` (`draw_fast=0`: no fast path in this build).

Expected on a clean Deck cp-verify run: `cb_mismatch=0`, `reg_side_mismatch=0`, every `mm=` kind
except `tex_desc` 0; `cb_saved` is the estimate of the uploads cp-reg removes (l96: 11.9 k
uploads per frame; 66 % of the ALU dwords were unchanged).

## 4. C: guest spin census and backoff (host: src/mcla_guest_spin.h, src/mcla_perf_split.cpp)

**Where the spin is.** The PPC hint `db16cyc` has no runtime intrinsic: the SDK code generator
emits it inline as `simde_mm_pause()` (rexglue-sdk src/codegen/builders/system.cpp:51-55,
`build_db16cyc`). The generated title code has 13 of them: 8 in `sub_82412F98` (the GPU-serial
poll step named in the Deck budget: `li r11,4` iterations of 8 hints = **32 hints per call**,
then one read of the GPU position; generated/default/mcla_recomp.27.cpp:7402), 1 each in
`sub_823EB3E0` and `sub_823EDE38` (mcla_recomp.25.cpp), 2 in `sub_8242F870`
(mcla_recomp.28.cpp), 1 in `sub_8261ED20` (mcla_recomp.48.cpp). Counting inside the intrinsic
would need a code generator change and a regeneration of all title code, and it is too hot by
the brief's rule (below). So the census counts in the host's existing strong override of
`sub_82412F98` (src/mcla_perf_split.cpp); the other four functions are not counted.

**Micro-benchmark** (host test binary on the owner PC, best of 5 x 20 000 polls; printed by
mcla_perf_split_test, not a pass/fail check): 32 pauses 413.5 ns per poll; a thread-local
increment per hint +1.49 % (above the 1 % budget: rejected, besides needing regeneration); the
census per poll (two TSC reads, the backoff state, three relaxed stores) +0.52 % (accepted).
Not measured on the Deck (`pause` latency differs by CPU).

**Census (always on).** Per guest thread (64 slots, registered at its first poll): polls,
yields and TSC ticks inside the poll. One line per perf-split window (minute and F10, from the
existing MCLA_THREAD_CPU worker) for the top 3 threads by hints:
`MCLA_SPIN v=1 reason=<r> seq=<n> thread=<tid> polls=<n> hints=<n> yields=<n> est_ms=<ms>
backoff=<N>` (hints = 32 x polls; est_ms = TSC time inside the polls at the window's TSC rate),
or `MCLA_SPIN v=1 reason=<r> seq=<n> thread=none backoff=<N>`. Thread ids are those of the
MCLA_THREAD_CPU lines.

**Backoff (`mcla_guest_spin_backoff`, host int 0..1048576, default 0).** `SpinBackoff`: polls
less than 65536 TSC ticks apart (about 23 us at the Deck's 2.8 GHz) are one wait; after N
consecutive hints of one wait the thread calls `SwitchToThread()` (Wine: `sched_yield`) before
its next poll and the count restarts. N = 64 yields before every second poll after the first two.
The yield adds a scheduling point; it cannot make the guest see the fence earlier, and with other
runnable threads it can delay the render thread by up to a time slice: hence default off and a
Deck A/B arm (4 cores; also the PC's 35 W cap).

## 5. Tests, build, install

- SDK `tests/unit/graphics/cp_l100_levers_test.cpp`, 10 cases (`[l100]`): classification
  (every base side-effect register, fetch, control, extended, unknown with / without debug log);
  the table against the l82 table (0x800 float, 0x28 bool/loop, 0x2000 plain without debug log);
  the skip predicate; `StoreConstantsCompare` against a reference (500 random ranges); the
  verifier states (stale, saved, mismatch, sizes, independent kinds, zero-size uploads); the
  constant model (40 seeds x 4000 steps: same constants at every draw in normal / skip / verify,
  `cb_mismatch == 0`, `cb_saved == normal - skip uploads`); the hidden-store model (caught); B
  rejection order and the reference update after a rejected draw, memexport on both sides,
  `Reset`; B mismatch kinds, `tex_desc` per draw, no mismatch on rejected draws; both line
  formats, no wrap on counters that go back.
- Host `tests/mcla_perf_split_test.cpp` (mcla_guest_spin.h): the backoff state machine (off,
  the N = 64 pattern, N = 100, a gap, a TSC step back, exactly the gap), census / window / lines
  (order, ties, deltas, top limit, slot overflow), the micro-benchmark.
- SDK build: fresh `private/l100-sdk-b` (the l99 options; base d3d7b6c built first, then
  incremental), output `private/l100-sdk-o`. First edit build: compile errors only in the
  register names outside `rex::graphics` (new test; file-scope static_asserts), fixed by
  qualification. Normalized warning set 218 entries, **0 new, 0 gone** against l99.
- `git-clang-format` against `d3d7b6c` (include, src, tests): applied (wrapping in the new header,
  the test and command_processor.cpp), rebuilt, then clean.
- ctest: **100 % passed, 0 failed of 2298 (2294 passed + 4 skipped BitStream; l99 2284 + 4;
  +10 new)** before the format, after it and after the install.
- Install `private/l100-sdk-install`, CMake `0.20.0.10-dev.g1e81149`:

| bin file | Bytes | SHA-256 |
|---|---|---|
| rexruntime.dll | 11680768 | D2941A85E23D848C592C628BB148B845E715AD4C8E61B40381B01CD250A9FB41 |
| rexgpu-xenos.dll | 3576832 | 10630A1B73273A00060C2CB1BFCDA5A6D0338AFB01A024EA3CF8786285911996 |
| TracyClient.dll | 246784 | F7898FA5A0FA54ED579A3DABAFE9E71000595322E1C939EA17CB5C2DD63E6EE8 |
| rexglue.exe | 4113920 | 0369144A4387B88FBF04F8D8D62F8418BC9205A902AA90FAA5AA7647C885062E |

File set against `l99-sdk-install`: 1237 -> 1238 files; added
`include/rex/graphics/util/cp_l100_levers.h`; none removed; 25 differ (the 4 bin files, 3 changed
public headers, `rex/version.h`, the 2 CMake config files, 12 static libraries, 3 `.pc` files).
rexruntime.dll keeps its size, rexgpu-xenos.dll grows by 18432 bytes; TracyClient.dll and
rexglue.exe have no source change (fresh build tree, as before).

**ABI:** `CommandProcessor`, `D3D12CommandProcessor` and `TextureCache` grew (installed headers):
a host must be rebuilt against this SDK (it is, for l100). The host's new code uses no new SDK
symbol, so the main checkout also builds against the gate SDK (l67).

SDK diff (`git show --numstat 1e81149`): 8 files, 1348 insertions, 5 deletions:
`include/rex/graphics/command_processor.h` +31, `include/rex/graphics/d3d12/command_processor.h`
+27, `include/rex/graphics/pipeline/texture/cache.h` +7,
`include/rex/graphics/util/cp_l100_levers.h` +438 (new), `src/graphics/command_processor.cpp`
+4 -4, `src/graphics/d3d12/command_processor.cpp` +304 -1, `tests/unit/CMakeLists.txt` +1,
`tests/unit/graphics/cp_l100_levers_test.cpp` +536 (new).

## 6. Not done / risks

- No game run: the skip's saving, the share of qualifying draws, the census cost on the Deck and
  the backoff's effect are all unobserved. cp-reg must follow a clean cp-verify run.
- The A verify proves the constant buffers' contents and the side-state table on the paths a run
  exercises; it does not cover trace playback (`RestoreRegisters` without callbacks).
- B: no fast path; its comparisons cover the results a fast path would reuse, not the work it
  would skip inside them (render-target ownership transfers inside `Update` are covered by the
  `copy` rejection only, not compared).
- C: only `sub_82412F98` is counted (4 other functions with db16cyc are not); the 65536-tick gap
  is a fixed TSC count, not calibrated.
- Scratch folder `%LocalAppData%\Temp\claude\C--BDU-MCLA-Recomp\l100-work`: `copy_tp.ps1`,
  `sdk-build.ps1`, `sdk-test.ps1`, `sdk-install.ps1`, `edit_sdk.py` (and two fix-ups by inline
  script: the test namespace, the qualified static_asserts), `sdk/` (the new header and test),
  `normwarn.py`, `fileset_diff.py`, `sdk-commit-msg.txt`; logs `copy_tp.log`,
  `sdk-configure.log`, `sdk-build0.log` .. `sdk-build3.log`, `w100all.txt`, `ctest1.log`,
  `ctest2.log`, `sdk-install.log`, `ctest-install.log`, `fileset_diff.log`.
