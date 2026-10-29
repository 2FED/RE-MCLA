#pragma once

#include "mcla_launcher_profile_policy.h"
#include "mcla_render_target_policy.h"

#include <algorithm>
#include <array>
#include <expected>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace mcla::launcher {

inline constexpr uint32_t kCapabilityPolicyVersion = 1;
inline constexpr uint32_t kStockGuestRefreshHz = 60;
inline constexpr std::string_view kRenderTargetTestedBuild =
    "9C6BEA718046F1B22D7FB9C4BEB8E03CC52DD3F466CCFB926CC585059F580BAA";
inline constexpr std::string_view kCleanRenderTargetTestedBuild =
    "FED35A361F4A4A10BB03A6BC45072C82924085A0D3ADB8A3298243066DB4A850";
inline constexpr std::string_view kStockTimingKey = "stock";
inline constexpr std::string_view kUnlockedTimingKey = "unlocked";
inline constexpr std::string_view kFixed60UnavailableReason = "not yet validated";
// Unlocked (experimental, M9-003): guest vblank override passed to the host.
// The same value as the owner-tested l33-sunc/l34-sunc builds.
inline constexpr uint32_t kUnlockedGuestVblankHz = 240;
// Exact builds whose GPU plugin carries the vblank override and UI cap
// (SDK a239b83). Other builds keep the option refused, not silently stock.
inline constexpr std::array<std::string_view, 1> kUnlockedTestedBuilds{{
    "0789DBB5F8B59C698FC269B18E87D20A9EC6EB44913709A321829A0DF36DC105"}};
// Owner admission for internal candidates (KI-045). An optional file in the
// launcher's local settings directory (never in a build or package folder)
// lists exact build fingerprints the owner admits for the gated options
// (1080p/2160p render targets and Unlocked FPS). Absent, unreadable or
// malformed files admit nothing. Format: ASCII, LF line endings:
//   MCLA-OWNER-ADMITTED-BUILDS-V1
//   <64 uppercase hex fingerprint>[ <label>]   (label: 1-32 of A-Z a-z 0-9 . _ -)
// Hand-edited files are tolerated (owner incident 2026-10-29): a leading UTF-8
// BOM, CRLF line endings and a missing final LF are normalised before the
// strict row check; everything else malformed rejects the whole file, and the
// launcher shows the reason instead of a silent refusal.
inline constexpr std::wstring_view kOwnerAdmissionFile = L"owner-admitted-builds.txt";
inline constexpr std::string_view kOwnerAdmissionHeader = "MCLA-OWNER-ADMITTED-BUILDS-V1\n";
inline constexpr size_t kOwnerAdmissionMaximumBytes = 16 * 1024;
inline constexpr size_t kOwnerAdmissionMaximumEntries = 64;
inline constexpr std::string_view kOwnerAdmissionKey = "owner-allowlist";

enum class CapabilityError {
  InvalidBuildIdentity,
  InvalidGameIdentity,
  UnverifiedTiming,
  UnsupportedTiming,
  UnsupportedVsync,
  UnknownRenderTarget,
  UntestedTargetBuild,
  InconsistentPlan,
  UntestedUnlockedBuild,
};

struct CapabilityRequest {
  std::string_view build_fingerprint;
  std::string_view game_fingerprint;
  TimingMode timing = TimingMode::Stock;
  uint32_t render_target = 720;
  bool vsync = true;
  // The build is listed in the owner admission file. Used only when the
  // requested option is otherwise gated for this exact build.
  bool owner_admitted = false;
};

// Resolved arguments to emit, NOT measured FPS or observed host state. Caller
// must still pin and verify both inventories, admit the signed build lease,
// validate GPU/paths and hold profile locks. A pure hash string is not trust.
struct CapabilityPlan {
  std::string build_fingerprint;
  std::string game_fingerprint;
  TimingMode timing = TimingMode::Stock;
  RenderTargetPlan target{};
  uint32_t guest_refresh_hz = kStockGuestRefreshHz;
  // 0 = stock vblank (no argument emitted); otherwise the override in Hz.
  uint32_t guest_vblank_hz = 0;
  bool vsync = true;
  // True only when a gated option was admitted by the owner admission file.
  bool owner_admitted = false;
};

