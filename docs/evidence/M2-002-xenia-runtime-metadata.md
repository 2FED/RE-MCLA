# M2-002 sanitized Xenia runtime metadata

Date: 2026-08-11
Result: PASS

## Snapshot identity

- Source: ignored private stock-Xenia log snapshot (raw path intentionally omitted)
- Snapshot SHA-256: `95573DE737058A8E9A71B776A6E0A3851379FB26AF1E28A160FFA0B037EE3DE0`
- Xenia build: `canary_experimental@7d8db5a2c`
- Module hash: `1984A3354B78CE19`
- Entry point: `0x821322B8`
- Title: `Midnight Club: LA`
- Title ID: `545407F8`
- Media ID: `5940C9DB`

## Sanitized mounts

| Role | Sanitized value |
| --- | --- |
| Module launch alias | `GAME:\default.xex` |
| Guest module device | `\Device\Harddisk0\Partition1\default.xex` |
| Host storage | `[PRIVATE_BASELINE]\storage` |
| Host content/save data | `[PRIVATE_BASELINE]\content` |
| Host cache | `[PRIVATE_BASELINE]\cache` |

No absolute host path, user/profile identifier, or proprietary payload path is reproduced in this report.

## Import and kernel expectations

| Library | XEX import records | Requested / minimum | Unique audited imports | Xenia implementation coverage |
| --- | ---: | --- | ---: | --- |
| `xam.xex` | 190 | `2.0.7371.0` / `2.0.7371.0` | 95 | 90% (86 implemented, 9 unimplemented) |
| `xboxkrnl.exe` | 313 | `2.0.7371.0` / `2.0.7371.0` | 162 | 91% (149 implemented, 13 unimplemented) |

The XEX statically identifies `XBOXKRNL 2.0.6995.0` while both import descriptors request and require `2.0.7371.0`. The pinned Xenia configuration reports numeric `kernel_build_version=1888`. These are three different metadata domains; the configured numeric build is not rewritten as a four-part Xbox version here. M2-013 will map every unique import and determine runtime relevance.

## Warning and fatal-event baseline

The snapshot contains **34** warning/error events, all assigned to the following finite classes. `First line` is relative to the ignored snapshot and reveals no host path.

| Sanitized class | Count | First line | Meaning |
| --- | ---: | ---: | --- |
| `optional-controller-database` | 1 | 39 | Optional SDL mapping database was absent. |
| `base-heap-commit` | 3 | 43 | Xenia committed an unreserved guest page during startup. |
| `dynamic-import-resolution` | 18 | 1079 | One vibration helper and force-feedback ordinals were unavailable. |
| `undefined-extern` | 2 | 1116 | The title called the unimplemented volume-dismount helper. |
| `stubbed-file-sector-query` | 2 | 1125 | The file-sector query used a Xenia stub. |
| `missing-development-device` | 7 | 1151 | A retail-safe lookup referenced the absent development t: device. |
| `new-profile-gpd` | 1 | 1199 | The isolated profile initially had no title GPD. |

Fatal/crash/assert/device-lost/unhandled-exception marker count: **0**. The first warning is `optional-controller-database` at snapshot line 39; there is no first fatal event because the fatal count is zero.

The word `ERROR` in the dynamic import messages is emitted at Xenia warning severity and did not terminate the observed gameplay route. This report records evidence, not final severity: import/runtime ownership remains M2-010/M2-014 work.

## Verification performed

- PowerShell parser and positive `-WhatIf` no-write preflight: PASS
- Two consecutive report generations produced the same SHA-256: PASS
- Wrong module hash rejection: PASS
- Unknown warning-class rejection: PASS
- Output-below-`docs/evidence` containment rejection: PASS
- Absolute host path and profile-identifier scan: PASS
- `ast-grep scan` and 3/3 structural rule tests: PASS

## Sanitization and reproducibility gate

`scripts/export-xenia-baseline.ps1` accepts an ignored log/config snapshot, requires the exact supported title/build identifiers, permits output only below `docs/evidence`, emits only whitelisted captures and fixed event summaries, and fails if any warning event is unclassified. A private-path scan of this generated report is required before commit.
