# M11-007: removing the per-frame KI-047 read-back wait (design, 2026-11-06)

Read-only design by an Opus subagent on 2026-11-06; accepted by the primary agent. Step 0 confirmed by the primary on the l72 PC kick sessions: MCLA_READBACK_SYNC matched_c sums 8,980 (256 arm) and 6,673 (128 arm) over ~3-minute races, i.e. about one rule-c force per frame, against matched_a 7/192 and matched_b 242/190 (the PC logs carry no `kind=composite` lines, so the key identity on PC is inferred from the Deck logs).

Read-only design. Sources: SDK worktree on `mcla/m11-content-unmount` `d08813b`
(`src/graphics/d3d12/command_processor.cpp`, `include/rex/graphics/util/readback_sync.h`,
`src/system/xmemory.cpp`), docs `known-issues.md` KI-047/KI-062, evidence
`M10-001-ki047-sync-fix-build-20261101`, `M10-001-720p-and-ki047-20261101`,
`M9-006-ki-047-diag-runs-20261031`, `M10-001-deck-l56-20261101`, `M10-l63-pc-ab-20261102`,
`M10-002-readback-design-20261101`, `M11-cp-cost-plan-20261106`, `M11-015-kick-interval-ab-20261106`,
and the two kept owner Deck logs of 2026-11-06 (`private/owner-deck-20261106-saves/*/mcla.log`,
diag v3 `kind=composite` lines and `MCLA_READBACK_SYNC` / `MCLA_READBACK` / `MCLA_PERF_SPLIT` minutes).
The l72 PC session logs were not found on this machine; PC statements below rest on the l63 PC
evidence and the l72 per-minute counters in the M11-015 report.

## 0. Headline finding

**The ~1 forced wait per frame is not the livery composite.** It is one steady key: a 256x256
k_8_8_8_8 colour resolve from EDRAM base 0 to guest physical `03702000` (262,144 bytes), issued at
draw 26 of every frame (title, menus and driving), resolved in consecutive frames
(`since_prev=1`), and consumed by the GPU as a tiled 256x256 texture in the same frame
(`tex_load ... base=03702000 tiled=1`, then `tex_inval ... by_gpu=1` the next frame). It matches
only **rule c** (square power of two 64..1024, EDRAM base 0, k_8_8_8_8; `readback_sync.h:106-116`);
rules a and b never fire for it (`other_keys=0`, not sporadic).

Evidence (Deck logs, 2026-11-06, diag v3 composite lines, budget-sampled):

| key | size | lines | forced | max consecutive frames seen | avg forced wait |
|---|---|---|---|---|---|
| `03702000` | 256x256 | 1,422 + 545 | all but the first (miss) | 154-162 (runs broken only by the line budget) | 5.2-6.0 ms |
| `050A0000` (livery level 0/1) | 1024x1024 / 512x512 | 75 + 56 | yes | 3 | 8.0-8.4 ms |
| `054A0000`, `055A0000`, `055E0000`, `055F0000` (livery mips/variants) | 512..64 | 7-11 each | yes | 3 | 0.5-1.1 ms |

Per-minute counters agree: Deck `matched_c` 933 / 1,394 / 1,234 per minute against 1,382 / 1,054 /
~1,090 frames (about 1 per frame), `matched_a` 0-48 and `matched_b` 25-51 (the livery events). PC:
l63 720p "1.02-1.21 forced waits per frame, rule c 98 %"; l72 1080p 1.04 forced waits per frame.
The same `03702000` key is listed in M10-002 family F10 ("256x256 fmt 6, per frame, GPU consumer")
and in the l50 PC world-load inventory (`M9-006-ki-047-diag-runs`, "256x256 k_8_8_8_8 at 0
(03702000)"), so the PC key is expected to be identical (guest physical addresses do not depend
on the scale); confirm it from the l72 PC logs (section 4, step 0).

So the KI-047 rule c has a per-frame false positive that costs ~6.3-6.5 ms/frame on the PC and
~7.1 ms/frame on the Deck. The true livery composites are a few dozen bursts per session.

## 1. Current synchronous path

