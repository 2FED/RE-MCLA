# M6-014 one-hour mixed-gameplay long-session gate

Status: PASS. Physical run `20260904-095603-fb908583` completed the full
60-minute gate; version `0.8.2.1` corrects verifier-only defects without
changing or rebuilding the tested `0.8.2.0` Release artifact.

The current M6-014 closure path deliberately limits new owner interaction to one
continuous hour. It combines the immutable two-hour frontend stage from suite
`20260831-133236-2cecb67b` with one 3,600-second mixed-gameplay process bound to
ReXGlue v0.10.0.1 commit
`7dd5cb33002a443b097c0f65d5566c0a0f2db838`. The accepted delivery regression
`20260901-203448-d31ab681` remains an immutable prerequisite, but is no longer
misused as the gameplay seed or current executable identity.

Version `0.8.2.0` made the runner clean-build the current Release source before
the hour, binds all five launch artifacts including `mcla_crash_handler.exe`,
and selects the newest complete hash-verified snapshot across the entire
M6-014 save archive. The accepted run selected recovery
`20260904-053615Z-5DCED57FBEEB5241-2FAEFBC7FF8CDEAD`, save SHA-256
`5DCED57FBEEB5241BED4EC2CD5E89CA019B7B7758EAA00C3ED2456A4CC6291BF`.
This prevents both the stale-artifact rejection and the stale post-delivery
profile regression that the previous runner would have caused.

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

Before launch, the copied working profile is rehashed against the selected
recovery source and preserved as an immutable seed copy. After shutdown, the
verifier rehashes the referenced source and final snapshots, their manifests,
and their save/header files rather than trusting `latest.json` metadata alone.

The accepted process ran for exactly 3,600 measured seconds with thirteen
ordered resource samples, four nontrivial captures, and five recorded capture
attempts. The first quarter-hour attempt was safely skipped because MCLA was
not foreground; the mandatory baseline and the remaining three attempts were
captured. The owner confirmed normal moving gameplay plus garage enter/exit,
pause/resume, and Alt-Tab return. The title then closed externally with exit
code zero, no forced cleanup, no runtime fatal marker, and zero new NVIDIA or
Sunshine host events. Final growth was 57,360,384 private bytes and 56,766,464
working-set bytes, with -1 handles and -9 threads; peak growth remained below
the gate limits. The evolved save SHA-256 is
`58B3385C89D9E4999B6791CCB95B6F7ED7963CE07F9C47618850841236784585`,
and the complete final archive tree is
`6539EB94AA60E0FBF7403D760B8B3FB35A1AB7876584118BE55EB9CDE1105888`.

The initial post-run verifier failure was evidence-tooling drift, not a gameplay
failure. It compared separately deserialized nested journals at PowerShell's
default JSON depth, required an obsolete `Execution complete` line absent from
the otherwise controlled current shutdown path, and assigned the read-only
automatic `$Host` variable. Version `0.8.2.1` fixes those three verifier defects,
adds source-contract coverage, and revalidates the immutable physical result.
No gameplay rerun is required, and the verifier deliberately checks the
recorded `0.8.2.0` build identity and hashes rather than demanding that a future
repository `VERSION` remain frozen.

This evidence does not claim that historical frontend and current gameplay used the
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
focused Race Back outcome. Current run `20260904-062842-fb1043bf` subsequently
completed three consecutive Race Back returns with camera and pause restored,
so another one-hour run is now allowed with KI-026 retained as an intermittent
open issue.

Version `0.8.0.0` adds the local diagnostic prerequisite for that next
reproduction. Pressing F10 while the camera is stuck now preserves bounded
process/window/runtime state, the already-presented client frame, a runtime-log
tail, normal minidump, and private save snapshot without waiting for a debugger.
Unhandled native failures are captured automatically by a separate helper. See
`docs/evidence/M6-014-live-crash-diagnostics.md`; this improves evidence capture
but does not itself close KI-026. If the defect recurs during the final hour,
F10 capture before restart is mandatory and the run is diagnostic rather than
accepted stability evidence.
