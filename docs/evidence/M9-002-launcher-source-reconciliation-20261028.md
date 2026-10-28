# M9-002 launcher source reconciliation (2026-10-28)

**Status:** the repository now builds the launcher the owner uses (l12). The
rebuilt `MCLA-Launcher.exe` has byte-identical code to the l12 binary. Staged
as `private/l35-s30` and `private/l35-sunc`. Pending: the owner's smoke on a
non-MAIN profile.

## Where the l12 launcher came from

- The l12 evidence records the controller source hash `4EAB5B2D...`
  ([KI-034 repair](M8-profile-support-repair-20261012.md)). No commit contains
  that file. Commit `2c9e0ac` committed only the KI-034 hunks; its controller
  hashes to `868D6109...`.
- The main checkout's uncommitted working tree has exactly `4EAB5B2D...`, and
  every launcher and export source there was last written before the l12 build
  (controller 2026-10-12 08:27, binary 08:32). So the l12 launcher was built
  from that working tree. It contains work that was never committed:
  - the diagnostic export dialog (M8-018 preview / confirm / save, 6 sources);
  - matching controller and UI changes (export actions, the compatibility,
    accessibility and cheats summary fields);
  - 3 export test suites and updated controller and UI tests;
  - CMake: export sources in the launcher target, the standalone
    `MCLA-Export-Metadata` tool and the test targets.
- The other launcher sources in that tree already equal the committed ones.
  The tree also carries many private probe changes (autostart, clock, arcade,
  render probes). They are not launcher code and were **not** brought over.

## What changed in the repository

Copied unchanged from the main working tree: `src/mcla_launcher_controller_win.cpp`,
`src/mcla_launcher_ui_win.{h,cpp}`, `src/mcla_diagnostic_export_*` (6 files),
`tests/mcla_launcher_{controller_win,ui_win,ui_render_win}_test`-family files and
`tests/mcla_diagnostic_export_*_test.cpp` (3). `CMakeLists.txt` gets only the
launcher and export hunks. The main checkout itself was not modified.

## Build and tests

Built from the `private/l34-src` clone with the M9-001 SDK install, Release,
`MCLA_BUILD_DIAGNOSTIC_TESTS=ON` (`private/l35-launcher-build`).

| Suite | Checks | Failures | Skips |
|---|---|---|---|
| Launcher controller | 335 | 0 | 0 |
| Native UI | 1065 | 0 | 0 |
| Preferences | 1064 | 0 | 0 |
| Build lease policy / runtime | 1417 / 47 | 0 | 0 |
| Diagnostic export policy / native / UI | 505 / 105 / 249 | 0 | 0 / 1 / 0 |
| Capabilities | 168 | 0 | 0 |
| Library codec / profile policy / import policy | 315 / 117 / 72 | 0 | 0 |
| Store / filesystem / snapshot | 125 / 76 / 26 | 0 | 1 / 0 / 0 |
| Build / game verifier, owned process, production import | 109 / 70 / 41 / 31 | 0 | 0 |
| Staging script (`Test-MclaInternalLauncherStaging.ps1`) | 63 | 0 | 0 |

The controller, UI, preferences, capabilities, export policy, import, store,
filesystem, snapshot and staging counts equal the l12 acceptance record. The
two skips are symlink fixtures (Windows privilege), as before. ast-grep scan
and rule tests pass.

## Binary comparison with l12

Both `MCLA-Launcher.exe` files are 1,202,176 bytes. Of those bytes, 324 differ:

- 3 in the PE header (link timestamp);
- 145 in `.rdata`, all import hint numbers (for example the hint of
  `CreateWindowExW`). Hints come from the Windows SDK import libraries; the
  loader still binds by name;
- 176 in `.data`, a repeated 7-character tag (`@<AhB<A` became `<jiC?@g`) in
  anonymous-namespace RTTI names. The compiler derives it from the source
  path, which changed;
- **0 in `.text`: the code is identical.**

l12 `DB0D84A5...`, rebuilt `AE8B7D46AC3EE714...`.

## Staging

`private/l35-s30` and `private/l35-sunc` are l34 inputs with the rebuilt
launcher. `mcla.exe` and its tickets are unchanged (A50E30C6... / 766899067E...),
so the tickets were copied, not renewed (as for l12).

## Spark routing

Not used: provenance was established by exact hashes and byte comparison,
which a model answer would not make cheaper to verify.

## Open

1. Owner smoke on a non-MAIN profile through `private/l35-s30` (profiles,
   Setup, readiness, F10, lock). MAIN still works with the l12 launcher.
2. The main checkout's uncommitted probe work stays where it is; a separate
   owner decision is needed before cleaning that tree.