1. **Resolve.** `IssueCopy` -> `IssueCopy_ReadbackResolvePath` (`command_processor.cpp` ~4300-4752).
   The rules see the live keys first: `ReadbackSyncEnabled` (2956), `EvaluateReadbackSyncRules`
   (4355-4383; `readback_sync.h:148-169`), then `PlanReadbackResolve` with
   `force_unless_miss = ShouldForceReadbackSync(...)` (4404-4411; `readback_sync.h:173-176`).
   The GPU copy shared memory (or the scaled-resolve downscale buffer) -> the key's read-back slot
   is recorded into the open submission (~4605-4616).
2. **Forced wait** (4642-4692): for a forced, non-miss resolve, `AwaitReadbackCopySubmission`
   (3020-3041) ends the open submission (`EndSubmission(false)`) and calls
   `CheckSubmissionFence(copy_submission)` (4756-4810), which blocks in
   `WaitForSingleObject(fence_completion_event_, INFINITE)` (4803) until the copy's submission
   completes; the full-drain fallback is `AwaitAllQueueOperationsCompletion`.
3. **CPU copy.** `std::memcpy(guest physical, mapped read-back slot, written_length)` (~4717-4723),
   i.e. the host writes the guest RAM directly; the guest CPU sees it through any of the physical
   views (A0000000/C0000000/E0000000).
4. **Why it blocks there:** the SDK cannot know when, or whether, the guest CPU will read the
   range, so a synchronous `full`-style resolve must make the bytes valid before `IssueCopy`
   returns (the guest may read right after the resolve's PM4 completes; the CP thread is the
   only place that can guarantee it). The resolve sits at draw 26 of frame N, so the wait mostly
   drains frame N-1's tail that is already submitted: GPU 96-97 % busy at the wait, backlog only
   ~40 draws (M11-015). Kicks (`MaybeKickReadbackSyncSubmission` 3043, called at 3583) cannot
   shorten it.

**Bytes.** The forced key copies 256 KB per frame. The ~9 MB/frame (PC) / 12.6 MB/frame (Deck:
`bytes_cpu_copied` 13.28 GB over 1,054 frames) of CPU read-back is the ordinary `some` traffic of
all ~56 resolves per frame (misses plus every 4th resolve per key; M10-002 families F1-F10), not
the composite. The wait cost is GPU latency, not copy size.

**Who reads what, and when.**
- `03702000`: no CPU reader is known. The GPU samples it in the same frame. Before l55 (l54 and
  earlier) this key was not forced (copied every 4th resolve from the previous slot) and no defect
  was ever tied to it. A CPU read has not been excluded either: the SDK has no read detection
  (`PhysicalHeap::EnableAccessCallbacks` asserts `!enable_data_providers`, `xmemory.cpp:1905-1909`).
- Livery composite (`050A0000` 1024x1024 + mip chain 512..8 in the same range, `054A0000`,
  `055A0000`...): rendered at the start of a frame (draws 3-17), resolved, and read by the guest
  CPU, which builds the car's 1024x1024 BC3 (DXT5) atlas from it (KI-033 / M8 vinyl evidence).
  The guest function that does this is **not named in any evidence** (the KI-047 and M8 analyses
  identify the data flow, not the function); a read trap would name it (thread id plus the host
  fault RIP mapped to the recompiled `sub_XXXXXXXX`). Timing of the first read relative to the
  resolve (same frame or later) is unmeasured.

## 2. Options

