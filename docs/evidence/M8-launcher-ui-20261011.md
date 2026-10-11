# Launcher usability and draft preservation

M8-019 improves the internal launcher before volunteer use. The accepted l2
package and the owner's MAIN profile remain unchanged. A successor needs native
regression checks, code review and visible owner acceptance before task closure.

## Import cleared unsaved setup

The owner followed the first-run instructions: entered build and game folders,
then imported a save. Both folder fields became empty, requiring re-entry before
Save settings. This was a UI defect, not an incorrect import procedure.

`TestImportPreservesUnsavedSetup` creates an invisible native launcher with empty
persisted roots, enters distinct build/game drafts, invokes the real UI import
handler and then Verify and Save. The synthetic backend models production:
import updates the library without persisting unrelated setup drafts. It neither
touches files nor launches a game.

Command after building the matching target:
`out/build/m7-final-sdk20/mcla_launcher_ui_win_test.exe`.
Two runs before the fix produced408 checks and the same4 failures:

- Import cleared the unsaved build and game folders.
- Verify received the cleared roots.
- Verification did not retain the original drafts.
- Save could not persist the originally entered roots.

Ranked hypotheses were unconditional form refresh replacing drafts with stored
preferences, import mutating preferences, and profile selection clearing fields.
The test retained the original selection and confirmed that import did not
persist setup; production `Render` unconditionally wrote stored roots into the
controls. The first hypothesis explains the exact observed failure.

The minimal UI-only fix seeds launch settings on the first response and retains
the current form drafts on subsequent responses. Library, selected profile,
in-game name, running ownership, status and GPU inventory remain authoritative.
The same regression then passed408 checks with0 failures. No controller/storage
change or implicit Save was introduced; Play retains its existing explicit
settings-validation/persistence contract.

## Usability scope

Native Play, Profiles and Setup sections separate everyday launch settings from
profile maintenance and initial folder setup. Navigation must not invoke backend
actions or discard drafts. Keyboard access, small-window scrolling, stock-only
capability guards and exact selected save paths remain required.

Implemented with native controls and no new framework. The exact selected save
path remains shared across all sections; Play also shows the authoritative
in-game name. Setup explains that Verify does not save. Play still validates and
saves current settings before launch. No implicit import or selection was added.

## Final checks and candidate

Fresh primary builds of launcher, UI test, renderer and controller test passed.
UI648 checks, controller69 checks and staging63 checks passed with0 failures.
The expanded UI matrix covers all six setup drafts, failed Save/Play, profile
selection, Refresh, exceptions, GPU reorder/removal and VSync Off-to-On repair.
The production keyboard-navigation helper passes forward/reverse/wrap and
non-consumed-key tests without keyboard injection. Minimum420x300 logical-window
tests verify navigation remains visible and all page controls remain reachable.

Independent read-only review returned GO. Its narrow-window focus finding was
fixed by resetting scroll before focus and explicitly revealing the selected
navigation control. Follow-up review confirmed the fix and shared keyboard seam.
Primary reviewed the diff and three final120DPI1111x863 synthetic images:
`private/evidence/m8-launcher-ui-20261011-{play,profiles,setup}-2.png`.
They contain29/36/27 visible page controls respectively; labels, buttons and
footers fit. Hidden edit/combo text remains absent in this renderer, so visible
owner acceptance is still required. These are not desktop or gameplay captures.

Retained synthetic fixtures under TEMP:
`mcla-controller-test-57e9d000c9240236936678ca9ec2854a` and
`MclaStagingTest-162dffbe57d44031b96a9e22f41393b3`.
ast-grep0.45.0 scan, all3 rule tests and diff whitespace checks passed.

New candidate: `C:\BDU\MCLA-Recomp\private\l3\MCLA-Launcher.exe`.

