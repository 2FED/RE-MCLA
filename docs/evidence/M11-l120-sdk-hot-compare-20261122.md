# M11 l120 SDK (2026-11-22): (a2) compare-before-copy for hot shared-memory pages, default OFF, with verify

**Headline.**
- SDK branch `mcla/m11-l120` (worktree `private/l120-sdk-wt`), one local commit
  **`c9c579c` (`c9c579c1065e8c972bf669c097f5022209dc73c7`)** on `c503e32` (l119); not pushed, no tag; the
  published fork stays v0.20.0.12 = `d889a16`.
- Behaviour change by default: **none** (`shared_memory_hot_compare` and `shared_memory_hot_compare_verify`
  default false; with both off the only new work per request is one relaxed atomic load).
- Built and unit-tested only: not run in the game, no physical PASS. Spark not used (deterministic source
  work checked by unit tests; nothing bounded to offload).
- Design source: [M11-l119-sdk-shm-census-20261120.md](M11-l119-sdk-shm-census-20261120.md) section 6.1,
  the study [M11-shared-memory-upload-and-watch-study-20261120.md](M11-shared-memory-upload-and-watch-study-20261120.md)
  (4 (a2)), the Deck numbers of [M11-l119-deck-analysis-20261122.md](M11-l119-deck-analysis-20261122.md)
  section 4. (e1) was not built (the Deck's barrier + pool + record time is 0.14 ms per frame).

## 1. Mechanism as built

A page (4 KiB, the host system page) has one of three states in `shm_hot::HotPageTable`
(`include/rex/graphics/util/shm_hot.h`, command-processor (CP) thread data):

| state | protection | valid bit | host buffer |
|---|---|---|---|
| cold | as before (armed at upload) | as before | as before |
| pending (classified hot, no shadow yet) | as before until its next upload | as before | as before |
| hot (has a shadow) | **never armed** (on) / as before (verify) | **stays clear** (on) / as before (verify) | = the shadow (on) |

- **Classification.** Per page an 8-bit history of "activity in the frame": a cold page is active when it
  was uploaded by a vertex / index request, a hot page when a compare found a change (or it got its first
  shadow). At each frame end (`SharedMemory::HotFrameEnd`, called from `WatchCensusFrameEnd`,
  `src/graphics/shared_memory.cpp:2088`) the histories of the active pages shift; a cold page with
  `popcount >= K` (cvar `shared_memory_hot_compare_k`, default 6 of 8) and no cooldown is a candidate.
  Only the active pages are walked (a list, not the 131,072 pages).
- **Entry** (under the global lock, `HotEligibleLocked` `:1805`): refused when the page holds GPU-written
  data (`refused=gpu`) or any texture / range watch overlaps it (walk of the 4 MiB watch bucket;
  `refused=texture`); refused when the shadow budget is full and nothing changed for 2 frames
  (`refused=cap`); a page used by another request source recently has a cooldown of 64 frames (`other`).
  The page becomes pending and its bit is set in the atomic `hot_bits_`.
- **Shadow.** At the pending page's next request that finds it invalid, `HotTakeShadow` copies the guest
  page into a 4 KiB slot of the shadow pool (1 MiB chunks, allocated on demand up to
  `shared_memory_hot_compare_max_mb`, default 16 = 4,096 pages; LRU-by-last-change eviction of a page
  unchanged for 2+ frames).
- **Request (on)** (`RequestRanges` `:870`, `HotSplitAndCompare` `:1879`). A vertex / index request whose
  range has a hot or pending page goes through the existing locked run building (the valid bit of a hot
  page is clear, so the lock-free fast path never claims it; upload-ahead may extend runs over hot pages,
  they are split off afterwards). For every hot page of the runs: `memcmp` of **the requested bytes only**
  (`RequestedSpan`) against the shadow; equal: nothing (the host buffer already holds the shadow's bytes:
  copies and draws are ordered on the one queue); different: **guest -> shadow (one snapshot of the whole
  page), then shadow -> upload buffer** (`D3D12SharedMemory::UploadShadowPages`,
  `src/graphics/d3d12/shared_memory.cpp:452`: the same transition and `CopyBufferRegion` recording as
  `UploadRanges`, no `MakeRangeValid`, so no valid bit and no arm). Hot pages not covered by the request
  (upload-ahead extension) are left out. The cold pages of the runs upload exactly as before.
- **Request (verify)** (`HotVerifyAfterRequest` `:1980`): the old path runs completely (arming, validity,
  uploads); afterwards every hot page of the request is compared (the requested bytes against the
  shadow), then its validity is read. The shadow follows the bytes the old path holds: it is refreshed
  from guest memory whenever the old path uploaded the page in this request.
- **Exit** (`HotExitPage` `:1752`): static data (`exit_equal` consecutive equal compares, default 32),
  hysteresis (`popcount(history) <= 2`), a texture / range watch registered over it (`WatchMemoryRange`),
  a GPU write (`RangeWrittenByGpu`) or a resolve / memexport request over it (`gpu`), any other request
  source over it (`src`: textures, other), the budget, a stale mark (below), a mode change, a reset
  (`ClearCache`, `InvalidateAllPages`). An exited page is cold, **invalid and unarmed** (on): its next
  request uploads and arms it like any cold page ("exit re-arms and re-validates").
- **Lines.** `MCLA_SHM_HOT v=1 kind=config mode= k= m=8 exit_k=2 exit_equal= max_mb= cooldown= frame=` at
  the first latch and on every change; `MCLA_SHM_HOT v=1 reason= seq= mode= k= frames= hot_pages= pending=
  hot_pages_avg= shadow_mb= shadow_mb_peak= entries= shadows= exits=static:,cold:,watch:,gpu:,src:,budget:,stale:,mode:
  refused=texture:,gpu:,cap:,other: cmp= cmp_kb= cmp_same= cmp_diff= cmp_same_valid= cmp_same_invalid=
  cmp_diff_valid= cmp_diff_invalid= cmp_same_but_faulted= cmp_ms= cmp_us_per= copies= copies_kb= copy_runs=
  copy_ms= arms_skipped= global_fires= stale= faults_hot=cp:,render:,other: bad=` in every perf-split
  window after `MCLA_SHM` while the mode is on or verify (`D3D12CommandProcessor::PerfSplitEmit`,
  `src/graphics/d3d12/command_processor.cpp:4981`); the first 16 `MCLA_SHM_HOT_BAD v=1 n= kind=diff_valid
  page= off= len= src= mode= frame=` lines (warning level).
- **Cvars** (GPU, hot-reload, latched at the frame end): `shared_memory_hot_compare` (bool, false),
  `shared_memory_hot_compare_verify` (bool, false; wins over the first), `shared_memory_hot_compare_k`
  (int, 6), `shared_memory_hot_compare_exit_equal` (int, 32), `shared_memory_hot_compare_max_mb` (int, 16).
  The on mode needs `SupportsShadowUploads()` (D3D12 yes; Vulkan, not built for MCLA: falls back to off).

## 2. Consumer audit (who relies on the valid bit or on the invalidation callback firing)

| consumer | how it learns that memory changed | with a hot page (on) | file:line |
|---|---|---|---|
| vertex requests and the l118 vertex-slot skips A / B | `IsRangeValidLockFree` before a skip, `RequestRange` | the valid bit of a hot page is clear: the skip is refused and the slot requested, so the compare runs | `src/graphics/d3d12/command_processor.cpp:6371`, `:6402`, `:6416` |
| index buffer (guest DMA) | `RequestRange` (source index) | compare | `src/graphics/primitive_processor.cpp:931` |
| converted index cache (CPU conversion, cached) | its OWN invalidation callback and its OWN arm of the range | unaffected: it arms the page itself (the hot page then faults, harmlessly); its callback is called by the heap | `src/graphics/primitive_processor.cpp:1378`, `:1381` |
| texture cache range watches | `WatchMemoryRange`, fired by `FireWatches` on a fault | refused at entry; a registration exits the page (it is then armed at its next request, so the watch fires on the next write); texture requests (source texture) exit hot pages before the normal upload and arm | `src/graphics/pipeline/texture/cache.cpp:981`, `:988`, `:758`, `:1287`; `src/graphics/shared_memory.cpp:490` |
| scaled-resolve global watch | `RegisterGlobalWatch`, fired by `FireWatches` | the compare fires the global watches for every changed hot page, and an exit fires them once more for the page (writes since its last compare) | `src/graphics/pipeline/texture/cache.cpp:234`; `HotFireGlobalWatches` `src/graphics/shared_memory.cpp:1954` |
| resolve destinations, memexport | request (source resolve / memexport), then `RangeWrittenByGpu` | the request exits the pages (`gpu`); `RangeWrittenByGpu` exits any left **before** the GPU data becomes valid; GPU-written pages are refused at entry | `src/graphics/d3d12/render_target_cache.cpp:2188`; `src/graphics/pipeline/texture/cache.cpp:426`; `src/graphics/d3d12/command_processor.cpp:6444`, `:6454`, `:6783`, `:6789`; `src/graphics/shared_memory.cpp:548` |
| trace download / trace writer, EDRAM snapshot | `PrepareForTraceDownload` (valid := GPU-written, host bytes unchanged); `WriteMemoryRead` per upload | hot pages are invalid anyway; the shadow still equals the host bytes; `UploadShadowPages` records `WriteMemoryRead` too; the EDRAM snapshot does not touch shared memory | `src/graphics/shared_memory.cpp:1562`; `src/graphics/d3d12/shared_memory.cpp:452` |
| other paths that make a page valid (an upload off the CP thread, GPU data off the CP), exact invalidations (guest protect / decommit / release), resets | `MakeRangeValid`, `MemoryInvalidationCallback(exact)`, `ClearCache`, `InvalidateAllPages` | mark the page stale (atomic `hot_stale_bits_`) or request a reset; the CP drops stale pages at their next touch and at every frame end | `src/graphics/shared_memory.cpp:610`, `:1241`, `:307`, `:378` |

**The decision on watches (the brief's either-or):** both, layered. Hot classification is refused for pages
with any range watch or GPU-written data, a watch registration or a non-vertex / index request takes the
page out before it is used (so range watches never overlap a hot page), and the compare path additionally
fires the global watches whenever it finds a change. Texture data pages cannot go silently stale: a texture
request always takes the page out first and the normal path uploads and arms it.

## 3. The 256 KiB widening

A fault on a cold page widens the invalidation to its 64-page block (`MemoryInvalidationCallback`,
`src/graphics/shared_memory.cpp:1262`). For hot pages (on) this clears a valid bit that is already clear and
leaves the shadow alone (the host buffer did not change): the compare still decides at the next request
(equal: no copy; changed: copy). The widening can therefore neither defeat the compare nor leave a changed
hot page "valid". The disarm of the widened window touches only watched pages; hot pages are not watched.
Tested: `l120 SharedMemory on` (a fault on page 5 widens over the hot page 2, whose next request copies
nothing and still matches the guest bytes).

## 4. Thread safety and the correctness argument

- All table, shadow and counter state is CP-thread data (requests, frame ends, `WatchMemoryRange` and
  `RangeWrittenByGpu` all run there in this code; off the CP the stale bitmap is used instead). Guest
  threads only read `hot_bits_` and set `hot_stale_bits_` / `hot_faulted_bits_` (relaxed atomics, under the
  global lock where they already were).
- **Equal compare.** The compare reads the requested guest bytes at request time, the same sampling point as
  today's copy. If they equal the shadow, the host buffer (which holds exactly the shadow's bytes, below)
  equals them in GPU order, so the draw reads the right bytes. A guest write that lands after the compare
  is the case today's protection handles with a fault for the *next* request; here the next request's
  compare sees it.
- **Copy order.** guest -> shadow first (one snapshot), then shadow -> upload buffer. The uploaded bytes are
  always exactly the shadow, so a guest write during the copy cannot make host and shadow differ; it makes
  guest and shadow differ, which the next compare detects. (Test `l120 compare and copy order`.)
- **Host writers other than the shadow copies**: CPU uploads of cold pages never include hot pages (split);
  GPU writes exit the page first; any other `MakeRangeValid` over a hot page marks it stale and it is
  dropped before its next compare. So "shadow = host bytes" holds for every hot page.
- **Readers that skip requests** check the valid bit (the vertex-slot skips), which is clear for hot pages.
- **Exits** return the page to "invalid, unarmed": the next request uploads current guest bytes and arms, as
  for any cold page after a fault.
- Remaining gap (not reachable in the current code): `WatchMemoryRange` off the CP thread only marks the
  page stale; a write between that registration and the CP's next touch would not fire the new watch. All
  callers (`src/graphics/pipeline/texture/cache.cpp:981`, `:988`) run on the CP thread.

## 5. Verify: what must be 0, and a deviation from the brief

The brief asked for `cmp_same_but_faulted` (must be 0: "every time the compare reports same the old path must
not have had a fault since the shadow was taken"). **That counter cannot be a must-be-0 gate:** a guest that
writes the *same* bytes again (a HUD or particle ring rewritten unchanged, a fault on other bytes of the page)
faults on the page and leaves the requested bytes equal. In that case the equal compare is correct by
definition (the requested bytes equal the host bytes). It is kept as an informational counter (expected
above 0; the unit test produces two such events on purpose).

**The must-be-0 counter is `cmp_diff_valid`** (the design document's name): in verify the compare runs after
the old path decided, then the page's validity is read. "Different while valid" means the guest bytes differ
from the bytes the old path holds in the host buffer although no write fault invalidated the page: either the
protection missed a write or the shadow bookkeeping is wrong. A write that lands between the old copy and the
compare faults (the page is armed before its copy) and is counted as invalid, never as "different while
valid". Each such event prints one of the first 16 `MCLA_SHM_HOT_BAD` lines. The verify also reports what the
on mode would save: `cmp_same_invalid` (false invalidations: uploads the on mode would not make),
`arms_skipped` (old-path upload runs of hot pages: arms the on mode would not make), `faults_hot` by thread
class (faults the on mode would not take), and the compare cost `cmp_ms`.

Other deviations from the design text (section 6.1):
- Cvars: two bools (`_compare`, `_verify`; verify wins) instead of one int 0/1/2, as the brief named them.
- Counters and line: the brief's names (`cmp_same`, `cmp_diff`, `hot_pages`, `shadow_mb`,
  `refused=texture:,gpu:,cap:,other:`, `cmp_same_but_faulted`) plus the design's four verify classes; no
  separate `cmp_eq_*` names.
- Pages, not blocks: per 4 KiB page (the census measured 256 KiB blocks; the Deck faults are per page).
- Entry signal: "uploaded (cold) or changed (hot) in K of 8 frames" (an upload follows a fault or a widened
  invalidation; while hot there are no faults, so a change found by the compare is the activity signal).
- Requested-bytes compare, whole-page copy (the design compared the requested sub-range too).
- No per-page `watch_count_`: entry walks the 4 MiB watch bucket under the lock instead (no always-on cost
  in `WatchMemoryRange` / `UnlinkWatchRange`), and registrations exit pages.
- Global watches fired on a compare difference and on an exit (not in the design).
- Shadow budget eviction: LRU by last change (page unchanged for at least 2 frames), else refusal (`cap`).

## 6. Tests, build, warnings

- `tests/unit/graphics/shm_hot_test.cpp`, 9 cases `[l120][shm_hot]`:
  1. classification, hysteresis and cooldown (5 of 8 frames: no entry; 6: entry; hot pages are never
     candidates; exit at 2 of 8; the cooldown blocks re-entry for 8 frames; static exit after exit_equal
     equal compares; a difference resets the run; a taint keeps a page out; the cvar sanitizers);
  2. shadow budget, eviction and slot reuse (256 slots at 1 MiB; refusal when full; LRU victim; slot reuse);
  3. compare and copy order (only the requested bytes compared; a write after the snapshot never reaches the
     upload, which reads the shadow, and is seen by the next compare; `RequestedSpan` edges);
  4. on mode through `SharedMemory` with a real guest physical heap and real write faults (entry, first
     shadow, the valid bit stays clear, **no fault and no arm** for the hot page, equal compares copy
     nothing, a change is copied from the shadow, a widened invalidation from a cold neighbour, switching off
     drops the set and the page is uploaded and armed again: the next write faults);
  5. watches, GPU writes, other sources and resets (a watched page is refused at entry; a compare difference
     fires the global watch; a watch registered over a hot page exits it and the page is armed at its next
     request so the watch fires on the next write; `RangeWrittenByGpu` exits a page before the GPU data is
     valid; a page GPU-written at the frame end is refused (`gpu`); a texture request exits a hot page and
     uploads / arms it; `InvalidateAllPages` drops the set);
  6. static exit re-arms; verify counters (verify keeps the page valid and armed; diff-invalid, same-valid,
     two same-invalid with same-but-faulted; `cmp_diff_valid` 0; faults on hot pages counted);
  7. **randomised model** (60 frames, a 24-page window with a ring that is rewritten often, warm and rare
     pages, same-bytes rewrites, random requests up to two pages, occasional texture requests): run with
     off, verify and on from the same seed; after **every** request the host buffer bytes of the requested
     range equal the guest bytes in all three runs, and the three runs' sequences of requested host bytes
     are equal; verify makes exactly the protection calls and faults of off and has `cmp_diff_valid` 0;
     on: faults 221 -> 172 and arms 1224 -> 859 against off (this seed);
  8. micro-benchmark (not a gate; build PC, cache-hot): **compare 4 KiB 33 ns, compare 256 B 3.4 ns,
     guest -> shadow copy 4 KiB 27 ns**;
  9. the lines.
- ctest (`private/l120-sdk-b`, Release): **2445 / 2445 passed** (4 BitStream skipped as before) three times
  before the commit and three times after the install (l119 2436 + the 9 new cases).
- Warnings: the fresh full build's normalized set had 211 = the l119 set (210) + 1 new
  (`-Wreorder-ctor` in the new test's model class); fixed (member order), the rebuilt file adds none:
  **0 new against l119, 0 gone** (scratch `l120-work/w120a.txt`, `w120b.txt`, `w120i.txt` against
  `l119-work/w119all.txt`).
- `git-clang-format` against `c503e32`: applied (wraps in `shm_hot.h`, `shared_memory.cpp`,
  `d3d12/shared_memory.cpp`, the test), then "clang-format did not modify any files"; `git diff --check`
  clean.
- Diff `c503e32..c9c579c` (numstat): `include/rex/graphics/util/shm_hot.h` +523 (new);
  `src/graphics/shared_memory.cpp` +647 / -5; `include/rex/graphics/shared_memory.h` +68;
  `src/graphics/d3d12/shared_memory.cpp` +49; `src/graphics/d3d12/command_processor.cpp` +10;
  `include/rex/graphics/d3d12/shared_memory.h` +4; `include/rex/graphics/d3d12/command_processor.h` +3;
  `tests/unit/CMakeLists.txt` +1; `tests/unit/graphics/shm_hot_test.cpp` +700 (new).
- Installed to `private/l120-sdk-install` (`0.20.0.5-dev.gc9c579c`):

| bin file | bytes | SHA-256 |
|---|---|---|
| rexruntime.dll | 11794944 | C2236840D18C241318E7CD6D0F26E1EF29EE15B14B310F04A05FDB2B86F18192 |
| rexgpu-xenos.dll | 4019200 | 151FA239AC43D21BBE9629402448B5F8EE27F864C530A4804E65E5B9A070D167 |
| TracyClient.dll | 246784 | 9935B162EC1C531837AF9BC5000E7C430217A321DC5D43CE0DD1CF0223ED8CFD |
| rexglue.exe | 4113408 | FC852BAEB147A14875BBFCCAECEC7D1259AA475BFA43EA6D863B7E4E6CF7BE35 |

rexruntime.dll, TracyClient.dll and rexglue.exe have no source change; their bytes differ in the fresh build
tree (as with every fresh SDK tree).

## 7. Cost and expected saving

- Off (default): one relaxed atomic load per request and one per `MakeRangeValid`, `WatchMemoryRange`,
  `RangeWrittenByGpu` and invalidation; the frame end returns after latching the cvars.
- On, per hot page per request: the locked run building (the valid bit is clear, so the lock-free fast path
  does not apply), a compare of the requested bytes (3-33 ns on the PC; about 2.5x on the Deck), a copy and
  an upload only on a change (no arm), and the global-watch call on a change.
- **Deck** (l119 run 1, minute 4: 492 arms x 16.4 us = 8.1 ms CP, 176 faults on the render thread x 37 us +
  3.5 us Wine dispatch): with the study's hot share h = 0.49-0.84 of the arms and faults, the on mode removes
  241-413 arms (**-4.0 to -6.8 ms CP**) and 86-148 faults (**-3.5 to -6.0 ms render thread**). The compares
  cost far less than the 1 us each the Deck analysis assumed: at about 5 requests per upload and 3 pages per
  request, about 3,600-6,200 page compares x 0.08-0.2 us = **+0.3 to +1.2 ms CP**, plus the lock taken by
  each hot request (uncontended now that hot pages do not fault). **Expected net: CP -3 to -6 ms, render
  thread -3.5 to -6 ms per frame**, before the verify run measures h (`faults_hot`, `arms_skipped`,
  `cmp_same_invalid`).
- **PC** (about 1,300 arms of about 1.2 us and 110 faults of about 11 us per frame at 6 k draws): -0.3 to
  -1.0 ms CP and -0.3 to -0.8 ms render thread at the same hot share; small, as the study expected.

## 8. Limits and risks

- Nothing ran in the game. The unit tests prove the mechanism against a model and real faults on a private
  guest heap, not the game's access pattern.
- Hot requests take the global lock on the CP thread (the run building); with the faults gone the lock is
  mostly uncontended, but on the Deck the lock is a Wine critical section: the verify run's `cmp_ms` and the
  on run's CP time decide.
- The on mode changes which pages are write-protected; a consumer that reads shared memory without a
  request and without the valid bit would see stale bytes. The audit (section 2) found none in the D3D12
  path; the Vulkan backend is not built for MCLA and falls back to off.
- Shadow memory: up to 16 MiB (budget), plus 2.3 MiB of per-page tables once the mode was on.
- `faults_hot` counts the faulting page before widening only (one page per fault).
