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
// - M11-002 / KI-067: one MCLA_CPU v=1 line next to each MCLA_VRAM line
//   (CpuReader / CpuWindow below; effective CPU clock, system and process CPU
//   load, ACPI thermal zone temperature when readable).
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

// l99 (mcla_texture_hard_margin_mb, docs/evidence/M11-l99-sdk-hitch-20261113.md):
// the monitor's hard-limit floor. margin 0 keeps the start floor (the starting
// soft limit); otherwise the floor is at least soft + margin, so a monitor
// that steps over the DXGI budget never collapses hard onto soft.
constexpr uint64_t HardFloorWithMargin(uint64_t floor_mb, uint64_t soft_mb, uint64_t margin_mb) {
  return margin_mb ? std::max(floor_mb, soft_mb + margin_mb) : floor_mb;
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

// ---------------------------------------------------------------------------
// M11-002 / KI-067 CPU census: one MCLA_CPU v=1 line next to each MCLA_VRAM
// line (same monitor thread, same reasons) while mcla_cpu_census is on (default
// on; only while the VRAM census runs, so never under Wine/Proton, where one
// "cpu=wine" line is logged at start instead). Sources, each optional and
// never fatal:
// - load: GetSystemTimes (system busy %) and GetProcessTimes (the game
//   process, % of all logical processors and in cores), 1 Hz deltas;
// - clock: PDH "\Processor Information(_Total)\% Processor Performance" x
//   "...\Processor Frequency" (nominal MHz) = effective MHz (what Task
//   Manager shows), 1 Hz; CallNtPowerInformation(ProcessorInformation)
//   CurrentMhz, MaxMhz and MhzLimit averaged over the logical processors as a
//   cross-check (CurrentMhz is often pinned to the nominal clock on Windows
//   10/11; MhzLimit drops under a power or thermal limit);
// - temperature: WMI ROOT\WMI MSAcpi_ThermalZoneTemperature, tried once at
//   start (usually needs administrator rights; often absent), the highest
//   thermal zone every 10 s when available. An ACPI thermal zone is not
//   necessarily the CPU package sensor (temp_src=acpi_tz says so).
// Each source is disabled after kMaxConsecutiveFailures failed samples.

struct CpuTimes {
  // GetSystemTimes, 100 ns units; kernel includes idle.
  uint64_t idle = 0, kernel = 0, user = 0;
  // GetProcessTimes of the game process, 100 ns units.
  uint64_t process_kernel = 0, process_user = 0;
  // Monotonic wall clock, 100 ns units.
  uint64_t wall = 0;
};

struct CpuLoad {
  bool valid = false;
  double system_pct = 0;     // 100 x (1 - idle / (kernel + user))
  double process_pct = 0;    // the process, % of all logical processors
  double process_cores = 0;  // the process, in logical processors
};

constexpr CpuLoad ComputeCpuLoad(const CpuTimes &prev, const CpuTimes &cur, uint32_t logical) {
  CpuLoad load;
  if (cur.idle < prev.idle || cur.kernel < prev.kernel || cur.user < prev.user ||
      cur.process_kernel < prev.process_kernel || cur.process_user < prev.process_user ||
      cur.wall <= prev.wall || logical == 0) {
    return load;
  }
  const uint64_t idle = cur.idle - prev.idle;
  const uint64_t total = (cur.kernel - prev.kernel) + (cur.user - prev.user);
  if (total == 0 || idle > total) {
    return load;
  }
  const uint64_t process =
      (cur.process_kernel - prev.process_kernel) + (cur.process_user - prev.process_user);
  const double wall = double(cur.wall - prev.wall);
  load.system_pct = 100.0 * double(total - idle) / double(total);
  load.process_cores = double(process) / wall;
  load.process_pct = 100.0 * load.process_cores / double(logical);
  load.valid = true;
  return load;
}

// Effective clock from the PDH pair (0 when either is missing).
constexpr double EffectiveMhz(double performance_pct, double nominal_mhz) {
  return performance_pct > 0 && nominal_mhz > 0 ? performance_pct * nominal_mhz / 100.0 : 0.0;
}

// ACPI thermal zone CurrentTemperature (tenths of a kelvin) to Celsius; -1000
// (not a temperature) for 0 or implausible values (below -50 C or above
// 150 C).
inline constexpr double kNoTemperature = -1000.0;
constexpr double ThermalZoneCelsius(uint32_t tenths_kelvin) {
  const double c = double(tenths_kelvin) / 10.0 - 273.15;
  return tenths_kelvin != 0 && c > -50.0 && c < 150.0 ? c : kNoTemperature;
}

struct PowerInfo {
  double current_mhz = 0;  // averages over the logical processors
  double max_mhz = 0;
  double limit_mhz = 0;
};

// The Windows calls behind an interface (fake in the tests).
class CpuApi {
 public:
  virtual ~CpuApi() = default;
  virtual uint32_t LogicalProcessors() = 0;
  virtual bool Times(CpuTimes &out) = 0;
  // PDH query with the two counters (OpenPdh also primes it).
  virtual bool OpenPdh() = 0;
  virtual bool QueryPdh(double &performance_pct, double &nominal_mhz) = 0;
  virtual void ClosePdh() = 0;
  virtual bool QueryPower(PowerInfo &out) = 0;
  // WMI; `status` receives the HRESULT of the failing step for the log.
  virtual bool OpenTemperature(uint32_t &status) = 0;
  // The highest valid thermal zone in Celsius.
  virtual bool QueryTemperature(double &celsius) = 0;
  virtual void CloseTemperature() = 0;
};

enum class SourceState : uint8_t {
  kOff,          // not started
  kUnavailable,  // the open or the first query failed
  kOk,
  kFailed,       // disabled after repeated failures
};

constexpr const char *SourceStateName(SourceState state) {
  switch (state) {
    case SourceState::kOff:
      return "off";
    case SourceState::kUnavailable:
      return "unavailable";
    case SourceState::kOk:
      return "ok";
    case SourceState::kFailed:
      return "failed";
  }
  return "unknown";
}

enum class CpuState : uint8_t {
  kOff,   // census off or no API
  kWine,  // Wine/Proton: never called
  kOk,
};

constexpr const char *CpuStateName(CpuState state) {
  switch (state) {
    case CpuState::kOff:
      return "off";
    case CpuState::kWine:
      return "wine";
    case CpuState::kOk:
      return "ok";
  }
  return "unknown";
}

struct CpuSample {
  bool load_ok = false;
  CpuLoad load;
  bool pdh_ok = false;
  double performance_pct = 0;
  double nominal_mhz = 0;
  bool power_ok = false;
  PowerInfo power;
  bool temperature_ok = false;
  double temperature_c = 0;
};

class CpuReader {
 public:
  static constexpr uint32_t kMaxConsecutiveFailures = 5;
  static constexpr uint64_t kTemperatureEveryTicks = 10;  // 10 s at 1 Hz

  explicit CpuReader(CpuApi *api) : api_(api) {}
  CpuReader(const CpuReader &) = delete;
  CpuReader &operator=(const CpuReader &) = delete;
  ~CpuReader() { Stop(); }

  CpuState Start(bool wine) {
    Stop();
    if (wine) {
      return state_ = CpuState::kWine;
    }
    if (!api_) {
      return state_ = CpuState::kOff;
    }
    logical_ = api_->LogicalProcessors();
    have_prev_ = api_->Times(prev_);
    times_ = have_prev_ ? SourceState::kOk : SourceState::kUnavailable;
    pdh_open_ = api_->OpenPdh();
    pdh_ = pdh_open_ ? SourceState::kOk : SourceState::kUnavailable;
    PowerInfo probe;
    power_ = api_->QueryPower(probe) ? SourceState::kOk : SourceState::kUnavailable;
    temperature_status_ = 0;
    temperature_ = SourceState::kUnavailable;
    if (api_->OpenTemperature(temperature_status_)) {
      temperature_open_ = true;
      double c = 0;
      if (api_->QueryTemperature(c) && c > kNoTemperature) {
        temperature_ = SourceState::kOk;
      } else {
        api_->CloseTemperature();
        temperature_open_ = false;
      }
    }
    times_failures_ = pdh_failures_ = power_failures_ = temperature_failures_ = 0;
    ticks_ = 0;
    return state_ = CpuState::kOk;
  }

  // Once per second on the monitor thread.
  CpuSample Sample() {
    CpuSample s;
    if (state_ != CpuState::kOk) {
      return s;
    }
    ++ticks_;
    if (times_ == SourceState::kOk || times_ == SourceState::kUnavailable) {
      CpuTimes cur;
      if (api_->Times(cur)) {
        if (have_prev_) {
          s.load = ComputeCpuLoad(prev_, cur, logical_);
          s.load_ok = s.load.valid;
        }
        prev_ = cur;
        have_prev_ = true;
        times_ = SourceState::kOk;
        times_failures_ = 0;
      } else {
        Fail(times_, times_failures_);
      }
    }
    if (pdh_ == SourceState::kOk) {
      if (api_->QueryPdh(s.performance_pct, s.nominal_mhz)) {
        s.pdh_ok = true;
        pdh_failures_ = 0;
      } else {
        s.performance_pct = s.nominal_mhz = 0;
        if (Fail(pdh_, pdh_failures_)) {
          api_->ClosePdh();
          pdh_open_ = false;
        }
      }
    }
    if (power_ == SourceState::kOk) {
      if (api_->QueryPower(s.power)) {
        s.power_ok = true;
        power_failures_ = 0;
      } else {
        s.power = {};
        Fail(power_, power_failures_);
      }
    }
    if (temperature_ == SourceState::kOk && ticks_ % kTemperatureEveryTicks == 1) {
      double c = 0;
      if (api_->QueryTemperature(c) && c > kNoTemperature) {
        s.temperature_ok = true;
        s.temperature_c = c;
        temperature_failures_ = 0;
      } else if (Fail(temperature_, temperature_failures_)) {
        api_->CloseTemperature();
        temperature_open_ = false;
      }
    }
    return s;
  }

  void Stop() {
    if (pdh_open_) {
      api_->ClosePdh();
      pdh_open_ = false;
    }
    if (temperature_open_) {
      api_->CloseTemperature();
      temperature_open_ = false;
    }
  }

  CpuState state() const { return state_; }
  uint32_t logical() const { return logical_; }
  SourceState times() const { return times_; }
  SourceState pdh() const { return pdh_; }
  SourceState power() const { return power_; }
  SourceState temperature() const { return temperature_; }
  uint32_t temperature_status() const { return temperature_status_; }

 private:
  // Counts a failure; true when the source was just disabled.
  static bool Fail(SourceState &state, uint32_t &failures) {
    if (++failures >= kMaxConsecutiveFailures) {
      state = SourceState::kFailed;
      return true;
    }
    return false;
  }

  CpuApi *api_ = nullptr;
  CpuState state_ = CpuState::kOff;
  uint32_t logical_ = 0;
  CpuTimes prev_;
  bool have_prev_ = false;
  bool pdh_open_ = false, temperature_open_ = false;
  SourceState times_ = SourceState::kOff, pdh_ = SourceState::kOff, power_ = SourceState::kOff,
              temperature_ = SourceState::kOff;
  uint32_t times_failures_ = 0, pdh_failures_ = 0, power_failures_ = 0,
           temperature_failures_ = 0;
  uint32_t temperature_status_ = 0;
  uint64_t ticks_ = 0;
};

// min/avg/max of values >= 0 (a CPU load can be 0; Stat drops zeros).
struct StatZero {
  double min = 0, max = 0, sum = 0;
  uint64_t count = 0;
  void Add(double value) {
    if (!(value >= 0) || value > 1e12) {
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

struct CpuSources {
  CpuState state = CpuState::kOff;
  uint32_t logical = 0;
  SourceState times = SourceState::kOff;
  SourceState pdh = SourceState::kOff;
  SourceState power = SourceState::kOff;
  SourceState temperature = SourceState::kOff;
};

inline CpuSources SourcesOf(const CpuReader &reader) {
  CpuSources src;
  src.state = reader.state();
  src.logical = reader.logical();
  src.times = reader.times();
  src.pdh = reader.pdh();
  src.power = reader.power();
  src.temperature = reader.temperature();
  return src;
}

class CpuWindow {
 public:
  void Add(const CpuSample &s) {
    ++samples_;
    if (s.load_ok) {
      system_pct_.Add(s.load.system_pct);
      process_pct_.Add(s.load.process_pct);
      process_cores_.Add(s.load.process_cores);
    }
    if (s.pdh_ok) {
      performance_pct_.Add(s.performance_pct);
      const double mhz = EffectiveMhz(s.performance_pct, s.nominal_mhz);
      if (mhz > 0) {
        effective_mhz_.Add(mhz);
      }
      if (s.nominal_mhz > 0) {
        nominal_mhz_ = s.nominal_mhz;
      }
    }
    if (s.power_ok) {
      if (s.power.current_mhz > 0) {
        power_mhz_.Add(s.power.current_mhz);
      }
      if (s.power.limit_mhz > 0) {
        power_limit_mhz_.Add(s.power.limit_mhz);
      }
      if (s.power.max_mhz > 0) {
        power_max_mhz_ = s.power.max_mhz;
      }
    }
    if (s.temperature_ok) {
      temperature_c_.Add(s.temperature_c);
    }
  }

  // Formats the line and starts the next window.
  std::string TakeLine(std::string_view reason, uint64_t seq, double window_s,
                       const CpuSources &src) {
    std::string line = "MCLA_CPU v=1 reason=" + SanitizeReason(reason) +
                       " seq=" + std::to_string(seq);
    char window[32];
    std::snprintf(window, sizeof(window), "%.1f", window_s);
    line += std::string(" window_s=") + window;
    line += " samples=" + std::to_string(samples_);
    line += std::string(" cpu=") + CpuStateName(src.state);
    line += " logical=" + (src.logical ? std::to_string(src.logical) : std::string("n/a"));
    line += std::string(" times=") + SourceStateName(src.times);
    line += std::string(" pdh=") + SourceStateName(src.pdh);
    line += std::string(" power=") + SourceStateName(src.power);
    line += std::string(" temp=") + SourceStateName(src.temperature);
    line += " nominal_mhz=" + Number(nominal_mhz_);
    line += " eff_mhz=" + effective_mhz_.Format(0);
    line += " perf_pct=" + performance_pct_.Format(1);
    line += " ntpi_mhz=" + power_mhz_.Format(0);
    line += " ntpi_max_mhz=" + Number(power_max_mhz_);
    line += " ntpi_limit_mhz=" + power_limit_mhz_.Format(0);
    line += " sys_pct=" + system_pct_.Format(1);
    line += " proc_pct=" + process_pct_.Format(1);
    line += " proc_cores=" + process_cores_.Format(2);
    line += " temp_src=acpi_tz temp_c=" + temperature_c_.Format(1);
    *this = CpuWindow{};
    return line;
  }

 private:
  static std::string Number(double value) {
    if (!(value > 0)) {
      return "n/a";
    }
    char text[32];
    std::snprintf(text, sizeof(text), "%.0f", value);
    return text;
  }

  uint64_t samples_ = 0;
  StatZero system_pct_, process_pct_, process_cores_, performance_pct_, effective_mhz_,
      power_mhz_, power_limit_mhz_, temperature_c_;
  double nominal_mhz_ = 0, power_max_mhz_ = 0;
};

// The single line logged when no CPU census runs (Wine/Proton, no monitor).
inline std::string CpuUnavailableLine(std::string_view reason, CpuState state) {
  CpuWindow empty;
  CpuSources src;
  src.state = state;
  return empty.TakeLine(reason, 0, 0.0, src);
}

// The Windows implementation (kernel32 times; pdh.dll and powrprof.dll entry
// points resolved at run time; WMI through COM on the calling thread, which
// must be the monitor thread). Never nullptr; each call fails cleanly when
// its source is missing. mcla_vram_census_win.cpp.
std::unique_ptr<CpuApi> CreateSystemCpuApi();

}  // namespace mcla::vram
