# Architecture

Owner: MCLA-R maintainers

Purpose: describe the implemented host/runtime architecture and the boundaries between generated guest code, ReXGlue, and MCLA-R-owned compatibility code.

Current state: repository foundation only. No runtime architecture has been implemented yet.

Planned boundaries:

- generated guest PowerPC-to-C++ output remains local and untracked
- ReXGlue supplies code generation and Xbox-derived runtime subsystems
- `src/app` owns application lifecycle and configuration
- `src/kernel` owns project-specific kernel/XAM compatibility behavior
- `src/hooks` and `src/patches` own byte-verified game-specific interventions
- `src/platform` owns host-only integration
- `src/diagnostics` owns structured logs, traces, and safe crash context
- all guest game-data access must pass through the selected VFS boundary

Update this document whenever a subsystem is introduced, ownership moves, a new runtime dependency is added, or a milestone changes the supported execution path.
