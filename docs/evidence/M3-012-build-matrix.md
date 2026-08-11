# M3-012 native build matrix

Date: 2026-08-11

Status: accepted

## Scope

M3-012 requires clean native Debug, RelWithDebInfo, and Release builds from the
exact project presets. It also owns the build-time half of KI-011: the intended
Xenos GPU plugin must be present beside every executable before M3-013 selects
and exercises it at runtime.

`CMakeLists.txt` now calls:

```cmake
rexglue_setup_target(mcla GPU_PLUGINS xenos)
```

The installed ReXGlue v0.9.0.7 helper resolves `rex::gpu-xenos` and stages the
configuration-specific DLL without linking the runtime-loaded plugin into the
host.

## Configuration-drift finding

The first clean Debug attempt compiled all 70 object inputs and then failed to
link `CaptureGuestCrashReport` and `FormatGuestCrashReport`. The installed
headers and RelWithDebInfo runtime were already from fork v0.9.0.7, while the
installed Debug and Release runtime binaries predated the M3-009 crash-report
API. This was a stale local SDK install, not a new upstream source defect.

The exact fork source was rebuilt for Debug and Release with the existing
`win-amd64` multi-config graph; `rexruntime` and `rexgpu-xenos` were installed
for both configurations. No SDK source changed. The repeated Debug link then
succeeded, and the final acceptance run rebuilt all project configurations
from clean object state.

Installing the previously stale Release tree also temporarily replaced the
installed CLI with its old 0.9.0.4 binary. The final bootstrap caught the exact
version mismatch. Rebuilding the Release `rexglue` target from the same pinned
source and reinstalling it restored `rexglue --version` to 0.9.0.7; the repeated
bootstrap then passed 12/12. This second finding is likewise local artifact
drift and did not require or justify an upstream issue.

## Final clean matrix

Private run: `20260811-144823-9604051d`.

| Configuration | Clean build | Generated objects | Time | Executable SHA-256 | Staged configuration DLLs |
|---|---:|---:|---:|---|---|
| Debug | exit 0, 71 actions | 65 | 33,115 ms | `989C7CC6DE457D19C8FFBC2907B56651A59FC364276A2C9DD7AF84468439DC9F` | `rexruntimed.dll`, `TracyClientd.dll`, `rexgpu-xenosd.dll` |
| RelWithDebInfo | exit 0, 71 actions | 65 | 57,202 ms | `F2F74DD08674D7174A18379B13032307387CBF3DF5C0A9313BA64E5FCA11A76D` | `rexruntimerd.dll`, `TracyClientrd.dll`, `rexgpu-xenosrd.dll` |
| Release | exit 0, 71 actions | 65 | 48,551 ms | `74A5F4ECBCD5C2284699B1082EB92655F958D37C2E407949C5FDA37AFB920043` | `rexruntime.dll`, `TracyClient.dll`, `rexgpu-xenos.dll` |

The runner deletes only nine exact copied-artifact names from each contained
build root before configuration. The verifier then rejects any other known
configuration variant left beside the executable, verifies all four artifact
hashes per row, checks the `MZ` header, and recounts generated object files.

## Gates

- build-result fixture: one positive and six negative cases;
- live clean matrix: 3/3 configurations and 195/195 generated objects;
- exact SDK version: 0.9.0.7;
- Xenos staging: correct configuration variant in all three build roots;
- generated integration: accepted corpus/hash/list/ignore contract in all three
  build roots;
- complete project PowerShell suite: 26/26 scripts passed;
- ast-grep scan: clean; rule tests: 3/3 passed;
- prerequisite/bootstrap audit: 12/12 checks passed, including installed CLI
  version 0.9.0.7;
- no proprietary generated source or build output is tracked.

## Re-evaluation

M3-012 removes compile/link configuration drift and the missing plugin-binary
risk. It does not claim that the plugin has loaded or initialized graphics.
M3-013 owns explicit runtime selection, the next deterministic startup boundary,
and any reachable `PPC_UNIMPLEMENTED` trap. KI-011 therefore remains open with a
narrower runtime-configuration scope.
