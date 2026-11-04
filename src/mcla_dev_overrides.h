#pragma once

// Owner-internal developer overrides (M10 l57 CPU A/B,
// docs/evidence/M10-l57-cpu-ab-build-20261101.md; M10-002 l58 read-back
// elision A/B, docs/evidence/M10-002-readback-elision-build-20261101.md;
// M10-004 l59 Vulkan Deck A/B,
// docs/evidence/M10-004-vulkan-deck-ab-build-20261101.md; M10-003 shader
// store seeding switch, docs/evidence/M10-prewarm-union-store-20261101.md;
// KI-047 targeted read-back wait A/B,
// docs/evidence/M10-ki047-targeted-wait-20261101.md; M10 watch census and
// steps 1-2 of the Deck watch/upload path,
// docs/evidence/M10-watch-steps0-2-20261101.md; M10 watch step 3 hot blocks,
// docs/evidence/M10-hot-blocks-sdk-20261103.md and
// docs/evidence/M10-hot-blocks-host-keys-20261103.md).
//
// Builds made with the CMake option MCLA_DEV_OVERRIDES (OFF by default; ON
// only for owner-internal measurement builds, never for volunteer builds) read
// the per-user file %LocalAppData%\MCLA-R-Launcher-v1\mcla-dev-overrides.txt
// once at game start, because the launcher cannot pass extra command-line
// flags. The file holds lines "name=true" or "name=false":
//
// - at most kMaxBytes bytes, ASCII only (no BOM); otherwise the whole file is
//   rejected and nothing is applied;
// - lines end in LF or CRLF; empty lines and lines starting with '#' are
//   skipped;
// - a line must be exactly name=true or name=false (no spaces); the name must
//   be one of kKeys. Malformed lines, unknown names and repeated names (the
//   first occurrence wins) are ignored and logged by line number.
// - shared_memory_upload_ahead_16 and shared_memory_upload_ahead_64 are bool
//   convenience keys for the integer SDK cvar shared_memory_upload_ahead_pages
//   (0 = off, 16 or 64): =true writes 16 or 64, =false changes nothing. Both
//   true in one file is contradictory: neither is applied (logged as a
//   conflict on the later line). Integer values are deliberately not accepted
//   in the file; any other value needs the command line.
// - shared_memory_hot_blocks_shadow and shared_memory_hot_blocks_on are the
//   same kind of pair for the integer SDK cvar shared_memory_hot_blocks
//   (0 = off, 1 = shadow, 2 = on): =true writes 1 or 2, =false changes
//   nothing, both true is a conflict (neither applied). The other hot-block
//   cvars (K, N, upload cap, probe frames) need the command line.
// - mcla_warp_probe (KI-035) is a plain host bool. mcla_warp_watchdog is a
//   bool convenience key for the integer host cvar mcla_warp_watchdog_seconds
//   (0 = off): =true writes 15, =false changes nothing. Other limits need the
//   command line.
//
// Each accepted value is applied to its cvar unless that cvar already holds a
// non-default value (set by the command line or the config file), and logged
// once as "MCLA dev override: name=value". Host init-only cvars are applied at
// startup (before the guest runs); SDK cvars of the graphics plugin are applied
// when the plugin has been loaded (they are registered by the plugin DLL).
// Without the option nothing is read or written, and the single-line l55 file
// read of mcla_readback_sync.cpp stays as before.
//
// Phase::kLauncher keys are not cvars: the host parses and ignores them; the
// launcher (MCLA_DEV_OVERRIDES launcher builds) reads the same file before a
// session and applies them itself (prewarm_seed_store=false: no shader store
// seeding, for cold baselines).
//
// The parser and the apply decision are pure and unit-tested
// (tests/mcla_dev_overrides_test.cpp); file and cvar calls are in
// mcla_dev_overrides.cpp.

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string_view>

