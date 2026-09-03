# M6-014 one-hour mixed-gameplay long-session gate

Status: runner and fail-closed verifier ready; physical 60-minute result blocked
on focused KI-026 repair and regression.

The current M6-014 closure path deliberately limits new owner interaction to one
continuous hour. It combines the immutable two-hour frontend stage from suite
`20260831-133236-2cecb67b` with one 3,600-second mixed-gameplay process bound to
current suite `20260901-153415-d747cf2d`, ReXGlue v0.10.0.1 commit
`7dd5cb33002a443b097c0f65d5566c0a0f2db838`, and the complete profile preserved
by accepted delivery regression `20260901-203448-d31ab681`.

The owner may play normally and progress the story, races, or deliveries. The
hour also includes one garage enter/exit, one pause/resume, and one Alt-Tab
away/return. The runner automatically records thirteen ordered process/I/O
samples and attempts one baseline plus four fifteen-minute nontrivial desktop
captures. The baseline is mandatory; quarter-hour attempts are best-effort and
are recorded as either `captured` or `skipped`, so an unfocused, paused, hidden,
or temporarily uncapturable window cannot terminate otherwise healthy gameplay.
Runtime logs, host NVIDIA/Sunshine event health, controlled external close, and
the final complete save archive remain mandatory. Only start and end
attestations require console input.

Before launch, the copied working profile is rehashed against the accepted
post-delivery source and preserved as an immutable seed copy. After shutdown,
the verifier rehashes the referenced final snapshot and its save/header rather
than trusting `latest.json` metadata alone.

Until a physical result passes, M6-014 remains open. Even after acceptance this
evidence will not claim that historical frontend and current gameplay used the
same executable, that one process ran for three hours, that the legacy five-stage
suite completed, that current gameplay ran for two hours, or that campaign,
rendering, music, or wheel-centering behavior is fully correct.

The first physical attempt (`20260903-135924-45456efb`) reached the fifteen-minute
checkpoint with a live process. Its desktop capture could not reacquire the MCLA
foreground window within twenty seconds, after which the old harness deliberately
sent `WM_CLOSE`. Runtime logs contain clean shutdown markers and no guest crash,
assertion, device-loss, Windows Error Reporting, NVIDIA, or Sunshine failure.
The attempt is therefore a harness-capture failure, not a game crash. Resource
samples through ten minutes and the save watcher's complete recovery snapshots
remain private diagnostic evidence; no stability time from that run is credited.

The second physical attempt (`20260903-210859-8ac102d3`) exposed KI-026 before
the first ten-minute resource boundary. After the owner lost a Red Light Driver
event and selected `Race Back`, the transition zoomed to the aerial city view
but never restored gameplay camera or pause control. Vehicle input continued to
move the car, and RB/R1 did not change the view. The exact process remained
responsive and its runtime log contains no guest crash, invalid target,
assertion, device loss, NVIDIA event, or Sunshine failure. The operator stopped
the console harness; the title was then closed through its exact window, and the
watcher preserved a complete recovery snapshot with save SHA-256
`5DEE5B6B2B107B17462F1D94EF404ABD676B94A7894CF740AB940C63450CE7CB`.
No duration from this attempt is credited.

Static XEX analysis binds the failing route to guest function `0x82666C50`,
which produces `raceOverCommand("raceBack")` and emits `raceOverTrigger`.
`Racer_ApplyGameCamera` is registered at `0x822AD640`; it conditionally calls
the camera application function at `0x822B0F10` with mode 4. Physical diagnostic
`20260903-215313-7602fb14` then completed a healthy return with one command
entry/return but zero handler or apply-edge calls, disproving that narrow route
as the normal success path. The default-off v2 probe now observes all six exact
direct call sites into `0x822B0F10` without changing stock behavior.
`scripts/run-race-back-camera-diagnostic.ps1` clean-builds the traced
title, selects the newest complete hash-verified M6-014 recovery profile,
automatically loads gameplay, records whether the command, handler, and each
camera-apply edge execute, preserves the evolving save, and ends after one
focused Race Back outcome. Another one-hour run is blocked until this short
diagnostic and a subsequent focused fix regression pass.
