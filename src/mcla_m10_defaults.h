#pragma once

// M10 volunteer speed-up defaults (l65;
// docs/evidence/M10-volunteer-speedup-defaults-20261102.md, measured in the
// owner's l63 PC A/B, docs/evidence/M10-l63-pc-ab-20261102.md; Wine/Proton
// entries l66, docs/evidence/M10-proton-defaults-and-ki064-20261103.md,
// measured in the owner's l64 Deck A/B, docs/evidence/M10-l64-deck-ab-20261103.md).
//
// Builds made with the CMake option MCLA_M10_SPEEDUP_DEFAULTS (OFF by
// default) write the SDK switches of kDefaults below, so a volunteer build
// ships the speed-ups that the owner's A/B proved without the owner-internal
// override file:
//
// - at startup (rexruntime cvars, after dev_overrides::OnStartup):
//   memory_watch_census=false (counters nobody reads in volunteer logs);
//   under Wine/Proton only: memory_watch_disarm_bridge_safe_gaps=true;
// - at graphics-plugin load (after dev_overrides::OnGraphicsPluginLoaded,
//   the KI-047 composite sync and the readback_resolve=some default):
//   d3d12_readback_sync_targeted=true, d3d12_readback_elide_unread=true,
//   d3d12_wrm_event_wake=true, d3d12_replay_batched_wake=true,
//   d3d12_readback_gpu_timing=false; under Wine/Proton only:
//   shared_memory_upload_ahead_pages=16.
//
// Each phase is the phase of the owner override key for the same cvar.
//
// A value is written only when the cvar is registered, still holds its
// default (the command line or config did not change it), and the owner
// override file (MCLA_DEV_OVERRIDES builds) did not write it: owner A/B arms
// and explicit user values always win. For upload-ahead, an override line
// shared_memory_upload_ahead_16=false or _64=false writes nothing itself but
// is the owner's "off" arm, so it also wins. The two diagnostic switches
// (memory_watch_census, d3d12_readback_gpu_timing) are left at the SDK default
// in MCLA_DEV_OVERRIDES (owner measurement) builds. The host switch
// --mcla_m10_speedup_defaults=false turns the whole set off for a session.
//
// kNeverWrittenCvars lists switches this module must not write: diagnostics,
// switches owned by other host modules (also the KI-035 warp probe and
// watchdog, owner builds only), the Vulkan backend, and the SDK hot
// blocks (M10 watch step 3, docs/evidence/M10-hot-blocks-host-keys-20261103.md)
// until an owner A/B accepts them; until then only the owner override keys
// shared_memory_hot_blocks_shadow/_on set shared_memory_hot_blocks.
//
// Wine-only entries (Platform::kWineOnly): disarm bridging and upload-ahead 16
// were the two largest Deck gains in l64 (upload-ahead CP -8.3 ms/frame,
// bridging -3.3 ms/frame) but net negative on Windows in l63, so on Windows
// they are never written. platform::RunningUnderWine() decides; the summary
// lines say platform=wine|windows. The host init-only switches
// mcla_native_d3d_refcount and mcla_native_xapi_interlocked already default to
// true (mcla_native_refcount.cpp) and are only reported.
//
// Semantics kept from the SDK: targeted sync acts only while
// d3d12_readback_sync_composite is on (MCLA_READBACK_SYNC_COMPOSITE_DEFAULT);
// elision acts only in readback_resolve=some (the host default). Both are
// reported in the summary line.
//
// The table and the decision are pure and unit-tested
// (tests/mcla_m10_defaults_test.cpp); cvar calls are in mcla_m10_defaults.cpp.

#include "mcla_dev_overrides.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <string_view>

