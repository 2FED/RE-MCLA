#pragma once

#include "mcla_launcher_capabilities.h"
#include <nlohmann/json.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <limits>
#include <map>
#include <optional>
#include <set>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace mcla::launcher::diagnostic_export {

struct ExportMetadataInput {
  std::string_view profile_id, session_receipt, session_state, manifest, diagnostic_state, capabilities;
};
struct MetadataProjection {
  std::string report_json;
  std::string state_sha256;
  uint64_t state_bytes = 0;
};

inline constexpr size_t kMaximumExportJsonBytes = 1024 * 1024;
inline constexpr size_t kMaximumExportReceiptBytes = 16384;

namespace detail {
using Json = nlohmann::json;
inline void Require(bool ok) { if (!ok) throw std::runtime_error("unsupported export metadata"); }

inline std::optional<uint32_t> Decimal(std::string_view text) {
  if (text.empty() || text.size() > 10 || (text.size() > 1 && text.front() == '0')) return {};
  uint64_t value = 0;
  for (const char c : text) {
    if (c < '0' || c > '9') return {};
    value = value * 10 + static_cast<unsigned>(c - '0');
    if (value > std::numeric_limits<uint32_t>::max()) return {};
  }
  return static_cast<uint32_t>(value);
}

// Validate UTF-8 without replacement characters or permissive decoder fallback.
// Text is consumed only for validation/vendor classification, never copied out.
inline bool SafeUtf8(std::string_view text, size_t maximum) {
  if (text.empty() || text.size() > maximum) return false;
  for (size_t i = 0; i < text.size();) {
    const auto first = static_cast<unsigned char>(text[i++]);
    if (first < 0x80) { if (first < 0x20 || first == 0x7f) return false; continue; }
    unsigned continuation = 0; uint32_t value = 0, minimum = 0;
    if (first >= 0xc2 && first <= 0xdf) { continuation = 1; value = first & 0x1f; minimum = 0x80; }
    else if (first >= 0xe0 && first <= 0xef) { continuation = 2; value = first & 0x0f; minimum = 0x800; }
    else if (first >= 0xf0 && first <= 0xf4) { continuation = 3; value = first & 0x07; minimum = 0x10000; }
    else return false;
    if (text.size() - i < continuation) return false;
    for (unsigned j = 0; j < continuation; ++j) {
      const auto next = static_cast<unsigned char>(text[i++]);
      if ((next & 0xc0) != 0x80) return false;
      value = (value << 6) | (next & 0x3f);
    }
    if (value < minimum || value > 0x10ffff || (value >= 0xd800 && value <= 0xdfff) ||
        (value >= 0x80 && value <= 0x9f)) return false;
  }
  return true;
}

inline bool Version(std::string_view text) {
  if (text.empty() || text.size() > 23) return false;
  unsigned count = 0;
  while (true) {
    const auto dot = text.find('.');
    const auto component = text.substr(0, dot);
    const auto value = Decimal(component);
    if (!value || *value > 65535 || ++count > 4) return false;
    if (dot == text.npos) return count == 4;
    text.remove_prefix(dot + 1);
  }
}

inline Json ParseJson(std::string_view bytes) {
  Require(!bytes.empty() && bytes.size() <= kMaximumExportJsonBytes && bytes.find('\0') == bytes.npos);
  size_t events = 0;
  std::vector<std::optional<std::set<std::string>>> containers;
  const auto callback = [&](int depth, Json::parse_event_t event, Json &parsed) {
    Require(depth <= 16 && ++events <= 8192);
    if (event == Json::parse_event_t::object_start || event == Json::parse_event_t::array_start) {
      Require(containers.size() < 16);
      containers.push_back(event == Json::parse_event_t::object_start ?
          std::optional<std::set<std::string>>(std::in_place) : std::nullopt);
    } else if (event == Json::parse_event_t::object_end || event == Json::parse_event_t::array_end) {
      Require(!containers.empty()); containers.pop_back();
    } else if (event == Json::parse_event_t::key) {
      Require(!containers.empty() && containers.back().has_value() && parsed.is_string());
      const auto &key = parsed.get_ref<const std::string &>();
      Require(key.size() <= 128 && key.find('\0') == key.npos && containers.back()->insert(key).second);
    } else if (event == Json::parse_event_t::value && parsed.is_string()) {
      // Even ignored private strings may not conceal embedded NULs.
      Require(parsed.get_ref<const std::string &>().find('\0') == std::string::npos);
    }
    return true;
  };
  auto result = Json::parse(bytes.begin(), bytes.end(), callback, true, false);
  Require(result.is_object());
  return result;
}

inline const Json &Object(const Json &parent, const char *name) {
  const auto &value = parent.at(name); Require(value.is_object()); return value;
}
inline std::string_view Text(const Json &parent, const char *name, size_t maximum) {
  const auto &value = parent.at(name); Require(value.is_string());
  const auto &text = value.get_ref<const std::string &>();
  Require(SafeUtf8(text, maximum)); return text;
}
inline bool Boolean(const Json &parent, const char *name) {
  const auto &value = parent.at(name); Require(value.is_boolean()); return value.get<bool>();
}
inline uint64_t Unsigned(const Json &parent, const char *name, uint64_t maximum) {
  const auto &value = parent.at(name); Require(value.is_number_unsigned());
  const auto number = value.get<uint64_t>(); Require(number <= maximum); return number;
}
inline std::map<std::string, std::string> Receipt(std::string_view bytes) {
  constexpr std::string_view header = "MCLA-MANAGED-SESSION-V1\n";
  Require(bytes.size() <= kMaximumExportReceiptBytes && bytes.starts_with(header) &&
          bytes.back() == '\n' && bytes.find_first_of(std::string_view("\r\0", 2)) == bytes.npos);
  bytes.remove_prefix(header.size());
  std::map<std::string, std::string> fields;
  while (!bytes.empty()) {
    const auto end = bytes.find('\n'); Require(end != bytes.npos);
    const auto line = bytes.substr(0, end); const auto space = line.find(' ');
    Require(space != line.npos && space > 0 && space + 1 < line.size());
    const auto key = line.substr(0, space), value = line.substr(space + 1);
    Require(key.size() <= 32 && value.size() <= 1024 && fields.emplace(key, value).second);
    bytes.remove_prefix(end + 1);
  }
  constexpr std::array<std::string_view, 9> required{
      "profile","build","game","timing","gpu","adapter","scale","vsync","fullscreen"};
  constexpr std::array<std::string_view, 4> optional{
      "render_target","guest_mode","capability_policy","capability_record"};
  for (const auto key : required) Require(fields.contains(std::string(key)));
  for (const auto &[key,value] : fields) {
    (void)value; bool allowed = false;
    for (const auto name : required) allowed |= key == name;
    for (const auto name : optional) allowed |= key == name;
    Require(allowed);
  }
  Require(fields.contains("render_target") == fields.contains("guest_mode"));
  Require(fields.contains("capability_policy") == fields.contains("capability_record"));
  return fields;
}
inline std::string_view Vendor(std::string_view gpu) {
  if (gpu == "NVIDIA" || gpu.starts_with("NVIDIA ")) return "NVIDIA";
  if (gpu == "AMD" || gpu.starts_with("AMD ")) return "AMD";
  if (gpu == "Intel" || gpu.starts_with("Intel ") || gpu.starts_with("Intel(R) ")) return "Intel";
  return "other";
}
inline std::string_view Reason(std::string_view reason) {
  if (reason == "f10") return "f10";
  if (reason == "unexpected-guest-exit") return "unexpected-guest-exit";
  return "other";
}
} // namespace detail

