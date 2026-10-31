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

// Under Wine/Proton the host's native Vulkan backend avoids D3D12 through
// vkd3d-proton; real Windows keeps D3D12. Only this argument differs.
inline std::wstring_view GpuBackendArgument(bool under_wine) {
  return under_wine ? std::wstring_view(L"--gpu_backend=vulkan") : std::wstring_view(L"--gpu_backend=d3d12");
}

}  // namespace mcla::launcher