- Launcher: `138E1CB99C967CF7D6F3A7A699EFC7E5C2E5C86C8FA1C6C28000AF64A6E27D83`.
- Unchanged host: `9195117A71A69F42B505838447C350EB8B0A6A2CC9FA14C535DCB0630240AAEE`.
- Unchanged build manifest: `9C6BEA718046F1B22D7FB9C4BEB8E03CC52DD3F466CCFB926CC585059F580BAA`.
- Unchanged ticket: `6C2FA6EAE358A2167ADA346B8E424BFC179957F072EC500CA800CFB5F360325A`.

All17 receipt-listed files match their sizes/hashes. Production verify-only
admission verified the build and game revision95DA2709... without host launch;
signed admission returned extended1/expires1794086618. The host was not rebuilt,
and the ticket was not reissued. l2 launcher remainsBE79E0C3... unchanged.
No personal preferences or saves were modified by these tests. No game ran.

l3 reuses the current local preferences, so it initially still selects l2/build
and the existing MAIN. That is intentional: the build manifest and host are
identical. No path change, reimport or migration is needed for this UI check.
The candidate contains no game assets/personal data and is not cleared for
external delivery. Source stays0.20.1.0 until task acceptance/version/commit.

The owner then confirmed l3 section switching, Ctrl+Tab and retained paths.
Their screenshot shows visible edit text. They requested another usability pass:
clear START GAME status and operation-specific profile dialogs. M8-019 therefore
remains open for successor acceptance, not another repeat of these l3 checks.

## Follow-up scope and resolution limits

Profile names label entries in the launcher library; they do not rename the
in-game user or move save folders. Permanent destination/source fields are being
replaced by dialogs specific to rename/create/register/import/copy/restore.
Cancellation must not submit an action, and the owner window must be disabled
until the operation form closes.

START GAME readiness requires a separate no-write Check setup action, bound to
the exact current settings/profile/root and expiring after60 seconds or earlier
signed eligibility expiry. Editing relevant fields invalidates green. Text must
explain unchecked, checked, blocked and running states in addition to color.
Start still independently validates and acquires the profile lock.

Read-only infrastructure checks pass: filesystem76, lease47 and snapshot26,
all0 failures. Two filesystem symlink cases and one snapshot symlink case skip
with Win32 1314. Exact existing production signed-cache inspection returned
extended1/expires1794086618 and preserved filenames, lengths, hashes and write
times. It did not test absent production cache discovery; that no-create branch
is source-reviewed. Independent helper review GO. Snapshot source validation
initially failed on nested portable inventory names; converting them to native
Windows separators fixed the26-check regression.

The actual dependency chain is `out/build/m7-final-sdk20` →
`private/sdk20install-unpublished` → `private/sdk20b` → `private/sdk20s`.
Its texture/cache CVars and EDRAM/resolve scale fields are integers. The launcher
fixes guest video mode to1280x720;1x/2x map to requested1280x720/2560x1440.
[Historical actual output evidence](M4-012-frontend-parity.md) confirms those two
principal guest-output sizes, not every intermediate render target or every GPU.
The SDK can clamp requested scale on unsupported hardware. Named targets are
requests, not a new runtime measurement.

1920x1080 would require1.5x scaling or a separately proven guest-native1080 mode;
neither is currently established. Window size/presenter resizing is not internal
render resolution.3840x2160 corresponds to integer3x, but current launcher/host
policy admits only1/2 and scale3 needs actual dimensions/HUD/device-clamp tests.
M8-004 stays open. This successor exposes only the two supported requested
targets; it does not fabricate1080p/2160p capability or alter host video timing.

## l4 implementation, review and pending owner acceptance

Operation-specific owned dialogs now collect only the fields required by rename,
create, register, legacy import, duplicate or snapshot restore. The main profile
name is read-only with Rename profile; explanatory text distinguishes it from
the in-game name. Parent commands and Ctrl+Tab are blocked while a dialog is open.
Cancel/close only preserve operation drafts in memory. Source-dependent submit
is bound to the acknowledged selected profile and root. Nothing auto-selects a
new profile. Copy/restore text references the selected profile; its exact path
remains visible on the disabled parent rather than repeated in the modal.

