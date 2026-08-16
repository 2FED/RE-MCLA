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

## M5-002 world-streaming RPF gate

```powershell
scripts/test-world-streaming-smoke.ps1
scripts/run-world-streaming-smoke.ps1
scripts/verify-world-streaming-smoke.ps1 -ResultPath <private-result.json>
```

The runner clean-builds the host, copies the exact pinned post-OOBE save into
an isolated user root, enables private noisy kernel I/O tracing, and executes
the established title to saved-gameplay to pause to Options route. It then
closes the game externally by exact PID/window `WM_CLOSE`; there is no in-game
Exit command to test or claim.

Before guest launch the host must resolve lowercase, uppercase, and deliberately
mixed-case spellings of `xarchive_cache.rpf` through both `game:` and `D:` to
the same VFS entry. The physical trace must contain exactly two successful
opens for the cache and audlo RPFs, at least 4,000/10 successful reads,
100,000,000/50,000 bytes read, and at least 95% highest-end coverage. Zero-length
guest requests are legal; a positive-length request returning zero, any failed
read, implicit offset, overrun, missing completion, or archive open failure is
rejected.

Exactly five railyard and two exposition-park requests may fail under the
absent retail `t:\mc4\art\city` development device. Every other guest open
failure is rejected. Raw noisy logs contain guest handles and private paths and
remain ignored. The sanitized result stores only counts, coverage, relative
evidence names, hashes, and booleans and rebinds the complete source-game tree,
pinned save, four runtime artifacts, rotated logs, and four frontend captures.

## M5-003 rendering-category gate

```powershell
scripts/test-rendering-smoke.ps1
scripts/run-rendering-smoke.ps1
scripts/verify-rendering-smoke.ps1 -ResultPath <private-result.json>
```

The runner clean-builds the host and enters the pinned post-OOBE saved free-roam
route with the D3D12 host-RTV path, draw scale 1, synchronous shader creation,
and the bounded gameplay render audit enabled. The default-off InitOnly probe
captures a stationary world frame, thirty one-second traffic samples, a raised
camera/sky frame, a returned street frame, and three burnout frames. Six
allowlisted `A` pulses dismiss phone/tutorial overlays every five seconds; every
source/guest down/up transition is logged, ordered, and required, so a paused
overlay cannot masquerade as active traffic or particles.

The verifier chooses the traffic sample with the largest change only within a
road ROI, binds all thirty samples rather than hiding the remainder, and requires
independent temporal floors for camera movement and the two particle intervals.
It also requires a gameplay-timed frozen Xenos checkpoint with successful
PSO/draw/depth/MSAA/gamma/render-target/ownership/resolve activity and zero
translation, PSO, binding, render-target, resolve, refresh, device-loss, or fatal
failure. Shader detail records intentionally saturate the privacy cap at 256;
the complete aggregate counters remain balanced and are separately bound.

The runner creates a private eight-tile Xenia/native contact sheet and leaves a
pending candidate. After the owner reviews the labeled native frames, finalize
that exact immutable run without replaying it:

```powershell
scripts/run-rendering-smoke.ps1 `
  -FinalizeExistingRun <run-id> -VisualPass
```

Finalization re-parses all logs and frames, hashes the complete cycle tree,
source game, pinned seed, four runtime artifacts, build log, executable, and
contact sheet, and records the owner verdict independently of automated image
metrics. Acceptance covers recognizable/usable road, buildings, player vehicle,
AI traffic, night sky, shadows, particles, and HUD in this bounded night
free-roam slice. It does not claim whole-frame Xenia parity, all locations,
weather/daylight, ROV/PWL/true-direct paths, or complete post-processing quality.

## M5-004 EDRAM/depth validation report

```powershell
scripts/test-edram-depth-report.ps1
scripts/run-edram-depth-report.ps1
scripts/verify-edram-depth-report.ps1 `
  -ResultPath <private-result.json>
```

This is a report gate, not a second gameplay launch. It physically re-verifies
the immutable accepted M5-003 result and then re-parses its complete rotated log
set. Acceptance requires the D3D12 host-RTV path, all ownership mode IDs `0..7`,
exact native 1x/2x/4x sample mapping, depth bindings and depth resolves at all
three sample counts, more than 500,000 ownership draws, more than 10,000
host-depth-store dispatches, and balanced successful resolve summaries with no
render-target, binding, resolve, device-loss, fatal, or snapshot-route marker.

The source contract verifies that host depth is stored into EDRAM before the
subsequent ownership-transfer phase and that the external snapshot-restoration
entry remains confined to the trace player. The accepted gameplay evidence has
1,599 draws with guest depth/stencil state but no host depth target; the report
records and caps this count instead of treating it as a failed binding or hiding
it. Owner review is cited only for the eight M5-003 rendering categories.