namespace mcla::dev_overrides {

inline constexpr std::string_view kDirectory = "MCLA-R-Launcher-v1";
inline constexpr std::string_view kFile = "mcla-dev-overrides.txt";
inline constexpr size_t kMaxBytes = 1024;

enum class Key : uint8_t {
  kReadbackSyncComposite,
  kNativeD3dRefcount,
  kNativeXapiInterlocked,
  kWrmEventWake,
  kReplayBatchedWake,
  kReadbackElideUnread,
  kGpuBackendVulkan,  // M10-004 Deck A/B: native Vulkan for the session
  kPrewarmSeedStore,  // M10-003: launcher shader store seeding (default on)
  kReadbackSyncTargeted,  // KI-047 cost: targeted read-back wait + early submit
  kWatchCensus,           // M10 watch census counters (SDK default on)
  kWatchProbes,           // M10 watch F10 probes (SDK default off)
  kWatchDisarmBridge,     // M10 step 1: disarm safe-gap bridging
  kUploadAhead16,         // M10 step 2: shared_memory_upload_ahead_pages=16
  kUploadAhead64,         // M10 step 2: shared_memory_upload_ahead_pages=64
  kHotBlocksShadow,       // M10 step 3: shared_memory_hot_blocks=1 (shadow)
  kHotBlocksOn,           // M10 step 3: shared_memory_hot_blocks=2 (on)
  kWarpProbe,             // KI-035: warp state / camera logging
  kWarpWatchdog,          // KI-035: mcla_warp_watchdog_seconds=15
};
inline constexpr size_t kKeyCount = 18;

// When the cvar can be written: host init-only cvars before the guest starts,
// graphics-plugin cvars once the plugin has registered them. Launcher keys are
// read by the launcher only (no host cvar).
enum class Phase : uint8_t {
  kStartup,
  kGraphicsPlugin,
  kLauncher,
};

struct KeyInfo {
  std::string_view name;
  Phase phase;
  // The cvar written (empty: the key's own name) and the text written for
  // true and false (empty: nothing is written for that value).
  std::string_view cvar = {};
  std::string_view true_text = "true";
  std::string_view false_text = "false";
};

// The whitelist, indexed by Key.
inline constexpr std::array<KeyInfo, kKeyCount> kKeys = {{
    {"mcla_readback_sync_composite", Phase::kStartup},
    {"mcla_native_d3d_refcount", Phase::kStartup},
    {"mcla_native_xapi_interlocked", Phase::kStartup},
    {"d3d12_wrm_event_wake", Phase::kGraphicsPlugin},
    {"d3d12_replay_batched_wake", Phase::kGraphicsPlugin},
    {"d3d12_readback_elide_unread", Phase::kGraphicsPlugin},
    {"mcla_gpu_backend_vulkan", Phase::kStartup},
    {"prewarm_seed_store", Phase::kLauncher},
    {"d3d12_readback_sync_targeted", Phase::kGraphicsPlugin},
    // rexruntime cvars, registered before startup.
    {"memory_watch_census", Phase::kStartup},
    {"memory_watch_probes", Phase::kStartup},
    {"memory_watch_disarm_bridge_safe_gaps", Phase::kStartup},
    // Registered by the graphics plugin (shared memory).
    {"shared_memory_upload_ahead_16", Phase::kGraphicsPlugin, "shared_memory_upload_ahead_pages",
     "16", ""},
    {"shared_memory_upload_ahead_64", Phase::kGraphicsPlugin, "shared_memory_upload_ahead_pages",
     "64", ""},
    {"shared_memory_hot_blocks_shadow", Phase::kGraphicsPlugin, "shared_memory_hot_blocks", "1",
     ""},
    {"shared_memory_hot_blocks_on", Phase::kGraphicsPlugin, "shared_memory_hot_blocks", "2", ""},
    // KI-035 host init-only cvars (mcla_ki026_probe.cpp), before the guest.
    {"mcla_warp_probe", Phase::kStartup},
    {"mcla_warp_watchdog", Phase::kStartup, "mcla_warp_watchdog_seconds", "15", ""},
}};

// Key pairs that write different values to one integer cvar: both true in one
// file is contradictory and neither is applied.
struct ConflictPair {
  Key first;
  Key second;
};
inline constexpr std::array<ConflictPair, 2> kConflictPairs = {{
    {Key::kUploadAhead16, Key::kUploadAhead64},
    {Key::kHotBlocksShadow, Key::kHotBlocksOn},
}};

constexpr const KeyInfo &Info(Key key) {
  return kKeys[static_cast<size_t>(key)];
}

// The cvar a key writes.
constexpr std::string_view CvarName(const KeyInfo &info) {
  return info.cvar.empty() ? info.name : info.cvar;
}

// The text written to the cvar for a value; empty: nothing is written.
constexpr std::string_view CvarText(const KeyInfo &info, bool value) {
  return value ? info.true_text : info.false_text;
}

constexpr std::optional<Key> FindKey(std::string_view name) {
  for (size_t i = 0; i < kKeyCount; ++i) {
    if (kKeys[i].name == name) {
      return static_cast<Key>(i);
    }
  }
  return std::nullopt;
}

enum class FileStatus : uint8_t {
  kAbsent,    // no file
  kRejected,  // too large, not ASCII or unreadable: nothing applied
  kParsed,    // read line by line (individual lines may be ignored)
};

enum class LineIssue : uint8_t {
  kMalformed,    // not exactly name=true or name=false
  kUnknownName,  // well formed, name not in kKeys
  kDuplicate,    // a name already given on an earlier line
  kConflict,     // contradicts an earlier line (both keys of a kConflictPairs pair true)
};

struct Issue {
  uint16_t line = 0;  // 1-based
  LineIssue issue = LineIssue::kMalformed;
};

inline constexpr size_t kMaxIssues = 16;

struct Parsed {
  FileStatus status = FileStatus::kAbsent;
  std::array<std::optional<bool>, kKeyCount> values{};
  std::array<uint16_t, kKeyCount> lines{};  // line of each value (1-based)
  std::array<Issue, kMaxIssues> issues{};
  size_t issue_count = 0;    // issues stored (at most kMaxIssues)
  size_t ignored_lines = 0;  // every ignored line, also beyond kMaxIssues

