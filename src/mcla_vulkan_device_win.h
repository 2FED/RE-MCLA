#pragma once

namespace mcla {

// KI-061 (M10-004): call from OnPreSetup once the session's GPU backend is
// "vulkan" and before the GPU plugin loads. Maps the launcher's d3d12_adapter
// choice to the SDK's vulkan_device cvar (kInitOnly, registered by
// rexruntime.dll and read when the Vulkan provider is created), unless
// vulkan_device was set explicitly. Logs one line. Never fails setup: any probe
// failure leaves the SDK's automatic selection.
#if defined(_WIN32)
void SelectVulkanDeviceForLauncherAdapter();
#else
inline void SelectVulkanDeviceForLauncherAdapter() {}
#endif

}  // namespace mcla
