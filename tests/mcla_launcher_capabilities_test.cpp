#include "mcla_launcher_capabilities.h"

#include <array>
#include <iostream>
#include <map>
#include <string>

namespace {
using namespace mcla::launcher;
unsigned checks = 0, failures = 0;
void Check(bool pass, const char *label) {
  ++checks;
  if (!pass) { ++failures; std::cerr << "FAIL " << label << '\n'; }
}
void Reject(const CapabilityRequest &request, CapabilityError expected) {
  const auto result = ResolveCapabilities(request);
  Check(!result && result.error() == expected, "invalid capability request returns exact error");
}
// Independently parse the minimal emitted contract to check uniqueness and
// completeness, not just substrings which could hide contradictory duplicate keys.
std::map<std::string, std::string> Fields(std::string_view text) {
  std::map<std::string, std::string> result;
  const auto first = text.find('\n');
  if (first == text.npos || text.substr(0, first) != "MCLA-LAUNCHER-CAPABILITIES-V1" || text.back() != '\n') return {};
  text.remove_prefix(first + 1);
  while (!text.empty()) {
    const auto end = text.find('\n');
    if (end == text.npos) return {};
    const auto line = text.substr(0, end);
    const auto split = line.find(' ');
    if (split == line.npos || !split || split + 1 == line.size() ||
        !result.emplace(line.substr(0, split), line.substr(split + 1)).second) return {};
    text.remove_prefix(end + 1);
  }
  return result;
}
}

