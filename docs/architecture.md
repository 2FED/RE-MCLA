# Architecture

Owner: MCLA-R maintainers

Purpose: describe the implemented host/runtime architecture and the boundaries between generated guest code, ReXGlue, and MCLA-R-owned compatibility code.

Current state: the canonical ReXGlue v0.10.0.0 `mcla` application consumes the ignored generated corpus and produces a native Windows executable. Crash instrumentation expands the current corpus to 65 generated translation units; the project-owned host lifecycle, loaded-image probes, privacy-safe synthetic crash path, guest-frame presentation and timing telemetry, scoped frontend render-path audit, reached single-local-user XAM contract, locale/path boundary, explicit network-disabled offline-service boundary, privacy-safe audio event windows and long-session recovery, reached unsupported-operation coverage, latest-active controllers, model-agnostic wheel input, and bounded title force feedback are verified independently.

## Consumer preparation model

The planned public distribution is an asset-free launcher/runtime package, not a
prebuilt copy of the game. A user supplies their legally obtained supported disc
dump locally. The launcher verifies the exact image identity, extracts only into
a contained staging root, runs the existing code-generation and native-build
pipeline locally, and atomically publishes a fingerprinted prepared-game
directory. Subsequent launches reuse that prepared directory; repair or a
project/SDK update may trigger a new preparation generation.

```text
user disc dump
    -> exact identity verification
    -> contained local extraction
    -> local guest code generation and native compilation
    -> atomic prepared/<content+toolchain fingerprint>/
    -> launcher starts the prepared native runtime
```

The source dump is read-only, is never uploaded or packaged, and is not deleted
by repair or uninstall. Game assets, extracted executables, and generated guest
code remain local and untracked. The current scripts implement the core
validation, extraction, code-generation, native-build, and launch stages; they
do not yet implement fingerprinted prepared-generation publication or reuse.
M9 owns that atomic/resumable prepare-repair-update orchestrator, the launcher
UI, portable toolchain decisions, and clean-machine package tests. A future
split between a prebuilt host/runtime and a locally generated guest module is
desirable if the pinned runtime supports it, but is not a prerequisite for this
distribution model.

Implemented bootstrap boundary:

- `CMakeLists.txt` pins ReXGlue 0.10.0.0, validates the contained 65-source generated graph, owns the host target, and invokes `rexglue_setup_target(mcla GPU_PLUGINS xenos)` so every configuration stages the matching runtime-loaded Xenos plugin
- `CMakePresets.json` provides the host/architecture/configuration matrix plus deterministic installed-SDK prefixes
- `mcla_manifest.toml` identifies the private entrypoint and ignored output roots
- `src/main.cpp` binds the generated module initialization to `MclaApp`
- `src/mcla_app.h` is the project-owned lifecycle extension point
- `src/mcla_app.cpp` owns the minimal logging-ready, path-finalization, and shutdown hooks; its opt-in `--mcla_lifecycle_probe` queues a clean UI-thread exit before guest runtime construction
- `src/mcla_logging.h/.cpp` owns the schema-1 `app`, `ppc`, `kernel`, `xam`, `vfs`, `gpu`, `audio`, `input`, and `patches` category registry; every category inherits the global level unless its init-only project override is set
- `generated/rexglue.cmake` is tracked non-proprietary SDK boilerplate
- every other file below `generated/` remains ignored guest-derived output
- when `generated/default` is absent, configuration exposes `mcla_codegen` but intentionally omits the native `mcla` target until a second configure

The lifecycle probe exists only to verify host application wiring. It does not
construct `Runtime`, load the XEX, or execute translated code. The separate
module-config probe owns image/dispatch verification. The VFS probe owns the
disc mount and write-containment contract; import behavior remains owned by
M3-005 and later tasks.

Normal guest launches select the staged `xenos` GPU plugin in `MclaApp::OnPreSetup`
when no explicit plugin is configured. Lifecycle, module-config, VFS, crash,
and logging probes are deliberately exempt so their guest-free contracts do not
acquire a graphics dependency. The final M3-013 dispatch map contains 30,025
entries after five bounded post-GPU callable boundaries were added through
non-force analysis configuration.

