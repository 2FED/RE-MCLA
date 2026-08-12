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

M2-016 ReXGlue SDK regression gate:

```powershell
cmake --preset win-amd64 -S third_party/rexglue-sdk -B third_party/rexglue-sdk/out/build/win-amd64-tests -DREXGLUE_BUILD_TESTS=ON -DREXGLUE_ENABLE_TRACY=OFF -DREXGLUE_ENABLE_PERF_COUNTERS=OFF
cmake --build third_party/rexglue-sdk/out/build/win-amd64-tests --config Release --target ppc_tests --parallel
third_party/rexglue-sdk/out/win-amd64/Release/ppc_tests.exe "*vpkd3d128*"
third_party/rexglue-sdk/out/win-amd64/Release/ppc_tests.exe
scripts/run-rexglue-vector-regression.ps1 -Confirm:$false
```

The SDK test tree must be configured with `REXGLUE_BUILD_TESTS=ON`. The focused test covers the project-fork mask-3 opcode, the full 166-file PPC corpus catches codegen regressions, and the project runner requires zero MCLA FLOAT16_4 warnings plus byte parity for all 64 accepted generated files. Raw logs and generated game code remain ignored under `private/evidence/M2-016`.

M3-001 generated-source/native-build integration gate:

```powershell
scripts\test-generated-integration.ps1
scripts\verify-generated-integration.ps1
cmake --preset win-amd64-release
cmake --build --preset win-amd64-release --target mcla --parallel
scripts\verify-generated-integration.ps1 -BuildRoot out\build\win-amd64-release
```

The verifier now requires the current M3-013 67-file/133,908,410-byte manifest, an exact unique 65-source `sources.cmake` list, repository containment, no reparse points, zero tracked generated files, Git-ignore coverage for every output, 65 compiled generated objects, and a PE executable. M3-001 retains the historical 64-file/62-source baseline in its evidence. Fixture tests reject a changed hash, an unlisted file, a wrong total-byte count, a duplicate manifest path, and a mismatched source list; the snapshot helper additionally rejects overwrite, outside-private output, and an empty generated root.

M3-009 privacy-safe crash-report gate:

```powershell
scripts\verify-crash-report-contract.ps1
scripts\test-crash-report-contract.ps1
scripts\test-crash-report.ps1
scripts\run-crash-report-smoke.ps1
```

The SDK source contract requires exception-aware guest function scopes, function
and basic-block PC breadcrumbs, typed/raw/stub import breadcrumbs, the `XThread`
C++ exception boundary, a 16-frame host-stack bound, and default guest-memory
exclusion. Its fixture suite passes one positive case and rejects a missing
exception boundary, enabled guest memory, missing generated PC tracking, and a
missing raw-hook breadcrumb. The report verifier requires the exact schema and
synthetic identifiers, contiguous bounded host frames, ordered lifecycle markers,
and no guest execution. It rejects missing guest PC, enabled guest memory, private
host paths, and a mismatched frame count. Raw reports/logs remain ignored below
`private/evidence/M3-009/`.

M3-010 structured-logging gate:

```powershell
scripts\verify-logging-contract.ps1
scripts\test-logging-contract.ps1
scripts\test-logging-schema.ps1
scripts\run-logging-smoke.ps1
```

The source contract requires all nine exact category registrations, nine
`inherit`-by-default allow-listed overrides, an off-by-default schema probe,
categorized operational calls, and CMake integration. Its fixtures reject a
missing category, enabled probe default, non-inheriting override default,
missing override application, and a generic logger regression. Log fixtures
reject the wrong category, duplicate schema markers, private paths, and an error
level. The live gate performs nine lifecycle-only runs with global logging off
and exactly one selected category at `info`; each log is capped at 64 KiB and
remains under ignored `private/evidence/M3-010/`.

M3-011 conditional skip-intro decision gate:

```powershell
scripts\verify-skip-intro-decision.ps1
scripts\test-intro-blocker-trace.ps1
scripts\run-intro-blocker-smoke.ps1
```

