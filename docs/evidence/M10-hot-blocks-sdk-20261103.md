# M10: hot blocks (watch step 3) in the SDK, 2026-11-03

Implementation by an Opus subagent on local SDK branch `mcla/m10-hotblocks` (`ff30a86` on v0.20.0.7 `43577c8`); accepted by the primary agent as code: ctest 2015/2015, model test with zero stale GPU reads over 40 seeds x 1,500 frames. Status: not run in the game; an independent review of the diff precedes the owner build, as for the census change.

---

# M10 Deck watch/upload path: step 3 hot blocks (l66 SDK, draft, 2026-11-03)

Implementation of step 3 ((b)+(e)) of `docs/evidence/M10-deck-watch-upload-design-20261101.md`,
motivated by `docs/evidence/M10-l64-deck-ab-20261103.md` (after upload-ahead 16, pages validated
>= 600 times/min carry 24% of page validations; arm 8.1 + rest of upload 6.2 + write faults 7.2
ms/frame on the Deck; design estimate CP -5 to -10 ms/frame). Implemented by an Opus subagent;
nothing launched, no host candidate built. **Status: SDK built and unit-tested; owner A/B pending.
No physical PASS.**

## Artifacts

| Item | Identity |
|---|---|
| SDK branch / commit | `mcla/m10-hotblocks` = `ff30a86db1dea49453b4dd7d985105600cf8ac45`, parent `43577c8` (v0.20.0.7), local only, not pushed |
| Worktree / build / output / install | `private/l66-sdk-wt`, `private/l66-sdk-b`, `private/l66-sdk-o`, `private/l66-sdk-install` (l65 configuration: Ninja Multi-Config, clang 20.1.8, VS 2022 Build Tools, `-march=x86-64-v3`, D3D12 on, Vulkan off, Tracy on, perf counters on) |
| Scripts / logs | `private/l66-sdk-work/` (`l66_sdk_build.ps1`, `l66_sdk_test.ps1`, `l66-sdk-configure.log`, `l66-sdk-build{,2,3}.log`, `l66-sdk-test.log`) |
| `bin/rexruntime.dll` | SHA-256 `e4e18a818f82ba42a0e959575eedb43c6a41512ef5f8fe8262f165fa47f2e385` (no runtime source change) |
| `bin/rexgpu-xenos.dll` | SHA-256 `3752a78de95c7392abd483a727ef2a632f82e989da53ed1064d597b4b329a792` |
| `lib/rexruntime.lib` | SHA-256 `03990569ea1e2d84c370216d1d7a39de3cb2ff2d4f457dc20da9beeb36559822` |
| `lib/rexgpu-xenos.lib` | SHA-256 `841edfd459dae28d03720df49a7f098a5aabd968866625978e8d270eee0d1ea0` |
| `bin/TracyClient.dll` | SHA-256 `45f1ec29c097b3d6e86884a77eda0f0a9145fc2dcd8a805e75def3693228ca87` |

Changed files: `include/rex/graphics/util/hot_blocks.h` (new, pure tracker),
`include/rex/graphics/shared_memory.h`, `src/graphics/shared_memory.cpp`,
`include/rex/graphics/d3d12/shared_memory.h`, `src/graphics/d3d12/shared_memory.cpp`,
`tests/unit/graphics/hot_blocks_test.cpp` (new), `tests/unit/CMakeLists.txt`.

## SDK switches (graphics plugin, all hot reload, default off)

| cvar | Default | Effect |
|---|---|---|
| `shared_memory_hot_blocks` | 0 | 0 off, 1 shadow (classify and count, behaviour unchanged), 2 on |
| `shared_memory_hot_k` | 6 | hot = CPU write faults in >= K of the last N frames (K clamped to N) |
| `shared_memory_hot_n` | 8 | window N, 1-8 |
| `shared_memory_hot_upload_cap_kb` | 4096 | per-submission budget of hot-path upload bytes (0 = no hot uploads) |
| `shared_memory_hot_probe_frames` | 64 | mode 2: frames in hot mode before a probe exit; 0 = never (addition, see Rules) |

