// M10 volunteer speed-up defaults (l65). See mcla_m10_defaults.h.

#include "mcla_m10_defaults.h"

#include "mcla_dev_overrides.h"
#include "mcla_logging.h"

#include <rex/cvar.h>

#include <string>

#ifndef MCLA_M10_SPEEDUP_DEFAULTS
#define MCLA_M10_SPEEDUP_DEFAULTS 0
#endif

#ifndef MCLA_DEV_OVERRIDES
#define MCLA_DEV_OVERRIDES 0
#endif

#if MCLA_M10_SPEEDUP_DEFAULTS
REXCVAR_DEFINE_BOOL(mcla_m10_speedup_defaults, true, "MCLA",
    "M10 volunteer speed-up defaults (targeted read-back wait, read-back "
    "elision, WAIT_REG_MEM event wake, batched replay wake; census and "
    "read-back GPU timing off). false leaves every SDK switch at its default")
    .lifecycle(rex::cvar::Lifecycle::kInitOnly);
#endif

namespace mcla::m10_defaults {
namespace {

#if MCLA_M10_SPEEDUP_DEFAULTS

std::string ReportValue(std::string_view name) {
  const std::string cvar(name);
  if (!rex::cvar::GetFlagInfo(cvar)) {
    return "unavailable";
  }
  return rex::cvar::GetFlagByName(cvar);
}

Tally ApplyPhase(Phase phase) {
  const rex::LogCategoryId category =
      phase == Phase::kStartup ? ::mcla::logging::App() : ::mcla::logging::Gpu();
  Tally tally;
  for (const Default &entry : kDefaults) {
    if (entry.phase != phase) {
      continue;
    }
    const std::string name(entry.cvar);
    const std::string text(ValueText(entry.value));
    Inputs in;
    in.option_on = true;
    in.switch_on = REXCVAR_GET(mcla_m10_speedup_defaults);
    in.measurement_build = MCLA_DEV_OVERRIDES != 0;
    in.registered = rex::cvar::GetFlagInfo(name) != nullptr;
    in.non_default = in.registered && rex::cvar::HasNonDefaultValue(name);
    in.dev_override_applied =
        entry.override_key && dev_overrides::AppliedValue(*entry.override_key).has_value();
    in.holds_value = in.registered && rex::cvar::GetFlagByName(name) == text;
    const Action action = Decide(entry, in);
    tally.Add(action);
    switch (action) {
      case Action::kDisabledByOption:
      case Action::kOffBySwitch:
        break;
      case Action::kApply:
        if (rex::cvar::SetFlagByName(name, text)) {
          REXLOG_CAT_INFO(category, "MCLA m10 default: {}={}", name, text);
        } else {
          ++tally.failed;
          REXLOG_CAT_ERROR(category, "MCLA m10 defaults: could not set {}={}", name, text);
        }
        break;
      case Action::kUnavailable:
        REXLOG_CAT_INFO(category, "MCLA m10 defaults: {} unavailable in this build", name);
        break;
      default:
        REXLOG_CAT_INFO(category, "MCLA m10 defaults: {} left at {} ({})", name,
                        rex::cvar::GetFlagByName(name), ActionName(action));
        break;
    }
  }
  return tally;
}

std::string TallyText(const Tally &tally) {
  return "applied=" + std::to_string(tally.Count(Action::kApply) - tally.failed) +
         " failed=" + std::to_string(tally.failed) +
         " user_set=" + std::to_string(tally.Count(Action::kUserSet)) +
         " dev_override=" + std::to_string(tally.Count(Action::kDevOverride)) +
         " measurement_build=" + std::to_string(tally.Count(Action::kMeasurementBuild)) +
         " already=" + std::to_string(tally.Count(Action::kAlready)) +
         " unavailable=" + std::to_string(tally.Count(Action::kUnavailable));
}

#endif  // MCLA_M10_SPEEDUP_DEFAULTS

}  // namespace

void OnStartup() {
#if MCLA_M10_SPEEDUP_DEFAULTS
  if (!REXCVAR_GET(mcla_m10_speedup_defaults)) {
    MCLA_APP_INFO("MCLA m10 defaults: off ({}=false); SDK defaults kept", kSwitchCvar);
    return;
  }
  const Tally tally = ApplyPhase(Phase::kStartup);
  MCLA_APP_INFO(
      "MCLA m10 defaults (startup): {} mcla_native_d3d_refcount={} "
      "mcla_native_xapi_interlocked={}",
      TallyText(tally), ReportValue("mcla_native_d3d_refcount"),
      ReportValue("mcla_native_xapi_interlocked"));
#endif
}

void OnGraphicsPluginLoaded() {
#if MCLA_M10_SPEEDUP_DEFAULTS
  if (!REXCVAR_GET(mcla_m10_speedup_defaults)) {
    return;
  }
  const Tally tally = ApplyPhase(Phase::kGraphicsPlugin);
  MCLA_GPU_INFO(
      "MCLA m10 defaults (graphics plugin): {} d3d12_readback_sync_composite={} "
      "readback_resolve={} memory_watch_disarm_bridge_safe_gaps={} "
      "shared_memory_upload_ahead_pages={}",
      TallyText(tally), ReportValue("d3d12_readback_sync_composite"),
      ReportValue("readback_resolve"), ReportValue(kNeverWritten[0]),
      ReportValue(kNeverWritten[1]));
#endif
}

}  // namespace mcla::m10_defaults
