# M11 KI-090 (2026-11-22): the minimap white square with shm-hot ON: what the hot-page lever changes, and what it does not

Read-only root-cause analysis. No builds, tests or game launches. Spark not used: this is a source audit plus
counter arithmetic over five sessions, with no bounded piece worth offloading.

## 0. Inputs

- SDK: `private/l120-sdk-wt` at `c9c579c` (l120, `shared_memory_hot_compare`).
- Sessions copied with robocopy (excluding save-private, before-user, after-user, save-files-private.json,
  *.dmp, runtime-journal-private.log) to `private/owner-l120-20261122/sessions/`: `03fe7a0c` (pc/shm-hot ON,
  the defect, 17 min), `b143fe23` (ON, 5 min, no report), `58119911` (verify, 8 min), and `39db5b41` /
  `314bfd24` (the clear sessions after them, 2 + 3 min). The F10 packages of `03fe7a0c` were copied to
  `private/owner-l120-20261122/diagnostics/03fe7a0c.../`. The four report.txt files are unanswered. The
  frames were not converted: the brief already says they were taken after the artefact.
- Session settings (launch receipt): RTX 5060 Laptop, **scale 2** (draw resolution 2x2), render target
  1920x1080, guest 960x540, readback levers `d3d12_readback_sync_composite`, `_sync_targeted` and
  `_elide_unread` on.
- Log time zone: local time, UTC+2. The F10 lines at 15:31:32, 15:35:05, 15:40:24 and 15:41:36 match the
  packages at 13:31:32Z ... 13:41:36Z.
- Owner (relayed by the coordinator): "a white square again". The whole minimap quad turns white for a
  moment, which is how KI-084 looked.

## 1. What the counters say (per minute, all on/verify sessions of l120)

| counter | verify 58119911 | ON b143fe23 | ON 03fe7a0c |
|---|---|---|---|
| entries | 103k-267k | 100k-297k | 124k-293k |
| exits static / cold | 0-1.6k / 103k-267k | 18k-79k / 81k-218k | 26k-83k / 98k-216k |
| exits watch / gpu / stale / budget | 0 / 0 / 0 / 0 | 0 / 0 / 0 / 0 | 0 / 0 / 0 / 0 |
| exits src | 0-21 | 0-22 | 0-33 |
| refused texture / gpu / cap | 0-29 / 0 / 0 | 3-15 / 0 / 0 | 0-40 / 0 / 0 |
| cmp_diff (any) | **0** | **0** | **0** |
| cmp_same_invalid (verify) | 0.67M-1.8M | - | - |
| cmp_same_but_faulted (verify) | 3k-93k | - | - |
| global_fires | 0 (verify does not fire) | 92k-285k | 118k-286k (= the exits with a shadow) |
| bad / cmp_diff_valid | 0 / 0 | 0 / 0 | 0 / 0 |

How to read it:
- **No hot page ever changed in its requested bytes, in any session or mode** (`cmp_diff=0`, including
  `cmp_diff_invalid=0` in verify). In the on mode the shadow is taken once, every later compare is equal,
  and nothing is ever copied after the first shadow. The hot set is made of pages the old path reuploads
  although their requested bytes did not change. In verify, 1/3 of the compares are `same_invalid`: the old
  path reuploaded the page, and in about 95% of those the page itself did not fault (`same_but_faulted` is
  about 5% of them), so the 256 KiB widening from a neighbour's fault invalidated it.
- `cmp_same_but_faulted` = 3k-93k per minute in verify. These pages were **written** while hot, but not in
  the bytes the vertex/index request asked for (or the writes put back the same values). In the on mode, the
  host-buffer copy of those other bytes stays at the shadow snapshot. See candidate A.
- Exits `gpu:0`, `refused=gpu:0`, `watch:0` and `stale:0` everywhere: no resolve, GPU write, memexport,
  off-thread upload or watch registration ever touched a hot page. Texture take-outs (`src`) happen about 10
  times per minute. Texture refusals at entry are 0-40 per minute.
- Texture-cache rates (MCLA_LIVERY_DIAG2 minute lines) do not depend on the mode. Per frame, ON 03fe7a0c has
  45 texture loads, 42 invalidations and 3.0 reloads without invalidation. Verify 58119911 has 45, 42 and 3.0.
  The clear session 314bfd24 has 44, 41 and 3.0. Invalidations not caused by the GPU are 0-35 per minute in
  every session. I found no extra, missing or unscaled reload of the resolved surfaces that could be pinned on
  the lever. Caveat: the per-texture lines are rate-limited, so a single transient event would not show.