Before Runtime construction, `MclaApp` fail-closes on any mismatch in the
accepted image/code ranges, bounded ordered function map, sentinel, or entry
mapping. After XEX load and before guest-thread creation it verifies exact Title
ID `545407F8`, Media ID `5940C9DB`, loaded image range
`82000000-829E0000`, entry `821322B8`, and dispatcher coverage, then emits the
canonical identity marker consumed by the M3-014 smoke gate.
`--mcla_module_config_probe` exercises that complete contract and exits before
guest execution.

The synthetic crash-report probe closes its report before scheduling its UI
exit callback. ReXGlue's general Runtime teardown can force-terminate the idle
Audio Worker while it owns a guest-heap lock, after which `XThread::FreeStack`
may block forever in `BaseHeap::Release`. Because this diagnostic route never
launches guest entry-point code, its callback emits the project shutdown
marker, flushes logging, and hard-exits the process. Normal application closure
does not use this probe shortcut: M3-015 separately drives the real window
through `WM_CLOSE` and requires ReXGlue's ordered close/hard-exit markers.

Before every normal guest launch, `MclaApp` verifies that `game:` and `d:` map
to `\Device\Harddisk0\Partition1`, resolves representative XEX/BIK/RPF files
through all aliases, rejects root traversal, and proves the game device is
read-only across open/create/delete and writable-mapping paths. The
`--mcla_vfs_probe` route exercises the same checks and exits before guest-thread
creation. ReXGlue v0.10.0.0 supplies the fail-closed device boundary and explicit
offline results for the ten direct XONLINE/social/XHV imports reviewed in M3-007;
user/cache writes remain on their separately configured roots.

ReXGlue v0.10.0.0 extends that boundary to the return-bearing progression
imports reached by MCLA. Achievement guide UI reports not signed in while the
local achievement manager remains authoritative; leaderboard creation returns
a bounded initialized enumerator with zero rows; and voice packet submission
returns busy rather than inheriting stale guest `r3`. Presence and Rate My Ride
remain explicitly unavailable, while Driving Test progression remains owned by
the title save. This policy does not synthesize Live identity, service data,
leaderboard rows, progression, or vehicle unlocks.

Early guest timing uses ReXGlue's host-derived 50-MHz guest clock for generated
`mftb`/`mftbu`, `KeQueryPerformanceFrequency`, and `KeQuerySystemTime`. Guest
threads notify the kernel, wait the Xenia-compatible 10-ms startup grace period,
deliver queued APCs, and then dispatch a reviewed raw or XAPI start plan.

The M4 presentation evidence boundary begins at a guest-originated Xenos swap.
ReXGlue assigns the refreshed guest output a monotonic sequence, records bounded
markers only when the final guest graphics effect reaches a successful DXGI
`Present`, and publishes that sequence as a release-ordered watermark after the
marker. Project capture runs on a separate stop-aware worker. The D3D12 capture
consumer holds mailbox ownership through resource transition, copy, fence wait,
and readback; it retries if the acquired guest sequence is newer than the
published present watermark. MCLA then computes bounded nontrivial-image metrics
and writes a private BMP. The physical verifier independently decodes that BMP
and binds the sequence, successful-present HRESULT class, dimensions, metrics,
and file hash. This proves presented guest activity plus a same-or-earlier
readback. M4-002 extends that boundary with an opt-in schema-1 Xenos audit. The
D3D12 command thread emits bounded RT storage mappings, actual guest BIND
formats, ownership-transfer modes, staged resolve modes, shader/PSO outcomes,
draw/depth/MSAA use, and semantic gamma state. After the project writes the
private title capture, it requests a GPU-thread checkpoint that freezes and
emits four internally balanced summaries before normal hard exit. The physical
verifier cross-links BIND storage tuples to RT records, validates direct/fallback
resolve equations, compares stable logo and `PRESS` edge regions with a pinned
private Xenia image, and independently rehashes the source game and current
runtime artifacts. The accepted route forces host RTV/DSV and asynchronous
shader compilation off for deterministic evidence. It does not claim ROV, PWL
gamma, true-direct resolve, whole-frame equality, or general rendering
correctness.

