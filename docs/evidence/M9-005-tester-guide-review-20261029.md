# M9-005 tester guide review against the merged tree (2026-10-29)

**Status:** documentation review only. No game, launcher or helper binary was
started; nothing was staged or published. This is not a tester smoke and not a
physical PASS. Base: branch `claude/admiring-jang-24d485` at `6981f51`.

Goal: the volunteer-facing guide (`docs/volunteer-tester-guide.txt`, staged as
`TESTER-GUIDE.txt`) must match what the merged tree does before the refreshed
volunteer package is built.

## Sources checked

- Code (user-visible strings and behaviour): `src/mcla_session_report.h`
  (launcher prompt, session `report.txt`), `src/mcla_diagnostic_trigger.h`
  (per-package `report.txt` fields, T300 default `"8,9"`, hold combo),
  `src/mcla_app.cpp` (cvar definitions and defaults, status overlay texts
  `RECORDING...`, `PACKAGE READY (IMAGE UNVERIFIED)`, `BUSY`, `FAILED`, trigger
  labels `F10`, `WHEEL R2`, `WHEEL L2 (WHEEL BUG)`, `PAD START+BACK`),
  `src/mcla_launcher_controller_win.cpp` (launch argument list, capability
  refusal path), `src/mcla_launcher_ui_win.cpp` (`CHECK SETUP`, `START GAME`,
  "Setup saved."), `src/mcla_launcher_capabilities.h` (gated options, refusal
  texts), `src/mcla_build_lease_policy.{h,cpp}` and
  `src/mcla_build_lease_runtime_win.cpp` (kill-switch rules and messages,
  3 days = 259200 s, 24 h = 86400 s).
- README-AI rows M9-005, M9-007, M9-009, M9-010; `docs/known-issues.md` rows
  KI-022, KI-026, KI-032, KI-035, KI-037, KI-038, KI-040, KI-041, KI-042, KI-043,
  KI-044, KI-045, KI-046 (closed), KI-047, KI-048; `docs/M9-priorities.md`
  (kill switch, report prompt); `docs/current-state.md`; evidence
  `M9-010-diagnostic-triggers-20261029.md`, `M9-launcher-admission-20261029.md`,
  `M9-l41-owner-candidate-20261029.md`, `M9-005-volunteer-build-20261028.md`.

## Key finding: volunteer builds cannot offer Unlocked or 1080p/2160p

`ResolveCapabilities` admits 1080p/2160p only for two fingerprints compiled
into the source and Unlocked FPS only for `kUnlockedTestedBuilds` (the old
l33/l34-sunc build), unless the owner admission file lists the build. The
fingerprint is the SHA-256 of `mcla-build.manifest`, which pins the signed
ticket, so no new volunteer build can be listed in its own source. Volunteers
have no admission file (owner-only, never in a package). Result on every
refreshed volunteer build: 720p and 1440p at 30 FPS only; CHECK SETUP refuses
1080p/2160p ("1080p/2160p require the render-target-tested build") and
Unlocked ("Unlocked FPS is not verified for this build; choose 30 FPS").
The previous guide presented Unlocked as a normal choice and recommended
"720p-1440p" (1080p included); both were wrong for volunteer builds.

## Guide changes (EN and UK kept equivalent; UTF-8 without BOM, CRLF kept)

| # | Change | Source |
|---|---|---|
| 1 | First start: resolution "720p or 1440p", frame rate "30 FPS (recommended)", CHECK SETUP shows "Setup saved." | capabilities gate; `mcla_launcher_ui_win.cpp` "Setup saved." (KI-044) |
| 2 | Online check reworded: signed list; offline 3 days from build or 24 h after the last successful check; a running game is never stopped | `EvaluateKillSwitch`, constants 259200/86400; M9-priorities kill switch |
| 3 | New section "Settings on this build": 1080p, 2160p, Unlocked not offered, CHECK SETUP refuses; 2160p exceeds 8 GB video memory | capabilities gate; KI-048 |
| 4 | Unlocked limits kept as a conditional line ("if a later build offers Unlocked"): pause/map/Arcade/meet spot at 30, junctions 30-40, night flicker, victory-screen lighting, one graphics reset | M9-007 closure (hangout capped on l42); KI-041; KI-043; current-state `DEVICE_HUNG` |
| 5 | Triggers: F10, wheel R2, pad Start+Back about 1 s (pause menu may open), wheel L2 for wheel/FFB bugs | M9-010 evidence; cvar defaults |
| 6 | One press = one report; RECORDING..., then PACKAGE READY (IMAGE UNVERIFIED); wait for READY; BUSY/FAILED meaning | overlay code in `mcla_app.cpp`; Spark finding (b1/c3) |
| 7 | L2/R2 set for a T300, not yet confirmed on a real wheel; on other wheels use F10 and name the wheel | `kDefaultWheelButtons = "8,9"` (unverified); the launcher passes no wheel cvar and has no pass-through, so volunteers cannot apply the probe setting (exact setting documented for owner runs in `docs/testing.md`) |
| 8 | Per-folder `report.txt` prefilled by the game; L2 report names the wheel; describe the scene because the picture may be earlier | `BuildPackageReport` fields; KI-042 workaround |
| 9 | Launcher lists reports after the session; send each whole folder by hand; nothing uploaded | `PromptText` |
| 10 | Known problems rewritten with actions: KI-035 (F10, wait 15 s, F10 again, wait 15 s, then close), KI-026 (F10, READY, restart from autosave), KI-022 (wheel L2), KI-038/032 (signs; F10 welcome), traffic-light paint glow kept; KI-037 and KI-040 moved to "should be fixed, report if seen" | KI rows; KI-037 owner PASS on l41; KI-040 fix in the SDK, informal owner re-acquire on l40 |
| 11 | Launcher messages: withdrawn, BUILD CHECK NEEDED, and a new clock line | kill-switch strings; "Invalid test-build identity or clock", "System UTC clock changed backwards or is invalid" |
| 12 | Removed: "the save is safe" after a garage freeze (not established by KI-035); "Signs ... mostly above 1440p" softened (above 1440p is not offered) | KI-035; capabilities gate |