The static decision gate requires zero project skip-intro implementation while
retaining the disabled M2-015 byte audit. The trace test passes one current
classification and rejects missing launch, missing GPU prerequisite, post-launch
Bink evidence, and guest crash evidence. The live runner expects the unpatched
process to remain alive for the bounded observation, then forcibly cleans it up
and stores only private raw logs/result metadata below `private/evidence/M3-011/`.

M3-012 clean build-matrix gate:

```powershell
scripts\test-build-matrix.ps1
scripts\run-build-matrix.ps1
scripts\verify-build-matrix.ps1 -ResultPath <private-result.json>
```

The manifest fixture passes one exact three-configuration result and rejects a
wrong task, missing configuration, cross-configuration GPU DLL, failed build,
invalid hash, and wrong generated-object count. The live gate independently
configures and clean-builds Debug, RelWithDebInfo, and Release, requires 65
generated objects in each tree, validates the PE and artifact hashes, and
rejects stale runtime/Tracy/Xenos variants from the other configurations. Raw
configure/build logs remain ignored below `private/evidence/M3-012/`.

M3-013 post-GPU startup-trap gate:

```powershell
scripts\verify-analysis-config.ps1
scripts\test-startup-trap-trace.ps1
scripts\run-startup-trap-smoke.ps1
```

The trace fixture passes one ordered project-default Xenos route and rejects
seven regressions: missing selection, no-GPU fallback, invalid function,
`PPC_UNIMPLEMENTED`, guest crash, missing pipeline progress, and post-launch
Bink evidence. The live runner must remain alive for the full 20-second window,
reach graphics and audio work, then be forcibly cleaned with no surviving
process. Raw logs/results remain ignored below `private/evidence/M3-013/`.

M3-014 canonical startup-smoke gate:

```powershell
scripts\test-startup-smoke.ps1
scripts\run-startup-smoke.ps1
scripts\verify-startup-smoke.ps1 -RuntimeLogPath <private-log>
scripts\verify-startup-smoke-result.ps1 -ResultPath <private-result.json> -RuntimeLogPath <private-log>
```

The static/fixture gate requires the exact project source identity contract,
passes complete log and result objects, rejects thirteen marker/failure
mutations, eight result-integrity/termination mutations, and one corrupt XEX
before process creation. The live runner accepts only the supported XEX hash,
requires exact Title ID, Media ID, loaded image range, entry, read-only VFS,
module, graphics, and audio evidence within the bounded deadline, then requires
its owned PID to terminate and signal exit within five seconds. It re-verifies
the post-exit log before binding its exact bytes/hash into the result. Raw
logs/results remain ignored below `private/evidence/M3-014/`.

M3-015 repeated launch/exit gate:

```powershell
scripts\test-launch-exit-cycles.ps1
scripts\run-launch-exit-cycles.ps1
scripts\verify-launch-exit-cycles.ps1 -ResultPath <private-result.json>
```

The final runner always performs a RelWithDebInfo clean build, then ten crash
probes followed by ten consecutive canonical normal startups. Crash probes
must write the privacy-safe report and shutdown marker and signal exit 0;
normal cycles must reach all M3-014 markers, accept `WM_CLOSE`, write exactly
one ordered window-close/hard-exit tail, and signal exit 0. Force cleanup,
surviving exact-path processes, game or binary drift, prior-cycle tree
mutation, and aggregate path leakage all fail closed. A reduced `-CycleCount`
is development-only and can never satisfy the result verifier. Raw evidence
remains ignored below `private/evidence/M3-015/`.

M3-002 application-lifecycle gate:

```powershell
scripts\test-app-lifecycle.ps1
scripts\run-app-lifecycle-smoke.ps1
```

The static regression check keeps the native entry point attached to the
project-owned `MclaApp` subclass and verifies the deliberately small hook/probe
contract. The real smoke follows the SDK windowed-app lifecycle without
constructing or launching the guest runtime and passes only on exit code 0 with
the four startup/probe/shutdown log markers in order. Raw logs stay below
ignored `private/evidence/M3-002/`.

M3-003 module-configuration gate:

```powershell
scripts\test-module-config.ps1
scripts\verify-module-config.ps1
scripts\run-module-config-smoke.ps1
```

