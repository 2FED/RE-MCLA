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
| Allowlisted staging and sentinel audit | Path-clean volunteer build done; signing, staging and audit need the owner's PowerShell 7 run |
| Tester guide, one tester smoke | Guide written (EN/UK); smoke pending |

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

## Volunteer audit and guide (2026-10-28)

- `scripts/Test-MclaVolunteerStaging.ps1` audits a staged folder and changes nothing:
  - an exact file allowlist, with no reparse points or alternate streams;
  - no game, save, PDB, TOML, log, dump or image files;
  - ASCII and UTF-16 searches of every file for owner-identifying strings: the`n    project path, `MCLA-Saves`, the Windows profile path in path form, the
    computer name and caller sentinels;
  - with `-RequireVolunteer`, a V2 ticket and an expiry-enabled host.
- First run on `private/l37` (an internal build, not volunteer):
  - **absolute source paths** (`C:/BDU/MCLA-Recomp/private/l34-src/...` from
    `__FILE__` in log and assert macros) in `mcla.exe`, `rexruntime.dll` and
    `rexgpu-xenos.dll`;
  - **absolute PDB paths** in `rexgpu-xenos.dll` and `TracyClient.dll`;
  - an old image helper (from l12) carrying the main-checkout path;
  - expected for that build: a V1 ticket and no expiry host.
  The bare Windows user name matched only colour and controller names, so it is
  now matched in path form only. The computer name occurs nowhere.
- Fix for volunteer builds: compile with `-ffile-prefix-map=<source root>=.`,
  link with `/pdbaltpath:%_PDB%`, and rebuild the image helper. A path-clean
  SDK build is running (`private/l38-sdk-*`).
- `Stage-MclaInternalLauncher.ps1 -TesterGuide <file>` adds `TESTER-GUIDE.txt`
  to the package and its receipt. Staging tests are still 63/0/0.
- `docs/volunteer-tester-guide.txt`: a one-page guide in English and Ukrainian,
  covering first start, the online check, the frame-rate choice, F10 and the
  report prompt, known issues and the launcher refusal messages. A claim that a
  wheel button can trigger F10 was removed: no such binding exists in this
  source.

## Path-clean volunteer build l38 (2026-10-28)

- **SDK** `private/l38-sdk-*`: same source as l34 (`a239b83`) and same flags,
  plus `-ffile-prefix-map=<clone>=.` and `/pdbaltpath:%_PDB%`. The three DLLs
  contain no `C:\BDU` path; the PDB reference is a bare file name.
- **Host** `private/l38-build`: compiled with `MCLA_TEST_BUILD_EXPIRY=ON`, the
  image helper is rebuilt (`MCLA_BUILD_IMAGE_CAPTURE_TESTS=ON`, Windows SDK
  10.0.26100 cppwinrt), and there are prefix maps for the clone and the SDK
  install. The first attempt still embedded the install path of the SDK's
  `rex_app.cpp`, so a backslash map was added. No compiler warnings. None of the
  seven binaries contains `C:\BDU` or `Users\<name>`. The admission message is
  present as a UTF-16 literal, which confirms expiry ON (the l34 expiry-OFF host
  lacks it); the audit now searches that marker in UTF-16.
- Identities: `mcla.exe` `9777D5BE...6E090CB`, launcher `009BC3B1...`. Input
  folder: `private/l38-vol-input`.
- `private/l38-entries.json` keeps the published l2 grant
  (`9195117A...`, valid until 2026-11-07) and adds the volunteer host until
  2026-11-27 18:57 Kyiv (30 days; the owner may change it).
- The current public `active-builds.txt` verifies with the real key through the
  lease probe: `SIGNED_LEASES_VERIFIED entries=1` (local, not deployment).
- Owner step (PowerShell 7): `private/l38-volunteer-prepare.ps1`. It signs the V2
  ticket and the new list, stages `private/l38-volunteer` with the guide,
  verifies the list and runs the volunteer audit. Publishing the list to
  `DebugBundle` is a separate step that needs the owner's explicit yes.
