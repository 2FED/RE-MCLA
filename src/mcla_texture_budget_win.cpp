// Size the SDK texture cache limits from the GPU's actual video-memory budget.
//
// The SDK texture cache destroys textures unused for 30 s once host texture
// memory passes texture_cache_memory_limit_soft (default 384 MB) and
// immediately past texture_cache_memory_limit_hard (default 768 MB). Destroying
// and recreating textures while driving is the suspected cause of the
// GPU-allocation hitches in docs/evidence/M8-001-timebox-20261025.md. GPUs range
// from 4 GB to 24 GB, so the limits follow the D3D12 adapter's local budget
// (which also covers shared-memory integrated GPUs). Under Wine/Proton (Steam
// Deck, dynamic memory) and whenever the query fails, the SDK defaults stay.

#include "mcla_texture_budget.h"

#include "mcla_logging.h"

#include <rex/cvar.h>

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#include <dxgi1_4.h>
#include <wrl/client.h>

#include <algorithm>
#include <cstdint>
#include <string>

#pragma comment(lib, "dxgi.lib")

namespace mcla {
namespace {

bool RunningUnderWine() {
  HMODULE ntdll = GetModuleHandleW(L"ntdll.dll");
  return ntdll && GetProcAddress(ntdll, "wine_get_version") != nullptr;
}

int32_t ConfiguredAdapterIndex() {
  const auto* info = rex::cvar::GetFlagInfo("d3d12_adapter");
  if (!info || !info->getter) return -1;
  try {
    return std::stoi(info->getter());
  } catch (...) {
    return -1;
  }
}

// Mirrors the SDK's D3D12 provider: an explicit index, or (-1) the first
// hardware adapter. WARP (-2) has no meaningful video-memory budget.
bool QueryLocalBudgetMb(uint64_t& budget_mb, std::wstring& adapter_name) {
  const int32_t wanted = ConfiguredAdapterIndex();
  if (wanted == -2) return false;
  Microsoft::WRL::ComPtr<IDXGIFactory1> factory;
  if (FAILED(CreateDXGIFactory1(IID_PPV_ARGS(&factory)))) return false;
  Microsoft::WRL::ComPtr<IDXGIAdapter1> adapter;
  for (UINT i = 0; factory->EnumAdapters1(i, &adapter) == S_OK; ++i, adapter.Reset()) {
    DXGI_ADAPTER_DESC1 desc{};
    if (FAILED(adapter->GetDesc1(&desc))) continue;
    const bool match = wanted >= 0 ? int32_t(i) == wanted
                                   : (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) == 0;
    if (!match) continue;
    Microsoft::WRL::ComPtr<IDXGIAdapter3> adapter3;
    DXGI_QUERY_VIDEO_MEMORY_INFO memory{};
    if (FAILED(adapter.As(&adapter3)) ||
        FAILED(adapter3->QueryVideoMemoryInfo(0, DXGI_MEMORY_SEGMENT_GROUP_LOCAL, &memory)) ||
        memory.Budget == 0) {
      return false;
    }
    budget_mb = memory.Budget / (1024 * 1024);
    adapter_name = desc.Description;
    return true;
  }
  return false;
}

}  // namespace

void ApplyTextureCacheBudget() {
  if (RunningUnderWine()) {
    MCLA_GPU_INFO("MCLA graphics: Wine/Proton detected; keeping SDK texture cache limits");
    return;
  }
  uint64_t budget_mb = 0;
  std::wstring adapter;
  if (!QueryLocalBudgetMb(budget_mb, adapter)) {
    MCLA_GPU_INFO("MCLA graphics: video-memory budget unavailable; keeping SDK texture cache limits");
    return;
  }
  const uint64_t soft_mb = std::clamp<uint64_t>(budget_mb / 4, 384, 4096);
  const uint64_t hard_mb = std::clamp<uint64_t>(budget_mb * 2 / 5, soft_mb + 384, 6144);
  MCLA_GPU_INFO("MCLA graphics: local video-memory budget {} MB", budget_mb);
  struct Limit { const char* name; uint64_t value; };
  for (const Limit limit : {Limit{"texture_cache_memory_limit_soft", soft_mb},
                            Limit{"texture_cache_memory_limit_hard", hard_mb}}) {
    if (rex::cvar::HasNonDefaultValue(limit.name)) continue;
    if (rex::cvar::SetFlagByName(limit.name, std::to_string(limit.value))) {
      MCLA_GPU_INFO("MCLA graphics: {} = {} MB", limit.name, limit.value);
    } else {
      MCLA_GPU_ERROR("MCLA graphics: could not set {}", limit.name);
    }
  }
}

}  // namespace mcla