The result is deliberately `accepted-s2-bounded-host-rtv`: it says no target
defect was reproduced on the reached host-RTV gameplay path and therefore no
behavior patch is justified. It does not claim that ROV/interlock rendering,
trace-player EDRAM snapshot restoration, PWL gamma, or true-direct resolve was
executed or fixed. Those paths remain later diagnostic/coverage work if a
canonical route actually depends on them.

## M5-005 representative material-pipeline report

```powershell
scripts/test-material-pipeline-report.ps1
scripts/run-material-pipeline-report.ps1
scripts/verify-material-pipeline-report.ps1 `
  -ResultPath <private-result.json>
```

This report gate does not launch the title again. It physically re-verifies the
immutable accepted M5-003 route and re-parses its complete rotated log set. The
shader gate requires at least 150 successful vertex translations, 190 pixel
translations, 300 successful PSOs, zero translation/PSO failure, 256 unique
bounded shader records, explicit nonzero shader-record overflow, and unique
successful PSO descriptions. Record overflow is preserved as a bounded-evidence
fact; it is not presented as complete enumeration of every translated shader.

Only successful `Loaded` texture events count toward coverage. Production floors
require 100,000 successful loads, 100,000 tiled loads, at least one linear load,
30,000 packed-mip loads, 80,000 unpacked-mip loads, all nine observed
representative formats, and at least 40 dimension classes. Any invalid fetch,
texture creation/load failure, Xenos audit failure, fatal marker, or D3D12 device
loss fails the report. The source contract additionally verifies that the common
`Loaded` marker follows successful load completion and that the D3D12 tiled path
orders compute dispatch, texture copy, and successful return.

Raw texture logs include guest addresses and remain private. The sanitized result
contains only aggregate counts and format names. The accepted owner visual pass
is inherited only for the eight labeled M5-003 categories: road, buildings,
player vehicle, traffic, night sky, shadows, particles, and HUD. The report does
not claim every guest texture format, every material/location/weather condition,
raw texture correctness in isolation, or whole-frame Xenia equivalence.

## M5-006 saved-gameplay input gate

```powershell
scripts/test-gameplay-input-smoke.ps1
scripts/run-gameplay-input-smoke.ps1
scripts/verify-gameplay-input-smoke.ps1 `
  -ResultPath <private-result.json>
```

The runner clean-builds the current host and autonomously loads the pinned
post-OOBE save. No controller or operator input is required for this current-run
probe. A private synthetic slot-0 driver sends and causally observes title
Start, full throttle, full brake, full-left and full-right steering under
partial throttle, neutral releases, and gameplay Start/pause. Six slow A pulses
clear startup overlays without depending on one exact overlay count. The route
captures neutral, active/released throttle and brake, left/right steering, and
pause frames before closing the exact game window externally with `WM_CLOSE`.

Acceptance requires exactly 24 ordered gameplay source/guest records, 24 ordered
overlay-dismiss records, eight canonical 1280x720 BMPs, at least 20,000 sampled
pixel differences for neutral-to-throttle, throttle-to-brake, and left-to-right
steering, and at least 500,000-ppm edge correlation against the pinned pause UI
ROI. The accepted pause value is 613,358 ppm; a calibrated non-pause gameplay
frame is only 46,465 ppm. Any guest crash, unimplemented PPC call, fatal marker,
device loss, input failure, forced cleanup, orphan process, save mutation, game
drift, artifact drift, malformed log topology, or evidence reparse point fails
the gate.

The final verifier separately re-runs the immutable M4-006 recovered hotplug
gate and source-compares the accepted ReXGlue `v0.9.0.13` input implementation
with current `v0.9.0.20`. This binds the current autonomous gameplay response to
the previously observed physical SDL digital, analog, focus, disconnect, and
reconnect route without pretending that synthetic input is a new physical-pad
test. The result covers one default-layout controller in saved free roam. It
does not claim race maneuver parity, multi-pad policy, title-driven rumble, or
force feedback; M5-007 owns the latter.

## M5-007 force-feedback degradation report

```powershell
scripts/test-force-feedback-report.ps1
scripts/run-force-feedback-report.ps1
scripts/verify-force-feedback-report.ps1 `
  -ResultPath <private-result.json>
