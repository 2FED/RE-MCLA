# M11 l121 SDK (2026-11-22): KI-090 fixes of the hot-page lever (whole-page compare, emulated faults, GPU exits) and the extended verify

**Headline.**
- SDK branch `mcla/m11-l121` (worktree `private/l121-sdk-wt`), one local commit
  **`b7cc959` (`b7cc959143e92f8368d561e38296eb1680509845`)** on `c9c579c` (l120). Not pushed, no tag.
  The published fork stays v0.20.0.12 = `d889a16`.
- Behaviour change by default: **none**. `shared_memory_hot_compare` and `_verify` stay default false.
  With both off, the new code costs nothing per request: the deferred-fault list is empty and the hot
  section is skipped as in l120.
- With the lever on, l121 changes the ON mode in three ways: **F1** whole-page compare, **F3** a change is
  treated as the write fault it replaces, **F2** a GPU-write exit fires nothing. The new cvar
  `shared_memory_hot_compare_span` (default false) restores the l120 span-only behaviour for an A/B. F2 stays
  on in that mode.
- **F4**, the extended verify: five must-be-0 counters, one informational counter, per-kind
  `MCLA_SHM_HOT_BAD` lines, `MCLA_SHM_HOT_INFO`, and a per-window `MCLA_SHM_HOT_PAGES` consumer list.
- Built and unit-tested only. Not run in the game, no physical PASS. Spark not used: these are
  deterministic source edits checked by unit tests, with no bounded piece worth offloading.
- Inputs: [M11-ki090-shm-hot-minimap-20261122.md](M11-ki090-shm-hot-minimap-20261122.md) (the candidates),
  [M11-l120-sdk-hot-compare-20261122.md](M11-l120-sdk-hot-compare-20261122.md) (the lever as built),
  [M11-l119-deck-analysis-20261122.md](M11-l119-deck-analysis-20261122.md) (why it matters on the Deck).

All file:line references below are to `private/l121-sdk-wt` at `b7cc959`.

## 1. F1: whole-page compare (candidate 1)

- `HotCompare` (`src/graphics/shared_memory.cpp:1981`) now returns both compares: the requested bytes
  (`span_equal`, the l120 rule) and the whole 4 KiB page (`page_equal`, `HotPageTable::PageEqual`,
  `include/rex/graphics/util/shm_hot.h:328`). `equal` decides: whole page by default, span only with
  `shared_memory_hot_compare_span`. Only the deciding compare advances the equal run
  (`HotPageTable::CountCompare`, `shm_hot.h:331`).
- ON (`HotSplitAndCompare`, `shared_memory.cpp:2025`): every hot page of the upload runs is compared.
  On any difference it goes guest -> shadow (the whole page) -> upload buffer -> host buffer, the same
  copy path as l120, which was already whole-page.
- In whole-page mode, the hot and pending pages that an upload-ahead run covers without a request are
  compared or shadowed too (`:2048-2062`). The l120 code left them out, which left them stale where the old
  path uploads them. `shared_memory_upload_ahead_pages` is 0 by default; the pc/upload-ahead arm sets it.
- With span mode on, the l120 behaviour is unchanged: span compare, unrequested pages left out, and a
  per-page global fire on a difference (`HotFireGlobalWatches`, `:2117`).
- Tests:
  - `l121 F1` (`tests/unit/graphics/shm_hot_test.cpp:841`): a write at byte 3000 of a hot page whose
    requests cover bytes 512-767.
    - Whole page: the host page is copied and equals guest memory.
    - Span: nothing is copied and byte 3000 stays stale. This is KI-090 candidate 1, reproduced.
  - The randomised model (`:562`) now also reads the WHOLE pages of every request. This models a GPU reader
    outside its request.
    - Off, verify and whole-page ON give identical whole-page reads, with 0 stale.
    - The span lever gives **56 stale whole-page reads** on the same seed.

### Which GPU readers can read bytes outside their own request (answer to the F1 question)

From the source, these readers of the shared-memory buffer exist:

