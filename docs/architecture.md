# Architecture

Owner: MCLA-R maintainers

Purpose: describe the implemented host/runtime architecture and the boundaries between generated guest code, ReXGlue, and MCLA-R-owned compatibility code.

Current state: the canonical ReXGlue v0.9.0 `mcla` application scaffold is implemented. It configures a C++23 host executable linked to `rex::runtime`; the executable cannot compile until M2 codegen creates the ignored guest-derived initialization header and sources.

Implemented bootstrap boundary:

- `CMakeLists.txt` owns the host target and invokes `rexglue_setup_target(mcla)`
- `CMakePresets.json` provides the upstream host/architecture/configuration matrix
- `mcla_manifest.toml` identifies the private entrypoint and ignored output roots
- `src/main.cpp` binds the generated module initialization to `MclaApp`
- `src/mcla_app.h` is the project-owned lifecycle extension point
- `generated/rexglue.cmake` is tracked non-proprietary SDK boilerplate
- every other file below `generated/` remains ignored guest-derived output

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
