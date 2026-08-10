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

No build command is authoritative yet. Add commands here only after they pass in a fresh PowerShell session and are captured by the bootstrap workflow.

Update this document whenever a prerequisite, version, path-discovery rule, preset, environment variable, codegen command, or package command changes.