Mode / K / N / probe changes take effect at the next frame end. Hot blocks run only on D3D12 (the
frame-end hook is the D3D12 census frame end; Vulkan call sites are unannotated and would be
"foreign" anyway).

## Rules as implemented

Unit: the 256 KiB block (64 x 4 KiB pages, the write-fault widening unit).

**Entry** (frame end, CP thread, under the global lock): the block's K/N history (bit per closed
frame, set when a CPU write fault invalidated it) satisfies K of N, and it is eligible:
- no texture/range watch overlaps it (new per-block watch count maintained in
  `WatchMemoryRange` / `UnlinkWatchRange`);
- no GPU-written page in it (`system_page_flags_valid_and_gpu_written_[block] == 0`);
- a vertex-fetch or index-buffer request on the CP thread touched it within the last N frames (not
  idle);
- no other request source (texture, resolve, memexport, unannotated `other`) touched it within the
  last 64 frames (`kForeignHoldFrames`).

**Mode 2 request path** (`RequestRanges`, CP thread, VB/IB source only): after the normal all-valid
fast path check, under the global lock, the invalid requested pages lying in hot blocks are
counted; if they fit the per-submission budget (`HotUploadEpoch()` = D3D12 current submission),
`MakeRangeValid` neither sets their valid bits nor arms them (other blocks of the same chunk are
handled normally, split into arm runs at block boundaries); the backend still copies them. So the
pages stay invalid and every later request uploads exactly the requested pages again. Over the cap
the whole request takes the normal path (valid + armed). Upload-ahead does not extend runs that
touch a hot block on a hot-path request. Requests from any other thread are never hot (normal path,
correct in any block).

**Immediate exits** (under the global lock): `WatchMemoryRange` overlapping the block (`watch`); any
`MakeRangeValid(written_by_gpu=true)`, i.e. `RangeWrittenByGpu` (resolves, memexport, the
whole-buffer memexport fallback) (`gpu`); a slow-path request from a non-VB/IB source on the CP
thread, before its uploads are computed (`source`).

**Frame-end exits**: exclusion (watched / GPU-written), a foreign request on the fast path this frame
(`source`), no VB/IB request for N frames (`idle`), K/N no longer met (`cold`), mode 2 only: in hot
mode for `probe_frames` frames (`probe`, history cleared so re-entry needs K real fault frames), and
a mode change (`off`).

**Restore**: every exit in mode 2 clears the block's CPU valid bits
(`valid &= gpu_written`; a hot block has none), so the next request uploads and arms as usual.
Pages still armed from before entry stay armed (the arm walk skips them; a write faults normally).
The census upload-ahead shadow bits of the block are cleared too. Mode 1 exits change nothing.

**Why the probe exists (deviation from the brief)**: in mode 2 a hot block is unprotected, so its
CPU writes cannot be observed and the K/N rule would decay to "cold" after 3 frames and oscillate.
While in hot mode under mode 2, a frame in which the block was requested by VB/IB counts as a write
frame; the probe exit (default every 64 frames) returns it to the armed path to re-check with real
faults, so a block that turned static does not stay hot forever (its cost is one extra memcpy per
request, bounded by the cap). Expected cost: about K+1 normal frames per 64, i.e. ~10% of the
saving. `shared_memory_hot_probe_frames=0` disables it. In mode 1 the block stays armed and its
history is fed by real faults only; there is no probe.

**Correctness invariants** (all hold by construction and are checked in the model test):
- a valid CPU page is always armed and current; hot pages are never valid, so every GPU read of a
  hot page comes from a copy taken by that request (the CP copies guest memory when it processes the
  draw);
- GPU-written memory is excluded (never hot), so guest reads of GPU-written memory and the scaled
  resolve global watch are unaffected; any GPU write exits first;
- KI-047 composite/scratch ranges are resolve targets (GPU-written, `resolve` requests): excluded;
- KI-033 vinyl atlas is texture-watched and texture-requested: excluded (exit on the texture request
  before its upload, exit on the watch registration, 64-frame hold after any texture request).

