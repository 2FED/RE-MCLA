#include "mcla_diagnostic_export_policy.h"

#include <iostream>
#include <string>
#include <utility>

namespace {
using namespace mcla::launcher;
using namespace mcla::launcher::diagnostic_export;
using Json = nlohmann::json;
unsigned checks = 0, failures = 0;
std::string current_case = "initialization", current_stage = "fixture construction", current_field;
const char *last_assertion = "none";
void Check(bool ok, const char *label) {
  last_assertion = label;
  ++checks;
  if (!ok) { ++failures; std::cerr << "FAIL " << label << '\n'; }
}
constexpr std::string_view kError =
    "Unsupported, malformed or inconsistent live diagnostic metadata; nothing projected";
const std::string kProfile(32, '1'), kHash(64, 'A'), kGame(64, 'B');
constexpr std::string_view kPrivate = "PRIVATE_SENTINEL_NEVER_EXPORT";

struct Fixture {
  std::string profile = kProfile;
  std::string receipt = "MCLA-MANAGED-SESSION-V1\nprofile " + kProfile +
      "\nbuild " + kHash + "\ngame " + kGame +
      "\ntiming stock\ngpu NVIDIA PRIVATE_SENTINEL_NEVER_EXPORT\nadapter 1\nscale 2\nvsync 1\nfullscreen 0\n";
  std::string session = "complete\nexit 0\n", capabilities, log_tail;
  Json state = {{"schema","mcla-diagnostic-state-v1"},{"reason","f10"}};
  Json manifest = {{"schema","mcla-diagnostic-package-v1"},{"kind","live"},
      {"platform","windows"},{"mcla_version","0.1.0.0"},{"reason","f10"},
      {"capture",{{"frame",false}}},
      {"artifacts",Json::array({{{"name","state.json"},{"sha256",kHash},{"bytes",1}},
          {{"name","log-tail.txt"}},{{"name","save-metadata.json"}},
          {{"name","save-files-private.json"}}})}};
  std::string state_bytes, manifest_bytes;

  void Freeze() {
    state_bytes = state.dump();
    // Negative fixtures must reach the production parser unchanged: do not
    // recreate absent artifacts or index a deliberately malformed container.
    const auto artifacts = manifest.find("artifacts");
    if (artifacts != manifest.end() && artifacts->is_array() && !artifacts->empty()) {
      auto &first = artifacts->front();
      if (first.is_object()) {
        const auto name = first.find("name");
        if (name != first.end() && name->is_string() && name->get_ref<const std::string &>() == "state.json")
          first["bytes"] = state_bytes.size();
      }
    }
    manifest_bytes = manifest.dump();
  }
  ExportMetadataInput Input() const {
    return {profile,receipt,session,manifest_bytes,state_bytes,capabilities,log_tail};
  }
  void Field(std::string_view key, std::string_view value) {
    const auto prefix = "\n" + std::string(key) + " ";
    const auto start = receipt.find(prefix);
    if (start == std::string::npos) { receipt += std::string(key) + " " + std::string(value) + "\n"; return; }
    const auto end = receipt.find('\n', start + 1);
    receipt.replace(start + prefix.size(), end - start - prefix.size(), value);
  }
  void Drop(std::string_view key) {
    const auto start = receipt.find("\n" + std::string(key) + " ");
    if (start != std::string::npos) receipt.erase(start + 1, receipt.find('\n',start + 1) - start);
  }
  void Target(uint32_t id, bool with_capabilities) {
    const auto *target = FindRenderTarget(id);
    Field("scale",std::to_string(target->scale));
    Field("render_target",std::to_string(target->width) + "x" + std::to_string(target->height));
    Field("guest_mode",std::to_string(target->guest_width) + "x" + std::to_string(target->guest_height));
    if (with_capabilities) {
      Field("build",kRenderTargetTestedBuild);
      Field("capability_policy","1"); Field("capability_record","capabilities.txt");
      const auto plan = ResolveCapabilities({kRenderTargetTestedBuild,kGame,TimingMode::Stock,id,true});
      capabilities = *SerializeCapabilities(*plan);
    }
  }
};

void RejectFrozen(const Fixture &fixture, const char *label) {
  current_case = label; current_stage = "ProjectLiveMetadata rejection";
  const auto result = ProjectLiveMetadata(fixture.Input());
  Check(!result, label);
  if (!result) Check(result.error() == kError, "rejection does not echo private input");
}
void Reject(Fixture fixture, const char *label) {
  current_case = label; current_stage = "Freeze rejection fixture";
  fixture.Freeze(); RejectFrozen(fixture,label);
}
Json Accept(Fixture fixture, const char *label) {
  current_case = label; current_stage = "Freeze acceptance fixture";
  fixture.Freeze(); current_stage = "ProjectLiveMetadata acceptance";
  const auto result = ProjectLiveMetadata(fixture.Input());
  Check(result.has_value(),label);
  if (!result) return Json::object();
  Check(result->state_sha256 == kHash && result->state_bytes == fixture.state_bytes.size(),
        "projection exposes exact descriptor for backend hash binding, not authentication");
  Check(!result->report_json.empty() && result->report_json.back() == '\n',"report terminates in LF");
  Check(ProjectLiveMetadata(fixture.Input())->report_json == result->report_json,"projection deterministic");
  Check(result->report_json.find(kPrivate) == std::string::npos,"private sentinel absent");
  return Json::parse(result->report_json);
}
}

