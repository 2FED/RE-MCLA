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
// - mcla_warp_finish_fix_off=true writes false to the host bool cvar
//   mcla_warp_finish_fix (KI-035 finish-race fix, default on) for the A/B
//   control arm; =false changes nothing.
// - mcla_texture_budget_soft_cap_off=true writes 0 (off) to the host integer
//   cvar mcla_texture_budget_soft_cap_mb (M10-007 / KI-048 A/B); =false changes
//   nothing.
// - mcla_album_probe (KI-056 MCLA_ALBUM lines) and mcla_pursuit_probe (KI-066
//   MCLA_PURSUIT lines) are plain host init-only bools (M11-002,
//   docs/evidence/M11-002-diagnostics-triad-20261106.md).
// - mcla_album_preview_restore (KI-056 fix A, default off) is a plain host
//   init-only bool (mcla_album_probe.cpp,
//   docs/evidence/M11-ki056-fix-a-20261106.md).
// - xam_content_stable_creation_time_off=true writes false to the SDK bool
//   cvar xam_content_stable_creation_time (M11-001 / KI-068 stable save
//   creation time, SDK default true; the Deck A/B control arm), and
//   xam_content_trace_off=true writes false to the SDK bool cvar
//   xam_content_trace (KI-068 content trace lines, SDK default true); =false
//   changes nothing. Both are rexruntime cvars, registered before startup.
// - KI-071 cut-scene audio A/B (docs/evidence/M11-ki071-cutscene-audio-analysis-20261106.md):
//   audio_speaker_config_stereo=true and audio_speaker_config_surround=true
//   write stereo or surround to the SDK string cvar audio_speaker_config
//   (default auto: follow the host playback device). Both true in one file is a
//   conflict (neither applied), as for the integer pairs. =false changes
//   nothing. audio_downmix_itu_off=true writes false to the SDK bool cvar
//   audio_downmix_itu (ITU Lo/Ro stereo downmix, default true; the A/B control
//   arm restores the legacy weights). audio_channel_rms_probe is the plain SDK
//   bool (1 Hz per-channel RMS log line). All are rexruntime cvars registered
//   before startup; an SDK without them logs the keys as unavailable.
// - M11-015 read-back sync kick-interval A/B
//   (docs/evidence/M11-cp-cost-plan-20261106.md item 1): readback_kick_256,
//   readback_kick_128 and readback_kick_1024 are bool convenience keys for the
//   integer SDK cvar d3d12_readback_sync_kick_draws (graphics plugin,
//   hot-reload; SDK default 512): =true writes 256, 128 or 1024, =false
//   changes nothing (all three false is the 512 default arm). The three form a
//   conflict group of three pairs: two or more true in one file is
//   contradictory and none of the true ones is applied.
// - Conflict rule (all groups): every key of a true-true kConflictPairs pair
//   (evaluated on the file as read, so a group of three is resolved as a
//   whole) loses its value; one kConflict issue is logged for each of those
//   lines except the earliest of its group.
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
  kTextureSoftCapOff,     // M10-007: mcla_texture_budget_soft_cap_mb=0
  kWarpFinishFixOff,      // KI-035 fix A/B: mcla_warp_finish_fix=false
  kAlbumProbe,            // M11-002 / KI-056: album preview diagnostic
  kPursuitProbe,          // M11-002 / KI-066: police pursuit transition log
  kStableCreationTimeOff, // KI-068 A/B: xam_content_stable_creation_time=false
  kContentTraceOff,       // KI-068: xam_content_trace=false
  kAudioSpeakerStereo,    // KI-071 A/B: audio_speaker_config=stereo
  kAudioSpeakerSurround,  // KI-071 A/B: audio_speaker_config=surround
  kAudioDownmixItuOff,    // KI-071 A/B: audio_downmix_itu=false (legacy weights)
  kAudioChannelRmsProbe,  // KI-071: audio_channel_rms_probe (1 Hz RMS line)
  kAlbumPreviewRestore,   // KI-056 fix A: mcla_album_preview_restore (default off)
  kReadbackKick256,       // M11-015 A/B: d3d12_readback_sync_kick_draws=256
  kReadbackKick128,       // M11-015 A/B: d3d12_readback_sync_kick_draws=128
  kReadbackKick1024,      // M11-015 A/B control: d3d12_readback_sync_kick_draws=1024
};
inline constexpr size_t kKeyCount = 32;

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
    // M10-007 host init-only cvar (mcla_texture_budget_win.cpp), read at
    // graphics-plugin load, so the startup phase is early enough.
    {"mcla_texture_budget_soft_cap_off", Phase::kStartup, "mcla_texture_budget_soft_cap_mb", "0",
     ""},
    // KI-035 finish-race fix (host init-only bool, default true), before the
    // guest.
    {"mcla_warp_finish_fix_off", Phase::kStartup, "mcla_warp_finish_fix", "false", ""},
    // M11-002 host init-only bools (mcla_album_probe.cpp,
    // mcla_pursuit_probe.cpp), before the guest.
    {"mcla_album_probe", Phase::kStartup},
    {"mcla_pursuit_probe", Phase::kStartup},
    // M11-001 / KI-068 SDK rexruntime bools (default true), before the guest.
    {"xam_content_stable_creation_time_off", Phase::kStartup, "xam_content_stable_creation_time",
     "false", ""},
    {"xam_content_trace_off", Phase::kStartup, "xam_content_trace", "false", ""},
    // KI-071 SDK rexruntime audio cvars, before the guest (the speaker config
    // is read at the title's first XAudioGetSpeakerConfig call).
    {"audio_speaker_config_stereo", Phase::kStartup, "audio_speaker_config", "stereo", ""},
    {"audio_speaker_config_surround", Phase::kStartup, "audio_speaker_config", "surround", ""},
    {"audio_downmix_itu_off", Phase::kStartup, "audio_downmix_itu", "false", ""},
    {"audio_channel_rms_probe", Phase::kStartup},
    // KI-056 fix A host init-only bool (mcla_album_probe.cpp), before the
    // guest.
    {"mcla_album_preview_restore", Phase::kStartup},
    // M11-015 kick-interval A/B: the integer SDK cvar is registered by the
    // graphics plugin (command_processor.cpp, hot-reload), so after the plugin
    // has loaded.
    {"readback_kick_256", Phase::kGraphicsPlugin, "d3d12_readback_sync_kick_draws", "256", ""},
    {"readback_kick_128", Phase::kGraphicsPlugin, "d3d12_readback_sync_kick_draws", "128", ""},
    {"readback_kick_1024", Phase::kGraphicsPlugin, "d3d12_readback_sync_kick_draws", "1024", ""},
}};

