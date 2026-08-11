# Build and environment

Owner: MCLA-R maintainers

Purpose: provide reproducible setup, codegen, build, test, and package commands without relying on undocumented machine state.

Current verified host components:

- Visual Studio Build Tools 2022 17.14
- MSVC 14.44
- Windows 11 SDK
- LLVM/clang-cl/lld 20.1.8 at `C:\Program Files\LLVM\bin`
- CMake 3.31.6 from Visual Studio
- Ninja 1.12.1 from Visual Studio
- ast-grep 0.45.0
- Eclipse Temurin JDK 21.0.12+8
- Ghidra 12.0.4 with XEXLoaderWV 13.0.0
- RenderDoc 1.45.0

Still required or unverified:

- aggregate prerequisite validation through `scripts/bootstrap.ps1`

## Deterministic toolchain discovery

Run the resolver from any PowerShell working directory:

```powershell
& C:\BDU\MCLA-Recomp\scripts\Resolve-Toolchain.ps1
```

It uses the installed `vswhere.exe` to locate Visual Studio, resolves the bundled CMake and Ninja executables, resolves the standalone LLVM installation, and rejects unsupported versions. It does not install software or persist environment changes.

To prepend the verified tool directories to `PATH` for the current PowerShell session, dot-source it:

```powershell
. C:\BDU\MCLA-Recomp\scripts\Resolve-Toolchain.ps1 -ExportPath
cmake --version
ninja --version
clang-cl --version
```

Dot-sourcing is required because a child PowerShell process cannot modify its parent's environment. `bootstrap.ps1` consumes this resolver inside its own fresh process and validates the remaining project prerequisites.

## ReXGlue SDK source and dependencies

MCLA-R tracks ReXGlue as a Git submodule. The authoritative source tag, commit, and complete recursive SHA manifest are in `docs/rexglue-sdk.md`. Initialize exactly that graph after cloning:

```powershell
git submodule sync --recursive
git submodule update --init --recursive
git submodule status --recursive
```

The first status line must contain ReXGlue project-fork commit `efac376998cbb0520295d308be4703574a12a995`; no line may begin with `-`, `+`, or `U`.

The verified v0.9.0.7 Windows build uses:

- ReXGlue 0.9.0.7 project fork, based on upstream v0.9.0, with the exact nested dependencies in `docs/rexglue-sdk.md`
- Visual Studio Build Tools 2022 17.14.37 and Windows SDK 10.0.26200
- Clang/Clang++ 20.1.8 in GNU-compatible driver mode
- CMake 3.31.6 and Ninja 1.12.1
- upstream `win-amd64` multi-config preset
- D3D12 enabled and Vulkan disabled
- C++23 and `-march=x86-64-v3`

## Build and install ReXGlue

Run these commands from the MCLA-R repository root in a new PowerShell session. They initialize the Visual Studio compiler/SDK environment without requiring an interactive Developer Prompt:

```powershell
$RepoRoot = (git rev-parse --show-toplevel).Trim()
$Toolchain = . (Join-Path $RepoRoot 'scripts\Resolve-Toolchain.ps1') -ExportPath
$LaunchVs = Join-Path $Toolchain.VisualStudioRoot 'Common7\Tools\Launch-VsDevShell.ps1'
& $LaunchVs -Arch amd64 -HostArch amd64 -SkipAutomaticLocation

$SdkSource = Join-Path $RepoRoot 'third_party\rexglue-sdk'
Push-Location $SdkSource
cmake --preset win-amd64
cmake --build out/build/win-amd64 --target install --parallel
Pop-Location
```

The install target builds Debug, Release, and RelWithDebInfo because the upstream preset sets `CMAKE_DEFAULT_CONFIGS=all`. It writes generated files only below the submodule's ignored `out/` directory. Do not edit or commit generated SDK files.

To rebuild without reconfiguring:

```powershell
cmake --build (Join-Path $SdkSource 'out\build\win-amd64') --target install --parallel
```

## Discover the local ReXGlue install

Use explicit session-local paths instead of relying on CMake's per-user package registry:

```powershell
$ReXGlueInstall = Join-Path $SdkSource 'out\install\win-amd64'
$ReXGlueBin = Join-Path $ReXGlueInstall 'bin'

$env:PATH = "$ReXGlueBin;$env:PATH"
if ([string]::IsNullOrEmpty($env:CMAKE_PREFIX_PATH)) {
    $env:CMAKE_PREFIX_PATH = $ReXGlueInstall
} else {
    $env:CMAKE_PREFIX_PATH = "$ReXGlueInstall;$env:CMAKE_PREFIX_PATH"
}

rexglue --version
Test-Path (Join-Path $ReXGlueInstall 'lib\cmake\rexglue\rexglueConfig.cmake')
```

