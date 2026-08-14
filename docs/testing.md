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

M4-003 treats playback, an evidence-justified development skip, and an
unpatched guest-selected bypass as distinct outcomes. Title reachability alone
is not playback proof. A bypass decision must combine private noisy guest-I/O
traces with physical title-frame verification and must show that every BIK
reference belongs to the project VFS preflight before module launch, while the
unmodified guest route opens no BIK after launch. The known skip word remains
absent unless repeated disabled runs first localize an actual Bink blocker.

```powershell
scripts\test-intro-route-decision.ps1
scripts\run-intro-route-decision.ps1
scripts\verify-intro-route-decision.ps1 -ResultPath <private-result.json>
```

The canonical bypass branch performs three serialized, isolated, unpatched
launches with noisy guest-I/O tracing, the M4-002 35-second title probe, a
two-second dwell, and exact-PID `WM_CLOSE`. Each cycle must physically pass the
title logo/`PRESS` ROI oracle, contain exactly the three project-owned
`intro720.bik` resolutions before module launch, contain zero Bink/BIK evidence
after launch, and retain successful `NtReadFile` positive controls near the
title marker. The verifier rehashes the accepted M4-002 result, canonical game,
four runtime artifacts, rotated logs, capture, and every evidence tree; it also
rejects reparse points, topology/privacy violations, patch drift, force cleanup,
orphans, and cross-cycle mutation. Raw noisy logs and BMPs remain private under
`private/evidence/M4-003/`.

M4-004 single-local-user profile gate:

```powershell
scripts\test-xam-profile-smoke.ps1
scripts\run-xam-profile-smoke.ps1
scripts\verify-xam-profile-smoke.ps1 -ResultPath <private-result.json>
```

The runner clean-installs RelWithDebInfo ReXGlue, executes the focused profile
defaults tests, clean-builds the app, and performs three isolated 35-second
title probes. The bounded XAM audit must prove slot 0 is locally signed in with
a stable nonzero mask-7 XUID and consistent name/sign-in information. Slots 1,
2, and 3 must each appear independently as signed out/no-such-user, with both
privacy-safe presence masks equal to hexadecimal `E`. Every cycle then passes
the existing physical title oracle, dwells for two seconds, and exits 0 through
exact-PID `WM_CLOSE` without force cleanup or an orphan.

The deterministic three voice setting defaults are covered by focused SDK unit
tests. The accepted title route does not call `ReadProfileSettings`, privilege
masks 251/252, or `SigninUI`; the gate validates such records fail-closed if
they become reachable but does not claim their runtime execution. Raw logs,
captures, and result JSON stay below ignored `private/evidence/M4-004/`.

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

## M4-005 physical SDL slot gate

```powershell
scripts\test-input-slot-smoke.ps1
scripts\run-input-slot-smoke.ps1
scripts\verify-input-slot-smoke.ps1 -ResultPath <private-result.json>
```

The interactive runner prints seven progress phases, clean-installs the pinned
SDK, runs the five focused SDL slot tests, clean-builds the app, and waits for
the verified title with exactly one pre-connected controller whose A button is
released. The operator holds A until the guest-down marker, then releases it. Acceptance
requires the causal SDL down/up source sequences to appear as guest-visible
slot-0 edges, slots 1-3 to remain disconnected, exact-PID `WM_CLOSE`, exit 0,
and complete post-run source-game/runtime-artifact/evidence integrity. Raw controller identity,
logs, capture, and result JSON remain private under `private/evidence/M4-005/`.

## M4-006 physical controller matrix

```powershell
scripts\test-controller-matrix.ps1
scripts\verify-controller-matrix.ps1 `
  -RecoveredHotplugEvidenceRun 20260813-144406-2c1974da `
  -RecoveredHotplugEvidenceOnly
```

The accepted gate combines three byte-bound physical layers: causal digital
controls and prior host-rumble commands, causal analog/focus coverage, and a
fresh hotplug-only continuation. The continuation proves physical removal,
guest disconnect, reconnect to slot 0, guest success, and controlled shutdown;
it replays no prior input and submits no nonzero rumble pulse. The final run is
classified through the recovered-evidence path because a stale `30025`
dispatch-count literal rejected the valid `30026` log after execution had
already completed. Recovery binds the original logs, BMP, build/test logs, and
evidence trees without inventing the absent post-run artifact snapshot.

The gate does not claim one monolithic process, controller identity, physical
multi-pad coverage, or title-driven force feedback. Raw evidence remains under
ignored `private/evidence/M4-006/`; see
`docs/evidence/M4-006-controller-matrix.md`.

## M4-007 frontend audio route

```powershell
scripts\test-audio-route-smoke.ps1
scripts\run-audio-route-smoke.ps1
scripts\verify-audio-route-smoke.ps1 -ResultPath <private-result.json>
```