- **Deferred claims (coordinator question):** `MCLA_XFER_EXTENT deferred=...resolve:` comes in bursts in
  every session, whatever the mode. Verify: 0, 753, 0, 0, 0, 0, 0, 42, 1263, 0, 0. ON b143fe23: 878, 0, 0,
  0, 0. ON 03fe7a0c: 47, 84, 42, 398, 0, 0, 305, 575, 0, 756, 0, 0, 0, 852, 166, 0, 0, 749, 0, 0, 5. Clear
  39db5b41: 89, 0. Clear 314bfd24: 46, 0, 0. The `resolve:5` in the last minute of 03fe7a0c is one of these
  quiet minutes and does not single out the lever. `would_claim` and `exact` follow the frame count in every
  mode. With scale 2, the resolve path makes **no** shared-memory request
  (`d3d12/render_target_cache.cpp:2174-2190`: the request is only in the unscaled branch), and EDRAM ownership
  and deferred claims never read shared memory. The lever cannot change ownership timing through the resolve
  path.

## 2. Candidates, with reachability

### A. Frozen non-requested bytes of a hot page (reachable; the best match for the counters; mechanism not observed)
- The old path uploads the **whole page** whenever any request touches an invalid page
  (`shared_memory.cpp:962`, `AppendInvalidPageRuns`). That refreshes every byte of the page in the host
  buffer.
- The on mode compares only the requested bytes (`HotCompare` `:1850`, `RequestedSpan` `shm_hot.h:441`).
  When those are equal it does not copy, so the host buffer keeps the whole-page snapshot from the first
  shadow (`:1916-1924`).
- In verify, 3k-93k compares per minute landed on pages that were written since the shadow. In the on mode,
  those writes stay out of the host buffer until a request covering exactly those bytes sees a difference.
  That never happened (`cmp_diff=0`).
- Any GPU reader that reads those bytes without a request covering them gets stale data. Examples: a vertex
  fetch past the fetch constant's size (the shared-memory SRV is the whole 512 MiB, nothing is bounds-checked),
  a vertex slot whose request was skipped, or a second draw over the same page with a wider span read through
  another path.
- The verify gate cannot see this: it compares the same requested spans only.
- Symptom: a draw that reads stale bytes. A minimap quad whose UVs, colour or constants come from such bytes
  would draw white or wrong for as long as the bytes stay stale. This fits a momentary white square that
  repeats, and fits four F10 presses in 10 minutes.
- Unproven: no counter names the page or the reader.

### B. Lost 256 KiB widened invalidations (reachable; harmless as far as the code shows)
- In the old path, a write fault on hot page P widens to its 64-page block (`:1261-1288`). `FireWatches`
  (`:1329`) then fires the texture watches and the scaled-resolve watch over the neighbours.
- In the on mode, P no longer faults, so those spurious invalidations are gone.
- This only matters if a neighbour's guest bytes change **without its own fault**. I found no such writer:
  - The readback deliveries `memcpy` into armed GPU-written pages and fault normally (`d3d12/command_processor.cpp:4342`, `:7638`).
  - Texture pages are armed by their own load (`MakeRangeValid` `:651`).
- Low probability.

### C. Texture watches (not a hole)
- Texture watches use SharedMemory's own `watch_buckets_` (`:463-488`, linked into every 4 MiB bucket they
  span). There is no separate structure in the texture cache. The entry walk `HotEligibleLocked` (`:1805`)
  therefore sees every texture watch.
- Every watch is created in `MakeUpToDateAndWatch` (`texture/cache.cpp:981`, `:988`), called only from
  `CommitPreparedTextureLoad` on the command-processor (CP) thread. This always follows a `kTexture` request
  (`:758`, `:1287`), which takes hot and pending pages out first (`shared_memory.cpp:886-892`). It does so
  before the lock-free fast path (`:915`), and the normal path then uploads and arms the pages (`:962`, `:651`).
- The off-thread branch (`:497`, stale mark only) is not reachable. Log: `stale=0`.
- The converted index cache arms its own range (`primitive_processor.cpp:1381`).

### D. Spurious scaled-resolve global-watch fires (reachable; harmless here; a latent ordering bug)
- Each exit with a shadow fires the global watch with by_gpu=false for that one page (`:1770-1772`). With
  `pre_mask_resolve_l2_block` at its default true (`texture/cache.cpp:92`, `:1502`), the callback clears only
  that page's scaled bits.
- Hot pages carry no scaled bits: scaled bits sit on GPU-written pages, every CPU invalidation fires the same
  callback, and gpu refusals and exits are 0. So the 234,715 calls per minute are CPU cost only.