The M6-012 performance boundary is a default-off InitOnly SDK audit. It assigns
each complete host command frame a monotonic guest-frame correlation and emits
one bounded sample only after both CPU elapsed time and a fence-safe D3D12
timestamp-query result are available. Host-file reads, SDL silence fills,
shader translations, and graphics-pipeline creations contribute per-frame
deltas under the same audit lifetime. Exactly 300 samples freeze the audit and
produce one balanced summary. Audit-off streaming and pipeline paths use a
single atomic enabled check and do not acquire the telemetry mutex or take
timestamps. Markers contain only counters/timings, never paths, shader bytes,
audio content, device identity, or guest addresses.

The M4-011 frontend smoke path is an opt-in project driver layered above the
normal input system. It supplies only slot-0 synthetic input and serializes
source and guest observations under one mutex. From a pinned private post-OOBE
save it applies two 200-ms `START` pulses and two 200-ms `RB` pulses, with a
two-second inter-tab debounce, and captures title, free-roam gameplay, pause,
and Settings/Options frames. The route closes through the exact native window's
`WM_CLOSE`; the title has no internal Exit action and the evidence does not
invent one. ReXGlue v0.10.0.0 canonicalizes only the relative suffix when
resolving mounted VFS roots, allowing saved-profile root opens with trailing
separators without changing containment.

ReXGlue v0.10.0.0 separates stable SDL physical storage slots from the guest's
logical controller selection. Xbox user slot zero follows the connected pad
with the latest button-down or analog deadzone transition, while release edges
and analog noise inside the standard XInput deadzones do not steal ownership.
Capabilities, keystrokes, and rumble resolve through the same selected pad;
disconnect chooses the remaining controller with the newest activity. The
selected pad is hidden at its original nonzero guest index so one device is not
exposed twice. When a deterministic project input driver overlays physical SDL,
the combined state receives its own monotonic guest packet sequence rather than
comparing unrelated per-driver packet counters.

The M6-002 garage gate reuses that synthetic driver as an opt-in, file-commanded
slot-0 controller. A strict allowlist converts sequence-numbered commands into
short digital pulses, and the driver waits for the corresponding guest-visible
down and up states before publishing each control marker. The canonical runner
uses 69 such pulses across two isolated processes; no physical controller or
operator prompt participates. Lifecycle capture requests are separate from the
input protocol, so only the twelve requested state frames are retained. The
second process receives an exact copy of the changed profile tree, making the
garage persistence result a save/restart proof rather than same-process state.

The M6-003 race-system boundary is a project-owned default-off InitOnly audit
around seven known guest script wrappers plus two requested frame captures. It
records only bounded scalar classifications and counters; no guest pointers,
race names, save content, or host paths are emitted. The reached retail Martin
route invokes `Race_Finish` but not the optional description, checkpoint, or
UI-result wrappers, so those zero-hit counters remain explicit limitations.
The accepted matrix combines this current physical finish/reward route with
immutable Ian-series, repeated-race reward/resource, and traffic evidence.
Operator completion and police observations are stored separately as external,
non-machine provenance.

The M6-004 environment-effects boundary reuses the project-owned synthetic
slot-0 driver without changing normal input or renderer behavior. A default-off
InitOnly probe performs one deterministic saved-game/Arcade route, records all
18 frontend pulses and two render challenges at both source and guest
boundaries, and writes six bounded private presenter readbacks. The verifier
binds the exact `RAINY` / `DAWN` selection sequence, successful-present
watermarks, sampled frame differences, external shutdown, current game/save/SDK
and runtime identities, and prior rendering/time/race evidence. It deliberately
does not convert category presence into parity: colored vehicle reflections,
alpha stipple/shimmer, and intermittent minimap flicker retain their separate
open S2 issue boundaries.