The canonical gate prints seven phases, clean-installs the SDK, runs focused
audio classifiers, clean-builds the host, captures the verified title, and
observes autonomous frontend audio for 300 seconds. Acceptance requires finite
nonzero PCM at XMA, guest-submit, and SDL-device layers, zero failure/drop
counters, queue depth at most 64, and at most two consecutive post-start
starvation fills. Raw audio is never captured; logs, BMPs, and JSON remain
private under `private/evidence/M4-007/`.

## M4-008 XMP fallback gate

```powershell
scripts/test-xmp-route-smoke.ps1
scripts/run-xmp-route-smoke.ps1
scripts/verify-xmp-route-smoke.ps1 -ResultPath <private-result.json>
```

The canonical gate clean-installs the exact SDK tag, runs the four focused XMP
fallback tests, clean-builds the host, and captures the physically verified
title under trace logging. Acceptance requires one bounded idle-status record,
at least 1,000 known queries with exact call/query equality, and zero playback,
state-change, unexpected, inconsistent, overflow, or drop counters. The XMP
summary must follow the title capture and precede the project marker and exact
controlled lifecycle tail. The final verifier physically rehashes the complete
runtime-log manifest, BMP, cycle tree, build/test logs, canonical source-game
tree, and four runtime artifacts, and rejects reparse traversal or a surviving
canonical process. This proves only the reached metadata-only fallback; raw
logs, BMP, and JSON remain under ignored `private/evidence/M4-008/`.
## M4-009 offline-service title gate

Run the compact fail-closed parser fixtures with:

```powershell
scripts/test-offline-service-smoke.ps1
```

The canonical gate clean-builds ReXGlue and the host, runs the focused
offline-service tests, enables the init-only guest socket block and bounded
XLiveBase audit, waits for the verified title frame, then closes through the
exact PID/title window:

```powershell
scripts/run-offline-service-smoke.ps1
scripts/verify-offline-service-smoke.ps1 -ResultPath <private-result.json>
```

The accepted autonomous route has zero XLiveBase dispatches and zero socket
attempts. Treat this as proof that the explicit network-disabled policy does
not block the frontend, combined with unit-tested semantics for all ten known
MCLA XLiveBase message IDs. It is not runtime evidence that those zero-hit
messages were exercised, an Xbox Live implementation, or a general host
firewall. Raw logs and captures remain under ignored `private/evidence/M4-009/`.

## M4-010 locale and Unicode-path gate

```powershell
scripts/test-locale-path-smoke.ps1
scripts/run-locale-path-smoke.ps1
scripts/verify-locale-path-smoke.ps1 -ResultPath <private-result.json>
```

The autonomous gate performs one clean SDK/app build followed by isolated
EN/US, FR/FR, and RU/RU title runs. Each cycle binds the actual XConfig language
value returned to the guest, exact Cyrillic/accented user/cache/log directory
names, UTF-8 log parsing, four frozen GPU checkpoint summaries, the title logo,
and a pinned language-specific prompt ROI. The localized ROI uses normalized
edge correlation, so the animated title fade may change brightness without
allowing a missing or differently shaped prompt. Acceptance also requires
exact-PID `WM_CLOSE`, exit 0, no orphan, and unchanged source-game/runtime
artifacts. Raw logs, BMPs, localized visual references, and JSON remain ignored
under `private/`; public evidence contains only hashes and bounded metrics.

The accepted title route reaches XConfig language but not `XGetLanguage` or
country. Those zero-hit paths are unit/static contract evidence only. Complete
localized menus, subtitles, voice, and gameplay remain later parity scope.

## M4-011 saved frontend smoke gate

```powershell
scripts/test-frontend-smoke.ps1
scripts/run-frontend-smoke.ps1
scripts/verify-frontend-smoke.ps1 -ResultPath <private-result.json>
```

The canonical gate performs one clean ReXGlue install, runs the focused VFS
root tests, clean-builds the host, and executes three isolated cycles seeded
from the pinned private post-OOBE profile. The autonomous route is exact:
startup to the Complete Edition title, `START` to saved free-roam gameplay,
`START` to pause, `RB` to Modes, and a second `RB` to Settings with Options
highlighted. Synthetic input is restricted to slot 0, uses 200-ms holds, and
waits two seconds between the two tab presses so the guest UI debounce observes
both transitions.

Each cycle must produce four distinct nontrivial 1280x720 captures, pass the
existing title gate plus pinned pause/options region comparisons, preserve the
post-OOBE seed and source-game/runtime-artifact identities, and finish with
exit 0 after exact-window `WM_CLOSE`. The console title has no internal Exit
action; the gate deliberately tests external closure and does not claim an
in-game Exit command. Raw saves, frames, logs, references, and JSON remain
ignored below `private/`.

