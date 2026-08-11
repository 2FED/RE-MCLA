# M2-014 startup import and stub set

Date: 2026-08-11
Result: FINITE STARTUP SET; NO MISSING SDK REGISTRATION

## Boundary and evidence

- Module entry: `xstart` at `0x821322B8`
- Title-main boundary: first `xstart` call to `sub_821305E8`
- Accepted generated-manifest SHA-256: `F8A97618215848CA3B2279725DE77D8AF556E6B2E02AF1CA66D51A812E60F933`
- M2-013 import matrix SHA-256: `7E761FAF92A6559B83C166ED1493E601BF588D3F97812F1D3536EFCD535BC96D`
- SDK: ReXGlue v0.9.0 at pinned commit `3eb9b511b4140d2769e27be63eae57d41bfa2afa`

The host runtime resolves variable imports before invoking `xstart`. The early guest envelope is then bounded at the first transition into title main, so post-main shutdown calls such as `DbgPrint` are not mislabeled startup requirements.

## Load-time minimum

All eleven variable imports must be mapped before guest entry. The pinned SDK contains all eleven mappings.

| Ordinal dec/hex | Variable | IAT slot | SDK state |
| --- | --- | --- | --- |
| `27 / 0x01B` | `ExThreadObjectType` | `0x820009E4` | mapped before guest entry |
| `89 / 0x059` | `KeDebugMonitorData` | `0x820007E8` | mapped before guest entry |
| `173 / 0x0AD` | `KeTimeStampBundle` | `0x82000968` | mapped before guest entry |
| `342 / 0x156` | `XboxHardwareInfo` | `0x8200096C` | mapped before guest entry |
| `344 / 0x158` | `XboxKrnlVersion` | `0x8200089C` | mapped before guest entry |
| `403 / 0x193` | `XexExecutableModuleHandle` | `0x820007B0` | mapped before guest entry |
| `430 / 0x1AE` | `ExLoadedCommandLine` | `0x820009F4` | mapped before guest entry |
| `446 / 0x1BE` | `VdGlobalDevice` | `0x82000864` | mapped before guest entry |
| `448 / 0x1C0` | `VdGpuClockInMHz` | `0x820008C8` | mapped before guest entry |
| `449 / 0x1C1` | `VdHSIOCalibrationLock` | `0x820008DC` | mapped before guest entry |
| `614 / 0x266` | `KeCertMonitorData` | `0x82000880` | mapped before guest entry |

## Pre-main guest envelope

- Internal functions in bounded transitive closure: **59**
- Maximum static call depth from the seven direct initializer roots: **6**
- Function imports in conservative pre-main set: **26**
- Missing ReXGlue registrations: **0**
- Explicit semantic stubs in this set: **1** (`XamShowMessageBoxUIEx`, error path)

