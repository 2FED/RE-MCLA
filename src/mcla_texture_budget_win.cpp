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
//
// M10-007 / KI-048 (docs/evidence/M10-007-vram-census-and-soft-cap-20261104.md):
// - soft cap: at draw resolution scale >= 2 on a LOCAL budget under 10 GB the
//   soft limit (and the monitor floor) start at
//   min(budget / 2, mcla_texture_budget_soft_cap_mb), default 2048 MB, 0 = off;
//   the hard limit is unchanged (mcla_vram_census_win.h DecideStartLimits).
// - census: one MCLA_VRAM v=1 line per minute, at each accepted F10 and at exit
//   (mcla_vram_census, default on; log only). When the user set the limits the
//   monitor runs for the census only and never changes them.
// - M11-002 / KI-067 CPU census: one MCLA_CPU v=1 line next to each MCLA_VRAM
//   line (mcla_cpu_census, default on; needs the VRAM census): effective CPU
//   clock, system and process CPU load, ACPI thermal zone temperature when
//   readable (mcla_vram_census_win.h CpuReader/CpuWindow). Under Wine/Proton
//   (no monitor) one "cpu=wine" line at start says the fields are unavailable.
// - l99 (docs/evidence/M11-l99-sdk-hitch-20261113.md): mcla_texture_hard_margin_mb
//   (default 0 = as before; owner key texture_hard_margin_256 writes 256) keeps
//   the stepped hard limit at least that far above the starting soft limit, so
//   over the DXGI budget the cache still has a soft-to-hard band instead of
//   evicting at once (the l98 soak stepped hard down to soft). Every second
//   the monitor also hands its local / non-local sample to the SDK
//   (MclaSdkVramHostSample: MCLA_HITCH vram_over_mb, MCLA_VRAM_SDK); a no-op
//   with an SDK without that entry point.

#include "mcla_texture_budget.h"

#include "mcla_logging.h"
#include "mcla_low_latency.h"
#include "mcla_vram_census_win.h"

#include <rex/cvar.h>

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#include <dxgi1_4.h>
#include <wrl/client.h>

#include "mcla_host_platform_win.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#pragma comment(lib, "dxgi.lib")

REXCVAR_DEFINE_INT32(
    mcla_texture_budget_soft_cap_mb, mcla::vram::kSoftCapDefaultMb, "MCLA",
    "KI-048: at draw resolution scale >= 2 on a video-memory budget under 10 GB, start the "
    "texture cache soft limit (and the budget monitor floor) at min(budget/2, this) MB; 0 = off")
    .range(0, 8192)
    .lifecycle(rex::cvar::Lifecycle::kInitOnly);

REXCVAR_DEFINE_INT32(
    mcla_texture_hard_margin_mb, 0, "MCLA",
    "l99: the budget monitor never steps the texture cache hard limit below the starting soft "
    "limit plus this many MB (0 = the starting soft limit, as before l99)")
    .range(0, 4096)
    .lifecycle(rex::cvar::Lifecycle::kInitOnly);

REXCVAR_DEFINE_BOOL(
    mcla_vram_census, true, "MCLA",
    "M10-007: log one MCLA_VRAM line per minute, at each F10 and at exit (DXGI local/non-local "
    "memory, texture cache limits, GPU clocks/temperature/power from D3DKMT; log only)")
    .lifecycle(rex::cvar::Lifecycle::kInitOnly);

REXCVAR_DEFINE_BOOL(
    mcla_cpu_census, true, "MCLA",
    "M11-002 / KI-067: with the video-memory census, log one MCLA_CPU line per minute, at each F10 "
    "and at exit (effective CPU clock, system and process CPU load, ACPI temperature if readable; "
    "log only)")
    .lifecycle(rex::cvar::Lifecycle::kInitOnly);

