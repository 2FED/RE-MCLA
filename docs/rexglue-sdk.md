# ReXGlue SDK pin

Owner: MCLA-R maintainers

Purpose: record the immutable ReXGlue source and recursive dependency graph used by this project.

Update trigger: any intentional ReXGlue or nested submodule revision change.

## Project fork pin

**M9 (2026-10-29, pushed 2026-10-29): the submodule pin moves to
`f519dd53fbabffca5492497cb42f782602aae9a1`** (`f519dd5`) on the fork branch `mcla/mcla-r-hotfixes`, tagged
`v0.20.0.3`, package version **0.20.0.3**. The SDK derives its version from
`git describe` (`cmake/rex_version.cmake`): the exact tag yields `0.20.0.3`;
the same commit without the tag reports `0.20.0.7-dev.gf519dd5`, so clean clones
must fetch tags. Fast-forward on top of `a239b83`:

- `259d0b2` input(sdl): KI-040 XInput re-scan on hotplug, state reset on joystick
  quit, opt-in `hid_sdl_rawinput` (default off), instance/backend/path fields on the
  controller-added log line; moves the `thirdparty/sdl3` gitlink to `422c186`.
- `9ad1c11` ui(sdl): read-only host SDL event observer
  (`rex/ui/sdl_event_observer.h`) and joystick snapshot queries (M9-009, M9-010).
- `d968f59` kernel(memory): rate-limited `REXKRNL_IMPORT_FAIL` log of failed guest
  allocations (KI-035 diagnostic, no behaviour change).
- `8b10d62` gpu(d3d12): optional bounded wait for pipelines still being created
  (KI-037).
- `f519dd5` build: `.gitmodules` points `thirdparty/sdl3` at the SDL fork
  `https://github.com/2FED/SDL.git`, branch `mcla/xinput-hotplug-poll`, which holds
  the vendored-SDL commit `422c186c0a272bf6f255b2f4537a726f804e924b` (Windows
  XInput user-mask poll for hotplug). No source change against `8b10d62`.

SDK ctest at `8b10d62`: 1874/1874 ([KI-037 build](evidence/M9-006-ki-037-fix-build-20261029.md));
`f519dd5` changes only `.gitmodules`. Every nested gitlink except `thirdparty/sdl3`
is unchanged from the manifest below. Verify a fresh clone with:

```powershell
git -C third_party/rexglue-sdk rev-parse HEAD                  # f519dd53fbabffca5492497cb42f782602aae9a1
git -C third_party/rexglue-sdk rev-list -n 1 v0.20.0.3         # same
git -C third_party/rexglue-sdk ls-tree HEAD thirdparty/sdl3    # 422c186c0a272bf6f255b2f4537a726f804e924b
git submodule status --recursive
```

The `a239b83` pin text that follows stays as history.

**M9-001 (2026-10-28): the submodule pin is now
`a239b834194011ab6f5fa197cea4998899010062`** on the published fork branch
`mcla/mcla-r-hotfixes`. On top of `5dd58f8` it adds `678adc4` (the 32bpp
resolve-downscale correction below, now in the SDK) and `a239b83` (the M8
command-processor optimizations and unlocked-frame-rate support, with
`mcla_guest_vblank_hz` defaulting to 0, i.e. stock 30). No build step applies
SDK patches any more; the patch files below are history and test fixtures. Unlocked mode is a launcher choice (M9-003) that passes `--mcla_guest_vblank_hz=240`, not an SDK source edit. The SDK still reports
package version 0.20.0.0. Evidence: [M9-001](evidence/M9-001-reproducible-build-20261028.md).
The text below describes the earlier `5dd58f8` pin and stays as history.

The previous source pin is tagged v0.20.0.0 and published in the project fork;
the remote tag resolves to5dd58f894bf34b9ea85e59d3903c7f0e8044d853
(read-only remote verification October12,2026). Its isolated build and install at
`private/sdk20install-unpublished` passed. The fresh WGC-enabled host build at
`out/build/m7-final-sdk20` also passed all 82 build actions; neither compilation
nor SDK test success promotes new binaries or establishes physical gameplay acceptance.