The static verifier requires the exact image/code/dispatch ranges, a bounded
ordered 30,008-entry function map, its final sentinel, and exactly one mapping
for the executable entry. Five negative fixtures exercise fail-closed behavior.
The real probe constructs Runtime, loads the private XEX, verifies the loaded
base/entry and dispatcher range, then exits before guest-thread creation. Raw
evidence and redirected user/cache paths remain below ignored
`private/evidence/M3-003/`.

M3-004 VFS disc-root and containment gate:

```powershell
scripts\test-vfs-policy.ps1
scripts\run-vfs-smoke.ps1
scripts\run-vfs-smoke.ps1 -BuildRoot out\build\win-amd64-relwithdebinfo
scripts\verify-game-manifest.ps1 -VerifyHashes
third_party\rexglue-sdk\out\win-amd64\Debug\unit_tests.exe "[filesystem]"
```

The native probe resolves representative XEX, BIK, and RPF files through
`game:`, `d:`, and the physical guest device; rejects alias and physical-device
root traversal; and requires access denied for write/create plus failure for
delete and writable mapping. The wrapper proves the 15-file/6,569,586,392-byte
host snapshot and XEX hash are unchanged. The SDK fixture uses only a temporary
directory and additionally covers rename, read opens, and read-only mappings.
Raw runtime logs remain below ignored `private/evidence/M3-004/`.

M4-001 first-valid-frame gate:

```powershell
scripts\test-first-frame-smoke.ps1
scripts\run-first-frame-smoke.ps1
scripts\verify-first-frame-smoke.ps1 -ResultPath <private-result.json>
```

The canonical runner performs one clean RelWithDebInfo build followed by twenty
serialized launches with isolated user/cache roots. Every cycle requires a
guest-originated active output, count-1 and count-3 successful guest-backed
DXGI presents with `SUCCEEDED` HRESULTs, a capture at or behind the published
successful-present watermark, an independently nontrivial 32-bpp BMP, a
two-second post-marker dwell, and controlled `WM_CLOSE` exit. The runner finds
exactly one visible window owned by its exact PID whose title matches the
versioned MCLA window contract; it never closes a helper or unrelated process.

The physical verifier reopens the sibling evidence tree, reruns the lower M3
startup verifier, hashes and parses every log/BMP, recomputes integer-exact
RGB555/luma/modal/grid metrics, rejects missing/extra/reparse artifacts and
private-path leakage, and binds all aggregate values to physical evidence. It
also requires unchanged exact runtime artifacts, complete source-game file and
directory topology, and all previously completed cycle trees. Force cleanup,
device loss, failed presentation, an ahead-of-present capture, orphan process,
or any data drift fails acceptance. Raw frames/logs remain ignored below
`private/evidence/M4-001/`; tracked evidence contains only bounded hashes,
counts, timings, and the human classification result.

M4-002 title render-path gate:

```powershell
scripts\test-sdk-profiling-lifetime.ps1
scripts\test-render-path-smoke.ps1
scripts\run-render-path-smoke.ps1
scripts\verify-render-path-smoke.ps1 -ResultPath <private-result.json>
```

The canonical runner clean-builds RelWithDebInfo, then performs ten serialized
launches with isolated user/cache roots, a 35-second settle, a two-second
checkpoint dwell, and exact-PID `WM_CLOSE`. It forces D3D12 host RTV and disables
asynchronous shader compilation. Each cycle must produce a 1280x720 title frame
whose stable logo and tight `PRESS` regions correlate at least 0.90 with the
pinned private Xenia reference.

The schema-1 audit records bounded RT/BIND/ownership, staged resolve,
shader/PSO, draw/depth/MSAA, and gamma state. Four GPU-thread summaries freeze
the audit before exit. The physical verifier recomputes BMP metrics and ROI
correlations, cross-links BIND tuples to RT records, balances resolve routes,
parses rotated logs, rehashes the canonical game and runtime artifacts, rejects
reparse/private/extra evidence, and confirms no exact-path process remains.
Raw evidence stays under ignored `private/evidence/M4-002/`.

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
