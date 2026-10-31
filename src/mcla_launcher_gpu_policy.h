#pragma once

// M9-006 pure launcher GPU choices (no DXGI, no Win32): the default adapter
// when no GPU was ever chosen (KI-051) and the backend argument (KI-053).

#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>

namespace mcla::launcher {

struct AdapterInfo {
  std::wstring name;
  uint64_t dedicated_video_memory = 0;  // DXGI_ADAPTER_DESC1.DedicatedVideoMemory
};

// None -> nullopt; one -> that adapter; several -> the largest dedicated video
// memory, ties to the first enumerated. Never invents a name.
inline std::optional<std::wstring> DefaultAdapterName(std::span<const AdapterInfo> adapters) {
  const AdapterInfo *best = nullptr;
  for (const auto &adapter : adapters)
    if (!best || adapter.dedicated_video_memory > best->dedicated_video_memory) best = &adapter;
  if (!best) return std::nullopt;
  return best->name;
}

// The setup form's GPU: a saved name is kept as is (an adapter that vanished
// stays an explicit empty selection in the UI, never another adapter); only a
// never-chosen (empty) name takes the default.
inline std::wstring InitialGpuName(std::wstring_view saved, std::span<const AdapterInfo> adapters) {
  if (!saved.empty()) return std::wstring(saved);
  return DefaultAdapterName(adapters).value_or(std::wstring{});
}

// KI-053 deferred: D3D12 on every platform, including Wine/Proton (vkd3d).
// The project's SDK installs are built with REXGLUE_USE_VULKAN=OFF, so the
// xenos plugin has no Vulkan backend and "vulkan" fails at startup. Return
// --gpu_backend=vulkan under Wine only after an SDK build with
// REXGLUE_USE_VULKAN=ON and a Steam Deck check of that backend.
inline std::wstring_view GpuBackendArgument(bool under_wine) {
  (void)under_wine;
  return std::wstring_view(L"--gpu_backend=d3d12");
}

}  // namespace mcla::launcher