inline constexpr bool ValidCapabilityIdentity(std::string_view value) {
  if (value.size() != 64) return false;
  for (const char c : value)
    if (!((c >= '0' && c <= '9') || (c >= 'A' && c <= 'F'))) return false;
  return true;
}

inline constexpr bool TimingPolicySupported(TimingMode timing) {
  return timing == TimingMode::Stock || timing == TimingMode::Unlocked;
}

inline constexpr bool UnlockedBuildTested(std::string_view build) {
  for (const auto tested : kUnlockedTestedBuilds)
    if (build == tested) return true;
  return false;
}

// Why an existing owner admission file was ignored. line: 1-based line of
// the offending row (1 = header), 0 for the whole file.
struct OwnerAdmissionRejection {
  std::string_view reason;
  size_t line = 0;
};

// The size limit applies to the raw bytes. After the BOM / CRLF / final-LF
// normalisation every row is checked strictly; any malformed row rejects the
// whole file.
inline std::expected<std::vector<std::string>, OwnerAdmissionRejection> CheckOwnerAdmission(
    std::string_view raw) {
  using Rejected = std::unexpected<OwnerAdmissionRejection>;
  if (raw.size() > kOwnerAdmissionMaximumBytes) return Rejected({"file is over 16 KiB", 0});
  if (raw.starts_with("\xEF\xBB\xBF")) raw.remove_prefix(3);
  std::string normalised;
  normalised.reserve(raw.size() + 1);
  for (size_t i = 0; i < raw.size(); ++i)
    if (!(raw[i] == '\r' && i + 1 < raw.size() && raw[i + 1] == '\n')) normalised.push_back(raw[i]);
  if (!normalised.empty() && normalised.back() != '\n') normalised.push_back('\n');
  std::string_view text = normalised;
  if (!text.starts_with(kOwnerAdmissionHeader))
    return Rejected({"first line is not MCLA-OWNER-ADMITTED-BUILDS-V1", 1});
  text.remove_prefix(kOwnerAdmissionHeader.size());
  std::vector<std::string> builds;
  size_t line = 1;
  while (!text.empty()) {
    ++line;
    const auto end = text.find('\n');  // never npos: the text ends with LF
    const auto row = text.substr(0, end);
    text.remove_prefix(end + 1);
    if (builds.size() >= kOwnerAdmissionMaximumEntries) return Rejected({"more than 64 builds", line});
    if (row.empty()) return Rejected({"empty line", line});
    if (row.size() < 64 || !ValidCapabilityIdentity(row.substr(0, 64)))
      return Rejected({"not a 64-character uppercase hex fingerprint", line});
    const auto build = row.substr(0, 64);
    if (row.size() > 64) {
      const auto label = row.substr(65);
      bool valid = row[64] == ' ' && !label.empty() && label.size() <= 32;
      for (const char c : label)
        valid = valid && ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
                          (c >= '0' && c <= '9') || c == '.' || c == '_' || c == '-');
      if (!valid) return Rejected({"label must be one space and 1-32 of A-Z a-z 0-9 . _ -", line});
    }
    if (std::find(builds.begin(), builds.end(), build) != builds.end())
      return Rejected({"duplicate fingerprint", line});
    builds.emplace_back(build);
  }
  return builds;
}

// Parse of the owner admission file. nullopt when the file is rejected.
inline std::optional<std::vector<std::string>> ParseOwnerAdmission(std::string_view text) {
  auto builds = CheckOwnerAdmission(text);
  if (!builds) return std::nullopt;
  return std::move(*builds);
}

inline bool OwnerAdmissionListed(std::string_view file_text, std::string_view build) {
  const auto builds = ParseOwnerAdmission(file_text);
  return builds && std::find(builds->begin(), builds->end(), build) != builds->end();
}

// Status text for a rejected file; the file admits nothing.
inline std::string OwnerAdmissionNotice(const OwnerAdmissionRejection &rejection) {
  std::string notice = "Owner admission file ignored: " + std::string(rejection.reason);
  if (rejection.line) notice += " (line " + std::to_string(rejection.line) + ")";
  return notice + ".";
}