| Library | Ordinal dec/hex | Import | Static owners (`function@depth`) | Xenia status | ReXGlue status |
| --- | --- | --- | --- | --- | --- |
| `xboxkrnl.exe` | `16 / 0x010` | `ExGetXConfigSetting` | sub_821320D0@1 | implemented | concrete registration |
| `xboxkrnl.exe` | `40 / 0x028` | `HalReturnToFirmware` | sub_82132A48@1 | implemented | concrete registration |
| `xboxkrnl.exe` | `83 / 0x053` | `KeBugCheckEx` | sub_82134860@4, sub_82135150@5 | implemented | concrete registration |
| `xboxkrnl.exe` | `102 / 0x066` | `KeGetCurrentProcessType` | sub_821342B8@3, sub_82134860@4, sub_82135150@5 | implemented | concrete registration |
| `xboxkrnl.exe` | `338 / 0x152` | `KeTlsAlloc` | sub_823DB480@1 | implemented | concrete registration |
| `xboxkrnl.exe` | `339 / 0x153` | `KeTlsFree` | sub_823DB190@2 | implemented | concrete registration |
| `xboxkrnl.exe` | `340 / 0x154` | `KeTlsGetValue` | sub_823DB138@5 | implemented | concrete registration |
| `xboxkrnl.exe` | `341 / 0x155` | `KeTlsSetValue` | sub_823DB138@5, sub_823DB480@1 | implemented | concrete registration |
| `xboxkrnl.exe` | `204 / 0x0CC` | `NtAllocateVirtualMemory` | sub_82132AA0@6, sub_82132D80@6, sub_82133BA8@4, sub_82133D10@5, sub_821342B8@3, sub_82134860@4 | implemented | concrete registration |
| `xboxkrnl.exe` | `207 / 0x0CF` | `NtClose` | sub_82131F90@2 | implemented | concrete registration |
| `xboxkrnl.exe` | `209 / 0x0D1` | `NtCreateEvent` | sub_82131F90@2 | implemented | concrete registration |
| `xboxkrnl.exe` | `220 / 0x0DC` | `NtFreeVirtualMemory` | sub_82132AA0@6, sub_82133D10@5, sub_82133F50@6, sub_821342B8@3, sub_82135150@5 | implemented | concrete registration |
| `xboxkrnl.exe` | `238 / 0x0EE` | `NtQueryVirtualMemory` | sub_821342B8@3 | implemented | concrete registration |
| `xboxkrnl.exe` | `253 / 0x0FD` | `NtWaitForSingleObjectEx` | sub_82135DC0@5 | implemented | concrete registration |
| `xboxkrnl.exe` | `283 / 0x11B` | `RtlCompareMemoryUlong` | sub_82133010@6 | implemented | concrete registration |
| `xboxkrnl.exe` | `293 / 0x125` | `RtlEnterCriticalSection` | sub_82132898@1, sub_82132900@2, sub_82134860@4, sub_82135150@5 | implemented | concrete registration |
| `xboxkrnl.exe` | `299 / 0x12B` | `RtlImageXexHeaderField` | sub_82132970@2 | implemented | concrete registration |
| `xboxkrnl.exe` | `302 / 0x12E` | `RtlInitializeCriticalSection` | sub_821342B8@3 | implemented | concrete registration |
| `xboxkrnl.exe` | `304 / 0x130` | `RtlLeaveCriticalSection` | sub_82132898@1, sub_82132900@2, sub_82134860@4, sub_821350FC@5, sub_82135150@5, sub_821353F0@6 | implemented | concrete registration |
| `xboxkrnl.exe` | `309 / 0x135` | `RtlNtStatusToDosError` | sub_82135D58@6 | implemented | concrete registration |
| `xboxkrnl.exe` | `310 / 0x136` | `RtlRaiseException` | sub_82134860@4 | implemented | concrete registration |
| `xam.xex` | `425 / 0x1A9` | `XamLoaderTerminateTitle` | xstart@0 | implemented | concrete registration |
| `xam.xex` | `732 / 0x2DC` | `XamShowMessageBoxUIEx` | sub_82131F90@2 | implemented | semantic stub (error path) |
| `xboxkrnl.exe` | `404 / 0x194` | `XexCheckExecutablePrivilege` | sub_821320D0@1 | implemented | concrete registration |
| `xam.xex` | `971 / 0x3CB` | `XGetAVPack` | sub_821320D0@1 | implemented | concrete registration |
| `xam.xex` | `973 / 0x3CD` | `XGetLanguage` | sub_821320D0@1 | implemented | concrete registration |

## Minimum project work before title main

- New project-owned function stubs required: **0**.
- New project-owned variable mappings required: **0**.
- Preserve the SDK warning/zero-return behavior of `XamShowMessageBoxUIEx` for now; it is reachable only through the bounded error-dialog branch and the pinned stock route reaches gameplay without taking it.
- Treat `HalReturnToFirmware`, `KeBugCheck*`, `RtlRaiseException`, and `XamLoaderTerminateTitle` as terminating/error paths, not successful startup markers.
- M3 must still build and smoke-test the real native runtime; static availability does not prove behavior parity.

M2-014 acceptance: PASS. Load-time mappings and the finite pre-main import envelope are explicit, and no missing SDK registration blocks entry into title main.
