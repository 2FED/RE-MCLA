# M7-016: physical-watch disarm fragmentation

Date: 2026-09-06. Task M7-016 / KI-027 remains open. Diagnostic-only follow-up
to the [CPU attribution](M7-016-deck-cpu-attribution.md),
[readback control](M7-016-deck-readback-cost.md), and
[Proton C++ runtime correction](M7-016-proton-cpp-runtime.md).

## Question and scope

Does the existing disarm loop issue many separate native protection calls
where two complete runs are separated only by committed, guest-read/write,
unwatched pages inside the same callback-approved physical alias range?

`watch_disarm_safe_gap_splits` counts those structural opportunities. It does
not combine ranges, change native protection arguments or watch bits, relax
guest rights, change callback expansion, or estimate saved time. Both entire
neighboring runs must qualify; leading/trailing gaps, unsafe permissions,
uncommitted pages, separate callbacks/aliases and unprotect=false are excluded.
The existing 4 MiB excess-unwatch bound and alias clipping remain unchanged.

Allocation updates page metadata under the heap mutex, not necessarily the
global region lock. Independent review caught new unsynchronized gap reads
before deployment. The final implementation tries the heap mutex without
blocking, after all callbacks. On contention it preserves the legacy
watched-only predicate and counts `watch_disarm_gap_lock_misses`; it does not
read additional gap/allocation metadata or wait while holding the global
lock. This is not a repair of pre-existing watched-page synchronization.

Counters are relaxed cumulative snapshots. Lock misses, protection failures,
or an interval with more gap splits than native disarm calls disqualify the
analyzer's completeness indicator. Even a complete observed interval is not
a coherent frame transaction or a future performance guarantee. Extra
try-lock/page classification overhead must be considered in physical results.

## Consumer contract

The SDK exports both counters through the graphics snapshot. The host keeps
them in raw F10 state and default-off `mcla_readback_cost_probe` sample v2.
The analyzer accepts exactly v2 or the original v1, rejects mixed-schema
intervals, and explicitly marks historical gap metadata unavailable. It does
not synthesize zero values for old samples. No audio or gameplay PASS is
inferred from a nontrivial capture; actual saved gameplay/pause must be viewed.

## Review and verification

- Primary source review and independent follow-up accepted the nonblocking
  remedy; native call predicate/address/length ordering remains unchanged.
- Eleven GPU-free cases cover complete runs, safe/unsafe gaps, state/rights,
  callback intersections, excess-unwatch cap, A/C/E mappings, repeated calls,
  concurrent aggregation, deliberate metadata-lock contention/resumption and
  same-thread recursive ownership. Larger host-page platforms are not covered.
- Root AST scan/3 rules, serializer133 checks/0 failures, Python28 tests;
  diagnostic contracts pass PowerShell7 and5.1:5 positive,20 negative,170 source.
- Final RelWithDebInfo and Release all-target builds and full CTest:1,856
  executed and four known BitStream skips (1,860 registered), zero failures.
  Focused Release11 cases/479 assertions pass. SDK locally tagged0.19.0.0 at
  c2e67161f1207ddcee609507c399efe7a1981b72; matching Release install verified.
  Preliminary pre-lock suites are retained but do not verify the final fix.
- Logs remain private under `private/evidence/M7-016/disarm-gap-locked-*`.

## Actual Deck measurement

The isolated Deck control completed in 154.075 seconds with exit0 and no
force cleanup. It used Proton9, Desktop Vulkan, 1280x720 windowed, stock fast
readback, the same owner-profile source and copied cache, and the reviewed
official app-local CRT without DLL overrides. No TOML was installed in this
test bin: its explicit paths/settings cannot be silently overridden by the
portable configuration. Published bundle, source profile and source cache
hashes remained unchanged. No game/perf attachment remains.

