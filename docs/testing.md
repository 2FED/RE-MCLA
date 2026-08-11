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

The exact-schema validator accepts only the pinned `mcla`/SDK identity, approved private game root and XEX, ignored generated output, and the single reviewed `config/mcla_functions.toml` include. It rejects absolute/backslash/traversal paths, unknown or duplicate TOML sections/keys, unsupported syntax, reparse traversal, and an unexpected XEX size/hash. M2-008's immutable first-analysis input used an empty include list; the current include is the reviewed M2-012 transition.

Current conservative first-analysis policy gate:

```powershell
scripts\verify-first-analysis-policy.ps1
scripts\test-first-analysis-policy.ps1
```

The historical gate requires all nine ReXGlue code-generation options to be explicitly `false`, verifies that the pinned SDK both parses those exact keys and retains false defaults, and rejects non-empty includes plus manual tuning. Its test reconstructs the immutable empty-include M2-008 state; it is not a validator for the post-M2-012 manifest.

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

Current M2-010 finding-triage evidence gates:

```powershell
scripts\export-codegen-triage.ps1
scripts\test-export-codegen-triage.ps1
```

The exporter cross-checks the immutable M2-009 transcript against a private 34-address Ghidra audit produced by `scripts/ghidra/AddressAudit.java`. It requires all seven branch pairs and all twenty vector-pack sites, verifies executable-section and boundary context, decodes every pack word as format 5/mask 3/shift 0, and emits one owner/category/severity/action row per unique finding. Tests require deterministic regeneration and reject a missing audit record, changed pack fields, mutated force evidence, and output outside `docs/evidence`.

Current M2-011 control-flow/exception evidence gates:

```powershell
scripts\export-control-flow-audit.ps1
scripts\test-export-control-flow-audit.ps1
```

`scripts/ghidra/ControlFlowAudit.java` scans the private loaded image for exact save/restore and standard setjmp/longjmp signatures, decodes all 8-byte PDATA records, locates each unresolved source/target relative to PDATA, and follows every target to its first unconditional terminal. The exporter additionally verifies the complete immutable force-generated snapshot, generated source-function ownership, target absence from the dispatcher, the sole direct `RtlUnwind` caller, and intentionally unset jump overrides. Tests require deterministic output and reject missing/changed audit records or output outside `docs/evidence`.

Current M2-012 reviewed-config and deterministic codegen gates:

```powershell
scripts\materialize-m2-011-generated-snapshot.ps1 -Confirm:$false # once when the ignored historical snapshot is absent
scripts\verify-analysis-config.ps1
scripts\test-analysis-config.ps1
scripts\run-rexglue-config-iteration.ps1 -Iteration NN-label -ExpectedUnresolvedCount N -ExpectedExitCode N
scripts\export-manual-analysis-config.ps1
scripts\test-export-manual-analysis-config.ps1
```

The exact config validator accepts only the eight address/end/name records supported by M2-011/M2-012 evidence and requires the sole parent relation at `0x824B0DE8`. It rejects any unreviewed function, switch table, invalid region, exception hint, or jump override. The runner snapshots every manifest/config/log input and successful generated-file manifest under ignored private evidence. The exporter verifies all ten iterations, the discovered `0x822C9948` follow-up, zero final analysis errors, and two clean byte-identical 64-file manifests. The materializer reconstructs the immutable empty-include M2-009 generated snapshot and validates all 64 files against its retained size/SHA-256 manifest; historical M2-009/M2-011 exporters use that snapshot so later manual-analysis output cannot invalidate their regression tests.

M2-013 import coverage gate:

```powershell
scripts\export-import-coverage.ps1
scripts\test-export-import-coverage.ps1
```

The exporter accepts only the immutable M2-002 Xenia import audit and the byte-verified M2-012 generated snapshot. It maps all 503 XEX records to 257 library/ordinal symbols, verifies all 246 generated function thunk addresses, classifies 11 variable imports, and scans every generated translation unit for direct import calls. Its tests require deterministic output and reject a changed audit, changed generated manifest, or output outside `docs/evidence`.

M2-014 startup dependency gate:

```powershell
scripts\export-startup-import-set.ps1
scripts\test-export-startup-import-set.ps1
```

The startup exporter pins the accepted generated/import evidence, verifies the exact `xstart` call order and title-main boundary, computes the transitive pre-main call closure, and requires every resulting import registration plus all eleven variable mappings in the pinned SDK. It distinguishes load-time mappings, pre-main functions, error-path semantics, and post-main shutdown calls. Tests require deterministic output and reject changed coverage, missing generated input, or output outside `docs/evidence`.

M2-015 public-patch byte audit:

```powershell
scripts\export-xenia-patch-audit.ps1
scripts\test-export-xenia-patch-audit.ps1
```

`scripts/ghidra/PatchAudit.java` reads the eight reviewed addresses from the loaded private XEX image. The exporter pins the upstream Complete Edition TOML SHA, upstream commit, target XEX, and Ghidra TSV; requires the exact title/module/media identity; verifies original bytes, containing words, instruction context, replacements, and disabled-by-default state; and publishes no proprietary bytes beyond the small patch-site words needed for verification. Tests reject an enabled/mutated upstream file, changed original bytes, or output outside `docs/evidence`.

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