START GAME retains a native accessible caption, focus indication, and
high-contrast system-color behavior. Normal palette: unchecked/blocked amber,
checked Ready green, running gray. CHECK SETUP inspects current drafts without
saving preferences, publishing lease cache, creating a session or launching a
host. Its temporary existing-only writer lock does not create/modify a lock file.
Green is point-in-time evidence, not a guarantee of future disk/network state;
launch revalidates. Relevant edits, profile/root/GPU changes,60s elapsed time,
UTC rollback or signed expiry invalidate it. A blocked unchanged launch requires
another explicit check; correcting a field returns to unchecked.

Independent review found two false-ready gaps before staging: source trees
ineligible for safe snapshots and an existing file at the build-specific cache
directory. Read-only source validation and existing-cache admission now reject
both. Start uses the same checks before creating a session receipt, avoiding a
known-invalid source leaving a new prepared session. Native tests cover source
hardlinks/ADS and cache-file collisions, no-write checks and early Start rejection.
All helpers and final integrated UI/controller deltas received independent GO.

Final primary execution:

- UI743 checks/0 failures, repeated twice. Readiness clock/expiry/draft guards,
  six forms, Cancel/Escape, parent teardown, minimum viewport and production
  WM_DRAWITEM amber/green/gray pixels are exercised in hidden synthetic HWNDs.
  The ordinary palette branch ran; high-contrast fallback is source-reviewed.
- Controller120/0, store107/0 (one symlink SKIP1314), production import29/0.
  Fixture roots are TEMP/mcla-controller-test-d00ca8219dc257f1c008a0279ac2961f,
  MclaStoreTest-f10953d52fd065566e95c0fb6c5dc60e and
  mcli-import-96f25d561caf7d319f26c8154d6b6c2f.
- Filesystem76/0, lease47/0 and snapshot26/0 are recorded above; staging63/0
  at TEMP/MclaStagingTest-06f24452c0054f28961a2276e94e9214.
- Production preflight against exact l4/build and p1/game, real RTX5060 name and
  signed admission:4/0 at TEMP/mcla-host-probe-94d121bb2d59df326060afc1fb353ca0.
  It creates a fresh synthetic empty managed profile for setup, then proves the
  Check action preserves its full inventory/bytes and launches no host/session.
  Earlier identical l3-host preflight also passed4/0. No personal profile was used.
- ast-grep0.45.0 scan and3 rule tests pass; diff whitespace check passes.

Initial modal tests were RED710/13 twice. The discovery helper matched any owned
window and mistook surviving native combo popups for the closed operation.
Requiring exact operation class plus owner fixed discovery; split assertions
independently verify destruction, parent re-enable and no extra backend calls.
Production modal behavior was not relaxed to pass. Final tests pass743/0 twice.

Primary inspected all9 owned synthetic120DPI renders. Three pages1111x875 and
six forms835px wide fit labels/buttons; edit/combo text remains absent in this
hidden WM_PRINT renderer, so it cannot prove visible text rendering. Final files
`private/evidence/m8-ui-l4-{play,profiles,setup,rename,create,register,import,copy,restore}-final.png`
are hash-identical to their inspected initial images. No desktop pixels were
captured. Long readiness reasons can clip in the short summary row; full error
text remains in the scrollable Status box. Owner-visible acceptance is pending.

New immutable private candidate: `C:\BDU\MCLA-Recomp\private\l4\MCLA-Launcher.exe`.
Launcher SHA256:
`A94814E6EBB5F3F36250859C1ACB18620F93E0E088CBC73FD1194313E854EBC6`.
All17 receipt-listed file sizes/hashes verify. Host9195117A..., manifest9C6BEA71...
and ticket6C2FA6EA... are identical to the full hashes recorded for l3 above.
l2 BE79E0C3... and l3 138E1CB9... launcher hashes remain unchanged. No host rebuild,
ticket/key reissue, game launch, personal preference/save modification or upload.
This package contains no game/assets/saves and is not cleared for external delivery.

