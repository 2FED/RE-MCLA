# M8-001: WGC clock-contract boundary after burst4

2026-10-04. Bounded primary-source research. This note supplements the
[temporal-model review](M8-001-wgc-temporal-model-20261004.md); it does not change
capture gates, validate a runtime, or reinterpret a failed result as PASS.

## New observation and its exact scope

The primary investigator supplied this burst4 failure tuple, with QPC frequency
10,000,000 Hz and a 1:1 conversion to 100-ns units:

- WGC `SystemRelativeTime`: `2254656574196`.
- Host `GpuReadyQpc`: `2254656550812`.
- Host catch observation: `2254656551305`.
- No `Map`/pixel read occurred for the rejected frame.

Integer subtraction gives a source timestamp **23,384 ticks / 2.3384 ms ahead
of GPU-ready**, and **22,891 ticks / 2.2891 ms ahead of catch**. This research
recomputed those differences, but did not independently inspect private native
evidence. The prior fixture6 comparison was only 14.8 microseconds ahead of
acquisition; moving the check later did not establish a universal ordering
guarantee. Burst4 remains FAILED under its frozen gate.

## What the primary contracts establish

**WGC supplies compositor time, but no explicit host-ordering inequality.**
Microsoft describes `SystemRelativeTime` as the QPC time of compositor rendering.
The property page and its MicrosoftDocs source provide no numerical error bound
or explicit guarantee `timestamp <= dequeue`, `timestamp <= local GPU query
completion`, or `timestamp <= CPU readback`. The wording is consistent with an
event-time interpretation, but the inspected documentation does not explain the
observed future-relative value. It is not evidence that such values are intended,
and it is not sufficient to diagnose a Windows defect.
[API documentation](https://learn.microsoft.com/en-us/uwp/api/windows.graphics.capture.direct3d11captureframe.systemrelativetime?view=winrt-26100),
[MicrosoftDocs source](https://github.com/MicrosoftDocs/winrt-api/blob/docs/windows.graphics.capture/direct3d11captureframe_systemrelativetime.md).

**Representation and QPC ambiguity do not explain this tuple.** TimeSpan raw
counts are 100-ns units; QPC tick duration is determined by its frequency.
At the supplied 10-MHz frequency those units coincide without rounding.
Microsoft documents cross-thread ordering ambiguity of one QPC tick; 23,384
ticks is not that one-tick case. These facts exclude these particular arithmetic
explanations given the reported identities; they do not establish the actual
cause or a suitable tolerance.
[TimeSpan](https://learn.microsoft.com/en-us/uwp/api/windows.foundation.timespan?view=winrt-26100),
[QPC guidance](https://learn.microsoft.com/en-us/windows/win32/sysinfo/acquiring-high-resolution-time-stamps).

**GPU completion is not a documented WGC clock barrier.** A D3D11 event query
reports completion of GPU command processing. Its contract does not connect that
completion to the temporal interpretation of metadata obtained from a separate
WGC frame. Finishing the consumer's copy is necessary for safe readback, but
does not, by itself, certify how WGC assigned the frame timestamp.
[D3D11_QUERY_EVENT](https://learn.microsoft.com/en-us/windows/win32/api/d3d11/ne-d3d11-d3d11_query).

**Predicted presentation exists elsewhere, not as a proven explanation here.**
DirectComposition explicitly exposes `nextEstimatedFrameTime`, the estimated
time of the next displayed frame. Its documentation distinguishes estimates
from actual composition and warns that scheduling/rate can change. The
compositor-clock guide also distinguishes creation, CPU confirmation, GPU
completion, and per-target presentation. Neither inspected source identifies
WGC `SystemRelativeTime` with `nextEstimatedFrameTime`. Therefore planned
presentation, vblank prediction, timestamp rounding inside DWM, cross-adapter
behavior and driver error remain hypotheses, not findings.
[Frame statistics](https://learn.microsoft.com/en-us/windows/win32/api/dcomptypes/ns-dcomptypes-dcomposition_frame_statistics),
[Compositor clock](https://learn.microsoft.com/en-us/windows/win32/directcomp/compositor-clock/compositor-clock).

The bounded search found an open timestamp-related report in Microsoft's sample
repository, issue127, concerning window-update versus compositor-update time.
The inspected page contained no authoritative resolution of future timestamps.
Its reporter's observations are not adopted as a Microsoft contract or as a
reproduction of this failure.
[Search lead only](https://github.com/microsoft/Windows.UI.Composition-Win32-Samples/issues/127).

## Implications for both ends of the freshness bracket

Inference: once a source timestamp can numerically lead the host observation,
the currently assumed common event-time relationship is unresolved. Keeping
`source >= arm` while merely relaxing `source <= GPU-ready` would not establish
post-arm pixel production: a leading label can pass the lower test even if the
underlying content existed before arm. This is a loss of proof, not a claim that
such pre-arm content was actually captured in burst4.

Likewise, an empty pool followed by a returned frame proves availability/order
of queue operations, not physical production after the empty poll. The API
returns the next pooled frame and null when empty; it does not provide that
extra temporal attestation.
[TryGetNextFrame](https://learn.microsoft.com/en-us/uwp/api/windows.graphics.capture.direct3d11captureframepool.trygetnextframe?view=winrt-26100).

Do not add an observed 2.3384-ms allowance, clamp timestamps, wait until the host
clock catches up, reset arm, or select another favorable frame. Such changes
could hide the mismatch without explaining it. Waiting changes transport time,
not the source timestamp's evidentiary meaning. Earlier pixel/ownership/cleanup
results retain their limited value; they are not universal freshness guarantees.

## Defensible bounded alternatives, pending primary review

1. **Strict route:** preserve current checks and their NO-GO. Continue only
   metadata diagnosis under a separately reviewed gate. A later host check is
   not a demonstrated fix; even a successful run would not prove universality.
2. **Transport-only route:** separately version a diagnostic that records actual
   host observations, ownership/geometry, normal cleanup and raw source labels.
   Do not assert the host arm/readback bracket contains compositor rendering.
   Source-clock deltas can be described as observed label differences, with
   unknown event-time error; do not promote them to bounded physical/guest timing
   or feed renamed point values into the existing bracket extractor. Any pixel
   admission under a different freshness proof requires an explicit new review.
3. **Native producer oracle:** use only an owned synthetic window, fresh
   session-specific identifiers and a monotonic, non-wrapping per-paint ID
   encoded in pixels. Record producer QPC before drawing the new ID and after
   checked paint cleanup, independently of the capture timestamp. Cross-check
   every accepted pixel ID against its producer record; repeated palette phases
   alone are ambiguous. For a causal freshness test, send a bounded fixture-only
   challenge after a recorded host arm and draw a never-previously-used ID. Seeing
   that exact ID proves the captured content incorporates that post-arm change
   without relying on WGC's source-clock inequality. This is an experiment design,
   not an implemented or tested proof.
4. **Interpret oracle limits:** a producer draw bracket is not the time a display
   scanned the pixels or a bound on WGC timestamp error. `GdiFlush` flushes the
   calling thread's batch, not a global compositor timeline. `DwmFlush` concerns
   the calling application's queued DirectX updates and explicitly does not
   flush the full session. Do not use either as a universal WGC barrier.
   [GdiFlush](https://learn.microsoft.com/en-us/windows/win32/api/wingdi/nf-wingdi-gdiflush),
   [DwmFlush](https://learn.microsoft.com/en-us/windows/win32/api/dwmapi/nf-dwmapi-dwmflush).
5. Preserve full raw metadata and declared selection/deadline/queue policy.
   Classify stale, duplicate, missing and mixed-ID observations honestly. Fixture
   oracle success would validate only that scoped fixture route; it cannot be
   transferred to an uninstrumented game or prove guest `Present` equivalence.

Recommendation: do not ask the owner to rerun the same timestamp gate or claim a
timing fix from a tolerance. Primary should first choose between retaining strict
NO-GO and designing the bounded producer oracle/transport diagnostic. No game
test, input injection, display capture, milestone credit or upstream submission
is authorized by this research note.

## Work record and unresolved items

Read research skill and prior temporal note, inspected Microsoft documentation
and Microsoft-owned repository pages, and recomputed supplied integer deltas.
Only this file was written. No native execution, GUI action, private write, build,
Git/index/status operation or test-suite run. No new external issue was filed.
The WGC timestamp assignment mechanism and its total error/ordering guarantees
remain unresolved; no official primary source found in this bounded search
establishes a safe replacement tolerance. Primary owns evidence verification,
design selection, integration, testing and any later owner action.