// Pure metadata projection only. Caller binds source paths/managed marker and
// verifies returned state_sha256/state_bytes against the exact consumed state
// bytes before presenting or publishing. No source authenticity is established.
inline std::expected<MetadataProjection, std::string> ProjectLiveMetadata(const ExportMetadataInput &input) {
  using namespace detail;
  try {
    Require(ValidOpaqueId(input.profile_id));
    constexpr std::string_view complete = "complete\nexit ";
    Require(input.session_state.size() <= 64 && input.session_state.starts_with(complete) && input.session_state.back() == '\n');
    Require(Decimal(input.session_state.substr(complete.size(), input.session_state.size() - complete.size() - 1)).has_value());
    const auto receipt = Receipt(input.session_receipt);
    Require(receipt.at("profile") == input.profile_id && ValidCapabilityIdentity(receipt.at("build")) &&
            ValidCapabilityIdentity(receipt.at("game")) && receipt.at("timing") == "stock" && receipt.at("vsync") == "1" &&
            (receipt.at("fullscreen") == "0" || receipt.at("fullscreen") == "1") &&
            SafeUtf8(receipt.at("gpu"), 800) && Decimal(receipt.at("adapter")).has_value());
    const auto scale = Decimal(receipt.at("scale")); Require(scale && *scale >= 1 && *scale <= 3);
    const RenderTargetPlan *target = nullptr;
    if (receipt.contains("render_target")) {
      for (const auto &candidate : kRenderTargets)
        if (receipt.at("render_target") == std::to_string(candidate.width) + "x" + std::to_string(candidate.height) &&
            receipt.at("guest_mode") == std::to_string(candidate.guest_width) + "x" + std::to_string(candidate.guest_height) &&
            *scale == candidate.scale) target = &candidate;
      Require(target != nullptr);
    } else Require(*scale <= 2); // The scale-only legacy schema admitted only 1/2.
    const bool has_capabilities = receipt.contains("capability_policy");
    if (has_capabilities) {
      Require(target && receipt.at("capability_policy") == "1" && receipt.at("capability_record") == "capabilities.txt" &&
              input.capabilities.size() <= kMaximumExportReceiptBytes);
      const auto plan = ResolveCapabilities({receipt.at("build"), receipt.at("game"), TimingMode::Stock, target->id, true});
      Require(plan.has_value());
      const auto canonical = SerializeCapabilities(*plan);
      Require(canonical && *canonical == input.capabilities);
    } else Require(input.capabilities.empty());

    const auto manifest = ParseJson(input.manifest), state = ParseJson(input.diagnostic_state);
    Require(Text(manifest, "schema", 64) == "mcla-diagnostic-package-v1" &&
            Text(manifest, "kind", 32) == "live" && Text(manifest, "platform", 32) == "windows" &&
            Text(state, "schema", 64) == "mcla-diagnostic-state-v1");
    const auto version = Text(manifest, "mcla_version", 23); Require(Version(version));
    const auto reason = Text(manifest, "reason", 1024);
    Require(Text(state, "reason", 1024) == reason);
    const bool frame_written = Boolean(Object(manifest, "capture"), "frame");
    const auto &artifacts = manifest.at("artifacts"); Require(artifacts.is_array() && artifacts.size() >= 4 && artifacts.size() <= 6);
    constexpr std::array<std::string_view, 6> allowed{
        "state.json","log-tail.txt","save-metadata.json","save-files-private.json","frame.bmp","process-private.dmp"};
    std::set<std::string> names;
    MetadataProjection projection;
    for (const auto &artifact : artifacts) {
      Require(artifact.is_object()); const auto name = Text(artifact, "name", 64);
      bool known = false; for (const auto candidate : allowed) known |= name == candidate;
      Require(known && names.insert(std::string(name)).second);
      if (name == "state.json") {
        const auto hash = Text(artifact, "sha256", 64); Require(ValidCapabilityIdentity(hash));
        projection.state_sha256 = hash;
        projection.state_bytes = Unsigned(artifact, "bytes", kMaximumExportJsonBytes);
        Require(projection.state_bytes > 0 && projection.state_bytes == input.diagnostic_state.size());
      }
    }
    for (const auto name : {"state.json","log-tail.txt","save-metadata.json","save-files-private.json"}) Require(names.contains(name));
    Require(names.contains("frame.bmp") == frame_written && !projection.state_sha256.empty());

    // Construct every emitted field from typed scalars/enums, never a source
    // subtree. Unknown source keys and source privacy flags have no authority.
    Json report = {
      {"schema","mcla-local-diagnostic-metadata-export-v1"}, {"version",1}, {"kind","live"},
      {"mcla_version",std::string(version)}, {"reason",std::string(Reason(reason))},
      {"session_recorded",{{"build_sha256",receipt.at("build")},{"game_sha256",receipt.at("game")},
          {"timing","stock"},{"scale",*scale},{"vsync",true},{"fullscreen",receipt.at("fullscreen") == "1"},
          {"gpu_vendor",std::string(Vendor(receipt.at("gpu")))},{"gpu_driver","unavailable"},{"cpu","unavailable"},
          {"capabilities",has_capabilities ? "recorded-v1" : "unavailable-legacy"}}},
      {"capture",{{"frame_written",frame_written}}},
      {"disclaimers",{"local-directory-provenance-not-authenticated","settings-receipt-reported-not-observed",
          "no-measured-fps-or-world-certification","review-before-sharing"}}
    };
    if (target) {
      report["session_recorded"]["render_target"] = {{"width",target->width},{"height",target->height}};
      report["session_recorded"]["guest_mode"] = {{"width",target->guest_width},{"height",target->guest_height}};
    }
    if (has_capabilities) report["session_recorded"]["guest_refresh_hz"] = kStockGuestRefreshHz;
    if (state.contains("window")) {
      const auto &window = Object(state, "window");
      report["diagnostic_window"] = {{"logical_width",Unsigned(window,"logical_width",8192)},
          {"logical_height",Unsigned(window,"logical_height",8192)},
          {"physical_width",Unsigned(window,"physical_width",8192)},
          {"physical_height",Unsigned(window,"physical_height",8192)},
          {"fullscreen",Boolean(window,"fullscreen")}};
    }
    projection.report_json = report.dump(2) + "\n";
    return projection;
  } catch (const std::exception &) {
    // Never include attacker-controlled source text/paths in a UI/export error.
    return std::unexpected("Unsupported, malformed or inconsistent live diagnostic metadata; nothing projected");
  }
}

} // namespace mcla::launcher::diagnostic_export
