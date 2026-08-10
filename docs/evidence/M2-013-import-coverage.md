# M2-013 import symbol and coverage matrix

Date: 2026-08-11
Result: ALL XAM/XBOXKRNL IMPORT RECORDS SYMBOLICALLY MAPPED

## Evidence identity

- Private Xenia audit SHA-256: `95573DE737058A8E9A71B776A6E0A3851379FB26AF1E28A160FFA0B037EE3DE0`
- Accepted generated-manifest SHA-256: `F8A97618215848CA3B2279725DE77D8AF556E6B2E02AF1CA66D51A812E60F933`
- Source XEX: verified Complete Edition dump; requested/minimum import version `2.0.7371.0`

## Coverage summary

| Library | XEX records | Unique symbols | Functions | Variables | Symbol known | Xenia implemented | Direct generated calls |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| `xam.xex` | 190 | 95 | 95 | 0 | 95/95 | 86/95 | 95/95 |
| `xboxkrnl.exe` | 313 | 162 | 151 | 11 | 162/162 | 149/162 | 145/151 |
| **Total** | **503** | **257** | **246** | **11** | **257/257** | **235/257** | **240/246** |

The generated corpus contains **1517** direct import call sites. Six function imports have zero direct generated calls and are explicitly classified as indirect-only; eleven variable imports have no callable thunk by design. This is static coverage, not an assertion that every direct caller executes during startup. M2-014 owns entry-point startup reachability and the minimum stub set.

## Full matrix

