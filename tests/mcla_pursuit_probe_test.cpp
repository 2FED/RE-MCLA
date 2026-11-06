// M11-002 / KI-066: pure parts of the pursuit diagnostic
// (src/mcla_pursuit_probe.h) and the generated code its offsets mirror
// (MCLA_PURSUIT_GENERATED_DIR).

#include "mcla_pursuit_probe.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#ifndef MCLA_PURSUIT_GENERATED_DIR
#error "MCLA_PURSUIT_GENERATED_DIR (generated/default) is set by CMake"
#endif

namespace {
int checks = 0;
void Check(bool value, const char *message) {
  if (!value) {
    std::fprintf(stderr, "FAIL: %s\n", message);
    std::exit(1);
  }
  ++checks;
}

namespace pursuit = mcla::pursuit;

void TestNames() {
  Check(pursuit::TacticTypeFromGetter(0x821A5CC0u) == 0 &&
            pursuit::TacticTypeFromGetter(0x82130008u) == 1 &&
            pursuit::TacticTypeFromGetter(0x823CD3D8u) == 2 &&
            pursuit::TacticTypeFromGetter(0x821A8AA0u) == 3 &&
            pursuit::TacticTypeFromGetter(0x821A7B50u) == 4,
        "the five li r3,N getters");
  Check(pursuit::TacticTypeFromGetter(0) == -1 && pursuit::TacticTypeFromGetter(0x82130000u) == -1,
        "unknown getters");
  Check(std::strcmp(pursuit::TacticName(0), "responding") == 0 &&
            std::strcmp(pursuit::TacticName(1), "pull-over") == 0 &&
            std::strcmp(pursuit::TacticName(2), "pursuit") == 0 &&
            std::strcmp(pursuit::TacticName(3), "pursuit-dbl") == 0 &&
            std::strcmp(pursuit::TacticName(4), "arrow-trap") == 0 &&
            std::strcmp(pursuit::TacticName(-1), "unknown") == 0,
        "tactic names");
  Check(std::strcmp(pursuit::CutsceneName(0), "none") == 0 &&
            std::strcmp(pursuit::CutsceneName(1), "copcam") == 0 &&
            std::strcmp(pursuit::CutsceneName(2), "ticket") == 0 &&
            std::strcmp(pursuit::CutsceneName(3), "arrest") == 0 &&
            std::strcmp(pursuit::CutsceneName(9), "unknown") == 0,
        "cut scene names");
  Check(pursuit::InfractionText(0) == "none", "no infraction");
  Check(pursuit::InfractionText(0x4000) == "block-traffic", "block traffic");
  Check(pursuit::InfractionText(0x4001) == "speed-low+block-traffic", "two bits in order");
  Check(pursuit::InfractionText(0x100) == "0x00000100", "unknown bit as hex");
  Check(pursuit::InfractionText(0x8102) == "speed-mid+0x00008100", "known and unknown bits");
}

void TestBand() {
  pursuit::Thresholds t;
  Check(std::strcmp(pursuit::ScoreBand(300, t), "unknown") == 0, "no thresholds");
  t.read = true;
  t.max = 1000;
  t.pull_over = 250;
  t.bullhorn = 500;
  t.pursuit = 600;
  Check(std::strcmp(pursuit::ScoreBand(0, t), "zero") == 0, "zero");
  Check(std::strcmp(pursuit::ScoreBand(249, t), "below") == 0, "below pull-over");
  Check(std::strcmp(pursuit::ScoreBand(250, t), "pull-over") == 0, "pull-over at 250");
  Check(std::strcmp(pursuit::ScoreBand(599, t), "pull-over") == 0, "pull-over below 600");
  Check(std::strcmp(pursuit::ScoreBand(600, t), "pursuit") == 0, "pursuit at 600");
  Check(std::strcmp(pursuit::ScoreBand(1000, t), "max") == 0, "max");
  t.pull_over = 0;
  Check(std::strcmp(pursuit::ScoreBand(300, t), "unknown") == 0, "zero threshold is unread");
}

void TestPhaseAndChanges() {
  pursuit::Snapshot s;
  Check(std::strcmp(pursuit::Phase(s), "no-manager") == 0, "no manager");
  s.present = true;
  Check(std::strcmp(pursuit::Phase(s), "none") == 0, "no police activity");
  s.grace = true;
  Check(std::strcmp(pursuit::Phase(s), "grace") == 0, "grace after a resolution");
  s.player_tactic = true;
  s.tactic_type = 1;
  Check(std::strcmp(pursuit::Phase(s), "pull-over") == 0, "tactic wins over grace");
  s.cutscene = 2;
  Check(std::strcmp(pursuit::Phase(s), "ticket-cutscene") == 0, "ticket cut scene wins");
  s.cutscene = 3;
  Check(std::strcmp(pursuit::Phase(s), "arrest-cutscene") == 0, "arrest");
  s.cutscene = 1;
  Check(std::strcmp(pursuit::Phase(s), "copcam-cutscene") == 0, "copcam");

  pursuit::Snapshot a;
  a.present = true;
  a.manager = 0xBC000000u;
  pursuit::Snapshot b = a;
  b.score = 400;
  b.grace_s = 1.0f;
  Check(pursuit::Changes(a, b).empty(), "score and grace seconds alone are no transition");
  b.bits_now = 0x4000;
  Check(pursuit::Changes(a, b) == "breaking", "law breaking starts");
  b.bits_now = 0x4001;
  a.bits_now = 0x4000;
  Check(pursuit::Changes(a, b).empty(), "a different infraction while breaking is no transition");
  b.player_tactic = true;
  b.tactic = 0xBC001000u;
  b.tactic_type = 1;
  b.fsm_state = 0;
  b.bits_latched = 0x4000;
  Check(pursuit::Changes(a, b) == "latched,tactic,fsm", "pull-over starts");
  pursuit::Snapshot c = b;
  c.cutscene = 2;
  c.busy = 1;
  c.fine = 100;
  Check(pursuit::Changes(b, c) == "cutscene,busy,fine", "ticket");
  pursuit::Snapshot d = c;
  d.present = false;
  d.manager = 0;
  Check(pursuit::Changes(c, d).rfind("manager", 0) == 0, "manager gone");
  pursuit::Snapshot e = a;
  e.grace = true;
  Check(pursuit::Changes(a, e) == "grace", "grace starts");
}

// --- generated code --------------------------------------------------------------
std::string ReadFile(const std::string &path) {
  std::ifstream in(path, std::ios::binary);
  std::ostringstream text;
  text << in.rdbuf();
  return text.str();
}

std::string Body(const std::string &text, const std::string &name) {
  const std::string head = "DEFINE_REX_FUNC(" + name + ") {";
  const size_t begin = text.find(head);
  if (begin == std::string::npos) {
    return {};
  }
  const size_t end = text.find("DEFINE_REX_FUNC(", begin + head.size());
  return text.substr(begin, end == std::string::npos ? std::string::npos : end - begin);
}

void InOrder(const std::string &body, const std::vector<std::string> &lines, const char *what) {
  size_t from = 0;
  for (const auto &line : lines) {
    const size_t at = body.find(line, from);
    if (at == std::string::npos) {
      std::fprintf(stderr, "missing in %s: %s\n", what, line.c_str());
      Check(false, what);
    }
    from = at + line.size();
  }
  Check(true, what);
}

std::string Gen(const char *file) {
  return ReadFile(std::string(MCLA_PURSUIT_GENERATED_DIR) + "/" + file);
}

void TestGenerated() {
  // kManagerPtr = 0x82870000 - 10016.
  static_assert(pursuit::kManagerPtr == 0x82870000u - 10016u);
  static_assert(pursuit::kTuning == 0x82830000u - 13624u);
  static_assert(pursuit::kDtAddress == 0x827D0000u + 29952u + 8u);
  const std::string g14 = Gen("mcla_recomp.14.cpp");
  const std::string g55 = Gen("mcla_recomp.55.cpp");
  Check(!g14.empty() && !g55.empty(), "generated files readable");
  InOrder(Body(g14, "sub_822CB9C8"),
          {"// li r3,228", "// lis r10,-32121", "// stw r31,-10016(r10)"},
          "manager created and stored at 0x8286D8E0");
  InOrder(Body(g14, "sub_822CB350"),
          {"// lis r11,-32121", "// lwz r3,-10016(r11)", "// bl 0x8270ae38"},
          "the frame update calls mcPoliceManager::Update with the global");
  InOrder(Body(g55, "sub_82705DA0"),
          {"// mr r29,r4", "// mr r31,r3", "// mulli r11,r29," + std::to_string(pursuit::kRecordStride),
           "// lis r10,-32125", "// add r28,r11,r31", "// mr r26,r5", "// mr r25,r6",
           "// addi r30,r10,-13624", "// lwz r24," + std::to_string(pursuit::kScore) + "(r28)",
           "// stw r10," + std::to_string(pursuit::kScore) + "(r28)"},
          "AddLawBreakScore: record = M + 68 * index, score +104, tuning table");
  InOrder(Body(g55, "sub_82704228"),
          {"// mr r26,r3", "// mr r27,r4", "// mr r23,r5", "// mr r28,r6"},
          "StartTactic registers");
  InOrder(Body(g55, "sub_8270AE38"),
          {"// mr r30,r3", "// lbz r11," + std::to_string(pursuit::kBusy) + "(r30)",
           "// lis r11,-32131", "// addi r10,r11,29952", "// lfs f31,8(r10)",
           "// lfs f13," + std::to_string(pursuit::kGrace) + "(r30)", "// fsubs f12,f13,f31",
           "// stfs f12," + std::to_string(pursuit::kGrace) + "(r30)"},
          "Update: busy flag, dt, grace countdown");
  InOrder(Body(g55, "sub_82703428"), {"// li r10,1", "// stw r10,60(r11)"}, "CopCam = 1");
  InOrder(Body(g55, "sub_82703490"), {"// li r8,2", "// stw r8,60(r11)"}, "Ticket = 2");
  InOrder(Body(g55, "sub_82703510"), {"// li r10,3", "// stw r10,60(r11)"}, "Arrest = 3");
  static_assert(pursuit::kCutscene == 60);
  Check(g55.find("// stw r3," + std::to_string(pursuit::kFine) + "(r19)") != std::string::npos,
        "fine stored at +76");
  InOrder(Body(Gen("mcla_recomp.56.cpp"), "sub_82712558"),
          {"// stb r11," + std::to_string(pursuit::kTacticActive) + "(r3)",
           "// stw r11," + std::to_string(pursuit::kTacticResult) + "(r3)"},
          "tactic ctor: active +20, result +24");
  InOrder(Body(Gen("mcla_recomp.61.cpp"), "sub_827A7220"),
          {"// stw r11," + std::to_string(pursuit::kFsmStates) + "(r3)",
           "// stw r10," + std::to_string(pursuit::kFsmCurrent) + "(r3)"},
          "Fsm ctor: states +4, current -1 at +12");
  // The five type getters.
  const struct {
    const char *file;
    const char *fn;
    const char *li;
  } getters[] = {{"mcla_recomp.4.cpp", "sub_821A5CC0", "// li r3,0"},
                 {"mcla_recomp.0.cpp", "sub_82130008", "// li r3,1"},
                 {"mcla_recomp.23.cpp", "sub_823CD3D8", "// li r3,2"},
                 {"mcla_recomp.4.cpp", "sub_821A8AA0", "// li r3,3"},
                 {"mcla_recomp.4.cpp", "sub_821A7B50", "// li r3,4"}};
  for (const auto &g : getters) {
    InOrder(Body(Gen(g.file), g.fn), {g.li, "// blr"}, "tactic type getter");
  }
}

}  // namespace

int main() {
  TestNames();
  TestBand();
  TestPhaseAndChanges();
  TestGenerated();
  std::printf("mcla_pursuit_probe_test: %d checks passed\n", checks);
  return 0;
}
