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
  // Exact historical and clean-candidate identities are independent of the
  // production constant, so replacing the old pin cannot silently drop rollback.
  constexpr std::array<std::string_view,2> reviewed_builds{{
      "9C6BEA718046F1B22D7FB9C4BEB8E03CC52DD3F466CCFB926CC585059F580BAA",
      "FED35A361F4A4A10BB03A6BC45072C82924085A0D3ADB8A3298243066DB4A850"}};
  for (const auto build : reviewed_builds) for (const auto &target : kRenderTargets) {
    const CapabilityRequest candidate{build,game,TimingMode::Stock,target.id,true};
    const auto resolved = ResolveCapabilities(candidate);
    Check(resolved.has_value(), "both exact historical and clean builds retain all four target admissions");
    if (resolved) {
      Check(resolved->build_fingerprint == build && resolved->target.id == target.id &&
            resolved->target.width == target.width && resolved->target.height == target.height &&
            resolved->target.guest_width == target.guest_width && resolved->target.guest_height == target.guest_height &&
            resolved->target.scale == target.scale, "two-build admission preserves exact identity and tuple");
      const auto serialized = SerializeCapabilities(*resolved);
      Check(serialized.has_value(), "both builds can serialize historical capability records");
      if (serialized) {
        const auto fields = Fields(*serialized);
        Check(fields.contains("build") && fields.at("build") == build &&
              fields.contains("render_target_id") && fields.at("render_target_id") == std::to_string(target.id) &&
              fields.contains("world_validation") && fields.at("world_validation") == "not-certified-by-policy" &&
              fields.contains("fps_measured") && fields.at("fps_measured") == "false",
              "identity addition cannot certify world quality or measured FPS");
      }
    }
    auto no_vsync = candidate; no_vsync.vsync = false; Reject(no_vsync,CapabilityError::UnsupportedVsync);
    auto fixed60 = candidate; fixed60.timing = TimingMode::Fixed60; Reject(fixed60,CapabilityError::UnverifiedTiming);
  }
  // One-nibble mutations and an unrelated well-formed hash are not admission.
  for (const auto build : reviewed_builds) for (const auto id : {1080u,2160u}) {
    std::string stale(build); stale.back() = stale.back() == '0' ? '1' : '0';
    Reject({stale,game,TimingMode::Stock,id,true},CapabilityError::UntestedTargetBuild);
    Reject({other_build,game,TimingMode::Stock,id,true},CapabilityError::UntestedTargetBuild);
  }
  const CapabilityRequest baseline{kRenderTargetTestedBuild, game, TimingMode::Stock, 720, true};
  Check(kCapabilityPolicyVersion == 1 && kStockGuestRefreshHz == 60, "stable schema and guest refresh policy");
  Check(StockTimingDescription() == "Stock only. 60 FPS unavailable: not yet validated.", "UI description remains honest stock-only policy");
  Check(TimingPolicySupported(TimingMode::Stock) && TimingPolicySupported(TimingMode::Unlocked) &&
        !TimingPolicySupported(TimingMode::Fixed60), "UI timing support agrees with resolver");
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
          result->timing == TimingMode::Stock && result->vsync && result->guest_refresh_hz == 60 &&
          result->guest_vblank_hz == 0,
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
  for (const auto invalid : {TimingMode::Fixed60, static_cast<TimingMode>(-1), static_cast<TimingMode>(3),
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
       CapabilityError::UnknownRenderTarget, CapabilityError::UntestedTargetBuild, CapabilityError::InconsistentPlan,
       CapabilityError::UntestedUnlockedBuild})
    Check(!CapabilityErrorDescription(error).empty(), "every rejected capability has an explanation");
  // M9-003 unlocked timing: exact tested builds only, one extra serialized key.
  Check(kUnlockedGuestVblankHz == 240 && kUnlockedTestedBuilds.size() == 1, "unlocked policy constants");
  for (const auto build : kUnlockedTestedBuilds) for (const auto &target : kRenderTargets) {
    const CapabilityRequest unlocked{build, game, TimingMode::Unlocked, target.id, true};
    const auto plan = ResolveCapabilities(unlocked);
    if (target.id == 1080 || target.id == 2160) {
      Check(!plan && plan.error() == CapabilityError::UntestedTargetBuild,
            "unlocked does not widen render-target admission");
      continue;
    }
    Check(plan && plan->timing == TimingMode::Unlocked && plan->guest_vblank_hz == 240 &&
          plan->guest_refresh_hz == 60 && plan->vsync, "tested build admits unlocked with the 240 Hz override");
    if (!plan) continue;
    const auto text = SerializeCapabilities(*plan);
    Check(text.has_value(), "unlocked plan serializes");
    if (!text) continue;
    auto fields = Fields(*text);
    Check(fields.size() == 18 && fields.at("timing") == "unlocked" && fields.at("guest_vblank_hz") == "240" &&
          fields.at("fixed60_available") == "false" && fields.at("fps_measured") == "false",
          "unlocked record adds exactly one key and keeps the honest claims");
    Check(text->find("\nguest_refresh_hz 60\nguest_vblank_hz 240\nvsync on\n") != text->npos,
          "unlocked key has a fixed position");
    const auto arguments = TimingLaunchArguments(*plan);
    Check(arguments.size() == 1 && arguments.front() == L"--mcla_guest_vblank_hz=240",
          "unlocked launch adds exactly the vblank override argument");
    auto bad = *plan; bad.guest_vblank_hz = 1000;
    Check(!SerializeCapabilities(bad), "mutated vblank cannot serialize");
    bad = *plan; bad.guest_vblank_hz = 0;
    Check(!SerializeCapabilities(bad), "unlocked plan without override cannot serialize");
    bad = *plan; bad.timing = TimingMode::Stock;
    const auto stock_text = SerializeCapabilities(bad);
    Check(!stock_text && stock_text.error() == CapabilityError::InconsistentPlan,
          "stock record cannot carry an unlocked override");
    auto stock_request = unlocked; stock_request.timing = TimingMode::Stock;
    const auto stock = ResolveCapabilities(stock_request);
    std::expected<std::string, CapabilityError> stock_serialized = std::unexpected(CapabilityError::InconsistentPlan);
    if (stock) stock_serialized = SerializeCapabilities(*stock);
    Check(stock && stock->guest_vblank_hz == 0 && stock_serialized &&
          stock_serialized->find("guest_vblank_hz") == std::string::npos &&
          Fields(*stock_serialized).at("timing") == "stock",
          "returning to 30 FPS on the same build restores the stock record");
    Check(stock && TimingLaunchArguments(*stock).empty(), "stock launch adds no timing argument");
    auto no_vsync = unlocked; no_vsync.vsync = false; Reject(no_vsync, CapabilityError::UnsupportedVsync);
  }
  for (const auto build : {std::string(kRenderTargetTestedBuild), std::string(kCleanRenderTargetTestedBuild), other_build})
    Reject({build, game, TimingMode::Unlocked, 720, true}, CapabilityError::UntestedUnlockedBuild);
  {
    std::string stale(kUnlockedTestedBuilds[0]); stale.back() = stale.back() == '5' ? '4' : '5';
    Reject({stale, game, TimingMode::Unlocked, 720, true}, CapabilityError::UntestedUnlockedBuild);
  }
  Check(TimingLimitation(TimingMode::Unlocked).find("Experimental") != std::string::npos &&
        TimingLimitation(TimingMode::Stock).find("Recommended") != std::string::npos,
        "visible limits name the experimental and recommended modes");
  Check(CheckedSettingsDescription(1440, TimingMode::Stock) == CheckedStockSettingsDescription(1440) &&
        CheckedSettingsDescription(1440, TimingMode::Unlocked).find("guest vblank 240 Hz") != std::string::npos &&
        CheckedSettingsDescription(999, TimingMode::Unlocked).empty(),
        "checked description follows the selected mode");
  // KI-045 owner admission: an exact listed build gains the gated options;
  // records show the admission only when it was actually used.
  {
    const std::string owner_build(64, 'D');
    const std::string header(kOwnerAdmissionHeader);
    const std::string file = header + owner_build + " l41-internal\n" + other_build + "\n";
    Check(ParseOwnerAdmission(file) && ParseOwnerAdmission(file)->size() == 2, "owner list parses labels and bare rows");
    Check(OwnerAdmissionListed(file, owner_build) && OwnerAdmissionListed(file, other_build) &&
          !OwnerAdmissionListed(file, std::string(64, 'E')), "only exact listed fingerprints are admitted");
    for (const auto &bad : {std::string(), header,
                            header + owner_build + " \n", header + owner_build + " bad label\n",
                            header + owner_build + " " + std::string(33, 'x') + "\n",
                            header + std::string(64, 'd') + "\n", header + owner_build + "\n" + owner_build + "\n",
                            header + "\n", "MCLA-OWNER-ADMITTED-BUILDS-V2\n" + owner_build + "\n",
                            header + owner_build + "X\n"})
      if (bad != header) Check(!OwnerAdmissionListed(bad, owner_build), "malformed owner list admits nothing");
    Check(ParseOwnerAdmission(header) && ParseOwnerAdmission(header)->empty() &&
          !OwnerAdmissionListed(header, owner_build), "empty owner list is valid and admits nothing");
    // Owner incident 2026-10-29: hand-edited files (no final LF, CRLF, BOM).
    const std::string crlf_header = "MCLA-OWNER-ADMITTED-BUILDS-V1\r\n";
    const std::string bom = "\xEF\xBB\xBF";
    for (const auto &edited : {file.substr(0, file.size() - 1),
                               crlf_header + owner_build + " l41-internal\r\n" + other_build + "\r\n",
                               crlf_header + owner_build + " l41-internal\r\n" + other_build,
                               bom + file, bom + crlf_header + owner_build + " l41-internal\r\n" + other_build,
                               header + owner_build + " l41-internal\r\n" + other_build + "\n"}) {
      const auto parsed = CheckOwnerAdmission(edited);
      Check(parsed && parsed->size() == 2 && (*parsed)[0] == owner_build && (*parsed)[1] == other_build,
            "missing final LF, CRLF and a UTF-8 BOM are accepted");
      Check(OwnerAdmissionListed(edited, owner_build), "hand-edited owner list admits its builds");
    }
    Check(ParseOwnerAdmission(header.substr(0, header.size() - 1)) &&
          ParseOwnerAdmission(header.substr(0, header.size() - 1))->empty(),
          "header without a final LF is an empty list");
    const auto reason = [](const std::string &text) {
      const auto parsed = CheckOwnerAdmission(text);
      return parsed ? std::string("accepted") : OwnerAdmissionNotice(parsed.error());
    };
    Check(reason(header + owner_build + "\n" + std::string(63, 'A') + "G\n") ==
              "Owner admission file ignored: not a 64-character uppercase hex fingerprint (line 3).",
          "a non-hex row rejects the file and names its line");
    Check(reason(crlf_header + owner_build + "\r\n" + std::string(64, 'a')) ==
              "Owner admission file ignored: not a 64-character uppercase hex fingerprint (line 3).",
          "lowercase hex still rejects a CRLF file without final LF");
    Check(!OwnerAdmissionListed(header + owner_build + "\n" + std::string(63, 'A') + "G\n", owner_build),
          "a rejected file admits nothing, not even its valid rows");
    Check(reason("MCLA-OWNER-ADMITTED-BUILDS-V2\n") ==
              "Owner admission file ignored: first line is not MCLA-OWNER-ADMITTED-BUILDS-V1 (line 1).",
          "unknown header names line 1");
    Check(reason(std::string()) ==
              "Owner admission file ignored: first line is not MCLA-OWNER-ADMITTED-BUILDS-V1 (line 1).",
          "empty file is rejected with a reason");
    Check(reason(header + owner_build + "\n" + owner_build + "\n") ==
              "Owner admission file ignored: duplicate fingerprint (line 3).", "duplicate row names its line");
    Check(reason(header + owner_build + " bad label\n").find("label must be") != std::string::npos &&
              reason(header + owner_build + " bad label\n").ends_with("(line 2)."), "bad label names its line");
    Check(reason(header + "\n") == "Owner admission file ignored: empty line (line 2).", "empty row names its line");
    Check(reason(header + owner_build + "\r\r\n").find("(line 2)") != std::string::npos,
          "a stray CR is still malformed");
    Check(reason(header + owner_build + "\r" + other_build + "\n").find("(line 2)") != std::string::npos,
          "a CR-only line break is still malformed");
    Check(reason(bom + bom + file).find("(line 1)") != std::string::npos, "only one BOM is stripped");
    Check(reason(header + std::string(kOwnerAdmissionMaximumBytes, 'A') + "\n") ==
              "Owner admission file ignored: file is over 16 KiB.", "oversized file reason has no line");
    std::string many = header;
    for (int i = 0; i < 65; ++i) {
      std::string row(64, '0'); const auto digits = std::to_string(i);
      row.replace(64 - digits.size(), digits.size(), digits); many += row + "\n";
    }
    Check(!ParseOwnerAdmission(many), "owner list is bounded to 64 entries");
    Check(reason(many) == "Owner admission file ignored: more than 64 builds (line 66).",
          "the 65th build names its line");
    Check(ParseOwnerAdmission(many.substr(0, many.size() - 65)) &&
              ParseOwnerAdmission(many.substr(0, many.size() - 65))->size() == 64,
          "64 builds are accepted");
    Check(!ParseOwnerAdmission(header + std::string(kOwnerAdmissionMaximumBytes, 'A') + "\n"), "owner list is size bounded");

    Reject({owner_build, game, TimingMode::Stock, 2160, true}, CapabilityError::UntestedTargetBuild);
    Reject({owner_build, game, TimingMode::Unlocked, 720, true}, CapabilityError::UntestedUnlockedBuild);
    const auto stock = ResolveCapabilities({owner_build, game, TimingMode::Stock, 1440, true, true});
    Check(stock && !stock->owner_admitted && stock->guest_vblank_hz == 0,
          "owner admission is not recorded for an ungated stock request");
    if (stock) {
      const auto text = SerializeCapabilities(*stock);
      Check(text && Fields(*text).size() == 17 && text->find("admission") == std::string::npos,
            "ungated record stays byte-compatible");
      auto forged = *stock; forged.owner_admitted = true;
      const auto refused = SerializeCapabilities(forged);
      Check(!refused && refused.error() == CapabilityError::InconsistentPlan,
            "an unneeded admission claim cannot be serialized");
    }
    for (const auto &[timing, target] : {std::pair{TimingMode::Stock, 2160u}, std::pair{TimingMode::Stock, 1080u},
                                         std::pair{TimingMode::Unlocked, 720u}, std::pair{TimingMode::Unlocked, 2160u}}) {
      const auto plan = ResolveCapabilities({owner_build, game, timing, target, true, true});
      Check(plan && plan->owner_admitted && plan->target.id == target &&
            plan->guest_vblank_hz == (timing == TimingMode::Unlocked ? 240u : 0u),
            "owner-admitted build gains 1080p/2160p and Unlocked");
      if (!plan) continue;
      const auto text = SerializeCapabilities(*plan);
      Check(text && text->ends_with("\nadmission owner-allowlist\n") &&
            Fields(*text).size() == (timing == TimingMode::Unlocked ? 19u : 18u),
            "owner-admitted record names the admission as its last key");
      auto stripped = *plan; stripped.owner_admitted = false;
      Check(!SerializeCapabilities(stripped), "admitted plan cannot drop its admission key");
    }
    auto no_vsync = CapabilityRequest{owner_build, game, TimingMode::Unlocked, 720, false, true};
    Reject(no_vsync, CapabilityError::UnsupportedVsync);
    Reject({owner_build, game, TimingMode::Fixed60, 720, true, true}, CapabilityError::UnverifiedTiming);
    Reject({owner_build, game, TimingMode::Stock, 999, true, true}, CapabilityError::UnknownRenderTarget);
    const auto tested = ResolveCapabilities({std::string(kRenderTargetTestedBuild), game, TimingMode::Stock, 2160, true, true});
    Check(tested && !tested->owner_admitted, "tested builds never record owner admission");
  }
  std::cout << "Capabilities: " << checks << " checks, " << failures << " failures\n";
  return failures ? 1 : 0;
}