Expected output is ReXGlue `0.9.0` and `True`. The application presets now exist; during M2 feasibility work they intentionally consume this session-local `CMAKE_PREFIX_PATH` so the configured SDK is explicit.

The first clean build evidence is in `docs/evidence/M1-005-rexglue-build.md`. ReXGlue unit tests are not enabled by the stock preset and are not claimed by M1-005/M1-006; SDK-changing work must configure `REXGLUE_BUILD_TESTS=ON` and run the matching CTest preset.

## Verify the supported source image

Before listing, extracting, or analyzing game content, run:

```powershell
& (Join-Path $RepoRoot 'scripts\verify-source.ps1') `
  -IsoPath 'C:\BDU\MCLA-Recomp\Midnight.Club.Los.Angeles.The.Complete.Edition.XBOX360\midmets4.iso'
```

The command returns one object with `Valid=True` only when the ISO size/hash, XDVDFS structure, embedded `default.xex` size/hash, Title ID, and Media ID all match the supported Complete Edition dump. Any mismatch throws and returns a nonzero process result when invoked from automation. Do not continue to extraction after an error.

## Extract the verified game payload

The wrapper performs verification, containment checks, staged extraction, and a no-overwrite final move:

```powershell
& (Join-Path $RepoRoot 'scripts\extract-game.ps1') `
  -IsoPath 'C:\BDU\MCLA-Recomp\Midnight.Club.Los.Angeles.The.Complete.Edition.XBOX360\midmets4.iso' `
  -DestinationPath 'private\game'
```

Use `-WhatIf` for a full tool/source/path preflight without writing extracted data. The final destination must not already exist. To regenerate it, deliberately move or remove the old private directory outside this wrapper and rerun; the script never overwrites an existing payload.

Validate the extracted inventory against the ignored local manifest:

```powershell
& (Join-Path $RepoRoot 'scripts\verify-game-manifest.ps1') -VerifyHashes
```

Without `-VerifyHashes`, the validator checks schema/source identity, safe relative paths, exact inventory, every file size, total bytes, and the required 4 RPF/6 BIK counts. Use `-VerifyHashes` for milestone gates and after any suspected corruption; it reads and hashes all 6.57 GB of extracted payload.

## Xenia Canary baseline

The pinned behavioral-reference emulator is installed at:

```text
private/tools/xenia-canary/artifacts/xenia_canary.exe
```

Its immutable release/source/hash record and no-game startup result are in `docs/evidence/M1-013-xenia-canary.md`. Do not replace it from a moving “latest” URL. Baseline captures must include the logged `Build:` line and keep logs, screenshots, GPU traces, saves, and caches below ignored `private/` paths.

## Ghidra, XEXLoaderWV, and RenderDoc

The verified reverse-engineering pair is Ghidra 12.0.4 plus SaveEditors XEXLoaderWV 13.0.0 and Temurin JDK 21.0.12+8. XEXLoaderWV 13.0.0 is built specifically for this Ghidra/JDK combination; do not upgrade Ghidra independently. The local Ghidra install, extension, user profile, projects, and logs live under ignored `private/tools/ghidra/`.

Run Ghidra headlessly with private user directories so projects and caches stay inside the repository's ignored boundary:

```powershell
$GhidraBase = Join-Path $RepoRoot 'private\tools\ghidra'
$env:JAVA_HOME = 'C:\Program Files\Eclipse Adoptium\jdk-21.0.12.8-hotspot'
$env:USERPROFILE = Join-Path $GhidraBase 'user'
$env:APPDATA = Join-Path $env:USERPROFILE 'AppData\Roaming'
$env:LOCALAPPDATA = Join-Path $env:USERPROFILE 'AppData\Local'

& (Join-Path $GhidraBase 'install\ghidra_12.0.4_PUBLIC\support\analyzeHeadless.bat')
```

The pinned extension passed a real headless import of `private/game/default.xex`; sanitized evidence is in `docs/evidence/M1-014-re-tools.md`. Do not publish Ghidra projects, decrypted program data, XEX bytes, caches, or analysis exports without a separate source-data review.

RenderDoc 1.45.0 is installed at `C:\Program Files\RenderDoc`. Verify it without starting a GUI:

```powershell
& 'C:\Program Files\RenderDoc\renderdoccmd.exe' --version
```

Store future `.rdc` captures under ignored `private/`. A tool upgrade requires recording new versions/hashes and repeating the representative-target smoke test.

## One-command prerequisite gate

From any working directory, run the complete read-only gate in a fresh PowerShell process:

```powershell
powershell.exe -NoLogo -NoProfile -ExecutionPolicy Bypass `
  -File C:\BDU\MCLA-Recomp\scripts\bootstrap.ps1
```

