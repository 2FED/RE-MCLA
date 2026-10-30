// Size the SDK texture cache limits from the GPU's actual video-memory budget.
//
// The SDK texture cache destroys textures unused for 30 s once host texture
// memory passes texture_cache_memory_limit_soft (default 384 MB) and
// immediately past texture_cache_memory_limit_hard (default 768 MB). Destroying
// and recreating textures while driving is the suspected cause of the
// GPU-allocation hitches in docs/evidence/M8-001-timebox-20261025.md. GPUs range
// from 4 GB to 24 GB, so the limits follow the D3D12 adapter's local budget,
// which also covers shared-memory integrated GPUs: soft 50 %, hard 80 %.
//
// Render targets, the guest-memory mirror, upload pools and the driver also
// live in video memory. If the process exceeds its budget, Windows demotes
// video memory to system RAM, which stalls far worse than evictions. The hard
// limit therefore starts at most budget - 1.5 GB, and a once-per-second monitor
// lowers it gently (256 MB per 10 s, never below the starting soft limit)
// whenever the process's total usage comes within 512 MB of the budget (the
// cache rereads the limits at every eviction). It logs the
// budget, peak usage and every adjustment. Under Wine/Proton (Steam Deck,
// dynamic memory), when the query fails, or when the user set the limits
// themselves, nothing is changed.

#include "mcla_texture_budget.h"

#include "mcla_logging.h"

#include <rex/cvar.h>

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#include <dxgi1_4.h>
#include <wrl/client.h>

#include "mcla_host_platform_win.h"

#include <algorithm>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <mutex>
#include <string>
#include <thread>

#pragma comment(lib, "dxgi.lib")

namespace mcla {
namespace {

constexpr uint64_t kMb = 1024 * 1024;
constexpr uint64_t kStartReserveMb = 1536;
constexpr uint64_t kHeadroomMb = 512;
constexpr uint64_t kStepMb = 256;
constexpr auto kStepInterval = std::chrono::seconds(10);
// The SDK cvars accept soft 64..4096 MB and hard 128..8192 MB.
constexpr uint64_t kSoftMinMb = 384, kSoftMaxMb = 4096;
constexpr uint64_t kHardMinMb = 768, kHardMaxMb = 8192;
constexpr const char* kSoftName = "texture_cache_memory_limit_soft";
constexpr const char* kHardName = "texture_cache_memory_limit_hard";

bool RunningUnderWine() { return platform::RunningUnderWine(); }

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
Microsoft::WRL::ComPtr<IDXGIAdapter3> FindAdapter() {
  const int32_t wanted = ConfiguredAdapterIndex();
  if (wanted == -2) return nullptr;
  Microsoft::WRL::ComPtr<IDXGIFactory1> factory;
  if (FAILED(CreateDXGIFactory1(IID_PPV_ARGS(&factory)))) return nullptr;
  Microsoft::WRL::ComPtr<IDXGIAdapter1> adapter;
  for (UINT i = 0; factory->EnumAdapters1(i, &adapter) == S_OK; ++i, adapter.Reset()) {
    DXGI_ADAPTER_DESC1 desc{};
    if (FAILED(adapter->GetDesc1(&desc))) continue;
    const bool match = wanted >= 0 ? int32_t(i) == wanted
                                   : (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) == 0;
    if (!match) continue;
    Microsoft::WRL::ComPtr<IDXGIAdapter3> adapter3;
    if (FAILED(adapter.As(&adapter3))) return nullptr;
    return adapter3;
  }
  return nullptr;
}

bool QueryLocal(IDXGIAdapter3* adapter, DXGI_QUERY_VIDEO_MEMORY_INFO& info) {
  return SUCCEEDED(adapter->QueryVideoMemoryInfo(0, DXGI_MEMORY_SEGMENT_GROUP_LOCAL, &info)) &&
         info.Budget != 0;
}

bool SetLimit(const char* name, uint64_t value_mb) {
  return rex::cvar::SetFlagByName(name, std::to_string(value_mb));
}

class Monitor {
 public:
  void Start(Microsoft::WRL::ComPtr<IDXGIAdapter3> adapter, uint64_t soft_mb, uint64_t hard_mb) {
    adapter_ = std::move(adapter);
    soft_mb_ = soft_mb;
    hard_mb_ = hard_mb;
    floor_mb_ = soft_mb;
    last_step_ = std::chrono::steady_clock::now() - kStepInterval;
    thread_ = std::thread([this] { Run(); });
  }
  void Stop() {
    if (!thread_.joinable()) return;
    {
      std::lock_guard lock(mutex_);
      stop_ = true;
    }
    wake_.notify_all();
    thread_.join();
    MCLA_GPU_INFO("MCLA graphics: video memory peak {} MB of {} MB budget; texture cache hard limit {} MB",
                  peak_mb_, last_budget_mb_, hard_mb_);
  }