## Counters (appended to `MCLA_WATCH v=1` minute and mark lines; needs `memory_watch_census=true`)

`hot=<mode> hot_k= hot_n= hot_probe= hot_blocks_now= hot_enter= hot_exit=watch:,gpu:,source:,idle:,cold:,probe:,off: hot_requests= hot_upload_kb= hot_cap_hits= hot_shadow_saved_arms=`

- mode 2: `hot_requests` / `hot_upload_kb` = requests served by the hot path and the bytes uploaded
  unarmed; `hot_cap_hits` = requests that fell back to the normal path.
- mode 1 (projections): `hot_requests` / `hot_upload_kb` = VB/IB requests (fast path included)
  touching a block in shadow hot mode and the bytes mode 2 would upload for them (all requested
  pages in those blocks); `hot_cap_hits` = projected cap hits; `hot_shadow_saved_arms` = arm calls
  of VB/IB uploads whose first page is in a shadow-hot block (what mode 2 would save; attributed by
  first page like the census classes).
- `hot_blocks_now` is the count at report time; enter/exit counts are per window (reset with the
  census window). The existing `arm_calls`, `src_vb/ib=up_kb`, `by_hot_*` and `cpu_faults` fields
  show the real effect in mode 2.

## Tests

- SDK `ctest -C Release`: **2015/2015** at the first run (baseline 2006 at v0.20.0.7 + 9 new),
  `l66-sdk-test.log`; tagged suites of the l65 script plus `[hot-blocks]`, `[watch-census]`,
  `[disarm-gap]` all pass.
- New `[hot-blocks]` (`tests/unit/graphics/hot_blocks_test.cpp`, 9 cases, 601 assertions):
  K/N and config/mode/cap sanitizing; entry after K of N with VB/IB requests (and with gaps);
  no entry while off, idle (incl. a stale request older than N), watched, GPU-written, or within the
  64-frame foreign hold (exact boundary); immediate exits (watch, gpu, source) and double-exit;
  frame-end exits watch/gpu/source with the restore flag (true only in mode 2), idle (mode 1 and 2),
  cold (mode 1 by real faults; mode 2 when requests thin out), probe after exactly probe_frames with
  history cleared and re-entry only after K real fault frames, no probe in shadow, mode changes
  (on -> shadow restores and re-enters, shadow -> off clears state and ignores notes); the cap
  (exact fit, per-epoch reset, single over-cap request, cap 0); a **model** of SharedMemory
  (valid/armed/GPU-written bits, block-widened invalidation with disarm, texture watches,
  RangeWrittenByGpu, hot path, cap, restore) run on 40 random seeds x 1,500 frames with phased
  workloads (dynamic rings, static VB, textures, GPU writes, foreign requests, random mode/K/N/probe
  and tiny caps): **zero stale GPU reads**, invariants hold after every operation, and every exit
  reason, cap hits and hot uploads occur; a **negative control** (hot pages marked valid without
  arming) is detected as stale reads; deterministic restore cases (hot path keeps pages invalid and
  unwatched with no faults; foreign request exit then normal armed upload; texture watch / GPU write
  exit restore the invalid state and keep the block excluded).
- No new compiler warnings (the shared_memory warning set equals the l65 build's, shifted lines;
  nothing from the new files). Changed lines clang-format clean (clang-format 20.1.8 via
  clang-format-diff; new files whole). All changed sources ASCII.
- Not tested: the real `SharedMemory` hot path against D3D12 (needs the game); the model mirrors it.

## Host dev-override keys needed (not implemented here)

Like the upload-ahead keys (bool keys mapped to the integer cvar, phase graphics plugin):

| Key | Writes |
|---|---|
| `shared_memory_hot_blocks_shadow=true` | `shared_memory_hot_blocks=1` |
| `shared_memory_hot_blocks_on=true` | `shared_memory_hot_blocks=2` |

