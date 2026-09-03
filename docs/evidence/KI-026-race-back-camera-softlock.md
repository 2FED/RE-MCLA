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

Private Ghidra analysis establishes the focused callback path:

- `0x82666C50` creates `raceOverCommand("raceBack")`, emits
  `raceOverTrigger`, and performs race-over UI bookkeeping;
- script registration maps `Racer_ApplyGameCamera` to `0x822AD640`;
- that handler resolves the racer camera at offset `0x370` and, when its stock
  predicate permits, calls `0x822B0F10` with camera mode 4.

The default-off `mcla_race_back_probe` traces the command and handler functions,
plus the exact direct-call edge into `0x822B0F10`, without altering normal title
behavior. The focused
`scripts/run-race-back-camera-diagnostic.ps1` route uses the newest complete
hash-verified M6-014 recovery profile, automatically enters saved gameplay,
records the command/handler/apply edges for one owner-confirmed `Race Back`
outcome, preserves any new autosave, and closes the title externally. Its source
contract is covered by `scripts/test-race-back-camera-diagnostic.ps1`.

Closure requires the focused trace to identify the missing edge, a narrow fix
with no unconditional camera reset, and a physical retry in which the gameplay
camera and pause both return after `Race Back`. The one-hour M6-014 gate remains
blocked until that focused regression passes.
