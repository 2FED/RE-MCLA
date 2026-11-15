// M11 l106 guest MSAA off: pure parts of the default-off control and the
// always-on MCLA_GMSAA line (src/mcla_guest_msaa.h), and the generated code
// their addresses, offsets and mode values mirror (MCLA_GMSAA_GENERATED_DIR).

#include "mcla_guest_msaa.h"

#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#ifndef MCLA_GMSAA_GENERATED_DIR
#error "MCLA_GMSAA_GENERATED_DIR (generated/default) is set by CMake"
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

bool Has(const std::string &text, const std::string &part) {
  return text.find(part) != std::string::npos;
}

namespace gm = mcla::guest_msaa;

void TestLayout() {
  static_assert(gm::kRendererPtr == 0x82880000u - 8092u, "lis -32120 / -8092");
  static_assert(gm::kParamNoMsaaA == 0x82880000u - 7944u, "lis -32120 / -7944");
  static_assert(gm::kParamNoMsaaB == 0x82880000u - 7924u, "lis -32120 / -7924");
  static_assert(gm::kModeOffset == 384 && gm::kWidthOffset == 416 && gm::kHeightOffset == 420);
  static_assert(gm::kHeightOffset + 4 <= gm::kRendererBytes && gm::kRendererBytes == 464);
  static_assert(gm::kFirstParamLoad == 0x822E4B78u + 8u, "lis, addi, then the load");
  Check(gm::PointerPlausible(0x40001000u) && gm::PointerPlausible(0xBC000000u) &&
            !gm::PointerPlausible(0) && !gm::PointerPlausible(0x100u) &&
            !gm::PointerPlausible(0x40001002u) && !gm::PointerPlausible(0xFFFFFF00u),
        "renderer pointer: non-null, aligned, room for the 464-byte object");
}

void TestModes() {
  Check(gm::KnownMode(1) && gm::KnownMode(2) && gm::KnownMode(4) && !gm::KnownMode(0) &&
            !gm::KnownMode(3) && !gm::KnownMode(8) && !gm::KnownMode(-1),
        "the game stores 1, 2 or 4");
  Check(std::string(gm::ModeName(1)) == "1x" && std::string(gm::ModeName(2)) == "2x" &&
            std::string(gm::ModeName(4)) == "4x" && std::string(gm::ModeName(-1)) == "unknown" &&
            std::string(gm::ModeName(3)) == "other" && std::string(gm::ModeName(0)) == "other",
        "mode names");
  static_assert(gm::TiledMode(1) == 1 && gm::TiledMode(2) == 2 && gm::TiledMode(3) == 4 &&
                gm::TiledMode(4) == 4);
}

void TestDecide() {
  using gm::Outcome;
  // Off: nothing, whatever the renderer looks like.
  for (const int64_t mode : {int64_t{-1}, int64_t{0}, int64_t{1}, int64_t{2}, int64_t{4}}) {
    for (const bool plausible : {false, true}) {
      Check(gm::Decide(false, plausible, mode) == Outcome::kNotRequested &&
                !gm::Writes(gm::Decide(false, plausible, mode)),
            "cvar off: never writes");
    }
  }
  Check(gm::Decide(true, true, 2) == Outcome::kApplied && gm::Writes(Outcome::kApplied),
        "HD start (2x): writes 1");
  Check(gm::Decide(true, true, 4) == Outcome::kApplied, "SD start (4x): writes 1");
  Check(gm::Decide(true, true, 1) == Outcome::kAlreadyOne && !gm::Writes(Outcome::kAlreadyOne),
        "already 1 (the game's own parameter): nothing written");
  Check(gm::Decide(true, false, 2) == Outcome::kNoRenderer && !gm::Writes(Outcome::kNoRenderer),
        "no plausible renderer: nothing written");
  for (const int64_t mode : {int64_t{0}, int64_t{3}, int64_t{8}, int64_t{-1}, int64_t{0x7FFFFFFF}}) {
    Check(gm::Decide(true, true, mode) == Outcome::kUnknownMode && !gm::Writes(gm::Decide(true, true, mode)),
          "a value the game never stores: nothing written");
  }
  static_assert(gm::ModeAfter(Outcome::kApplied, 2) == 1 && gm::ModeAfter(Outcome::kApplied, 4) == 1 &&
                gm::ModeAfter(Outcome::kAlreadyOne, 1) == 1 &&
                gm::ModeAfter(Outcome::kNotRequested, 2) == 2 &&
                gm::ModeAfter(Outcome::kUnknownMode, 3) == 3);
  Check(std::string(gm::OutcomeName(Outcome::kNotRequested)) == "not_requested" &&
            std::string(gm::OutcomeName(Outcome::kApplied)) == "applied" &&
            std::string(gm::OutcomeName(Outcome::kAlreadyOne)) == "already_1x" &&
            std::string(gm::OutcomeName(Outcome::kNoRenderer)) == "no_renderer" &&
            std::string(gm::OutcomeName(Outcome::kUnknownMode)) == "unknown_mode",
        "outcome names");
}

