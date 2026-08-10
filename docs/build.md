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

Still required or unverified:

- pinned ReXGlue v0.9.0 build/install
- deterministic source verification/extraction scripts
- pinned Xenia Canary baseline

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

Dot-sourcing is required because a child PowerShell process cannot modify its parent's environment. The eventual `bootstrap.ps1` will consume this resolver and validate the remaining project prerequisites.

No project build command is authoritative yet. Add commands here only after they pass in a fresh PowerShell session and are captured by the bootstrap workflow.

Update this document whenever a prerequisite, version, path-discovery rule, preset, environment variable, codegen command, or package command changes.