Source stays0.20.1.0 and M8-019 remains open/uncommitted until the short
[owner check](../internal-launcher-ui-check.md). Current library/MAIN/l2 build
selection should remain as-is; no reimport, game replay or repeated F10 required.
Observed weekly allowance at final testing:88% remaining; stopping for the
new visible UI acceptance gate, not usage exhaustion. M8-004 remains next.

## l5 automatic checks and collection parent

Owner approved automatic background startup checking and collection-parent
prefill after l4 delivery. The new candidate implements both without changing
the preferences v1 schema. Optional `collection-parent.dat` is bounded, versioned
UTF-8 under the locked settings store. Successful create/import/copy/restore
remembers the canonical parent; failure or arbitrary registration does not.
Malformed, unsafe or unavailable hints are ignored without repair. Fallback is
only the parent of the acknowledged selected managed profile, never its user
subdirectory or an arbitrary library entry. No startup/modal-open writes occur.
A hint-write failure reports success-with-warning for the already completed
profile operation, preventing a misleading retry. Typed/cancelled dialog drafts
win over suggestions, including a deliberately cleared parent.

Only CheckLaunch executes on the background worker; every other controller call
is serialized until it finishes. Edits/navigation stay responsive. Captured
generation, settings, profile root and GPU inventory bind both the outstanding
request and its returned model. Edit-then-revert still rejects the old result.
Worker exceptions leave a manual retry, not an endless automatic loop. Closing
hides the window and drains the check; direct destruction and message-loop exit
also wait before the controller can be destroyed. Workers never access HWNDs.
Unchanged Ready checks renew after60 seconds/eligibility expiry; dirty settings
wait for explicit CHECK SETUP. Navigation, modal open/Cancel and successful
unchanged Save/Verify preserve readiness. Explicit verification failure blocks
old green through a structured flag, not status-text parsing. Successful Verify
alone cannot restore an invalidated Ready state.

Independent review found and resolved returned-model identity drift and failed
Verify retaining green. Final source review GO. Primary fresh final build passed
all six launcher/UI/controller/import/probe/render targets. UI808/0 passed twice;
controller153/0, production import31/0, staging63/0, AST scan/3 rule tests and
whitespace checks pass. UI tests use deterministic promise gates and production
button drawing, including callback serialization, exceptions, close/drain,
expiry renewal, returned identity mismatch, draft retention and parent prefill.

Final isolated fixtures are retained under TEMP:

- `mcla-controller-test-6711e355da4699e3623797cd64969b17`.
- `mcli-import-dd76ae49648d94669890b0ff6c5e7191`.
- `MclaStagingTest-6f74eca9d92a44f99c9bc51532e3b3b0`.
- `mcla-host-probe-6b6c74461ebab3062aa7df53353e01f4`: production worker-thread
  preflight4/0 against exact l5 build/p1 game/RTX5060 and signed eligibility.
  Fresh synthetic profile bytes/inventory unchanged; no host or session launch.

One preliminary controller/import build also compiled the then-in-progress UI;
its artifacts were not staged. All consumers were freshly rebuilt and rerun
after writers froze; only that final integrated build produced l5.

Primary inspected new120DPI Play/Profiles/Setup/Create hidden renders at
`private/evidence/m8-ui-l5-{play,profiles,setup,create}.png`. Labels/buttons fit;
native hidden edit/combo text still does not render, so visible owner acceptance
remains required. No desktop capture or actual game run occurred.

Candidate `C:\BDU\MCLA-Recomp\private\l5\MCLA-Launcher.exe`, SHA256
`B020C7DC70360A05FFDB1EF17765983A76D6A1E57B2781145058E75F1DAFDA16`.
All17 receipt files match. Host9195117A..., manifest9C6BEA71... and ticket6C2FA6EA...
remain identical to l4; old l2/l3/l4 launcher hashes match their recorded values.
No game build/key/ticket changes, personal settings/save writes, new profile
imports or external delivery. Source0.20.1.0 and M8-019 remain uncommitted pending
the [short l5 owner check](../internal-launcher-ui-check.md). Genuine1080p/2160p
remain M8-004 and were not implemented by these usability changes.