int RunTests() {
  {
    // M9-005: per-minute performance counters from the package log tail.
    const std::string minute =
        "[2026-10-28 17:35:01.600] [info] [gpu] [t1] MCLA replay/min: submissions 21261 (streamed 21261), replay 6937137 us, end-of-submission wait 1 us\n"
        "[2026-10-28 17:35:01.601] [info] [gpu] [t1] MCLA frame pacing/min: frames 4942 (82.4 fps), interval us p50 11980 p90 16389 p95 18060 p99 22359 max 131938; >20ms 136 >34ms 7 >50ms 2 >100ms 1\n"
        "[2026-10-28 17:35:01.602] [info] [gpu] [t1] MCLA draws per frame/min: p10 2930 p50 3622 p90 4713 max 6873\n"
        "[2026-10-28 17:35:01.603] [info] [gpu] [t1] MCLA gpu busy/min: 49809 ms over 21261 submissions, 10.08 ms per frame\n";
    Fixture fixture;
    fixture.log_tail = "[x] [info] [core] Game directory: C:\\Users\\PRIVATE_SENTINEL_NEVER_EXPORT\\game\n" + minute +
        "[x] MCLA frame pacing/min: frames NaN (abc fps) PRIVATE_SENTINEL_NEVER_EXPORT\n";
    const auto report = Accept(fixture, "log tail with counters accepted");
    const auto perf = report.contains("performance_per_minute") ? report.at("performance_per_minute") : Json();
    Check(perf.is_array() && perf.size() == 2, "one record per pacing line; malformed line keeps no numbers");
    if (perf.is_array() && !perf.empty()) {
      const auto &m = perf.at(0);
      Check(m.at("fps") == 82.4 && m.at("frame_p50_ms") == 12.0 && m.at("frame_p95_ms") == 18.1 &&
            m.at("frames_over_50ms") == 2 && m.at("draws_p50") == 3622 && m.at("gpu_busy_ms_per_frame") == 10.1 &&
            m.at("cp_replay_ms_per_minute") == 6937.1, "counters parsed into rounded numbers");
      Check(perf.size() < 2 || !perf.at(1).contains("fps"), "unparseable values are omitted, not guessed");
    }
    Check(report.at("disclaimers").dump().find("performance-counters-from-log-not-certified") != std::string::npos,
          "performance block carries its own disclaimer");
    std::string many;
    for (int i = 0; i < 25; ++i) many += minute;
    fixture.log_tail = many;
    const auto bounded = Accept(fixture, "long log tail accepted");
    Check(bounded.at("performance_per_minute").size() == kMaximumPerformanceMinutes, "only the last 10 minutes are kept");
    fixture.log_tail = "[x] [info] [core] PRIVATE_SENTINEL_NEVER_EXPORT nothing to see\n";
    Check(!Accept(fixture, "log tail without counters accepted").contains("performance_per_minute"),
          "no counters, no performance block");
    fixture.log_tail = std::string(kMaximumExportLogTailBytes + 1, 'x');
    Reject(fixture, "oversized log tail is rejected");
  }
  {
    // M9-003: an unlocked session exports with its recorded mode, and its
    // capability record must be the canonical unlocked plan of a tested build.
    Fixture unlocked; unlocked.Field("timing","unlocked");
    unlocked.Field("scale","1"); unlocked.Field("render_target","1280x720"); unlocked.Field("guest_mode","1280x720");
    const std::string build(kUnlockedTestedBuilds[0]);
    unlocked.Field("build",build);
    unlocked.Field("capability_policy","1"); unlocked.Field("capability_record","capabilities.txt");
    const auto plan = ResolveCapabilities({build,kGame,TimingMode::Unlocked,720,true});
    unlocked.capabilities = plan ? *SerializeCapabilities(*plan) : std::string();
    const auto report = Accept(unlocked,"unlocked session metadata accepted");
    Check(report.contains("session_recorded") && report.at("session_recorded").at("timing") == "unlocked",
          "report records the unlocked mode, not stock");
    auto stock_record = unlocked;
    const auto stock_plan = ResolveCapabilities({build,kGame,TimingMode::Stock,720,true});
    stock_record.capabilities = stock_plan ? *SerializeCapabilities(*stock_plan) : std::string();
    Reject(stock_record,"unlocked receipt with a stock capability record is inconsistent");
    auto untested = unlocked; untested.Field("build",kHash);
    Reject(untested,"unlocked capability record of an untested build is rejected");
  }
  const auto baseline = Accept(Fixture{},"minimum historical live metadata accepted");
  const auto &record = baseline.at("session_recorded");
  Check(record.at("capabilities") == "unavailable-legacy" && !record.contains("guest_refresh_hz") &&
        !record.contains("render_target") && !record.contains("guest_mode"),"legacy never fabricates target or refresh");
  Check(record.at("gpu_vendor") == "NVIDIA" && record.at("gpu_driver") == "unavailable" &&
        record.at("cpu") == "unavailable", "only GPU vendor, no current hardware substitution");
  Check(baseline.at("schema") == "mcla-local-diagnostic-metadata-export-v1" && baseline.at("version") == 1 &&
        baseline.at("kind") == "live" && baseline.at("capture").at("frame_written") == false,
        "report schema and source kind explicit");
  Check(baseline.size() == 8 && record.size() == 10 && baseline.at("disclaimers") == Json::array({
        "local-directory-provenance-not-authenticated","settings-receipt-reported-not-observed",
        "no-measured-fps-or-world-certification","review-before-sharing"}),"report fixed fields and honest disclaimers");
  for (const auto scale : {1,2}) {
    Fixture f; f.Field("scale",std::to_string(scale));
    Check(Accept(f,"both legacy scales accepted").at("session_recorded").at("scale") == scale,"historical scale preserved");
  }
  for (const auto &target : kRenderTargets) for (const bool with_capabilities : {false,true}) {
    Fixture f; f.Target(target.id,with_capabilities);
    const auto report = Accept(f,"explicit tuple and optional capability contract accepted");
    const auto &r = report.at("session_recorded");
    Check(r.at("render_target") == Json({{"width",target.width},{"height",target.height}}) &&
          r.at("guest_mode") == Json({{"width",target.guest_width},{"height",target.guest_height}}) &&
          r.at("scale") == target.scale,"exact recorded target tuple projected");
    Check(r.contains("guest_refresh_hz") == with_capabilities,"refresh requires exact recorded capability contract");
    if (with_capabilities) Check(r.at("guest_refresh_hz") == 60,"recorded guest refresh not FPS");
  }
  // Build the full historical record from an existing canonical fixture, then
  // substitute BOTH recorded identities. This reaches the export resolver even
  // while the candidate is still policy-RED, without dereferencing a failed plan.
  // Literal pins ensure a future production constant replacement cannot hide
  // loss of old-session/rollback compatibility.
  for (const auto build : {
      "9C6BEA718046F1B22D7FB9C4BEB8E03CC52DD3F466CCFB926CC585059F580BAA",
      "FED35A361F4A4A10BB03A6BC45072C82924085A0D3ADB8A3298243066DB4A850"}) {
    for (const auto id : {1080u,2160u}) {
      Fixture f; f.Target(id,true);
      f.Field("build",build);
      const auto marker = f.capabilities.find("\nbuild ");
      Check(marker != std::string::npos,"historical capability fixture has exact build field");
      if (marker == std::string::npos) continue;
      f.capabilities.replace(marker + 7,64,build);
      const auto report = Accept(f,"old and clean high-target historical completed receipts both export");
      if (!report.empty()) {
        const auto &r = report.at("session_recorded");
        const auto *target = FindRenderTarget(id);
        Check(r.at("build_sha256") == build && r.at("game_sha256") == kGame && r.at("guest_refresh_hz") == 60 &&
              r.at("render_target") == Json({{"width",target->width},{"height",target->height}}) &&
              r.at("guest_mode") == Json({{"width",target->guest_width},{"height",target->guest_height}}) &&
              r.at("scale") == target->scale,"export retains historical identity and exact target, not current defaults");
      }
      auto mismatched = f; mismatched.Field("build",kHash);
      Reject(mismatched,"historical high-target receipt cannot borrow another build capability");
      auto stale = f;
      std::string changed(build); changed.back() = changed.back() == '0' ? '1' : '0';
      stale.Field("build",changed); stale.capabilities.replace(marker + 7,64,changed);
      Reject(stale,"matching but unreviewed high-target historical identity remains rejected");
      auto unrelated = f; unrelated.Field("build",kHash); unrelated.capabilities.replace(marker + 7,64,kHash);
      Reject(unrelated,"unrelated canonical high-target identity remains rejected in export");
    }
  }
  for (const auto &[gpu,vendor] : {std::pair{"AMD PRIVATE_SENTINEL_NEVER_EXPORT","AMD"},
       {"Intel(R) PRIVATE_SENTINEL_NEVER_EXPORT","Intel"},{"Intel PRIVATE_SENTINEL_NEVER_EXPORT","Intel"},
       {"Unrecognized PRIVATE_SENTINEL_NEVER_EXPORT","other"},{"NVIDIAevil PRIVATE_SENTINEL_NEVER_EXPORT","other"}}) {
    Fixture f; f.Field("gpu",gpu);
    Check(Accept(f,"vendor classification").at("session_recorded").at("gpu_vendor") == vendor,"vendor prefix bounded");
  }
  for (const auto &[reason,expected] : {std::pair{"f10","f10"},{"unexpected-guest-exit","unexpected-guest-exit"},
       {"PRIVATE_SENTINEL_NEVER_EXPORT","other"}}) {
    Fixture f; f.state["reason"] = reason; f.manifest["reason"] = reason;
    Check(Accept(f,"live reason enum").at("reason") == expected,"unknown reason is enum other, never a crash claim");
  }
  {
    Fixture f; f.state["window"] = {{"logical_width",0},{"logical_height",720},
        {"physical_width",8192},{"physical_height",1080},{"fullscreen",true},{"focused",kPrivate}};
    const auto report = Accept(f,"optional recorded window with zero and boundary values");
    Check(report.at("diagnostic_window").size() == 5 && report.at("diagnostic_window").at("fullscreen") == true,
          "window projection includes only dimensions and fullscreen");
    for (const auto &bad : {Json(-1),Json(1.0),Json("1280"),Json(true),Json(nullptr),Json(8193)}) {
      auto g = f; g.state["window"]["logical_width"] = bad; Reject(g,"window dimensions require bounded unsigned integers");
    }
    f.state["window"]["fullscreen"] = 1; Reject(f,"window fullscreen strictly boolean");
  }
  {
    // KI-026: the F10 state block guest_ki026 is diagnostic-only and has no
    // export authority; the projection is identical with and without it.
    Fixture f;
    f.state["guest_ki026"] = {{"ui_logic","8286D8D4"},{"camera_paused",1},
        {"warper",{{"state",29},{"queue",0},{"flags272",{0,1,0,0,0,0,0,0,0}}}},
        {"timer_scale",nullptr},{"complete",false},{"note",kPrivate}};
    const auto with_block = Accept(f,"extra guest_ki026 state block accepted");
    Check(with_block == Accept(Fixture{},"baseline without guest_ki026"),
          "guest_ki026 never reaches the exported metadata");
  }
  {
    Fixture f; f.manifest["capture"]["frame"] = true;
    f.manifest["artifacts"].push_back({{"name","frame.bmp"},{"sha256",kPrivate},{"safe_to_share",true}});
    f.manifest["artifacts"].push_back({{"name","process-private.dmp"},{"bytes",kPrivate}});
    f.state["process"] = {{"pid",kPrivate},{"threads",kPrivate},{"path",kPrivate}};
    f.state["runtime"] = {{"save",kPrivate},{"cpu",kPrivate}};
    f.manifest["privacy"] = {{"safe_to_share",true},{"path",kPrivate}};
    f.manifest["created_utc"] = kPrivate; f.manifest["capture"]["error"] = kPrivate;
    f.manifest["artifacts"][0]["safe_to_share"] = "NOT AN AUTHORIZATION";
    Check(Accept(f,"ignored private fields and non-state descriptors never copied").at("capture").at("frame_written") == true,
          "frame metadata allowed without copying image or trusting safe-to-share");
  }
  for (const auto &state : {"", "complete\nexit 0", "complete\r\nexit 0\r\n", "complete\nexit 00\n",
       "complete\nexit -1\n", "complete\nexit 4294967296\n", "complete\nexit 0\nextra\n", "running\n", "complete\nexit \n"}) {
    Fixture f; f.session = state; Reject(f,"only canonical completed session accepted");
  }
  { Fixture f; f.session = "complete\nexit 4294967295\n"; Accept(f,"completed nonzero exit is not crash certification"); }
  for (const auto &[key,value] : {std::pair{"profile","different"},{"build","lowercase"},{"game",""},
       {"timing","60"},{"timing","90"},{"timing","120"},{"timing","fixed60"},{"vsync","0"},
       {"fullscreen","true"},{"scale","0"},{"scale","3"},{"scale","02"},{"scale","2.0"},
       {"adapter","-1"},{"adapter","01"},{"adapter","4294967296"},{"gpu",""}}) {
    Fixture f; f.Field(key,value); Reject(f,"unsupported or malformed receipt setting rejected");
  }
  for (const auto key : {"profile","build","game","timing","gpu","adapter","scale","vsync","fullscreen"}) {
    Fixture f; f.Drop(key); Reject(f,"required receipt fields cannot be omitted");
  }
  {
    Fixture f; f.profile = std::string(32,'G'); Reject(f,"managed marker id must be canonical");
    f = {}; f.Field("build",std::string(64,'a')); Reject(f,"lowercase fingerprints rejected");
    f = {}; f.receipt += "scale 2\n"; Reject(f,"duplicate receipt field rejected");
    f = {}; f.receipt += "unknown PRIVATE_SENTINEL_NEVER_EXPORT\n"; Reject(f,"unknown receipt field rejected");
    f = {}; f.receipt.back() = '\r'; Reject(f,"receipt requires final LF");
    f = {}; f.receipt += '\0'; Reject(f,"receipt embedded NUL rejected");
    f = {}; f.Field("gpu",std::string("NVIDIA ") + char(0xc0) + char(0xaf)); Reject(f,"invalid UTF8 GPU rejected");
    f = {}; f.Field("gpu",std::string(801,'A')); Reject(f,"GPU input bounded");
    f = {}; f.receipt += std::string(kMaximumExportReceiptBytes,' '); Reject(f,"receipt total bounded");
  }
  for (const auto &[key,value] : {std::pair{"render_target","1920x1080"},{"guest_mode","960x540"},
       {"capability_policy","1"},{"capability_record","capabilities.txt"}}) {
    Fixture f; f.Field(key,value); Reject(f,"optional receipt pairs cannot be partial");
  }
  {
    Fixture f; f.Target(1080,false); f.Field("scale","1"); Reject(f,"tuple scale mismatch rejected");
    f = {}; f.Target(1080,false); f.Field("guest_mode","1280x720"); Reject(f,"tuple guest mismatch rejected");
    f = {}; f.Target(1080,false); f.Field("render_target","01920x1080"); Reject(f,"tuple canonical spelling required");
    f = {}; f.capabilities = "PRIVATE_SENTINEL_NEVER_EXPORT"; Reject(f,"orphan capability input rejected");
    f = {}; f.Target(1080,true); f.capabilities.clear(); Reject(f,"missing referenced capability input rejected");
    f = {}; f.Target(1080,true); f.capabilities += "\n"; Reject(f,"capability record must match byte exactly");
    f = {}; f.Target(1080,true); f.Field("capability_record","../capabilities.txt"); Reject(f,"capability reference fixed, traversal rejected");
    f = {}; f.Target(1080,true); f.Field("capability_policy","2"); Reject(f,"unknown capability version rejected");
    f = {}; f.Target(1080,true); f.Field("build",kHash); Reject(f,"new target capability rejects stale build gate");
    f = {}; f.Target(720,true); f.Field("game",kHash); Reject(f,"capability game identity binds receipt");
    f = {}; f.Target(720,true); f.Field("scale","2"); Reject(f,"capability tuple binds receipt");
  }
  for (const auto &[key,value] : {std::pair{"schema","unknown"},{"kind","crash"},{"platform","linux"},
       {"mcla_version","1.2.3"},{"mcla_version","01.2.3.4"},{"mcla_version","1.2.3.65536"},
       {"reason","different"}}) {
    Fixture f; f.manifest[key] = value; Reject(f,"manifest consumed fields and native crash unsupported");
  }
  for (const auto key : {"schema","kind","platform","mcla_version","reason","capture","artifacts"}) {
    current_field = key;
    Fixture f; f.manifest.erase(key); Reject(f,"required manifest field absent");
    f = {}; f.manifest[key] = false; Reject(f,"manifest consumed type must match");
  }
  current_field.clear();
  for (const auto key : {"schema","reason"}) {
    Fixture f; f.state.erase(key); Reject(f,"required state field absent");
    f = {}; f.state[key] = 1; Reject(f,"state field strict type");
  }
  {
    Fixture f; f.manifest["capture"]["frame"] = 1; Reject(f,"frame flag strict bool");
    f = {}; f.manifest["capture"]["frame"] = true; Reject(f,"frame flag requires listed artifact");
    f = {}; f.manifest["artifacts"].push_back({{"name","frame.bmp"}}); Reject(f,"frame artifact requires flag");
    f = {}; f.state["window"] = nullptr; Reject(f,"present optional window must be object");
    f = {}; f.state["window"] = Json::object(); Reject(f,"present window requires complete consumed fields");
  }
  for (const auto name : {"../state.json","STATE.JSON","sub/state.json","state.json:stream","unknown.txt","state.json "}) {
    Fixture f; f.manifest["artifacts"][1]["name"] = name; Reject(f,"artifact path aliases traversal unknown rejected");
  }
  {
    Fixture f; f.manifest["artifacts"][1]["name"] = "state.json"; Reject(f,"artifact duplicates rejected");
    f = {}; f.manifest["artifacts"].erase(1); Reject(f,"missing base artifact rejected");
    f = {}; f.manifest["artifacts"][0]["sha256"] = std::string(64,'a'); Reject(f,"descriptor hash canonical");
    f = {}; f.manifest["artifacts"][0]["sha256"] = true; Reject(f,"descriptor hash strict type");
    f = {}; f.manifest["artifacts"][1] = "log-tail.txt"; Reject(f,"artifact strict object");
    f = {}; f.manifest["artifacts"][1]["name"] = false; Reject(f,"artifact name strict string");
    for (const auto &bad : {Json(0),Json(-1),Json(1.0),Json("1"),Json(true),Json(kMaximumExportJsonBytes+1)}) {
      f = {}; f.Freeze(); f.manifest["artifacts"][0]["bytes"] = bad; f.manifest_bytes = f.manifest.dump();
      RejectFrozen(f,"descriptor byte count must be exact bounded unsigned");
    }
  }
  // Parser attacks in ignored fields are rejected too; none are echoed in errors.
  for (const bool manifest : {false,true}) {
    const auto raw = [&](std::string bytes) {
      Fixture f; f.Freeze();
      if (manifest) f.manifest_bytes = std::move(bytes); else f.state_bytes = std::move(bytes);
      RejectFrozen(f,"bounded strict JSON parser rejects malformed input");
    };
    raw("[]"); raw("{}"); raw("{} trailing"); raw("{/*comment*/}");
    raw("{\"x\":1,\"x\":2}"); raw("{\"x\":{\"secret\":1,\"secret\":2}}");
    raw("{\"ignored\":\"\\u0000\"}"); raw("{\"\\u0000\":1}");
    raw(std::string("{\"x\":\"") + char(0xff) + "\"}");
    raw(std::string("{\"x\":\"") + '\0' + "\"}");
    raw(std::string(kMaximumExportJsonBytes+1,' '));
    raw("{\"x\":\"\\ud800\"}");
    std::string deep = "{}"; for (int i=0;i<18;++i) deep = "{\"x\":" + deep + "}"; raw(deep);
    std::string events = "{\"x\":[0"; for (int i=0;i<8200;++i) events += ",0"; events += "]}"; raw(events);
  }
  // Use otherwise-valid documents so duplicate/ignored-string rejection cannot
  // pass merely because their mandatory fields were missing.
  {
    Fixture f; f.Freeze(); f.manifest_bytes.insert(1,"\"kind\":\"live\","); RejectFrozen(f,"valid manifest duplicate consumed field rejected");
    f = {}; f.Freeze(); f.manifest_bytes.insert(1,"\"ignored\":{\"x\":1,\"x\":2},"); RejectFrozen(f,"valid manifest duplicate ignored field rejected");
    f = {}; f.Freeze(); f.manifest_bytes.insert(1,"\"ignored\":\"\\u0000\","); RejectFrozen(f,"valid manifest hidden NUL rejected");
    f = {}; f.Freeze(); f.state_bytes.insert(1,"\"reason\":\"f10\",");
    f.manifest["artifacts"][0]["bytes"] = f.state_bytes.size(); f.manifest_bytes = f.manifest.dump();
    RejectFrozen(f,"valid state duplicate consumed field rejected");
  }
  {
    Fixture f; Json deep = Json::object();
    for (int i=0;i<18;++i) deep = {{"nested",deep}};
    f.manifest["ignored"] = deep; Reject(f,"depth budget enforced on otherwise valid document");
    f = {}; f.manifest["ignored"] = Json::array();
    for (int i=0;i<8200;++i) f.manifest["ignored"].push_back(i);
    Reject(f,"event budget enforced on otherwise valid document");
    f = {}; f.manifest["ignored"] = std::string(kMaximumExportJsonBytes,'A');
    Reject(f,"size budget enforced on otherwise valid document");
    f = {}; f.Freeze(); f.manifest_bytes.insert(1,std::string("\"ignored\":\"") + char(0xff) + "\",");
    RejectFrozen(f,"ignored string UTF8 validated in otherwise valid document");
    f = {}; f.Freeze(); f.manifest_bytes.insert(1,"\"ignored\":\"\\ud800\",");
    RejectFrozen(f,"lone surrogate rejected in otherwise valid document");
    f = {}; f.manifest["ignored"] = {{"nested",Json::array({1,true,nullptr,"PRIVATE_SENTINEL_NEVER_EXPORT"})}};
    Accept(f,"bounded well-formed ignored subtrees allowed without copying");
  }
  std::cout << "diagnostic export policy: " << checks << " checks, " << failures << " failures\n";
  return failures ? 1 : 0;
}

int main() {
  try {
    return RunTests();
  } catch (const std::exception &error) {
    // This executable consumes synthetic fixtures only. Production projection
    // continues to return a fixed generic error and never echoes source data.
    std::cerr << "UNEXPECTED TEST EXCEPTION after " << checks << " checks; case=" << current_case
              << "; stage=" << current_stage << "; field=" << current_field
              << "; last_assertion=" << last_assertion << "; what=" << error.what() << '\n';
    return 1;
  } catch (...) {
    std::cerr << "UNEXPECTED NON-STANDARD TEST EXCEPTION after " << checks << " checks; case=" << current_case
              << "; stage=" << current_stage << "; field=" << current_field << '\n';
    return 1;
  }
}
