# M10-002 steps 0 and 1: read-back counters, GPU timing and exact elision of unread GPU copies (l58, 2026-11-01)

Status: **SDK and host implemented, built and unit-tested; not run in the
game.** Implemented by an Opus subagent for the primary agent, following
[M10-002-readback-design-20261101.md](M10-002-readback-design-20261101.md)
sections 3.1 and 6 (steps 0 and 1). The behaviour switch
`d3d12_readback_elide_unread` defaults **off**; the `MCLA_READBACK v=1`
counters and the GPU read-back timestamps are always on. No physical result
exists; every gain below is still the design estimate.

## 1. Artifacts

- SDK: repository `private/l39-sdk-src`, new local branch `mcla/m10-readback`
  from `mcla/m10-cpu-waits` (`62cd66d` = v0.20.0.6 + the l57 CPU waits),
  linked worktree `private/l58-sdk-wt` (submodule trees copied from
  `private/l57-sdk-wt`, gitlinks identical to `62cd66d`). One local commit
  **`141e13f72580066513db36efdd956da1245df4b2`**, author Bolodymyr, **not pushed**. 5 files, 985 insertions, 80 deletions. SDK ctest **1978/1978** (the 1970 baseline plus the 8 new `[readback-slots]` cases); the tagged suites `[perf-split]`, `[pipeline-wait]`, `[livery-diag2]`, `[wrm]`, `[replay-wake]` and `[readback-slots]` pass. Build warnings: 503, the identical set of the l57 SDK build (`warn.py` comparison, line numbers ignored); no new warning in the touched files. clang-format 20.1.8 clean on the changed lines and the new files; ASCII, LF. A mutation check (elision condition weakened to `next_count + 0 >= 4`) makes the equivalence tests fail; the source was restored and rebuilt before the commit.
- SDK files: new `include/rex/graphics/util/readback_slots.h` (pure slot
  state machine, counters, report line) and
  `tests/unit/graphics/readback_slots_test.cpp`; changed
  `src/graphics/d3d12/command_processor.cpp`,
  `include/rex/graphics/d3d12/command_processor.h`, `tests/unit/CMakeLists.txt`.
- SDK build: `private/l58-sdk-b` (output `private/l58-sdk-o`, install
  `private/l58-sdk-install`), the l57 configuration (Ninja Multi-Config,
  clang 20.1.8, `-march=x86-64-v3`, D3D12 only, Tracy and perf counters on).
  Scripts and logs: `private/l58-sdk-work/`.
- Host: main `3170991` + this patch (`src/mcla_dev_overrides.h`,
  `tests/mcla_dev_overrides_test.cpp`, this document), built from a scratch
  copy of the patched tree into `private/l58-build` against
  `private/l58-sdk-install` with the l57 options
  (`MCLA_TEST_BUILD_EXPIRY`, `MCLA_RAW_TIMER_DEFAULT_ON`,
  `MCLA_PERF_SPLIT_DEFAULT`, `MCLA_LIVERY_DIAG2_DEFAULT`,
  `MCLA_READBACK_SYNC_COMPOSITE_DEFAULT`, `MCLA_DEV_OVERRIDES` ON). Scripts
  and logs: `private/l58-ab-work/`.
- Candidate input `private/l58-int-input` (seven files) and
  `private/l58-internal-prepare.ps1` (from `l57-internal-prepare.ps1`: V1
  ticket, pins, staging to `private/l58-internal`, internal audit, KI-045
  admission row for l58). Not run; nothing staged or signed.

## 2. Step 1 design: the exact elision condition

The decision logic of `IssueCopy_ReadbackResolvePath` now lives in
`readback_slots.h` (`PlanReadbackResolve`, `NoteReadbackSlotWrite`,
`CommitReadbackResolve`) and is evaluated **before** the GPU copy is
recorded. Its inputs are all known there: the mode (now read once per
resolve), whether the previous slot `1 - w` is usable (present, mapped, large
enough; the allocation of slot `w` does not touch it), whether the l55 sync
rules force the resolve (`ShouldForceReadbackSync` with `cache_miss = false`;
the rules were already evaluated before the entry is touched), the per-key
counter and the switch.

