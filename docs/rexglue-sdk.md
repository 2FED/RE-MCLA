# ReXGlue SDK pin

Owner: MCLA-R maintainers

Purpose: record the immutable ReXGlue source and recursive dependency graph used by this project.

Update trigger: any intentional ReXGlue or nested submodule revision change.

## Project fork pin

- Repository: `https://github.com/2FED/rexglue-sdk.git`
- Upstream repository: `https://github.com/rexglue/rexglue-sdk.git`
- Upstream base tag: `v0.9.0`
- Upstream base commit: `3eb9b511b4140d2769e27be63eae57d41bfa2afa`
- Project release tag: `v0.9.0.28`
- Immutable project commit: `6354bbe2150c7ce06bee5ffe399f17a94c948616`
- Fork branch: `mcla/mcla-r-hotfixes`
- Local path: `third_party/rexglue-sdk`

The fork carries the previously reviewed fix groups plus bounded D3D12 guest-presentation and render-path observability. `FLOAT16_4` packing accepts mask 3 like mask 2 while retaining the upstream `shift <= 2` bound, with direct PPC regression coverage. ReXApp converts UTF-8 path CVars and logging filenames to native filesystem paths and safely opens non-ASCII Windows paths. It also destroys Runtime/input drivers before their attached Window, preventing a teardown use-after-free after a loaded-image probe. Read-only host VFS devices reject root traversal, write-intent opens, rename, and writable mappings instead of silently downgrading or reaching the host filesystem. The ten direct XONLINE/social/XHV imports selected by M3-007 return explicit offline states and emit once-only `[OFFLINE]` diagnostics instead of preserving stale caller `r3`. Guest threads retain Xenia's 10-ms compatibility grace period and use regression-tested raw/XAPI start plans, while the 50-MHz guest clock path has direct monotonicity coverage. Generated functions and import hooks maintain metadata-only breadcrumbs, and C++ guest exceptions crossing `XThread` emit bounded host-stack context without guest memory by default. The v0.9.0.9 delta assigns monotonic guest-output sequences, records successful guest-backed DXGI presents and actual success HRESULTs, holds mailbox ownership through capture readback, and accepts capture evidence only at or behind the published successful-present watermark. The v0.9.0.10 delta makes Tracy zones safe before manual profiler startup and adds an init-only privacy-safe Xenos render audit. The v0.9.0.11 delta adds Xenia's cooperative sign-in-state yield, deterministic local-profile tests, and bounded privacy-safe XAM identity/profile telemetry with distinct absent-slot masks. The v0.9.0.12 delta adds deterministic first-controller slot-zero selection and a bounded privacy-safe SDL-to-XAM input audit. The v0.9.0.13 delta adds full controller-matrix auditing, real disconnect/focus/rumble semantics, and an explicit MCLA-only critical-section compatibility option. The v0.9.0.14 delta adds bounded privacy-safe XMA/XAudio/SDL route and buffer-health auditing. The v0.9.0.15 delta makes unsupported XMP title playback fail explicitly while preserving idle state, guards empty playlist navigation/output capture, and adds bounded race-free XMP route evidence. The v0.9.0.16 delta classifies the MCLA XLiveBase message inventory into deterministic local-offline/unavailable results and adds a bounded init-only offline-service/socket audit. The v0.9.0.17 delta returns the configured Xbox language consistently, adds bounded locale telemetry, and makes all host path/log CVar conversions Unicode-safe. The v0.9.0.18 delta resolves mounted VFS roots without stripping the trailing-separator boundary before relative-path extraction and adds direct content-symlink-root coverage. The v0.9.0.19 delta adds nonvirtual, thread-safe presenter diagnostics for the latest active guest-output sequence and the guest-vblank count, without changing the graphics plugin ABI or renderer behavior. See the M2/M3 evidence listed below plus the complete M4 evidence through `docs/evidence/M4-closure-review.md`.

The v0.9.0.20 delta adds ordered, bounded, privacy-safe SDL device-output
windows for music, ambient, voice, engine, collision, and UI listening
evidence. It records only counts and normalized peaks, never PCM or
asset/device identity.

The v0.9.0.21 delta makes saved-content creation fail closed when its durable
header cannot be written, removes a partial header on write/close failure, and
rolls back the newly mounted package. Focused tests verify that saved-game
metadata survives reconstruction of the content manager and that truncated
headers are rejected.

