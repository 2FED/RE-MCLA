# M7-016: small-index caching and cache coherency

2026-09-05. M7-016 / KI-027 remain open; M7 remains **0/16**.
This follows the accepted [physical-backing return](M7-016-deck-physical-backing.md).

## Observations and decision

MCLA 0.14.1.0 / SDK 0.16.0.0 returned 1,213 new guest outputs in 100 seconds
(12.13/s). Native watch arm/disarm mean cost fell to about 17 us from 64-71 us.
The owner perceives some improvement but still reports poor performance.
Different routes/draw counts preclude a controlled FPS speedup claim. The full
profile/album and completed return are preserved; see the preceding report for
all identities. Temporary F10 ImGui repaints explain the Steam overlay spike;
they are not new guest frames and are excluded from guest-output counters.

Residual native watch work remains numerous: 896,601 arm and 336,432 disarm
calls in the interval. These counters combine clients and **do not establish
the primitive cache's share**. No GPU-busy percentage or raw dump stack words
are treated as a sampled CPU profile.

Source review found the port's `primitive_processor_cache_min_indices` default
was 0, despite the retained original declaration/comment using 4096 and noting
that caching small inputs can cost more than recomputation. This cache only
reuses CPU-converted/reset-processed indices within a frame; ordinary draws
that do not need CPU processing are unaffected. Each eligible cache miss arms
physical write watches. At threshold 4096, 4095 indices are recomputed; 4096
remain eligible for reuse. This is a bounded **throughput candidate**, not a
measured whole-title cache attribution or guaranteed FPS improvement.

## Regression-proven correctness fixes

Before changing production, nine GPU-free cases exercised the real Process
and Memory paths: **six failed / three passed**. The failing set includes
actual A/C/E alias write faults and rearming, not just a model of the logic.

SDK **0.16.1.0**, commit `2fdf4ea4de5ff3c0a686cd627bd865a03d28640a`:

1. Uses half-open overlap `entry_start < invalidation_end && entry_end >
   invalidation_start`. The old comparison against invalidation_end missed
   contained/equal-end entries and ordinary expanded bucket notifications.
2. Cancels insertion if overlapping invalidation arrives during conversion.
   The callback clears active size under the cache mutex; the destructor saves
   validity before clearing bookkeeping and inserts only a still-valid result.
3. Returns only the inspected invalidation/bucket range even when no entry
   matched. It must not authorize unwatching an uninspected live entry elsewhere.
   Current SharedMemory independently bounds this on 4 KiB hosts, so the old
   reply is not proven to escape buckets in the current Deck configuration.
4. Restores the 4096-index cache threshold. Small inputs are read/recomputed
   fresh, not reused without invalidation. SharedMemory uploads, GPU coherency,
   guest permissions, physical backing and guest timing are not bypassed.

No new public ABI is introduced. None of these tests proves that a specific
neon, shadow, reflection or minimap bug is fixed. Current-draw atomicity versus
simultaneous guest writes is not promised; the fix prevents subsequent stale
reuse after a cancelled conversion.

## Review and tests

Primary integration plus two bounded independent read-only reviews covered
range/lock semantics and test design. One reviewer supplied the disjoint CPU
fixture; primary integrated the four CPU implementation sources and snappy
directly into unit_tests, without linking the private renderer plugin or
duplicating rexruntime objects. No guest execution/device/window/title is needed.

Final coverage: **12 cases / 800 assertions** on RelWithDebInfo and Release.
It covers the default, 4095/4096/zero thresholds, fresh small mutations, exact/
containing/interior/partial/adjacent invalidations, non-exact invalidation,
no-hit unwatch bounds, an isolated unrelated-bucket fault, real A/C/E writes
and rearming, frame clear, deterministic invalidation during conversion,
disjoint active conversion retention, and a two-bucket index range.

Review caught the newly added crossing fixture's contradictory one-bucket
assertion. Its placement now uses the actual physical allocation (including
alias offsets), and its assertion is conditional. Initial post-fix run was
10/11; final 12/12 passed after correction. This was a test defect, not a new
production failure. Gaps: 32-bit index/multi-entry list exhaustive coverage
and true concurrent stress; reentrant callback coverage is not a thread race test.

Both full all-target configurations: **1,834 PASS / four known BitStream skips /
zero failures**, 1,838 registered. SDK AST scan and 1/1 fixture pass. Logs and
red/green evidence remain private under
`private/evidence/M7-016/deck-return-20260905-0.14.1.0`.

## Publication and parent validation

MCLA **0.14.2.0** consumes the tagged SDK. Clean publication receipt:
`private/evidence/M7-016/bundle-20260905-092703-c432b824/result.json`.
Syncthing root `private/bundles/M7-016/current.txt` selects
`mcla-0.14.2.0-win64-39F3F5564492EB26-CC3A3BF697F95FE2-58B3385C89D9E499`.
Manifest SHA-256 `03DDB7B01419550D9C7EF30456ECE4955F4F80E417E08A150FEC74F1AC9E29F6`;
lock SHA-256 `1903B9865FED87E4636C0CF54C0C2779035852C2036C1C6C69F27DEAA7BB7170`.
All 25 immutable files and relocated launcher/session/guest-free diagnostics
passed; relocated diagnostic process exited 0. Full returned profile/album
`3BBC44D0...` is preserved unchanged. Old mutable return `57DD284A...` was
verified into the private return archive before the obsolete payload was pruned.

Parent AST scan/3 fixtures pass. Both PowerShell 7 and Windows PowerShell 5.1:
analyzer 4 positive/8 negative; diagnostics 1/7/116 source checks; bundle
1/13/41 source checks; save lineage 46 checks; session 1/10; build matrix 1/6;
alpha 1/31. Production non-Fixture alpha verification passes on both hosts.
Current SDK bootstrap passes 12/12. Identity-only consumer updates retain
historical alpha evidence and all artifact/save validation requirements.
Final primary payload/launcher rehash passes again after documentation review.
All documentation link targets resolve; README-AI retains 151 task definitions /
151 criterion rows and M7 0/16. Independent final handoff review found no blocker.

A fresh Windows saved-gameplay/UI control was attempted but not launched:
computer-use's native helper failed initialization and its reset/retry, before
window selection. No custom Windows UI workaround or new gameplay PASS is
claimed. Prior 0.14.1 gameplay control is historical, not successor evidence.

## Next acceptance boundary

**Return accepted 2026-09-05:** the owner completed this check. Both F10s were
gameplay; 947 outputs/65s (14.57/s) with changed draw load is not a controlled
speedup. The full profile/album is unchanged and the complete return archived.
Owner reports steadier 10-15 FPS, lower 5-8 in pause, and the traffic light no
longer dithering. Pause was after the pair, not measured inside it. Both GDI
BMPs are black; they are not visual parity evidence. The next experiment and
all current identities are in [resolve-readback attribution](M7-016-deck-resolve-readback.md).
The original requested boundary below is historical, not a request to repeat it.

No Deck performance PASS is inferred from native tests or publication.
Next owner check stays short: same Proton/Desktop/settings and route if possible,
two F10s separated by about 60 seconds of driving, then normal exit and full sync.
Compare real guest cadence, native protect/fault counts and time, CPU deltas,
visible geometry and audible breakup. A change combines an independently needed
coherency repair and threshold policy, so any FPS change is not solely attributable
to the threshold. Do not ask for another long soak or physical wheel test.
