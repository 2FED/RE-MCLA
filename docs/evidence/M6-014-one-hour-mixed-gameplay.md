# M6-014 one-hour mixed-gameplay long-session gate

Status: runner and fail-closed verifier ready; physical 60-minute result pending.

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
