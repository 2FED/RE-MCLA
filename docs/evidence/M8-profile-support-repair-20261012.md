# Automatic recreation of missing profile support folders

October 12, 2026. The owner could not launch l11 because the selected MAIN
profile lacked `logs`, `backups`, `metadata` and `metadata/update`. The build,
game and profile roots existed. Calling the production directory guard directly
reproduced `cannot hold directory ancestor (Win32 2)` for these four paths.
The cause of their disappearance is unknown; no owner action is presumed.

The l12 candidate permits this layout and creates missing support folders when
the owner presses START GAME. Engineering tests and independent source/package
review pass. The owner now confirms automatic repair, successful launch and the
existing save present in l12. This accepts the reproduced missing-folder route,
not M8 closure or recovery of lost historical files.

## Repair boundary

CHECK SETUP, including its automatic startup check, remains read-only. Missing
`cache`, `logs`, `diagnostics`, `backups`, `metadata` or `metadata/update` can
produce Ready with an explanation that the folders will be recreated on START
GAME. Existing files are neither restored nor replaced. Folder recreation does
not recover any historical contents that might have been lost.

START GAME independently rechecks the profile identity, existing exclusive
writer lock, import/pending state, completed session history, save snapshot
eligibility, existing cache, build/game identity, GPU, capabilities and signed
eligibility. Only after these checks does it create missing allowlisted folders,
one component at a time under held parent guards. Each created directory is
reopened and guarded through the session. Competing entries, files, reparse
points and other access errors fail closed. Partial empty-folder creation is
retained, never recursively rolled back.

`user`, `sessions`, the managed-profile marker and writer lock are mandatory.
They are never invented by this repair. In particular, Start now opens the
existing writer lock rather than recreating one. Missing or obstructed folder
errors identify the relative profile folder rather than only a generic ancestor.

## Regression evidence

The controller test first failed with 275 checks and 18 failures on the old
implementation. Final tests pass 335 checks with zero failures. Coverage includes
each optional folder absent, all absent, absent metadata parent, file obstruction,
a late obstruction preventing earlier creation, changes after Ready, mandatory
root/identity/lock absence, competing writer, pending/import state, unfinished
session, changed identity, bad name, hardlink/ADS save, invalid build/game/GPU/
target/VSync, actual bounded fixture-child launch, normal completion, retained
user and backup sentinel files, and idempotent read-only recheck.

The directory-symlink fixture remains SKIP because Windows denied creation with
1314; it is not counted as native reparse coverage. Additional filesystem/store/
snapshot suites retain four analogous skips. Passing totals: filesystem 76,
store 125, snapshot 26, preferences 1064, capabilities 168, diagnostic export
policy 505, production import 31 and native UI 1065. AST scan and all three rule
fixtures pass. Staging passes 63 checks with no skips.

Production no-game preflight against the exact l12 build/game/GPU passes 20
checks with its own fresh profile missing all support folders, then 16 checks
with a complete fresh profile. Both preserve the complete fixture inventories;
neither creates a session or launches a game. The new
`--preflight-missing-support` switch requires `--preflight-only`, accepts no owner
profile path and returns before Play. Synthetic-child execution above is a
separate test, not a real-game PASS.

Preserved private evidence:

- Direct production guard diagnostic: `private/evidence/M8-profile-layout-20261012`.
- RED controller: TEMP `mcla-controller-test-f83f411fa71f060b16cc665d83ebfa9a`.
- Final controller: TEMP `mcla-controller-test-a86fcc632c51c2fa47a4be605273a6dd`.
- Import: TEMP `mcli-import-837a141288890374b0d57402da858627`.
- Staging: TEMP `MclaStagingTest-20d91cb06be44b4d976c727dda97acd9`.
- Missing-support preflight: TEMP `mcla-host-probe-5b98cb0d1f183aec26cc54ef2b637113`.
- Complete preflight: TEMP `mcla-host-probe-7a9775a9937c1a09c969fd28af3df473`.

## Candidate identity and owner return

Use `private/l12/MCLA-Launcher.exe`. Its 17 payload entries plus receipt verify
independently with no extras, reparse points or alternate streams. Only launcher
and receipt differ from l11; the complete FED35 build, CRT and signed ticket are
unchanged. No game assets or personal data are staged; external delivery remains
unapproved. The ticket was copied, not renewed.

