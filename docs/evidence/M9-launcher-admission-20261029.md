# M9 launcher: owner admission (KI-045), kept Build folder (KI-044), per-package report.txt (2026-10-29)

**Status:** implemented, unit tested and compiled in `private/l39-merge-build`
against `private/l39-sdk-install`. Not physically verified: no launcher, game,
helper or probe binary was started. This is not a physical PASS. The l41 owner
candidate is NOT prepared here (primary's scope change): it will be built once
from the merged tree against `private/l41-sdk-install`.

Owner reports, 2026-10-29 (rows KI-044 and KI-045 in `docs/known-issues.md`),
plus the owner decision of the same day that every report folder carries its
own prefilled `report.txt` written by the game.

## KI-045: owner admission for internal candidates

### What gated the options

`ResolveCapabilities` (`src/mcla_launcher_capabilities.h`) admitted 1080p and
2160p only for two fingerprints in source (`kRenderTargetTestedBuild`,
`kCleanRenderTargetTestedBuild`) and Unlocked FPS only for one
(`kUnlockedTestedBuilds`, the l33/l34-sunc build). There is no data file or
`config/` matrix behind it. The fingerprint is the SHA-256 of
`mcla-build.manifest`, and the manifest pins `mcla-test-build.ticket`, so every
re-ticketed candidate has a new fingerprint. A source table therefore cannot
list a candidate before its ticket is signed, and every owner build lost both
options.

### Mechanism chosen

An optional file in the launcher's local settings directory, next to
`preferences.dat`:

```
%LocalAppData%\MCLA-R-Launcher-v1\owner-admitted-builds.txt
```

Format (ASCII, LF only, mandatory final LF, at most 16 KiB and 64 rows):

```
MCLA-OWNER-ADMITTED-BUILDS-V1
<64 uppercase hex build fingerprint>[ <label>]
```

The label is optional, 1-32 characters of `A-Z a-z 0-9 . _ -`. Any malformed
byte (CRLF, lowercase hex, duplicates, unknown header, oversize) makes the
whole file admit nothing; an absent or unreadable file admits nothing.

Behaviour:

- A listed build gains exactly the gated options: 1080p/2160p render targets
  and Unlocked FPS (240 Hz guest vblank, unchanged). VSync Off and 60 FPS stay
  refused. 30 FPS stays the default and keeps the exact stock command line.
- The capability record (`capabilities.txt`) gains one final key,
  `admission owner-allowlist`, only when the admission actually admitted a
  gated option. Stock 720p/1440p sessions on a listed build, and every session
  on a tested build, write byte-identical records to before. A record cannot
  claim an admission it did not need, and an admitted plan cannot drop it
  (`SerializeCapabilities` re-resolves and refuses both).
- CHECK SETUP adds "Owner-admitted build (owner list), not a tested build." to
  its status when the admission was used.
- The metadata export (`src/mcla_diagnostic_export_policy.h`) re-resolves the
  record with the admission flag taken from that key, so owner sessions at
  2160p/Unlocked stay exportable; the canonical re-serialization must still
  match byte for byte.
- Volunteer-facing refusal texts are unchanged.

Why this mechanism:

- It follows an existing pattern: `collection-parent.dat` is an optional,
  strictly parsed hint in the same directory whose invalid bytes are ignored,
  with controller tests. The privacy story is the same: the file holds only
  build fingerprints and optional labels, no paths or names, and it is never
  read by the export.
- It lives per user on the owner's PC and is never part of a build folder,
  candidate folder or volunteer package, so no package can carry admission to
  a volunteer. Admission is by exact fingerprint, which pins every artifact
  and the ticket.
- One file covers several candidates (l39, l40, l41) because the settings
  directory is shared by every launcher copy.

Rejected alternatives: a source table (cannot know a fingerprint before the
ticket is signed; needs a rebuild per candidate); a file in the Build folder's
parent or a declaration in `candidate-receipt.json` (travels with a copied
folder, and a launcher pointed at `private/l40-internal/build` would need a
write into `l40-internal`); a gate on the ticket kind (V1 owner tickets only):
`private/l39-internal` carries a **V2** (kill-switch) ticket, so such a gate
would refuse l39 although the owner asked for it; the exact-fingerprint list
already excludes every volunteer build the owner does not list.

### Fingerprints measured (SHA-256 of `build/mcla-build.manifest`)

| Candidate | Ticket | Fingerprint |
|---|---|---|
| `private/l39-internal` | V2 | `D0F1B45F4EECE536559BEC212ADE7CE26C9360E191D5700EEA753FFEFAC10265` |
| `private/l40-internal` | V1 | `C0BB61C865F325C9DF5B396C0AC054508A76DAE8D7B5FD60CF21808D11582094` |

l40's value equals `build_manifest_sha256` in `private/l40-internal/candidate-receipt.json`.
The l41 fingerprint exists only after its ticket is signed and it is staged.

File content admitting l39 and l40 (each row ends with LF, not CRLF):

```
MCLA-OWNER-ADMITTED-BUILDS-V1
D0F1B45F4EECE536559BEC212ADE7CE26C9360E191D5700EEA753FFEFAC10265 l39-internal
C0BB61C865F325C9DF5B396C0AC054508A76DAE8D7B5FD60CF21808D11582094 l40-internal
```

Recommendation for the l41 prepare script (primary's scope): after staging,
read `private\l41-internal\build\mcla-build.manifest`, hash it, and write the
file atomically with the l39, l40 and l41 rows. Keep any valid existing rows,
write LF-only ASCII, and refuse to create the file when `preferences.dat` is
absent (see limits).

## KI-044: the new Build folder was not kept

### Trace

- `Browse...` (`PickFolder`) only sets the Build edit control. Nothing is
  saved.
- CHECK SETUP (`StartCheck`, worker thread) sends the form as a `CheckLaunch`
  request. `LauncherController::State::CheckLaunch` is deliberately read-only:
  it never calls `Settings` and never writes `preferences.dat`. Existing tests
  assert this.
- The form keeps its draft after initialization (`Render` uses the draft), and
  a stale result (generation or setup changed while checking) is discarded by
  `PollCheck`. Within one window the field therefore cannot revert.
- Only Save settings and START GAME (`Play` -> `Settings` -> `store.Save`)
  persisted the folder.

Root cause: after Browse + CHECK SETUP, a Ready result for the new folder was
shown but the folder was never saved. When the launcher was closed and opened
again (or another launcher copy was opened), it re-read `preferences.dat` and
showed the previous folder (l39). The second attempt saved l40 because it
went through Save settings or START GAME. No ordering race between an old
check result and the new selection was found in source: stale results are
already dropped by generation.

### Fix

`PersistCheckedSetup` (`src/mcla_launcher_ui_win.cpp`, GUI thread, after the
worker has finished): when a check result is **current** (same generation and
setup as the form) and **Ready**, the UI sends `SaveSettings` with exactly the
checked request, unless it already equals the saved setup. The status gets
"Setup saved." or "Setup was NOT saved: <reason>". A stale result is discarded
before this step, so it saves nothing. The controller's Check stays read-only.

Tests:

- UI (`TestCheckPersistsChosenFolder`): select A + check saves A; select B +
  check saves B; B2 chosen while A's check is pending, then A's result
  arrives: nothing saved and the field keeps B2; checking B2 saves B2; a Ready
  check of the already saved setup does not save again; no callback overlap
  and no wrong thread.
- Controller (`FolderChoiceMatrix`): Check of B is read-only and reports the
  saved A; SaveSettings(B) persists B; a late Check of A leaves B saved; a
  reopened store and a restarted controller show B.

## Per-package report.txt (owner decision 2026-10-29)

### Game host (written when each package is written)

`mcla::diagnostics::BuildPackageReportText` (pure, `src/mcla_diagnostic_trigger.h`)
is written as `report.txt` into each package folder:

- live packages: `src/mcla_diagnostics.cpp`, in the `.partial` folder before
  `manifest.json` and the publish rename;
- crash packages: `src/mcla_crash_handler_win.cpp` (the crash handler helper),
  after `README.txt`.

Content (LF, printable ASCII, fields bounded to 128 bytes, never guessed):
header and instructions, `package`, `kind`, `created_utc`, `mcla_version`,
`category`, `source` (`f10`, `wheel-r2`, `wheel-l2`, `pad`, `crash`, or the
reason), `wheel`, `window` (physical size and fullscreen/windowed, or
`unknown`), the line
`launcher_session pending: the launcher adds build, mode and resolution after the game closes`,
then the four questions with empty answers.

`wheel` line: for wheel packages (`wheel-l2`, which carry the M9-010 wheel
state) the trigger device, else the first SDL device flagged as a wheel:
`<name> | VID 0xNNNN PID 0xNNNN | GUID <guid>`, or `unknown` when no wheel is
present. Other live packages say `not-recorded (wheel L2 records the wheel)`;
crash packages `not-recorded (crash)`. Capturing the wheel identity for every
package was not done: it would add the wheel capture to non-wheel triggers.

The host does not know the build fingerprint, mode or render target, so those
come from the launcher.

### Launcher (after the session)

- The session-level `report.txt` index is still written, with categories; its
  text now points to the per-package files.
- `CompletePackageReports` (`src/mcla_launcher_controller_win.cpp`): for each
  completed package, if `report.txt` contains the pending line exactly once,
  that one line is replaced by `build`, `mode`, `render_target`,
  `session_started_utc`, `session_ended_utc`, `exit_code`. A file without the
  line (tester text) is never rewritten. A package without `report.txt` (older
  host or failed write) gets a launcher fallback with `CREATE_NEW`, so an
  existing file is never replaced. Unfinished `.partial` packages get nothing.
- The prompt says each report folder (`live\...` or `crash\...`) has its own
  `report.txt`.

### Export and other consumers

`report.txt` is not a manifest artifact (no hash, as the tester edits it). The
metadata export reads only `launch-receipt.txt`, `state.txt`, `manifest.json`,
`state.json`, `capabilities.txt` and `log-tail.txt`; it never reads
`report.txt`, and a manifest that lists `report.txt` as an artifact is
rejected by the existing artifact allowlist (new test). So the tester text
stays out of the automatic export. `scripts/verify-diagnostics-smoke.ps1`
compared the exact top-level file set of a package and would have failed on
the new file; it now ignores `report.txt` only.

Tester guide (`docs/volunteer-tester-guide.txt`, EN and UK): "Each report
folder contains report.txt; fill it in", the wheel note, and send that whole
report folder.

## Files changed

- `src/mcla_launcher_capabilities.h`: owner admission file constants and
  parser, `owner_admitted` in request and plan, admission key in the record.
- `src/mcla_launcher_controller_win.cpp`: reads the owner file for Check and
  Start, status note, `CompletePackageReports`.
- `src/mcla_launcher_ui_win.cpp`: `PersistCheckedSetup` (KI-044).
- `src/mcla_session_report.h`: pending line, `SessionFacts`,
  `FillPackageReport`, `PackageReportText`, prompt and index text.
- `src/mcla_diagnostic_export_policy.h`: admission flag from the record.
- `src/mcla_diagnostic_trigger.h`: `BuildPackageReportText`,
  `WheelReportLine`, `ReportField`.
- `src/mcla_diagnostics.cpp`, `src/mcla_crash_handler_win.cpp`: write
  `report.txt`.
- `scripts/verify-diagnostics-smoke.ps1`: ignore `report.txt` in the top-level
  file check.
- Tests: `tests/mcla_launcher_capabilities_test.cpp`,
  `tests/mcla_launcher_controller_win_test.cpp`,
  `tests/mcla_launcher_ui_win_test.cpp`, `tests/mcla_session_report_test.cpp`,
  `tests/mcla_diagnostic_trigger_test.cpp`,
  `tests/mcla_diagnostic_export_policy_test.cpp`.
- `docs/volunteer-tester-guide.txt`.

## Tests

Build script `m9launcher_build.ps1` (jobs tmp; same toolchain as the l39
merge build: VS 2022 Build Tools, LLVM clang++ 20.1.8, Ninja Release). Targets
`mcla`, `mcla_crash_handler`, `mcla_internal_launcher` and 32 test
executables; 0 warnings, 0 errors. All 32 test executables rc 0.

| Test | Before | After |
|---|---|---|
| `mcla_launcher_capabilities_test` | 200 | 238 |
| `mcla_launcher_controller_win_test` | 343 | 367 |
| `mcla_launcher_ui_win_test` | 971 | 983 |
| `mcla_session_report_test` | 30 | 39 |
| `mcla_diagnostic_trigger_test` | 87 | 102 |
| `mcla_diagnostic_export_policy_test` | 573 | 585 |

"Before" for the four pure tests was measured by compiling the HEAD
(`7ab1b11`) sources with the same clang++; controller and UI from the brief
and the M9-010 evidence.

Unchanged suites, 0 failures: preferences 2088, profile policy 118, import
policy 72, library codec 315, build_win 109, store_win 125 (1 skip), fs_win
76, process_win 41, snapshot_win 26, game_win 70, import_win 31,
diagnostic_export_win 105 (1 skip), diagnostic_export_ui_win 249,
managed_config_policy 50, portable_adapter_policy 78, portable_path_policy 52,
menu_state 447, guest_sampler 9, diagnostic_hotkey 8, cursor_autohide 266,
frame_signal 24, diagnostic_image_protocol 119, readback_cost_sample 133,
build_lease_policy 1438, build_lease_network_win 26 (no network request),
build_lease_runtime_win 69.

`ast-grep scan src tests scripts`: clean. `ast-grep test`: 3 passed.

## Binaries built (verification only, `private/l39-merge-build`, l39 SDK)

| File | Bytes | SHA-256 |
|---|---|---|
| `MCLA-Launcher.exe` | 1,335,296 | `B82C1D821C121F531A651539B40FA83A9A939437F0FA4646D1442A75C33C5C00` |
| `mcla.exe` | 53,059,072 | `F3DD4F5EA4F814CEF646C041A3D14DDBF3EE2FFFDE4AEB6BDF0E6B7F27560640` |
| `mcla_crash_handler.exe` | 211,968 | `4104CE3919F4B2888EBA0DD0ACF222565ED3E7894E3C85D2EFF05105CB50726B` |
| `mcla_diagnostic_image_helper.exe` | 60,928 | `BC6E649F...4CA539B` (unchanged from l40) |

The launcher link is not bit-reproducible (two links of identical objects gave
different hashes), so its hash identifies this binary only. These are not
the l41 binaries: l41 is to be built from the merged tree against
`private/l41-sdk-install`.

## Owner steps (after l41 is prepared)

1. Make sure the launcher has run once on this PC (`preferences.dat` exists in
   `%LocalAppData%\MCLA-R-Launcher-v1`).
2. Create or extend `owner-admitted-builds.txt` there with the l39, l40 and
   l41 rows (the l41 prepare script should do this).
3. Start the l41 launcher, choose the Build folder, press CHECK SETUP. After
   "Ready" the status says "Setup saved."; close and reopen the launcher to
   confirm the folder is kept (KI-044).
4. Choose 3840x2160 or Unlocked FPS: CHECK SETUP must be Ready with
   "Owner-admitted build". Without the file the old refusal returns.
5. Record a report with F10, wheel L2 and pad Start+Back. Each package folder
   must contain `report.txt` with package, time, category and source; the
   wheel L2 one must name the wheel. After the game closes the launcher adds
   build, mode and resolution to each file and does not touch a file you
   already answered.

## Limits and risks

- The admission is a convenience gate on the owner's own PC, not a security
  boundary: any user can create the file in their own settings directory. It
  is never shipped and admits only exact fingerprints.
- If `owner-admitted-builds.txt` exists in a settings directory without
  `preferences.dat`, the first launcher start refuses the directory as a
  "nonempty store" (existing first-run rule). Create the file only after the
  first start.
- The wheel line exists only for wheel L2 packages; R2, pad and F10 packages
  say not-recorded.
- A tester who edits `report.txt` while the game is still running and keeps
  the pending line gets the session facts added to it after the game closes;
  other lines are kept.
- No physical check of any of this has been done.

Spark: not used. The work was bounded C++ implementation whose checks are the
compiler and unit tests; no extraction, log triage or inventory step was large
enough to offload.
