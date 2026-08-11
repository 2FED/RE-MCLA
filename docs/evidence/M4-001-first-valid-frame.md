# M4-001 first valid frame evidence

Date: 2026-08-11

Decision: PASS.

## Scope and claim boundary

This task proves that the supported local game image produces guest-originated
graphics work that reaches successful D3D12 presentation and a nontrivial
guest-output readback. It does not prove frontend reachability, pixel-accurate
rendering, intro correctness, gameplay, input, audio correctness, or public
package readiness.

The accepted path uses ReXGlue SDK `0.9.0.9` at immutable commit
`01f895afb1b084652915c0b463910d57049f06be`. The SDK assigns monotonic guest
output sequences, emits bounded count-1/count-3 markers only after successful
guest-backed DXGI `Present`, publishes the present watermark after the marker,
and holds mailbox ownership through capture readback. Captures ahead of the
published successful-present watermark fail silently and retry.

## Automated result

- private run ID: `20260811-190657-eae1665b`
- private result SHA-256:
  `2AB3992CB8F39F97C81390A7EFB58C25FD77EA0D1657B464D68728AE565DAB7E`
- clean build: RelWithDebInfo immediately before cycle 01
- cycles: 20/20 passed
- minimum successful guest-backed presents: 3 per cycle
- physical captures: 20/20 independently parsed and nontrivial
- capture dimensions: 1280x720 in all cycles
- capture/present watermark equality: 20/20
- first-frame time: 9,785-10,117 ms; average 9,807 ms
- controlled exit time: 160-198 ms; average 179.4 ms
- process cleanup: 20/20; no force cleanup and no surviving owned process
- source identity: 15 files, 6,569,586,392 bytes, one directory; full tree and
  manifest identity unchanged
- runtime artifact identity: executable, runtime, Tracy, and Xenos hashes
  unchanged across all cycles
- prior-cycle evidence trees: unchanged after every later cycle
- unexpected post-hard-exit lines: zero

Cycle 01 capture SHA-256 is
`5F55F7CEAD3ACE323517F1A745238FFBA0A847D4901529086AA227116D1038EA`.
Its independently recomputed 1280x720 metrics are 35 occupied RGB555 bins,
luma p05/p95 0/69, modal share 913 per mille, and 77 occupied nonmodal cells
in the 16x9 grid. The raw BMP, runtime logs, build log, and result remain under
the ignored private evidence root and are not distributed.

## Human classification

- reviewer: project owner
- classification date: 2026-08-11
- capture: `mcla-first-frame.bmp`
- capture SHA-256:
  `5F55F7CEAD3ACE323517F1A745238FFBA0A847D4901529086AA227116D1038EA`
- result: PASS — recognizable MCLA-derived legal screen, correctly oriented,
  with no host-only blank/solid or obvious whole-frame garbage

The required classification is limited to whether the private capture is
recognizable game-derived output, correctly oriented, and not host-only blank,
solid, or obvious whole-frame garbage.

## Verification

```powershell
scripts\test-first-frame-smoke.ps1
scripts\run-first-frame-smoke.ps1
scripts\verify-first-frame-smoke.ps1 -ResultPath <private-result.json>
```

The fixture suite passes one physical positive and rejects 37 negative cases,
including invalid sequence/order/HRESULT, forged metrics, corrupt or trivial
BMPs, missing/extra evidence, process/cleanup failures, binary or game-tree
drift, directory-only mutation, path leakage, and a reparse-point case.

The ReXGlue `[ui]` subset passes 12/12 tests with 31 assertions. A separate
full non-Release SDK test invocation exposed a deterministic pre-runtime Tracy
manual-lifetime access violation in profiled export wrappers. It is unrelated
to the M4 presenter path, is tracked as KI-012, and was reported upstream as
[rexglue/rexglue-sdk#410](https://github.com/rexglue/rexglue-sdk/issues/410);
this evidence does not claim the full upstream SDK suite is clean.