| Library | Type | Ordinal dec/hex | Symbol | IAT slot | Function thunk | Xenia status | Static reachability |
| --- | :---: | --- | --- | --- | --- | --- | --- |
| `xam.xex` | F | `977 / 0x3D1` | `XGetVideoMode` | `0x82000600` | `0x827BCD84` | implemented | direct (7 sites) |
| `xam.xex` | F | `700 / 0x2BC` | `XamShowSigninUI` | `0x82000604` | `0x827BCD94` | implemented | direct (1 sites) |
| `xam.xex` | F | `703 / 0x2BF` | `XamShowFriendsUI` | `0x82000608` | `0x827BCDA4` | unimplemented | direct (1 sites) |
| `xam.xex` | F | `725 / 0x2D5` | `XamShowGamerCardUIForXUID` | `0x8200060C` | `0x827BCDB4` | unimplemented | direct (1 sites) |
| `xam.xex` | F | `709 / 0x2C5` | `XamShowAchievementsUI` | `0x82000610` | `0x827BCDC4` | implemented | direct (1 sites) |
| `xam.xex` | F | `710 / 0x2C6` | `XamShowPlayerReviewUI` | `0x82000614` | `0x827BCDD4` | unimplemented | direct (1 sites) |
| `xam.xex` | F | `711 / 0x2C7` | `XamShowMarketplaceUI` | `0x82000618` | `0x827BCDE4` | implemented | direct (1 sites) |
| `xam.xex` | F | `715 / 0x2CB` | `XamShowDeviceSelectorUI` | `0x8200061C` | `0x827BCDF4` | implemented | direct (1 sites) |
| `xam.xex` | F | `729 / 0x2D9` | `XamShowDirtyDiscErrorUI` | `0x82000620` | `0x827BCE04` | implemented | direct (1 sites) |
| `xam.xex` | F | `732 / 0x2DC` | `XamShowMessageBoxUIEx` | `0x82000624` | `0x827BCE14` | implemented | direct (1 sites) |
| `xam.xex` | F | `973 / 0x3CD` | `XGetLanguage` | `0x82000628` | `0x827BCE24` | implemented | direct (1 sites) |
| `xam.xex` | F | `971 / 0x3CB` | `XGetAVPack` | `0x8200062C` | `0x827BCE34` | implemented | direct (1 sites) |
| `xam.xex` | F | `425 / 0x1A9` | `XamLoaderTerminateTitle` | `0x82000630` | `0x827BCE44` | implemented | direct (2 sites) |
| `xam.xex` | F | `420 / 0x1A4` | `XamLoaderLaunchTitle` | `0x82000634` | `0x827BCE54` | implemented | direct (1 sites) |
| `xam.xex` | F | `551 / 0x227` | `XamUserGetSigninInfo` | `0x82000638` | `0x827BDC94` | implemented | direct (1 sites) |
| `xam.xex` | F | `408 / 0x198` | `XamInputGetKeystrokeEx` | `0x8200063C` | `0x827BDC84` | implemented | direct (1 sites) |
| `xam.xex` | F | `402 / 0x192` | `XamInputSetState` | `0x82000640` | `0x827BDC74` | implemented | direct (1 sites) |
| `xam.xex` | F | `401 / 0x191` | `XamInputGetState` | `0x82000644` | `0x827BDC64` | implemented | direct (1 sites) |
| `xam.xex` | F | `400 / 0x190` | `XamInputGetCapabilities` | `0x82000648` | `0x827BDC54` | implemented | direct (2 sites) |
| `xam.xex` | F | `780 / 0x30C` | `XamVoiceCreate` | `0x8200064C` | `0x827BDC44` | implemented | direct (1 sites) |
| `xam.xex` | F | `782 / 0x30E` | `XamVoiceSubmitPacket` | `0x82000650` | `0x827BDC34` | implemented | direct (2 sites) |
| `xam.xex` | F | `504 / 0x1F8` | `XMsgCancelIORequest` | `0x82000654` | `0x827BDC24` | implemented | direct (3 sites) |
| `xam.xex` | F | `783 / 0x30F` | `XamVoiceClose` | `0x82000658` | `0x827BDC14` | implemented | direct (3 sites) |
| `xam.xex` | F | `781 / 0x30D` | `XamVoiceHeadsetPresent` | `0x8200065C` | `0x827BDC04` | implemented | direct (1 sites) |
| `xam.xex` | F | `790 / 0x316` | `XamSessionCreateHandle` | `0x82000660` | `0x827BDBF4` | implemented | direct (1 sites) |
| `xam.xex` | F | `791 / 0x317` | `XamSessionRefObjByHandle` | `0x82000664` | `0x827BDBE4` | implemented | direct (14 sites) |
| `xam.xex` | F | `640 / 0x280` | `XamGetExecutionId` | `0x82000668` | `0x827BD484` | implemented | direct (1 sites) |
| `xam.xex` | F | `750 / 0x2EE` | `XamUserCreateAchievementEnumerator` | `0x8200066C` | `0x827BD474` | implemented | direct (1 sites) |
| `xam.xex` | F | `752 / 0x2F0` | `XamWriteGamerTile` | `0x82000670` | `0x827BD464` | implemented | direct (1 sites) |
| `xam.xex` | F | `778 / 0x30A` | `XamParseGamerTileKey` | `0x82000674` | `0x827BD454` | implemented | direct (1 sites) |
| `xam.xex` | F | `757 / 0x2F5` | `XamReadTileToTexture` | `0x82000678` | `0x827BD444` | implemented | direct (1 sites) |
| `xam.xex` | F | `759 / 0x2F7` | `XamUserCreateStatsEnumerator` | `0x8200067C` | `0x827BD434` | implemented | direct (2 sites) |
| `xam.xex` | F | `530 / 0x212` | `XamUserCheckPrivilege` | `0x82000680` | `0x827BD424` | implemented | direct (3 sites) |
| `xam.xex` | F | `531 / 0x213` | `XamUserAreUsersFriends` | `0x82000684` | `0x827BD414` | implemented | direct (2 sites) |
| `xam.xex` | F | `526 / 0x20E` | `XamUserGetName` | `0x82000688` | `0x827BD404` | implemented | direct (1 sites) |
| `xam.xex` | F | `650 / 0x28A` | `XamNotifyCreateListener` | `0x8200068C` | `0x827BD3F4` | implemented | direct (2 sites) |
| `xam.xex` | F | `592 / 0x250` | `XamEnumerate` | `0x82000690` | `0x827BD3E4` | implemented | direct (1 sites) |
| `xam.xex` | F | `606 / 0x25E` | `XamContentGetDeviceData` | `0x82000694` | `0x827BD3D4` | implemented | direct (1 sites) |
| `xam.xex` | F | `613 / 0x265` | `XamContentGetDeviceState` | `0x82000698` | `0x827BD3C4` | implemented | direct (2 sites) |
| `xam.xex` | F | `604 / 0x25C` | `XamContentCreateEnumerator` | `0x8200069C` | `0x827BD3B4` | implemented | direct (1 sites) |
| `xam.xex` | F | `602 / 0x25A` | `XamContentClose` | `0x820006A0` | `0x827BD3A4` | implemented | direct (1 sites) |
| `xam.xex` | F | `601 / 0x259` | `XamContentCreateEx` | `0x820006A4` | `0x827BD394` | implemented | direct (1 sites) |
| `xam.xex` | F | `431 / 0x1AF` | `XamTaskSchedule` | `0x820006A8` | `0x827BD384` | implemented | direct (1 sites) |
| `xam.xex` | F | `433 / 0x1B1` | `XamTaskCloseHandle` | `0x820006AC` | `0x827BD374` | implemented | direct (1 sites) |
| `xam.xex` | F | `435 / 0x1B3` | `XamTaskShouldExit` | `0x820006B0` | `0x827BD364` | implemented | direct (1 sites) |
| `xam.xex` | F | `422 / 0x1A6` | `XamLoaderSetLaunchData` | `0x820006B4` | `0x827BD044` | implemented | direct (1 sites) |
| `xam.xex` | F | `651 / 0x28B` | `XNotifyGetNext` | `0x820006B8` | `0x827BD054` | implemented | direct (9 sites) |
| `xam.xex` | F | `424 / 0x1A8` | `XamLoaderGetLaunchData` | `0x820006BC` | `0x827BD064` | implemented | direct (1 sites) |
| `xam.xex` | F | `423 / 0x1A7` | `XamLoaderGetLaunchDataSize` | `0x820006C0` | `0x827BD074` | implemented | direct (1 sites) |
| `xam.xex` | F | `972 / 0x3CC` | `XGetGameRegion` | `0x820006C4` | `0x827BD084` | implemented | direct (5 sites) |
| `xam.xex` | F | `590 / 0x24E` | `XamCreateEnumeratorHandle` | `0x820006C8` | `0x827BD094` | implemented | direct (2 sites) |
| `xam.xex` | F | `508 / 0x1FC` | `XMsgStartIORequestEx` | `0x820006CC` | `0x827BD0A4` | implemented | direct (1 sites) |
| `xam.xex` | F | `503 / 0x1F7` | `XMsgStartIORequest` | `0x820006D0` | `0x827BD0B4` | implemented | direct (29 sites) |
| `xam.xex` | F | `642 / 0x282` | `XamGetSystemVersion` | `0x820006D4` | `0x827BD0C4` | implemented | direct (8 sites) |
| `xam.xex` | F | `500 / 0x1F4` | `XMsgInProcessCall` | `0x820006D8` | `0x827BD0D4` | implemented | direct (8 sites) |
| `xam.xex` | F | `591 / 0x24F` | `XamGetPrivateEnumStructureFromHandle` | `0x820006DC` | `0x827BD0E4` | implemented | direct (2 sites) |
| `xam.xex` | F | `1 / 0x001` | `NetDll_WSAStartup` | `0x820006E0` | `0x827BD0F4` | implemented | direct (1 sites) |
| `xam.xex` | F | `2 / 0x002` | `NetDll_WSACleanup` | `0x820006E4` | `0x827BD104` | implemented | direct (1 sites) |
| `xam.xex` | F | `3 / 0x003` | `NetDll_socket` | `0x820006E8` | `0x827BD114` | implemented | direct (1 sites) |
| `xam.xex` | F | `4 / 0x004` | `NetDll_closesocket` | `0x820006EC` | `0x827BD124` | implemented | direct (1 sites) |
| `xam.xex` | F | `6 / 0x006` | `NetDll_ioctlsocket` | `0x820006F0` | `0x827BD134` | implemented | direct (1 sites) |
| `xam.xex` | F | `7 / 0x007` | `NetDll_setsockopt` | `0x820006F4` | `0x827BD144` | implemented | direct (1 sites) |
| `xam.xex` | F | `9 / 0x009` | `NetDll_getsockname` | `0x820006F8` | `0x827BD154` | implemented | direct (1 sites) |
| `xam.xex` | F | `11 / 0x00B` | `NetDll_bind` | `0x820006FC` | `0x827BD164` | implemented | direct (1 sites) |
| `xam.xex` | F | `12 / 0x00C` | `NetDll_connect` | `0x82000700` | `0x827BD174` | implemented | direct (1 sites) |
| `xam.xex` | F | `13 / 0x00D` | `NetDll_listen` | `0x82000704` | `0x827BD184` | implemented | direct (1 sites) |
| `xam.xex` | F | `14 / 0x00E` | `NetDll_accept` | `0x82000708` | `0x827BD194` | implemented | direct (1 sites) |
| `xam.xex` | F | `15 / 0x00F` | `NetDll_select` | `0x8200070C` | `0x827BD1A4` | implemented | direct (1 sites) |
| `xam.xex` | F | `18 / 0x012` | `NetDll_recv` | `0x82000710` | `0x827BD1B4` | implemented | direct (1 sites) |
| `xam.xex` | F | `20 / 0x014` | `NetDll_recvfrom` | `0x82000714` | `0x827BD1C4` | implemented | direct (1 sites) |
| `xam.xex` | F | `22 / 0x016` | `NetDll_send` | `0x82000718` | `0x827BD1D4` | implemented | direct (1 sites) |
| `xam.xex` | F | `24 / 0x018` | `NetDll_sendto` | `0x8200071C` | `0x827BD1E4` | implemented | direct (1 sites) |
| `xam.xex` | F | `26 / 0x01A` | `NetDll_inet_addr` | `0x82000720` | `0x827BD1F4` | implemented | direct (1 sites) |
| `xam.xex` | F | `27 / 0x01B` | `NetDll_WSAGetLastError` | `0x82000724` | `0x827BD204` | implemented | direct (1 sites) |
| `xam.xex` | F | `34 / 0x022` | `NetDll___WSAFDIsSet` | `0x82000728` | `0x827BD214` | implemented | direct (1 sites) |
| `xam.xex` | F | `51 / 0x033` | `NetDll_XNetStartup` | `0x8200072C` | `0x827BD224` | implemented | direct (1 sites) |
| `xam.xex` | F | `52 / 0x034` | `NetDll_XNetCleanup` | `0x82000730` | `0x827BD234` | implemented | direct (1 sites) |
| `xam.xex` | F | `55 / 0x037` | `NetDll_XNetRegisterKey` | `0x82000734` | `0x827BD244` | implemented | direct (1 sites) |
| `xam.xex` | F | `56 / 0x038` | `NetDll_XNetUnregisterKey` | `0x82000738` | `0x827BD254` | implemented | direct (1 sites) |
| `xam.xex` | F | `57 / 0x039` | `NetDll_XNetXnAddrToInAddr` | `0x8200073C` | `0x827BD264` | implemented | direct (1 sites) |
| `xam.xex` | F | `58 / 0x03A` | `NetDll_XNetServerToInAddr` | `0x82000740` | `0x827BD274` | unimplemented | direct (1 sites) |
| `xam.xex` | F | `63 / 0x03F` | `NetDll_XNetUnregisterInAddr` | `0x82000744` | `0x827BD284` | unimplemented | direct (1 sites) |
| `xam.xex` | F | `65 / 0x041` | `NetDll_XNetConnect` | `0x82000748` | `0x827BD294` | unimplemented | direct (1 sites) |
| `xam.xex` | F | `66 / 0x042` | `NetDll_XNetGetConnectStatus` | `0x8200074C` | `0x827BD2A4` | unimplemented | direct (1 sites) |
| `xam.xex` | F | `69 / 0x045` | `NetDll_XNetQosListen` | `0x82000750` | `0x827BD2B4` | implemented | direct (1 sites) |
| `xam.xex` | F | `70 / 0x046` | `NetDll_XNetQosLookup` | `0x82000754` | `0x827BD2C4` | unimplemented | direct (1 sites) |
| `xam.xex` | F | `72 / 0x048` | `NetDll_XNetQosRelease` | `0x82000758` | `0x827BD2D4` | implemented | direct (1 sites) |
| `xam.xex` | F | `73 / 0x049` | `NetDll_XNetGetTitleXnAddr` | `0x8200075C` | `0x827BD2E4` | implemented | direct (1 sites) |
| `xam.xex` | F | `75 / 0x04B` | `NetDll_XNetGetEthernetLinkStatus` | `0x82000760` | `0x827BD2F4` | implemented | direct (1 sites) |
| `xam.xex` | F | `310 / 0x136` | `XNetLogonGetTitleID` | `0x82000764` | `0x827BD304` | unimplemented | direct (2 sites) |
| `xam.xex` | F | `528 / 0x210` | `XamUserGetSigninState` | `0x82000768` | `0x827BD314` | implemented | direct (5 sites) |
| `xam.xex` | F | `537 / 0x219` | `XamUserReadProfileSettings` | `0x8200076C` | `0x827BD324` | implemented | direct (4 sites) |
| `xam.xex` | F | `522 / 0x20A` | `XamUserGetXUID` | `0x82000770` | `0x827BD334` | implemented | direct (1 sites) |
| `xam.xex` | F | `492 / 0x1EC` | `XamFree` | `0x82000774` | `0x827BD344` | implemented | direct (9 sites) |
| `xam.xex` | F | `490 / 0x1EA` | `XamAlloc` | `0x82000778` | `0x827BD354` | implemented | direct (8 sites) |
| `xboxkrnl.exe` | F | `253 / 0x0FD` | `NtWaitForSingleObjectEx` | `0x82000780` | `0x827BD034` | implemented | direct (10 sites) |
| `xboxkrnl.exe` | F | `309 / 0x135` | `RtlNtStatusToDosError` | `0x82000784` | `0x827BD024` | implemented | direct (10 sites) |
| `xboxkrnl.exe` | F | `310 / 0x136` | `RtlRaiseException` | `0x82000788` | `0x827BD014` | implemented | direct (3 sites) |
| `xboxkrnl.exe` | F | `238 / 0x0EE` | `NtQueryVirtualMemory` | `0x8200078C` | `0x827BD004` | implemented | direct (2 sites) |
| `xboxkrnl.exe` | F | `302 / 0x12E` | `RtlInitializeCriticalSection` | `0x82000790` | `0x827BCFF4` | implemented | direct (14 sites) |
| `xboxkrnl.exe` | F | `283 / 0x11B` | `RtlCompareMemoryUlong` | `0x82000794` | `0x827BCFE4` | implemented | direct (7 sites) |
| `xboxkrnl.exe` | F | `220 / 0x0DC` | `NtFreeVirtualMemory` | `0x82000798` | `0x827BCFD4` | implemented | direct (9 sites) |
| `xboxkrnl.exe` | F | `102 / 0x066` | `KeGetCurrentProcessType` | `0x8200079C` | `0x827BCFC4` | implemented | direct (7 sites) |
| `xboxkrnl.exe` | F | `83 / 0x053` | `KeBugCheckEx` | `0x820007A0` | `0x827BCFB4` | implemented | direct (4 sites) |
| `xboxkrnl.exe` | F | `294 / 0x126` | `RtlFillMemoryUlong` | `0x820007A4` | `0x827BCFA4` | implemented | direct (1 sites) |
| `xboxkrnl.exe` | F | `204 / 0x0CC` | `NtAllocateVirtualMemory` | `0x820007A8` | `0x827BCF94` | implemented | direct (13 sites) |
| `xboxkrnl.exe` | F | `40 / 0x028` | `HalReturnToFirmware` | `0x820007AC` | `0x827BCF84` | implemented | direct (1 sites) |
| `xboxkrnl.exe` | V | `403 / 0x193` | `XexExecutableModuleHandle` | `0x820007B0` | - | implemented | data import (no call site) |
| `xboxkrnl.exe` | F | `299 / 0x12B` | `RtlImageXexHeaderField` | `0x820007B4` | `0x827BCF74` | implemented | direct (5 sites) |
| `xboxkrnl.exe` | F | `293 / 0x125` | `RtlEnterCriticalSection` | `0x820007B8` | `0x827BCF64` | implemented | direct (153 sites) |
| `xboxkrnl.exe` | F | `304 / 0x130` | `RtlLeaveCriticalSection` | `0x820007BC` | `0x827BCF54` | implemented | direct (179 sites) |
| `xboxkrnl.exe` | F | `196 / 0x0C4` | `MmQueryAddressProtect` | `0x820007C0` | `0x827BCF44` | implemented | direct (4 sites) |
| `xboxkrnl.exe` | F | `189 / 0x0BD` | `MmFreePhysicalMemory` | `0x820007C4` | `0x827BCF34` | implemented | direct (5 sites) |
| `xboxkrnl.exe` | F | `197 / 0x0C5` | `MmQueryAllocationSize` | `0x820007C8` | `0x827BCF24` | implemented | direct (1 sites) |
| `xboxkrnl.exe` | F | `186 / 0x0BA` | `MmAllocatePhysicalMemoryEx` | `0x820007CC` | `0x827BCF14` | implemented | direct (3 sites) |
| `xboxkrnl.exe` | F | `421 / 0x1A5` | `__C_specific_handler` | `0x820007D0` | `0x827BCF04` | unimplemented | indirect-only (0 direct sites) |
| `xboxkrnl.exe` | F | `3 / 0x003` | `DbgPrint` | `0x820007D4` | `0x827BCEF4` | implemented | direct (2 sites) |
| `xboxkrnl.exe` | F | `404 / 0x194` | `XexCheckExecutablePrivilege` | `0x820007D8` | `0x827BCEE4` | implemented | direct (6 sites) |
| `xboxkrnl.exe` | F | `16 / 0x010` | `ExGetXConfigSetting` | `0x820007DC` | `0x827BCED4` | implemented | direct (12 sites) |
| `xboxkrnl.exe` | F | `209 / 0x0D1` | `NtCreateEvent` | `0x820007E0` | `0x827BCEC4` | implemented | direct (4 sites) |
| `xboxkrnl.exe` | F | `207 / 0x0CF` | `NtClose` | `0x820007E4` | `0x827BCEB4` | implemented | direct (27 sites) |
| `xboxkrnl.exe` | V | `89 / 0x059` | `KeDebugMonitorData` | `0x820007E8` | - | implemented | data import (no call site) |
| `xboxkrnl.exe` | F | `511 / 0x1FF` | `XAudioGetSpeakerConfig` | `0x820007EC` | `0x827BD494` | implemented | direct (2 sites) |
| `xboxkrnl.exe` | F | `321 / 0x141` | `RtlTryEnterCriticalSection` | `0x820007F0` | `0x827BD4A4` | implemented | direct (3 sites) |
| `xboxkrnl.exe` | F | `82 / 0x052` | `KeBugCheck` | `0x820007F4` | `0x827BD4B4` | implemented | direct (7 sites) |
| `xboxkrnl.exe` | F | `338 / 0x152` | `KeTlsAlloc` | `0x820007F8` | `0x827BD4C4` | implemented | direct (2 sites) |
| `xboxkrnl.exe` | F | `340 / 0x154` | `KeTlsGetValue` | `0x820007FC` | `0x827BD4D4` | implemented | direct (3 sites) |
| `xboxkrnl.exe` | F | `341 / 0x155` | `KeTlsSetValue` | `0x82000800` | `0x827BD4E4` | implemented | direct (3 sites) |
| `xboxkrnl.exe` | F | `339 / 0x153` | `KeTlsFree` | `0x82000804` | `0x827BD4F4` | implemented | direct (1 sites) |
| `xboxkrnl.exe` | F | `77 / 0x04D` | `KeAcquireSpinLockAtRaisedIrql` | `0x82000808` | `0x827BD504` | implemented | direct (92 sites) |
| `xboxkrnl.exe` | F | `133 / 0x085` | `KeRaiseIrqlToDpcLevel` | `0x8200080C` | `0x827BD514` | implemented | direct (92 sites) |
| `xboxkrnl.exe` | F | `179 / 0x0B3` | `KfLowerIrql` | `0x82000810` | `0x827BD524` | implemented | direct (116 sites) |
| `xboxkrnl.exe` | F | `174 / 0x0AE` | `KeTryToAcquireSpinLockAtRaisedIrql` | `0x82000814` | `0x827BD534` | implemented | direct (1 sites) |
| `xboxkrnl.exe` | F | `137 / 0x089` | `KeReleaseSpinLockFromRaisedIrql` | `0x82000818` | `0x827BD544` | implemented | direct (116 sites) |
| `xboxkrnl.exe` | F | `21 / 0x015` | `ExRegisterTitleTerminateNotification` | `0x8200081C` | `0x827BD554` | implemented | direct (12 sites) |
| `xboxkrnl.exe` | F | `157 / 0x09D` | `KeSetEvent` | `0x82000820` | `0x827BD564` | implemented | direct (13 sites) |
| `xboxkrnl.exe` | F | `136 / 0x088` | `KeReleaseSemaphore` | `0x82000824` | `0x827BD574` | implemented | direct (2 sites) |
| `xboxkrnl.exe` | F | `504 / 0x1F8` | `XAudioGetVoiceCategoryVolume` | `0x82000828` | `0x827BD584` | implemented | direct (1 sites) |
| `xboxkrnl.exe` | F | `503 / 0x1F7` | `XAudioGetVoiceCategoryVolumeChangeMask` | `0x8200082C` | `0x827BD594` | implemented | direct (1 sites) |
| `xboxkrnl.exe` | F | `176 / 0x0B0` | `KeWaitForSingleObject` | `0x82000830` | `0x827BD5A4` | implemented | direct (11 sites) |
| `xboxkrnl.exe` | F | `175 / 0x0AF` | `KeWaitForMultipleObjects` | `0x82000834` | `0x827BD5B4` | implemented | direct (1 sites) |
| `xboxkrnl.exe` | F | `146 / 0x092` | `KeResumeThread` | `0x82000838` | `0x827BD5C4` | implemented | direct (1 sites) |
| `xboxkrnl.exe` | F | `13 / 0x00D` | `ExCreateThread` | `0x8200083C` | `0x827BD5D4` | implemented | direct (3 sites) |
| `xboxkrnl.exe` | F | `116 / 0x074` | `KeInitializeSemaphore` | `0x82000840` | `0x827BD5E4` | implemented | direct (1 sites) |
| `xboxkrnl.exe` | F | `190 / 0x0BE` | `MmGetPhysicalAddress` | `0x82000844` | `0x827BD5F4` | implemented | direct (6 sites) |
| `xboxkrnl.exe` | F | `550 / 0x226` | `XMAReleaseContext` | `0x82000848` | `0x827BD604` | implemented | direct (1 sites) |
| `xboxkrnl.exe` | F | `548 / 0x224` | `XMACreateContext` | `0x8200084C` | `0x827BD614` | implemented | direct (1 sites) |
| `xboxkrnl.exe` | F | `131 / 0x083` | `KeQueryPerformanceFrequency` | `0x82000850` | `0x827BD624` | implemented | direct (4 sites) |
| `xboxkrnl.exe` | F | `15 / 0x00F` | `ExFreePool` | `0x82000854` | `0x827BD634` | implemented | direct (3 sites) |
| `xboxkrnl.exe` | F | `499 / 0x1F3` | `XAudioRegisterRenderDriverClient` | `0x82000858` | `0x827BD644` | implemented | direct (1 sites) |
| `xboxkrnl.exe` | F | `500 / 0x1F4` | `XAudioUnregisterRenderDriverClient` | `0x8200085C` | `0x827BD654` | implemented | direct (2 sites) |
| `xboxkrnl.exe` | F | `501 / 0x1F5` | `XAudioSubmitRenderDriverFrame` | `0x82000860` | `0x827BD664` | implemented | direct (1 sites) |
| `xboxkrnl.exe` | V | `446 / 0x1BE` | `VdGlobalDevice` | `0x82000864` | - | implemented | data import (no call site) |
| `xboxkrnl.exe` | F | `180 / 0x0B4` | `KfReleaseSpinLock` | `0x82000868` | `0x827BD674` | implemented | direct (14 sites) |
| `xboxkrnl.exe` | F | `177 / 0x0B1` | `KfAcquireSpinLock` | `0x8200086C` | `0x827BD684` | implemented | direct (13 sites) |
| `xboxkrnl.exe` | F | `479 / 0x1DF` | `KiApcNormalRoutineNop` | `0x82000870` | `0x827BD694` | implemented | direct (1 sites) |
| `xboxkrnl.exe` | F | `438 / 0x1B6` | `VdEnableRingBufferRPtrWriteBack` | `0x82000874` | `0x827BD6A4` | implemented | direct (1 sites) |
| `xboxkrnl.exe` | F | `451 / 0x1C3` | `VdInitializeRingBuffer` | `0x82000878` | `0x827BD6B4` | implemented | direct (1 sites) |
| `xboxkrnl.exe` | F | `473 / 0x1D9` | `VdSetSystemCommandBufferGpuIdentifierAddress` | `0x8200087C` | `0x827BD6C4` | implemented | direct (3 sites) |
| `xboxkrnl.exe` | V | `614 / 0x266` | `KeCertMonitorData` | `0x82000880` | - | implemented | data import (no call site) |
| `xboxkrnl.exe` | F | `455 / 0x1C7` | `VdPersistDisplay` | `0x82000884` | `0x827BD6D4` | implemented | direct (1 sites) |
| `xboxkrnl.exe` | F | `603 / 0x25B` | `VdSwap` | `0x82000888` | `0x827BD6E4` | implemented | direct (1 sites) |
| `xboxkrnl.exe` | F | `445 / 0x1BD` | `VdGetSystemCommandBuffer` | `0x8200088C` | `0x827BD6F4` | implemented | direct (1 sites) |
| `xboxkrnl.exe` | F | `315 / 0x13B` | `sprintf` | `0x82000890` | `0x827BD704` | implemented | direct (7 sites) |
| `xboxkrnl.exe` | F | `436 / 0x1B4` | `VdEnableDisableClockGating` | `0x82000894` | `0x827BD714` | implemented | direct (1 sites) |
| `xboxkrnl.exe` | F | `333 / 0x14D` | `_vsnprintf` | `0x82000898` | `0x827BD724` | implemented | direct (2 sites) |
| `xboxkrnl.exe` | V | `344 / 0x158` | `XboxKrnlVersion` | `0x8200089C` | - | implemented | data import (no call site) |
| `xboxkrnl.exe` | F | `476 / 0x1DC` | `VdShutdownEngines` | `0x820008A0` | `0x827BD734` | implemented | direct (2 sites) |
| `xboxkrnl.exe` | F | `458 / 0x1CA` | `VdQueryVideoMode` | `0x820008A4` | `0x827BD744` | implemented | direct (2 sites) |
| `xboxkrnl.exe` | F | `442 / 0x1BA` | `VdGetCurrentDisplayInformation` | `0x820008A8` | `0x827BD754` | implemented | direct (3 sites) |
| `xboxkrnl.exe` | F | `467 / 0x1D3` | `VdSetDisplayMode` | `0x820008AC` | `0x827BD764` | implemented | direct (1 sites) |
| `xboxkrnl.exe` | F | `469 / 0x1D5` | `VdSetGraphicsInterruptCallback` | `0x820008B0` | `0x827BD774` | implemented | direct (2 sites) |
| `xboxkrnl.exe` | F | `450 / 0x1C2` | `VdInitializeEngines` | `0x820008B4` | `0x827BD784` | implemented | direct (1 sites) |
| `xboxkrnl.exe` | F | `454 / 0x1C6` | `VdIsHSIOTrainingSucceeded` | `0x820008B8` | `0x827BD794` | implemented | direct (1 sites) |
| `xboxkrnl.exe` | F | `441 / 0x1B9` | `VdGetCurrentDisplayGamma` | `0x820008BC` | `0x827BD7A4` | implemented | direct (1 sites) |
| `xboxkrnl.exe` | F | `457 / 0x1C9` | `VdQueryVideoFlags` | `0x820008C0` | `0x827BD7B4` | implemented | direct (1 sites) |
| `xboxkrnl.exe` | F | `433 / 0x1B1` | `VdCallGraphicsNotificationRoutines` | `0x820008C4` | `0x827BD7C4` | implemented | direct (1 sites) |
| `xboxkrnl.exe` | V | `448 / 0x1C0` | `VdGpuClockInMHz` | `0x820008C8` | - | implemented | data import (no call site) |
| `xboxkrnl.exe` | F | `453 / 0x1C5` | `VdInitializeScalerCommandBuffer` | `0x820008CC` | `0x827BD7D4` | implemented | direct (1 sites) |
| `xboxkrnl.exe` | F | `125 / 0x07D` | `KeLeaveCriticalRegion` | `0x820008D0` | `0x827BD7E4` | implemented | direct (6 sites) |
| `xboxkrnl.exe` | F | `617 / 0x269` | `VdRetrainEDRAM` | `0x820008D4` | `0x827BD7F4` | implemented | direct (2 sites) |
| `xboxkrnl.exe` | F | `618 / 0x26A` | `VdRetrainEDRAMWorker` | `0x820008D8` | `0x827BD804` | implemented | direct (1 sites) |
| `xboxkrnl.exe` | V | `449 / 0x1C1` | `VdHSIOCalibrationLock` | `0x820008DC` | - | implemented | data import (no call site) |
| `xboxkrnl.exe` | F | `95 / 0x05F` | `KeEnterCriticalRegion` | `0x820008E0` | `0x827BD814` | implemented | direct (6 sites) |
| `xboxkrnl.exe` | F | `260 / 0x104` | `ObDeleteSymbolicLink` | `0x820008E4` | `0x827BD824` | implemented | direct (4 sites) |
| `xboxkrnl.exe` | F | `259 / 0x103` | `ObCreateSymbolicLink` | `0x820008E8` | `0x827BD834` | implemented | direct (2 sites) |
| `xboxkrnl.exe` | F | `300 / 0x12C` | `RtlInitAnsiString` | `0x820008EC` | `0x827BD844` | implemented | direct (33 sites) |
| `xboxkrnl.exe` | F | `107 / 0x06B` | `KeLockL2` | `0x820008F0` | `0x827BD854` | implemented | direct (1 sites) |
| `xboxkrnl.exe` | F | `108 / 0x06C` | `KeUnlockL2` | `0x820008F4` | `0x827BD864` | implemented | direct (1 sites) |
| `xboxkrnl.exe` | F | `143 / 0x08F` | `KeResetEvent` | `0x820008F8` | `0x827BD874` | implemented | direct (4 sites) |
| `xboxkrnl.exe` | F | `407 / 0x197` | `XexGetProcedureAddress` | `0x820008FC` | `0x827BD884` | implemented | direct (10 sites) |
| `xboxkrnl.exe` | F | `405 / 0x195` | `XexGetModuleHandle` | `0x82000900` | `0x827BD894` | implemented | direct (4 sites) |
| `xboxkrnl.exe` | F | `602 / 0x25A` | `StfsControlDevice` | `0x82000904` | `0x827BD8A4` | unimplemented | indirect-only (0 direct sites) |
| `xboxkrnl.exe` | F | `601 / 0x259` | `StfsCreateDevice` | `0x82000908` | `0x827BD8B4` | unimplemented | indirect-only (0 direct sites) |
| `xboxkrnl.exe` | F | `239 / 0x0EF` | `NtQueryVolumeInformationFile` | `0x8200090C` | `0x827BD8C4` | implemented | direct (5 sites) |
| `xboxkrnl.exe` | F | `223 / 0x0DF` | `NtOpenFile` | `0x82000910` | `0x827BD8D4` | implemented | direct (9 sites) |
| `xboxkrnl.exe` | F | `599 / 0x257` | `XeKeysConsoleSignatureVerification` | `0x82000914` | `0x827BD8E4` | unimplemented | direct (1 sites) |
| `xboxkrnl.exe` | F | `402 / 0x192` | `XeCryptSha` | `0x82000918` | `0x827BD8F4` | implemented | direct (7 sites) |
| `xboxkrnl.exe` | F | `255 / 0x0FF` | `NtWriteFile` | `0x8200091C` | `0x827BD904` | implemented | direct (12 sites) |
| `xboxkrnl.exe` | F | `240 / 0x0F0` | `NtReadFile` | `0x82000920` | `0x827BD914` | implemented | direct (3 sites) |
| `xboxkrnl.exe` | F | `598 / 0x256` | `XeKeysConsolePrivateKeySign` | `0x82000924` | `0x827BD924` | implemented | direct (1 sites) |
| `xboxkrnl.exe` | F | `210 / 0x0D2` | `NtCreateFile` | `0x82000928` | `0x827BD934` | implemented | direct (6 sites) |
| `xboxkrnl.exe` | F | `314 / 0x13A` | `_snprintf` | `0x8200092C` | `0x827BD944` | implemented | direct (7 sites) |
| `xboxkrnl.exe` | F | `219 / 0x0DB` | `NtFlushBuffersFile` | `0x82000930` | `0x827BD954` | implemented | direct (2 sites) |
| `xboxkrnl.exe` | F | `59 / 0x03B` | `IoDismountVolume` | `0x82000934` | `0x827BD964` | unimplemented | direct (2 sites) |
| `xboxkrnl.exe` | F | `213 / 0x0D5` | `NtCreateSemaphore` | `0x82000938` | `0x827BD974` | implemented | direct (1 sites) |
| `xboxkrnl.exe` | F | `243 / 0x0F3` | `NtReleaseSemaphore` | `0x8200093C` | `0x827BD984` | implemented | direct (1 sites) |
| `xboxkrnl.exe` | F | `254 / 0x0FE` | `NtWaitForMultipleObjectsEx` | `0x82000940` | `0x827BD994` | implemented | direct (1 sites) |
| `xboxkrnl.exe` | F | `246 / 0x0F6` | `NtSetEvent` | `0x82000944` | `0x827BD9A4` | implemented | direct (1 sites) |
| `xboxkrnl.exe` | F | `247 / 0x0F7` | `NtSetInformationFile` | `0x82000948` | `0x827BD9B4` | implemented | direct (12 sites) |
| `xboxkrnl.exe` | F | `231 / 0x0E7` | `NtQueryFullAttributesFile` | `0x8200094C` | `0x827BD9C4` | implemented | direct (3 sites) |
| `xboxkrnl.exe` | F | `232 / 0x0E8` | `NtQueryInformationFile` | `0x82000950` | `0x827BD9D4` | implemented | direct (4 sites) |
| `xboxkrnl.exe` | F | `320 / 0x140` | `RtlTimeToTimeFields` | `0x82000954` | `0x827BD9E4` | implemented | direct (5 sites) |
| `xboxkrnl.exe` | F | `212 / 0x0D4` | `NtCreateMutant` | `0x82000958` | `0x827BD9F4` | implemented | direct (1 sites) |
| `xboxkrnl.exe` | F | `242 / 0x0F2` | `NtReleaseMutant` | `0x8200095C` | `0x827BDA04` | implemented | direct (1 sites) |
| `xboxkrnl.exe` | F | `245 / 0x0F5` | `NtResumeThread` | `0x82000960` | `0x827BDA14` | implemented | direct (1 sites) |
| `xboxkrnl.exe` | F | `25 / 0x019` | `ExTerminateThread` | `0x82000964` | `0x827BDA24` | implemented | direct (2 sites) |
| `xboxkrnl.exe` | V | `173 / 0x0AD` | `KeTimeStampBundle` | `0x82000968` | - | implemented | data import (no call site) |
| `xboxkrnl.exe` | V | `342 / 0x156` | `XboxHardwareInfo` | `0x8200096C` | - | implemented | data import (no call site) |
| `xboxkrnl.exe` | F | `285 / 0x11D` | `RtlCompareStringN` | `0x82000970` | `0x827BDA34` | implemented | direct (11 sites) |
| `xboxkrnl.exe` | F | `217 / 0x0D9` | `NtDeviceIoControlFile` | `0x82000974` | `0x827BDA44` | implemented | direct (3 sites) |
| `xboxkrnl.exe` | F | `411 / 0x19B` | `XexLoadImageHeaders` | `0x82000978` | `0x827BDA54` | implemented | direct (1 sites) |
| `xboxkrnl.exe` | F | `132 / 0x084` | `KeQuerySystemTime` | `0x8200097C` | `0x827BDA64` | implemented | direct (7 sites) |
| `xboxkrnl.exe` | F | `307 / 0x133` | `RtlMultiByteToUnicodeN` | `0x82000980` | `0x827BDA74` | implemented | direct (1 sites) |
| `xboxkrnl.exe` | F | `206 / 0x0CE` | `NtClearEvent` | `0x82000984` | `0x827BDA84` | implemented | direct (1 sites) |
| `xboxkrnl.exe` | F | `323 / 0x143` | `RtlUnicodeToMultiByteN` | `0x82000988` | `0x827BDA94` | implemented | direct (1 sites) |
| `xboxkrnl.exe` | F | `65 / 0x041` | `IoInvalidDeviceRequest` | `0x8200098C` | `0x827BDAA4` | unimplemented | indirect-only (0 direct sites) |
| `xboxkrnl.exe` | F | `271 / 0x10F` | `ObReferenceObject` | `0x82000990` | `0x827BDAB4` | implemented | direct (1 sites) |
| `xboxkrnl.exe` | F | `55 / 0x037` | `IoCreateDevice` | `0x82000994` | `0x827BDAC4` | implemented | direct (1 sites) |
| `xboxkrnl.exe` | F | `57 / 0x039` | `IoDeleteDevice` | `0x82000998` | `0x827BDAD4` | implemented | direct (1 sites) |
| `xboxkrnl.exe` | F | `11 / 0x00B` | `ExAllocatePoolTypeWithTag` | `0x8200099C` | `0x827BDAE4` | implemented | direct (2 sites) |
| `xboxkrnl.exe` | F | `319 / 0x13F` | `RtlTimeFieldsToTime` | `0x820009A0` | `0x827BDAF4` | implemented | direct (6 sites) |
| `xboxkrnl.exe` | F | `53 / 0x035` | `IoCompleteRequest` | `0x820009A4` | `0x827BDB04` | unimplemented | direct (11 sites) |
| `xboxkrnl.exe` | F | `1 / 0x001` | `DbgBreakPoint` | `0x820009A8` | `0x827BDB14` | implemented | direct (1 sites) |
| `xboxkrnl.exe` | F | `329 / 0x149` | `RtlUpcaseUnicodeChar` | `0x820009AC` | `0x827BDB24` | implemented | direct (2 sites) |
| `xboxkrnl.exe` | F | `265 / 0x109` | `ObIsTitleObject` | `0x820009B0` | `0x827BDB34` | unimplemented | direct (2 sites) |
| `xboxkrnl.exe` | F | `52 / 0x034` | `IoCheckShareAccess` | `0x820009B4` | `0x827BDB44` | unimplemented | direct (2 sites) |
| `xboxkrnl.exe` | F | `71 / 0x047` | `IoSetShareAccess` | `0x820009B8` | `0x827BDB54` | unimplemented | direct (3 sites) |
| `xboxkrnl.exe` | F | `69 / 0x045` | `IoRemoveShareAccess` | `0x820009BC` | `0x827BDB64` | unimplemented | direct (2 sites) |
| `xboxkrnl.exe` | F | `60 / 0x03C` | `IoDismountVolumeByFileHandle` | `0x820009C0` | `0x827BDB74` | unimplemented | direct (1 sites) |
| `xboxkrnl.exe` | F | `228 / 0x0E4` | `NtQueryDirectoryFile` | `0x820009C4` | `0x827BDB84` | implemented | indirect-only (0 direct sites) |
| `xboxkrnl.exe` | F | `241 / 0x0F1` | `NtReadFileScatter` | `0x820009C8` | `0x827BDB94` | implemented | indirect-only (0 direct sites) |
| `xboxkrnl.exe` | F | `218 / 0x0DA` | `NtDuplicateObject` | `0x820009CC` | `0x827BDBA4` | implemented | direct (1 sites) |
| `xboxkrnl.exe` | F | `90 / 0x05A` | `KeDelayExecutionThread` | `0x820009D0` | `0x827BDBB4` | implemented | direct (1 sites) |
| `xboxkrnl.exe` | F | `417 / 0x1A1` | `XexUnloadImage` | `0x820009D4` | `0x827BDBC4` | implemented | direct (1 sites) |
| `xboxkrnl.exe` | F | `409 / 0x199` | `XexLoadImage` | `0x820009D8` | `0x827BDBD4` | implemented | direct (1 sites) |
| `xboxkrnl.exe` | F | `151 / 0x097` | `KeSetAffinityThread` | `0x820009DC` | `0x827BCEA4` | implemented | direct (1 sites) |
| `xboxkrnl.exe` | F | `156 / 0x09C` | `KeSetDisableBoostThread` | `0x820009E0` | `0x827BCE94` | implemented | direct (1 sites) |
| `xboxkrnl.exe` | V | `27 / 0x01B` | `ExThreadObjectType` | `0x820009E4` | - | implemented | data import (no call site) |
| `xboxkrnl.exe` | F | `272 / 0x110` | `ObReferenceObjectByHandle` | `0x820009E8` | `0x827BCE84` | implemented | direct (5 sites) |
| `xboxkrnl.exe` | F | `153 / 0x099` | `KeSetBasePriorityThread` | `0x820009EC` | `0x827BCE74` | implemented | direct (4 sites) |
| `xboxkrnl.exe` | F | `261 / 0x105` | `ObDereferenceObject` | `0x820009F0` | `0x827BCE64` | implemented | direct (23 sites) |
| `xboxkrnl.exe` | V | `430 / 0x1AE` | `ExLoadedCommandLine` | `0x820009F4` | - | implemented | data import (no call site) |
| `xboxkrnl.exe` | F | `327 / 0x147` | `RtlUnwind` | `0x820009F8` | `0x827BDCA4` | unimplemented | direct (1 sites) |
| `xboxkrnl.exe` | F | `205 / 0x0CD` | `NtCancelTimer` | `0x820009FC` | `0x827BDCB4` | implemented | direct (1 sites) |
| `xboxkrnl.exe` | F | `250 / 0x0FA` | `NtSetTimerEx` | `0x82000A00` | `0x827BDCC4` | implemented | direct (1 sites) |
| `xboxkrnl.exe` | F | `215 / 0x0D7` | `NtCreateTimer` | `0x82000A04` | `0x827BDCD4` | implemented | direct (1 sites) |

## Gate result

- Unknown library/ordinal symbols: 0
- Unmapped XEX import records: 0
- Generated function thunk mismatches: 0
- Unclassified static reachability entries: 0
- Xenia-unimplemented symbols retained for M2-014/M3 ownership: 22

M2-013 acceptance: PASS. Every unique import maps to a symbolic export and an explicit static reachability class.