The English section stays ASCII (the bilingual header line is the only
non-ASCII line in it). Length: English 68 lines, Ukrainian 73 lines, at most
82 characters per line (the longest line is unchanged from before).

## Other documents

- `docs/test-build-expiry.md`: new section "Owner-internal versus volunteer
  tickets (owner rule 2026-10-29)": V1 for owner-internal candidates (14 days,
  no kill switch, no online requirement; the list can only extend), V2 for
  volunteer candidates; roles of the prepare scripts described without
  personal paths (checked against the owner's l38 volunteer and l41 internal
  prepare scripts: `-KillSwitch`, `New-MclaTestBuildLeases.ps1`, `-TesterGuide`,
  lease probe, `Test-MclaVolunteerStaging.ps1 -RequireVolunteer`; V1 asserts
  `MCLA-TEST-BUILD-V1`). The new lines use LF like the adjacent kill-switch
  section.
- `docs/testing.md` (M6-014 live snapshot section only): current status text
  and trigger labels; M9-010 cvars with defaults; the probe procedure with the
  exact flags `--mcla_wheel_button_probe=true` and
  `--mcla_wheel_diagnostic_buttons=<L2>,<R2>`, stating that the managed launcher
  passes neither (direct owner runs only); per-package and session
  `report.txt`; link to this review.

## Spark (DGX) advisory job

- Packet: EN and UK draft guide text plus a bullet list of implemented
  behaviour; task: (a) EN/UK inconsistencies, (b) contradictions with the list,
  (c) missing tester-critical steps. No secret in the packet (checked: no
  `LITELLM` string). Packet SHA-256
  `17b50bc2bcb940d580d99d18e88e7e4da1c1b22dddc94c067c6eaf2da8c7448a`.
- Job: `private/evidence/spark-m9005-guide-review-20261029`, alias
  `qwen3.8-27b`, `--max-tokens 4096`, `--timeout 600`, started
  2026-10-29T16:53:42Z, 191.9 s. **Receipt status `REJECTED`,
  `chat_not_complete`, `finish_reason: length`** (4096 completion tokens,
  0 reasoning tokens; the model spent the budget restating equal passages).
  Receipt SHA-256 `9638a0ebd6cfc1d2add51e6d36270faffde4d87536336240053bfef26681eb0e`.
  Not retried (doc rule). The persisted partial answer was read as advisory
  text only.
- Accepted (verified against `mcla_app.cpp`): the guide omitted `BUSY` and
  `FAILED` and shortened `PACKAGE READY (IMAGE UNVERIFIED)`; added to both
  languages. Accepted in part: "set for a T300" lacked "not yet verified";
  added "not yet confirmed on a real wheel".
- Rejected: "prefilled by the game contradicts the launcher session line"
  (the game writes the file; the launcher adds one line, so "prefilled by the
  game" is correct); "(a) graphics reset vs збій (скидання) відеокарти" was
  itself concluded as equivalent by the model; listing every report field was
  judged unnecessary for testers.
- The model found no EN/UK meaning difference (category (a): none). The
  primary also compared both sections line by line after the edits.

## Tests

- `scripts/Test-MclaVolunteerStaging.ps1` (Windows PowerShell) on a temporary
  folder containing only the edited guide as `TESTER-GUIDE.txt`, with extra
  sentinels for the owner's name, e-mail local part and test folder name:
  11 checks, 0 findings, rc 0. This checks the guide for owner strings and
  allowlist conformance only; a full staged audit needs the refreshed package.
- No other test or fixture references the guide (`grep` over `scripts/` and
  `tests/` finds only the stage script and the audit).

## Not verified / open

- The Unlocked limits line is conditional because no volunteer build can
  offer Unlocked under the current gate. If volunteers should get Unlocked or
  1080p, a source change is needed (out of this scope); then section 2 must be
  rewritten.
- The launcher's own Unlocked note (`TimingLimitation`: "fast menus over the
  city") predates the M9-007 cap; it is a code string, not changed here.
- L2/R2 on a real T300 and on other wheels, the Start+Back hold and the
  per-package `report.txt` on a volunteer build are not physically verified on
  a volunteer build; KI-040 fix acceptance is informal.
- KI-042: the overlay says IMAGE UNVERIFIED; the guide asks testers to describe
  the scene rather than claiming a fresh image.
- `docs/testing.md` M6-014 one-hour section still says `F10: READY`
  (historical gate text, left unchanged).
