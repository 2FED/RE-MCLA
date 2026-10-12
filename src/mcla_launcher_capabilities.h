#pragma once

#include "mcla_launcher_profile_policy.h"
#include "mcla_render_target_policy.h"

#include <expected>
#include <string>
#include <string_view>

namespace mcla::launcher {

inline constexpr uint32_t kCapabilityPolicyVersion = 1;
inline constexpr uint32_t kStockGuestRefreshHz = 60;
inline constexpr std::string_view kRenderTargetTestedBuild =
    "9C6BEA718046F1B22D7FB9C4BEB8E03CC52DD3F466CCFB926CC585059F580BAA";
inline constexpr std::string_view kCleanRenderTargetTestedBuild =
    "FED35A361F4A4A10BB03A6BC45072C82924085A0D3ADB8A3298243066DB4A850";
inline constexpr std::string_view kStockTimingKey = "stock";
inline constexpr std::string_view kFixed60UnavailableReason = "not yet validated";

enum class CapabilityError {
  InvalidBuildIdentity,
  InvalidGameIdentity,
  UnverifiedTiming,
  UnsupportedTiming,
  UnsupportedVsync,
  UnknownRenderTarget,
  UntestedTargetBuild,
  InconsistentPlan,
};

struct CapabilityRequest {
  std::string_view build_fingerprint;
  std::string_view game_fingerprint;
  TimingMode timing = TimingMode::Stock;
  uint32_t render_target = 720;
  bool vsync = true;
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
  bool vsync = true;
};

inline constexpr bool ValidCapabilityIdentity(std::string_view value) {
  if (value.size() != 64) return false;
  for (const char c : value)
    if (!((c >= '0' && c <= '9') || (c >= 'A' && c <= 'F'))) return false;
  return true;
}

inline constexpr bool TimingPolicySupported(TimingMode timing) {
  return timing == TimingMode::Stock;
}

inline std::string StockTimingDescription() {
  // The same stock-only policy drives admission and the visible limitation.
  return std::string(TimingPolicySupported(TimingMode::Stock) ? "Stock only. " : "Stock unavailable. ") +
      "60 FPS unavailable: " + std::string(kFixed60UnavailableReason) + ".";
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
  const auto *target = FindRenderTarget(request.render_target);
  if (!target) return std::unexpected(CapabilityError::UnknownRenderTarget);
  // Retain historical records and rollback alongside the reviewed clean SDK
  // candidate. Exact admission is not a world-quality or gameplay certificate.
  if ((target->id == 1080 || target->id == 2160) &&
      request.build_fingerprint != kRenderTargetTestedBuild &&
      request.build_fingerprint != kCleanRenderTargetTestedBuild)
    return std::unexpected(CapabilityError::UntestedTargetBuild);
  return CapabilityPlan{std::string(request.build_fingerprint), std::string(request.game_fingerprint),
                        TimingMode::Stock, *target, kStockGuestRefreshHz, true};
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
                                             plan.timing, plan.target.id, plan.vsync});
  if (!resolved) return std::unexpected(resolved.error());
  const auto &expected = resolved->target;
  if (plan.guest_refresh_hz != resolved->guest_refresh_hz ||
      plan.target.width != expected.width || plan.target.height != expected.height ||
      plan.target.guest_width != expected.guest_width || plan.target.guest_height != expected.guest_height ||
      plan.target.scale != expected.scale)
    return std::unexpected(CapabilityError::InconsistentPlan);
  return "MCLA-LAUNCHER-CAPABILITIES-V1\npolicy_version " + std::to_string(kCapabilityPolicyVersion) +
      "\nbuild " + plan.build_fingerprint + "\ngame " + plan.game_fingerprint +
      "\nsettings_scope resolved-launch-arguments\ntiming " + std::string(kStockTimingKey) +
      "\nguest_refresh_hz " + std::to_string(plan.guest_refresh_hz) +
      "\nvsync on\nrender_target_id " + std::to_string(plan.target.id) +
      "\nrender_target " + std::to_string(plan.target.width) + "x" + std::to_string(plan.target.height) +
      "\nguest_mode " + std::to_string(plan.target.guest_width) + "x" + std::to_string(plan.target.guest_height) +
      "\ndraw_scale " + std::to_string(plan.target.scale) + "x" + std::to_string(plan.target.scale) +
      "\nfixed60_available false\nfixed60_reason " + std::string(kFixed60UnavailableReason) +
      "\nhost_state_observed false\nfps_measured false\nworld_validation not-certified-by-policy\nrender_limitation " +
      std::string(RenderTargetLimitation(plan.target.id)) + "\n";
}

} // namespace mcla::launcher