```
w           = current_index
miss        = !(buffers[1-w] && len <= sizes[1-w] && mapped[1-w])
forced      = sync rules match && !miss            (fast/some only)
own         = miss || forced
copies_now  = own || resolves_since_copy + 1 >= 4  (some; fast/full: always)
next_count  = copies_now ? 0 : resolves_since_copy + 1
guard       = !own && copies_now && slot_elided[1-w]   (never in constant some)
own        |= guard
record_gpu  = !(mode == some && elide) || own || next_count + 1 >= 4
```

**Elided exactly when:** mode `some`, switch on, not a miss, not forced, and
`next_count + 1 < 4`, i.e. the next resolve of the same key will not take
its every-4th copy from this slot. In steady play that is 3 resolves in 4:
counters 1 and 2 (deferred) and the every-4th copy itself (which reads the
*previous* slot, and leaves the counter at 0); only the resolve that leaves
counter 3 records.

Skipped for an elided resolve: the downscale descriptors' contents, the UAV
barrier and transitions of the scaled range, the downscale dispatch, the
downscale-buffer transitions, the slot copy and the barrier submissions
(scaled); `UseAsCopySource`, its barrier submission and the slot copy
(unscaled). Kept: every early-exit check and allocation before the recording
(slot allocation and mapping, downscale pipeline and format checks, downscale
buffer growth, the scaled buffer lookup and the descriptor request), so every
early exit happens exactly as before; the counter, `current_index`, the miss
test, the waits, the sync counters and the guest copy. The GPU state after an
elided resolve is the state `readback_resolve=none` leaves after every
resolve.

Proof sketch. The GPU copy of resolve N into slot `w` is read only (a) by N
itself when it reads its own slot (miss, forced, full; always recorded) or
(b) by the next resolve N+1 of the key, which writes `1-w` and reads `w` as
its previous slot only when it is neither a miss nor forced and its counter
reaches the interval, i.e. only if `next_count + 1 >= 4` (recorded). A miss
or a forced N+1 reads its own slot; a deferred N+1 reads nothing. N+2 writes
slot `w` again (recorded or elided, covered by the same argument by
induction), so no later resolve can see N's slot content. An eviction (60
frames unused, or the 256-key cap) drops both slots, making the next resolve
a miss; a failed slot creation or mapping makes the next resolve a miss
exactly as before because the presence and mapping test is unchanged. A
resolve that exits early does not flip `current_index` (as before), so the
next one rewrites the same slot. The guard covers the only case outside the
proof, a run-time switch of `readback_resolve` from `some` to `fast` between
two resolves of a key: such a resolve would read an elided previous slot; it
is turned into a miss before recording (record, wait, read the own slot) and
counted as `guard_misses`.

Unit tests (`[readback-slots]`, 8 cases): the steady 1-in-4 pattern; fast,
full and elision-off always record; forced and miss read and record the own
slot; the some-to-fast guard; **a model of the D3D12 path** (slot presence,
mapping and allocation failures, slot contents as resolve ids, guest memory,
eviction at 60 frames and at a key cap of 5 out of 8 keys, keys sharing an
address with different lengths and partially overlapping ranges, forced
resolves, frame gaps) run over 300 random sequences of 4,000 events with
elision off and on: the delivered (step, key, value) sequence, the guest
memory, misses, forced and copies are identical, `guard_misses = 0`, and no
read ever hits an elided slot; a steady 20,000-event sequence elides
between 70% and 76% of the GPU copies (asserted); 100 random sequences with mode switches never read an elided slot
(the guard fires and reads the fresh own slot); the report line format.

## 3. Step 0: counters and GPU timing

One line per pacing minute (next to `MCLA frame pacing/min` and `MCLA_WRM`,
whether or not `d3d12_perf_split` is on), reset after it, and one at every
F10 (`kind=mark`, the current minute so far, not reset):

