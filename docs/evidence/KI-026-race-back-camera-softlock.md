# KI-026 Race Back camera/modal softlock

KI-026 is an open S2 race-transition defect discovered during the second
physical M6-014 one-hour attempt, `20260903-210859-8ac102d3`.

The owner lost a Red Light Driver event, selected `Race Back`, and observed the
normal map/cinematic zoom out. The view then remained at the aerial city camera.
The vehicle could still be driven, but pause did not open and the documented
RB/R1 camera command did not change the view. The exact native process stayed
alive and responsive. Its runtime log has no guest crash, assertion, invalid or
unregistered function, D3D12 device loss, NVIDIA event, or Sunshine failure.
This distinguishes a live guest transition/modal softlock from a host or title
process crash.

The operator stopped the console harness. The exact title window was closed
externally without force termination, and the save watcher preserved the latest
complete profile with save SHA-256
`5DEE5B6B2B107B17462F1D94EF404ABD676B94A7894CF740AB940C63450CE7CB`.
The failed attempt contributes no accepted M6-014 stability time.

Private Ghidra analysis established the initial focused callback hypothesis:

- `0x82666C50` creates `raceOverCommand("raceBack")`, emits
  `raceOverTrigger`, and performs race-over UI bookkeeping;
- script registration maps `Racer_ApplyGameCamera` to `0x822AD640`;
- that handler resolves the racer camera at offset `0x370` and, when its stock
  predicate permits, calls `0x822B0F10` with camera mode 4.

The first physical diagnostic, `20260903-215313-7602fb14`, completed a healthy
`Race Back` return with gameplay camera and pause restored. It observed one
command entry and return, but zero `Racer_ApplyGameCamera` handler or direct
apply-edge calls. That result rejects the initial handler as the normal success
path; no camera reset has been guessed from it.

The default-off `mcla_race_back_probe` v2 therefore traces the command and
handler functions plus all six statically identified direct call sites into
`0x822B0F10`: `0x822A5990`, `0x822AD698`, `0x822B1258`, `0x822B1464`,
`0x822B3460`, and `0x822B359C`. Every stock call still executes. The focused
`scripts/run-race-back-camera-diagnostic.ps1` route uses the newest complete
hash-verified M6-014 recovery profile, automatically enters saved gameplay,
records the command/handler/apply edges and per-site counts for one
owner-confirmed `Race Back` outcome, preserves any new autosave, and closes the
title externally. `RETURNED` is valid only when both gameplay camera and pause
work. Its source contract is covered by
`scripts/test-race-back-camera-diagnostic.ps1`.

Current-artifact run `20260904-062842-fb1043bf` then exercised three consecutive
lost-event `Race Back` selections in one process. All three restored the
gameplay camera and pause. The trace recorded three command selections, three
successful command returns, no legacy handler calls, and 27 total direct apply
edges: six from `0x822B3460` and 21 from `0x822B359C`. Each return began with a
repeatable sequence: `0x822B359C` about 1.7–2.0 seconds after the command,
`0x822B3460` about 0.63–0.66 seconds later, then two rapid `0x822B359C` calls.
The exact process exited through external `WM_CLOSE` without forced cleanup,
and the watcher preserved evolved save SHA-256
`012407994331E607BFA404F50937B3046747B08D7CE76DAF50DCE51CB74F9C8D`.

This zero-of-three non-reproduction does not prove that KI-026 was fixed: no
camera behavior was changed. It does establish the current healthy v2 path and
makes repeated forced reproduction disproportionate for an intermittent S2
issue. KI-026 remains open. The later M6-014 one-hour gate completed without a
recurrence and M6 closed without claiming a camera fix. If the softlock recurs
during normal play, press F10 and wait for completion before restart; that live
package plus the trace will drive a narrow M7 fix.