The accepted October 10 p1 candidate remains the separately staged hybrid:
host82A69B5E, runtime542DD347 and GPU E2EA29B7 are unchanged. Its original
`out/install/win-amd64-profile-20261010` prefix replaced only the runtime
DLL/import library/profile header, retaining the accepted GPU and Tracy binaries.
Name/progress reload and actual F10 now have accepted owner returns. Keep their
artifact identities separate from v0.20.0.0; see [profile rename evidence](evidence/M7-profile-rename-20261010.md).

- Repository: `https://github.com/2FED/rexglue-sdk.git`
- Upstream repository: `https://github.com/rexglue/rexglue-sdk.git`
- Upstream base tag: `v0.9.0`
- Upstream base commit: `3eb9b511b4140d2769e27be63eae57d41bfa2afa`
- Published project tag: `v0.20.0.0`
- Immutable project commit: `5dd58f894bf34b9ea85e59d3903c7f0e8044d853`
- Fork branch: `mcla/mcla-r-hotfixes`
- Local path: `third_party/rexglue-sdk`

The accepted M8 clean32bpp packing correction is a separate delta on this
base. Its two-file [patch](../patches/rexglue/resolve-downscale-32bpp.patch) and
[reconstruction manifest](../patches/rexglue/resolve-downscale-32bpp.json) retain
the exact tested source/bytecode independently of private build directories.
Apply only to a separate clean checkout; preserve unrelated graphics work in
the submodule and the accepted SDK install. Clean SDK tests and four normal-host
scene pairs pass. Owner garage/driving improvement, F10 and fresh-process
name/progress reload are accepted on l12; further vinyl sharpness is deferred.
No submodule pin, default installation or release tag changes follow from this
patch. [Acceptance and runnable regression](evidence/M8-vinyl-packing-acceptance-20261012.md).

Delta pinned in `f519dd5`, pushed 2026-10-29 (2026-10-29, M9-006 KI-040; see the M9 pin above): an isolated copy of the pinned SDK at `private/l39-sdk-src` carries SDK commit `259d0b2` on `a239b83` (branch `mcla/ki-040-xinput-rescan`) and vendored-SDL commit `422c186`: a once-per-second XInput connection poll that forces SDL's normal re-scan on change, state reset on joystick quit, an opt-in `hid_sdl_rawinput` driver setting (default off) and instance/backend/path fields on the controller-added log line. SDK tests 1867/1867 (three new hotplug cases). It reports version `0.20.0.3-dev.g259d0b2`. No submodule pin, fork publication or default-install change yet; publishing path decided by the owner 2026-10-29 (option A): the vendored-SDL commit `422c186` is published on the new fork `https://github.com/2FED/SDL`, branch `mcla/xinput-hotplug-poll` (pushed 2026-10-29 with explicit owner approval); the SDK `.gitmodules` for `thirdparty/sdl3` will point at that fork, the SDK branch will be pushed to `2FED/rexglue-sdk` `mcla/mcla-r-hotfixes` and the project pin bumped once the SDL event observer fix lands. The same branch adds SDK commit `9ad1c11`: a read-only host-facing SDL event observer (`rex/ui/sdl_event_observer.h`: `AddSdlEventObserver`/`RemoveSdlEventObserver` fan out one `SDL_AddEventWatch` inside `rexruntime.dll`) and joystick snapshot queries (`CaptureSdlJoysticks`/`QuerySdlJoystick`, never opening devices or touching haptics), used by M9-009 and M9-010; SDK tests 1869/1869 (two new cases). SDK commit `d968f59` adds the KI-035 diagnostic: a rate-limited `REXKRNL_IMPORT_FAIL` line on failed `NtAllocateVirtualMemory`/`MmAllocatePhysicalMemoryEx` (first 32, then every 256th), no behaviour change; ctest 1869/1869. `private/l39-sdk-install` now carries all three deltas (`rexruntime.dll` `9E7449DD...`). [Fix build](evidence/M9-006-ki-040-fix-build-20261029.md).

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