namespace mcla::m10_defaults {

inline constexpr std::string_view kSwitchCvar = "mcla_m10_speedup_defaults";

enum class Phase : uint8_t {
  kStartup,
  kGraphicsPlugin,
};

enum class Platform : uint8_t {
  kAll,       // Windows and Wine/Proton
  kWineOnly,  // only under Wine/Proton (net negative on Windows, l63)
};

struct Default {
  std::string_view cvar;
  std::string_view value;  // the text written
  Phase phase;
  // Diagnostics only: left at the SDK default in owner measurement builds.
  bool diagnostic;
  Platform platform = Platform::kAll;
};

inline constexpr std::array<Default, 8> kDefaults = {{
    {"memory_watch_census", "false", Phase::kStartup, true},
    {"memory_watch_disarm_bridge_safe_gaps", "true", Phase::kStartup, false, Platform::kWineOnly},
    {"d3d12_readback_sync_targeted", "true", Phase::kGraphicsPlugin, false},
    {"d3d12_readback_elide_unread", "true", Phase::kGraphicsPlugin, false},
    {"d3d12_wrm_event_wake", "true", Phase::kGraphicsPlugin, false},
    {"d3d12_replay_batched_wake", "true", Phase::kGraphicsPlugin, false},
    {"d3d12_readback_gpu_timing", "false", Phase::kGraphicsPlugin, true},
    {"shared_memory_upload_ahead_pages", "16", Phase::kGraphicsPlugin, false,
     Platform::kWineOnly},
}};

// Cvars this module writes only under Wine/Proton, never on Windows.
inline constexpr std::array<std::string_view, 2> kWineOnlyCvars = {
    "memory_watch_disarm_bridge_safe_gaps",
    "shared_memory_upload_ahead_pages",
};

// Cvars never written by this module (see above). The unit test checks that
// none is in kDefaults and that every non-launcher override key writes either
// a kDefaults cvar or one of these.
inline constexpr std::array<std::string_view, 14> kNeverWrittenCvars = {
    "memory_watch_probes",
    "mcla_readback_sync_composite",
    "d3d12_readback_sync_composite",
    "d3d12_readback_sync_kick_draws",
    "mcla_native_d3d_refcount",
    "mcla_native_xapi_interlocked",
    "mcla_gpu_backend_vulkan",
    "shared_memory_hot_blocks",
    "shared_memory_hot_k",
    "shared_memory_hot_n",
    "shared_memory_hot_upload_cap_kb",
    "shared_memory_hot_probe_frames",
    "mcla_warp_probe",
    "mcla_warp_watchdog_seconds",
};

constexpr bool IsNeverWritten(std::string_view cvar) {
  for (const auto name : kNeverWrittenCvars) {
    if (name == cvar) {
      return true;
    }
  }
  return false;
}

constexpr bool DefaultsAvoidNeverWritten() {
  for (const auto &entry : kDefaults) {
    if (IsNeverWritten(entry.cvar)) {
      return false;
    }
  }
  return true;
}
static_assert(DefaultsAvoidNeverWritten(), "a never-written cvar is in kDefaults");

// True when an owner override key writes the entry's cvar (its line is the
// owner's A/B arm and wins).
constexpr bool HasOverrideKey(const Default &entry) {
  for (const auto &info : dev_overrides::kKeys) {
    if (info.phase != dev_overrides::Phase::kLauncher &&
        dev_overrides::CvarName(info) == entry.cvar) {
      return true;
    }
  }
  return false;
}

constexpr std::string_view ValueText(bool value) {
  return value ? "true" : "false";
}

constexpr std::string_view PlatformName(bool wine) {
  return wine ? "wine" : "windows";
}

enum class Action : uint8_t {
  kDisabledByOption,  // build without MCLA_M10_SPEEDUP_DEFAULTS
  kOffBySwitch,       // --mcla_m10_speedup_defaults=false
  kNotThisPlatform,   // Wine-only entry on Windows
  kMeasurementBuild,  // diagnostic switch in an MCLA_DEV_OVERRIDES build
  kDevOverride,       // the owner override file wrote (or set off) this cvar
  kUnavailable,       // the cvar is not registered (e.g. an older SDK)
  kUserSet,           // the command line or config changed the cvar
  kAlready,           // the cvar already holds the value (SDK default equal)
  kApply,             // write the value
};
inline constexpr size_t kActionCount = 9;

struct Inputs {
  bool option_on = false;          // MCLA_M10_SPEEDUP_DEFAULTS
  bool switch_on = true;           // mcla_m10_speedup_defaults
  bool wine = false;               // platform::RunningUnderWine()
  bool measurement_build = false;  // MCLA_DEV_OVERRIDES
  bool registered = false;         // rex::cvar::GetFlagInfo != nullptr
  bool non_default = false;        // rex::cvar::HasNonDefaultValue
  // An override key of this cvar was applied, or its file line is an explicit
  // "off" that writes nothing (shared_memory_upload_ahead_16/_64=false).
  bool dev_override_applied = false;
  bool holds_value = false;  // the current value already equals the default's
};

constexpr Action Decide(const Default &entry, const Inputs &in) {
  if (!in.option_on) {
    return Action::kDisabledByOption;
  }
  if (!in.switch_on) {
    return Action::kOffBySwitch;
  }
  if (entry.platform == Platform::kWineOnly && !in.wine) {
    return Action::kNotThisPlatform;
  }
  if (entry.diagnostic && in.measurement_build) {
    return Action::kMeasurementBuild;
  }
  // A file value equal to the SDK default is not visible as non-default; the
  // applied record is what keeps such an A/B arm.
  if (HasOverrideKey(entry) && in.dev_override_applied) {
    return Action::kDevOverride;
  }
  if (!in.registered) {
    return Action::kUnavailable;
  }
  if (in.non_default) {
    return Action::kUserSet;
  }
  if (in.holds_value) {
    return Action::kAlready;
  }
  return Action::kApply;
}

constexpr const char *ActionName(Action action) {
  switch (action) {
    case Action::kDisabledByOption:
      return "disabled";
    case Action::kOffBySwitch:
      return "switch_off";
    case Action::kNotThisPlatform:
      return "wine_only";
    case Action::kMeasurementBuild:
      return "measurement_build";
    case Action::kDevOverride:
      return "dev_override";
    case Action::kUnavailable:
      return "unavailable";
    case Action::kUserSet:
      return "user_set";
    case Action::kAlready:
      return "already";
    case Action::kApply:
      return "applied";
  }
  return "unknown";
}

// Per-phase tally for the summary line.
struct Tally {
  std::array<size_t, kActionCount> counts{};
  size_t failed = 0;  // kApply whose write was refused

  constexpr void Add(Action action) { ++counts[static_cast<size_t>(action)]; }
  constexpr size_t Count(Action action) const { return counts[static_cast<size_t>(action)]; }
};

// Runtime (mcla_m10_defaults.cpp). Both calls do nothing in builds without
// MCLA_M10_SPEEDUP_DEFAULTS.

// Phase::kStartup defaults. Call after dev_overrides::OnStartup().
void OnStartup();

// Phase::kGraphicsPlugin defaults. Call once after the graphics plugin has
// been loaded, after dev_overrides::OnGraphicsPluginLoaded() and the
// readback_sync and readback_resolve defaults.
void OnGraphicsPluginLoaded();

}  // namespace mcla::m10_defaults
