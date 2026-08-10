# M1-009 safe extraction wrapper evidence

Date: 2026-08-10

Script: `scripts/extract-game.ps1`

## Safety contract

- `IsoPath` and `DestinationPath` are mandatory.
- Relative destinations resolve from the repository root.
- The destination must be a child of the repository's ignored `private/` root.
- Existing destinations are always rejected; the wrapper has no implicit or forced overwrite mode.
- The destination parent must already exist, remain under `private/`, and contain no reparse-point ancestor through the private root.
- The extractor executable must match SHA-256 `7C7AF9C17E095C3C1E78E644DF5F0E72F01C4690B3117F038AAFE26EB5A8A2F4`.
- `verify-source.ps1` must validate the ISO before extraction begins.
- Extraction occurs in a random `.extracting-<guid>` directory under `private/`.
- A nonzero extractor result or empty output fails the operation.
- The completed staging directory moves to the final path only after all pre-move checks pass.
- Failed staging data is removed only after its full path and generated name are revalidated.
- All `extract-xiso` options precede the ISO operand because its Windows `getopt` implementation stops option parsing at the first non-option argument.

The script supports `-WhatIf`; preflight still performs source/tool verification but does not create staging or destination data.

## Tests

Positive preflight against the supported ISO and `private/game` returned:

```text
Extracted       : False
PreflightPassed : True
DestinationPath : C:\BDU\MCLA-Recomp\private\game
SourceSha256    : AFCAAE68593246B1F83328681B690E254A13C37DD2E8DC34AB0DEB6C0F471FDB
```

The destination did not exist before or after the `-WhatIf` test.

Negative tests passed:

- `generated/game` was rejected because it is outside `private/`.
- an existing `private/existing-destination-test` directory was rejected before extraction.

The actual supported payload extraction and inventory validation are M1-010/M1-011, not part of this wrapper-only task.

The first full extraction exposed and closed an argument-order regression: `-x ISO -d staging` extracted to a default working-directory folder and then treated `-d` as another input. The wrapper now invokes `-q -x -d staging ISO`. The misplaced generated directory was validated as the exact 15-file/6,569,586,392-byte payload, removed as reproducible output, and the corrected staged extraction completed successfully.
