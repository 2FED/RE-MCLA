# M9-005 volunteer build (in progress, started 2026-10-28)

Carries M8-018. Distribution option A (owner decision 2026-10-28): a closed
group of trusted testers, shared by the owner through a private link. Plan and
rules: [M9 priorities](../M9-priorities.md).

| Part | State |
|---|---|
| Kill switch (V2 tickets) | Implemented and unit-tested; not published |
| Post-session report prompt | Implemented and unit-tested; not yet in a tester build |
| Fresh F10 image or explicit stale mark (KI-042) | Implemented, builds; in-game check pending |
| Diagnostics export with frame-pacing/CP/GPU summaries | Implemented and unit-tested |
| Allowlisted staging and sentinel audit | Not started |
| Tester guide, one tester smoke | Not started |

## Kill switch (2026-10-28)

Policy text: [test-build expiry](../test-build-expiry.md#kill-switch-builds-v2-tickets-m9-005).

- `mcla_build_lease_policy`: `MCLA-TEST-BUILD-V2` tickets (`kill_switch`)
  and a pure `EvaluateKillSwitch` that returns admit/refuse plus a cache action
  (keep / refresh / revoke). V1 `Evaluate` refuses V2 tickets, so a V2 build
  never gets the 14-day allowance.
- `mcla_build_lease_runtime_win`: V2 tickets go to `CheckKillSwitch`. It reads
  `<host>.killswitch` (check time plus the signed list, or `revoked`), fetches
  the list unless the managed host can follow a check younger than 24 hours,
  applies the decision and writes the record atomically under the existing
  cache writer lock. V1 behaviour is unchanged.
- Owner rules as implemented: offline start for 3 days from creation or
  24 hours after a listing check; a verified list without the host refuses and
  records a withdrawal (also refused offline); network errors, timeouts and
  untrusted lists neither revoke nor extend.
- **One stricter choice than the literal rule.** A withdrawn build stays refused
  offline even inside its first 3 days. Without this, a tester could defeat a
  withdrawal by going offline. It is reversed as soon as a list names the
  host again.
- Tooling: `New-MclaTestBuildTicket.ps1 -KillSwitch` writes V2.
  `Mcla-TestBuildSigning.ps1` accepts V2 payloads. The staging check accepts
  V1 or V2 tickets.

Tests:

| Suite | Before | Now | Failures |
|---|---|---|---|
| Build lease policy | 1417 | 1438 | 0 |
| Build lease runtime (signed fixtures, real cache dir, injected network) | 47 | 69 | 0 |
| Launcher controller | 338 | 338 | 0 |
| Staging script | 63 | 63 | 0 |

ast-grep is clean.

Not yet run: `Test-MclaTestBuildSigning.ps1` (PowerShell 7). This agent session
may not execute the downloaded PowerShell 7 binary; the owner can run it, or
allow it.

Not done: nothing was published to `DebugBundle`. Every volunteer host hash
must be listed there before testers go online, and each publication needs the
owner's explicit yes. The volunteer `mcla.exe` must also be built with
`MCLA_TEST_BUILD_EXPIRY=ON`, so the host checks as well as the launcher.

## Post-session report prompt (2026-10-28)

- When the launcher sees the game exit, it lists this session's completed
  packages in `<profile>/diagnostics/<session>/live` and `/crash`. Unfinished
  `.partial` packages and unexpected names are skipped; crashes come first.
  Every session has its own diagnostics folder, so only this session counts.
- If there are any, it writes `report.txt` into that folder at once. The file
  holds the build fingerprint, mode, resolution, start and end UTC, exit code
  and the package list, then four questions for the tester: what they were
  doing, steps, what went wrong, what they expected. No paths, profile names
  or hardware identifiers. It is written before any dialog, so it exists even
  if the launcher is closed.
- The launcher then asks once: `N reports were recorded in this session (k
  crash). Open the folder now? Fill in report.txt there, then send the whole
  folder to the owner by hand. Nothing is uploaded automatically.`
  Yes opens the folder in Explorer.
- This is simpler than the in-launcher form in the plan: the tester fills in
  `report.txt` in a text editor instead. That is the owner decision as
  relayed (open the dumps folder to prompt a bug report); a form can be added
  if testers struggle.
- Code: `src/mcla_session_report.h` (pure), controller `Poll` completion and
  `OpenReportFolder`, and the prompt on the running-to-stopped transition in
  the UI. Tests: `mcla_session_report_test` 17 checks. The controller fixture
  writes a real package and checks `report.txt` content, the clearing and no
  rewrite (343 checks). The UI checks prompt once, yes/no, and silence without
  reports (971 checks). All 21 launcher, export and lease suites pass; ast-grep
  is clean.

## Fresh F10 frame, KI-042 (2026-10-28)

- Cause: the F10 package used a `BitBlt` of the game window. With a D3D12
  flip-model swapchain, GDI can keep returning an old image (three presses,
  one identical `frame.bmp`).
- Fix: F10 now reads back the game's own last presented output through the
  SDK presenter (`CaptureGuestOutput`, the path the first-frame and race
  probes already use), so the image is the game's 1280x720 frame without window
  artifacts. The manifest records `source guest-output-readback`,
  `freshness presented-guest-output`.
- Hang safety: that readback waits on a GPU fence without a bound, and F10
  must still work during a freeze (KI-035). It runs on a detached helper
  thread; the package worker waits at most 1.5 s and then falls back to the
  window capture. The host hard-exits at shutdown, so a stuck helper cannot
  block the exit.
- Fallback guard: a window capture byte-identical to the previous one in the
  same process is dropped and marked `freshness stale-duplicate-dropped`,
  so no package carries a misleading picture.
- Build: `mcla.exe` rebuilt (private `l38-host`); no compiler warnings;
  ast-grep clean. Pending: one in-game check with two F10 presses in different
  scenes (needs an announced run or the owner).

## Export performance summary (2026-10-28)

- The export (Export report... in the launcher) now also reads the package's
  private `log-tail.txt` when present. That file is pinned, capped at 1 MiB and
  bound into the confirmation token like the other inputs.
- Only four counter lines are parsed, into numbers: `frame pacing/min`,
  `draws per frame/min`, `gpu busy/min` and `replay/min`. The report gains
  `performance_per_minute` (last 10 minutes): `fps`, `frame_p50_ms`,
  `frame_p95_ms`, `frames_over_50ms`, `gpu_busy_ms_per_frame`,
  `draws_p50`, `cp_replay_ms_per_minute`, plus the disclaimer
  `performance-counters-from-log-not-certified`. No other log text, path or name
  can reach the report. A value that does not parse is omitted, not guessed.
- Tests use real line shapes copied from owner logs, a private-path sentinel,
  malformed values, a 25-minute log (bounded to 10) and an oversized tail. Export
  policy 538 checks, 0 failures; all 21 suites pass.
