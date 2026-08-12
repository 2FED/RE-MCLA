# M4-002 frontend render-path evidence

Date: 2026-08-12

Decision: PASS for the scoped D3D12 host-RTV title path.

## Scope and claim boundary

This task proves that the supported image autonomously reaches the Complete
Edition title frontend and exercises a bounded, internally balanced set of
D3D12 Xenos paths. It covers host RTV/DSV render targets, staged common-copy
EDRAM resolves, depth and 1x/2x/4x MSAA use, shader translation, successful PSO
creation, guest draw execution, and non-identity table-gamma dispatch.

It does not prove ROV/interlock rendering, PWL gamma, true-direct resolves,
whole-frame pixel equality, all frontend screens, intro correctness, menu
navigation, controller/profile/audio behavior, gameplay, or public packaging.

The accepted path uses ReXGlue SDK `0.9.0.10` at immutable commit
`f7e9df0e7923b13ae3e879d39dc827b9010a0ab9`. The SDK adds init-only schema-1
render auditing, serializes shader/PSO audit completion with checkpoint freeze,
and safely disables Tracy instrumentation before manual profiler startup.

## Automated result

- private run ID: `20260812-085022-cc01a857`
- private result SHA-256:
  `C879E46679AC1AAD9810E80D8EBDBAE0F50381FB2F2ED8038B256B16023C9ECB`
- clean build: RelWithDebInfo immediately before cycle 01
- clean configure and build time: 50,955 ms
- cycles and physical 1280x720 title captures: 10/10
- capture time: 42,246-46,973 ms
- controlled exit: 275-588 ms; exit code 0 in all cycles
- logo edge correlation: 0.918981-0.998869
- tight `PRESS` edge correlation: 0.920328-0.999956
- render targets: exactly 27 per cycle
- bound-format records: 39-40 per cycle
- ownership modes: exactly 40 per cycle
- successful PSOs: 117-217 per cycle; zero translation/PSO failures
- successful common-copy resolves: 7,597-18,153 per cycle
- guest draws: 1,199,122-3,085,569 per cycle
- non-identity table-gamma dispatches: 815-1,028 per cycle
- gamma uploads: at least 2 per cycle
- process cleanup: 10/10; no force cleanup or surviving exact-path process
- source identity: 15 files and 6,569,586,392 bytes; full manifest and tree
  identity physically unchanged
- executable/runtime/Tracy/Xenos identity: physically unchanged
- prior-cycle trees: unchanged after every later cycle
- rotated runtime logs physically verified: 172,506,414 aggregate bytes

The accepted runtime artifact SHA-256 values are:

- `mcla.exe`: `370CB77B61B515DDA3D06C3807CF7F8F6EBB496029F4F21990A17D43BA534B96`
- `rexruntimerd.dll`: `305A7B6EC2ED1D848A6ADC36B3EEB90C322272165FBE9353F1CB55E57460EB98`
- `TracyClientrd.dll`: `4B794F74709EDAA3A10AF2C96CF7F4855C548751D742A10DB307B8E88D16F57C`
- `rexgpu-xenosrd.dll`: `5654D3607CA8723CFDE709395D9CA44A7877F09B14F842B7FCC404B40015F36F`

Raw BMPs, logs, build logs, and result JSON remain ignored and private.

## Human classification

- reviewer: project owner
- classification date: 2026-08-12
- result: PASS — recognizable Complete Edition title/frontend, correctly
  oriented, readable logo and `PRESS`, with no obvious global corruption,
  severe tint/gamma banding, broken whole-frame compositing/depth, or
  catastrophic aliasing
- noted deviation: a vehicle near the lower-left edge has a green-tinted
  shadow; the owner classified it as minor and non-blocking at this stage

The shadow is tracked as KI-013 for M4-012. The visual PASS is not a claim of
whole-frame equivalence.

## Verification

```powershell
scripts\test-sdk-profiling-lifetime.ps1
scripts\test-render-path-smoke.ps1
scripts\run-render-path-smoke.ps1
scripts\verify-render-path-smoke.ps1 -ResultPath <private-result.json>
```

The final fixture suite passes six positive routes and rejects 53 fail-closed
negative cases. Coverage includes native/emulated 2x MSAA, BIND-to-RT
cross-links, direct/fallback resolve accounting, concurrent record ordering,
checkpoint freeze, rotated logs, integer-exact BMP metrics, title regions,
lifecycle/process cleanup, source/artifact drift, containment, privacy, and
reparse points.

The full project script suite passes 32/32, ast-grep scan is clean, and rule
tests pass 3/3. The focused ReXGlue `[offline]` suite passes 3/3 cases and 10/10
assertions without the prior Tracy access violation. Remaining upstream unit
baseline failures are pre-existing template/migration and FunctionDispatcher
memory groups outside this task's change scope.
