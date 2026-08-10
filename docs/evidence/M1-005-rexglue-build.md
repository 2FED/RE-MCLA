# M1-005 ReXGlue Windows build evidence

Date: 2026-08-10

Source: `third_party/rexglue-sdk` at `v0.9.0` / `3eb9b511b4140d2769e27be63eae57d41bfa2afa`

Host toolchain:

- Visual Studio Build Tools 2022 17.14.37 developer environment
- CMake 3.31.6
- Ninja 1.12.1
- Clang/Clang++ 20.1.8
- Windows SDK 10.0.26200

The unmodified upstream `win-amd64` preset configured successfully in 109.6 seconds. Its summary reported ReXGlue 0.9.0, C++23, D3D12 enabled, Vulkan disabled, Tracy enabled, and performance counters enabled.

The upstream CI-equivalent `install` target completed 2,258 Debug, Release, and RelWithDebInfo build/install actions in 223.3 seconds. Output was written only below the SDK's ignored `out/` directory, and `git status --ignore-submodules=none` remained clean.

Installed SDK root:

```text
third_party/rexglue-sdk/out/install/win-amd64
```

Verification results:

- `bin/rexglue.exe --version` returned `0.9.0` with exit code 0.
- `bin/rexglue.exe --help` returned exit code 0 and listed `codegen`, `init`, and `recompile-tests`.
- `lib/cmake/rexglue/rexglueConfig.cmake` and per-configuration target files exist.
- Runtime, Xenos GPU, FFmpeg, SDL3, Tracy, and support libraries exist for all three configurations.
- Built `rexglue.exe` size: 3,947,008 bytes.
- Built `rexglue.exe` SHA-256: `C3B97527A2C7E69CBB3F26646FFF49ED4812C1C0284E01F8D0ED8A6042A6AD05`.

This executable hash is local build evidence, not a cross-machine reproducibility promise: debug information, timestamps, paths, and compiler details may affect native binary hashes.