| option | mechanism | expected saving (CP busy per frame) | KI-047 risk | verdict |
|---|---|---|---|---|
| **f. Steady-key exemption of rule c** (new) | Rule c does not force a resolve whose key was resolved in each of the previous K frames (K = 30) and that matches neither rule a nor rule b. Track a per-key consecutive-frame run in `ReadbackBuffer` next to `last_used_frame`. Exempt resolves fall back to plain `some` (l54 behaviour for that key). | PC **-6.3 ms** (readback_wait 6.46 -> ~0.1; the livery bursts remain), Deck **-7.1 ms** | low: the livery keys show runs of at most 3 frames (10x margin); rules a/b still force every livery mip; the exempt key returns to its pre-l55 state, which never showed a defect | **recommended** |
| b. Read trap / lazy completion | Do not wait; arm NoAccess on the destination pages; the first guest touch blocks that thread on the copy fence, then memcpy and unprotect. | same -6.3 / -7.1 ms for `03702000` if never touched; on livery events the wait moves to the reading guest thread | medium-high: needs the read-watch plumbing that does not exist (data providers asserted off; `TriggerCallbacks` write-only), host code touching trapped pages under the global lock, Deck fault ~80 us; a missed trap = stale livery (the KI-047 failure itself) | later, as the general M11-007 step 2-4 machinery; use its census to back option f |
| a. One-frame-late delivery | Serve the previous resolve's slot (already what `some` does) | same as f for `03702000` | **high for the livery**: a one-shot composite has no "previous" content of the same car; serving the last slot is exactly the l54 KI-047 mechanism (`decision=deferred read=prev` 25 s before the catch). Visibly wrong at every car spawn / livery change | reject for the composite family; for `03702000` it is identical to f |
| c. GPU-side priority (separate queue / split list) | Run the 26 pre-pass draws + copy on another queue ahead of frame N-1's tail | at best part of the 6 ms, only if a second queue can see the EDRAM/render-target state | high complexity: the render-target cache and EDRAM emulation are single-queue; cross-queue fences per frame | reject |
| d. Guest-side hook / defer the consumer | Hook the CPU consumer and run it a frame later or on another thread | nothing for `03702000` (no known CPU consumer); livery events are rare | needs the unnamed atlas builder; changes game timing | reject (not where the cost is) |
| e. Smaller composite | Resolve fewer bytes | ~0 (the 256 KB copy is microseconds; the wait is GPU drain) | n/a | reject |

Note on kicks: with the exemption the forced waits nearly vanish, but `ShouldKickReadbackSyncSubmission`
(`readback_sync.h:259-269`) kicks only when the GPU is idle, so kicks also feed an idle GPU. Kick cost:
PC 0.69 ms/frame, Deck 1.4 ms/frame (1,476 ms per 1,054 frames). Measure `kick_draws=0` as a separate
arm after the exemption, do not assume the saving.

Expected frame effect on the PC (1080p, South Central Tour): CP busy 26.7 -> ~20.4 ms; the GPU
(22.4 ms/frame busy, 80-82 % load) becomes the wall, so roughly 36 -> 42-44 fps, bounded by the
GPU. The GPU idle bubble after each forced wait also disappears. Deck: CP busy ~49 ms/frame in
driving (`cp_busy_ms` 51,887 over 1,054 frames), the CP is the wall there, so -7 ms is ~14 % of
the CP frame.

## 3. Recommendation: option f, backed by a narrow read census

### Step 0: confirm the key on the PC (no build)
From the l72 PC session logs (diag v3 is on by default): tabulate `MCLA_LIVERY_DIAG2 kind=composite`
by `base`/`size`/`since_prev`/`sync_forced`/`sync_wait_us` and the per-minute `matched_a/b/c`.
Predict: `03702000` 256x256 at draw ~26, `since_prev=1`, ~98 % of forced waits. If another
steady key appears, it joins the same rule (no address constant in code).

### Step 1: SDK change behind a cvar (owner-internal build)
- `readback_sync.h`: add `kReadbackSyncSteadyMinFrames = 30` and a pure predicate
  `IsReadbackSyncSteadyExempt(r, run)`: `rules.composite_shape && !rules.shared_range &&
  !rules.sporadic && run >= K`. `ReadbackSyncRules` gains `steady_exempt`; `any()` for forcing
  ignores c when exempt (keep the rule bits for the diagnostics). Unit tests: steady key exempt after
  K frames, a run of 3 (livery burst) not exempt, any rule a or b overlap not exempt, a gap of one
  frame resets the run, a key re-used by a livery composite after the run is forced on the first
  non-consecutive resolve.
