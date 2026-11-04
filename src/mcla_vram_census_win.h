#pragma once

// M10-007 / KI-048 video-memory census and the short-term soft-cap mitigation
// (docs/evidence/M10-007-budget-design-20261104.md section 4 and "Short-term
// mitigation"; docs/evidence/M10-007-vram-census-and-soft-cap-20261104.md).
//
// - Start-up texture-cache limits (DecideStartLimits): the legacy sizing
//   (hard = clamp(min(0.8 x budget, budget - 1536)), soft = clamp(budget / 2))
//   plus the soft cap of mcla_texture_budget_soft_cap_mb (default 2048, 0 =
//   off): at draw resolution scale >= 2 on a LOCAL budget under 10 GB the soft
//   limit starts at min(budget / 2, cap) and the monitor floor follows it; the
//   hard limit is unchanged.
// - One MCLA_VRAM v=1 line per minute, at each accepted F10 and at exit
//   (CensusWindow::TakeLine): DXGI LOCAL and NON_LOCAL usage/budget, budget
//   deltas since the previous line, the limits as the monitor knows them, the
//   near-budget step count, and GPU engine/memory clocks, temperature, power
//   and fan from D3DKMT perf data (min/avg/max of the 1 Hz samples; n/a when
//   the driver reports 0 or the query is unavailable).
// - The D3DKMT calls sit behind KmtApi; GpuPerfReader owns the
//   open/query/close policy (never under Wine, disabled after repeated
//   failures, never fails the game).
//
// Everything in this header is pure and unit-tested
// (tests/mcla_vram_census_win_test.cpp); the DXGI and D3DKMT calls are in
// mcla_vram_census_win.cpp and mcla_texture_budget_win.cpp.

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <memory>
#include <string>
#include <string_view>

