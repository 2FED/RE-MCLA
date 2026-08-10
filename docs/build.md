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

The first status line must contain ReXGlue commit `3eb9b511b4140d2769e27be63eae57d41bfa2afa`; no line may begin with `-`, `+`, or `U`.

The verified v0.9.0 Windows build uses:

- ReXGlue 0.9.0 with the exact nested dependencies in `docs/rexglue-sdk.md`
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

## Configure the MCLA application scaffold

After initializing the compiler environment and `CMAKE_PREFIX_PATH` as above, configure the canonical scaffold with:

```powershell
cmake --preset win-amd64-debug
```

The configured Ninja graph must expose both `mcla` and `mcla_codegen`. Do not build `mcla` before codegen: its required `generated/default/mcla_init.h` and guest-derived sources are intentionally absent until the M2 analysis gate. `generated/rexglue.cmake` is the sole tracked file below `generated/`; all codegen output remains ignored.

The immutable M2-008 first non-force gate was run through `scripts\run-rexglue-codegen-gate.ps1`. It completed all six analysis phases and returned exit code 1 for seven validation-stage `UnresolvedCall` findings without emitting `generated/default`. Its raw streams remain ignored; regenerate the reviewed public evidence with `scripts\export-rexglue-codegen-gate.ps1`. Do not remove or overwrite the private first-run root, rerun this one-shot gate, add `--force` to it, or use the M2-008 root for the separate M2-009 force inventory.

Update this document whenever a prerequisite, version, path-discovery rule, preset, environment variable, codegen command, or package command changes.
