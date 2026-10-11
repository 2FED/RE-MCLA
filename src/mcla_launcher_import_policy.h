#pragma once

#include <nlohmann/json.hpp>

#include <array>
#include <cstdint>
#include <expected>
#include <initializer_list>
#include <optional>
#include <set>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace mcla::launcher {

inline constexpr size_t kMaximumLegacyManifestBytes = 1024 * 1024;
inline constexpr size_t kMaximumLegacyIdBytes = 256;

namespace import_detail {
using Json = nlohmann::json;
inline bool Space(char c) { return c == ' ' || c == '\t' || c == '\r' || c == '\n'; }
inline bool ExactKeys(const Json &object, std::initializer_list<std::string_view> required,
                      std::initializer_list<std::string_view> optional = {}) {
  if (!object.is_object()) return false;
  for (const auto name : required) if (!object.contains(std::string(name))) return false;
  for (auto it = object.begin(); it != object.end(); ++it) {
    bool known = false;
    for (const auto name : required) known |= it.key() == name;
    for (const auto name : optional) known |= it.key() == name;
    if (!known) return false;
  }
  return true;
}
inline bool Text(const Json &object, std::string_view name, std::string_view value) {
  const auto it = object.find(std::string(name));
  return it != object.end() && it->is_string() && it->get_ref<const std::string &>() == value;
}
inline bool Unsigned(const Json &value, uint64_t expected) {
  return value.is_number_unsigned() && value.get<uint64_t>() == expected;
}
inline bool Boolean(const Json &value, bool expected) { return value.is_boolean() && value.get<bool>() == expected; }
inline bool Hex(std::string_view value, size_t length, bool lower = false) {
  if (value.size() != length) return false;
  for (const char c : value) {
    if (!((c >= '0' && c <= '9') || (lower ? c >= 'a' && c <= 'f' : c >= 'A' && c <= 'F'))) return false;
  }
  return true;
}
inline bool Hash(const Json &value) { return value.is_string() && Hex(value.get_ref<const std::string &>(), 64); }
inline bool Version(const Json &value) {
  if (!value.is_string()) return false;
  const auto &text = value.get_ref<const std::string &>();
  if (text.empty() || text.size() > 23) return false;
  unsigned groups = 0, digits = 0, component = 0;
  for (const auto c : text) {
    if (c == '.') {
      if (!digits || ++groups > 3) return false;
      digits = 0; component = 0;
    } else {
      if (c < '0' || c > '9' || ++digits > 5) return false;
      component = component * 10 + static_cast<unsigned>(c - '0');
      if (component > 65535) return false;
    }
  }
  return groups == 3 && digits != 0;
}
inline bool UtcTimestamp(const Json &value) {
  if (!value.is_string()) return false;
  const auto &text = value.get_ref<const std::string &>();
  if (text.size() < 20 || text.size() > 28 || text.back() != 'Z' ||
      text[4] != '-' || text[7] != '-' || text[10] != 'T' || text[13] != ':' || text[16] != ':') return false;
  for (size_t i = 0; i < 19; ++i) {
    if (i == 4 || i == 7 || i == 10 || i == 13 || i == 16) continue;
    if (text[i] < '0' || text[i] > '9') return false;
  }
  if (text.size() != 20) {
    if (text[19] != '.' || text.size() < 22) return false;
    for (size_t i = 20; i + 1 < text.size(); ++i) if (text[i] < '0' || text[i] > '9') return false;
  }
  const auto number = [&](size_t offset, size_t size) {
    unsigned result = 0;
    for (size_t i = offset; i < offset + size; ++i) result = result * 10 + static_cast<unsigned>(text[i] - '0');
    return result;
  };
  const unsigned year = number(0, 4), month = number(5, 2), day = number(8, 2);
  if (!year || !month || month > 12 || !day || number(11, 2) > 23 || number(14, 2) > 59 || number(17, 2) > 59) return false;
  constexpr std::array<unsigned, 12> days{31,28,31,30,31,30,31,31,30,31,30,31};
  const bool leap = year % 4 == 0 && (year % 100 != 0 || year % 400 == 0);
  return day <= days[month - 1] + static_cast<unsigned>(month == 2 && leap);
}
inline bool SourceSnapshot(const Json &value) {
  if (!value.is_string()) return false;
  const auto &path = value.get_ref<const std::string &>();
  if (path.size() > 256) return false;
  constexpr std::string_view m7 = "private/save-archive/M7-009/";
  if (path.starts_with(m7)) return Hex(std::string_view(path).substr(m7.size()), 64);
  constexpr std::string_view m6 = "private/save-archive/M6-014/";
  if (!path.starts_with(m6) || path.size() == m6.size()) return false;
  size_t begin = 0;
  for (const char c : path) {
    if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
          c == '/' || c == '-' || c == '_' || c == '.')) return false;
  }
  for (;;) {
    const auto slash = path.find('/', begin);
    const auto component = std::string_view(path).substr(begin, slash == path.npos ? slash : slash - begin);
    if (component.empty() || component == "." || component == "..") return false;
    if (slash == path.npos) return true;
    begin = slash + 1;
  }
}
} // namespace import_detail