## l6 fixed windows and overflow scrolling

Owner accepted l5 automatic startup/green readiness, destination-parent prefill,
Cancel preservation and fullscreen edit/recheck. Requested follow-up: remove
resizing/maximizing and both outer scrollbars when default content already fits.

The main window now has caption/system menu/minimize only; all six operation
dialogs have caption/system menu only. Work-area clamping remains. Layout no
longer reserves unconditional scrollbar space. Scroll ranges omit
SIF_DISABLENOSCROLL, recover the bar-free viewport and solve the coupled axes
from neither bar. A guard prevents recursive WM_SIZE while native bars change.
Offsets clamp back to zero when the content fits again. Small constrained windows
retain scrolling and focus-reveal access. The separate Status edit control's
diagnostic scrolling is unchanged.

Deterministic regression tests were RED846/23 twice before the production fix,
then GREEN846/0. Added reverse-axis coupling and repeated-update assertions;
final UI855/0 passed twice. Controller153/0, production import31/0, staging63/0,
production background preflight4/0, AST scan/3 rule tests and diff check pass.
Independent read-only final source/test review: GO, no blockers.
An intermediate renderer compile caught LONG/int template deduction; explicitly
typed std::max<LONG> fixed the test helper before the final build and staging.

Retained isolated fixtures under TEMP:

- `mcla-controller-test-43427d04c12eca1da1af832ad89f25df`.
- `mcli-import-f0709172b973e4d2b80a972a196afe51`.
- `MclaStagingTest-8d538cce24914d7682b1cc43ef3c4780`.
- `mcla-host-probe-45531c7cbddd1c0f568d2a1f60af7bab`: exact l6 build/p1 game/
  RTX5060 read-only readiness; fresh synthetic profile unchanged, no host/session.

All nine120DPI hidden owned renders inspected at
`private/evidence/m8-ui-l6-{play,profiles,setup,rename,create,register,import,copy,restore}.png`.
Renderer uses declared scroll-range content extents and actual child bounds,
without phantom bar padding, and asserts fitting content has no outer bars.
Three pages1125x875, rename850x378, create/register/copy850x470,
import/restore850x563. Labels and buttons fit. Hidden WM_PRINT still omits native
edit/combo text; this is not a visible-text or multi-DPI physical PASS.

New immutable candidate `C:\BDU\MCLA-Recomp\private\l6\MCLA-Launcher.exe`, SHA256
`A665EB7D12B46C70C5F8AD8E089E477829F26092E9D4662CE1A2731B5108763E`.
All17 receipt files verify. Host9195117A..., manifest9C6BEA71... and ticket6C2FA6EA...
remain identical; l2/l3/l4/l5 launcher hashes match their recorded values.
No game rebuild/launch, ticket/key reissue, personal save/settings mutation,
import, desktop screenshot or external delivery occurred.
M8-019 remains uncommitted pending only the [l6 geometry check](../internal-launcher-ui-check.md).
No repeat gameplay/F10 or l5 functional acceptance is required.

## Final owner acceptance

October11 owner confirms l6 has no unwanted scrollbars, nothing is clipped and
Cancel works. Combined with the accepted l3/l5 checks, all M8-019 criteria pass.
Final closure rerun: UI855, controller153, import31, filesystem76, snapshot26 and
lease47, all zero failures. Two filesystem and one snapshot symlink cases remain
SKIP (Win32 1314), not PASS. l6 launcher hash remains A665EB7D... above.
Source version advances to0.20.2.0; accepted binaries are unchanged. M8-019 is
closed locally after review; M8 stays open and no push or external distribution
is part of this acceptance. Next work is M8-004 genuine render targets.