namespace mcla {
namespace {

constexpr uint64_t kMb = 1024 * 1024;
constexpr uint64_t kHeadroomMb = 512;
constexpr uint64_t kStepMb = 256;
constexpr auto kStepInterval = std::chrono::seconds(10);
constexpr const char* kSoftName = "texture_cache_memory_limit_soft";
constexpr const char* kHardName = "texture_cache_memory_limit_hard";

bool RunningUnderWine() { return platform::RunningUnderWine(); }

int32_t IntFlag(const char* name, int32_t fallback) {
  if (!rex::cvar::GetFlagInfo(name)) return fallback;
  try {
    return std::stoi(rex::cvar::GetFlagByName(name));
  } catch (...) {
    return fallback;
  }
}

bool FlagSet(const char* name) {
  return rex::cvar::GetFlagInfo(name) && rex::cvar::HasNonDefaultValue(name);
}

// The SDK's draw resolution scale (graphics plugin cvars; 1 if absent).
uint32_t ConfiguredDrawScale() {
  return vram::EffectiveDrawScale(IntFlag("resolution_scale", 1), FlagSet("resolution_scale"),
                                  IntFlag("draw_resolution_scale_x", 1),
                                  FlagSet("draw_resolution_scale_x"),
                                  IntFlag("draw_resolution_scale_y", 1),
                                  FlagSet("draw_resolution_scale_y"));
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

bool QueryNonLocal(IDXGIAdapter3* adapter, DXGI_QUERY_VIDEO_MEMORY_INFO& info) {
  return SUCCEEDED(adapter->QueryVideoMemoryInfo(0, DXGI_MEMORY_SEGMENT_GROUP_NON_LOCAL, &info));
}

bool SetLimit(const char* name, uint64_t value_mb) {
  return rex::cvar::SetFlagByName(name, std::to_string(value_mb));
}

class Monitor {
 public:
  // steps = false: census only (the user set the limits; never changed).
  void Start(Microsoft::WRL::ComPtr<IDXGIAdapter3> adapter, uint64_t soft_mb, uint64_t hard_mb,
             uint64_t floor_mb, bool steps, uint32_t scale, bool census) {
    adapter_ = std::move(adapter);
    soft_mb_ = soft_mb;
    hard_mb_ = hard_mb;
    floor_mb_ = floor_mb;
    steps_ = steps;
    scale_ = scale;
    census_ = census;
    cpu_census_ = census && REXCVAR_GET(mcla_cpu_census);
    last_step_ = std::chrono::steady_clock::now() - kStepInterval;
    thread_ = std::thread([this] { Run(); });
    running_.store(true, std::memory_order_release);
  }
  void Stop() {
    running_.store(false, std::memory_order_release);
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
  // An accepted F10 (or other live snapshot): one census line at once.
  void Mark(std::string_view reason) {
    if (!running_.load(std::memory_order_acquire) || !census_) return;
    {
      std::lock_guard lock(mutex_);
      if (marks_.size() < 8) marks_.emplace_back(reason);
    }
    wake_.notify_all();
  }

 private:
  vram::MemorySample Sample(const DXGI_QUERY_VIDEO_MEMORY_INFO& local) {
    vram::MemorySample m;
    m.local_usage_mb = local.CurrentUsage / kMb;
    m.local_budget_mb = local.Budget / kMb;
    m.local_available_mb = local.AvailableForReservation / kMb;
    DXGI_QUERY_VIDEO_MEMORY_INFO non_local{};
    if (QueryNonLocal(adapter_.Get(), non_local)) {
      m.nonlocal_ok = true;
      m.nonlocal_usage_mb = non_local.CurrentUsage / kMb;
      m.nonlocal_budget_mb = non_local.Budget / kMb;
    }
    return m;
  }

  vram::MonitorView View() const {
    vram::MonitorView v;
    v.soft_mb = soft_mb_;
    v.hard_mb = hard_mb_;
    v.floor_mb = floor_mb_;
    v.near_budget_steps = steps_taken_;
    v.near_budget_seconds = near_seconds_;
    v.peak_mb = peak_mb_;
    v.scale = scale_;
    v.steps_enabled = steps_;
    return v;
  }

  void EmitCensus(std::string_view reason, std::chrono::steady_clock::time_point now) {
    const double window_s = std::chrono::duration<double>(now - census_window_start_).count();
    const std::string line =
        census_window_.TakeLine(reason, census_seq_ + 1, window_s, View(), gpu_->state());
    if (cpu_) {
      // The CPU window follows the VRAM window; its line does not need a DXGI
      // sample.
      const double cpu_window_s = std::chrono::duration<double>(now - cpu_window_start_).count();
      MCLA_GPU_INFO("{}", cpu_window_.TakeLine(reason, ++cpu_seq_, cpu_window_s,
                                               vram::SourcesOf(*cpu_)));
      cpu_window_start_ = now;
    }
    if (line.empty()) return;
    ++census_seq_;
    census_window_start_ = now;
    MCLA_GPU_INFO("{}", line);
  }

  void Run() {
    std::unique_ptr<vram::KmtApi> kmt_api;
    if (census_) {
      kmt_api = vram::CreateSystemKmtApi();
    }
    vram::GpuPerfReader gpu(kmt_api.get());
    gpu_ = &gpu;
    if (census_ && kmt_api) {
      const DXGI_ADAPTER_DESC desc = [this] {
        DXGI_ADAPTER_DESC d{};
        if (FAILED(adapter_->GetDesc(&d))) d = {};
        return d;
      }();
      const vram::KmtState state =
          gpu.Start(RunningUnderWine(), desc.AdapterLuid.LowPart, desc.AdapterLuid.HighPart);
      MCLA_GPU_INFO("MCLA graphics: video-memory census on; D3DKMT perf data {} (3D node {})",
                    vram::KmtStateName(state), gpu.node());
    } else if (census_) {
      MCLA_GPU_INFO("MCLA graphics: video-memory census on; D3DKMT entry points unavailable");
    }
    // M11-002 / KI-067: the CPU sources live on this thread (WMI uses COM
    // initialised for this thread only).
    std::unique_ptr<vram::CpuApi> cpu_api;
    std::unique_ptr<vram::CpuReader> cpu;
    if (cpu_census_) {
      cpu_api = vram::CreateSystemCpuApi();
      cpu = std::make_unique<vram::CpuReader>(cpu_api.get());
      const vram::CpuState state = cpu->Start(RunningUnderWine());
      MCLA_GPU_INFO("MCLA graphics: CPU census {}; {} logical processors; times {}, PDH {}, "
                    "power information {}, ACPI temperature {} (status {:08X})",
                    vram::CpuStateName(state), cpu->logical(),
                    vram::SourceStateName(cpu->times()), vram::SourceStateName(cpu->pdh()),
                    vram::SourceStateName(cpu->power()),
                    vram::SourceStateName(cpu->temperature()), cpu->temperature_status());
      if (state == vram::CpuState::kOk) {
        cpu_ = cpu.get();
      }
    }
    auto last_report = std::chrono::steady_clock::now();
    census_window_start_ = last_report;
    cpu_window_start_ = last_report;
    std::vector<std::string> marks;
    std::unique_lock lock(mutex_);
    for (;;) {
      const bool woke =
          wake_.wait_for(lock, std::chrono::seconds(1), [this] { return stop_ || !marks_.empty(); });
      const bool stopping = stop_;
      const bool tick = !woke;
      marks.swap(marks_);
      lock.unlock();
      DXGI_QUERY_VIDEO_MEMORY_INFO info{};
      const bool have = QueryLocal(adapter_.Get(), info);
      const auto now = std::chrono::steady_clock::now();
      if (have) {
        const uint64_t usage_mb = info.CurrentUsage / kMb;
        last_budget_mb_ = info.Budget / kMb;
        // l99: the SDK's view of the budget (no D3D query of its own).
        if (tick) {
          DXGI_QUERY_VIDEO_MEMORY_INFO non_local{};
          const uint64_t non_local_mb =
              QueryNonLocal(adapter_.Get(), non_local) ? non_local.CurrentUsage / kMb : 0;
          MclaSdkVramHostSample(last_budget_mb_, usage_mb, non_local_mb);
        }
        peak_mb_ = std::max(peak_mb_, usage_mb);
        const bool near_budget = usage_mb + kHeadroomMb > last_budget_mb_;
        if (tick && near_budget) ++near_seconds_;
        // Only ever lower the hard limit, gently: in-use textures cannot be
        // evicted, so usage may not drop at once, and ratcheting every second
        // made the cache destroy textures a scene still needed (l17 garage hang,
        // 2026-10-25). One kStepMb step per kStepInterval, never below the
        // floor (the starting soft limit).
        if (steps_ && tick && near_budget && hard_mb_ > floor_mb_ && now - last_step_ >= kStepInterval) {
          const uint64_t new_hard = std::max(floor_mb_, hard_mb_ > kStepMb ? hard_mb_ - kStepMb : 0);
          if (new_hard < hard_mb_ && SetLimit(kHardName, new_hard)) {
            hard_mb_ = new_hard;
            last_step_ = now;
            ++steps_taken_;
            MCLA_GPU_INFO("MCLA graphics: video memory {} MB near budget {} MB; texture cache hard limit now {} MB",
                          usage_mb, last_budget_mb_, hard_mb_);
          }
        }
        if (census_) {
          census_window_.AddMemory(Sample(info));
          if (tick) census_window_.AddGpu(gpu.Sample());
        }
        if (cpu_ && tick) cpu_window_.Add(cpu_->Sample());
        if (tick && now - last_report >= std::chrono::seconds(60)) {
          last_report = now;
          MCLA_GPU_INFO("MCLA graphics: video memory {} MB (peak {} MB) of {} MB budget", usage_mb,
                        peak_mb_, last_budget_mb_);
          if (census_) EmitCensus("minute", now);
        }
      }
      if (census_) {
        for (const auto& reason : marks) EmitCensus(reason, now);
        if (stopping) EmitCensus("exit", now);
      }
      marks.clear();
      lock.lock();
      if (stopping) break;
    }
    lock.unlock();
    gpu.Stop();
    gpu_ = nullptr;
    cpu_ = nullptr;
    if (cpu) cpu->Stop();
  }

  Microsoft::WRL::ComPtr<IDXGIAdapter3> adapter_;
  std::thread thread_;
  std::atomic<bool> running_{false};  // Mark may come from another thread
  std::mutex mutex_;
  std::condition_variable wake_;
  bool stop_ = false;                // guarded by mutex_
  std::vector<std::string> marks_;   // guarded by mutex_
  // Monitor thread only (Stop reads them after join).
  uint64_t soft_mb_ = 0, hard_mb_ = 0, floor_mb_ = 0, peak_mb_ = 0, last_budget_mb_ = 0;
  uint64_t steps_taken_ = 0, near_seconds_ = 0, census_seq_ = 0;
  uint32_t scale_ = 1;
  bool steps_ = true, census_ = false, cpu_census_ = false;
  vram::GpuPerfReader* gpu_ = nullptr;
  vram::CensusWindow census_window_;
  std::chrono::steady_clock::time_point last_step_{}, census_window_start_{};
  // M11-002 / KI-067 CPU census (monitor thread only).
  vram::CpuReader* cpu_ = nullptr;
  vram::CpuWindow cpu_window_;
  uint64_t cpu_seq_ = 0;
  std::chrono::steady_clock::time_point cpu_window_start_{};
};

Monitor monitor;

}  // namespace

void ApplyTextureCacheBudget() {
  const bool wine = RunningUnderWine();
  const bool user_set =
      rex::cvar::HasNonDefaultValue(kSoftName) || rex::cvar::HasNonDefaultValue(kHardName);
  Microsoft::WRL::ComPtr<IDXGIAdapter3> adapter;
  DXGI_QUERY_VIDEO_MEMORY_INFO info{};
  bool budget_available = false;
  if (!wine) {
    adapter = FindAdapter();
    budget_available = adapter && QueryLocal(adapter.Get(), info);
  }
  const bool census = REXCVAR_GET(mcla_vram_census);
  switch (vram::DecideBudgetAction(wine, user_set, budget_available)) {
    case vram::BudgetAction::kWine:
      MCLA_GPU_INFO("MCLA graphics: Wine/Proton detected; keeping SDK texture cache limits");
      // KI-067: no census monitor here; the CPU fields are marked unavailable.
      if (census && REXCVAR_GET(mcla_cpu_census)) {
        MCLA_GPU_INFO("{}", vram::CpuUnavailableLine("start", vram::CpuState::kWine));
      }
      return;
    case vram::BudgetAction::kUnavailable:
      if (user_set) {
        MCLA_GPU_INFO("MCLA graphics: texture cache limits set by the user; keeping them");
      }
      MCLA_GPU_INFO("MCLA graphics: video-memory budget unavailable; keeping SDK texture cache limits");
      if (census && REXCVAR_GET(mcla_cpu_census)) {
        MCLA_GPU_INFO("{}", vram::CpuUnavailableLine("start", vram::CpuState::kOff));
      }
      return;
    case vram::BudgetAction::kUserSet: {
      MCLA_GPU_INFO("MCLA graphics: texture cache limits set by the user; keeping them");
      if (census) {
        const auto soft = uint64_t(std::max(0, IntFlag(kSoftName, 0)));
        const auto hard = uint64_t(std::max(0, IntFlag(kHardName, 0)));
        monitor.Start(std::move(adapter), soft, hard, hard, false, ConfiguredDrawScale(), true);
      }
      return;
    }
    case vram::BudgetAction::kApply:
      break;
  }
  const uint64_t budget_mb = info.Budget / kMb;
  const uint32_t scale = ConfiguredDrawScale();
  const int32_t cap_mb = REXCVAR_GET(mcla_texture_budget_soft_cap_mb);
  const vram::StartLimits limits = vram::DecideStartLimits(budget_mb, scale, cap_mb);
  if (!SetLimit(kSoftName, limits.soft_mb) || !SetLimit(kHardName, limits.hard_mb)) {
    MCLA_GPU_ERROR("MCLA graphics: could not set texture cache limits");
    return;
  }
  MCLA_GPU_INFO("MCLA graphics: texture cache soft cap {}: scale {}, budget {} MB, cap {} MB; "
                "soft {} MB (uncapped {} MB), floor {} MB, hard {} MB",
                vram::SoftCapReasonName(limits.reason), scale, budget_mb, cap_mb, limits.soft_mb,
                limits.legacy_soft_mb, limits.floor_mb, limits.hard_mb);
  MCLA_GPU_INFO("MCLA graphics: video-memory budget {} MB (in use {} MB); texture cache limits {}/{} MB",
                budget_mb, info.CurrentUsage / kMb, limits.soft_mb, limits.hard_mb);
  // l99: the hard-limit floor with the optional margin above the soft limit.
  const uint64_t margin_mb = uint64_t(std::max(0, REXCVAR_GET(mcla_texture_hard_margin_mb)));
  const uint64_t floor_mb = vram::HardFloorWithMargin(limits.floor_mb, limits.soft_mb, margin_mb);
  MCLA_GPU_INFO("MCLA graphics: texture cache hard limit floor {} MB (soft {} MB, margin {} MB)",
                floor_mb, limits.soft_mb, margin_mb);
  monitor.Start(std::move(adapter), limits.soft_mb, limits.hard_mb, floor_mb, true, scale,
                census);
}

void MarkTextureCacheBudgetCensus(std::string_view reason) { monitor.Mark(reason); }

void StopTextureCacheBudgetMonitor() { monitor.Stop(); }

}  // namespace mcla