The v0.9.0.22 delta adds an InitOnly, default-preserving guest sign-in-state
selector. State `1` remains the default local profile; an explicit state `2`
provides title-compatible offline save permission for MCLA and emits a bounded
configuration marker. Invalid values fail closed to state `1`. This does not
implement or claim Xbox Live or any network service.

The v0.9.0.23 delta makes saved-game replacement transactional across process
interruption. A marker and package/header backups restore the prior complete
save after an interrupted overwrite, while interrupted brand-new saves remain
unloadable and are removed. Recovery is restart-idempotent, enumeration cannot
roll back an active in-process write, malformed metadata is omitted, and an
InitOnly dummy-HDD free-space boundary returns disk-full before create,
overwrite, or truncate mutation. Focused coverage is 10 cases / 117 assertions.

The v0.9.0.24 delta persists known standard XAM scalar and Unicode profile
settings beneath the local user's global profile root. Files carry a fixed
magic plus setting-id/type/size metadata, enforce setting-key bounds, and use
temporary/backup recovery. Guest profile writes validate and materialize the
entire batch before mutation, reject unknown, mismatched, duplicate, null, and
oversized entries, and complete overlapped calls consistently. Existing
title-specific binary slots stay in their per-title root. Focused profile
coverage is 7 cases / 40 assertions; the generic CVar restart suite remains 26
cases / 128 assertions.

The v0.9.0.25 delta adds physical SDL pause/resume and default-playback-device
migration support to the audio driver, plus bounded privacy-safe recovery
telemetry. The title probe requires two ordered pause/resume recoveries and a
machine-observed default-endpoint migration with resumed nonzero device output;
it records no endpoint identity. Focused audio coverage is 10 cases / 42
assertions. XMP remains a state-correct metadata-only fallback with no decoder
or system-music playback claim.

The v0.9.0.26 delta replaces three return-bearing XAM stubs reached by MCLA
with explicit offline semantics: achievements guide UI fails as not signed in,
voice packet submission fails as busy, and leaderboard creation returns a
bounded valid empty enumerator. A six-entry policy matrix keeps achievements
and Driving Test progress local, treats presence, Rate My Ride, and voice as
unavailable, and never fabricates leaderboard rows, Live identity, or unlocks.
Focused coverage is 15 cases / 107 assertions.

The v0.9.0.27 delta adds restart-safe host audio configuration. Playback uses
the operating-system default endpoint unless `audio_device` names an exact SDL
playback device; an unavailable requested device fails initialization rather
than silently changing output. `audio_volume` is bounded to 0.0–1.0 and scales
converted host PCM without changing guest audio buffers. Focused coverage is
3 cases / 27 assertions.

The v0.9.0.28 delta adds default-off, InitOnly, privacy-safe performance
telemetry correlated by guest frame and monotonic host timestamp. A bounded
300-sample trace records host command-thread CPU frame time, real D3D12
timestamp-query GPU time, host streaming reads and 5-ms stalls, SDL silence-fill
underruns, and shader-translation/PSO-creation counts, failures, and elapsed
time. Audit-off file and pipeline hot paths use an atomic check and do not take
the telemetry mutex or timestamp work. Focused coverage is 2 cases / 7
assertions.

KI-012 is closed in v0.9.0.10: profiled hook/debug wrappers check
`TracyIsStarted` before creating zones or publishing Tracy metadata. The
filtered `[offline]` suite passes 3/3 cases and 10/10 assertions without
starting the manual-lifetime profiler. See the public report at
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
git -C third_party/rexglue-sdk rev-parse refs/tags/v0.9.0.28^{}
git submodule status --recursive
git -C third_party/rexglue-sdk status --short --ignore-submodules=none
```

The first two commands must return `6354bbe2150c7ce06bee5ffe399f17a94c948616`. The upstream base remains available as `refs/tags/v0.9.0` at `3eb9b511b4140d2769e27be63eae57d41bfa2afa`. Every `git submodule status --recursive` line must begin with one space: `-` means uninitialized, `+` means a commit mismatch, and `U` means a merge conflict.

## Recursive SHA manifest

The nested dependency SHAs were captured from the clean upstream v0.9.0 checkout on 2026-08-10; the root line is the reviewed v0.9.0.28 project-fork commit:

```text
6354bbe2150c7ce06bee5ffe399f17a94c948616 third_party/rexglue-sdk
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