int main() {
  using namespace mcla::launcher;
  const std::string game = "95DA270942B4BDDBC8DC5EB45AA742AC6BE191D74E430022070D9B9B5AF25048";
  const std::string other_build(64, 'A');
  const CapabilityRequest baseline{kRenderTargetTestedBuild, game, TimingMode::Stock, 720, true};
  Check(kCapabilityPolicyVersion == 1 && kStockGuestRefreshHz == 60, "stable schema and guest refresh policy");
  Check(StockTimingDescription() == "Stock only. 60 FPS unavailable: not yet validated.", "UI description remains honest stock-only policy");
  Check(TimingPolicySupported(TimingMode::Stock) && !TimingPolicySupported(TimingMode::Fixed60), "UI timing support agrees with resolver");
  for (const auto &target : kRenderTargets) {
    auto request = baseline; request.render_target = target.id;
    const auto result = ResolveCapabilities(request);
    Check(result.has_value(), "pinned build admits each existing explicit target policy");
    if (!result) continue;
    Check(result->target.id == target.id && result->target.width == target.width &&
          result->target.height == target.height && result->target.guest_width == target.guest_width &&
          result->target.guest_height == target.guest_height && result->target.scale == target.scale,
          "resolved tuple exactly matches reviewed target table");
    Check(result->build_fingerprint == request.build_fingerprint && result->game_fingerprint == game &&
          result->timing == TimingMode::Stock && result->vsync && result->guest_refresh_hz == 60,
          "resolved identities and stock settings bind exact request");
    const auto text = SerializeCapabilities(*result);
    Check(text.has_value(), "valid resolved plan serializes");
    if (!text) continue;
    Check(SerializeCapabilities(*result) == text, "serialization deterministic");
    const auto fields = Fields(*text);
    const std::map<std::string,std::string> expected{
      {"policy_version","1"}, {"build",std::string(kRenderTargetTestedBuild)}, {"game",game},
      {"settings_scope","resolved-launch-arguments"}, {"timing","stock"}, {"guest_refresh_hz","60"},
      {"vsync","on"}, {"render_target_id",std::to_string(target.id)},
      {"render_target",std::to_string(target.width)+"x"+std::to_string(target.height)},
      {"guest_mode",std::to_string(target.guest_width)+"x"+std::to_string(target.guest_height)},
      {"draw_scale",std::to_string(target.scale)+"x"+std::to_string(target.scale)},
      {"fixed60_available","false"}, {"fixed60_reason","not yet validated"},
      {"host_state_observed","false"}, {"fps_measured","false"},
      {"world_validation","not-certified-by-policy"}, {"render_limitation",std::string(RenderTargetLimitation(target.id))}};
    Check(fields == expected, "serialized field set and values are exact without duplicate or extra keys");
    Check(text->find('\r') == text->npos && text->back() == '\n', "LF-only terminated record");
    const auto limitation = fields.find("render_limitation");
    if (target.id == 2160)
      Check(limitation != fields.end() && limitation->second.find("KI-032-open") != std::string::npos,
            "2160p explicitly retains open world corruption");
    if (target.id == 1080)
      Check(limitation != fields.end() && limitation->second.find("960x540") != std::string::npos,
            "1080p does not imply native1080 guest mode");
    request.build_fingerprint = other_build;
    if (target.id == 1080 || target.id == 2160) Reject(request, CapabilityError::UntestedTargetBuild);
    else Check(ResolveCapabilities(request).has_value(), "legacy target compatibility remains unchanged without new build validation claim");
    for (unsigned field = 0; field < 6; ++field) {
      auto bad = *result;
      switch(field) {
        case 0: ++bad.target.width; break;
        case 1: ++bad.target.height; break;
        case 2: ++bad.target.guest_width; break;
        case 3: ++bad.target.guest_height; break;
        case 4: ++bad.target.scale; break;
        case 5: bad.guest_refresh_hz = 1000; break;
      }
      const auto rejected = SerializeCapabilities(bad);
      Check(!rejected && rejected.error() == CapabilityError::InconsistentPlan, "serializer refuses mutated effective tuple");
    }
  }
  for (const auto invalid : {TimingMode::Fixed60, static_cast<TimingMode>(-1), static_cast<TimingMode>(2),
                            static_cast<TimingMode>(60), static_cast<TimingMode>(90), static_cast<TimingMode>(120)}) {
    auto request = baseline; request.timing = invalid;
    Reject(request, invalid == TimingMode::Fixed60 ? CapabilityError::UnverifiedTiming : CapabilityError::UnsupportedTiming);
    Check(!TimingPolicySupported(invalid), "invalid timing cannot become a UI-supported mode");
  }
  auto request = baseline; request.vsync = false; Reject(request, CapabilityError::UnsupportedVsync);
  for (const auto invalid : {0u,1u,2u,3u,1081u,4320u,0xffffffffu}) {
    request = baseline; request.render_target = invalid; Reject(request, CapabilityError::UnknownRenderTarget);
  }
  const std::array<std::string,8> invalid_identities{{"", std::string(63,'A'), std::string(65,'A'),
      std::string(64,'a'), std::string(64,'G'), std::string(63,'A')+"\n",
      std::string(63,'A')+std::string(1,'\0'), std::string(63,'A')+" "}};
  for (const auto &invalid : invalid_identities) {
    request = baseline; request.build_fingerprint = invalid; Reject(request, CapabilityError::InvalidBuildIdentity);
    request = baseline; request.game_fingerprint = invalid; Reject(request, CapabilityError::InvalidGameIdentity);
  }
  {
    std::string mutable_build(kRenderTargetTestedBuild), mutable_game(game);
    const auto owned = ResolveCapabilities({mutable_build, mutable_game, TimingMode::Stock, 1080, true});
    mutable_build.assign(64, 'B'); mutable_game.assign(64, 'C');
    Check(owned && owned->build_fingerprint == kRenderTargetTestedBuild && owned->game_fingerprint == game,
          "resolved identity owns bytes independently of request lifetime");
    if (owned) {
      auto stale = *owned; stale.build_fingerprint = other_build;
      const auto text = SerializeCapabilities(stale);
      Check(!text && text.error() == CapabilityError::UntestedTargetBuild,
            "serialized new-target plan cannot retain approval after build identity changes");
    }
  }
  auto plan = ResolveCapabilities(baseline);
  if (plan) {
    const auto text = SerializeCapabilities(*plan);
    const auto golden = std::string("MCLA-LAUNCHER-CAPABILITIES-V1\npolicy_version 1\nbuild ") +
        std::string(kRenderTargetTestedBuild) + "\ngame " + game +
        "\nsettings_scope resolved-launch-arguments\ntiming stock\nguest_refresh_hz 60\nvsync on"
        "\nrender_target_id 720\nrender_target 1280x720\nguest_mode 1280x720\ndraw_scale 1x1"
        "\nfixed60_available false\nfixed60_reason not yet validated\nhost_state_observed false"
        "\nfps_measured false\nworld_validation not-certified-by-policy"
        "\nrender_limitation baseline-rendering-limitations-remain\n";
    Check(text && *text == golden, "V1 serialization keeps its exact key order and delimiters");
    auto bad = *plan; bad.vsync = false; Check(!SerializeCapabilities(bad), "mutated VSync cannot serialize");
    bad = *plan; bad.timing = TimingMode::Fixed60; Check(!SerializeCapabilities(bad), "mutated60 cannot serialize");
    bad = *plan; bad.target.id = 0; Check(!SerializeCapabilities(bad), "mutated unknown target cannot serialize");
    bad = *plan; bad.game_fingerprint += "\ninjected true"; Check(!SerializeCapabilities(bad), "identity cannot inject serialized keys");
  }
  for (const auto error : {CapabilityError::InvalidBuildIdentity, CapabilityError::InvalidGameIdentity,
       CapabilityError::UnverifiedTiming, CapabilityError::UnsupportedTiming, CapabilityError::UnsupportedVsync,
       CapabilityError::UnknownRenderTarget, CapabilityError::UntestedTargetBuild, CapabilityError::InconsistentPlan})
    Check(!CapabilityErrorDescription(error).empty(), "every rejected capability has an explanation");
  std::cout << "Capabilities: " << checks << " checks, " << failures << " failures\n";
  return failures ? 1 : 0;
}
