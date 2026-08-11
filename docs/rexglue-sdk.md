# ReXGlue SDK pin

Owner: MCLA-R maintainers

Purpose: record the immutable ReXGlue source and recursive dependency graph used by this project.

Update trigger: any intentional ReXGlue or nested submodule revision change.

## Project fork pin

- Repository: `https://github.com/2FED/rexglue-sdk.git`
- Upstream repository: `https://github.com/rexglue/rexglue-sdk.git`
- Upstream base tag: `v0.9.0`
- Upstream base commit: `3eb9b511b4140d2769e27be63eae57d41bfa2afa`
- Project release tag: `v0.9.0.9`
- Immutable project commit: `01f895afb1b084652915c0b463910d57049f06be`
- Fork branch: `mcla/mcla-r-hotfixes`
- Local path: `third_party/rexglue-sdk`

The fork carries the seven previously reviewed fix groups plus bounded D3D12 guest-presentation observability. `FLOAT16_4` packing accepts mask 3 like mask 2 while retaining the upstream `shift <= 2` bound, with direct PPC regression coverage. ReXApp converts host paths through `rex::path_to_utf8()` rather than locale-dependent `std::filesystem::path::string()`. It also destroys Runtime/input drivers before their attached Window, preventing a teardown use-after-free after a loaded-image probe. Read-only host VFS devices reject root traversal, write-intent opens, rename, and writable mappings instead of silently downgrading or reaching the host filesystem. The ten direct XONLINE/social/XHV imports selected by M3-007 return explicit offline states and emit once-only `[OFFLINE]` diagnostics instead of preserving stale caller `r3`. Guest threads retain Xenia's 10-ms compatibility grace period and use regression-tested raw/XAPI start plans, while the 50-MHz guest clock path has direct monotonicity coverage. Generated functions and import hooks maintain metadata-only breadcrumbs, and C++ guest exceptions crossing `XThread` emit bounded host-stack context without guest memory by default. The v0.9.0.9 delta assigns monotonic guest-output sequences, records successful guest-backed DXGI presents and actual success HRESULTs, holds mailbox ownership through capture readback, and accepts capture evidence only at or behind the published successful-present watermark without changing renderer control flow. See `docs/evidence/M2-016-rexglue-vector-regression.md`, `docs/evidence/M3-002-app-lifecycle.md`, `docs/evidence/M3-003-module-config.md`, `docs/evidence/M3-004-vfs-disc-root.md`, `docs/evidence/M3-007-offline-services.md`, `docs/evidence/M3-008-early-init.md`, and `docs/evidence/M3-009-crash-reporting.md`.

KI-012 tracks a separate upstream non-Release test-lifetime defect: profiled
`REX_HOOK` wrappers do not yet guard Tracy manual lifetime before
`Runtime::Setup`. The minimal filtered test raises `0xC0000005`; normal runtime
startup and the 12-test UI subset are unaffected. See
[rexglue/rexglue-sdk#410](https://github.com/rexglue/rexglue-sdk/issues/410).

## Reproduction

After cloning MCLA-R, initialize the pinned graph with:

```powershell
git submodule sync --recursive
git submodule update --init --recursive
```

Verify the root pin and recursive cleanliness with:

```powershell
git -C third_party/rexglue-sdk rev-parse HEAD
git -C third_party/rexglue-sdk rev-parse refs/tags/v0.9.0.9^{}
git submodule status --recursive
git -C third_party/rexglue-sdk status --short --ignore-submodules=none
```

The first two commands must return `01f895afb1b084652915c0b463910d57049f06be`. The upstream base remains available as `refs/tags/v0.9.0` at `3eb9b511b4140d2769e27be63eae57d41bfa2afa`. Every `git submodule status --recursive` line must begin with one space: `-` means uninitialized, `+` means a commit mismatch, and `U` means a merge conflict.

## Recursive SHA manifest

The nested dependency SHAs were captured from the clean upstream v0.9.0 checkout on 2026-08-10; the root line is the reviewed v0.9.0.9 project-fork commit:

```text
01f895afb1b084652915c0b463910d57049f06be third_party/rexglue-sdk
0604b464c7cb4ebc94940cf1f324a3b26b87717c third_party/rexglue-sdk/thirdparty/FFmpeg
88abf9bf325c798c33f54f6b9220ef885b267f4f third_party/rexglue-sdk/thirdparty/catch2
bfffd37e1f804ca4fae1caae106935791696b6a9 third_party/rexglue-sdk/thirdparty/cli11
407c905e45ad75fc29bf0f9bb7c5c2fd3475976f third_party/rexglue-sdk/thirdparty/fmt
f4f1d8a352ca1908943aea2ad8c54b39b4879080 third_party/rexglue-sdk/thirdparty/glslang
6d910d5487d11ca567b61c7824b0c78c569d62f0 third_party/rexglue-sdk/thirdparty/imgui
7d1b4600b68595085a949743331c2e5673f511ea third_party/rexglue-sdk/thirdparty/inja
305907723a4e7ab2018e58040059ffb5e77db837 third_party/rexglue-sdk/thirdparty/libmspack
388a73fd9007300e5130c5fe352d9ce3288b6dde third_party/rexglue-sdk/thirdparty/o1heap
8bf3b7215ad9fc3deb583c6a3a37c6c67f2e24e4 third_party/rexglue-sdk/thirdparty/sdl3
71fd833d9666141edcd1d3c109a80e228303d8d7 third_party/rexglue-sdk/thirdparty/simde
da8f73412998e4f1adf1100dc187533a51af77fd third_party/rexglue-sdk/thirdparty/simde/test/munit
6af9287fbdb913f0794d0148c6aa43b58e63c8e3 third_party/rexglue-sdk/thirdparty/snappy
d572f4777349d43653b21d6c2fc63020ab326db2 third_party/rexglue-sdk/thirdparty/snappy/third_party/benchmark
b796f7d44681514f58a683a3a71ff17c94edb0c1 third_party/rexglue-sdk/thirdparty/snappy/third_party/googletest
79524ddd08a4ec981b7fea76afd08ee05f83755d third_party/rexglue-sdk/thirdparty/spdlog
04f10f650d514df88b76d25e83db360142c7b174 third_party/rexglue-sdk/thirdparty/spirv-headers
04d0b166dcd62e29509bf2aac3ca0c5ccdcb6929 third_party/rexglue-sdk/thirdparty/spirv-tools
30172438cee64926dc41fdd9c11fb3ba5b2ba9de third_party/rexglue-sdk/thirdparty/tomlplusplus
05cceee0df3b8d7c6fa87e9638af311dbabc63cb third_party/rexglue-sdk/thirdparty/tracy
63d64de49fd6b829f7c8694df5ab2ee625cb7134 third_party/rexglue-sdk/thirdparty/utfcpp
0b17a763ba5643e32da1b2152f8140461b3b7345 third_party/rexglue-sdk/thirdparty/volk
49f1a381e2aec33ef32adf4a377b5a39ec016ec4 third_party/rexglue-sdk/thirdparty/vulkan-headers
1d8f600fd424278486eade7ed3e877c99f0846b1 third_party/rexglue-sdk/thirdparty/vulkan-memory-allocator
e626a72bc2321cd320e953a0ccf1584cad60f363 third_party/rexglue-sdk/thirdparty/xxHash
```
