# Testing

Owner: MCLA-R maintainers

Purpose: define test layers, required evidence, regression routes, supported environments, and milestone-specific quality gates.

Current structural checks:

```powershell
ast-grep scan
ast-grep test --skip-snapshot-tests
```

Current rule fixtures cover unbounded `strcpy`, `sprintf`/`vsprintf`, and `strcat` calls.

Current M1 environment/source gate:

```powershell
powershell.exe -NoLogo -NoProfile -ExecutionPolicy Bypass `
  -File C:\BDU\MCLA-Recomp\scripts\bootstrap.ps1
```

The gate must report all 12 prerequisites. Its normal route must pass 12/12, and a deliberately missing required-tool override must return nonzero after reporting the remaining checks. It includes full ISO and extracted-payload hashes.

Current stock Xenia baseline preflight:

```powershell
scripts\run-xenia-baseline.ps1 -WhatIf
```

For a real run, omit `-WhatIf` or supply a new `-BaselinePath` below `private/`. The launcher verifies the exact Xenia/XEX hashes, refuses overwrite/outside/reparse destinations, isolates storage/content/cache/log paths, disables patches and title updates, and keeps `time_scalar=1`. M2-001 requires private evidence that the run reaches active gameplay, not merely module load.

To verify save reload after the first Xenia process has exited, reuse the same isolated profile explicitly:

```powershell
scripts\run-xenia-baseline.ps1 `
  -BaselinePath private\baseline\M2-001 `
  -Resume `
  -WhatIf
```

Remove `-WhatIf` only after confirming that no Xenia process is using the profile. Resume mode requires an explicit existing baseline, revalidates the pinned Xenia/XEX identities, requires non-reparse storage/content/cache directories, and creates a uniquely named `xenia-resume-*.log` rather than overwriting the stock-session log. A save reload passes only when the resumed process finds the existing profile and the operator reaches the previously saved progress.

Current Xenia metadata export gate:

```powershell
scripts\export-xenia-baseline.ps1 -WhatIf
scripts\export-xenia-baseline.ps1
```

The exporter reads an ignored immutable log/config snapshot, requires the exact supported Xenia build and title identifiers, classifies every `w>`/`!>` event, and writes only whitelisted metadata below `docs/evidence`. Validation must cover deterministic regeneration, rejection of an unexpected module hash, rejection of an unknown warning class, rejection of an output path outside `docs/evidence`, and a scan proving that the generated report contains no absolute host path or profile identifier.

Current ReXGlue manifest gate:

```powershell
scripts\verify-rexglue-manifest.ps1
```

The exact-schema validator accepts only the pinned `mcla`/SDK identity, approved private game root and XEX, ignored generated output, and an empty first-analysis include list. It rejects absolute/backslash/traversal paths, unknown or duplicate TOML sections/keys, unsupported syntax, reparse traversal, and an unexpected XEX size/hash. Tests must include independent TOML parsing plus negative traversal, unknown-key, duplicate-key, and wrong-output fixtures.

Planned test layers:

1. host utility unit tests
2. ReXGlue/PPC tests when SDK code changes
3. structural analysis
4. startup smoke tests
5. frontend and first-race routes
6. save/load and soak tests
7. full campaign matrix
8. clean package tests

Update this document when any command, fixture, baseline, supported hardware configuration, canonical route, or closure gate changes.