This proves repeatable navigation for the supported saved route only. It does
not cover first-run OOBE and its cutscenes/vehicle selection, race entry or
completion, detailed gameplay correctness, persistence writes, or frame/audio
parity; those remain later tasks.

## M5-001 canonical first-race route gate

```powershell
scripts/test-first-race-route.ps1
scripts/verify-first-race-route.ps1
```

The tracked schema `config/first-race-route.json` defines
`pinned-save-sunset-strip-race-v1`. The exact supported image, ReXGlue release,
stock 30 FPS D3D12 host-RTV configuration, two-file post-OOBE seed, three
private Xenia state references, white Nissan 240SX dry-night starting state,
one SDL controller in slot 0, default Xbox controls, Sunset Strip Race/Trevor
event identity, ordered transitions, and bounded timeouts are all exact.

The route uses `BACK` to open GPS and `Y` to challenge the selected opponent
with the default headlights action. It must start a two-car event and finish in position `1/2`. It must then show results, return to controllable free roam,
and close through exact-window external `WM_CLOSE`. Event identity is normative until a
native calibration observes it from the pinned seed; the verifier rejects any
attempt to mark it physically proven in M5-001.

The verifier rejects absolute/private paths in the public schema, reparse
traversal, physical seed/XEX/Xenia-frame drift, wrong image/runtime/state,
alternate events or controls, reordered actions, relaxed timeouts, second
place, omitted results/return, and claims of persistence or whole-frame parity.
Raw saves and frames remain ignored below `private/`.

## M4-012 frontend parity gate

```powershell
scripts/test-frontend-parity.ps1
scripts/run-frontend-parity.ps1
scripts/verify-frontend-parity.ps1 -ResultPath <private-result.json>
```

The gate re-verifies all three accepted 1280x720 M4-011 cycles, the pinned
five-minute M4-007 audio route, and three immutable stock-Xenia title,
free-roam, and pause frames. It then clean-builds and autonomously repeats the
saved title-to-gameplay-to-pause-to-Options route with native draw scale 2,
producing four 2560x1440 BMPs and a private comparison contact sheet. The
scale-2 route uses exact 45-second pre-input and gameplay waits to reject
loading-state captures.

Acceptance uses normalized edge correlation in stable logo, prompt, HUD, menu
footer, and Options regions. Pause permits only a bounded +/-8 by +/-2-pixel
translation to account for viewport anchoring. Animated backgrounds are not
whole-frame matched. Audio comparison is limited to the shared Xenia/native
decoder-worker-client lifecycle plus the physically verified native nonzero
sample route; the baseline contains no individual UI/music event identity, so
none is claimed. The final verifier binds clean-build output, rotated logs,
captures, contact sheet, prior results, source-game/save trees, runtime
artifacts, controlled external close, and exact process cleanup.

## M4-013 milestone closure gate

```powershell
scripts/test-frontend-smoke.ps1
scripts/run-frontend-smoke.ps1 -CycleCount 20 -MilestoneClosure
scripts/verify-frontend-smoke.ps1 -ResultPath <private-result.json> -MilestoneClosure
```

Closure mode keeps the exact saved title-to-gameplay-to-pause-to-Options route
but requires twenty consecutive isolated cycles after one clean SDK/app build.
It pins 45-second title/gameplay waits and a four-second pause-animation
settle, physically re-verifies every
rotated log set and four-frame capture set, uses the bounded pause-panel
registration established by M4-012, and rejects any force cleanup, orphan,
source-game/save mutation, runtime-artifact drift, fatal marker, or failed
cycle. The game is closed externally through exact-window `WM_CLOSE`; MCLA has
no internal Exit action, and the gate does not invent one.

If all twenty physical cycles and controlled exits completed but the original
runner stopped on a subsequently repaired verifier-only false negative, finalize
that exact pre-result root without replaying the title:

```powershell
scripts/run-frontend-smoke.ps1 -CycleCount 20 -MilestoneClosure `
  -FinalizeExistingClosureRun <run-id>
```

This recovery mode accepts only the exact four-child pre-result topology,
cycles `01` through `20`, the pinned focused-test totals, no live canonical
process, and the canonical seed/game/runtime identities. It reconstructs every
cycle record from physical logs, captures, and tree hashes, records that elapsed
stopwatch metrics are unavailable, and then runs the complete result verifier.
If final verification fails, it removes the provisional aggregate.

The accompanying M4-013 report audits every remaining frontend limitation by
severity, workaround, and target milestone. Closure is `GO M5 WITH PINNED
SAVE`: it supplies a deterministic saved free-roam prerequisite for M5, not a
clean-new-game, race-completion, persistence, or general-playability claim.