The v0.9.0.29 delta replaces the two import stubs actually reached during
broad MCLA play. `IoDismountVolumeByFileHandle` validates a real file handle
and acknowledges the operation while retaining the runtime-owned host VFS
mount. `XeKeysConsolePrivateKeySign` fails explicitly when no console private
key exists, preserves the caller buffer, and rejects null inputs rather than
reporting false signing success. Focused coverage is 2 cases / 5 assertions.

The v0.9.0.30 delta routes Xbox user slot zero to the connected SDL controller
with the latest meaningful button-down or analog activity, regardless of its
physical SDL player index. Standard XInput deadzones suppress drift-based
ownership changes; disconnect falls back to the most recently active remaining
controller; capabilities, keystrokes, and rumble follow the same mapping. The
selected physical controller is hidden at its original nonzero guest index to
avoid duplicate exposure. InputSystem now publishes a merged monotonic packet
sequence when physical and synthetic drivers coexist. Focused coverage is 21
SDL cases / 124 assertions, including 7 slot cases / 23 assertions and the
15-case controller/RTL selection / 104-assertion gate. A physical Steam Input
probe observed native Steam Controller slot 0 plus XInput wrapper slot 1,
selected physical slot 1 for guest slot 0, and received owner-confirmed input.

The v0.10.0.0 delta adds a public model-agnostic racing-wheel input and force-
feedback surface. SDL wheel classification, with bounded vendor/product
fallbacks for drivers that omit the type, maps configurable steering, pedal,
hat, and button controls to an Xbox 360 wheel guest identity. The existing
latest-meaningful-device policy switches guest slot zero between wheel and
gamepad without a brand allowlist. The title's 64 `XInputdFF` effect slots map
constant, ramp, square, sine, triangle, both sawtooth families, spring, and
damper effects into SDL haptics. Focus loss and latest-active switching stop
wheel force and restore only valid current infinite effects. Host policy keeps
overall gain at 100%, continuous periodic texture at 40%, neutralizes infinite
directional constant force, and floors nonzero finite transient amplitudes at
75% of current gain without altering zero or infinite effects. Focused coverage
passes 227 assertions / 9 wheel cases and 358 assertions / 31 SDL input cases.
Physical T300 evidence covers input, hotplug, centering, finite curb/collision
events, focus recovery, and wheel-to-gamepad-to-wheel switching; other SDL
wheel models are configuration-compatible but not physically verified.

The v0.10.0.1 delta stabilizes latest-active ownership when Steam Input exposes
one gamepad through synchronized native and XInput endpoints. Mirrored edges no
longer bounce logical ownership, analog takeover from a force-feedback wheel
requires deliberate movement rather than drift, and small steering or pedal
deltas let an inactive wheel reclaim ownership without crossing a new deadzone
class. It also adds a project hook after GPU-plugin load and before presentation
setup, allowing MCLA to select its delayed render-target readback default after
the plugin CVars exist. Focused SDL coverage passes 369 assertions / 33 cases;
physical Photo Mode and extended T300 confirmation remain release gates.

The v0.10.0.2 delta restores runtime borderless-fullscreen switching in the SDL
window layer. A non-repeated Alt+Enter press and every complete left-button
double-click toggle the same window state; the triggering down/up pair is
consumed so it cannot become accidental title input. Both routes emit bounded
source/state markers for physical regression evidence.

The v0.11.0.0 delta exposes an application-triggered bounded frame audit.
D3D12 timestamp resources are prepared during graphics setup, but timestamp
queries and command-processor CPU measurements run only while the 300-frame
audit is active. A late diagnostic trigger refuses unavailable GPU timing and
cannot reset an in-flight window. MCLA uses this once per process from F10 so
a later F10 package can return completed gameplay-only CPU/GPU, streaming,
audio-underrun, shader, and PSO evidence.

