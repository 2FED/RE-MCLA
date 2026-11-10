# KI-074 recurrence: read-back delivery design (2026-11-10)

Read-only SDK design by an Opus subagent (SDK c40db4b), accepted by the primary agent as a design; the stale path is a hypothesis until the counters confirm it. Spark not used: source reading.

**Headline.** Source reading corrects the first diagnosis: the exempt steady key 039CA000 is never served stale (every delivery on l86 was a complete copy exactly one frame old; the "fast-wait" is already a real bounded wait). The remaining stale path is the cadence copy of the ~55 non-exempt read-back keys: every 4th resolve copies from the previous slot with no fence check, so with the GPU about a frame behind guest RAM gets older bytes or a torn mix (14.4 copies, 9.2 MB per frame on l86), and a deferred fence can still land before its resolve bytes. Unproven until measured. Recommended fix (d): `d3d12_readback_fenced_delivery` (default off): skip a cadence copy whose previous slot is unfenced and deliver at completion, before any fence store; no waits. First step: counters only (cad_unfenced etc.) in the next owner build, one night run.

---

# KI-074 recurrence (l86, ~50 fps): SDK design for a frame-rate-independent fix

Read-only design, 2026-11-10. Sources: SDK worktree private/l82-sdk-wt (c40db4b, SDK of l82-l86),
evidence M11-007/007c/007e, M11-ki074-*, M10-002 families, l86 session logs (private/owner-l86-flicker-20261110).
Nothing built, run or edited.

## 1. Current pipeline (from source)

- Fence observed: EVENT_WRITE_SHD -> CommandProcessor calls the D3D12 hook DeferGuestFenceWrite
  (src/graphics/d3d12/command_processor.cpp:3317-3342). Target = DeferredFenceTargetSubmission
  (include/rex/graphics/util/deferred_fence_writes.h:112-117): the open host submission, else the last ended
  one. Stored only when ID3D12Fence submission_fence_ GetCompletedValue() >= target (queue Flush,
  deferred_fence_writes.h:180-199; FlushDeferredFenceWrites cp.cpp:3344-3363). Service points: packet
  boundary, IssueDraw (cp.cpp:4112), WRM poll, idle loop (OnIdlePoll 3287, WaitIdleForDeferredMemoryWrites
  3419-3442), after every CheckSubmissionFence (5523-5525). Early submit only idle/WRM or GPU idle
  (PlanDeferredFenceProgress, dfw.h:135-150). cvar d3d12_fence_write_deferred default true (cp.cpp:47).
- Steady exemption: per-key consecutive-frame run (cp.cpp:4892-4905), IsReadbackSyncSteadyExempt +
  FilterReadbackSyncSteady (4943-4951; cvar d3d12_readback_sync_steady_exempt default true, cp.cpp:166).
  An exempt rule-c key is not forced (no targeted wait on its own copy).
- "Fast-wait" in code: for an exempt resolve in some mode (cp.cpp:5025-5065) the previous slot
  (resolve N-1 of the key, ~1 frame earlier) is polled; PlanReadbackSteadyDelivery
  (include/rex/graphics/util/readback_slots.h:264-278): complete -> kFast (copy now); written+submitted but
  not complete -> kWait = AwaitReadbackSubmissionBounded (cp.cpp:3243-3268, fence-only, budget
  d3d12_readback_sync_steady_fast_wait_ms = 20) then copy; else kForce (own slot, targeted wait).
  steady_fast_waits counts kWait. So a fast-wait IS already a real wait; it is a CP-lead bound at the
  steady resolve (~draw 34), not a stale delivery.
- l86 logs: steady_fast_copies == steady_exempt every minute, steady_fast_wait_timeouts=0,
  steady_fast_forced=0, wait max 4-7 ms, total 0.25-0.76 ms/frame. => every exempt resolve delivered a
  complete, exactly 1-frame-old copy of 039CA000. The exempt key is NOT the stale path.
- cp_lead: sampled at each frame end in EndSubmission (cp.cpp:5940-5947): SubmissionsInFlight and
  FrameEndSubmissionRing<8>::InFlight (include/rex/graphics/util/submission_stats.h:96-122), "frames"
  includes the frame just closed. l86 driving: cp_lead_frames avg 1.00, max 2; fence_late_f1 ~50-58 % of
  deferred fences, f2p 0.01-0.2 %.