namespace mcla::vram {

// Legacy start-up sizing constants (mcla_texture_budget_win.cpp, 2026-10-25).
inline constexpr uint64_t kStartReserveMb = 1536;
// The SDK cvars accept soft 64..4096 MB and hard 128..8192 MB.
inline constexpr uint64_t kSoftMinMb = 384, kSoftMaxMb = 4096;
inline constexpr uint64_t kHardMinMb = 768, kHardMaxMb = 8192;
// Soft cap (KI-048 short-term mitigation).
inline constexpr int32_t kSoftCapDefaultMb = 2048;
inline constexpr uint64_t kSoftCapBudgetLimitMb = 10 * 1024;  // "under 10 GB"
inline constexpr uint32_t kSoftCapMinScale = 2;

// The SDK's TextureCache::GetConfigDrawResolutionScale: resolution_scale
// applies to an axis whose own cvar was not set. Returns the larger axis
// scale, at least 1.
constexpr uint32_t EffectiveDrawScale(int32_t shared, bool shared_set, int32_t x, bool x_set,
                                      int32_t y, bool y_set) {
  const int32_t sx = shared_set && !x_set ? shared : x;
  const int32_t sy = shared_set && !y_set ? shared : y;
  const int32_t m = std::max({sx, sy, int32_t{1}});
  return static_cast<uint32_t>(m);
}

enum class SoftCapReason : uint8_t {
  kOff,          // mcla_texture_budget_soft_cap_mb = 0
  kScale1,       // draw resolution scale 1 (720p): unaffected
  kLargeBudget,  // LOCAL budget >= 10 GB: unaffected
  kNotBelow,     // budget / 2 already at or below the cap: unchanged
  kCapped,       // soft limit (and floor) lowered to the cap
};

constexpr const char *SoftCapReasonName(SoftCapReason reason) {
  switch (reason) {
    case SoftCapReason::kOff:
      return "off";
    case SoftCapReason::kScale1:
      return "scale_1";
    case SoftCapReason::kLargeBudget:
      return "budget_10gb_or_more";
    case SoftCapReason::kNotBelow:
      return "half_budget_not_above_cap";
    case SoftCapReason::kCapped:
      return "capped";
  }
  return "unknown";
}

struct StartLimits {
  uint64_t soft_mb = 0;
  uint64_t hard_mb = 0;
  uint64_t floor_mb = 0;         // the monitor never lowers hard below this
  uint64_t legacy_soft_mb = 0;   // the soft limit without the cap
  SoftCapReason reason = SoftCapReason::kOff;
};

// Start-up limits from the LOCAL budget (MB), the effective draw resolution
// scale and the soft cap cvar (MB; <= 0 = off).
constexpr StartLimits DecideStartLimits(uint64_t budget_mb, uint32_t scale, int32_t cap_mb) {
  StartLimits out;
  const uint64_t hard_cap = budget_mb > kStartReserveMb ? budget_mb - kStartReserveMb : 0;
  out.hard_mb = std::clamp<uint64_t>(std::min<uint64_t>(budget_mb * 4 / 5, hard_cap), kHardMinMb,
                                     kHardMaxMb);
  out.legacy_soft_mb =
      std::min(std::clamp<uint64_t>(budget_mb / 2, kSoftMinMb, kSoftMaxMb), out.hard_mb);
  out.soft_mb = out.legacy_soft_mb;
  if (cap_mb <= 0) {
    out.reason = SoftCapReason::kOff;
  } else if (scale < kSoftCapMinScale) {
    out.reason = SoftCapReason::kScale1;
  } else if (budget_mb >= kSoftCapBudgetLimitMb) {
    out.reason = SoftCapReason::kLargeBudget;
  } else {
    const uint64_t target = std::max<uint64_t>(
        std::min<uint64_t>(budget_mb / 2, static_cast<uint64_t>(cap_mb)), kSoftMinMb);
    if (target >= out.legacy_soft_mb) {
      out.reason = SoftCapReason::kNotBelow;
    } else {
      out.soft_mb = target;
      out.reason = SoftCapReason::kCapped;
    }
  }
  out.floor_mb = out.soft_mb;
  return out;
}

// What ApplyTextureCacheBudget does at start-up.
enum class BudgetAction : uint8_t {
  kWine,         // Wine/Proton: SDK limits, no monitor, no census
  kUserSet,      // the user set texture_cache_memory_limit_soft/hard: kept;
                 // census only (no limit changes, no steps)
  kUnavailable,  // no adapter or no budget: SDK limits, no monitor
  kApply,        // DecideStartLimits, monitor with steps and census
};

constexpr BudgetAction DecideBudgetAction(bool wine, bool user_set_limits, bool budget_available) {
  if (wine) {
    return BudgetAction::kWine;
  }
  if (!budget_available) {
    return BudgetAction::kUnavailable;
  }
  return user_set_limits ? BudgetAction::kUserSet : BudgetAction::kApply;
}

// ---------------------------------------------------------------------------
// D3DKMT perf data behind an interface (fake in the tests).

struct NodePerf {
  uint64_t frequency_hz = 0;
  uint64_t max_frequency_hz = 0;
};

struct AdapterPerf {
  uint64_t memory_frequency_hz = 0;
  uint32_t power_tenth_pct = 0;     // D3DKMT_ADAPTER_PERFDATA::Power (0.1 %)
  uint32_t temperature_deci_c = 0;  // D3DKMT_ADAPTER_PERFDATA::Temperature (0.1 C)
  uint32_t fan_rpm = 0;
};

class KmtApi {
 public:
  virtual ~KmtApi() = default;
  // D3DKMTOpenAdapterFromLuid; false on any failure.
  virtual bool Open(uint32_t luid_low, int32_t luid_high) = 0;
  // KMTQAITYPE_NODEMETADATA: false when the node does not exist or the query
  // failed; is_3d = engine type DXGK_ENGINE_TYPE_3D.
  virtual bool NodeIs3D(uint32_t node, bool &is_3d) = 0;
  // KMTQAITYPE_NODEPERFDATA / KMTQAITYPE_ADAPTERPERFDATA.
  virtual bool QueryNode(uint32_t node, NodePerf &out) = 0;
  virtual bool QueryAdapter(AdapterPerf &out) = 0;
  // D3DKMTCloseAdapter (only called after a successful Open).
  virtual void Close() = 0;
};

struct GpuPerfSample {
  bool node_ok = false;
  bool adapter_ok = false;
  NodePerf node;
  AdapterPerf adapter;
};

enum class KmtState : uint8_t {
  kOff,         // not started (census off or no API)
  kWine,        // Wine/Proton: never called
  kOpenFailed,  // D3DKMTOpenAdapterFromLuid failed
  kOk,          // sampling
  kFailed,      // disabled after repeated query failures
};

constexpr const char *KmtStateName(KmtState state) {
  switch (state) {
    case KmtState::kOff:
      return "off";
    case KmtState::kWine:
      return "wine";
    case KmtState::kOpenFailed:
      return "open_failed";
    case KmtState::kOk:
      return "ok";
    case KmtState::kFailed:
      return "failed";
  }
  return "unknown";
}

class GpuPerfReader {
 public:
  static constexpr uint32_t kMaxNodes = 64;
  static constexpr uint32_t kMaxConsecutiveFailures = 5;