// Metadata recognition ONLY, not a verified bundle, trusted manifest, current
// save validation or import authorization. The caller must pin/read the source,
// establish quiescence, validate its complete live profile inventory and archive
// before copying. selected_save hashes describe the historical seed, NOT the
// current live save: never compare them to live bytes to reject later progress.
inline std::expected<std::string, std::string> ValidateLegacyMetadata(
    std::string_view id_bytes, std::string_view manifest_bytes) {
  using namespace import_detail;
  if (id_bytes.empty() || id_bytes.size() > kMaximumLegacyIdBytes) return std::unexpected("legacy bundle ID byte bound violated");
  while (!id_bytes.empty() && Space(id_bytes.front())) id_bytes.remove_prefix(1);
  while (!id_bytes.empty() && Space(id_bytes.back())) id_bytes.remove_suffix(1);
  if (id_bytes.empty() || id_bytes.size() > 128) return std::unexpected("legacy bundle ID length invalid");
  for (const auto c : id_bytes) {
    if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '-' || c == '_' || c == '.'))
      return std::unexpected("legacy bundle ID must be a safe ASCII identifier");
  }
  if (manifest_bytes.empty() || manifest_bytes.size() > kMaximumLegacyManifestBytes || manifest_bytes.find('\0') != manifest_bytes.npos)
    return std::unexpected("legacy manifest byte bound violated");
  try {
    size_t events = 0;
    std::vector<std::optional<std::set<std::string>>> containers;
    const auto callback = [&](int depth, Json::parse_event_t event, Json &parsed) {
      if (depth > 16 || ++events > 8192) throw std::runtime_error("legacy JSON nesting/event bound exceeded");
      if (event == Json::parse_event_t::object_start || event == Json::parse_event_t::array_start) {
        if (containers.size() >= 16) throw std::runtime_error("legacy JSON nesting bound exceeded");
        containers.push_back(event == Json::parse_event_t::object_start ? std::optional<std::set<std::string>>(std::in_place) : std::nullopt);
      } else if (event == Json::parse_event_t::object_end || event == Json::parse_event_t::array_end) {
        if (containers.empty()) throw std::runtime_error("legacy JSON container state invalid");
        containers.pop_back();
      } else if (event == Json::parse_event_t::key) {
        if (containers.empty() || !containers.back() || !parsed.is_string()) throw std::runtime_error("legacy JSON key state invalid");
        const auto &key = parsed.get_ref<const std::string &>();
        if (key.size() > 128 || !containers.back()->insert(key).second) throw std::runtime_error("legacy JSON duplicate/oversized key rejected");
      }
      return true;
    };
    const auto manifest = Json::parse(manifest_bytes.begin(), manifest_bytes.end(), callback, true, false);
    if (!ExactKeys(manifest, {"schema","task","bundle_id","project_version","sdk_version","sdk_commit","created_utc",
        "platform","architecture","launcher","cpp_runtime","window_mode","immutable_file_count","game","selected_save",
        "writable_roots","retention","private_content"}, {"image_capture"}))
      return std::unexpected("legacy manifest fields/topology unsupported");
    if (!Text(manifest, "schema", "mcla-portable-campaign-bundle-v2") || !Text(manifest, "task", "M7-016") ||
        !Text(manifest, "bundle_id", id_bytes) || !Text(manifest, "platform", "windows-proton") ||
        !Text(manifest, "architecture", "x86_64") || !Text(manifest, "launcher", "Launch-MCLA.exe") ||
        !Version(manifest.at("project_version")) || !Version(manifest.at("sdk_version")) ||
        !manifest.at("sdk_commit").is_string() || !Hex(manifest.at("sdk_commit").get_ref<const std::string &>(), 40, true) ||
        !UtcTimestamp(manifest.at("created_utc"))) return std::unexpected("legacy manifest identity unsupported or mismatched");
    const auto prefix = "mcla-" + manifest.at("project_version").get<std::string>() + "-win64-";
    if (!id_bytes.starts_with(prefix)) return std::unexpected("legacy ID/project version mismatch");
    const auto suffix = id_bytes.substr(prefix.size());
    if (suffix.size() != 50 || suffix[16] != '-' || suffix[33] != '-' ||
        !Hex(suffix.substr(0, 16), 16) || !Hex(suffix.substr(17, 16), 16) || !Hex(suffix.substr(34), 16))
      return std::unexpected("legacy bundle ID fingerprint structure invalid");
    const auto &runtime = manifest.at("cpp_runtime");
    if (!ExactKeys(runtime, {"policy","version"}) || !Text(runtime, "policy", "msvc-14.44-app-local-v1") ||
        !Text(runtime, "version", "14.44.35211.0")) return std::unexpected("legacy C++ runtime metadata unsupported");
    const auto &window = manifest.at("window_mode");
    if (!ExactKeys(window, {"startup_fullscreen","alt_enter_toggle","left_double_click_toggle"}) ||
        !Boolean(window.at("startup_fullscreen"), true) || !Boolean(window.at("alt_enter_toggle"), true) ||
        !Boolean(window.at("left_double_click_toggle"), true)) return std::unexpected("legacy window metadata unsupported");
    const auto &game = manifest.at("game");
    if (!ExactKeys(game, {"manifest_sha256","file_count","payload_bytes"}) || !Hash(game.at("manifest_sha256")) ||
        !Unsigned(game.at("file_count"), 15) || !Unsigned(game.at("payload_bytes"), 6569586392))
      return std::unexpected("legacy prepared-game metadata unsupported");
    const auto &save = manifest.at("selected_save");
    if (!ExactKeys(save, {"source_snapshot","save_relative","header_relative","save_sha256","header_sha256"},
                    {"profile_sha256","ancestor_profile_sha256"}) ||
        !Text(save, "save_relative", "B13EBABEBABEBABE/545407F8/00000001/mc4.sav/mc4.sav") ||
        !Text(save, "header_relative", "B13EBABEBABEBABE/545407F8/Headers/00000001/mc4.sav.header") ||
        !SourceSnapshot(save.at("source_snapshot")) || !Hash(save.at("save_sha256")) || !Hash(save.at("header_sha256")))
      return std::unexpected("legacy selected-save metadata unsupported");
    if (save.contains("profile_sha256") && !Hash(save.at("profile_sha256"))) return std::unexpected("legacy seed profile hash invalid");
    if (save.at("source_snapshot").get_ref<const std::string &>().starts_with("private/save-archive/M7-009/") && !save.contains("profile_sha256"))
      return std::unexpected("M7 seed provenance requires a profile hash");
    if (save.contains("ancestor_profile_sha256")) {
      const auto &ancestors = save.at("ancestor_profile_sha256");
      if (!save.contains("profile_sha256") || !ancestors.is_array() || ancestors.size() > 256)
        return std::unexpected("legacy seed ancestry bounds invalid");
      std::set<std::string> seen;
      for (const auto &ancestor : ancestors) {
        if (!Hash(ancestor) || ancestor == save.at("profile_sha256") || !seen.insert(ancestor.get<std::string>()).second)
          return std::unexpected("legacy seed ancestry duplicate/self/invalid hash");
      }
    }
    const auto &roots = manifest.at("writable_roots");
    const Json expected_roots = {"user","cache","logs","diagnostics","results","update","metadata"};
    if (roots != expected_roots) return std::unexpected("legacy writable-root layout unsupported");
    const auto &retention = manifest.at("retention");
    if (!ExactKeys(retention, {"completed_sessions","save_snapshots_per_session"}) ||
        !Unsigned(retention.at("completed_sessions"), 32) || !Unsigned(retention.at("save_snapshots_per_session"), 32))
      return std::unexpected("legacy retention metadata unsupported");
    const auto &privacy = manifest.at("private_content");
    if (!ExactKeys(privacy, {"game_and_save_private","source_iso_included"}) || !Boolean(privacy.at("game_and_save_private"), true) ||
        !Boolean(privacy.at("source_iso_included"), false)) return std::unexpected("legacy private-content metadata unsupported");
    const bool image = manifest.contains("image_capture");
    if (!Unsigned(manifest.at("immutable_file_count"), image ? 33 : 32)) return std::unexpected("legacy immutable inventory cardinality invalid");
    if (image) {
      const auto &capture = manifest.at("image_capture");
      if (!ExactKeys(capture, {"mode","helper_path","helper_sha256","helper_bytes","host_sha256"}) ||
          !Text(capture, "mode", "window-wgc-v1") || !Text(capture, "helper_path", "bin/mcla_diagnostic_image_helper.exe") ||
          !Hash(capture.at("helper_sha256")) || !Hash(capture.at("host_sha256")) || !capture.at("helper_bytes").is_number_unsigned() ||
          capture.at("helper_bytes").get<uint64_t>() == 0 || capture.at("helper_bytes").get<uint64_t>() > 64 * 1024 * 1024)
        return std::unexpected("legacy image-capture metadata unsupported");
    }
    return std::string(id_bytes);
  } catch (const std::exception &) {
    // Do not echo untrusted JSON or large parser diagnostics into UI/logs.
    return std::unexpected("legacy JSON is malformed, duplicate-keyed, too deeply nested or structurally invalid");
  }
}

} // namespace mcla::launcher
