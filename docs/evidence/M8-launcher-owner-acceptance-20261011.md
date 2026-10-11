# Internal launcher owner acceptance

October11, 2026. The owner completed the bounded launcher test: existing save and
name loaded, controls/pause/audio/Alt+Enter worked, F10 recorded, save/name survived
restart, closing the launcher left the game running, and the game then closed
normally from the owner's perspective. Primary visual and integrity review plus
an independent read-only evidence review accept the return. No replay is needed.

## Exact build and personal root

The immutable candidate remains `private/l2`, with its original package version
0.19.0.0. Source version advances independently when tasks are committed; that
does not replace or re-label these accepted binaries.

- Host: `9195117A71A69F42B505838447C350EB8B0A6A2CC9FA14C535DCB0630240AAEE`
- Launcher: `BE79E0C3269CC80F9736D0066AC01177C4D73B34AE597964DB84259FDE9BB2DB`
- Build manifest: `9C6BEA718046F1B22D7FB9C4BEB8E03CC52DD3F466CCFB926CC585059F580BAA`
- Game fingerprint: `95DA270942B4BDDBC8DC5EB45AA742AC6BE191D74E430022070D9B9B5AF25048`

The selected MAIN root is
`C:\BDU\MCLA-Saves\MCLA-f5d835a679843f1e95de84646d2a8646`. The explicit legacy
import records `private/p1/user`; original p1 remains intact. MAIN is now the
active owner branch. Never continue both independently or merge them implicitly.
The three receipts agree on this profile, build/game, stock timing, named RTX5060
adapter1,2x,VSync on and startup fullscreen off. Owner Alt+Enter confirmation is
runtime behavior, not a claim that the startup fullscreen checkbox was enabled.

## Sessions and save lineage

Sessions `746c7587cbceb6298a0e40b1825c76c7`,
`d90ed29091526e434b6112ac3d406e5f` and
`a80907f81e26d381bd5c969def228598` all finish `complete / exit 0`.
Their launch-receipt SHA256 is
`D2049299473B10758A8791003F994F316B265B8EA91DDB70A5EEE653B91E6FC9`.

All six before/after snapshots contain exactly6files and8directories. Independent
review verified36 snapshot files,11 diagnostic manifest artifacts,8 private
diagnostic-save files and12 current/source profile checks with no mismatch.
All snapshot manifests and import receipt share SHA256
`164A8DC42C64FEBBD8EDB0E6EE41D74FA10094775007B46FCC3729245F8F73DA`.
Current managed user and original p1 user both match the imported six-file tree.
This proves preservation/reload, not new progression or newly changed save bytes.

Main save SHA256:
`439E2A841D01863087EEA2D132FED9AB47DBEE3A36AFDDEF6FB8AEC41929A972`.
Display-name file SHA256:
`257870D8ACCCF4D44E97758A0DA2920C489C0F03938E3FE4FFD2B2047F5BDD00`.
No remaining game, launcher or image-helper process was observed at primary's
return check. Launcher-close survival is owner-confirmed; receipts do not
independently timestamp the moment the launcher window closed.

## F10 image and honest shutdown limits

Accepted package under the selected root:
`diagnostics/d90ed29091526e434b6112ac3d406e5f/live/live-20261011T063617Z-27796-1`.
Reason `f10`, six artifacts verified by both primary and independent review.
Primary viewed the1600x900 BMP: actual nighttime road scene with the player's
Mazda and F10 recording overlay; no blank overlay or desktop capture.
Frame hash `48B04081C03D0F5A8172089FD6B3D096C820254CAD8DD284A97CC6ACAD6056BC`.
The admitted WGC helper reports client/helper failure0, helper exit0 and completed
cleanup. Freshness remains `not-established`; this is image-only evidence.

The audit contains155 samples0..154 and no END marker because the game closed
about7seconds after F10. This is not a complete300-sample or performance PASS.
No additional F10 is needed to accept this launcher's image/package routing.

The first session's `.partial` at06:31:53UTC is an automatic
`unexpected-guest-exit` diagnostic, not a failed owner F10. Preserve it without
calling it complete. The final session's06:37:32UTC automatic package is complete
but has no frame, also not an F10 capture. All three logs show window closing
followed by `Title terminated; hard-exiting process.` Thus launcher finalization
and user-visible exit pass, but graceful SDK teardown and automatic-exit report
classification do not. This existing shutdown/reporting limitation remains
M8-017 work. No runtime fix or new game reproduction was attempted here; the
diagnosing-bugs workflow was limited to read-only classification of retained
artifacts, with implementation/repro phases intentionally out of this acceptance.

## Final gates and commit boundaries

Independent production review found no blocking defect in the inspected
controller/store/filesystem/snapshot/process/managed-host/UI lifetime boundaries.
It did find a missing literal two-build fixture. The new test creates two valid
different build fingerprints, proves one-profile contention rejects the second
writer, then runs separate profiles simultaneously with distinct save markers,
receipts, cache/diagnostics roots and correct final snapshots.

Primary rebuilt and ran the controller suite:69 checks,0 failures. Retained
fixture `TEMP/mcla-controller-test-2a814ae2f2f5090690cf7451be225489`.
Other replayed gates: policy117, codec315, preferences138, managed50, filesystem72,
store107, process41, build109, game70, snapshots21, import-policy72,
production-import29, UI399; expiry policy1417/transport26/runtime37. Six native
symlink cases remain SKIP with Windows1314, never promoted to PASS.

Implementation commit b67e1be closes M8-015 and includes required M8-016 profile
infrastructure plus the already-tested lease dependency. A separate M8-016
acceptance commit at source0.20.1.0 closes its isolation/lineage matrix. This preserves one closed
task per commit; it does not retrospectively claim an agreed combined task.
M8-018 remains open for explicit diagnostic export, volunteer acceptance and
distribution-content gates. Explicit render targets, validated60 and baseline
bugs are not closed by this launcher acceptance. No source push is authorized
mid-milestone; the separately approved public allowlist is the only early push.

Final staging63, structural scan and all3 structural-rule tests passed again.
Explicit73-file staged inventory and dependency/privacy review returned GO;
no private/generated/game/runtime binaries or SDK WIP entered the implementation
commit. A transient Git index lock from background status scans cleared without
deleting any lock or terminating another process; the subsequent commit succeeded.

At the two-task checkpoint the owner separately requested UI/UX refinement.
M8-019 tracks that next change; functional M8-015 acceptance is not a claim that
the initial dense UI is tester-ready or visually final. Existing l2 remains the
accepted gameplay artifact while the new interface is developed independently.