The default run validates all pinned build, source, extraction, emulator, reverse-engineering, and graphics-debug prerequisites. It hashes the complete ISO and extracted payload, so it can take tens of seconds and perform substantial read I/O. It never installs or repairs a missing component. Every check is reported, and any failure produces a nonzero exit code after the remaining checks have run.

Use the named path parameters only when the private layout differs from the documented defaults or when testing a failure. Overrides do not disable version or hash validation. The verified positive and missing-tool transcripts are summarized in `docs/evidence/M1-015-bootstrap.md`.

## Generate, verify, and build the MCLA application

The presets pin the installed project-fork SDK at ReXGlue `0.9.0.7`. After successful non-force codegen, verify the ignored corpus, configure, build, and verify the Release integration with:

```powershell
scripts\verify-generated-integration.ps1
cmake --preset win-amd64-release
cmake --build --preset win-amd64-release --target mcla --parallel
scripts\verify-generated-integration.ps1 -BuildRoot out\build\win-amd64-release
```

With the accepted M3-013 corpus present, the configured Ninja graph exposes `mcla` and `mcla_codegen`; the native target must consume exactly 65 generated C++ sources. If `generated/default` is absent, configuration succeeds in codegen-only mode: `mcla_codegen` remains available and `mcla` is intentionally omitted. Run codegen and configure again. A mismatched `REXSDK_VERSION` is rejected.

`generated/rexglue.cmake` is the sole tracked file below `generated/`. The current 67 generated files, all object files, and the resulting executable remain ignored because they contain or embed translated proprietary game code. M3-001 preserves the first clean Release baseline; M3-009 records the current 65-source instrumented corpus in `docs/evidence/M3-009-crash-reporting.md`.

Verify the privacy-safe crash-report pipeline with:

```powershell
scripts\verify-crash-report-contract.ps1
scripts\test-crash-report-contract.ps1
scripts\test-crash-report.ps1
scripts\run-crash-report-smoke.ps1
```

The synthetic probe loads the verified XEX but skips guest execution. Its report
contains guest addresses, thread/import identifiers, and at most 16 host frames;
it excludes guest memory, registers, stack data, and absolute host paths.

Verify independent structured logging filters without constructing Runtime:

```powershell
scripts\verify-logging-contract.ps1
scripts\test-logging-contract.ps1
scripts\test-logging-schema.ps1
scripts\run-logging-smoke.ps1
```

Each `mcla_log_<category>` init-only option accepts `inherit`, `trace`, `debug`,
`info`, `warn`, `error`, `critical`, or `off`; `inherit` is the default. The live
gate sets the global level to `off`, enables one category per isolated run, and
writes only ignored logs below `private/evidence/M3-010/`.

Re-evaluate the conditional skip-intro decision with an unpatched bounded run:

```powershell
scripts\verify-skip-intro-decision.ps1
scripts\test-intro-blocker-trace.ps1
scripts\run-intro-blocker-smoke.ps1
```

The current accepted classification is `gpu-plugin-unconfigured-before-bink`.
The wrapper observes for 15 seconds, requires module launch plus both no-GPU
markers, rejects post-launch Bink/fatal/crash evidence, kills the isolated
process, and confirms cleanup. It never enables or writes a guest patch.

Build and verify the complete Windows AMD64 configuration matrix with:

```powershell
scripts\test-build-matrix.ps1
scripts\run-build-matrix.ps1
```

The runner configures the exact Debug, RelWithDebInfo, and Release presets and
performs a clean `mcla` build for each. Before each build it removes only the
known copied runtime, Tracy, and Xenos DLL names from that preset's contained
build root, preventing stale cross-configuration artifacts from satisfying the
gate. Every accepted configuration must compile all 65 generated C++ objects,
produce a PE executable, and stage only its matching `rexruntime`,
`TracyClient`, and `rexgpu-xenos` DLL variant. Raw logs and the hash manifest
remain ignored below `private/evidence/M3-012/`.

Verify the post-GPU startup boundary without an explicit `gpu_plugin` argument:

```powershell
scripts\verify-analysis-config.ps1
scripts\test-startup-trap-trace.ps1
scripts\run-startup-trap-smoke.ps1
```

Normal MCLA launches select `xenos` by project default. The runner observes the
RelWithDebInfo process for 20 seconds, requires project selection, plugin load,
module launch, graphics interrupt/pipeline, and audio callback markers, then
performs bounded cleanup. It rejects a no-GPU route, fatal or unregistered
guest target, `PPC_UNIMPLEMENTED`, guest crash, and any post-launch Bink evidence.
Guest-free lifecycle/config/VFS/crash/logging probes do not select a GPU.

