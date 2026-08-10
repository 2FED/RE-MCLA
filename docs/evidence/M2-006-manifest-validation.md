# M2-006 ReXGlue manifest validation

Date: 2026-08-11
Result: PASS

## Accepted manifest contract

Pinned ReXGlue v0.9.0 uses `mcla_manifest.toml` rather than the legacy config filename in the original plan. The tracked manifest is constrained to:

| Field | Required value |
| --- | --- |
| `project.name` | `mcla` |
| `project.sdk_version` | `0.9.0` |
| `project.game_root` | `private/game` |
| `entrypoint.file_path` | `private/game/default.xex` |
| `entrypoint.out_directory_path` | `generated/default` |
| `entrypoint.includes` | empty |

`scripts/verify-rexglue-manifest.ps1` parses this exact first-analysis schema, rejects unknown/duplicate sections or keys and unsupported syntax, requires portable non-traversing relative paths, resolves the game/XEX paths below ignored `private/`, resolves output below ignored `generated/`, rejects reparse traversal, and validates the exact supported XEX size and SHA-256.

## Verification

- PowerShell parser: PASS
- positive exact manifest: PASS
- independent Python `tomllib` parse: PASS
- project/SDK and all six required values: PASS
- XEX size `9252864` and SHA-256 `C386F4001FA569E6AD4B982F441F67412F00B3F47C166134555CD4B59854A432`: PASS
- parent traversal fixture: rejected
- absolute/backslash path fixture: rejected
- generated output outside the approved root: rejected
- unknown key fixture: rejected
- duplicate key fixture: rejected
- unsupported non-empty includes syntax: rejected
- `ast-grep scan` and 3/3 structural rule tests: PASS

The validator emits relative public configuration values and the verified XEX identity. It performs no codegen and writes no output, preserving the M2-007-before-M2-008 analysis order. ReXGlue's own manifest consumer will be exercised by the first non-force codegen gate in M2-008.
