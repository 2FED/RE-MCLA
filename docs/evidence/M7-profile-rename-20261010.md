# M7 local profile rename

The owner explicitly retained persistent local rename in M7 on October 10.
The implementation separates the displayed name from storage identity. Native
fixture and SDK tests pass. The owner now confirms the new name is visible in
the interface and name/progress survive restart, with pause, audio and Alt+Enter
working. F10 was initially not pressed; the later owner-triggered capture below
now passes its separate integration gate.
The prior Pinkslip campaign remains untouched.

## Accepted renamed profile return

### Subsequent F10 acceptance

The owner subsequently pressed F10 and explicitly observed recording complete.
After normal game exit, a separate return was preserved at
`private/evidence/M7-002/profile-f10-return-20261010-2`:220 entries/55,314,850 bytes,
tree `3E852D397061D7B94021A0373F640FC871BD823DC85E37B533C523818B1E72F2`,
receipt `BE32F88838D44DB0419CDA3E93B393CDCA1712C7E4042D1D0372E60B37C3060F`.
Full profileEFF52498/saveEE824CF5 remain unchanged. The original return is kept.

Package `live-20261010T124941Z-42540-1` has reason `f10`, all six artifact hashes
verify, helper6E766EDF is admitted with normal exit0/cleanup complete/no errors.
Primary and independent visual review accept a coherent1920x1080 nighttime
car/street/traffic scene with English Karol phone text and the recording overlay.
Image SHA256 `19F2FEE7D7ADE81811748B733154573006357EAE55EE950AA3DE277058CD11C2`.
There is no visible desktop, empty black client or unrelated covering window.
The log has300 consecutive audit samples0–299 and one PASS summary. The new
`session-20261010T124849Z-26276` passes production verification:gameplay exit0,
33immutable files,one snapshot,zero watcher errors,RTX5060.

Independent review rehashed all220 entries and the unchanged full-profile archive.
This accepts the owner-triggered F10 integration, not exact image/sample alignment,
temporal freshness, measured cadence/performance or graceful diagnostics teardown.
The manifest truthfully retains `freshness: not-established`. No repeat needed.

### Initial rename and restart acceptance

The October 10 return from `private/p1` is preserved at
`private/evidence/M7-002/profile-owner-return-20261010-1`: 180 entries,
38,375,514 bytes, full return SHA256
`13048698570CF0C2784108B23CF7ED68B6B4DD970C766F68D08EBE1978C42B0C`.
Receipt SHA256:
`0BA4EBC51AEBB5BF01DE3AC9284232ECF574C20BF6EE5D1DBAAFA2DBD6E5ACFA`.
The collector held the source bundle lock, validated the production return,
verified the copied tree and unchanged source, and archived the complete profile.

Full profile/archive:
`EFF5249882D8E895AA820199EB800BD33F427555AD205797CA02EE0FC8B45F6A`.
Main save:
`EE824CF54BBE717783909C62EEBC135B37C29B4516A4D177D70E88A593228DEB`.
Header remains `1D717009B10A84525F468AD826A7DC016B744FA78274865A7DAB8A1BB65A4957`.
The source seed is MAIN693B, followed by the completed OrangeNoob rename and
two gameplay sessions: `session-20261010T123005Z-47616` and
`session-20261010T123631Z-40908`. Both exit0, verify33 immutable files and report
zero save-watcher errors, with three and two recoverable snapshots respectively.
Logs identify RTX5060 and30035 mappings. The production return resolver accepts
the full session/name lineage; normal pointer and older campaign branches remain.

The owner explicitly confirms name visibility in the UI, persistence of name and
prior progress after restart, and working pause/audio/Alt+Enter. Their initial
F10 claim was corrected: the key was not pressed. No accepted gameplay F10 image
exists in this return. Its completed live package is the earlier diagnostics
probe, and two empty automatic-exit partials are neither F10 success nor crashes.
This initial return did not accept F10; the subsequent capture above does.

## Owner launch path failure and correction

The owner's first launch failed before game creation while copying the initial
recoverable snapshot: `The filename or extension is too long`. The original
candidate root is 136 UTF-16 characters; nested snapshot paths exceed ordinary
Win32 limits. Its earlier short temporary-folder relocation PASS did not test
session creation at the actual published root. Verify-only is insufficient.

The native synthetic-child fixture now has `-LaunchOnly` and `-RootLength`.
With the exact 67B8 launcher, root length100 passes one child/one snapshot and
preserves the synthetic profile; length136 reproduces the exact owner error
before child launch, without any rename or proprietary files. The fixture's
bounded supervisor terminated only its own launcher after the failure dialog;
synthetic temporary files were cleaned. The successful rename is not the cause.

Read-only return validation confirms the owner's `OrangeNoob` preference and
live profile `09D0FBF11A4D1B657DBEBCEEE069D3F295B829C8ECA27D8C9F0383308B0F002D`
match the completed rename journal's after state. Before remains MAIN693B;
save49DE59C7/header1D717009 are unchanged. No game session or game log was created.

