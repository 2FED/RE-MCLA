# M8-001: WGC temporal model research

2026-10-04. Primary-source research, not runtime acceptance or a change to the
frozen harness gates. Scope: distinguish compositor sampling time from capture
transport time. No native test, game, save, desktop capture or input was run.

## Documented facts and limits

- `Direct3D11CaptureFrame.SystemRelativeTime` identifies the QPC time when the
  compositor rendered that captured frame. It is not documented as the time the
  consumer dequeued it, copied it, or finished CPU readback. The property page
  does not specify a numerical error bound, strict uniqueness for successive
  returned frames, or equivalence to a game's `Present`/guest frame. Those claims
  cannot be inferred from the timestamp name.
  [Microsoft property contract](https://learn.microsoft.com/en-us/uwp/api/windows.graphics.capture.direct3d11captureframe.systemrelativetime?view=winrt-26100).
- `Windows.Foundation.TimeSpan` stores signed 64-bit durations in 100-nanosecond
  units; C++/WinRT exposes the underlying duration count through `count()`.
  This is a representation unit, not a promise of 100-nanosecond measurement
  accuracy. Raw `SystemRelativeTime().count()` must not be compared directly to
  raw QPC ticks unless their unit conversion is established.
  [Microsoft TimeSpan contract](https://learn.microsoft.com/en-us/uwp/api/windows.foundation.timespan?view=winrt-26100).
- QPC is nondecreasing and independent of UTC/system-clock adjustments. Its
  frequency is fixed at boot and consistent across processors. Microsoft warns
  that readings from different threads differing by one tick have ambiguous
  ordering; accuracy depends on hardware characteristics. Consequently,
  nondecreasing QPC does not by itself promise distinct WGC frame timestamps,
  a hard scheduler deadline, or a total WGC timestamp-error bound.
  [Microsoft QPC guidance and FAQ](https://learn.microsoft.com/en-us/windows/win32/sysinfo/acquiring-high-resolution-time-stamps).
- WGC explicitly supports `SystemRelativeTime` for synchronizing media. A frame
  is checked out from its pool until its lifetime ends; references to its
  underlying surface must not outlive that checkout. `FrameArrived` denotes
  availability, and applications can also pull frames manually. The lifecycle
  guide gives no guaranteed frame-delivery frequency or promise that every
  refresh, application draw, or identical-content interval produces a frame.
  [Microsoft screen-capture lifecycle](https://learn.microsoft.com/en-us/windows/apps/develop/media-authoring-processing/screen-capture).
- `TryGetNextFrame` returns the next pooled captured frame, or null if empty.
  It is not documented as returning the newest frame, nor as attesting that a
  subsequently returned frame was composed after the caller's last empty poll.
  [Microsoft dequeue contract](https://learn.microsoft.com/en-us/uwp/api/windows.graphics.capture.direct3d11captureframepool.trygetnextframe?view=winrt-26100).
- `SetTimer` specifies a timeout in milliseconds, while `WM_TIMER` delivery is
  low priority and depends on the message queue. Therefore the fixture's
  100-ms timer is not an independent exact 10-Hz clock standard.
  [SetTimer](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-settimer),
  [WM_TIMER](https://learn.microsoft.com/en-us/windows/win32/winmsg/wm-timer).
- GPU copying and reading completion are separate events: `CopyResource` is
  asynchronous and can enter a command queue; `GetData` can report unavailable
  data before later reporting completion. Their host durations are not defined
  as compositor timestamps.
  [CopyResource](https://learn.microsoft.com/en-us/windows/win32/api/d3d11/nf-d3d11-id3d11devicecontext-copyresource),
  [GetData](https://learn.microsoft.com/en-us/windows/win32/api/d3d11/nf-d3d11-id3d11devicecontext-getdata).

The opened `MinUpdateInterval` property page provides a signature but no useful
rate guarantee. No claim that WGC emits only changed frames, emits unchanged
frames at refresh rate, or can be forced to exact 10 Hz is adopted here.
[Microsoft property page](https://learn.microsoft.com/en-us/uwp/api/windows.graphics.capture.graphicscapturesession.minupdateinterval?view=winrt-26100).

## Consequence for the current gate

Repository sources inspected: `scripts/mcla_cycle_extract.py`,
`scripts/mcla_capture_fixture.py`, and the burst helper/protocol. `_series`
currently requires the entire adjacent sample-gap interval to fit 80..120 ms
at 10 Hz, not merely each capture duration to be below 200 ms.

For adjacent brackets `[a_i,b_i]`, define width `w_i=b_i-a_i`.
The accepted gap interval is `[a_next-b_i, b_next-a_i]`; its width is
`w_i+w_next`. **Necessary condition: adjacent capture widths sum to at most
40 ms.** This algebraic constraint follows directly from the current test and
does not depend on Windows behavior. With identical 100-ms arm spacing, each
width must be at most 20 ms to meet both gap limits.

Inference: a post-arm freshness rule can require waiting for the next compositor
frame, then GPU readback. An unsynchronized 100-ms fixture producer and 100-ms
consumer can therefore make this gate infeasible even when every returned pixel
is correct. The documentation supports the possibility, not its occurrence or
precise cost in the failed run.

The primary investigator reported burst attempt 2 failed `Guard 15` before timed
slot 18, with lateness at least100ms at the guard and107.8573ms at the subsequent
catch observation, and no retained successful-prefix slot timing
metadata. This research did not inspect private evidence. That result must stay
FAILED: it does not isolate scheduler delay, producer delay, GPU delay, or a
driver problem, and cannot establish that source-timestamp cadence would pass.

## Recommended acceptance design, pending primary review

Use two explicit temporal records rather than laundering one into the other:

1. Preserve actual host schedule/arm/dequeue/copy/GPU-ready/postcopy/close QPC
   observations, process/HWND ownership, crop/geometry/visibility guards, queue
   eligibility, bounded age/deadline, and normal helper/Job cleanup. These prove
   containment and transport/freshness conditions, not animation speed.
2. If adopted, define a separately versioned **compositor-source-time** series
   carrying original 100-ns timestamps, immutable frame identity/pixels and
   links to those host records. Use it only for recurrence in captured compositor
   images. Do not overwrite `qpc_before`/`qpc_after`, silently pass zero-width
   pseudo-brackets to `_series`, or reinterpret old failed receipts as PASS.
3. Review and declare the uncertainty model before claiming elapsed-time bounds.
   Integer floor/ceiling conversion can bound arithmetic quantization; it does
   not bound undocumented compositor instrumentation error or guest-presentation
   offset. If those uncertainties remain unknown, label the result a
   source-clock observation, not a certified physical/game timing bound. A
   replacement uncertainty bound must have explicit supporting evidence, not
   merely a convenient 100-ns unit or passing cadence.
4. Test both layers separately: pure conversion/overflow/order/duplicate/gap and
   missing-frame rejection; synthetic queue/deadline/cleanup adversaries; then
   a bounded no-game fixture with independent producer phase/paint witnesses
   and known pixel identities. Record actual producer timestamps rather than
   infer timing from the nominal `WM_TIMER` interval. Validate source timestamp
   ordering and relation to host transport without rewriting observed values.
5. Freeze frame selection before observation; never select by favorable pixels,
   retry failed eligible frames, resample/interpolate gaps, or reset the epoch to
   manufacture regularity. Retain bounded diagnostic timing metadata on failure
   without reading rejected pixels, if separately reviewed and implemented.
6. Only after independent source/fixture review consider a new stock-game gate.
   Compositor recurrence still needs scene/cycle identity and anti-aliasing
   checks; it grants no guest frame/`Present` equivalence, FPS, timer correctness,
   interval1 promotion, M7 credit or runtime PASS by itself.

This is a proposed evidence model, not authorization to relax existing gates.
The current host-bracket route can instead remain unchanged and report NO-GO
while transport costs are measured. Main owner chooses and reviews the route.

## Work record

Read current handoff, README-AI prerequisite/current-task excerpts, relevant
source, and Git status (already dirty). Consulted only official Microsoft
documentation for platform claims. Wrote only this note. No build or test suite
was run; no runtime validity is claimed. No user action is required for this
research handoff; a later native experiment may require the owner's visibility.
