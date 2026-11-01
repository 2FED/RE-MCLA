#pragma once

// KI-047 fix candidate host side (owner-internal l55;
// docs/evidence/M10-001-ki047-sync-fix-build-20261101.md):
//
// - MCLA_READBACK_SYNC_COMPOSITE_DEFAULT builds set the SDK cvar
//   d3d12_readback_sync_composite=true at graphics-plugin load (synchronous,
//   fenced read-back of livery-composite resolves under readback_resolve
//   fast/some), unless the user set that SDK cvar, the SDK has no such cvar
//   (the pinned volunteer SDK), or the host switch mcla_readback_sync_composite
//   is false.
// - The host switch defaults to true in such builds. For the owner A/B it can
//   be turned off without command-line access: a per-user override file
//   (%LocalAppData%\MCLA-R-Launcher-v1\mcla-dev-overrides.txt) holding exactly
//   one line "mcla_readback_sync_composite=false" (or "=true"). The file is
//   read only in builds with the option ON, must be ASCII and at most 256
//   bytes, and is parsed strictly; anything else is ignored and logged. An
//   explicit --mcla_readback_sync_composite on the command line wins over the
//   file. Without the option nothing is read or written.
// - Builds that also have MCLA_DEV_OVERRIDES (l57 and later owner-internal
//   builds) read the same file through mcla_dev_overrides.h instead, which
//   accepts several name=value lines (this single line among them) and writes
//   the host switch at startup; this module then only reports that value.
//
// The decisions are pure and unit-tested (tests/mcla_readback_sync_test.cpp);
// the cvar and file calls are in mcla_readback_sync.cpp.

#include <cstddef>
#include <string_view>

namespace mcla::readback_sync {

inline constexpr std::string_view kSdkCvar = "d3d12_readback_sync_composite";
inline constexpr std::string_view kHostCvar = "mcla_readback_sync_composite";
inline constexpr std::string_view kOverrideDirectory = "MCLA-R-Launcher-v1";
inline constexpr std::string_view kOverrideFile = "mcla-dev-overrides.txt";
inline constexpr size_t kOverrideMaxBytes = 256;

enum class OverrideStatus {
  kAbsent,   // no file
  kInvalid,  // present but not exactly one valid line: ignored
  kTrue,     // mcla_readback_sync_composite=true
  kFalse,    // mcla_readback_sync_composite=false
};

// Strict parse of the override file content: exactly
// "mcla_readback_sync_composite=true" or "=false", optionally followed by one
// line ending (LF or CRLF). ASCII only; no spaces, comments or other keys.
constexpr OverrideStatus ParseOverride(std::string_view content) {
  if (content.size() > kOverrideMaxBytes) {
    return OverrideStatus::kInvalid;
  }
  for (const char c : content) {
    if (static_cast<unsigned char>(c) > 0x7F) {
      return OverrideStatus::kInvalid;
    }
  }
  if (content.ends_with("\r\n")) {
    content.remove_suffix(2);
  } else if (content.ends_with("\n")) {
    content.remove_suffix(1);
  }
  if (content.size() <= kHostCvar.size() + 1 || !content.starts_with(kHostCvar) ||
      content[kHostCvar.size()] != '=') {
    return OverrideStatus::kInvalid;
  }
  const std::string_view value = content.substr(kHostCvar.size() + 1);
  if (value == "true") {
    return OverrideStatus::kTrue;
  }
  if (value == "false") {
    return OverrideStatus::kFalse;
  }
  return OverrideStatus::kInvalid;
}

constexpr const char *OverrideStatusName(OverrideStatus status) {
  switch (status) {
    case OverrideStatus::kAbsent:
      return "absent";
    case OverrideStatus::kInvalid:
      return "invalid";
    case OverrideStatus::kTrue:
      return "true";
    case OverrideStatus::kFalse:
      return "false";
  }
  return "unknown";
}

// The host switch after the override: an explicit command-line value wins,
// then a valid file, then the build default (host_value).
constexpr bool EffectiveHostValue(bool host_value, bool host_user_set, OverrideStatus file) {
  if (host_user_set) {
    return host_value;
  }
  if (file == OverrideStatus::kTrue) {
    return true;
  }
  if (file == OverrideStatus::kFalse) {
    return false;
  }
  return host_value;
}

enum class StartAction {
  kDisabledByOption,  // build without MCLA_READBACK_SYNC_COMPOSITE_DEFAULT
  kUnavailable,       // SDK without d3d12_readback_sync_composite
  kSdkUserSet,        // the user set the SDK cvar; left alone
  kOffByHost,         // mcla_readback_sync_composite=false (command line or file)
  kEnable,            // write d3d12_readback_sync_composite=true
};

constexpr StartAction DecideStart(bool option_on, bool sdk_has_cvar, bool sdk_user_set,
                                  bool effective_host_value) {
  if (!option_on) {
    return StartAction::kDisabledByOption;
  }
  if (!sdk_has_cvar) {
    return StartAction::kUnavailable;
  }
  if (sdk_user_set) {
    return StartAction::kSdkUserSet;
  }
  return effective_host_value ? StartAction::kEnable : StartAction::kOffByHost;
}

// Applies DecideStart at graphics-plugin load. Safe to call once.
void OnGraphicsPluginLoaded();

}  // namespace mcla::readback_sync