The narrow correction copies the entire unchanged package, including live name
and rename journal, to absent `private/p1`. Source locking, full materialized
copy/source rehashes, returned-profile lineage and normal-pointer checks are
implemented in `scripts/stage-short-profile-candidate.ps1` and independently
reviewed. Preserve the original long-root package; do not reseed or rename again.
No runtime binary, game setting, registry or OS long-path policy changes.
Staging PASS: 85 entries/6,650,025,530 bytes, full tree hash
`4D0C631554703926FD777212DE984DF874774CB73484FDD098E51005327D2E56`.
Receipt `private/evidence/M7-002/profile-short-path-20261010-1/relocation.json`
SHA256 `4899E0BC9F4975D672DC64FA031643200D599856F174030D01FE12E2B5DBAC6E`.
Every file is an independent materialized copy; source and normal pointer match.

The actual `private/p1` diagnostic probe passed with named RTX5060, exit0,
one recoverable snapshot, zero watcher errors and unchanged09D0 live profile.
Session `session-20261010T122347Z-27972` and its diagnostic package validate with
the production session verifier. This runs the diagnostic host path, not guest
gameplay. The existing owner wrapper now targets `private/p1`, retaining all
immutable pins. No second rename is required. Arbitrary long-path support is
not claimed; future package destinations/profile growth need fresh validation.

Publisher now budgets the actual staged/final roots and every selected profile
entry against known snapshot, rename and diagnostic nesting before build and
again before staging. Conservative limits are259 file/247 directory UTF-16
characters, with maximum PID and diagnostic sequence widths. Independent review
GO; PS5/7 package suites PASS5 positives/105 negatives/45 source checks and three
path-boundary positives. The first source-order assertion searched a nonexistent
directory-creation spelling; corrected to the actual call, without guard changes.
PS5/7 redirected owner-entry CheckOnly verifies all33 immutable files and named
RTX5060 index1. No new binary or game-performance claim accompanies this correction.

## Identity and recovery contract

One local profile retains XUID `B13EBABEBABEBABE`, legacy settings storage key
`User`, and existing content paths. `local_user_display_name` defaults to `User`
and accepts 1–15 ASCII letters, digits, spaces, underscores or hyphens, without
leading/trailing spaces. Invalid resolved configuration fails before runtime
initialization; names are neither truncated nor silently substituted.

`Launch-MCLA.exe --set-profile-name="New Name"` is a standalone operation, not
a game launch. It requires a complete existing save/header, takes the bundle
lock, rejects unfinished sessions, snapshots the full profile before and after,
and proves that only `local-display-name.txt` changed. The file replacement and
completed journal publication are atomic individually; the entire operation is
not a rollback transaction. A failure preserves its partial journal and backup
for explicit recovery. Same-name requests do not create a transition.

Every subsequent launch reads the preference from the profile and passes it as
one quoted argument. Inherited name environment overrides and immutable TOML
name overrides are rejected. Native rename journals enter the production save
lineage resolver and content-addressed archive. Only validated name-only edges
may collapse for gameplay ancestry; divergent gameplay remains an error.

The native launcher also resolves an exact D3D12 adapter name against current
DXGI enumeration. It requires one capable match and rejects fallback/conflicting
backend, environment or config selection. A read-only adapter probe selected
the RTX 5060 Laptop GPU at index 1 on this machine; this is not gameplay proof.

## SDK build and test results

SDK base is `c2e67161f1207ddcee609507c399efe7a1981b72`, with reviewed local changes.
The isolated Release build is `out/build/win-amd64-profile-20261010` within the
SDK; outputs are `out/profile-20261010/Release`. The optional absolute
`REXGLUE_OUTPUT_ROOT` preserves historical defaults while avoiding writes to the
accepted shared outputs. Only runtime and unit-test targets were built.

New runtime SHA256:
`542DD347B56B0494F42A7B261EC8309897E867C14FE1D2456B1836AE6C12650F`.
All 114 imports required by the accepted GPU DLL resolve in this runtime. Its
29 dependency module names match the old runtime; neither imports TracyClient.
Symbol availability is not a substitute for physical integration testing.

Focused SDK results: profile 11 cases/140 assertions, content 10/117,
configuration 27/149. The first new content test used a private API and failed
compilation; it was replaced with the public write/read/content-list path and
the subsequent runtime/unit build passed.

The monolithic SDK test process fails fixed guest-memory reservations. A minimal
two-case reproduction also fails without the new profile cases: the unchanged
heap-allocation and function-dispatcher fixtures retain static `Memory` objects
until process exit. Each conflicting case passes alone. No runtime memory
workaround was introduced.

