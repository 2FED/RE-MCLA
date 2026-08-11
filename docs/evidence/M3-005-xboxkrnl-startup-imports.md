# M3-005 xboxkrnl startup import matrix

Date: 2026-08-11
Result: PASS — NO PROJECT-OWNED XBOXKRNL STUB REQUIRED

## Boundary and method

The reviewed boundary is unchanged from M2-014: module entry `xstart` at `0x821322B8` through the first call to title main at `sub_821305E8`. All observations use the exact Complete Edition XEX and isolated private user/cache roots.

The RelWithDebInfo executable was launched under CDB with breakpoints on all 22 xboxkrnl functions in the conservative pre-main set. Every breakpoint writes a name-only marker and continues. A breakpoint on `sub_821305E8` writes one boundary marker and terminates the private debugger run before title-main behavior can contaminate the startup matrix. The verifier requires:

- exactly one title-main boundary marker;
- all eleven load-time variable patch records;
- the exact ten-function stock pre-main path;
- no unknown startup import marker;
- no fatal, invalid-function, or `PPC_UNIMPLEMENTED` marker before the boundary.

Reproduction:

```powershell
.\scripts\test-xboxkrnl-startup-import-trace.ps1
.\scripts\run-xboxkrnl-startup-import-trace.ps1
```

Final private run: `private/evidence/M3-005/20260811-115645-bf7db151-cdb`

## Load-time variable imports

The runtime patched every required xboxkrnl variable exactly once before guest entry.

| Variable | Result |
| --- | --- |
| `ExThreadObjectType` | patched |
| `KeDebugMonitorData` | patched |
| `KeTimeStampBundle` | patched |
| `XboxHardwareInfo` | patched |
| `XboxKrnlVersion` | patched |
| `XexExecutableModuleHandle` | patched |
| `ExLoadedCommandLine` | patched |
| `VdGlobalDevice` | patched |
| `VdGpuClockInMHz` | patched |
| `VdHSIOCalibrationLock` | patched |
| `KeCertMonitorData` | patched |

## Function-import trace matrix

Hit counts are diagnostic, not timing assertions: critical-section counts may vary slightly under the debugger. The verifier asserts the exact unique function set and a positive hit count for each required stock-path function.

| Import | Stock pre-main classification | Observed hits |
| --- | --- | ---: |
| `ExGetXConfigSetting` | reached | 3 |
| `HalReturnToFirmware` | terminating/error branch, not reached | 0 |
| `KeBugCheckEx` | terminating/error branch, not reached | 0 |
| `KeGetCurrentProcessType` | reached | 1 |
| `KeTlsAlloc` | reached | 2 |
| `KeTlsFree` | conditional cleanup, not reached | 0 |
| `KeTlsGetValue` | conditional TLS path, not reached | 0 |
| `KeTlsSetValue` | reached | 2 |
| `NtAllocateVirtualMemory` | reached | 4 |
| `NtClose` | conditional event cleanup, not reached | 0 |
| `NtCreateEvent` | conditional error-dialog path, not reached | 0 |
| `NtFreeVirtualMemory` | conditional cleanup, not reached | 0 |
| `NtQueryVirtualMemory` | conditional allocator query, not reached | 0 |
| `NtWaitForSingleObjectEx` | conditional wait path, not reached | 0 |
| `RtlCompareMemoryUlong` | conditional allocator check, not reached | 0 |
| `RtlEnterCriticalSection` | reached | 355 |
| `RtlImageXexHeaderField` | reached | 1 |
| `RtlInitializeCriticalSection` | reached | 43 |
| `RtlLeaveCriticalSection` | reached | 355 |
| `RtlNtStatusToDosError` | conditional failure conversion, not reached | 0 |
| `RtlRaiseException` | exception branch, not reached | 0 |
| `XexCheckExecutablePrivilege` | reached | 1 |

Final verified run totals: 22 functions reviewed, 10 unique stock-path functions reached, 767 calls observed, and one title-main boundary. No xboxkrnl registration or project-owned semantic stub was missing. The twelve zero-hit entries remain in the conservative envelope because their static branches are real; they are not minimum requirements for the observed successful stock route.

## Guest control-flow prerequisite repair

The first real launch initially failed before import execution on a sequence of valid indirect guest targets omitted by static GapFill. Runtime-guided, non-force regeneration added twelve bounded callable entries:

`827A7FD0`, `827A8220`, `827AD168`, `827AFC78`, `827B0538`, `827B0558`, `827B0578`, `827B0598`, `827B1048`, `827B1068`, `827B4B58`, and `827B4B78`.

Each generated body was reviewed as a bounded tail or leaf; the exact intervals are recorded in `docs/reverse-engineering.md` and enforced by `scripts/verify-analysis-config.ps1`. Mapping count increased from 30,008 to 30,020. After the final pair, a bounded 15-second launch reached archive I/O, XInput/FFB ordinal resolution, XAM notification setup, and video initialization without another invalid-function trap.

Two current non-force codegen manifests are byte-identical:

- files: 64
- bytes: 128,038,099
- manifest SHA-256: `88E5C11E5A239972E074B2AC23F9042A1E9B570A9E7030BEDCD578FFD5DAC346`

This repair is control-flow metadata, not an xboxkrnl behavior implementation. A sanitized upstream report may be prepared after a full duplicate audit, per project policy.

## Evidence identity

For final private run `20260811-115645-bf7db151-cdb`:

- executable SHA-256: `BFDB60385D889A350038E618B2CB134A2BC49632DEB9C0C2AACAF2848243B106`
- CDB transcript SHA-256: `4006CF27BE58BB9C3FE931E12969A1D677BB59A14FBBE506111D84FD5E0823DE`
- runtime log SHA-256: `2A33F97049CA6B1D444455BFA62985E845AE0B8D3502ECBAD6EA832469B3DFB3`

M3-005 acceptance: PASS. Every project-required stock-path xboxkrnl function has a debugger trace comparison, all load-time variables are proven mapped, conditional/error paths are explicitly separated, and no missing kernel stub is hidden by the successful launch.