| reader | requests its range? | can read outside it? | file:line |
|---|---|---|---|
| vertex fetch in the **vertex** shader | yes, per fetch slot: `[address, address + size)` of the fetch constant, source `vb` | **yes**: no bounds check, so any index or offset past `size` reads the SRV (the whole 512 MiB buffer) | request `src/graphics/d3d12/command_processor.cpp:6420-6427`; no bounds check `src/graphics/pipeline/shader/dxbc_translator_fetch.cpp:146` |
| vertex fetch in the **pixel** shader | **no**: the vertex-buffer loop walks only the vertex shader's `vertex_fetch_bitmap` | **yes**, entirely outside any request | loop `command_processor.cpp:6360-6362`; the pixel map is used only for the fetch constant buffer, `:9958` |
| l118 vertex-slot skips (in-sync, same address and size) | skip only while `IsRangeValidLockFree` holds | no: a hot page's valid bit is clear, so the slot is requested | `command_processor.cpp:6371-6416` |
| guest index buffer (DMA) | yes, source `ib`, exactly the indices drawn | no (the IBV is bounded by the count) | `src/graphics/primitive_processor.cpp:929-931` |
| converted index cache | its own arm of the range (the CPU reads guest memory) | not a GPU reader of the buffer | `primitive_processor.cpp:1381` |
| texture load shaders | yes, base and mips (source `tex`); the request takes hot pages out first | no (their ranges are the texture's extent) | `src/graphics/pipeline/texture/cache.cpp:758`, `:1287`; exit `shared_memory.cpp:914-931` |
| memexport (shared memory as UAV) | yes, the export ranges, or the WHOLE buffer when they are unknown; source `memexport` takes hot pages out | reads in that draw go through the UAV, and every page they can reach was taken out by the request | `command_processor.cpp:6453-6465` |
| resolve, scale 1 | yes, source `resolve`, which takes hot pages out | a resolve writes, it does not read | `src/graphics/d3d12/render_target_cache.cpp:2187-2188` |
| resolve, scale 2 (the PC sessions) | no request (scaled buffer) | does not read shared memory; `MarkRangeAsResolved -> RangeWrittenByGpu` takes the pages out (F2) | `cache.cpp:395-431` |
| trace download / readback | only GPU-written ranges, never hot | - | `shared_memory.cpp:1652`; `d3d12/shared_memory.cpp:296` |

**Decision: a per-request whole-page compare is sufficient; no once-per-frame sweep of unrequested hot
pages was built.**

Why: with whole-page compare, the host bytes of a hot page are, at every moment, the guest bytes as of the
last request that touched the page. The old path guarantees no more than that.
- Old path, page valid: host = guest.
- Old path, page written: the page faults, becomes invalid, and stays stale in the host buffer until the
  next request uploads it.
- So a reader that bypasses requests (pixel-shader vfetch, a vertex fetch past `size`) sees the same stale
  bytes in both paths, for the same window: from the write to the next request of that page.
- What l120 broke was the time *after* such a request. Only the requested bytes were refreshed, so a
  bypassing reader read stale bytes even right after the page had been requested, where the old path's
  whole-page upload had refreshed them. F1 closes exactly that.
- A frame-start sweep would make hot pages *fresher* than the old path makes cold ones. It is not needed
  for parity, and it would cost one 4 KiB compare per hot page per frame on top of the request compares.

## 2. F2: GPU-write exit ordering (candidate 4)

- `HotExitPage` (`shared_memory.cpp:1842`, F2 branch at `:1854-1863`): an exit for `kGpu` makes no fire and
  no emulated fault. This covers both `RangeWrittenByGpu` (`:583`) and resolve and memexport requests.
  - It produces exactly the callbacks of the non-hot path for the same event: `RangeWrittenByGpu`'s
    `FireWatches(..., invalidated_by_gpu = true)` (`:590`), then `MakeRangeValid(..., true)` (`:606`).
  - l120 fired the global watch with by_gpu=false here. With `pre_mask_resolve_l2_block` that cleared the
    scaled bits that `MarkRangeAsResolved` had just set (`texture/cache.cpp:395-431`).
- `gpu_exit_after_mark` counts kGpu exits of a page that already has scaled bits (`:1860`). It uses the
  texture cache's scaled probe: `SharedMemory::SetHotScaledProbe`, registered at `cache.cpp:235`, thunk at
  `:1476`.
- Test `l121 F2` (`shm_hot_test.cpp:953`, both compare modes), with a scaled model: a global watch that
  clears its pages on a CPU-style fire, and the probe. The sequence is: the hot page changed since its
  last compare, the scaled bit is set, then `RangeWrittenByGpu`.
  - The only fire is `by_gpu=true`.
  - The scaled bit survives; `gpu_exit_after_mark=1`, `exits gpu=1`.
  - Nothing deferred fires later either: a vertex request and a frame end follow, with no fire.

## 3. F3: widened invalidation parity (candidate 3)

- The 256 KiB widening and the invalidation of `MemoryInvalidationCallback` moved, with unchanged
  arithmetic, into `WidenInvalidationLocked` (`:1234`) and `InvalidatePageRangeLocked` (`:1265`).
  The callback calls them at `:1364` and `:1405`; `FireWatches` now returns the range callbacks it fired
  (`:534`).
- In whole-page ON mode, a difference found at a request is the write fault it replaces.
  `HotEmulateFault` (`:2441`) does exactly what the callback does for a non-exact fault on that page:
  1. it widens to the page's block, stopping at GPU-written pages and respecting
     `MCLA_INVALIDATE_WIDEN_PAGES`;
  2. it clears the valid and GPU-written bits and the census shadow;
  3. it fires the global watches and the range watches with by_gpu=false over the same range.
  - It changes no protection: the hot page was never armed. An armed cold neighbour stays armed, and its
    next upload re-validates it; arming an armed page makes no call (`src/system/xmemory.cpp:2311`).
- Request differences are emulated right after the split, before the upload is recorded (`:2079`, `:2114`).
- An exit is a separate case. A hot page that leaves for any reason other than kGpu is compared once more
  (whole page), and a difference is a pending fault (`:1875-1885`).
  - Exits happen inside `WatchMemoryRange`: firing there would let the callback null the texture's handle
    before `WatchMemoryRange` returns it.
  - Exits also happen inside a texture request: firing there could mark a prepared texture outdated
    between its prepare and its commit.
  - So the exit fault is deferred to the next vertex or index request (`:912`) or the frame end
    (`:2352`, `:2399`).
- l120 fired a conservative global watch for every exit with a shadow. In the l120 sessions that was 234k
  calls per minute. l121 fires only when the page really changed.
- Tests:
  - `l121 F3` (`shm_hot_test.cpp:879`): a texture watch on page 9 of the hot page's block. The old path
    (off) takes the fault on the write; whole-page ON makes the change at the next request. The result is
    the **same single global fire (same range, by_gpu=false)**, the same watch fired once, and the same
    invalidation of page 9. In span mode nothing fires.
  - `l121 exits` (`:993`):
    - an unchanged page taken out by a texture request fires nothing;
    - a changed one fires once, at the next vertex request, not inside the texture request;
    - a watch registered over a changed hot page gets no callback inside `WatchMemoryRange`, and the fault
      comes at the frame end.

**What remains different from the old path (stated, not fixed):**
1. **Timing.** The old path fires at the guest write, on the guest thread. The lever fires at the next
   request of the page, or for an exit at the next vertex or index request or frame end.
   - A texture watched in the widened block is therefore invalidated one draw or more later, if a
     neighbour's own fault does not do it first.
   - This matters only if a neighbour changes without its own fault. The KI-090 analysis found no such
     writer (section 2.B there).
2. **Same-bytes writes.** In the old path a write of the same bytes still faults and fires the block. The
   lever sees no difference and fires nothing. Verify counts exactly those watches as `tex_watch_missed`.
   - The l120 verify sessions show such writes (`cmp_same_but_faulted` 3k-93k per minute). Whether they hit
     a block with texture watches is what Run 1 measures.
3. **Writes directly before a GPU write over a hot page.** Their neighbour invalidations are not reproduced
   (F2 fires nothing on a kGpu exit). In every l120 session `exits gpu=0`.
4. **The lever's own extra invalidations.** An emulated fault also invalidates cold neighbours that the old
   path had already re-validated since the write. They upload the same bytes once more. This costs time,
   not correctness.

## 4. F4: extended verify

All new fields are appended to the `MCLA_SHM_HOT` line after `bad=`; the l120 fields keep their order
(`shm_hot.h:556-566`):
`compare=page|span cmp_page_diff_span_same= tex_watch_missed= global_fire_extra= global_fire_cleared=
gpu_exit_after_mark= emul= emul_pages= emul_tex= info=`. The config line ends in `compare=page|span`.

| counter | meaning | must be | where |
|---|---|---|---|
| `cmp_diff_valid` (l120) | the whole page (span in span mode) differs although the page is valid in the old path: a missed protection | **0** | `shared_memory.cpp:2143` `verify_page` |
| `tex_watch_missed` | range watches the old path fired from write faults on a hot page since its shadow, which the lever would not fire: the next compare found the page equal (span mode: always) | **0** | records at fault time `:1367-1403`; resolved by `HotResolveFaultRecord` `:2511` |
| `global_fire_extra` | fires the lever would make (whole page: the emulated fault; span: the page fire) although no CPU invalidation covered the page since its shadow | **0** | `HotVerifyWouldEmulate` `:2485`; coverage bits set in the callback `:1390-1403` |
| `global_fire_cleared` | lever fires (ON: made; verify: would make) whose range holds scaled-resolve pages | **0** | `:2455-2466`, `:2497-2502` |
| `gpu_exit_after_mark` | kGpu take-outs of a page that already has scaled bits | **0** | `:1860` |
| `cmp_page_diff_span_same` | the whole page differs while the requested bytes are equal: what the l120 lever left stale | informational, **expected > 0** | `verify_page`; the first 16 as `MCLA_SHM_HOT_INFO v=1 kind=page_diff_span_same page= off= len= first_diff= src= slot=` |
| `emul` / `emul_pages` / `emul_tex` | emulated faults: pages of the widened ranges, and the range callbacks fired (ON) | information (verify: would make) | `HotEmulateFault`, `HotVerifyWouldEmulate` |

- **Deviation from the brief ("all must be 0").** `cmp_page_diff_span_same` cannot be a gate. It counts
  writes the game really makes to other bytes of a hot page; that is candidate 1's precondition, and the
  fixed lever handles it by construction. It is the stale-byte count *of the l120 lever*. Its INFO lines are
  kept separate from the BAD lines, so that the "no BAD line" rule stays clean.
- **BAD lines.** `MCLA_SHM_HOT_BAD v=1 n= kind=<diff_valid|tex_watch_missed|global_fire_extra|global_fire_cleared|gpu_exit_after_mark> page= ... mode= frame=`.
  Each kind has its own budget of 16 lines (`HotBad` `:2541`); `bad=` counts all printed lines.
- **The consumer list**, one line per perf-split window after `MCLA_SHM_HOT`
  (`d3d12/command_processor.cpp:4990-4996`):
  `MCLA_SHM_HOT_PAGES v=1 reason= seq= n= more= pages=0xADDR:src:slot:requests:bytes:diffs,...`.
  - It lists the first 16 hot pages compared in the window: guest address, request source (`vb` or `ib`),
    the vertex fetch slot (set by the CP around each vertex request, `:6423-6426`; -1 otherwise), the
    requests, the requested bytes, and the compares that found a change.
  - `more` counts the compares of pages that did not fit.
  - Texture and other sources never appear: they take hot pages out.
- In verify mode the old path stays fully in force. The verify code only reads, compares, refreshes its own
  shadows and computes the widened range under the lock; it fires nothing and invalidates nothing.
- Test `l121 F4` (`shm_hot_test.cpp:1046`) produces each counter on purpose:
  - a write to other bytes gives `cmp_page_diff_span_same`, an INFO line, and `emul`;
  - a same-bytes write next to a watched page gives `tex_watch_missed`;
  - a real change with a watch is not missed;
  - a write through the unprotected physical view gives `cmp_diff_valid` and `global_fire_extra`;
  - a scaled page inside the block gives `global_fire_cleared`;
  - a scaled hot page plus `RangeWrittenByGpu` gives `gpu_exit_after_mark`;
  - the consumer list is checked too.
- The l120 verify case now runs in both compare modes (span keeps the l120 numbers).
- `l121 lines` (`:1131`) checks the exact BAD, INFO and PAGES formats.

## 5. Cost of the whole-page compare

- Micro-benchmark (`shm_hot_test.cpp`, not a gate; build PC, cache-hot): **`PageEqual` 4 KiB 32 ns**,
  span compare 256 B 3.4 ns, guest -> shadow copy 4 KiB 17-24 ns.
- PC: the l120 verify sessions ran 2-5.4 M compares per minute (`cmp_same_invalid` 0.67-1.8 M is one
  third). That is about 550-1,500 compares per frame at 60 fps, so about **+0.02 to +0.05 ms of CP per
  frame** for the whole page instead of the span.
- Deck: the l120 estimate was 3,600-6,200 page compares per frame. At about 2.5x the PC cost (about 80 ns)
  that is about **+0.3 to +0.5 ms of CP per frame**, against the -4.0 to -6.8 ms of arm cost the lever
  removes (l120 document, section 7).
- The emulated faults add one locked invalidation and fire per *changed* hot page. Those are the faults the
  old path took anyway, without the protection calls.
- The verify run measures all of this: `cmp_us_per`, `cmp_kb` (now 4 KiB per page compare, plus the span
  in verify) and `emul`.
- Verify itself compares twice (span and page) and widens under the lock on every difference. Its
  `cmp_ms` is an upper bound for ON.

## 6. Tests, build, warnings

- `tests/unit/graphics/shm_hot_test.cpp`: 15 cases `[shm_hot]`, the 9 l120 cases (2 extended) plus 6
  `[l121]` cases. `unit_tests "[shm_hot]"`: **All tests passed (14122 assertions in 15 test cases)**.
- Model on that seed: faults 221 -> 172 and arms 1224 -> 861 (off -> ON; l120: 859).
- ctest (`private/l121-sdk-b`, Release): **2451 / 2451 passed** three times before the commit and three
  times after the install (l120 2445 + 6). The 4 BitStream cases are skipped, as before.
- Warnings: the fresh full build (`l121-work/sdk-build1.log`) normalized to **210 = the l119/l120 set, 0 new,
  0 gone**. The union with the incremental, test-target and install builds is the same 210.
- `git-clang-format` against `c9c579c`: applied (wraps in `shm_hot.h`, `shared_memory.cpp`, the test), then
  "clang-format did not modify any files". `git diff --check`: clean.
- Diff `c9c579c..b7cc959` (numstat):

  | file | + | - |
  |---|---|---|
  | `src/graphics/shared_memory.cpp` | 541 | 126 |
  | `tests/unit/graphics/shm_hot_test.cpp` | 476 | 15 |
  | `include/rex/graphics/util/shm_hot.h` | 126 | 2 |
  | `include/rex/graphics/shared_memory.h` | 74 | 4 |
  | `src/graphics/d3d12/command_processor.cpp` | 12 | 1 |
  | `src/graphics/pipeline/texture/cache.cpp` | 6 | 0 |
  | `include/rex/graphics/pipeline/texture/cache.h` | 3 | 0 |
  | `include/rex/graphics/d3d12/command_processor.h` | 2 | 0 |

- Installed to `private/l121-sdk-install` (`0.20.0.6-dev.gb7cc959`):

  | bin file | bytes | SHA-256 |
  |---|---|---|
  | rexruntime.dll | 11794944 | B734A799CBEFD174A0231922EF9FA7CE79C98E8AB574D020C7971CA16E2EB5BC |
  | rexgpu-xenos.dll | 4037120 | 6440093FEE6A9E75432874DAA69CA0E8B8516E96BB5076E337D2FD2EB3774EB1 |
  | TracyClient.dll | 246784 | 7B268EB2475F97169EF33DBA6701C563D7990821BBC70F9AAE420C8017510B67 |
  | rexglue.exe | 4113408 | 23C9DD7377B75462802594E3E9193CE20B8D34356C51351133AE5FD6A2EBDE7D |

  `rexruntime.dll`, `TracyClient.dll` and `rexglue.exe` have no source change. Their bytes differ in the
  fresh build tree, as with every fresh SDK tree.

## 7. Limits and risks

- Nothing ran in the game. The unit tests prove the mechanism against the old path's callbacks on a private
  guest heap, not the game's access pattern. KI-090's cause is still not proven: l121 removes every
  candidate the analysis found, and Run 1 of verify measures each of them.
- **The emulated faults fire texture and global watches on the command-processor thread inside a vertex or
  index request.** The old path fires them on guest threads at any time, so the callbacks are already
  thread-agnostic. A texture invalidated there is reloaded at its next use.
- F3 reproduces the old path's over-invalidation, so the ON saving is smaller than l120's.
  - Cold neighbours of a changing hot page are uploaded again, as in the old path.
  - What remains saved: the arm, the fault and the disarm of the hot page; its upload when unchanged; and
    the compare instead of a fault on the render thread.
- Hot requests still take the global lock (run building, emulated faults).
- `tex_watch_missed` depends on how often the game rewrites unchanged bytes on hot pages next to texture
  data. If Run 1 shows it above 0, the l121 ON mode can lose those invalidations. The BAD lines name the
  page and the first watch.