The v0.12.0.0 delta exposes the already compiled Xenos backend choice through
`RuntimeConfig::gpu_backend` and the init-only
`--gpu_backend=any|d3d12|vulkan` option. `any` preserves the platform default;
an application may explicitly request either registered backend without a
second plugin ABI or separate guest build. The same release also makes the
migration/template fixtures CRLF-safe and collision-free on Windows.

The v0.13.0.0 delta adds a read-only command-processor performance diagnostic
snapshot without changing normal presentation policy. The Vulkan backend
publishes monotonic counts for guest-frame, swap, output-refresh, placeholder,
missing-source, and submission activity plus cumulative microseconds spent in
frame-fence wait, pipeline-end submission, deferred execution, queue submit,
and total guest-frame wall span. The application samples these counters only
when producing an explicit diagnostic package, so the route adds no per-frame
file I/O or continuously enabled trace buffer.

The clean v0.10.0.2 SDK validation executed all 1,805 registered cases: 1,792
passed, four known BitStream cases were skipped, and nine pre-existing
migration/template fixture cases failed outside this windowing delta. The
changed SDL window source compiled in the clean install and the real title
physically passed both toggle routes with exact geometry and runtime markers.

The clean v0.11.0.0 Release build and install completed. Focused frame-audit
coverage passes 11 assertions in three cases. A full 1,806-case RelWithDebInfo
CTest run during the same change passed 1,793, skipped the same four BitStream
cases, and retained nine pre-existing migration/template fixture failures;
all PPC cases passed.

The clean v0.12.0.0 Windows configuration enabled both D3D12 and Vulkan. Its
full RelWithDebInfo suite passed all 1,802 executed cases with four known
BitStream cases skipped: 100 percent pass, zero failures out of 1,806
registered tests. The matching Release SDK install completed and reports
version `0.12.0.0`.

The clean v0.13.0.0 RelWithDebInfo build and install retained that full result:
1,802 executed cases passed, four known BitStream cases were skipped, and zero
tests failed out of 1,806 registered. The matching clean Release build/install
completed and reports version `0.13.0.0`.

The v0.14.0.0 delta adds explicit persistent-dialog visibility and weak-token
UI-thread wakeups. Empty achievement notifications no longer keep the UI
presenter polling/repainting unchanged guest images. Seven new headless
UI/memory cases cover actual drawer dispatch, self-hide iteration, queued
wake/destruction, populated toast rendering, and read-only snapshot getters.
Four cumulative counters distinguish completed command-ring idle and
WAIT_REG_MEM episodes from handled non-MMIO write-fault activity. The owned
Memory/MMIO getter avoids global-pointer lifetime dependencies; the counters
do not change scheduling, protection or coherency. Aggregate fault durations
span threads and are not additive graphics-frame stages.

Full dual-backend RelWithDebInfo and tagged Release validation pass 1,809
executed cases with four known BitStream skips and zero failures (1,813
registered). The matching Release install reports `0.14.0.0`. SDK-local
`README-AI.md`, `sgconfig.yml`, one scoped safety rule and its fixtures now
document and gate this fork's agent workflow. Physical Deck performance is
not certified by these native Windows tests.