| Artifact | SHA256 |
| --- | --- |
| l12 launcher | DB0D84A5C7A2F61A27EA1C68859420F54E95BC4EC23FA4BA46F30AB4A297B4C8 |
| l12 receipt | 7AFA6877A15D8992EBAA8BC1B1169D93F4746BC0F6CC50E673571FA2708757EC |
| Unchanged build manifest | FED35A361F4A4A10BB03A6BC45072C82924085A0D3ADB8A3298243066DB4A850 |
| Controller source | 4EAB5B2D826B087CC2668DB1C16EFA7654B6315FED2FFA9C3A3B2A6C515E6EB6 |
| Controller test | C90E6068FAE3BB0C3B0D7397ED2265AC6B7AA5A5FF04B5A64A072B2E88BFA027 |
| Host probe source | 51B01038E56448B5D306DB826FE727E09E3C430B75587444E19FE8DC095EA28B |
| Host probe binary | 05B94F68E177BD4D0B740655514D385DB873AC1CFEA24F0B631808599194570B |

Before the owner return, the original MAIN was not repaired manually or launched. Its six protected user
files and launcher preferences remain byte-identical; campaign AD934778, name
257870D8 and preferences D10D4E0A. All eight existing session states are complete.
Stored preferences still contain the older l2 build path, while the owner reports
selecting l11 in the unsaved Setup form. This is not the missing-folder cause.

### Accepted owner return

The owner launched l12 and reports that the folders repaired themselves, the game
started and the existing save was present. Missing support containers now exist.
No manual repair, reimport or campaign branch was performed by the agent.

Two new sessions are complete with exit0: `71ff547af27ac1e5a03cb427de38306c`
used the historical 9C6BEA71 build; `0777f856984c2b27541ef2872d9a9aee` used
the corrected FED35A36 build, stock timing, RTX5060 adapter1 and 2560x1440
(1280x720 guest, scale2, VSync on). The latter's six before-user and six
after-user files match their snapshot manifests. The current six user files
match after-user. Campaign changed from AD934778 to EDADDCBF during owner
play; the name remains 257870D8. This is expected save evolution, not a failed
unchanged-file test. Preferences now select l12/build and have hash5255CD8E.

The corrected session also completed F10 package
`live-20261012T054443Z-47424-1`: all six artifact lengths and hashes verify.
The 1600x900 WGC image shows the RX8 outside in the street, not a blank overlay.
The normal close ends in exit0; the SDK still logs its known hard-exit path.
No graceful SDK teardown or post-session fresh-process reload is inferred.

### Final reload and bounded closure

The owner subsequently confirms that name and progress survive a fresh launch.
Session `0e9580e73e5f5827535f32fc4863eb44` uses FED35, stock1440/VSync,
RTX5060 and completes with exit0. Its before/after snapshots and the intervening
completed rename archive `rename-d4bd65b2b5319dba315c495d196726a2` verify:
24 files, exact file inventories, previous after -> rename before, and rename
after -> reload before. Current user files match reload after. Primary and
independent audits agree. The renamed display-name hash is E1D1D30F; current
campaign is BACE2039. Earlier EDADDCBF/257870D8 describe historical states,
not current guards. No save restoration, import or agent rename occurred.

KI-034's bounded missing-support-folder defect is closed. The user accepts the
existing vinyl improvement and explicitly defers additional sharpness; that
does not close every M8-017 graphics defect, M8-007 UI review or M8-018 export.

### Isolated repair commit verification

The repair is committed separately from pending export/UI/capability changes.
An isolated HEAD-plus-repair snapshot builds both controller and host-probe
targets in Release with Clang20.1.8. Controller tests pass286 checks/0failures,
with one explicit Windows1314 symlink SKIP. Production no-game preflight passes
20 missing-support checks and16 complete-layout checks against the previously
admitted 9C6BEA71 l7 build and exact95DA2709 game. Neither starts the game.
AST scan and all3 rule tests pass; independent staged-source review is GO.

Preserved local evidence: `private/ki034-commit-20261012/source-v2` and
`build-v2`; controller TEMP96a57ddc27dba0a88f0b56f9cd29b6be, missing-support
TEMP5f66880b03c456a2cfe99e789fff08a2, complete TEMP5d05ad80cd7e846847323a578cad8ff5.
The first index-extraction snapshot misplaced insertion-only patch hunks and
failed compilation; it remains preserved as `source`/`build`. Corrected
extraction and a fresh build pass; no runtime was launched from the bad snapshot.

Repository version advances to0.20.3.1 for this bounded repair. Immutable l12
remains the owner-tested superset; its335-check suite is not attributed to this
286-check isolated commit. The isolated commit does not add FED35 high-target
admission or export code. M8 remains4complete/6deferred/19total, YELLOW; no push
or milestone closure follows from KI-034 closure.