// Key pairs that write different values to one cvar (integer, or the
// audio_speaker_config string): both true in one file is contradictory and
// neither is applied. A group of three keys for one cvar is listed as its
// three pairs (first < second in Key order); see ResolveConflicts.
struct ConflictPair {
  Key first;
  Key second;
};
inline constexpr std::array<ConflictPair, 6> kConflictPairs = {{
    {Key::kUploadAhead16, Key::kUploadAhead64},
    {Key::kHotBlocksShadow, Key::kHotBlocksOn},
    {Key::kAudioSpeakerStereo, Key::kAudioSpeakerSurround},
    {Key::kReadbackKick256, Key::kReadbackKick128},
    {Key::kReadbackKick256, Key::kReadbackKick1024},
    {Key::kReadbackKick128, Key::kReadbackKick1024},
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
  kConflict,     // contradicts an earlier line (two keys of one cvar true, kConflictPairs)
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

// Two keys of a conflict pair true: none of the keys in a true-true pair
// applies. Pairs are evaluated on the values as parsed (before any reset), so
// a group of three (listed as its three pairs) is dropped as a whole when two
// or more of its keys are true. One conflict is logged on each dropped line
// that has an earlier-line partner in a true-true pair (for a pair: the later
// line), in line order.
constexpr void ResolveConflicts(Parsed &parsed) {
  std::array<bool, kKeyCount> dropped{};
  std::array<bool, kKeyCount> logged{};
  for (const ConflictPair &pair : kConflictPairs) {
    const size_t a = static_cast<size_t>(pair.first);
    const size_t b = static_cast<size_t>(pair.second);
    if (parsed.values[a] == true && parsed.values[b] == true) {
      dropped[a] = true;
      dropped[b] = true;
      logged[parsed.lines[a] > parsed.lines[b] ? a : b] = true;
    }
  }
  uint16_t previous = 0;
  for (;;) {
    // The next logged line after `previous` (lines are unique per key).
    uint16_t next = 0;
    for (size_t i = 0; i < kKeyCount; ++i) {
      if (logged[i] && parsed.lines[i] > previous && (next == 0 || parsed.lines[i] < next)) {
        next = parsed.lines[i];
      }
    }
    if (next == 0) {
      break;
    }
    AddIssue(parsed, next, LineIssue::kConflict);
    previous = next;
  }
  for (size_t i = 0; i < kKeyCount; ++i) {
    if (dropped[i]) {
      parsed.values[i].reset();
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
// written. Used for keys whose value writes nothing (upload-ahead, hot blocks
// and read-back kick =false).
std::optional<bool> FileValue(Key key);

}  // namespace mcla::dev_overrides
