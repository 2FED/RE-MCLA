# M10-004 / KI-061: Vulkan device follows the launcher's GPU choice (l64)

Date: 2026-11-02. Status: host fix implemented and unit-tested; owner-internal
candidate l64 staged as input only (not prepared, not run). Physical PASS is
pending: the PC Vulkan arm must be repeated on the owner's laptop.

## Problem

KI-061 (Vulkan picks the first device, ignoring the launcher's GPU choice): in
the l63 all-vulkan arm (2026-11-02) the game ran on the integrated AMD Radeon
860M (Vulkan physical device 0) instead of the RTX 5060 Laptop GPU (device 1),
at 10-14 fps.

## How the adapter identity flows (before the fix)

1. Launcher (`src/mcla_launcher_controller_win.cpp`, `Adapters`/`AdapterIndex`):
   `CreateDXGIFactory1` + `EnumAdapters1(i)` (software and non-FL11_0 adapters skipped); each entry keeps the description,
   the raw ordinal `i` and `DedicatedVideoMemory`. KI-051 picks the default
   (single adapter, else the largest dedicated memory) when no GPU name is
   saved; the saved choice is a name, resolved back to the ordinal.
2. The launch arguments carry only `--d3d12_adapter=<ordinal>` and
   `--gpu_backend=d3d12` (`src/mcla_launcher_gpu_policy.h`). The portable
   launcher passes the same raw `EnumAdapters1` ordinal.
3. SDK D3D12 provider (`src/ui/d3d12/d3d12_provider.cpp`, cvar `d3d12_adapter`
   in rexruntime.dll): the same `EnumAdapters1` ordinal. The host's texture
   budget (`src/mcla_texture_budget_win.cpp`) mirrors that rule.
4. SDK Vulkan provider (`src/ui/vulkan/vulkan_provider.cpp`, SDK 8081f63):
   cvar `vulkan_device` (int32, default -1, `kInitOnly`, registered by
   rexruntime.dll). With -1 the provider keeps the enumeration order unless the
   `vulkan_prefer_*` feature scores differ, then takes the first supported
   device: device 0. `d3d12_adapter` is never consulted. With an index >= 0 it
   tries that device first and falls back to the automatic order if the device
   is unsupported.

## Can the host set vulkan_device in time? Yes (no SDK change)

`ReXApp::SetupPresentation` calls `OnPreSetup(config_)` and only then
`LoadGpuPlugin(gpu_plugin, gpu_backend)`, which creates the Vulkan provider.
`vulkan_device` lives in rexruntime.dll (string present there, absent in
rexgpu-xenos.dll), so it is registered before `OnPreSetup`.
`rex::cvar::FinalizeInit` is never called outside SDK tests, so the
`kInitOnly` guard in `SetFlagByName` does not refuse the write.

## Fix (host only)

- `src/mcla_vulkan_device_select.h` (pure, no Vulkan/DXGI/Win32):
  `Select(explicit_vulkan_device, launcher_adapter, devices)`. Order:
  1. `vulkan_device >= 0` (explicit): kept, nothing probed.
  2. no devices (probe failed): nothing set.
  3. one device (Steam Deck APU): nothing set (the SDK picks it anyway).
  4. launcher adapter matched by LUID (only devices with `deviceLUIDValid`).
  5. else by vendor+device id when exactly one device has them (two identical
     GPUs fall through).
  6. else the discrete GPU (`VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU`) with the
     most device-local heap memory, first on ties (also used when the launcher
     passed no adapter, -1, or WARP, -2).
  7. else nothing set (the SDK's automatic choice, as before).
- `src/mcla_vulkan_device_win.{h,cpp}`: reads `vulkan_device` and
  `d3d12_adapter` through `rex::cvar::GetFlagInfo` (cross-DLL; a missing cvar
  is not mistaken for index 0); resolves the ordinal with
  `CreateDXGIFactory1`/`EnumAdapters1`/`GetDesc1` (LUID, VendorId, DeviceId;
  software adapters ignored); lists Vulkan devices through a private instance
  of the system loader (`LoadLibraryExW("vulkan-1.dll",
  LOAD_LIBRARY_SEARCH_SYSTEM32)`, `vkGetInstanceProcAddr`, API 1.1 when the
  loader has it: `vkGetPhysicalDeviceProperties2` +
  `VkPhysicalDeviceIDProperties`; 1.0 loaders give ids/type only), destroys
  the instance and keeps the loader module loaded (the SDK loads it moments
  later). No link dependency on Vulkan; the probe compiles only when the SDK
  install provides `vulkan/vulkan_core.h`. Sets `vulkan_device` through
  `rex::cvar::SetFlagByName` only for rules 4-6. Never fails setup.
- `src/mcla_app.cpp` `OnPreSetup`: after the dev switch and the KI-053 branch,
  `if (config.gpu_backend == "vulkan") mcla::SelectVulkanDeviceForLauncherAdapter();`
  inside `#if REX_HAS_VULKAN`. D3D12 sessions are untouched.
- `CMakeLists.txt`: the new source in the Windows `mcla` sources; new test
  target `mcla_vulkan_device_select_test`.

One log line per Vulkan session, for example on the owner's laptop:

    MCLA gpu backend: vulkan device 1 'NVIDIA GeForce RTX 5060 Laptop GPU' (matched by LUID); vulkan_device set; launcher d3d12_adapter=<n>; 2 Vulkan device(s)

Other forms: `(explicit vulkan_device kept)`, `(matched by single)`,
`(matched by fallback)` / `(matched by no devices)` with `'SDK automatic'` and
`; probe: <reason>`. The SDK's own list ("Available Vulkan physical devices")
follows in the same log, so the chosen index can be checked against it.

## Tests

- New `mcla_vulkan_device_select_test`: 15 checks (LUID match for the dGPU and
  for an explicitly chosen iGPU; LUID wins between identical twins; ids match
  without valid LUIDs; an invalid LUID is ignored; ambiguous ids fall through
  to the discrete rule; no match -> discrete; no match and no discrete ->
  nothing set; no launcher adapter -> discrete, largest memory first on ties;
  single device (Deck) -> nothing set; explicit `vulkan_device` kept; probe
  failure -> nothing set).
- Build `private\l64-host-build` (scratch tree = main 0d7dc62 + this patch,
  plus `generated/default` and the two SDK-submodule header trees the tests
  use, copied read-only from the checkout), against
  `private\l63vk-sdk-install` with the l63 configure options: 235/235 steps,
  0 warnings.
- All host unit test executables: 44/44 rc=0 (l63 had 43; +1 new).
- ast-grep 0.50.0: `scan` rc=0, `test` 3/3 rc=0.

## Candidate l64 (input staged only)

`private\l64-int-input` (seven files; SDK DLLs are the l63 files):

| File | SHA-256 |
| --- | --- |
| mcla.exe | 9D73C364E880C231042BF8E5D1D979144CE01E4398E3C6CBB95E0EF8F194BA1D |
| MCLA-Launcher.exe | F9D6343A0DD5A23E35CA152624117C3E8D3F814E6B2AD29097D8F153E4F10C6C |
| mcla_crash_handler.exe | CD6660BA4F4B26DE51D69381B98C50FC7E6FEAE8A3ABA826609F7630D7D93228 |
| mcla_diagnostic_image_helper.exe | A351E3D23773229363F5141A478ED434601BCCC6878AFCF8CB710BDA6F672F43 |
| rexruntime.dll | 2108DAA113F81A8566F6ED2D9F99F2362A11EE887EA3C1F9E2C2FA06F0486F5A |
| rexgpu-xenos.dll | 29CFE7A0DCD789CAAE2F992ED7E2E7030D7F13B3C065419A6130BC670143B21B |
| TracyClient.dll | 752DDD0815F4D9B1B74666385B5F794BA48266E6C40E643C89EA824D1F42E7DA |

`private\l64-internal-prepare.ps1` (derived from the l63 script: V1 ticket,
pins, stage `private\l64-internal`, internal audit, adds an `l64-internal`
admission row and keeps every existing row). The l63 arm files are reused
unchanged (`private\l63-arm-*.cmd`, `private\l63-arms\deck\...`).

## Owner test (pending)

1. Run `l64-internal-prepare.ps1` in pwsh 7; set the launcher build folder to
   `private\l64-internal\build`, CHECK SETUP.
2. Run `private\l63-arm-all-vulkan.cmd`; start the game with the RTX 5060
   selected in the launcher.
3. In `mcla.log`: the `MCLA gpu backend: vulkan device 1 ... (matched by
   LUID)` line and the SDK's device list; fps comparable to the D3D12 arms,
   not 10-14. Then `l63-arm-clear.cmd`.
4. Steam Deck (optional): the same line reads `(matched by single)` and nothing
   else changes.

## Risks

- Enumeration order: the probe's instance and the SDK's instance are separate
  instances of the same loader in the same process; order is expected to match
  (the loader sorts by adapter on Windows), but the SDK may enable different
  instance extensions/layers. The log names the device; the SDK's device list
  in the same log allows a check.
- Under Wine/Proton with several Vulkan devices, winevulkan/DXVK may not expose
  matching LUIDs or may spoof the vendor id; the rule then falls to the
  discrete GPU or the SDK default (no regression versus l63).
- An extra Vulkan instance at startup loads implicit layers (Steam overlay,
  OBS, RTSS) once more: a small startup cost.
- `vulkan_device` becomes a modified cvar for the session; a non-managed
  session that saves the SDK config could persist it. Managed launches
  unregister the settings/console binds, so the SDK config is not saved there.
- The launcher choosing the iGPU is respected (LUID match), by design.
