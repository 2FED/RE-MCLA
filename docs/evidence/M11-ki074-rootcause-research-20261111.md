# KI-074 root-cause research (2026-11-11)

Read-only research by an Opus subagent (SDK ef72fd1 and generated title code), accepted by the primary agent as research; hypotheses unproven. Spark not used: source reading.

**Headline.** No mechanism proven. Ruled out with source evidence: host GPU ordering hazards (single D3D12 queue, in-order submission, deferred destruction), guest RAM reaching the GPU (0 unscaled loads of read-back ranges in 1,600 sampled), guest code skipping updates on a late fence (all fence readers are blocking waits; the one non-blocking query keeps its previous result), and any event unique to "F-1 unfinished" frames in the F10 flight rings. Ranked: (1) age of the NON-exempt read-back keys in guest RAM (cadence copies every 4th resolve from the previous slot; when F-1 is unfinished the data is older, torn or delivered later) read by a raw-pointer guest CPU reader that the l76 lock probe could not see; fits dawn (28 new keys, 5.6 -> 9.3 MB/frame) and survives the fenced-arm result; (2) a game-side race between the render worker blocked in fence waits and the main thread preparing F+1; (3) a missing barrier (no candidate). Probes proposed: readback_age_inject under strict, a read watch on read-back pages that logs the guest PC of each reader, guest_swap_hold. Side finding: every guest occlusion query ends the submission and waits for the GPU with no bound (full CP-to-GPU drain; on by default), a possible daytime performance cost and KI-041 lead.

---

# KI-074 root cause research notes (read-only, 2026-11-11)

Sources: SDK private/l91-sdk-wt (ef72fd1), generated/default (TUn = mcla_recomp.n.cpp), F10 rings in
private/owner-l88/l90/l91-flicker-20261111 (log-tail.txt only). Script: ring.py (this folder).

## Killed / weakened
- Host GPU ordering: one direct queue only (no COPY/COMPUTE queue anywhere in src/graphics, src/ui).
  Texture reloads, resolves, uploads, presenter paint all go through ExecuteCommandLists on it,
  in order (command_processor.cpp:6136/6151; presenter refresher EndSubmission(true) at :2713 before
  the mailbox publish; d3d12_presenter.cpp:546-550 tracker).
- Texture destruction / bindless SRV release only after last-use submission completed
  (pipeline/texture/cache.cpp:280; d3d12/texture_cache.cpp:1492-1504 placed heap; :2443).
- Guest RAM -> GPU path for read-back keys: 0 of 1600 sampled tex_load events across all three
  evidence folders are unscaled loads of a read-back range (1078 sr=1 rb=1, 522 sr=0 rb=0).
  Scaled resolve writes only the scaled buffer (render_target_cache.cpp:1406-1445) and marks the
  unscaled range valid (shared_memory.cpp:460-476), so guest RAM bytes never re-upload.
- Guest D3D fence readers (device+10896 -> writeback block: +0 fence, +4 CP read ptr, +60, +64.. swap
  ring): all in TU27; every reader is a blocking wait/kick (BlockOnFence 82411E98, ring space
  82411180/82411218, lock 82421CA0, swap ring 82419E98) or a fence stamp (SetTexture 82410F88 writes
  resource+8 = device+10908). No "skip update if busy" branch found.
- The only non-blocking fence test: D3DQuery_GetData sub_8241E088 (TU27:34671; type 9 occlusion,
  10 extents, 8 event; returns S_FALSE when the query fence has not passed). One game caller:
  rage wrapper sub_82179720 (TU2:41113) <- sub_822DD088 (TU14:57992), a single global object
  0x827E1048 (two queries ping-pong at +16/+20, index +12; on not-ready it KEEPS the previous sample
  count; +4 = samples/total clamped) called once per frame from sub_822DD720 (TU14:58980) <- TU16:27151.
  Looks like the sun/flare visibility. Not-ready keeps old value -> no oscillation from lateness.
- F10 rings: per-frame composite of 039CA000 only + sampled tex/resolve. Frames with
  steady_delivery=wait (595) vs fast (3077): same event kinds, same rates (inval 6.9 vs 4.7 %,
  load 4.4 vs 2.9 %); no event unique to "F-1 unfinished" frames. Ring cannot show lead per frame.

## Side finding (perf, possibly KI-041 light flicker)
- EndGuestOcclusionQuery (d3d12/command_processor.cpp:8169-8203): EndSubmission then
  CheckSubmissionFence(just-submitted) = WaitForSingleObject INFINITE (:5649-5660): a full CP->GPU
  drain per guest occlusion query; occlusion_query_enable default true (command_processor.cpp:54),
  no host override. If the flare query runs every frame in daylight it drains the GPU each frame.
  On BeginSubmission failure etc. fallback writes fake 1000 samples (fully visible).

## Live hypothesis ranking
1. Guest-RAM age of NON-exempt read-back keys, read by a raw-pointer guest CPU reader (unproven
   reader). "some" cadence: only every 4th resolve copies, from the previous slot
   (command_processor.cpp:5148-5160, 5541-5566); with F-1 unfinished the slot is unfenced (default:
   older/torn bytes) or skipped and delivered later (fenced arm). Age therefore grows exactly with
   "F-1 still on GPU mid-frame" and is 1 resolve in steady-off/cap44/strict/Locked/l77-fence runs.
   Matches every clean/flicker split incl. dawn (cad_unf_other peak at presses, 28 new fmt-2 keys,
   read-back 5.6 -> 9.3 MB/frame). The lock probe cannot see raw reads (l90 doc), so "no CPU
   reader" is not established for these keys.
2. Game-side timing reaction: the render worker blocks in fence/ring waits mid-frame while the
   main thread prepares F+1 (sub_82305D00 hand-off); a guest data race on shared per-frame state.
3. GPU hazard inside a submission (missing barrier) whose visibility depends on overlap: weak,
   no candidate found.