- The lead-dependent stale path that remains (no completion check): the some-mode cadence of the
  ~55 NON-exempt keys. PlanReadbackResolve (readback_slots.h:132-166) copies every 4th resolve from the
  previous slot with no fence check (only the elision guard); with elide_unread the recorded copy is the
  one at resolve k-1, so at resolve k (per-frame key: one frame later) the slot is complete only if the GPU
  lags < ~1 frame. Otherwise the memcpy (cp.cpp:5402-5415) gives guest RAM resolve k-5's bytes or a torn
  mix while the GPU copy runs (the SDK's own comment, readback_slots.h:198-201, describes exactly this for
  the l75 steady cadence). l86: copies_now 14.4/frame, 9.2 MB/frame, memcpy 0.57 ms/frame.
- Order hazard (RE step 2, never done): deferred fences now land at GPU completion, but resolve bytes reach
  guest RAM only at the key's next (or 4th next) resolve, so a fence-synchronised CPU reader can see the
  fence before the data.
- Why lead matters for visible output: guest RAM of a resolve is only seen through CPU readers (M10-002:
  F7 luminance chain fmt 36 = auto-exposure, F8 64x32 HDR, F10 k_8 512x512/256x256 "occlusion or flare"),
  or through a CPU write fault that re-uploads a page from guest RAM. Night race + "headlight-lit hills" +
  whole buildings/walls/fences/signs fits auto-exposure (F7) and CPU-side visibility (F10) reading
  5-frame-old or torn data. Not proven: the counter in section 3 decides it.

## 2. Options

| opt | mechanism | CP cost @50 fps GPU-bound | risk / regress | removes stale read? |
|---|---|---|---|---|
| a | bound lead to 1 frame (wait for N-1 completion when lead > 1) | already in place at the steady resolve (kWait); trips on 0.2 % of fences -> ~0; a strict <1-frame bound = the pre-exemption targeted wait (6.3-6.5 ms/f PC, M11-007) -> ~-20-25 % fps | fps given back; cap in disguise | no (non-exempt unfenced cadence still happens whenever GPU lag ~1 frame) |
| b | deliver completed pending read-back copies before storing a deferred fence | 0 waits; memcpy moved | low; alone it is undone by a later unfenced cadence copy (regresses guest RAM) | partial (ordering only) |
| c | conditional exemption (wait when previous copy incomplete) | already implemented (007c, readback_slots.h:264-278); l86 0 timeouts | none new | no: exempt key already never stale |
| d (recommended) | fenced cadence + completion delivery: a non-exempt some-cadence copy whose previous slot is not complete (polled fence) is NOT copied; the key's newest recorded copy is delivered by the CP when its submission completes, before any deferred fence with target >= that submission is stored | 0 waits, 0 extra submissions; same bytes (<= 9 MB/f, ~0.6 ms/f memcpy) | delivery later than resolve but earlier than today's next-resolve copy; clobber risk lower than today; KI-047 forced a/b path unchanged | yes: guest RAM never receives an unfenced/torn slot, and data precedes its fence (hardware order) |
| d' | bounded wait on unfenced cadence slots (kWait for all keys) | ~1-3 ms/f (first wait per submission) | CP becomes the wall (CP 85-94 % busy) -> -5..-10 % fps | yes, but costs |

## 3. Recommendation: d (with b built in)

- cvar `d3d12_readback_fenced_delivery` (bool, default false, hot reload; host dev key
  `readback_fenced_delivery`). Off: plan, memcpy and lines byte-identical.
- Mechanism:
  1. PlanReadbackResolve input `fenced_cadence` + `previous_slot_complete` (polled fence, as 5030-5034):
     cadence copy only when complete; else `plan.cadence_deferred` (no memcpy, resolves_since_copy reset as
     if copied so the cadence does not re-fire next resolve).
  2. readback_pending_ gains a completion FIFO {submission, key, generation} appended in EndResolve for
     every recorded copy of a non-exempt key (exempt key optional, arm 2). `DeliverCompleted(completed)`
     pops the prefix <= completed and memcpys under the table lock only if generation is still newest and
     undelivered (existing DeliverPending rules; superseded entries skipped).
  3. Call sites (CP thread): FlushDeferredFenceWrites before the store loop (data before fence), and the
     same service points (packet boundary when completed advanced, CheckSubmissionFence). Never blocks.
  4. BeginResolve/Erase already drop entries before slot reuse/release; FIFO entries checked by generation.