- `ReadbackBuffer`: `uint32_t consecutive_frames` updated next to `rb.last_used_frame`
  (`command_processor.cpp` ~4385): `previous_frame + 1 == frame ? run + 1 : (previous_frame == frame ? run : 1)`.
  Pass the run into `ReadbackSyncResolve` before `EvaluateReadbackSyncRules`.
- Cvar `d3d12_readback_sync_steady_exempt` (bool, hot reload, SDK default false); host dev key
  `readback_steady_exempt` in the dev-override file (as `readback_kick_*`), logged at start
  (`MCLA_READBACK_SYNC kind=start ... steady_exempt=1 steady_min_frames=30`).
- Counters on the existing minute line: `steady_exempt=`, `steady_exempt_keys=`, `steady_max_run=`,
  and one `kind=steady_key` line when a key first becomes exempt (base, len, size, fmt, EDRAM base,
  draw index) and when an exempt key falls out (gap, rule a/b). Keep `forced`, `forced_c`,
  `targeted_*`, `composite_read_prev`, `composite_not_copied` unchanged so KI-047 counters stay
  comparable.
- Optional census in the same build (if cheap enough to add safely): arm NoAccess on the pages of
  exempt keys only (one key, 64 pages) after their resolve and count guest read faults
  (`rw_touch_read`, thread id, host RIP) with a sticky "touched -> stop exempting this key and force
  again" fallback. This is the smallest slice of M11-007 step 2 and turns the remaining risk
  (an unknown CPU reader of `03702000`) into a logged, self-correcting event. If it needs the
  data-provider plumbing beyond a page-count NoAccess arm, ship the exemption without it and rely
  on the visual checks plus the l54 history.

### Step 2: owner A/B (PC, then Deck)
- PC, Arcade "South Central Tour", MAIN 1080p, two arms (`readback_steady_exempt` off / on), two
  fresh-process runs each, F10 at the same checkpoint, no foreign load (the M11-015 `find` process
  stopped). Accept on: `readback_wait` and `fence_wait` down by >= 5 ms/frame, CP busy down, fps up
  beyond the run spread, `forced_wait_failed=0`, `fallbacks_to_full=0`, livery `forced_a/forced_b`
  counts unchanged, `steady_exempt_keys` = 1 (or the named step-0 set).
- KI-047 livery routine (both arms, cold and warm per-build store, because every KI-047 catch was
  on a cold store): police car close-up in Photo Mode at the race start and after a pursuit spawn
  (1440p and 1080p); player car vinyl after the garage vinyl editor -> apply -> drive (KI-033); a
  traffic-dense block to force car spawns; garage car switch (new composite); title and menu car
  views. Each with one F10. Plus the KI-038/032 sign check and the KI-056 album preview (same
  read-back path family). Ten cold sessions is the KI-047 confidence bar from M10 (3 in 10 broke
  before l55); propose at least 3 cold + 3 warm with the police check before a volunteer default.
- Then `kick_draws=0` vs 512 as a separate arm with the exemption on.
- Deck: one A/B (exemption off/on) at the title and one drive.

## 4. Deck relevance
The Deck pays the same false positive: about 1 forced wait per frame on `03702000`, 3.6-5.2 ms
each, `readback_wait_ms` 7,497 per 1,054 frames = **7.1 ms/frame**, plus kicks 1.4 ms/frame, on a
CP-bound platform (CP busy ~49 ms/frame driving). The exemption removes the 7.1 ms with no
Wine-specific cost (no faults, no extra submissions), so it is the cheapest Deck CP win available;
the vkd3d fence round trip (the ~4.5 ms floor per wait noted in l64) no longer matters for the
steady key. Kicks are then the next Deck item (1.4 ms/frame, measure `kick_draws=0`).

## 5. Open points
- The `03702000` content is unidentified (2D, no depth, start-of-frame pass; possibly a reflection
  or UI render-to-texture). Not needed for the exemption, useful for the check list.
- The livery CPU consumer function is unnamed; the step-1 census (if built) names it on the first
  livery event if its pages are ever trapped (only the exempt key is trapped in step 1, so the
  livery reader stays unnamed until M11-007 step 2 proper).
- PC confirmation of the key identity (step 0) is pending.