`=false` writes nothing; both true in one file is a conflict (neither applied, logged on the later
line). K, N, cap and probe stay at their defaults (command line only). Add `shared_memory_hot_blocks`
to `kNeverWritten` in `mcla_m10_defaults.h` until an A/B accepts it. `memory_watch_census` must be on
for the counters.

## Owner A/B protocol

Same l66-based owner build for all arms; arms differ only in `mcla-dev-overrides.txt`. Base for all
three Deck arms = the l64 "all" set (native refcount, XAPI interlocked, disarm bridging, WRM event
wake, replay batched wake, read-back elision, targeted sync, upload-ahead 16, census on).

Deck (stock 30, 1280x720, the l64 loop: start F10, stationary ~90 s F10, lap 1 F10, lap 2 F10):

| Arm | Extra override lines | Purpose |
|---|---|---|
| D-all | (none) | reference; should reproduce l64 "all" (~14.7 fps laps) |
| D-shadow | `shared_memory_hot_blocks_shadow=true` | overhead ~0 vs D-all; `hot_blocks_now`, enter/exit mix, `hot_shadow_saved_arms` vs `arm_calls`, projected `hot_upload_kb` and `hot_cap_hits` |
| D-on | `shared_memory_hot_blocks_on=true` | the effect |

Order D-all, D-shadow, D-on, D-on, D-all (warm shader store; l64 showed run-to-run 1-2%).

Read per segment: fps and pacing (>100 ms frames), CP busy and upload ms/frame (perf split), arm
calls/ms, render write faults and fault ms, `hot_*` fields, `src_vb/ib=up_kb` (upload MB/frame
up), `hot_cap_hits` (should be small; if large, raise the cap by command line in a follow-up),
exit mix (`probe` expected ~1 per hot block per 64 frames; many `source`/`watch`/`gpu` means the
gating is hit often). Expected: arm calls and faults down, CP -5 to -10 ms/frame per design (to be
measured, not assumed).

Safety per arm: `memory_watch_protect_failures=0`, no "Recovered stale physical page protection",
no "Failed to get an upload buffer", KI-047 diag v2 `composite_read_prev` / `composite_not_copied`
0, police car livery (KI-047 trap lap) when a police car appears, garage and driving vinyl (KI-033),
traffic/HUD/vehicle geometry visually correct (stale VB data would show as flickering or exploded
geometry), no crash.

PC regression check (720p BENCH-B, l63 PC "all" defaults): P-off (none) vs P-on
(`shared_memory_hot_blocks_on=true`), plus one 1440p KI-047 police lap with P-on. Mode 0 cost is one
thread-id compare per request plus watch counting per texture watch; P-off should be identical to
l65-class builds.

## Risks

- **Hot-path correctness depends on the request-source annotation**: any VB/IB-annotated request
  whose consumer relies on a write watch (rather than re-requesting) would see the stale-until-next-
  request behaviour; known VB/IB consumers re-request per draw. Unannotated call sites are treated as
  foreign (safe).
- **Concurrency window**: a guest write that lands after the hot copy but before the GPU executes
  the draw is not seen until the next request (same as the design's "copy at CP processing time"
  semantics; on real hardware the GPU would read later). If rings are rewritten while the GPU frame
  is still queued, geometry from the newer data could differ from today; watch for visual artefacts.
- **Upload volume**: every hot request copies; repeated requests of an unchanged range within a
  frame are no longer free. Bounded per submission by the cap; mode 1 projects the volume first.
- **Probe**: added beyond the brief to avoid oscillation and stuck-hot static blocks; costs ~10% of
  the saving at default 64.
- **Foreign hold**: 64 frames after any texture/resolve/memexport/other request a block cannot
  enter; a ring sharing a block with such data never goes hot (conservative).
- **Shadow accuracy**: saved arms are attributed by the first page's block; projected bytes assume
  every requested page in a hot block would be uploaded (upper bound).
- Thread model: hot state is written by the CP thread (with the global lock held where validity or
  watches change) and fault bits under the global lock; `WatchMemoryRange` is assumed to run on the
  CP thread (texture cache), as today.
- Nothing here is a physical PASS; the owner A/B and visual checks are required.
