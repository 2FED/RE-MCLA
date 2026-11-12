// M10 volunteer speed-up defaults (l65; Wine/Proton entries l66). See
// mcla_m10_defaults.h.

#include "mcla_m10_defaults.h"

#include "mcla_dev_overrides.h"
#include "mcla_logging.h"

#if defined(_WIN32)
#include "mcla_host_platform_win.h"
#endif

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
    "elision, WAIT_REG_MEM event wake, batched replay wake, strict frame lead "
    "bound; census and read-back GPU timing off; under Wine/Proton also disarm "
    "bridging and upload-ahead 16). false leaves every SDK switch at its "
    "default")
    .lifecycle(rex::cvar::Lifecycle::kInitOnly);
#endif

namespace mcla::m10_defaults {
namespace {

#if MCLA_M10_SPEEDUP_DEFAULTS

bool RunningUnderWine() {
#if defined(_WIN32)
  return ::mcla::platform::RunningUnderWine();
#else
  return false;
#endif
}

std::string ReportValue(std::string_view name) {
  const std::string cvar(name);
  if (!rex::cvar::GetFlagInfo(cvar)) {
    return "unavailable";
  }
  return rex::cvar::GetFlagByName(cvar);
}

// An owner override key of this cvar was applied, or its valid file line is an
// explicit value that writes nothing and the entry treats it as the owner's
// "off" arm (upload-ahead _16/_64=false); see KeyLineWins.
bool DevOverrideApplied(const Default &entry) {
  for (size_t i = 0; i < dev_overrides::kKeyCount; ++i) {
    const auto key = static_cast<dev_overrides::Key>(i);
    if (KeyLineWins(entry, dev_overrides::Info(key), dev_overrides::AppliedValue(key),
                    dev_overrides::FileValue(key))) {
      return true;
    }
  }
  return false;
}

Tally ApplyPhase(Phase phase, bool wine) {
  const rex::LogCategoryId category =
      phase == Phase::kStartup ? ::mcla::logging::App() : ::mcla::logging::Gpu();
  Tally tally;
  for (const Default &entry : kDefaults) {
    if (entry.phase != phase) {
      continue;
    }
    const std::string name(entry.cvar);
    const std::string text(entry.value);
    Inputs in;
    in.option_on = true;
    in.switch_on = REXCVAR_GET(mcla_m10_speedup_defaults);
    in.wine = wine;
    in.measurement_build = MCLA_DEV_OVERRIDES != 0;
    in.registered = rex::cvar::GetFlagInfo(name) != nullptr;
    in.non_default = in.registered && rex::cvar::HasNonDefaultValue(name);
    in.dev_override_applied = DevOverrideApplied(entry);
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
                        ReportValue(name), ActionName(action));
        break;
    }
  }
  return tally;
}

std::string TallyText(const Tally &tally, bool wine) {
  return "platform=" + std::string(PlatformName(wine)) +
         " applied=" + std::to_string(tally.Count(Action::kApply) - tally.failed) +
         " failed=" + std::to_string(tally.failed) +
         " user_set=" + std::to_string(tally.Count(Action::kUserSet)) +
         " dev_override=" + std::to_string(tally.Count(Action::kDevOverride)) +
         " measurement_build=" + std::to_string(tally.Count(Action::kMeasurementBuild)) +
         " wine_only=" + std::to_string(tally.Count(Action::kNotThisPlatform)) +
         " already=" + std::to_string(tally.Count(Action::kAlready)) +
         " unavailable=" + std::to_string(tally.Count(Action::kUnavailable));
}

#endif  // MCLA_M10_SPEEDUP_DEFAULTS

}  // namespace

void OnStartup() {
#if MCLA_M10_SPEEDUP_DEFAULTS
  const bool wine = RunningUnderWine();
  if (!REXCVAR_GET(mcla_m10_speedup_defaults)) {
    MCLA_APP_INFO("MCLA m10 defaults: off ({}=false); SDK defaults kept platform={}", kSwitchCvar,
                  PlatformName(wine));
    return;
  }
  const Tally tally = ApplyPhase(Phase::kStartup, wine);
  MCLA_APP_INFO(
      "MCLA m10 defaults (startup): {} mcla_native_d3d_refcount={} "
      "mcla_native_xapi_interlocked={} memory_watch_disarm_bridge_safe_gaps={}",
      TallyText(tally, wine), ReportValue("mcla_native_d3d_refcount"),
      ReportValue("mcla_native_xapi_interlocked"), ReportValue(kWineOnlyCvars[0]));
#endif
}

void OnGraphicsPluginLoaded() {
#if MCLA_M10_SPEEDUP_DEFAULTS
  if (!REXCVAR_GET(mcla_m10_speedup_defaults)) {
    return;
  }
  const bool wine = RunningUnderWine();
  const Tally tally = ApplyPhase(Phase::kGraphicsPlugin, wine);
  MCLA_GPU_INFO(
      "MCLA m10 defaults (graphics plugin): {} d3d12_readback_sync_composite={} "
      "readback_resolve={} memory_watch_disarm_bridge_safe_gaps={} "
      "shared_memory_upload_ahead_pages={} d3d12_frame_lead_strict={}",
      TallyText(tally, wine), ReportValue("d3d12_readback_sync_composite"),
      ReportValue("readback_resolve"), ReportValue(kWineOnlyCvars[0]),
      ReportValue(kWineOnlyCvars[1]), ReportValue("d3d12_frame_lead_strict"));
#endif
}

}  // namespace mcla::m10_defaults
