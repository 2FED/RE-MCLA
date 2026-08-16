# M4-013 frontend deviation audit

Date: 2026-08-14

Status: accepted; M4 implementation complete and closure review ready

M4-013 consolidates every frontend limitation still relevant after M4-001
through M4-012. A row is not a promise that the feature is broken: it records
the boundary not established by M4, the severity of making a stronger claim,
the supported workaround, and the milestone that owns the missing proof.

## Deviation register

| Area | Severity | M4 disposition | Workaround / supported boundary | Target |
| --- | --- | --- | --- | --- |
| First-run OOBE, long opening cutscenes, and starter-vehicle selection | S1 | Unverified | Use the pinned post-OOBE private save for deterministic frontend and M5 work; use stock Xenia for a clean-new-game route. | M7-002, M7-006 |
| Race selection, race completion, and general playability | S1 | Unverified | Treat the saved free-roam route as a development prerequisite only, not a playable release. Define and validate one exact race next. | M5-001 through M5-014 |
| Bink decode, video/audio playback, and skip behavior | S2 | Unverified; not a boot blocker | Keep the supported unpatched guest-selected bypass. Do not add the dormant skip patch merely because the intro is absent. | M7-006 |
| ROV/interlock rendering, PWL gamma, and true-direct resolves | S2 | Not exercised | Keep the verified D3D12 host-RTV, table-gamma, staged common-copy route; diagnose only if the canonical gameplay slice reaches a dependency. | M5-004, M6-004 |
| Whole-frame and animated-background parity | S2 | Not claimed | Use the pinned stable-ROI comparisons at 1280x720 and 2560x1440. Compare broader gameplay categories separately. | M5-003 through M5-005, M8-004 |
| Saturated red/green local-light contribution on vehicle paint (initially described as a green shadow) | S2 | Open; fixed-camera gameplay repro | Preserve the bounded M4 UI pass, but do not claim vehicle reflection parity; diagnose HDR light accumulation, packed-float/blend, resolve, and gamma stages against console/Xenia references. | M6-004 |
| Individual frontend audio-event identity and human fidelity | S2 | Not observed | Use the verified sustained XMA/XAudio/SDL lifecycle route; do not identify a UI/music event from aggregate counters. | M5-009, M6-007 |
| XMP decoder and user-selected system music | S2 | Explicitly unsupported/contained | Retain the state-correct `Idle` fallback and fail unsupported playback/capture requests. | M6-007 |
| Title-driven force feedback | S2 | Not exercised | The host left/right/both diagnostic proves the selected device can rumble, but must not be called title-driven feedback. | M5-007 |
| Multiple physical controllers | S2 | Not exercised | Support and claim one selected SDL controller at slot 0; keep multi-pad policy unit-tested only. | M6-010 |
| Host dead-zone filtering | S3 | Not implemented by design | Preserve raw XInput-compatible values; M4 proves threshold classification, extremes, neutral return, focus neutralization, and reconnect. | M5-006 |
| `ReadProfileSettings`, privileges 251/252, and `SigninUI` runtime paths | S2 | Zero-hit on the accepted title route | Retain concrete guarded implementations and unit/static contracts; do not claim physical runtime coverage. | M6-006 |
| General profile/settings/save persistence | S1 | Unverified | Re-copy the pinned save into each isolated run; never infer persistence from frontend navigation. | M5-011, M6-005, M6-006 |
| `XGetLanguage` and country runtime paths; full localized content | S2 | Zero-hit / partial | Use the physically reached XConfig language and validated Unicode paths; do not claim complete subtitles, voice, or localized gameplay. | M7-010 |
| Known XLiveBase message runtime reachability and broader offline services | S2 | Zero-hit / partial | Keep the guest socket block and deterministic unit-tested offline results; do not claim Xbox Live or a host firewall. | M6-008, M7-014 |
| External close versus an in-game Exit command | S3 | Expected console-title behavior | Close the exact game window with `WM_CLOSE`; MCLA has no top-level internal Exit action, so no such action is claimed or required. | M6-010 soak/teardown only |
| General upstream ReXGlue behavior outside the supported image/route | S2 | Out of scope | Keep the exact published fork pin and the scoped project regressions; upstream issues remain tracked separately in `docs/known-issues.md`. | SDK/upstream |

## Closure interpretation

The M4 gate is satisfied by a repeatable process-start to pinned saved-gameplay
route, not by a clean-new-game or race-completion claim. The save bypasses the
unverified OOBE and enters active free roam after title `START`; pause and the
top-level Modes and Settings/Options panels are then navigated autonomously.
MCLA has no internal Exit menu, so safe exit means exact-window external
`WM_CLOSE`, `Execution complete`, hard exit, exit code 0, and no surviving
canonical process.

M5 must begin by defining the exact first-race vertical slice. The M4 decision
is therefore `GO M5 WITH PINNED SAVE`, with first-run/OOBE, race correctness,
and persistence explicitly still open.

## Accepted closure gate

The canonical private run is
`private/evidence/M4-013/20260814-023505-e69d76a2`. Its sanitized
`result.json` has SHA-256
`2CF4BE3210C260181F7C2CF9C00D07667ED12D6CA644B4E06ECBF12D7A9B0A28`.
It clean-built ReXGlue v0.9.0.18 and the RelWithDebInfo host, passed the focused
2-case/33-assertion VFS suite, and completed 20 consecutive isolated routes
from the exact pinned post-OOBE save. Every cycle reached saved free roam,
pause, and Settings/Options; produced four distinct 1280x720 captures; used
external exact-window `WM_CLOSE`; exited 0 without force cleanup or an orphan;
and preserved the source-game, seed, and four runtime-artifact identities.

The registered pause ROI ranged from 926480 to 932330 ppm and the Options ROI
from 984124 to 999391 ppm. Nineteen lifecycle tails logged `Execution complete`
before the hard-exit marker. One logged it immediately after the marker because
the guest/module and UI shutdown threads raced by three milliseconds. Another
completed normally with only bounded in-flight GPU trace records emitted while
the hard-exit flush was finishing. Both production tail forms are now covered
by positive fixtures; unknown post-hard-exit content, fatal/device-loss output,
missing lifecycle records, and malformed ordering still fail closed.

The aggregate was finalized after repairing those verifier-only false
negatives, rather than rerunning the already complete 20-cycle series. The
recovery path rebuilt every cycle manifest from the immutable physical logs,
captures, and trees, then ran the full result verifier a second time. Because
the original in-memory stopwatches were unavailable after the initial verifier
exception, the recovered result deliberately omits elapsed-stopwatch metrics
and records that limitation explicitly; elapsed time is not an M4 acceptance
criterion.