  constexpr std::optional<bool> Value(Key key) const {
    return values[static_cast<size_t>(key)];
  }
  constexpr size_t ValueCount() const {
    size_t count = 0;
    for (const auto &value : values) {
      count += value.has_value() ? 1 : 0;
    }
    return count;
  }
};

namespace detail {

constexpr void AddIssue(Parsed &parsed, uint16_t line, LineIssue issue) {
  if (parsed.issue_count < kMaxIssues) {
    parsed.issues[parsed.issue_count++] = Issue{line, issue};
  }
  ++parsed.ignored_lines;
}

constexpr void ParseLine(Parsed &parsed, uint16_t number, std::string_view line) {
  if (line.ends_with('\r')) {
    line.remove_suffix(1);
  }
  if (line.empty() || line.front() == '#') {
    return;
  }
  size_t equals = std::string_view::npos;
  for (size_t i = 0; i < line.size(); ++i) {
    const auto c = static_cast<unsigned char>(line[i]);
    if (c < 0x21 || c > 0x7E) {
      AddIssue(parsed, number, LineIssue::kMalformed);
      return;
    }
    if (c == '=') {
      if (equals != std::string_view::npos) {
        AddIssue(parsed, number, LineIssue::kMalformed);
        return;
      }
      equals = i;
    }
  }
  if (equals == std::string_view::npos || equals == 0) {
    AddIssue(parsed, number, LineIssue::kMalformed);
    return;
  }
  const std::string_view name = line.substr(0, equals);
  const std::string_view text = line.substr(equals + 1);
  bool value = false;
  if (text == "true") {
    value = true;
  } else if (text != "false") {
    AddIssue(parsed, number, LineIssue::kMalformed);
    return;
  }
  const std::optional<Key> key = FindKey(name);
  if (!key) {
    AddIssue(parsed, number, LineIssue::kUnknownName);
    return;
  }
  auto &slot = parsed.values[static_cast<size_t>(*key)];
  if (slot.has_value()) {
    AddIssue(parsed, number, LineIssue::kDuplicate);
    return;
  }
  slot = value;
  parsed.lines[static_cast<size_t>(*key)] = number;
}

// Both keys of a conflict pair true: neither applies.
constexpr void ResolveConflicts(Parsed &parsed) {
  for (const ConflictPair &pair : kConflictPairs) {
    const size_t a = static_cast<size_t>(pair.first);
    const size_t b = static_cast<size_t>(pair.second);
    if (parsed.values[a] == true && parsed.values[b] == true) {
      const uint16_t later = parsed.lines[a] > parsed.lines[b] ? parsed.lines[a] : parsed.lines[b];
      parsed.values[a].reset();
      parsed.values[b].reset();
      AddIssue(parsed, later, LineIssue::kConflict);
    }
  }
}

}  // namespace detail

// Strict parse of the whole file content (see the rules above).
constexpr Parsed Parse(std::string_view content) {
  Parsed parsed;
  if (content.size() > kMaxBytes) {
    parsed.status = FileStatus::kRejected;
    return parsed;
  }
  for (const char c : content) {
    if (static_cast<unsigned char>(c) > 0x7F) {
      parsed.status = FileStatus::kRejected;
      return parsed;
    }
  }
  parsed.status = FileStatus::kParsed;
  uint16_t number = 0;
  while (!content.empty()) {
    ++number;
    const size_t end = content.find('\n');
    if (end == std::string_view::npos) {
      detail::ParseLine(parsed, number, content);
      break;
    }
    detail::ParseLine(parsed, number, content.substr(0, end));
    content.remove_prefix(end + 1);
  }
  detail::ResolveConflicts(parsed);
  return parsed;
}

constexpr const char *LineIssueName(LineIssue issue) {
  switch (issue) {
    case LineIssue::kMalformed:
      return "malformed (expected name=true or name=false)";
    case LineIssue::kUnknownName:
      return "unknown name";
    case LineIssue::kDuplicate:
      return "repeated name (the first line wins)";
    case LineIssue::kConflict:
      return "conflicting keys for one cvar (neither is applied)";
  }
  return "unknown";
}

enum class ApplyAction : uint8_t {
  kApply,        // write the cvar
  kUnavailable,  // the cvar is not registered (e.g. an SDK without it)
  kUserSet,      // the command line or config set the cvar: left alone
};

constexpr ApplyAction DecideApply(bool cvar_registered, bool cvar_non_default) {
  if (!cvar_registered) {
    return ApplyAction::kUnavailable;
  }
  if (cvar_non_default) {
    return ApplyAction::kUserSet;
  }
  return ApplyAction::kApply;
}

// Runtime (mcla_dev_overrides.cpp). Both calls do nothing in builds without
// MCLA_DEV_OVERRIDES.

// True in builds made with MCLA_DEV_OVERRIDES.
bool Enabled();

// Reads and parses the file once and applies the Phase::kStartup keys. Call
// after logging is set up and before the guest starts.
void OnStartup();

// Applies the Phase::kGraphicsPlugin keys. Call once after the graphics plugin
// has been loaded.
void OnGraphicsPluginLoaded();

// The value written to the key's cvar by this module (nullopt when the file
// has no valid line for it, the cvar was left alone, or the write failed).
std::optional<bool> AppliedValue(Key key);

// The value of the key's valid file line (nullopt without the option, without
// a parsed file or without a valid line), whether or not anything was
// written. Used for keys whose value writes nothing (upload-ahead and hot
// blocks =false).
std::optional<bool> FileValue(Key key);

}  // namespace mcla::dev_overrides