inline std::string StockTimingDescription() {
  // The same stock-only policy drives admission and the visible limitation.
  return std::string(TimingPolicySupported(TimingMode::Stock) ? "Stock only. " : "Stock unavailable. ") +
      "60 FPS unavailable: " + std::string(kFixed60UnavailableReason) + ".";
}

// Visible limits next to the frame-rate choice; the same text for every build.
inline std::string TimingLimitation(TimingMode timing) {
  if (timing == TimingMode::Unlocked)
    return "Experimental: fast menus over the city, 30-40 FPS at busy junctions, night light flicker.";
  return "Stock 30 FPS, the original pacing. Recommended.";
}

// Presentation after a successful read-only check, not an admission operation.
// Two short lines fit the existing status control without hiding the caveat.
inline std::string CheckedStockSettingsDescription(uint32_t render_target) {
  const auto *target = FindRenderTarget(render_target);
  if (!target) return {};
  return "Stock timing | VSync On | render " + std::to_string(target->width) +
      " x " + std::to_string(target->height) + " | guest refresh " +
      std::to_string(kStockGuestRefreshHz) + " Hz.\r\n"
      "Checked arguments, not measured FPS or world quality. Start Game rechecks.";
}

inline std::string CheckedSettingsDescription(uint32_t render_target, TimingMode timing) {
  if (timing != TimingMode::Unlocked) return CheckedStockSettingsDescription(render_target);
  const auto *target = FindRenderTarget(render_target);
  if (!target) return {};
  return "Unlocked FPS (experimental) | VSync On | render " + std::to_string(target->width) +
      " x " + std::to_string(target->height) + " | guest vblank " +
      std::to_string(kUnlockedGuestVblankHz) + " Hz after the start delay.\r\n"
      "Checked arguments, not measured FPS or world quality. Start Game rechecks.";
}

inline std::expected<CapabilityPlan, CapabilityError> ResolveCapabilities(
    const CapabilityRequest &request) {
  if (!ValidCapabilityIdentity(request.build_fingerprint))
    return std::unexpected(CapabilityError::InvalidBuildIdentity);
  if (!ValidCapabilityIdentity(request.game_fingerprint))
    return std::unexpected(CapabilityError::InvalidGameIdentity);
  if (request.timing == TimingMode::Fixed60)
    return std::unexpected(CapabilityError::UnverifiedTiming);
  if (!TimingPolicySupported(request.timing))
    return std::unexpected(CapabilityError::UnsupportedTiming);
  if (!request.vsync) return std::unexpected(CapabilityError::UnsupportedVsync);
  const bool owner_unlocked =
      request.timing == TimingMode::Unlocked && !UnlockedBuildTested(request.build_fingerprint);
  if (owner_unlocked && !request.owner_admitted)
    return std::unexpected(CapabilityError::UntestedUnlockedBuild);
  const auto *target = FindRenderTarget(request.render_target);
  if (!target) return std::unexpected(CapabilityError::UnknownRenderTarget);
  // Retain historical records and rollback alongside the reviewed clean SDK
  // candidate. Exact admission is not a world-quality or gameplay certificate.
  const bool owner_target = (target->id == 1080 || target->id == 2160) &&
      request.build_fingerprint != kRenderTargetTestedBuild &&
      request.build_fingerprint != kCleanRenderTargetTestedBuild;
  if (owner_target && !request.owner_admitted)
    return std::unexpected(CapabilityError::UntestedTargetBuild);
  const bool unlocked = request.timing == TimingMode::Unlocked;
  // Owner admission is recorded only when it actually admitted a gated option.
  return CapabilityPlan{std::string(request.build_fingerprint), std::string(request.game_fingerprint),
                        request.timing, *target, kStockGuestRefreshHz,
                        unlocked ? kUnlockedGuestVblankHz : 0, true, owner_unlocked || owner_target};
}

// Host arguments added by the frame-rate choice. Stock adds none, so returning
// to 30 FPS restores the exact stock command line; unlocked adds only the
// admitted guest vblank override.
inline std::vector<std::wstring> TimingLaunchArguments(const CapabilityPlan &plan) {
  if (!plan.guest_vblank_hz) return {};
  return {L"--mcla_guest_vblank_hz=" + std::to_wstring(plan.guest_vblank_hz)};
}