`MCLA_READBACK v=1 kind=minute|mark reason= frame= frames= mode= elide=0|1
timing=0|1 resolves= aborted= copies_now= copies_deferred= copies_skipped=
misses= guard_misses= forced= full_waits= gpu_copies_recorded=
gpu_copies_elided= bytes_gpu_copied= bytes_gpu_elided= bytes_cpu_copied=
memcpy_ms= readback_wait_ms= gpu_readback_ms= gpu_readback_ms_per_frame=
gpu_blocks= gpu_blocks_dropped=`

- `resolves`: read-back resolves that reached the slot decision (written
  length > 0 and a translatable address); `aborted`: of those, the ones that
  left before the guest copy step (allocation, format, descriptor or wait
  failures).
- `copies_now` / `copies_deferred` / `copies_skipped`: guest copy done / not
  due (some interval) / due but impossible (unmapped slot, no destination);
  the same classification as `MCLA_PERF_SPLIT`.
- `misses` (previous slot unusable), `guard_misses`, `forced` (l55 sync
  forced and waited), `full_waits` (full mode).
- `gpu_copies_recorded` / `gpu_copies_elided` and their bytes (guest
  length): what the GPU was asked to copy into the READBACK heap.
- `bytes_cpu_copied`, `memcpy_ms` (steady_clock around the memcpy),
  `readback_wait_ms` (every queue wait in the path: full, miss, guard and
  forced).
- `gpu_readback_ms`: GPU timestamps around every **recorded** read-back
  block (pending barriers are submitted first so that they are not counted;
  begin before the first barrier of the block, end after the last barrier or
  the copy), summed when their submission completes (attributed to the minute
  of completion); `gpu_blocks` blocks timed, `gpu_blocks_dropped` blocks not
  timed because the 2,048-pair ring was full. Implementation: a separate
  timestamp query heap and READBACK buffer (2 x 2,048 queries, 32 KiB),
  `EndQuery` through the deferred command list (also correct for the
  streaming replay), one `ResolveQueryData` per submission on the command
  list next to the existing submission timestamps, collected in
  `CheckSubmissionFence` like `gpu_busy`. Switch `d3d12_readback_gpu_timing`
  (SDK default **on**, hot reload, not in the host whitelist). Cost: two
  timestamps per recorded block and possibly one extra barrier batch.
  Caveat: timestamps are not a precise cost of overlapping GPU work; the
  block is serialised by its own barriers, so the number is a fair estimate
  of `G_rb`, and the authoritative A/B metric stays `gpu busy/min` and fps.
  The elide arm records fewer blocks, so its `gpu_readback_ms` drops by
  construction.

## 4. Host patch

`src/mcla_dev_overrides.h`: `Key::kReadbackElideUnread` =
`d3d12_readback_elide_unread` (graphics-plugin phase), `kKeyCount` 6;
`tests/mcla_dev_overrides_test.cpp`: six keys, the l58 baseline arm file,
the elide phase, `d3d12_readback_gpu_timing` and a prefix not whitelisted.
Apply (primary, main checkout, copy-only with hash checks):
`powershell -NoProfile -ExecutionPolicy Bypass -File C:\Users\orange\AppData\Local\Temp\claude\C--BDU-MCLA-Recomp\l58-patch\apply.ps1`

Host build (`private/l58-build`, scratch tree
`%TEMP%\claude\C--BDU-MCLA-Recomp\l58-patch\tree` = main 3170991 sources,
verified identical to the main checkout for `src`, `tests`, `cmake`,
`config`, `rules`, `rule-tests`, `utils`, `scripts`, `generated`,
`CMakeLists.txt`, `VERSION`, `sgconfig.yml`, plus the two patched files):
236/236 steps, **0 warnings**. Host tests: 41 executables, 40 pass;
`mcla_dev_overrides_test` 83 checks. `mcla_diagnostic_export_ui_win_test`
fails 1 of 249 checks ("preview shrinks before adding outer scrollbars",
compact layout 993x634); the **l57 binary of the same test, which passed in
the l57 runs, fails the same check now**, so this is the current desktop
state (display/DPI), not this patch; rerun it on the primary's side.
ast-grep 0.50.0 on the scratch tree: scan rc=0, rule tests 3/3; scan of the
touched SDK files rc=0. Logs: `private/l58-ab-work/` (`l58-configure.log`,
`l58-build.log`, `l58-tests.log`, `l58-astgrep.log`).