- Counters (MCLA_READBACK minute/mark, appended only while on or counted):
  `cad_copies cad_unfenced cad_unfenced_served cad_deferred cad_unwritten done_delivered done_bytes
  done_superseded done_lat_ms_max done_lat_frames_max fence_before_data guest_regress lead2_frames`
  - cad_unfenced counted in BOTH arms (polled fence) = the discriminator; off arm cad_unfenced_served =
    cad_unfenced, on arm must be 0.
  - fence_before_data: a fence stored while a pending copy with submission <= its target is undelivered
    (on: must be 0). guest_regress: delivery of an older generation than already delivered (must be 0).
  - lead2_frames: frame-end samples with cp_lead_frames >= 2.
  - per-family split (depth vs colour, fmt 36/k_8/other) for cad_unfenced, to name the reader.
  - diag2 resolve flight-ring line field `prev_polled=complete|unfenced|unwritten cad=served|deferred`.
- Unit tests (SDK ctest): readback_slots_test (off = old plan bit for bit; on never reads an unfenced
  previous slot; deferral resets cadence; miss/forced/full unchanged; exempt path unchanged);
  readback_pending_test (FIFO prefix delivery, newest generation only, superseded, BeginResolve/Erase drop,
  no regression); deferred_fence_writes_test (model: data delivered before a same-target fence store;
  DeliverCompleted never waits; idle model unchanged); counters/line format and off-line unchanged.
- Owner A/B on the l86 successor: MAIN save, same night race in the hills, 1080p Unlocked, arms A
  (defaults) / B (`readback_fenced_delivery=true`), ABAB, >= 5 min driving each, F10 on any flicker plus one
  every ~2 min. PASS: B 0 flicker in 2 runs (A expected to flicker); B every minute cad_unfenced_served=0,
  fence_before_data=0, guest_regress=0, done_delivered + done_superseded = cad_deferred +- in-flight;
  fps >= A - 3 %; p99 not > A + 5 %; CP busy <= A + 0.5 ms/f; subm/frame unchanged; 0 frames >100 ms;
  no hang. Discriminator check: A's cad_unfenced share in flicker windows >= ~20 % of cad_copies and tracks
  steady_fast_waits. Falsifier: B flickers with served=0 -> not a read-back path; next suspect texture
  cache / upload reuse keyed on completion.

## 4. Interactions

- Deck/Wine (CP-bound, GPU not saturated): previous slots complete -> cad_unfenced ~0, behaviour = off; added
  cost = FIFO push per recorded copy + prefix compare at flush points (no waits, no submits, no vkd3d
  calls beyond the existing GetCompletedValue). Check subm/frame and memcpy ms unchanged.
- Locked 30: GPU has idle -> same as Deck, near no-op.
- Placed texture heaps: unrelated (read-back slots are committed readback buffers).
- l80 timer queue (timer_queue_blocking_wait/spin tail): no new CP waits -> no interaction.
- KI-074b despawns: seen in base 1440p and at results screens in every arm -> game-side; not expected to
  change. KI-067/072: own-build CPU contention; unrelated.

## 5. Size / first step

- Files: readback_slots.h (+plan inputs), readback_pending.h/.cpp (FIFO + DeliverCompleted), readback
  counters header (fields + format), d3d12/command_processor.cpp (cvar, plan input poll ~5005-5071, EndResolve
  call 5454, FlushDeferredFenceWrites 3344, CheckSubmissionFence 5523, minute line 6345), command_processor.h
  (members), 3 test files. ~300-400 lines incl. tests.
- Smallest first step: counter-only (cad_copies/cad_unfenced/per-family, prev_polled in diag2), no
  behaviour change, on in the l86 successor; one owner night run tells whether unfenced cadence copies
  rise with the flicker before building d.