  explicit GpuPerfReader(KmtApi *api) : api_(api) {}
  GpuPerfReader(const GpuPerfReader &) = delete;
  GpuPerfReader &operator=(const GpuPerfReader &) = delete;
  ~GpuPerfReader() { Stop(); }

  KmtState Start(bool wine, uint32_t luid_low, int32_t luid_high) {
    Stop();
    if (wine) {
      return state_ = KmtState::kWine;
    }
    if (!api_) {
      return state_ = KmtState::kOff;
    }
    if (!api_->Open(luid_low, luid_high)) {
      return state_ = KmtState::kOpenFailed;
    }
    open_ = true;
    node_ = 0;
    for (uint32_t n = 0; n < kMaxNodes; ++n) {
      bool is_3d = false;
      if (!api_->NodeIs3D(n, is_3d)) {
        break;
      }
      if (is_3d) {
        node_ = n;
        break;
      }
    }
    failures_ = 0;
    return state_ = KmtState::kOk;
  }

  GpuPerfSample Sample() {
    GpuPerfSample s;
    if (state_ != KmtState::kOk) {
      return s;
    }
    s.node_ok = api_->QueryNode(node_, s.node);
    s.adapter_ok = api_->QueryAdapter(s.adapter);
    if (!s.node_ok) {
      s.node = {};
    }
    if (!s.adapter_ok) {
      s.adapter = {};
    }
    if (s.node_ok || s.adapter_ok) {
      failures_ = 0;
    } else if (++failures_ >= kMaxConsecutiveFailures) {
      Stop();
      state_ = KmtState::kFailed;
    }
    return s;
  }

  void Stop() {
    if (open_) {
      api_->Close();
      open_ = false;
    }
  }

  KmtState state() const { return state_; }
  uint32_t node() const { return node_; }

