# Internal launcher foundation verification

October 10, 2026. M8-015 and M8-016 remain in progress, pending exact-candidate
owner gameplay acceptance. Native source, private staging and guest-free runtime
checks passed. No original owner save was imported, renamed or played during
these checks. M7 remains closed and p1 remains the accepted gameplay artifact.

## Implemented boundary

The native Windows launcher separates a versioned local library from immutable
build/game roots and named managed profiles. Explicit selection changes a pointer,
not save contents. Runtime admission validates canonical local roots, profile ID,
full build/game inventories and a cross-build exclusive profile lock. A sanitized
child environment, owned job and pinned resources last through all descendants.
Closing the UI leaves a running game owned until it exits.

Legacy import coordinates with the old bundle lock, validates recognized metadata
and the live save/header pair, then copies the complete user tree to a new ID.
Only verified copies receive a completed marker. Duplicate and snapshot restore
also produce new roots; restore checks archived hashes and does not clear the
original interrupted-state blocker. In-game rename archives before/after trees
without changing XUID or storage identity. Preferences publish atomically with a
previous-valid backup. Rejected/partial output is preserved, not silently reused.

Managed host mode pins a comment-only session configuration; explicit CLI roots
and supported settings replace the immutable build configuration. SDK F4/console
settings are unavailable in this mode. Stock requires VSync on: the current SDK's
off path changes guest vblank to 1000 Hz and is not an admitted stock capability.
The read-only Verify build / game action exposes exact build SHA256 before Play;
Play revalidates rather than trusting the earlier result.

## Native evidence

Built with the isolated SDK 0.20 installation, LLVM 20.1.8 and CMake 3.31.6 in
`out/build/m7-final-sdk20`. Shipping launcher defines neither synthetic admission
nor guest-free probe macros. Pure/native results on the final source:

| Gate | Checks passed |
| --- | ---: |
| Profile policy / library codec / preferences | 117 / 315 / 138 |
| Managed configuration policy | 50 |
| Filesystem / store / owned process | 72 / 107 / 41 |
| Build / game verification | 109 / 70 |
| Full-tree snapshot / legacy metadata | 21 / 72 |
| Native UI | 399 |
| Controller launch, rename, branch, restore and verify | 56 |
| Production legacy import | 29 |
| Allowlisted staging | 60 |
| Exact staged host, guest-free | 14 |

Six native symlink-creation cases were unavailable with Windows error1314 across
filesystem/store/build/game/snapshot suites; they are SKIP, not physical PASS.
Hardlink, ADS and supported alias/overlap rejection cases passed. Staging's
junction and ADS negatives passed with zero skips. Staging also passed60 checks
in Windows PowerShell5 on the earlier identical staging implementation.

Final controller fixture: TEMP `mcla-controller-test-693827f68b628aabe133632514a3a3eb`.
Final import fixture: TEMP `mcli-import-603381ab7bed4fd8bd309dc4dab7fab3`.
Final staging fixture: TEMP `MclaStagingTest-ca5c26c143da4d41b3a520fe4accbc90`.
All preserve synthetic evidence. Import's first failed fixture exposed forward
slashes following an extended Windows path prefix; preferred separators fixed
fixture setup, not production admission. Filesystem atomic replacement previously
failed because its own validation handle denied deletion; close-before-publication
fixed it under the retained cooperative lock. Process fixtures account for the
verified system conhost job member instead of assuming exactly one process.

Existing regressions: F10 admission8, image protocol119, real SDK audit-rearm1809,
fake image-helper client51, adapter policy78 with `config/mcla.toml.example`.
The adapter test without its required argument failed as expected; its configured
invocation passed. ast-grep0.45.0 scan and3 rule fixtures passed.

Hidden synthetic WM_PRINT layout evidence is
`private/evidence/m8-launcher-ui-20261010-3.png`:120DPI,1184x990,48 controls.
Labels/buttons/footer fit, but hidden native edit/combo text did not render in
this seam. Control values and dispatch passed native assertions; this image is
NOT full visual acceptance. Actual visible UI readability remains an owner check.
No desktop image, external input or game capture was used for this layout test.

## Exact private candidate

`private/l1/candidate-receipt.json` indexes16 artifacts plus its final receipt;
only one build subdirectory is present. There are no saves, game assets, local
settings, logs, diagnostics, secrets or PDBs in this allowlisted stage. This is
not approval to redistribute generated/proprietary code or ship to volunteers.

- Launcher: `5DFA4CD23EEC743FD3E342BC646CDB59DBAA5BF1F1FEE79BAB1F16CF5A724ABA`
- Host: `6E6CFFC5B69CA3FA3D37B9E7D7C120F5434F54680D95BD584D831A53755F42AF`
- Build manifest: `5C00F685A6BFA7474FA5902C244E886555534C33AB00ED9F92D2FDC8C44C2298`
- Game revision: `95DA270942B4BDDBC8DC5EB45AA742AC6BE191D74E430022070D9B9B5AF25048`

The exact candidate's production build/game verification and actual host lifecycle
probe passed14 checks using a fresh empty TEMP profile. Evidence:
`mcla-host-probe-ede669d98cc486b4574353283a48e53c`. It proved explicit routing,
sentinel immutability, guest-skipped/completed log markers, empty before/after
user trees and released ownership. It does not prove gameplay or real F10.

Internal headless verification remains available without launching a host:

```powershell
out/build/m7-final-sdk20/mcla_launcher_host_probe_win.exe --verify-only --build=C:\BDU\MCLA-Recomp\private\l1\build --game=C:\BDU\MCLA-Recomp\private\p1\game
```

## Review and remaining gate

Scoped architecture/privacy reviews found no blocking defect for a bounded
private owner test on a NEW imported branch, conditioned on the completed tests.
Primary reviewed integration, exact artifacts and returned findings. Several
reviewed primitives share their reviewer with their author; primary acceptance
and native negative tests remain necessary rather than claiming total independence.

Spark `qwen3.8-27b` supplied a routine seven-case test inventory, checked against
the exact source by primary. Receipt: `private/evidence/M8-launcher-spark-20261010-1`.
No model-generated mutation or unreviewed approval was accepted.

Next: [owner first-run instructions](../internal-launcher-owner-test.md). Required
physical evidence is original progress/name on the new branch, actual F10,
pause/audio/Alt+Enter, normal exit/reload and UI-close/game-lifetime behavior.
No task completion, commit, version bump, external transfer or M8 closure is
claimed before that gate. Deferred60 timing and baseline bug work remain separate.