The M6-005 save boundary combines native title evidence with isolated platform
fault tests. The accepted M6-002 save chain proves native autosave/overwrite,
exact process-to-process handoff, and fresh-process load. Clean-save creation
remains the stock-Xenia baseline plus an isolated SDK creation/restart roundtrip. ReXGlue
v0.9.0.23 wraps saved-game creation and replacement in a marker-backed
transaction: the old package/header are copied before mutation, marker removal
is the commit point, and a later manager restores any still-marked transaction
idempotently. In-process enumeration recognizes active transactions and cannot
trigger recovery against a live write. Missing, truncated, or identity-mismatched
headers are omitted rather than fabricated. An InitOnly dummy-HDD free-space
boundary rejects create/overwrite/truncate before transaction start, while
existing-content opens remain available. Destructive tests use temporary roots;
the canonical HANGOUT save is only rehashed and never modified.

The M6-006 profile/settings boundary is also isolated from gameplay. ReXGlue
v0.10.0.0 stores only known standard XAM settings under the local user's global
profile root, with an explicit magic/id/type/size header, per-setting size
bounds, temporary/backup recovery, and full-batch validation before any guest
write is applied. Title-specific binary settings retain their existing
per-title location and cannot leak into the global root. The host CVar layer
continues to serialize named language and controller preferences through its
TOML restart path. MCLA does not import `XamUserWriteProfileSettings`, so this
is platform restart coverage backed by prior physical profile/language/input
routes, not a fabricated native title-write or first-run account flow.

The M4 XAM evidence boundary is opt-in and privacy-safe. ReXGlue records only
semantic results for the reached local-user APIs: slot class, sign-in state,
XUID nonzero/mask agreement, name/profile consistency, and distinct absent-slot
bits. It never publishes the XUID, profile name, guest pointers, or host paths.
The project requests one checkpoint summary immediately before its verified
title capture. The accepted route proves slot 0 is the sole local user and that
slots 1-3 are independently absent. Profile-setting reads, privilege checks,
and sign-in UI were not reached in that route; deterministic voice defaults are
covered by focused SDK tests, while generalized writes and persistence remain
later scope.

Retired-service entitlements are not part of the compatibility layer. The
canonical M6-009 policy preserves the title's retail lock and any authentic
entitlement already present, but it never synthesizes Social Club completion,
rewrites a save, or edits source game data. Compatibility fixes remain limited
to restoring observable local/on-disc platform contracts. A future convenience
unlock must live in an explicitly named default-off InitOnly cheat surface,
remain non-persistent, and be excluded from canonical progression evidence.
The current tree intentionally implements no such cheat.

Generated functions retain metadata-only crash breadcrumbs: current PPC function,
the last tracked function/basic-block guest PC, and the last typed, raw, or stubbed
import. C++ exceptions escaping guest execution are caught at the `XThread`
boundary and paired with a bounded module-relative host stack. Guest registers,
guest stack data, and guest memory are excluded by default.

Project-owned operational messages use semantic category loggers instead of the
generic ReXGlue `core` logger. Existing lifecycle/crash, image/dispatch, and
disc-policy messages route through `app`, `ppc`, and `vfs`; the remaining six
categories are registered now for their owning M3+ subsystems. The opt-in
logging probe emits one bounded schema marker per category and is used only by
the private filter/write smoke gate.

Planned subsystem boundaries:

- generated guest PowerPC-to-C++ output remains local and untracked
- ReXGlue supplies code generation and Xbox-derived runtime subsystems
- `src/app` owns application lifecycle and configuration
- `src/kernel` owns project-specific kernel/XAM compatibility behavior
- `src/hooks` and `src/patches` own byte-verified game-specific interventions
- `src/platform` owns host-only integration
- `src/diagnostics` owns structured logs, traces, and safe crash context
- all guest game-data access must pass through the selected VFS boundary

Update this document whenever a subsystem is introduced, ownership moves, a new runtime dependency is added, or a milestone changes the supported execution path.