## 5. Candidate

`private/l58-int-input` (SHA-256; `private/l58-ab-work/l58-int-input.sha256`,
`l58-pins.json`):

| File | SHA-256 |
|---|---|
| MCLA-Launcher.exe | 0EB336957B60631A3EFA0B41ECA2CB318C2FB9DEE94A6261FD216F6BF6CA6E05 |
| mcla.exe | 834A05C0030445E7F8488A588A1FD267C48E300A40A5862C70B504640BECAFE5 |
| mcla_crash_handler.exe | 7DD74220967CBD21B56EFCC2367903F0361608493B61851F1017BA289F7DFC67 |
| mcla_diagnostic_image_helper.exe | C3F2F76D67410D77B37431F6481EBA362D334A22500BA231B7CC117EBE1F2C1A |
| rexgpu-xenos.dll | DD0FC35C6F6443ABE9FD79B1DD36E577B68FA1B8D42706BBD35D849CC44257F8 |
| rexruntime.dll | 1348627CBB21A3356C739051781BEFAC2E32B513DC16DA3BA9B3E5E502324611 |
| TracyClient.dll | 673C4E3E1EEE26B0B7A052EA5846C8595E3D1E2107FFE0EE2A7025065CB408C8 |

The three DLLs are byte-identical in `l58-build` and `l58-sdk-install/bin`.
Path scan (`l58-pathscan.log`): no build path, user path or worktree name in
any file (the only hits are three/four-byte opcode patterns in `mcla.exe`
and the launcher's generic `AppData` strings, as in l57); the KI-045
admission marker is present once. Strings (`l58-strings.log`): `mcla.exe`
holds the override file name and `d3d12_readback_elide_unread`;
`rexgpu-xenos.dll` holds `MCLA_READBACK v={}`, `d3d12_readback_elide_unread`
and `d3d12_readback_gpu_timing`.

`private/l58-internal-prepare.ps1` (SHA-256
DDB7673969BDF29DDFC74B256080A8514FD2B3CDD2F76DC5C9CD02D0C575C56C, parses
with 0 errors) differs from `l57-internal-prepare.ps1` only in its header,
the l58 paths, the pinned hashes, `$expected`/`$expectedRuntime` and the
final hint. Run (owner, PowerShell 7):
`C:\BDU\MCLA-Recomp\private\tools\pwsh\pwsh.exe -NoProfile -File C:\BDU\MCLA-Recomp\private\l58-internal-prepare.ps1`.
It signs a plain V1 ticket, stages `private\l58-internal`, runs the
internal audit and admits the l58 fingerprint (plus the known l39-l42,
l33-sunc, l53, l54 rows; existing rows such as l57 are kept).

Arm files: `l58-ab-baseline.cmd` 9B713F84F818F963FF4DDD616D84DFB2306F146B6B2D63F0A8D47CD5518FF53F,
`l58-ab-elide.cmd` B20A672E3DADB304D9A574D76AF7986F277B5940134E9CD3BDF3E05B50FE69A1,
`l58-ab-all.cmd` 4FAE4A6333C8847D915062E782EBEA37EF18BEC3E2450956EF3ED53325A6BE04,
`l58-ab-clear.cmd` 684391B9EB0153AC0B8E9687BAFD8742D5B4EB0B536EE0B791E03EFDA3FC9AE8.

## 6. Owner A/B (BENCH-C, warm store)

Arm scripts (generated like the l57 ones, ASCII CRLF): `private\l58-ab-baseline.cmd`
(every M10 optimization false, `mcla_readback_sync_composite=true`),
`private\l58-ab-elide.cmd` (only `d3d12_readback_elide_unread=true`, plus
the read-back sync), `private\l58-ab-all.cmd` (all six true),
`private\l58-ab-clear.cmd` (deletes the override file: l58 build defaults,
native refcount/XAPI on, the other switches off, read-back sync on).

1. Run `private\l58-internal-prepare.ps1` (once), CHECK SETUP.
2. **1440p Unlocked**, BENCH-C copy, warm store, the same route per arm
   (stationary, lap 1, lap 2): `l58-ab-baseline.cmd`, session, F10 at the
   end; `l58-ab-elide.cmd`, session, F10. Then the **720p pair** in the same
   order. Optionally `l58-ab-all.cmd` once. `l58-ab-clear.cmd` at the end.
3. Each session log must show `MCLA dev override: d3d12_readback_elide_unread=...`
   with the arm's value.

Read per minute: `MCLA_READBACK`, `MCLA gpu busy/min`, frame pacing,
`MCLA_PERF_SPLIT`, `MCLA_READBACK_SYNC`.

**Invariant (correctness check inside the run):** per minute and per frame,
`copies_now`, `copies_deferred`, `bytes_cpu_copied`, `misses` and `forced`
(and `MCLA_READBACK_SYNC forced`, `MCLA_PERF_SPLIT copies_now/readback_kb`)
must match between the arms within route noise (l53/l54 windows agreed to
about 1%); `guard_misses = 0`; in the elide arm
`gpu_copies_elided / resolves` about 0.74 and `bytes_gpu_copied` about a
quarter of the baseline. A mismatch beyond noise is a failure of step 1
whatever the fps says.

**Expected:** `gpu_readback_ms_per_frame` in the baseline arm measures `G_rb`
(design estimate 2.9-5.5 ms at 1440p); `gpu busy/min` per frame at 1440p
down by about 0.74 x G_rb (estimate 2.1-4.1 ms, 38 to about 42-46 fps, capped
near 49 by the render thread); at 720p GPU headroom only. Pass: invariants
equal, GPU -5% or more at 1440p, police lap and livery (KI-047, KI-033)
clean.

## 7. Risks

- Elided resolves leave the scaled range and shared memory in the state the
  resolve left them (no NON_PIXEL_SHADER_RESOURCE round trip, no
  COPY_SOURCE); this is the state `readback_resolve=none` always uses for the
  GPU consumers, so texture sampling of resolved data is not affected, but it
  is a GPU-side change only visible in game: check scene, shadows, bloom,
  reflections and the livery in both arms.
- The proof assumes the next same-key resolve reads the previous slot only
  through the some-mode rule; a run-time mode switch is covered by the guard
  (counted); any other unmodelled reader of `rb.buffers` would break it (none
  exists in the SDK: memexport uses its own map).
- `d3d12_readback_gpu_timing` is on by default in this SDK (both arms): two
  timestamps and possibly one extra barrier batch per recorded block. If the
  baseline arm is slower than l57 beyond noise, set
  `d3d12_readback_gpu_timing=false` in the config file for a control run.
- GPU timestamps around serialised blocks approximate the block cost; with
  overlap they can under- or over-count. Use `gpu busy` and fps as the
  verdict.
- Dev SDK, unpinned, local branch not pushed; volunteer builds are not
  affected (the switch is off by default and the whitelist exists only with
  `MCLA_DEV_OVERRIDES`).
- The Vulkan path (Deck) is unchanged; step 1 there is a separate port
  (design section 3.6).

## Primary verification (2026-11-01)

- Patch applied byte-exact to main (hashes match); `private/l57-host-build` rebuilt from main with 0 warnings; `mcla_dev_overrides_test` 83 checks pass; `git diff --check` and ast-grep clean.
- One environment-dependent failure: `mcla_diagnostic_export_ui_win_test` "preview shrinks before adding outer scrollbars" (1 of 249; compact layout 993x634, vertical 579/578). The l52 and l56 binaries of the same test, which passed when they were built, now fail identically, so the cause is the current desktop state (display or scale), not this change. Recheck at the next build on a normal desktop.