Run the canonical fail-closed startup smoke:

```powershell
scripts\test-startup-smoke.ps1
scripts\run-startup-smoke.ps1
scripts\verify-startup-smoke-result.ps1 -ResultPath <private-result.json> -RuntimeLogPath <private-log>
```

The runner first requires the supported 9,252,864-byte Complete Edition
`default.xex` and exact SHA-256. It then gives the RelWithDebInfo process at
most 20 seconds to emit 15 ordered lifecycle, Xenos, static/loaded image
identity, read-only VFS, module-launch, graphics, and audio markers. Success is
marker-driven; the harness terminates only its own PID, requires its process
handle to signal exit within a separate five-second cleanup deadline, then
re-verifies the immutable final log and exact result byte/hash fields.
Private logs/results remain below `private/evidence/M3-014/`.

Run the final repeated-process and integrity gate with:

```powershell
scripts\test-launch-exit-cycles.ps1
scripts\run-launch-exit-cycles.ps1
scripts\verify-launch-exit-cycles.ps1 -ResultPath <private-result.json>
```

The canonical command performs its own `--clean-first` RelWithDebInfo build;
do not use `-SkipCleanBuild` or a reduced cycle count for acceptance. It runs
all ten crash probes first so the first invocation is immediately post-relink,
then runs ten consecutive normal windows through controlled `WM_CLOSE`. Every
invocation receives unique ignored user/cache roots. The gate re-hashes all
completed cycle trees after later runs, all four staged binaries before/after,
and the complete 15-file private game manifest before/after.

Verify the M3 early-initialization contract and repeat the bounded runtime ordering trace with:

```powershell
scripts\verify-early-init-contract.ps1
scripts\test-early-init-contract.ps1
scripts\test-early-init-trace.ps1
scripts\run-early-init-smoke.ps1 -RunCount 3 -TimeoutSeconds 30
```

The live probe uses CDB, creates isolated ignored user/cache roots, records only event markers and hashes, and stops at the first child guest-thread start plan after all reviewed timing/thread imports have appeared.

Exercise only the project-owned host lifecycle, without constructing the guest
runtime or launching translated code, with:

```powershell
scripts\run-app-lifecycle-smoke.ps1
```

The wrapper launches the ignored Release executable with
`--mcla_lifecycle_probe`, requires a clean exit within 15 seconds, verifies four
ordered lifecycle markers, and stores the raw log under ignored
`private/evidence/M3-002/`. This is a host startup smoke test, not a module-boot
or gameplay claim.

Validate the accepted guest image/dispatch metadata and exercise a loaded-XEX
probe that stops before guest-thread creation with:

```powershell
scripts\verify-module-config.ps1
scripts\run-module-config-smoke.ps1
```

The default probe reads `private/game/default.xex`, redirects user/cache output
to an ignored per-run evidence directory, requires five ordered contract and
shutdown markers, and rejects any log indicating guest execution.

Validate the mounted disc aliases and fail-closed write policy with:

```powershell
scripts\test-vfs-policy.ps1
scripts\run-vfs-smoke.ps1
scripts\run-vfs-smoke.ps1 -BuildRoot out\build\win-amd64-relwithdebinfo
```

The smoke requires the exact 15-file private game inventory, snapshots metadata
and the XEX hash before and after the process, redirects all approved writes to
ignored evidence roots, and fails if a probe file appears in `private/game`.

The immutable M2-008 first non-force gate was run through `scripts\run-rexglue-codegen-gate.ps1`. It completed all six analysis phases and returned exit code 1 for seven validation-stage `UnresolvedCall` findings without emitting `generated/default`. Its raw streams remain ignored; regenerate the reviewed public evidence with `scripts\export-rexglue-codegen-gate.ps1`. Do not remove or overwrite the private first-run root, rerun this one-shot gate, add `--force` to it, or use the M2-008 root for the separate M2-009 force inventory.

M2-009 then ran the separate global-force command `rexglue --force codegen mcla_manifest.toml` through `scripts\run-rexglue-force-inventory.ps1`. It returned exit code 0 and emitted 64 ignored files (128,010,691 bytes) below `generated/default`; every output has a private size/SHA-256 manifest. Regenerate the sanitized inventory with `scripts\export-rexglue-force-inventory.ps1`. Never stage these generated files or reuse/overwrite either immutable evidence root.

Update this document whenever a prerequisite, version, path-discovery rule, preset, environment variable, codegen command, or package command changes.