 private:
  KmtApi *api_ = nullptr;
  KmtState state_ = KmtState::kOff;
  bool open_ = false;
  uint32_t node_ = 0;
  uint32_t failures_ = 0;
};

// ---------------------------------------------------------------------------
// MCLA_VRAM v=1 line.

struct MemorySample {
  uint64_t local_usage_mb = 0;
  uint64_t local_budget_mb = 0;
  uint64_t local_available_mb = 0;  // AvailableForReservation
  bool nonlocal_ok = false;
  uint64_t nonlocal_usage_mb = 0;
  uint64_t nonlocal_budget_mb = 0;
};

// The texture-cache limits and step counters as the monitor knows them.
struct MonitorView {
  uint64_t soft_mb = 0;
  uint64_t hard_mb = 0;
  uint64_t floor_mb = 0;
  uint64_t near_budget_steps = 0;    // hard-limit steps so far
  uint64_t near_budget_seconds = 0;  // 1 Hz samples within the headroom so far
  uint64_t peak_mb = 0;
  uint32_t scale = 1;
  bool steps_enabled = true;  // false when the user set the limits
};

// Keeps the line single and parseable: [A-Za-z0-9._-], at most 32 chars.
inline std::string SanitizeReason(std::string_view reason) {
  std::string text;
  for (const char c : reason.substr(0, std::min<size_t>(reason.size(), 32))) {
    const bool ok = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') ||
                    c == '.' || c == '_' || c == '-';
    text.push_back(ok ? c : '?');
  }
  return text.empty() ? std::string("none") : text;
}

inline std::string FormatSignedMb(int64_t value) {
  return value > 0 ? "+" + std::to_string(value) : std::to_string(value);
}

// min/avg/max of the non-zero samples (a driver that leaves a field at 0
// reports n/a).
struct Stat {
  double min = 0, max = 0, sum = 0;
  uint64_t count = 0;
  void Add(double value) {
    if (!(value > 0)) {
      return;
    }
    min = count ? std::min(min, value) : value;
    max = count ? std::max(max, value) : value;
    sum += value;
    ++count;
  }
  std::string Format(int decimals) const {
    if (!count) {
      return "n/a";
    }
    char text[96];
    std::snprintf(text, sizeof(text), "%.*f/%.*f/%.*f", decimals, min, decimals,
                  sum / double(count), decimals, max);
    return text;
  }
};

class CensusWindow {
 public:
  // 1 Hz (and at a mark): the latest DXGI reading.
  void AddMemory(const MemorySample &m) {
    if (!have_baseline_) {
      have_baseline_ = true;
      line_local_budget_ = m.local_budget_mb;
      line_nonlocal_budget_ = m.nonlocal_budget_mb;
    }
    last_ = m;
    have_memory_ = true;
    budget_min_ = memory_samples_ ? std::min(budget_min_, m.local_budget_mb) : m.local_budget_mb;
    budget_max_ = memory_samples_ ? std::max(budget_max_, m.local_budget_mb) : m.local_budget_mb;
    usage_max_ = memory_samples_ ? std::max(usage_max_, m.local_usage_mb) : m.local_usage_mb;
    if (m.nonlocal_ok) {
      nonlocal_max_ = std::max(nonlocal_max_, m.nonlocal_usage_mb);
    }
    ++memory_samples_;
  }

  void AddGpu(const GpuPerfSample &s) {
    if (s.node_ok) {
      gpu_mhz_.Add(double(s.node.frequency_hz) / 1e6);
      if (s.node.max_frequency_hz) {
        gpu_max_mhz_ = double(s.node.max_frequency_hz) / 1e6;
      }
    }
    if (s.adapter_ok) {
      mem_mhz_.Add(double(s.adapter.memory_frequency_hz) / 1e6);
      temp_c_.Add(double(s.adapter.temperature_deci_c) / 10.0);
      power_pct_.Add(double(s.adapter.power_tenth_pct) / 10.0);
      fan_rpm_.Add(double(s.adapter.fan_rpm));
    }
  }

