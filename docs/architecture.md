# Architecture

Owner: MCLA-R maintainers

Purpose: describe the implemented host/runtime architecture and the boundaries between generated guest code, ReXGlue, and MCLA-R-owned compatibility code.

Current state: the canonical ReXGlue v0.9.0.2 `mcla` application consumes the ignored M2 generated corpus and produces a native Windows Release executable. The first clean build compiled 62 generated translation units and linked successfully; the project-owned host lifecycle now has a guest-free smoke path.

Implemented bootstrap boundary:

- `CMakeLists.txt` pins ReXGlue 0.9.0.2, validates the contained 62-source generated graph, owns the host target, and invokes `rexglue_setup_target(mcla)`
- `CMakePresets.json` provides the host/architecture/configuration matrix plus deterministic installed-SDK prefixes
- `mcla_manifest.toml` identifies the private entrypoint and ignored output roots
- `src/main.cpp` binds the generated module initialization to `MclaApp`
- `src/mcla_app.h` is the project-owned lifecycle extension point
- `src/mcla_app.cpp` owns the minimal logging-ready, path-finalization, and shutdown hooks; its opt-in `--mcla_lifecycle_probe` queues a clean UI-thread exit before guest runtime construction
- `generated/rexglue.cmake` is tracked non-proprietary SDK boilerplate
- every other file below `generated/` remains ignored guest-derived output
- when `generated/default` is absent, configuration exposes `mcla_codegen` but intentionally omits the native `mcla` target until a second configure

The lifecycle probe exists only to verify host application wiring. It does not
construct `Runtime`, load the XEX, or execute translated code. Guest memory,
image launch, VFS, and import behavior remain owned by M3-003 and later tasks.

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
