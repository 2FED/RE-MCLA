# M2-005 ReXGlue application scaffold

Date: 2026-08-11
Result: PASS

## Initialization

The pinned installed ReXGlue v0.9.0 CLI initialized the repository root with:

```powershell
rexglue init `
  --project-name mcla `
  --xex-path private/game/default.xex `
  --game-root private/game `
  --project-root .
```

The CLI completed successfully and created the canonical manifest-first project:

| File | Role |
| --- | --- |
| `CMakeLists.txt` | C++23 `mcla` host target linked through `rexglue_setup_target` |
| `CMakePresets.json` | Windows/Linux, AMD64/ARM64, Debug/Release/RelWithDebInfo preset matrix |
| `mcla_manifest.toml` | ReXGlue v0.9.0 project/entrypoint manifest |
| `src/main.cpp` | generated-module/app binding entrypoint |
| `src/mcla_app.h` | project-owned `rex::ReXApp` extension point |
| `generated/rexglue.cmake` | non-proprietary SDK discovery, target setup, and codegen target boilerplate |

No codegen command ran during this task, so no guest-derived source/header exists.

## Fresh-clone boundary correction

The repository previously ignored the entire `generated/` tree. That would have silently omitted ReXGlue's required, non-proprietary `generated/rexglue.cmake` bootstrap and made a fresh clone unconfigurable. The ignore policy now tracks only that exact file while `generated/default/**` and every other guest-generated path remain ignored.

This exception does not weaken the source-data policy: the tracked CMake file contains SDK/project boilerplate only and no XEX bytes, decompiled logic, generated guest code, user profile data, or absolute private host path.

## Verification

- ReXGlue CLI version: `0.9.0`
- one-time `rexglue init`: PASS
- all six expected scaffold files present: PASS
- second init without force rejected overwrite and left every scaffold hash unchanged: PASS
- CMake `win-amd64-debug` configure with the pinned installed SDK: PASS
- configured targets include `mcla` and `mcla_codegen`: PASS
- `generated/rexglue.cmake` is visible to Git: PASS
- synthetic `generated/default/guest.cpp` remains ignored: PASS
- pre-codegen guest-generated payload is empty: PASS

Building `mcla` is intentionally not a task criterion yet: `src/main.cpp` includes `generated/default/mcla_init.h`, which is correctly absent until M2-008 codegen. M2-006 hardens and parses the manifest; M2-007 freezes first-analysis flags before any codegen attempt.

## Plan re-evaluation

The original plan named a legacy `mcla_config.toml`. Pinned ReXGlue v0.9.0 is manifest-first and `rexglue init` emits `mcla_manifest.toml`; the CLI codegen and CMake target consume that manifest. M2-006 is therefore updated to validate/harden the current manifest instead of fabricating an obsolete parallel config.