  // Formats the line and starts the next window (deltas are against this
  // line's budgets). An empty string while no DXGI sample exists.
  std::string TakeLine(std::string_view reason, uint64_t seq, double window_s,
                       const MonitorView &view, KmtState kmt) {
    if (!have_memory_) {
      return {};
    }
    const MemorySample &m = last_;
    std::string line = "MCLA_VRAM v=1 reason=" + SanitizeReason(reason) +
                       " seq=" + std::to_string(seq);
    char window[32];
    std::snprintf(window, sizeof(window), "%.1f", window_s);
    line += std::string(" window_s=") + window;
    line += " samples=" + std::to_string(memory_samples_);
    line += " local_mb=" + std::to_string(m.local_usage_mb);
    line += " local_max_mb=" + std::to_string(usage_max_);
    line += " local_budget_mb=" + std::to_string(m.local_budget_mb);
    line += " local_budget_delta_mb=" +
            FormatSignedMb(int64_t(m.local_budget_mb) - int64_t(line_local_budget_));
    line += " local_budget_min_mb=" + std::to_string(budget_min_);
    line += " local_budget_max_mb=" + std::to_string(budget_max_);
    line += " local_avail_mb=" + std::to_string(m.local_available_mb);
    if (m.nonlocal_ok) {
      line += " nonlocal_mb=" + std::to_string(m.nonlocal_usage_mb);
      line += " nonlocal_max_mb=" + std::to_string(nonlocal_max_);
      line += " nonlocal_budget_mb=" + std::to_string(m.nonlocal_budget_mb);
      line += " nonlocal_budget_delta_mb=" +
              FormatSignedMb(int64_t(m.nonlocal_budget_mb) - int64_t(line_nonlocal_budget_));
    } else {
      line += " nonlocal_mb=n/a nonlocal_max_mb=n/a nonlocal_budget_mb=n/a "
              "nonlocal_budget_delta_mb=n/a";
    }
    line += " peak_mb=" + std::to_string(view.peak_mb);
    line += " tex_soft_mb=" + std::to_string(view.soft_mb);
    line += " tex_hard_mb=" + std::to_string(view.hard_mb);
    line += " tex_floor_mb=" + std::to_string(view.floor_mb);
    line += std::string(" steps=") + (view.steps_enabled ? "on" : "off");
    line += " near_budget_steps=" + std::to_string(view.near_budget_steps);
    line += " near_budget_s=" + std::to_string(view.near_budget_seconds);
    line += " scale=" + std::to_string(view.scale);
    line += std::string(" kmt=") + KmtStateName(kmt);
    line += " gpu_mhz=" + gpu_mhz_.Format(0);
    if (gpu_max_mhz_ > 0) {
      char max_text[32];
      std::snprintf(max_text, sizeof(max_text), "%.0f", gpu_max_mhz_);
      line += std::string(" gpu_max_mhz=") + max_text;
    } else {
      line += " gpu_max_mhz=n/a";
    }
    line += " mem_mhz=" + mem_mhz_.Format(0);
    line += " temp_c=" + temp_c_.Format(1);
    line += " power_pct=" + power_pct_.Format(1);
    line += " fan_rpm=" + fan_rpm_.Format(0);
    // Next window.
    line_local_budget_ = m.local_budget_mb;
    line_nonlocal_budget_ = m.nonlocal_budget_mb;
    memory_samples_ = 0;
    budget_min_ = budget_max_ = m.local_budget_mb;
    usage_max_ = m.local_usage_mb;
    nonlocal_max_ = m.nonlocal_ok ? m.nonlocal_usage_mb : 0;
    gpu_mhz_ = {};
    mem_mhz_ = {};
    temp_c_ = {};
    power_pct_ = {};
    fan_rpm_ = {};
    gpu_max_mhz_ = 0;
    return line;
  }

 private:
  MemorySample last_;
  bool have_memory_ = false;
  bool have_baseline_ = false;
  uint64_t line_local_budget_ = 0, line_nonlocal_budget_ = 0;
  uint64_t memory_samples_ = 0;
  uint64_t budget_min_ = 0, budget_max_ = 0, usage_max_ = 0, nonlocal_max_ = 0;
  Stat gpu_mhz_, mem_mhz_, temp_c_, power_pct_, fan_rpm_;
  double gpu_max_mhz_ = 0;
};

// The Windows D3DKMT implementation (gdi32 entry points resolved at run time;
// nullptr when they are missing). mcla_vram_census_win.cpp.
std::unique_ptr<KmtApi> CreateSystemKmtApi();

}  // namespace mcla::vram
