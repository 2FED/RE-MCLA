#pragma once

// M10 volunteer speed-up defaults (l65;
// docs/evidence/M10-volunteer-speedup-defaults-20261102.md, measured in the
// owner's l63 PC A/B, docs/evidence/M10-l63-pc-ab-20261102.md).
//
// Builds made with the CMake option MCLA_M10_SPEEDUP_DEFAULTS (OFF by
// default) write the SDK switches of kDefaults below, so a volunteer build
// ships the speed-ups that the owner's A/B proved without the owner-internal
// override file:
//
// - at startup (rexruntime cvars, after dev_overrides::OnStartup):
//   memory_watch_census=false (counters nobody reads in volunteer logs);
// - at graphics-plugin load (after dev_overrides::OnGraphicsPluginLoaded,
//   the KI-047 composite sync and the readback_resolve=some default):
//   d3d12_readback_sync_targeted=true, d3d12_readback_elide_unread=true,
//   d3d12_wrm_event_wake=true, d3d12_replay_batched_wake=true,
//   d3d12_readback_gpu_timing=false.
//
// A value is written only when the cvar is registered, still holds its
// default (the command line or config did not change it), and the owner
// override file (MCLA_DEV_OVERRIDES builds) did not write it: owner A/B arms
// and explicit user values always win. The two diagnostic switches
// (memory_watch_census, d3d12_readback_gpu_timing) are left at the SDK default
// in MCLA_DEV_OVERRIDES (owner measurement) builds. The host switch
// --mcla_m10_speedup_defaults=false turns the whole set off for a session.
//
// Not written (net negative on PC in l63): memory_watch_disarm_bridge_safe_gaps
// stays false and shared_memory_upload_ahead_pages stays 0. The host init-only
// switches mcla_native_d3d_refcount and mcla_native_xapi_interlocked already
// default to true (mcla_native_refcount.cpp) and are only reported.
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
#include <optional>
#include <string_view>

namespace mcla::m10_defaults {

inline constexpr std::string_view kSwitchCvar = "mcla_m10_speedup_defaults";

enum class Phase : uint8_t {
  kStartup,
  kGraphicsPlugin,
};

struct Default {
  std::string_view cvar;
  bool value;
  Phase phase;
  // Diagnostics only: left at the SDK default in owner measurement builds.
  bool diagnostic;
  // The owner override file key writing the same cvar, if any.
  std::optional<dev_overrides::Key> override_key;
};

inline constexpr std::array<Default, 6> kDefaults = {{
    {"memory_watch_census", false, Phase::kStartup, true, dev_overrides::Key::kWatchCensus},
    {"d3d12_readback_sync_targeted", true, Phase::kGraphicsPlugin, false,
     dev_overrides::Key::kReadbackSyncTargeted},
    {"d3d12_readback_elide_unread", true, Phase::kGraphicsPlugin, false,
     dev_overrides::Key::kReadbackElideUnread},
    {"d3d12_wrm_event_wake", true, Phase::kGraphicsPlugin, false,
     dev_overrides::Key::kWrmEventWake},
    {"d3d12_replay_batched_wake", true, Phase::kGraphicsPlugin, false,
     dev_overrides::Key::kReplayBatchedWake},
    {"d3d12_readback_gpu_timing", false, Phase::kGraphicsPlugin, true, std::nullopt},
}};

// Cvars this module must never write (net negative on PC, l63).
inline constexpr std::array<std::string_view, 2> kNeverWritten = {
    "memory_watch_disarm_bridge_safe_gaps",
    "shared_memory_upload_ahead_pages",
};

constexpr std::string_view ValueText(bool value) {
  return value ? "true" : "false";
}

enum class Action : uint8_t {
  kDisabledByOption,  // build without MCLA_M10_SPEEDUP_DEFAULTS
  kOffBySwitch,       // --mcla_m10_speedup_defaults=false
  kMeasurementBuild,  // diagnostic switch in an MCLA_DEV_OVERRIDES build
  kDevOverride,       // the owner override file wrote this cvar
  kUnavailable,       // the cvar is not registered (e.g. an older SDK)
  kUserSet,           // the command line or config changed the cvar
  kAlready,           // the cvar already holds the value (SDK default equal)
  kApply,             // write the value
};
inline constexpr size_t kActionCount = 8;

struct Inputs {
  bool option_on = false;          // MCLA_M10_SPEEDUP_DEFAULTS
  bool switch_on = true;           // mcla_m10_speedup_defaults
  bool measurement_build = false;  // MCLA_DEV_OVERRIDES
  bool registered = false;         // rex::cvar::GetFlagInfo != nullptr
  bool non_default = false;        // rex::cvar::HasNonDefaultValue
  bool dev_override_applied = false;  // dev_overrides::AppliedValue has a value
  bool holds_value = false;        // the current value already equals the default's
};

constexpr Action Decide(const Default &entry, const Inputs &in) {
  if (!in.option_on) {
    return Action::kDisabledByOption;
  }
  if (!in.switch_on) {
    return Action::kOffBySwitch;
  }
  if (entry.diagnostic && in.measurement_build) {
    return Action::kMeasurementBuild;
  }
  // A file value equal to the SDK default is not visible as non-default; the
  // applied record is what keeps such an A/B arm.
  if (entry.override_key && in.dev_override_applied) {
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
