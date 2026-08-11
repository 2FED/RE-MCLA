# Architecture

Owner: MCLA-R maintainers

Purpose: describe the implemented host/runtime architecture and the boundaries between generated guest code, ReXGlue, and MCLA-R-owned compatibility code.

Current state: the canonical ReXGlue v0.9.0.9 `mcla` application consumes the ignored generated corpus and produces a native Windows executable. Crash instrumentation expands the current corpus to 65 generated translation units; the project-owned host lifecycle, loaded-image probes, privacy-safe synthetic crash path, and guest-frame presentation telemetry are verified independently.

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

- `CMakeLists.txt` pins ReXGlue 0.9.0.9, validates the contained 65-source generated graph, owns the host target, and invokes `rexglue_setup_target(mcla GPU_PLUGINS xenos)` so every configuration stages the matching runtime-loaded Xenos plugin
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
creation. ReXGlue v0.9.0.9 supplies the fail-closed device boundary and explicit
offline results for the ten direct XONLINE/social/XHV imports reviewed in M3-007;
user/cache writes remain on their separately configured roots.

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
readback; it does not claim frontend reachability or visual correctness.

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