```

This is a report gate and does not launch the title or vibrate a controller. It
re-verifies the accepted M5-006 saved-gameplay result and the immutable M4-006
physical controller chain, then audits the current SDK source. Acceptance
requires exactly eight `XInputdFF*` stub exports, no advertised
`X_INPUT_CAPS_FFB_SUPPORTED` flag, a concrete `XamInputSetState` delegation,
concrete SDL rumble submission/result mapping, and a visible
`DEVICE_NOT_CONNECTED` result when no physical controller is present.

The current gameplay log must contain all eight exact module-resolution records
for ordinals `0282..0289`, zero `XInputdFF* STUB` call markers, one gameplay
input PASS summary, one controlled execution-complete marker, and no crash,
unimplemented, fatal, or device-loss marker. The physical baseline must contain
six exact successful/supported host commands in LEFT/stop, RIGHT/stop,
BOTH/stop order and must re-prove physical disconnect plus reconnect. The
owner's earlier report of feeling those patterns is preserved only as an
external user report; it is neither claimed as recorded in the run nor as
independently machine-verified.

The accepted decision is deliberately bounded: advanced guest force-feedback
effects are unavailable but not advertised, so they cannot block the saved
gameplay route. Basic `XamInputSetState`/SDL rumble is concrete and physically
bounded by the host diagnostic. The report does not claim title-driven FFB,
multi-pad rumble policy, or a new physical vibration test.

## M5-008 stock physics-timing gate

```powershell
scripts/test-physics-timing-smoke.ps1
scripts/run-physics-timing-smoke.ps1
scripts/verify-physics-timing-smoke.ps1 `
  -ResultPath <private-result.json>
```

This gate clean-builds the current SDK and host in optimized `Release`, then
runs three isolated saved-gameplay cycles. A default-off, InitOnly diagnostic
wraps the existing stock timer function at guest address `0x821BDA90` without
patching guest code. After the normal title-to-gameplay route, each cycle
captures a ten-second full-throttle window and records the stock effective,
clamped, and raw timing values together with the 50-MHz guest clock, guest
vblank count, and successful guest-output publication sequence.

Acceptance requires 294-306 timer calls and output frames over the bounded
window, exact 33,333-microsecond effective and clamped steps, raw host deltas in
the 25-75 ms diagnostic band, a 59.4-60.6 Hz guest-vblank rate, a 29.4-30.6 FPS
output rate, and simulated-time/wall plus guest-clock/wall ratios within the
pinned ppm bounds. Each start/end frame pair must be canonical 1280x720 BGRA
and visibly non-identical. Every cycle must exit 0 through exact external
`WM_CLOSE`, with unchanged source-game, save, and runtime-artifact identities.

`RelWithDebInfo` is explicitly not a performance baseline: profiling and debug
instrumentation reduce this same route to roughly 20 FPS on the accepted host.
It remains useful for correctness diagnostics. The accepted stock-speed claim
is therefore limited to the optimized Release configuration, the pinned save,
and the observed free-roam route; it does not claim arbitrary host hardware,
race logic, traffic density, or unlocked/high-refresh behavior.

## M5-009 six-class audio-event gate

```powershell
scripts/test-audio-event-smoke.ps1
scripts/run-audio-event-smoke.ps1
scripts/verify-audio-event-smoke.ps1 `
  -ResultPath <private-result.json>
```

The runner clean-installs exact ReXGlue v0.9.0.20, requires the focused eight
audio-audit cases and 30 assertions, clean-builds RelWithDebInfo, and copies the
pinned post-OOBE save into isolated user/cache roots. Its synthetic route emits
status lines for fixed music, ambient, voice, engine, collision, and UI
listening windows. The operator judges only whether the named class is present;
other simultaneous sounds are explicitly permitted.

Machine acceptance requires the six allowlisted windows exactly once and in
order, calibrated device-frame duration floors, at least 900,000 ppm nonzero
output in every window, bounded per-class peaks, zero invalid frames or submit
failures, and a healthy current XMA/XAudio/SDL summary. After external
exact-window `WM_CLOSE`, exit 0, and process cleanup, the runner accepts only the
exact confirmation `PASS MUSIC AMBIENT VOICE ENGINE COLLISION UI`.

The final verifier reopens the physical logs and canonical 1280x720 title BMP,
rehashes the source-game manifest, pinned save/header, four runtime artifacts,
three build/test logs, and complete contained non-reparse evidence tree. Raw PCM
and audio-device identity are never captured. The result proves bounded event-
class presence, not isolation, mix balance, exact asset identity, spatial
fidelity, exhaustive coverage, XMP decoding, or long-session/device-transition
stability.

## M5-010 route-failure report

```powershell
scripts/test-route-failure-report.ps1
scripts/run-route-failure-report.ps1
scripts/verify-route-failure-report.ps1 `
  -ResultPath <private-result.json>
```

This report physically re-verifies the immutable M5-002 noisy world-streaming
route, all three M5-008 stock-speed gameplay cycles, and the longer M5-009
audio/gameplay route. The five-process union must preserve five controlled
external exits, all accepted result/evidence-tree hashes, and the current
source-game/save/runtime identities.