- Latent bug: `MarkRangeAsResolved` sets the scaled bits (`texture/cache.cpp:395-417`) and only then calls
  `RangeWrittenByGpu` (`:426`). That calls `HotExitRange(kGpu)` (`shared_memory.cpp:553`), which fires the
  global watch with by_gpu=false and **clears the scaled bits the resolve has just set**.
- At scale 2 no resolve request takes the page out beforehand. A texture over that page would then be
  re-created unscaled from guest memory, which is stale or white. That is the exact KI-084 look.
- Not hit in 03fe7a0c (`exits gpu:0`), but it must be fixed before any later run.

### E. Resolve destinations, memexport, readback delivery, `MakeRangeValid` off the CP thread (not reachable here)
- GPU-written pages are refused at entry, and `RangeWrittenByGpu` takes pages out before `MakeRangeValid`
  (`:553`, `:576`).
- An exit never copies anything. Copies go guest -> shadow -> upload only for hot pages, so no GPU result is
  ever overwritten by CPU bytes.
- The readback destinations are GPU-written, so never hot.
- Off-thread `MakeRangeValid` marks the page stale (`:615`). Log: `stale=0`.

## 3. Conclusion

- **(i) Most likely, with medium-low confidence (about 40%): candidate A**, a GPU read of bytes on a hot page
  that the on mode left frozen.
  - It is the only behaviour change the lever makes to GPU-visible memory in these sessions: no copies after
    the first shadow, while 3k-93k writes per minute land on hot pages outside the compared spans.
  - Exposure supports the lever as the cause. The defaults have run clean since l114 (10 minutes of races, a
    92-minute MAIN session, l117v, three l120 sessions today). The ON session had at least one event, with
    four F10s in 10 minutes.
- **Second:** a KI-084 residual that happened to show up in the ON session, about 30%. The deferred-claim
  pattern and the texture rates do not differ by mode, and the scaled resolve path never touches shared
  memory.
- **Next after that:** B (about 15%) and the latent ordering of D (about 10%, not hit here).

## 4. Minimal fixes

- **A:**
  - Option 1: compare and copy **whole pages**: `HotCompare` over the full 4 KiB (33 ns at 4 KiB, measured in
    l120 test 8). The host buffer then equals the guest page after every request, as the old path guaranteed.
  - Option 2: keep the span compare, but at the first compare in each frame also do a whole-page compare, and
    treat any difference as a change.
  - Either way, `cmp_same_but_faulted > 0` becomes a "page changed" signal.
- **D:** in `HotExitPage`, do not fire the global watches for `ExitReason::kGpu`, or fire them with
  by_gpu=true. The GPU data supersedes the CPU bytes.
- **B:** if it is ever implicated, in on mode emulate the widening: when a hot page's whole-page compare
  differs, fire range watches over its 64-page block, as `FireWatches` does.

## 5. The missing VERIFY

The current verify checks only the requested bytes. Add these counters:

1. `cmp_page_diff_span_same`: on each verify compare, also `memcmp` the whole page. Count "requested span
   equal, page different, and the old path uploaded". Each one is a stale-byte event in the on mode. It must
   be 0, or every case must be explained.
2. `tex_watch_missed`: in `FireWatches`, while the faulting page is hot (verify), count range-watch callbacks
   and their texture keys (first 16 as lines). These are the texture invalidations the on mode would not
   make. Must be 0.
3. `global_fire_extra`: global fires the on mode would add (exits with a shadow, by reason), with
   `global_fire_cleared` = how many of them changed scaled bits. Must be 0.
4. `gpu_exit_after_mark`: kGpu take-outs inside `RangeWrittenByGpu` whose page already has scaled bits.
   Must be 0.
5. The first 16 hot entries with page address and request source, so a frame of the minimap can be matched
   to its pages.

## 6. Deck plan

**Wait.** Nothing in A, B or D is PC-specific, and A would apply to the Deck too. Run deck/shm-hot-verify
only after the verify above exists, then deck/shm-hot only with the whole-page fix.

## 7. Questions for the owner

1. Was it the same white square as KI-084 (the whole disc or quad white, icons gone), or also a wrong or
   garbled tile or a missing map?
2. For how long: one frame, under a second, or several seconds?
3. Once or several times? Did each of the four F10 presses (13:31, 13:35, 13:40, 13:41 UTC) follow a
   separate white square?
4. In a race or in free roam; at a particular moment (start, a checkpoint, a map zoom change)?
5. If it shows again: a video, with F10 pressed while it is white.