KI-012 is closed in v0.9.0.10: profiled hook/debug wrappers check
`TracyIsStarted` before creating zones or publishing Tracy metadata. The
filtered `[offline]` suite passes 3/3 cases and 10/10 assertions without
starting the manual-lifetime profiler. See the public report at
[rexglue/rexglue-sdk#410](https://github.com/rexglue/rexglue-sdk/issues/410).

The v0.15.0.0 delta adds 21 address-free command, Vulkan fence and physical
memory-watch/fault phase counters. Packet scopes include early-return and
exception paths; fault phases retain the successful non-MMIO write denominator.
Native protection ranges, guest access rights, aliases, scheduling and coherency
are unchanged. Timers are cumulative and nested across threads, not exclusive
frame stages. Eight new regression cases exercise packet scopes, real protected
writes, aliases and safe/unsafe watch gaps. RelWithDebInfo and tagged Release
each pass 1,817 executed cases / four known skips / zero failures (1,821
registered). See [Deck attribution evidence](evidence/M7-016-deck-command-memory-cost.md).

The v0.16.1.0 delta repairs CPU primitive-cache overlap, cancels insertion after
in-flight invalidation, bounds no-hit unwatch replies, and restores the 4096-index
cache threshold. Small converted inputs are recomputed fresh without a primitive
reuse watch; GPU uploads/coherency and guest rights remain intact. Pre-fix tests
fail 6/9; final real CPU/Memory coverage passes 800 assertions / 12 cases, and
both complete configurations pass 1,834 cases plus four known skips. The threshold
is not yet a measured MCLA/Deck speedup. See
[cache evidence](evidence/M7-016-deck-primitive-cache.md).

The v0.16.0.0 delta separates the already-precommitted 512 MiB Windows physical
RAM into one SEC_COMMIT section shared by all five physical aliases. Virtual
memory stays SEC_RESERVE; POSIX layout, guest metadata/permissions, watch calls
and coherency remain unchanged. This avoids Proton 9.0-4's per-protection/query
SEC_RESERVE committed-range server requests without relaxing memory safety.
Mapping retry clears completed unmaps and stops on cleanup failure. Five new
native mapping cases cover 79 assertions. Full **all-target** RelWithDebInfo
and tagged Release builds/CTest each pass 1,822 cases / four known skips /
zero failures (1,826 registered). A unit-only build initially left a stale
PPC runner after the private class-layout change; both complete suites were
rebuilt and rerun. The CMake ABI floor is 0.16.0. Native control is not Deck
acceptance. See [physical-backing evidence](evidence/M7-016-deck-physical-backing.md).

## Reproduction

The preceding v0.17.0.0 public snapshot delta added seven Vulkan resolve counters:
inclusive IssueCopy calls/time, actual memcpy count/bytes/time and explicit
resolve readback waits/time. All four wait sites are measured; memexport waits
are excluded. Copy faults are included, mapped-memory invalidation is not.
No readback/coherency/scheduling policy changed. Both full native configurations
and tagged Release pass 1,834 tests plus four known skips. A first parallel run
exposed a test-only XAM temp-directory collision; atomic unique-directory
ownership fixes it, with seven saved-game fixtures repeated five times per
configuration. See [resolve evidence](evidence/M7-016-deck-resolve-readback.md).

The preceding v0.17.1.0 preserves the exact Xenon db16cyc hint as one bounded host
`simde_mm_pause()` instead of emitting only a comment. On x86 this compiles to
PAUSE, not Sleep or an OS yield; it neither changes PPC state nor reproduces
exactly sixteen Xenon cycles. Three PPC semantic cases plus an emission
regression cover neighboring OR instructions as well as the exact hint.
All-target RelWithDebInfo, Release and tagged Release pass 1,838 tests plus
four known skips. Actual MCLA non-force regeneration adds exactly 13 hints
across four files; removing only those lines restores all 67 previous files
byte-for-byte. The optimized native polling loop contains eight consecutive
PAUSE instructions. Its compatibility floor was 0.17.1; the parent pins the
full four-part tag and commit. Deck performance benefit is NOT proven.
See [accepted return and correction](evidence/M7-016-deck-delay-hint.md).

The v0.18.0.0 delta uses a one-entry-per-stage Vulkan descriptor snapshot keyed
by actual ordered values, current set/validity and layout/frame/submission.
Requests, uploads, barriers, usage marks and pool fences remain unchanged;
misses allocate fresh sets and partial writes cannot publish reusable values.
Layout/external/epoch transitions invalidate, with conservative full rebinding
on layout change. Six public counters measure successful stage-set reuse/write
and inclusive UpdateBindings/native-driver calls/time, so the compatibility
floor advances to 0.18.0. Independent review and both all-target configurations
pass 1,843 executed/four known skips (1,847 registered); tagged Release also
passes. Five helper fixtures/37 assertions do not substitute for real Vulkan
lifetime/failure injection or physical Deck evidence. See
[current candidate and accepted return](evidence/M7-016-deck-descriptor-reuse.md).

Preceding v0.18.0.1 corrects only the two new high-frequency elapsed counters:
accumulate nanoseconds internally and convert cumulative totals to public
microseconds. Per-call microsecond rounding on a native driver discarded
short calls. Descriptor policy and the six-field schema/0.18.0 floor remain
unchanged. Independent review, nanosecond/early-return/unwind fixtures and
full all-target RelWithDebInfo/Release pass: 1,845 executed, four known skips
(1,849 registered), zero failures. After making a new tag, explicitly rerun
CMake configure before rebuilding/installing: tags alone do not invalidate
cached package-version metadata. Exact installed metadata is a release gate.

Preceding v0.19.0.0 adds safe-disarm-gap and metadata-lock-miss counters with no
native protection/coherency policy change. Independent review caught and
corrected new unsynchronized metadata reads before deployment: classification
now tries the heap mutex nonblocking after callbacks, with exact legacy
watched-only fallback and explicit incompleteness on contention. Eleven cases /
479 assertions and both all-target/full configurations pass1,856 executed plus
four known skips (1,860 registered). Public API floor0.19.0; no guest codegen
change. Physical results and limits: [disarm fragmentation](evidence/M7-016-disarm-fragmentation.md).

Current v0.20.0.0 separates the local display name from its stable storage key;
the changed public constructor ABI raises the API floor to 0.20.0. The isolated
source at `private/sdk20s` excludes unrelated renderer work. Windows lacked
file-symlink privilege, so 15 libmspack file symlinks were materialized from their
targets in that isolated dependency checkout; no other source changes were
introduced there. This is a recorded checkout accommodation, not a new dependency
revision or a claim that the materialized dependency worktree is Git-clean.

Tagged-source tests pass 401 unit cases/3364 assertions across 41 suites, with four
known skips, and 1462 PPC cases/5753 assertions. Full SDK build/install to
`private/sdk20install-unpublished` passes. These results do not turn the earlier
monolithic test-process collision into a PASS or relabel p1's tested runtime.
Use the explicit isolated prefix in [the build recipe](build.md) until the
default install has separately been updated and verified.

After the commit/tag and parent gitlink are published, a fresh clone can
initialize the pinned graph with:

```powershell
git submodule sync --recursive
git submodule update --init --recursive
```

Verify the root pin and recursive cleanliness with:

```powershell
git -C third_party/rexglue-sdk rev-parse HEAD
git -C third_party/rexglue-sdk rev-list -n 1 v0.20.0.0
git submodule status --recursive
git -C third_party/rexglue-sdk status --short --ignore-submodules=none
```

The first two commands must return `5dd58f894bf34b9ea85e59d3903c7f0e8044d853`. The upstream base remains available as `refs/tags/v0.9.0` at `3eb9b511b4140d2769e27be63eae57d41bfa2afa`. Every `git submodule status --recursive` line must begin with one space: `-` means uninitialized, `+` means a commit mismatch, and `U` means a merge conflict. Source changes must be reviewed separately; matching submodule commits alone do not prove clean worktrees. Do not rebuild the original checkout's unrelated graphics experiments into the accepted profile integration.

## Recursive SHA manifest

The nested dependency SHAs were captured from the clean upstream v0.9.0 checkout on 2026-08-10 and remain unchanged; the root line is the reviewed, locally tagged v0.20.0.0 project-fork commit. For the M9 pin `f519dd5` the root line is `f519dd53fbabffca5492497cb42f782602aae9a1` and the `thirdparty/sdl3` line is `422c186c0a272bf6f255b2f4537a726f804e924b` (fork `2FED/SDL`); all other lines are identical:

```text
5dd58f894bf34b9ea85e59d3903c7f0e8044d853 third_party/rexglue-sdk
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