inline std::string_view CapabilityErrorDescription(CapabilityError error) {
  switch (error) {
    case CapabilityError::InvalidBuildIdentity: return "Build fingerprint must be canonical uppercase SHA256";
    case CapabilityError::InvalidGameIdentity: return "Game fingerprint must be canonical uppercase SHA256";
    case CapabilityError::UnverifiedTiming: return "60 FPS is not yet validated";
    case CapabilityError::UnsupportedTiming: return "Only stock timing is supported";
    case CapabilityError::UnsupportedVsync: return "Stock launch requires VSync On";
    case CapabilityError::UnknownRenderTarget: return "Select a supported render resolution target";
    case CapabilityError::UntestedTargetBuild: return "1080p/2160p require the render-target-tested build";
    case CapabilityError::InconsistentPlan: return "Resolved settings disagree with the capability policy";
    case CapabilityError::UntestedUnlockedBuild: return "Unlocked FPS is not verified for this build; choose 30 FPS";
  }
  return "Unknown capability error";
}

inline constexpr std::string_view RenderTargetLimitation(uint32_t id) {
  if (id == 1080) return "experimental-960x540-guest-plus-2x;vinyl-KI-033-open";
  if (id == 2160) return "world-sign-corruption-KI-032-open;vinyl-KI-033-open";
  return "baseline-rendering-limitations-remain";
}

// Stable ASCII LF-only key/value record, fixed order, one mandatory final LF.
// No personal paths/names, save data, credentials or inferred runtime metrics.
// Public plans are revalidated so mutation cannot serialize a forged capability.
inline std::expected<std::string, CapabilityError> SerializeCapabilities(const CapabilityPlan &plan) {
  const auto resolved = ResolveCapabilities({plan.build_fingerprint, plan.game_fingerprint,
                                             plan.timing, plan.target.id, plan.vsync,
                                             plan.owner_admitted});
  if (!resolved) return std::unexpected(resolved.error());
  const auto &expected = resolved->target;
  if (plan.owner_admitted != resolved->owner_admitted ||
      plan.guest_refresh_hz != resolved->guest_refresh_hz ||
      plan.guest_vblank_hz != resolved->guest_vblank_hz ||
      plan.target.width != expected.width || plan.target.height != expected.height ||
      plan.target.guest_width != expected.guest_width || plan.target.guest_height != expected.guest_height ||
      plan.target.scale != expected.scale)
    return std::unexpected(CapabilityError::InconsistentPlan);
  return "MCLA-LAUNCHER-CAPABILITIES-V1\npolicy_version " + std::to_string(kCapabilityPolicyVersion) +
      "\nbuild " + plan.build_fingerprint + "\ngame " + plan.game_fingerprint +
      "\nsettings_scope resolved-launch-arguments\ntiming " +
      std::string(plan.timing == TimingMode::Unlocked ? kUnlockedTimingKey : kStockTimingKey) +
      "\nguest_refresh_hz " + std::to_string(plan.guest_refresh_hz) +
      // Stock records stay byte-identical to policy V1; only unlocked adds a key.
      (plan.guest_vblank_hz ? "\nguest_vblank_hz " + std::to_string(plan.guest_vblank_hz) : std::string()) +
      "\nvsync on\nrender_target_id " + std::to_string(plan.target.id) +
      "\nrender_target " + std::to_string(plan.target.width) + "x" + std::to_string(plan.target.height) +
      "\nguest_mode " + std::to_string(plan.target.guest_width) + "x" + std::to_string(plan.target.guest_height) +
      "\ndraw_scale " + std::to_string(plan.target.scale) + "x" + std::to_string(plan.target.scale) +
      "\nfixed60_available false\nfixed60_reason " + std::string(kFixed60UnavailableReason) +
      "\nhost_state_observed false\nfps_measured false\nworld_validation not-certified-by-policy\nrender_limitation " +
      std::string(RenderTargetLimitation(plan.target.id)) +
      // Records without owner admission stay byte-identical to earlier ones.
      (plan.owner_admitted ? "\nadmission " + std::string(kOwnerAdmissionKey) : std::string()) + "\n";
}

} // namespace mcla::launcher