void TestLines() {
  Check(gm::Line(true, true, 2, 1, gm::kWhereRendererCreate) ==
            "MCLA_GMSAA v=1 requested=1 applied=1 mode_before=2 mode_after=1 where=renderer_create",
        "the applied line");
  Check(gm::Line(false, false, 2, 2, gm::kWhereRendererCreate) ==
            "MCLA_GMSAA v=1 requested=0 applied=0 mode_before=2 mode_after=2 where=renderer_create",
        "the default line (always on, nothing written)");
  Check(gm::Line(true, true, 1, 1, gm::kWhereTargetSetup) ==
            "MCLA_GMSAA v=1 requested=1 applied=1 mode_before=1 mode_after=1 where=rt_create",
        "the render-target set-up line");
  Check(gm::Line(true, false, -1, -1, gm::kWhereRendererCreate) ==
            "MCLA_GMSAA v=1 requested=1 applied=0 mode_before=-1 mode_after=-1 where=renderer_create",
        "no renderer");
  gm::State s;
  const std::string idle = gm::StateJson(s);
  Check(Has(idle, "\"version\": 1") && Has(idle, "\"requested\": false") &&
            Has(idle, "\"created\": false") && Has(idle, "\"outcome\": \"not_requested\"") &&
            Has(idle, "\"applied\": false") && Has(idle, "\"mode_now\": -1") &&
            Has(idle, "\"mode_now_name\": \"unknown\"") && idle.front() == '{' && idle.back() == '}',
        "F10 object before the renderer exists");
  s.requested = true;
  s.created = true;
  s.outcome = gm::Outcome::kApplied;
  s.renderer = 0x40A01230u;
  s.mode_before = 2;
  s.mode_after = 1;
  s.width = 1280;
  s.height = 720;
  s.target_setups = 1;
  s.target_setup_mode = 1;
  s.mode_now = 1;
  const std::string on = gm::StateJson(s);
  Check(on == "{\"version\": 1, \"requested\": true, \"created\": true, \"outcome\": \"applied\", "
              "\"applied\": true, \"renderer\": \"0x40A01230\", \"mode_before\": 2, "
              "\"mode_after\": 1, \"size\": \"1280x720\", \"rt_create_calls\": 1, "
              "\"rt_create_mode\": 1, \"mode_now\": 1, \"mode_now_name\": \"1x\"}",
        "F10 object with the arm applied");
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

size_t Count(const std::string &text, const std::string &part) {
  size_t n = 0;
  for (size_t at = text.find(part); at != std::string::npos; at = text.find(part, at + 1)) {
    ++n;
  }
  return n;
}

// Stores of a word to +384 through a register other than the stack pointer.
size_t ModeStores(const std::string &text) {
  size_t n = 0;
  std::istringstream lines(text);
  std::string line;
  while (std::getline(lines, line)) {
    if (Has(line, "// stw r") && Has(line, ",384(r") && !Has(line, ",384(r1)")) {
      ++n;
    }
  }
  return n;
}

void TestGenerated() {
  const std::string dir = MCLA_GMSAA_GENERATED_DIR;
  const std::string g15 = ReadFile(dir + "/mcla_recomp.15.cpp");
  const std::string g16 = ReadFile(dir + "/mcla_recomp.16.cpp");
  const std::string g61 = ReadFile(dir + "/mcla_recomp.61.cpp");
  const std::string g2 = ReadFile(dir + "/mcla_recomp.2.cpp");
  const std::string reg = ReadFile(dir + "/mcla_register.cpp");
  Check(!g15.empty() && !g16.empty() && !g61.empty() && !g2.empty() && !reg.empty(),
        "generated files readable");

  // The constructor: mode 2, the two size parameters replace it with 4.
  const std::string ctor = Body(g15, "sub_822E47E0");
  InOrder(ctor,
          {"REX_FUNC_PROLOGUE(0x822E47E0);", "// mr r31,r3", "// li r11,2", "// stw r11,384(r31)",
           "// li r8,4", "// addi r4,r5,-8004", "// lwz r11,4(r4)", "// li r11,640",
           "// stw r8,384(r31)", "// li r7,480", "loc_822E48E4:", "// addi r7,r11,-8044",
           "// lwz r11,4(r7)", "// stw r8,384(r31)", "// li r11,960", "// li r8,720"},
          "constructor: +384 = 2, the size parameters write 4");
  InOrder(ctor, {"// lwz r11,17012(r10)", "// stw r11,416(r31)", "// lwz r11,17016(r9)",
                 "// stw r11,420(r31)"},
          "constructor: +416 / +420 = the display size");

  // The creation: allocate 464, construct, publish, SD 4x, the parameters 1.
  const std::string create = Body(g15, "sub_822E4AB0");
  InOrder(create,
          {"REX_FUNC_PROLOGUE(0x822E4AB0);", "// li r3,464", "// bl 0x82130528",
           "// bl 0x822e47e0", "// lis r11,-32120", "// addi r31,r11,-8092",
           "// stw r3,-8092(r11)", "// bl 0x82178880", "// clrlwi r11,r3,24",
           "// bne cr6,0x822e4b78", "// lwz r11,17012(r29)", "// cmpwi cr6,r11,720",
           "// bgt cr6,0x822e4b78", "// lwz r11,17016(r11)", "// cmpwi cr6,r11,576",
           "// bgt cr6,0x822e4b78", "// lwz r11,0(r31)", "// li r10,4", "// stw r10,384(r11)",
           "loc_822E4B78:", "// lis r11,-32120", "// addi r10,r11,-7944", "// lwz r11,4(r10)",
           "// bne cr6,0x822e4ba0", "// addi r10,r11,-7924", "// lwz r11,4(r10)",
           "// beq cr6,0x822e4bac", "loc_822E4BA0:", "// lwz r11,0(r31)", "// li r10,1",
           "// stw r10,384(r11)", "loc_822E4BAC:"},
          "creation: SD output 4x, either parameter 1x");
  const size_t tail = create.find("loc_822E4BAC:");
  Check(tail != std::string::npos && !Has(create.substr(tail), "384(") &&
            Count(create.substr(tail), "return;") == 2,
        "creation: nothing after loc_822E4BAC reads +384 (two returns)");
  Check(Count(create, "lwz r11,4(r10)") == 2, "creation: the two parameter tests");
  // Every +384 store in the renderer's unit is in the constructor (3) and the
  // creation (2).
  Check(ModeStores(ctor) == 3 && ModeStores(create) == 2 && ModeStores(g15) == 5,
        "the renderer's unit stores +384 only in the constructor and the creation");
  // fIsHiDef: XGetVideoMode, byte +16 of the 80(r1) structure.
  InOrder(Body(g2, "sub_82178880"), {"// addi r3,r1,80", "// bl 0x827bcd84", "// lwz r11,96(r1)",
                                     "// xori r3,r9,1"},
          "sub_82178880 returns the HD flag of XGetVideoMode");
  Check(Has(reg, "SetFunction(0x827BCD84, __imp__XGetVideoMode)"), "0x827BCD84 is XGetVideoMode");

  // The one caller of the creation and its return address.
  InOrder(Body(g16, "sub_823041E8"),
          {"REX_FUNC_PROLOGUE(0x823041E8);", "// bl 0x822e7098", "// bl 0x822e4ab0",
           "ctx.lr = 0x823042BC;", "sub_822E4AB0(ctx, base);", "// bl 0x826123e0"},
          "graphics start-up calls the creation (return 0x823042BC)");
  size_t create_calls = 0;
  for (int unit = 0; unit < 65; ++unit) {
    const std::string g = ReadFile(dir + "/mcla_recomp." + std::to_string(unit) + ".cpp");
    create_calls += Count(g, "sub_822E4AB0(ctx, base);");
  }
  Check(create_calls == 1, "the creation has one direct caller");

  // The parameters are registered with 0 by static initialisers.
  InOrder(Body(g61, "sub_827ACBB0"), {"// lis r10,-32120", "// addi r3,r10,-7944", "// li r6,0",
                                      "// li r4,0", "// b 0x821c06c8"},
          "parameter A registration");
  InOrder(Body(g61, "sub_827ACBD0"), {"// lis r10,-32120", "// addi r3,r10,-7924", "// li r6,0",
                                      "// li r4,0", "// b 0x821c06c8"},
          "parameter B registration");

  // The render-target set-up reads the mode into its descriptor and is
  // called once, with r3 = the renderer.
  const std::string setup = Body(g15, "sub_822E4CC8");
  InOrder(setup,
          {"REX_FUNC_PROLOGUE(0x822E4CC8);", "// mr r31,r3", "// lwz r9,384(r31)",
           "// stw r9,100(r1)", "// lwz r6,416(r31)", "// addi r9,r1,96", "// lwz r10,56(r11)",
           "// stw r3,396(r31)", "// lwz r10,56(r11)", "// stw r3,400(r31)",
           "// stw r28,100(r1)", "// lwz r6,17012(r6)", "// lwz r10,56(r11)",
           "// stw r3,412(r31)"},
          "render-target set-up: +384 into the descriptor, +396 / +400 with it, +412 with 0");
  InOrder(Body(g16, "sub_82305230"),
          {"REX_FUNC_PROLOGUE(0x82305230);", "// lis r31,-32120", "// lwz r3,-8092(r31)",
           "// bl 0x822e4cc8", "ctx.lr = 0x82305250;", "sub_822E4CC8(ctx, base);"},
          "the set-up is called with the renderer (return 0x82305250)");
  size_t setup_calls = 0;
  for (int unit = 0; unit < 65; ++unit) {
    const std::string g = ReadFile(dir + "/mcla_recomp." + std::to_string(unit) + ".cpp");
    setup_calls += Count(g, "sub_822E4CC8(ctx, base);");
  }
  Check(setup_calls == 1, "the render-target set-up has one direct caller");

  // Per frame: the scene begin hands the mode to BeginTiledRendering.
  InOrder(Body(g15, "sub_822E5ED8"),
          {"REX_FUNC_PROLOGUE(0x822E5ED8);", "// mr r31,r3", "// lwz r8,384(r31)",
           "// ble cr6,0x822e604c", "// mr r6,r8", "// cmpwi cr6,r8,3", "// li r6,4",
           "// bl 0x8217a470"},
          "scene begin: +384 is the tiled-rendering sample count (3 -> 4)");
  Check(Has(g2, "DEFINE_REX_FUNC(sub_8217A470) {") &&
            Has(Body(g2, "sub_8217A470"), "// mr r18,r6"),
        "BeginTiledRendering keeps the sample count in r18");
  // Start-up: the depth / shadow-collector sizes take the mode too.
  InOrder(Body(g16, "sub_82310E40"),
          {"// lwz r11,-8092(r31)", "// lwz r10,420(r11)", "// lwz r9,416(r11)",
           "// lwz r6,384(r11)", "// bl 0x825f4750"},
          "start-up targets: the mode and the size into sub_825F4750");
  Check(Has(reg, "SetFunction(0x822E4AB0, sub_822E4AB0)") &&
            Has(reg, "SetFunction(0x822E4CC8, sub_822E4CC8)"),
        "both functions are registered by symbol (the overrides replace them)");
}

}  // namespace

int main() {
  TestLayout();
  TestModes();
  TestDecide();
  TestLines();
  TestGenerated();
  std::printf("mcla_guest_msaa_test: %d checks passed\n", checks);
  return 0;
}
