# M4-003 intro-route decision evidence

Date: 2026-08-12

Decision: PASS as `guest-bypass-no-patch`.

## Scope and claim boundary

The supported Complete Edition image reaches the title frontend through a
stable, unmodified guest-selected route that does not open or read a Bink file
after module launch. The known skip-intro candidate remains absent and disabled;
there is no demonstrated blocker that would justify adding it.

This result does not prove Bink decoding, video playback, audio playback, or
general cutscene correctness. Representative media fidelity and skip behavior
remain owned by M7-006.

## Automated result

- private run ID: `20260812-095920-a6979ac7`
- private result SHA-256:
  `10E0D3E8DE607568A7E27559742D522B6460E2679AAFA3CBE2851195B8D033EE`
- branch: `guest-bypass-no-patch`
- isolated unpatched title cycles: 3/3
- title capture time: 43,263-44,659 ms
- controlled exit: 289-514 ms; exit code 0 in all cycles
- project VFS `intro720.bik` resolutions before module launch: exactly 3 per cycle
- post-launch Bink/BIK references or guest file requests: 0 in every cycle
- post-launch `NtCreateFile` requests: 19 per cycle
- post-launch `NtReadFile` calls and successful results: 4,633-4,642 per cycle
- successful read controls near the physical title marker: 10-23 per cycle
- logo edge correlation: 0.919497-0.965254
- tight `PRESS` edge correlation: 0.920328-0.995872
- render resolves: 15,933-17,335 per cycle
- guest draws: 1,696,128-2,727,892 per cycle
- process cleanup: 3/3; no force cleanup or surviving exact-path process
- source identity: unchanged 15 files and 6,569,586,392 bytes
- executable/runtime/Tracy/Xenos hashes: unchanged from accepted M4-002
- prior-cycle and complete evidence trees: physically immutable

The accepted runtime artifact hashes remain:

- `mcla.exe`: `370CB77B61B515DDA3D06C3807CF7F8F6EBB496029F4F21990A17D43BA534B96`
- `rexruntimerd.dll`: `305A7B6EC2ED1D848A6ADC36B3EEB90C322272165FBE9353F1CB55E57460EB98`
- `TracyClientrd.dll`: `4B794F74709EDAA3A10AF2C96CF7F4855C548751D742A10DB307B8E88D16F57C`
- `rexgpu-xenosrd.dll`: `5654D3607CA8723CFDE709395D9CA44A7877F09B14F842B7FCC404B40015F36F`

Raw noisy logs, BMPs, guest handles/addresses, and result JSON remain ignored
and private.

## Verification

```powershell
scripts\test-intro-route-decision.ps1
scripts\run-intro-route-decision.ps1
scripts\verify-intro-route-decision.ps1 -ResultPath <private-result.json>
```

The fixture suite passes four physical positives, rejects eight fail-closed
negative cases, and passes fourteen source-contract checks. It binds rotated
logs and the physical title BMP, requires successful guest-I/O controls near
the title boundary, rejects post-launch Bink evidence, re-verifies accepted
M4-002 evidence, and fails on patch, topology, containment, reparse, process,
privacy, or source/artifact drift.

The final aggregate project suite passes 33/33 scripts. `ast-grep scan` is
clean, rule tests pass 3/3, and the read-only prerequisite bootstrap passes
12/12 checks. The independent final review found one pre-launch extra-BIK
fail-closed gap; exact complete-trace cardinality and a dedicated negative
fixture closed it before task completion, and re-review approved the result.