`scripts/test-sdk-unit-isolated.ps1` explicitly discovers all compiled cases,
then runs each source suite in a new bounded process and checks exact case names
and counts. Result: **414 cases, 17,300 assertions, 43 suites, four existing
BitStream Write skips**, no failures. This is not a monolithic-process PASS.
Receipt: `private/evidence/M7-002/sdk-unit-isolated-cf05f093e7944141870cad8e886ab210`.
The earlier equivalent 43-report receipt was independently rehashed and reviewed.

## Native and packaging checks

- Native adapter policy: 78 checks PASS.
  Both the actual template and packaged config pass. A later invocation without
  its required config argument correctly failed; corrected invocations pass.
- Native profile fixture in PS5.1 and PS7: 26 calls, 21 rejections, two dummy
  child launches and two rename journals PASS. Full snapshots, spaced argument,
  legacy settings, production lineage/archive and empty save/header rejection
  are exercised without game files or owner saves.
- Save lineage: 123 checks PASS in PS5.1 and PS7, including repeated renames,
  gameplay divergence and exact live-profile selection.
- Bundle fixtures: each shell passes five positives, 95 fail-closed negatives,
  45 source checks, two capture-cache and two settings positives. Explicit SDK
  selection, WGC manifests and old bundle return formats remain covered.

The candidate SDK prefix copies the accepted install and replaces only the
runtime DLL/import library and profile header. The accepted GPU and Tracy DLLs
must remain byte-identical; no speculative renderer changes are promoted.
`scripts/stage-profile-name-sdk.ps1` records complete before/after manifests and
source/cache/link provenance without invoking installation or changing registry
package entries. Independent review required symmetric overlap rejection; that
guard is included. Candidate staging/build/owner acceptance are separate gates.

Staging completed with 1,237 files and exactly three replacements; receipt
`33B89D237D42038BC08DF0BCB5C75258013BC4CDB3CB1120B2D4C1D5C8A30437`
is in `out/install/win-amd64-profile-20261010/profile-name-sdk-staging.json`
within the SDK. The accepted source prefix was rehashed unchanged.

The first publisher invocation stopped before creating the candidate or building:
`PSBoundParametersDictionary` does not expose the hashtable-style one-argument
`Contains` method. The real bound-parameter reproduction failed, then the guard
was changed to key enumeration. Both shell fixture suites now include this seam
and pass; the clean candidate build subsequently started.

## Clean candidate and remaining acceptance

Clean Release completed 82/82 steps. The WGC-enabled stock 2x/VSync candidate
uses the staged SDK and explicit main archive
`693B31EBB2A0F92EE6DD452F60A053DB5086B9A573FCBA479E840E4E9E8865C8`
in fresh private root `private/bundles/M7-Profile-Portable-20261010-1`.
Bundle ID: `mcla-0.18.0.0-win64-82A69B5E922DE807-67B8F5272016D686-49DE59C7F2BFEFE6`.
Publisher evidence: `private/evidence/M7-016/bundle-20261010-120612-c5724211`.
Native verification and relocated diagnostics/save-session probe PASS; no
obsolete bundles were removed. The temporary relocation copy was cleaned after
its evidence was preserved; the candidate and original saves remain available.

| Artifact | SHA256 |
|---|---|
| Host | 82A69B5E922DE80726F76B2345C9FFE1FE143FFB485E4269FB1586DE9230509A |
| Native launcher | 67B8F5272016D686EDF2E8B5FBAE43945B8E00E19917637AC8DF29C1DEB5F927 |
| WGC helper | 6E766EDF536C8CA8B65BC5234C37A4A4BDA1463470E203538E71798F882B4675 |
| Immutable lock manifest | 5BFEF66655B9A9E9C18F080EB3915CE9251534885848E881FB0A43A60C713BCD |
| Bundle manifest | FD46B60F6CDBE61761D253A72715B00D3FBB3D7105FBA9133055B40D9F7DECD2 |

Final clean launcher passes the native profile/lineage/archive fixture in both
shells (26 calls, 21 rejections, two dummy launches, two rename journals).
Owner entries are `Rename M7 Profile.cmd` and `Run M7 Profile Candidate.cmd`.
The former prompts for a valid name and never launches the game. The latter
resolves the named RTX 5060, with fixed stock/2x/VSync settings and WGC F10.
The wrapper pins the launcher and both manifests; the launcher checks every
immutable payload before launching. No F4 config edits during this package test.
PS5/7 entry CheckOnly passes with 33 verified immutable files and named RTX5060
index1. Native stale-lock/interrupted-session/unfinished-diagnostic recovery
passes with the final launcher and a synthetic child. Independent final entry,
artifact and seed review GO. Main and candidate profile hashes still match693B;
old Pinkslip host/runtime and normal0.16.0.1 pointer remain unchanged.

The name/progress/audio/pause/Alt+Enter/reload and subsequent gameplay F10 gates
are accepted above. No race replay or deliberate interruption is required.
Clean and Sandbox branches are not promoted; accepted races need not be replayed.
No M7 task closure, normal publication, version bump, commit or push is implied.