Primary inspection of the actual returned images confirms the saved Eclipse
in street gameplay with minimap/speed HUD, followed by the real PAUSE menu in
both interval endpoint captures. These are not PRESS START frames. This
confirms autonomous loading, not moving gameplay/audio or GamingMode parity.
The collector's original `visual_validation_pending` field is immutable;
this report records the subsequent human-agent image review separately.

Exact measured identities (before the parent version-only bump):

- Parent source0.17.0.0, base d44d1b6 plus the reviewed diagnostic changes;
  SDK0.19.0.0 / c2e67161f1207ddcee609507c399efe7a1981b72.
- Host SHA256 `0E90319E41606927BA71E96880355F527F8761FBA917050C639F0EDFD8574FAD`.
- Runtime `DBFF1BE301B8E83578A601F41C0B641C4E9228EAAC23297D5B7C431661D54A1F`.
- GPU `2D26216495195318B899CE8CA300499E0A7E43D8FAFFE458DDDD98045341E852`.
- The reviewed source checkpoint becomes0.18.0.0; that does not relabel this
  measured binary or claim a new publication.

The exact 20.000047-second pause interval yields:

| Counter | Interval result |
|---|---:|
| New guest outputs | 255 / 12.749970 per second |
| Disarm native calls / inclusive time | 84,274 / 1.129031 seconds |
| Safe internal disarm gap splits | 65,942 / 78.25% of call count |
| Metadata lock misses / protection failures | 0 / 0 |
| Arm native calls / inclusive time | 163,464 / 2.371418 seconds |
| Arm gap splits | 77 |
| Resolve copies / inclusive copy time | 16,575 / 1.185834 seconds |
| Copied bytes | 9,446,277,120 |
| Resolve waits | 0 |
| Draw packets / inclusive time | 1,353,923 / 12.358237 seconds |
| Binding updates / inclusive time | 1,337,348 / 2.135863 seconds |

These nested spans overlap; do not add or subtract them as an exclusive
frame budget. Gap splits indicate structurally eligible joins, not a proven
safe implementation, a measured saved-time fraction, or a speedup. There
were two cumulative metadata misses before both endpoints, none inside this
interval. Extra diagnostic try-lock/scan overhead has not been isolated.

The verified private return is `private/evidence/M7-016/disarm-gap-return-20260906`:
24 regular files /22,669,366 bytes, every member hash checked, archive SHA256
`9C98890BB1ECA01C313053556BD27627DB1C678DD97506E575C8CC734BFF3B67`.
Raw samples are under `verified/run/fast-gap-control/user/mcla-readback-cost/`;
rerun `scripts/analyze-readback-cost.py` on `samples.jsonl`. Frames, logs and
experimental profile remain private. Do not promote the experimental profile.

## Decision and next lead

Do not implement protection coalescing merely because the call-count ratio
is high: even all measured disarm work is only about1.13 seconds of20. It
cannot by itself explain the severe stock-FPS deficit; arm fragmentation is
negligible. Keep the counters and defer policy changes.

The independent source/profile follow-up selects eligible PM4 Type-0
constant-register batching as the next bounded experiment. The existing
wrap-safe bulk route is already used elsewhere, while Type-0 still invokes
the virtual per-register writer repeatedly. Accepted gameplay GPU leaf
samples in the base writer, Vulkan writer and Type-0 handler total148/1419
(~10.4% residency), not an exclusive recoverable budget; qualifying constant
traffic is not yet quantified. Preserve repeated-register and side-effectful
fallback order, endian/ring semantics and both backend invalidation contracts.
Require GPU-free edge fixtures, independent review and a matched private
A-B-A before accepting any benefit. No new timers, broad threading change,
owner test request, normal-bundle publication, or30FPS promise.

Additional parent gates: non-force SDK19 codegen keeps all67 files /
133,916,451 bytes identical (65 compiled C++ objects); AST scan/3 fixtures,
dualPS portable3/34/45, lineage48, build-matrix1/6 and alpha1/31 pass.
Bootstrap12/12 accepts exact installed SDK19. Review caught one escaped
old-SDK regex in the alpha verifier; corrected before its final dualPS pass.