The noisy trace must contain 8,054 successful reads plus one valid asynchronous
pending read with a successful IOSB, 87/87 virtual allocations, 49/49 frees,
2,590/2,590 physical allocations, and no failed result. Every process may
contain exactly the seven known retail development `.loc` misses; any other
open failure is rejected. The complete log union rejects fatal allocation,
archive, streaming, guest-crash, assertion, unregistered-function, and
device-loss markers.

Acceptance means no target defect was observed in this bounded route, so a
behavior patch is not justified. It does not claim long-session leak freedom,
failure-injection coverage, all-region streaming, or repeated complete races.

## M5-011 save/content persistence contract

```powershell
scripts/test-save-content-contract.ps1
scripts/run-save-content-contract.ps1
scripts/verify-save-content-contract.ps1 -ResultPath <private-result.json>
```

The gate clean-builds and installs exact ReXGlue v0.9.0.21, runs the focused
`[system][xam][content]` suite, and binds the current implementation to the
immutable M5-002 existing-save route. The physical prior route proves content
enumeration, writable `save0:` mount/open, successful reads, and unmount. The
focused test writes saved-game metadata, destroys its content manager,
constructs a new manager, reads and enumerates the same metadata, and rejects a
truncated header.

Source acceptance also requires `XamContentCreateEx` to propagate a header
write failure, close the mount, delete the incomplete package, and return an
unknown disposition. `WriteContentHeaderFile` must check both data writes and
close, remove a partial header, and return access denied on failure.

This is the required persistence API prerequisite. It deliberately records
`race_result_write_physically_reached=false`: a real race-result write and its
survival across a fresh process remain M5-012 and must not be inferred from the
focused metadata roundtrip.

## M5-012 race-results and Release-restart gate

```powershell
scripts/test-race-results-smoke.ps1
scripts/test-race-restart-smoke.ps1
scripts/verify-race-results-smoke.ps1 `
  -RunPath <private-completed-route-cycle>
scripts/verify-race-restart-smoke.ps1 `
  -ResultPath <private-restart-result.json>
```

The physical route runner is intentionally a one-series evidence collector. An
operator confirms the two-car start, the final results/rewards state after every
`NEXT RACE` event in that series, and return to controllable free roam. Each
confirmation creates a request in the isolated user root; the guest-output
probe consumes it, captures one 1280x720 frame, and records an ordered present
sequence. Invalid console input is ignored without touching the game.

The final acceptance gate is the separate optimized Release restart. It
cryptographically binds the completed-route evidence tree and exact completed
save, copies that save into a fresh user root, autonomously reaches controllable
gameplay, and records the stock fixed-step sample: approximately 300 presents
and 600 vblanks over ten seconds, with simulated time tracking wall time. Both
processes must close externally through exact-window `WM_CLOSE` with no fatal,
unregistered-function, assertion, device-loss, or forced-cleanup marker.

This closes one complete Ian event series, results-to-free-roam transition, and
fresh-process save load. It does not claim a fixed series length, whole-frame
parity, five repeated races, or bounded resource growth; the latter repeated
race/resource work remains M5-013.

## M5-013 repeated-race resource-growth gate

```powershell
scripts/test-race-resource-smoke.ps1
scripts/run-race-resource-smoke.ps1
scripts/verify-race-resource-smoke.ps1 `
  -ResultPath <private-result.json>
```

The runner clean-builds Release, copies the exact completed M5-012 save into an
isolated user root, and keeps one process alive across five real completed race
events. At title/gameplay it records a baseline; after each exact operator
confirmation it waits five seconds, consumes a numbered user-root request,
captures one private guest frame, and records the median of three host process
samples. Metrics are private bytes, working set, handles, host threads, and the
Windows `GPU Process Memory` dedicated/shared counters for the exact PID.
Transient counter samples with nonzero `Status` are ignored before reading
`CookedValue`; the runner retries for up to ten seconds until both exact-PID
counter classes are valid. Its focused self-test covers valid aggregation,
invalid status, wrong PID, and incomplete-pair retry classification.
The runner rewrites the bounded sample file after the baseline and every
checkpoint so a later failed transition retains partial diagnostics; the
verifier still accepts only the complete ordered six-sample set.

The bounded-growth comparison uses race 1 through race 5, excluding normal
title-to-gameplay warm-up. Limits are 512 MiB private memory, 512 MiB working
set, 128 handles, 16 threads, 512 MiB dedicated GPU memory, and 256 MiB shared
GPU memory. Both final and maximum sampled growth must stay inside those
limits. All five frame markers and captures, six numeric samples, the exact
completed M5-012 seed save/header, the shape and hashes of the possibly updated
isolated working save/header, runtime logs, Release artifacts, controlled
external close, and evidence tree are reverified. Acceptance is a five-race
regression bound, not a general or lifetime leak-freedom claim.

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
