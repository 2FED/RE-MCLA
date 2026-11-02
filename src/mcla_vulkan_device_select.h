#pragma once

// KI-061 (M10-004): pure choice of the SDK's vulkan_device index for the
// Vulkan backend (no Vulkan, no DXGI, no Win32). The launcher passes its GPU
// choice only as --d3d12_adapter=<raw IDXGIFactory1::EnumAdapters1 ordinal>;
// the SDK's Vulkan provider ignores it and, with vulkan_device=-1, takes
// physical device 0 (an integrated GPU on hybrid laptops). The host resolves
// the ordinal to the adapter's LUID and PCI ids and maps it here.
//
// Order: an explicit vulkan_device (>= 0) is kept; one device needs no
// override; then the launcher adapter by LUID (deviceLUIDValid only), then by
// vendor+device id when exactly one device has them; then the discrete GPU
// with the most device-local memory (first on ties); otherwise the SDK's own
// automatic selection is left in place.

#include <array>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>

namespace mcla::vulkan_device {

using Luid = std::array<uint8_t, 8>;

// One entry per vkEnumeratePhysicalDevices result, in enumeration order.
struct Device {
  std::string name;
  uint32_t vendor_id = 0;
  uint32_t device_id = 0;
  bool discrete = false;  // VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU
  bool luid_valid = false;  // VkPhysicalDeviceIDProperties.deviceLUIDValid
  Luid luid{};
  uint64_t device_local_bytes = 0;  // sum of DEVICE_LOCAL memory heaps
};

// The launcher-selected DXGI adapter (DXGI_ADAPTER_DESC1).
struct Adapter {
  Luid luid{};
  uint32_t vendor_id = 0;
  uint32_t device_id = 0;
};

enum class Reason { kExplicit, kNoDevices, kSingle, kLuid, kIds, kDiscrete, kFallback };

struct Choice {
  int32_t index = -1;     // -1: leave the SDK's automatic selection
  Reason reason = Reason::kFallback;
  bool set_cvar = false;  // true only when the host must set vulkan_device
};

constexpr std::string_view ReasonName(Reason reason) {
  switch (reason) {
    case Reason::kExplicit: return "explicit";
    case Reason::kNoDevices: return "no devices";
    case Reason::kSingle: return "single";
    case Reason::kLuid: return "LUID";
    case Reason::kIds: return "ids";
    case Reason::kDiscrete: return "discrete";
    case Reason::kFallback: return "fallback";
  }
  return "fallback";
}

inline Choice Select(int32_t explicit_vulkan_device, const std::optional<Adapter> &adapter,
                     std::span<const Device> devices) {
  if (explicit_vulkan_device >= 0) return {explicit_vulkan_device, Reason::kExplicit, false};
  if (devices.empty()) return {-1, Reason::kNoDevices, false};
  if (devices.size() == 1) return {0, Reason::kSingle, false};
  if (adapter) {
    for (size_t i = 0; i < devices.size(); ++i)
      if (devices[i].luid_valid && devices[i].luid == adapter->luid)
        return {static_cast<int32_t>(i), Reason::kLuid, true};
    int32_t match = -1;
    int matches = 0;
    for (size_t i = 0; i < devices.size(); ++i) {
      if (devices[i].vendor_id == adapter->vendor_id &&
          devices[i].device_id == adapter->device_id) {
        if (match < 0) match = static_cast<int32_t>(i);
        ++matches;
      }
    }
    if (matches == 1) return {match, Reason::kIds, true};
  }
  int32_t best = -1;
  for (size_t i = 0; i < devices.size(); ++i) {
    if (!devices[i].discrete) continue;
    if (best < 0 || devices[i].device_local_bytes > devices[best].device_local_bytes)
      best = static_cast<int32_t>(i);
  }
  if (best >= 0) return {best, Reason::kDiscrete, true};
  return {-1, Reason::kFallback, false};
}

}  // namespace mcla::vulkan_device