 private:
  void Run() {
    auto last_report = std::chrono::steady_clock::now();
    std::unique_lock lock(mutex_);
    while (!wake_.wait_for(lock, std::chrono::seconds(1), [this] { return stop_; })) {
      DXGI_QUERY_VIDEO_MEMORY_INFO info{};
      if (!QueryLocal(adapter_.Get(), info)) continue;
      const uint64_t usage_mb = info.CurrentUsage / kMb;
      last_budget_mb_ = info.Budget / kMb;
      peak_mb_ = std::max(peak_mb_, usage_mb);
      // Only ever lower the hard limit, gently: in-use textures cannot be
      // evicted, so usage may not drop at once, and ratcheting every second
      // made the cache destroy textures a scene still needed (l17 garage hang,
      // 2026-10-25). One kStepMb step per kStepInterval, never below the
      // starting soft limit.
      const auto now = std::chrono::steady_clock::now();
      if (usage_mb + kHeadroomMb > last_budget_mb_ && hard_mb_ > floor_mb_ &&
          now - last_step_ >= kStepInterval) {
        const uint64_t new_hard = std::max(floor_mb_, hard_mb_ > kStepMb ? hard_mb_ - kStepMb : 0);
        if (new_hard < hard_mb_ && SetLimit(kHardName, new_hard)) {
          hard_mb_ = new_hard;
          last_step_ = now;
          MCLA_GPU_INFO("MCLA graphics: video memory {} MB near budget {} MB; texture cache hard limit now {} MB",
                        usage_mb, last_budget_mb_, hard_mb_);
        }
      }
      if (now - last_report >= std::chrono::seconds(60)) {
        last_report = now;
        MCLA_GPU_INFO("MCLA graphics: video memory {} MB (peak {} MB) of {} MB budget", usage_mb,
                      peak_mb_, last_budget_mb_);
      }
    }
  }

  Microsoft::WRL::ComPtr<IDXGIAdapter3> adapter_;
  std::thread thread_;
  std::mutex mutex_;
  std::condition_variable wake_;
  bool stop_ = false;
  uint64_t soft_mb_ = 0, hard_mb_ = 0, floor_mb_ = 0, peak_mb_ = 0, last_budget_mb_ = 0;
  std::chrono::steady_clock::time_point last_step_{};
};

Monitor monitor;

}  // namespace

void ApplyTextureCacheBudget() {
  if (RunningUnderWine()) {
    MCLA_GPU_INFO("MCLA graphics: Wine/Proton detected; keeping SDK texture cache limits");
    return;
  }
  if (rex::cvar::HasNonDefaultValue(kSoftName) || rex::cvar::HasNonDefaultValue(kHardName)) {
    MCLA_GPU_INFO("MCLA graphics: texture cache limits set by the user; keeping them");
    return;
  }
  auto adapter = FindAdapter();
  DXGI_QUERY_VIDEO_MEMORY_INFO info{};
  if (!adapter || !QueryLocal(adapter.Get(), info)) {
    MCLA_GPU_INFO("MCLA graphics: video-memory budget unavailable; keeping SDK texture cache limits");
    return;
  }
  const uint64_t budget_mb = info.Budget / kMb;
  const uint64_t hard_cap = budget_mb > kStartReserveMb ? budget_mb - kStartReserveMb : 0;
  const uint64_t hard_mb =
      std::clamp<uint64_t>(std::min<uint64_t>(budget_mb * 4 / 5, hard_cap), kHardMinMb, kHardMaxMb);
  const uint64_t soft_mb =
      std::min(std::clamp<uint64_t>(budget_mb / 2, kSoftMinMb, kSoftMaxMb), hard_mb);
  if (!SetLimit(kSoftName, soft_mb) || !SetLimit(kHardName, hard_mb)) {
    MCLA_GPU_ERROR("MCLA graphics: could not set texture cache limits");
    return;
  }
  MCLA_GPU_INFO("MCLA graphics: video-memory budget {} MB (in use {} MB); texture cache limits {}/{} MB",
                budget_mb, info.CurrentUsage / kMb, soft_mb, hard_mb);
  monitor.Start(std::move(adapter), soft_mb, hard_mb);
}

void StopTextureCacheBudgetMonitor() { monitor.Stop(); }

}  // namespace mcla
