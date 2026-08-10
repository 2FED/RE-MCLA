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

Current conservative first-analysis policy gate:

```powershell
scripts\verify-first-analysis-policy.ps1
scripts\test-first-analysis-policy.ps1
```

The gate requires all nine ReXGlue code-generation options to be explicitly `false`, verifies that the pinned SDK both parses those exact keys and retains false defaults, and rejects manual hint or analysis-tuning sections. Run it before every M2-008 non-force codegen attempt.

Current M2-008 non-force codegen evidence gates:

```powershell
scripts\run-rexglue-codegen-gate.ps1 -WhatIf
scripts\export-rexglue-codegen-gate.ps1
scripts\test-export-rexglue-codegen-gate.ps1
```

The one-shot runner refuses an existing private evidence root or non-empty generated target, revalidates the manifest and conservative policy, invokes only `rexglue codegen mcla_manifest.toml`, and records both streams plus exit metadata privately. The exporter is fail-closed: it requires the immutable stream hashes, exact non-force command/exit classification, known line grammar, six analysis phases, and seven `UnresolvedCall` details before publishing the full path/thread-sanitized transcript. Export tests require deterministic regeneration and rejection of force-enabled metadata, an unknown log line, and an output outside `docs/evidence`.

Current M2-009 force-inventory evidence gates:

```powershell
scripts\run-rexglue-force-inventory.ps1 -WhatIf
scripts\export-rexglue-force-inventory.ps1
scripts\test-export-rexglue-force-inventory.ps1
```

The one-shot force runner requires the immutable M2-008 rejection, a clean generated target, and the unchanged conservative manifest; it invokes exactly `rexglue --force codegen mcla_manifest.toml` into a separate private root. Its private manifest hashes every generated file. The exporter verifies that manifest against all current ignored outputs, inventories all validation/writer categories including explicit zero classes, and publishes a complete sanitized transcript. Tests require two deterministic exports and rejection of disabled-force metadata, unknown output, a changed generated count, and an outside output path.

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
