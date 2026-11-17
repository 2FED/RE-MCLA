// M11 l107 guest tiling and scene MSAA: pure parts of the always-on
// MCLA_GTILE line and the default-off scene sample-count control
// (src/mcla_guest_tiling.h), and the generated code their addresses,
// offsets, rules and values mirror (MCLA_GTILE_GENERATED_DIR).

#include "mcla_guest_tiling.h"

#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#ifndef MCLA_GTILE_GENERATED_DIR
#error "MCLA_GTILE_GENERATED_DIR (generated/default) is set by CMake"
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

namespace gt = mcla::guest_tiling;
using gt::Where;

void TestLayout() {
  static_assert(gt::kTileCount == 0x827D0000u + 17164u - 104u, "lis -32131 / 17164, -104");
  static_assert(gt::kTileRects == 0x82840000u - 28072u, "lis -32124 / -28072");
  static_assert(gt::kTileRects == 0x82840000u - 27776u - 296u, "EndTiled: r24 - 296");
  static_assert(gt::kMaxTileRects == 18 && gt::kTileRectBytes == 16);
  static_assert(gt::kRendererPtr == 0x82880000u - 8092u, "lis -32120 / -8092");
  static_assert(gt::kSdFlagOffset < gt::kSdPassOnOffset && gt::kSdPassOnOffset < gt::kSdPassModeOffset &&
                gt::kSdPassModeOffset < gt::kModeOffset && gt::kModeOffset < gt::kForce4Offset &&
                gt::kPendingValueOffset + 1 <= gt::kRendererBytes);
  Check(gt::FromReturn(0x822E6020u) == Where::kScene &&
            gt::FromReturn(0x822E543Cu) == Where::kSdPass &&
            gt::FromReturn(0x825F494Cu) == Where::kShadow &&
            gt::FromReturn(0x822E601Cu) == Where::kOther && gt::FromReturn(0) == Where::kOther,
        "callers by return address");
  Check(std::string(gt::WhereName(Where::kScene)) == "scene" &&
            std::string(gt::WhereName(Where::kSdPass)) == "sd_pass" &&
            std::string(gt::WhereName(Where::kShadow)) == "shadow" &&
            std::string(gt::WhereName(Where::kOther)) == "other",
        "caller names");
  Check(gt::Controlled(Where::kScene) && gt::Controlled(Where::kSdPass) &&
            !gt::Controlled(Where::kShadow) && !gt::Controlled(Where::kOther),
        "the control acts on the two renderer passes only");
}

void TestControl() {
  Check(gt::RequestedCount(0) == 0 && gt::RequestedCount(1) == 1 && gt::RequestedCount(2) == 2 &&
            gt::RequestedCount(3) == 0 && gt::RequestedCount(4) == 0 &&
            gt::RequestedCount(-1) == 0,
        "requested count: 1 or 2, else off");
  Check(gt::RequestValid(0) && gt::RequestValid(1) && gt::RequestValid(2) &&
            !gt::RequestValid(4) && !gt::RequestValid(-2),
        "cvar validity");
  Check(gt::RequestsCreationOne(1) && !gt::RequestsCreationOne(2) &&
            !gt::RequestsCreationOne(0) && !gt::RequestsCreationOne(3),
        "only 1x also makes the creation write");
  static_assert(gt::AppliedCount(4, 2, Where::kScene) == 2);
  static_assert(gt::AppliedCount(4, 1, Where::kScene) == 1);
  static_assert(gt::AppliedCount(4, 1, Where::kSdPass) == 1);
  static_assert(gt::AppliedCount(4, 2, Where::kShadow) == 4, "shadow untouched");
  static_assert(gt::AppliedCount(4, 2, Where::kOther) == 4, "unknown caller untouched");
  static_assert(gt::AppliedCount(2, 2, Where::kScene) == 2 &&
                gt::AppliedCount(1, 2, Where::kScene) == 1, "never raised");
  static_assert(gt::AppliedCount(4, 0, Where::kScene) == 4, "off: the game's count");
  static_assert(gt::AppliedCount(0, 2, Where::kScene) == 0 &&
                gt::AppliedCount(-1, 1, Where::kScene) == -1);
  for (int game : {0, 1, 2, 3, 4, 8}) {
    for (Where w : {Where::kScene, Where::kSdPass, Where::kShadow, Where::kOther}) {
      Check(gt::AppliedCount(game, 0, w) == game, "cvar off: never changes r6");
    }
  }
  static_assert(gt::BaseCount(Where::kScene, 2, 0) == 2 && gt::BaseCount(Where::kScene, 3, 0) == 4 &&
                gt::BaseCount(Where::kSdPass, 2, 0) == 0 && gt::BaseCount(Where::kSdPass, 2, 3) == 4 &&
                gt::BaseCount(Where::kShadow, 2, 0) == 4 && gt::BaseCount(Where::kOther, 2, 0) == -1);
  static_assert(gt::Forced4x(Where::kScene, 4, 2) && !gt::Forced4x(Where::kScene, 4, 4) &&
                !gt::Forced4x(Where::kScene, 2, 2) && gt::Forced4x(Where::kSdPass, 4, 0) &&
                !gt::Forced4x(Where::kShadow, 4, 4));
  static_assert(gt::EffectiveSdFlag(1, 0, 0, 0) == 1 && gt::EffectiveSdFlag(1, 1, 0, 0) == 0 &&
                gt::EffectiveSdFlag(0, 1, 1, 0) == 1 && gt::EffectiveSdFlag(1, 1, 0, 1) == 1,
                "sub_822E5B30: the pending value only on a thread whose TLS byte is 0");
}

void TestCollector() {
  // l109 collector_2x: only the collector, only with the control on, only
  // while the scene runs at 1x or 2x, never raised.
  static_assert(gt::CollectorCount(4, true, Where::kShadow, 2) == 2);
  static_assert(gt::CollectorCount(4, true, Where::kShadow, 1) == 2);
  static_assert(gt::CollectorCount(4, true, Where::kShadow, 4) == 4, "SD path: scene 4x");
  static_assert(gt::CollectorCount(4, true, Where::kShadow, -1) == 4, "scene not seen yet");
  static_assert(gt::CollectorCount(4, false, Where::kShadow, 2) == 4, "off");
  static_assert(gt::CollectorCount(4, true, Where::kScene, 2) == 4, "scene untouched");
  static_assert(gt::CollectorCount(4, true, Where::kSdPass, 2) == 4, "SD pass untouched");
  static_assert(gt::CollectorCount(4, true, Where::kOther, 2) == 4, "unknown caller untouched");
  static_assert(gt::CollectorCount(2, true, Where::kShadow, 2) == 2 &&
                    gt::CollectorCount(1, true, Where::kShadow, 2) == 1,
                "never raised");
  for (int game = -1; game <= 4; ++game) {
    for (int scene = -1; scene <= 4; ++scene) {
      for (int w = 0; w < static_cast<int>(Where::kCount); ++w) {
        Check(gt::CollectorCount(game, false, static_cast<Where>(w), scene) == game,
              "control off: never changes r6");
        const int c = gt::CollectorCount(game, true, static_cast<Where>(w), scene);
        Check(c == game || (c == 2 && game > 2 && static_cast<Where>(w) == Where::kShadow &&
                            (scene == 1 || scene == 2)),
              "control on: only the collector lowered to 2");
      }
    }
  }
  // The 960x540 collector surface at 2x is one tile under the game's rule.
  Check(gt::GameTileRule(960, 540, 2, false, 0).count == 1, "2x collector: one tile");
}

void TestRule() {
  static_assert(gt::EdramTile(1).width == 80 && gt::EdramTile(1).height == 16 &&
                gt::EdramTile(2).width == 80 && gt::EdramTile(2).height == 8 &&
                gt::EdramTile(4).width == 40 && gt::EdramTile(4).height == 8 &&
                gt::EdramTile(0).height == 16 && gt::EdramTile(3).width == 40);
  // The measured case: 960x540 at 4x is two tiles split at row 320 (the
  // MCLA_GPU_SURF window offsets 0 and -320, KI-083 at scaled row 640).
  constexpr gt::TileRule r4 = gt::GameTileRule(960, 540, 4, false, 0);
  static_assert(r4.span == 320 && r4.count == 2);
  constexpr gt::TileRule r2 = gt::GameTileRule(960, 540, 2, false, 0);
  static_assert(r2.span == 672 && r2.count == 1, "2x: one tile by the game's own rule");
  constexpr gt::TileRule r1 = gt::GameTileRule(960, 540, 1, false, 0);
  static_assert(r1.span == 1344 && r1.count == 1, "1x: one tile");
  // 720p (research note: 2x is 1280x512 + 1280x208, 4x three tiles).
  static_assert(gt::GameTileRule(1280, 720, 2, false, 0).span == 512 &&
                gt::GameTileRule(1280, 720, 2, false, 0).count == 2);
  static_assert(gt::GameTileRule(1280, 720, 4, false, 0).span == 256 &&
                gt::GameTileRule(1280, 720, 4, false, 0).count == 3);
  static_assert(gt::GameTileRule(1280, 720, 1, false, 0).count == 1);
  // Overlap and the column variant.
  static_assert(gt::GameTileRule(960, 540, 4, false, 16).span == 320 &&
                gt::GameTileRule(960, 540, 4, false, 16).count == 2);
  static_assert(gt::GameTileRule(1280, 720, 2, true, 0).span == 800 &&
                gt::GameTileRule(1280, 720, 2, true, 0).count == 2);
  static_assert(gt::GameTileRule(0, 540, 4, false, 0).count == 0 &&
                gt::GameTileRule(960, 0, 4, false, 0).count == 0);
  // 4x at 960x540 in one tile would need 24 x 68 colour + as many depth
  // EDRAM tiles: more than the 2048 the console (and our SDK) has.
  static_assert((960 / 40) * ((540 + 7) / 8) * 2 == 3264 && 3264 > 2048);
  const gt::Rect two[2] = {{0, 0, 960, 320}, {0, 320, 960, 540}};
  const gt::Cover c = gt::CoverOf(two, 2);
  Check(c.width == 960 && c.height == 540 && c.first_span == 320, "two tiles cover 960x540");
  const gt::Cover none = gt::CoverOf(two, -1);
  Check(none.width == 0 && none.height == 0 && none.first_span == 0, "count -1: nothing");
  const gt::Rect one[1] = {{0, 0, 960, 540}};
  Check(gt::CoverOf(one, 1).first_span == 540, "one tile");
}

void TestLinesAndState() {
  gt::Observation o;
  o.tiles = 2;
  o.msaa = 4;
  o.game_msaa = 4;
  o.width = 960;
  o.height = 540;
  o.forced4x = true;
  o.mode = 2;
  o.sd = 1;
  o.force4 = 0;
  o.first_span = 320;
  o.rule_tiles = 2;
  Check(gt::Line(o, 0, Where::kScene, 1) ==
            "MCLA_GTILE v=1 tiles=2 msaa=4 surface=960x540 forced4x=1 requested_msaa=0 "
            "applied=0 where=scene game_msaa=4 mode=2 sd=1 force4=0 tile_span=320 "
            "rule_tiles=2 rule_ok=1 calls=1",
        "the stock line (960x540 SD guest mode)");
  gt::Observation a = o;
  a.tiles = 1;
  a.msaa = 2;
  a.applied = true;
  a.first_span = 540;
  a.rule_tiles = 1;
  Check(gt::Line(a, 2, Where::kScene, 7) ==
            "MCLA_GTILE v=1 tiles=1 msaa=2 surface=960x540 forced4x=1 requested_msaa=2 "
            "applied=1 where=scene game_msaa=4 mode=2 sd=1 force4=0 tile_span=540 "
            "rule_tiles=1 rule_ok=1 calls=7",
        "the 2x line");
  gt::Observation bad = o;
  bad.rule_tiles = 3;
  Check(Has(gt::Line(bad, 0, Where::kSdPass, 2), "where=sd_pass") &&
            Has(gt::Line(bad, 0, Where::kSdPass, 2), "rule_ok=0"),
        "a rule mismatch is visible");

  gt::PerWhere w;
  Check(gt::Record(w, o, false), "first call writes a line");
  Check(!gt::Record(w, o, false) && w.calls == 2 && w.changes == 1, "same observation: no line");
  Check(gt::Record(w, a, true) && w.rewrites == 1 && w.changes == 2, "a change writes a line");
  gt::Record(w, bad, false);
  Check(w.rule_mismatches == 1, "rule mismatches counted");
  gt::PerWhere flip;
  for (int i = 0; i < 100; ++i) {
    gt::Record(flip, i % 2 ? o : a, false);
  }
  Check(flip.changes == gt::kMaxChangeLines && flip.suppressed == 100 - gt::kMaxChangeLines &&
            flip.calls == 100,
        "at most 32 change lines per caller, the rest counted");

  gt::State s;
  const std::string idle = gt::StateJson(s);
  Check(Has(idle, "{\"version\": 1, \"requested_msaa\": 0, \"cvar\": 0, \"cvar_valid\": true") &&
            Has(idle, "\"scene\": {\"calls\": 0") && Has(idle, "\"sd_pass\": {") &&
            Has(idle, "\"shadow\": {") && Has(idle, "\"other\": {") && idle.back() == '}',
        "F10 object before any call");
  s.cvar = 2;
  s.where[0] = w;
  const std::string on = gt::StateJson(s);
  Check(Has(on, "\"requested_msaa\": 2, \"cvar\": 2, \"cvar_valid\": true") &&
            Has(on, "\"scene\": {\"calls\": 4, \"changes\": 3, \"suppressed\": 0, "
                    "\"rewrites\": 1, \"rule_mismatches\": 1, \"tiles\": 2, \"msaa\": 4, "
                    "\"game_msaa\": 4, \"surface\": \"960x540\", \"forced4x\": true, "
                    "\"applied\": false, \"mode\": 2, \"sd\": 1, \"force4\": 0, "
                    "\"tile_span\": 320}"),
        "F10 object with a scene record");
  s.cvar = 4;
  Check(Has(gt::StateJson(s), "\"requested_msaa\": 0, \"cvar\": 4, \"cvar_valid\": false"),
        "an invalid cvar is reported and treated as off");
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

void TestGenerated() {
  const std::string dir = MCLA_GTILE_GENERATED_DIR;
  const std::string g2 = ReadFile(dir + "/mcla_recomp.2.cpp");
  const std::string g15 = ReadFile(dir + "/mcla_recomp.15.cpp");
  const std::string g16 = ReadFile(dir + "/mcla_recomp.16.cpp");
  const std::string g46 = ReadFile(dir + "/mcla_recomp.46.cpp");
  const std::string reg = ReadFile(dir + "/mcla_register.cpp");
  Check(!g2.empty() && !g15.empty() && !g16.empty() && !g46.empty() && !reg.empty(),
        "generated files readable");

  // BeginTiledRendering: registers, the EDRAM tile size, the row rule, the
  // count store, the rectangles and the D3D tiling begin.
  const std::string begin = Body(g2, "sub_8217A470");
  InOrder(begin,
          {"REX_FUNC_PROLOGUE(0x8217A470);", "// mr r11,r5", "// mr r17,r3", "// mr r18,r6",
           "// mr r31,r7", "// mr r27,r8", "// addi r14,r10,17200", "// lis r11,-32124",
           "// addi r20,r11,-28072", "// addi r16,r11,17164", "// stw r31,296(r20)",
           "// lwz r10,24(r11)", "// lwz r8,28(r9)", "// lwz r19,60(r17)"},
          "BeginTiled: surface r3, count r6, flags r7, overlap r8, size via vtable +24 / +28");
  InOrder(begin,
          {"// cmpwi cr6,r18,1", "// bgt cr6,0x8217a5c8", "// li r10,80", "loc_8217A5C8:",
           "// cmpwi cr6,r18,2", "// li r10,80", "loc_8217A5D8:", "// li r10,40",
           "loc_8217A5DC:", "// li r11,16", "loc_8217A5E8:", "// li r11,8", "loc_8217A5EC:",
           "// clrlwi r9,r31,31", "// beq cr6,0x8217a680"},
          "BeginTiled: EDRAM tile 80x16 / 80x8 / 40x8 pixels; flags bit 0 picks columns");
  InOrder(begin,
          {"loc_8217A680:", "// addi r8,r28,159", "// li r9,160", "// rlwinm r5,r11,10,0,21",
           "// divwu r11,r8,r9", "// rlwinm r4,r27,1,0,30", "// rlwinm r3,r3,5,0,26",
           "// divw r31,r3,r10", "// divw r8,r5,r31", "// rlwinm r8,r8,0,0,26",
           "// subf r11,r4,r8", "// subf r5,r27,r8", "// add r11,r11,r25", "// addi r11,r11,-1",
           "// divw r11,r11,r5", "// stw r11,-104(r16)", "loc_8217A700:"},
          "BeginTiled: row rule (1024 EDRAM tiles, rows rounded down to 32) and the count");
  InOrder(begin,
          {"// addi r9,r25,31", "// rlwinm r5,r10,10,0,21", "// rlwinm r4,r9,0,0,26",
           "// divw r3,r4,r11", "// divw r10,r5,r3", "// divwu r10,r10,r9",
           "// stw r11,-104(r16)", "// b 0x8217a700"},
          "BeginTiled: the column rule");
  InOrder(begin,
          {"// addi r29,r20,384", "// addi r11,r16,-136", "// cmpw cr6,r10,r18",
           "// cmpwi cr6,r31,32", "// bl 0x821bd618", "loc_8217A87C:", "// lwz r8,-104(r16)",
           "// stw r17,448(r20)", "// subf r6,r27,r26", "// stw r28,4(r11)",
           "// stw r15,-4(r11)", "// mullw r4,r5,r6", "// stw r4,0(r11)", "// stw r9,8(r11)",
           "// addi r11,r11,16", "// lwz r5,-104(r16)", "// lwz r3,-28076(r29)",
           "// bl 0x8241be78"},
          "BeginTiled: an 8-slot surface cache keyed by count / size / format, abutting "
          "16-byte rectangles, then the D3D tiling begin with the count");
  // EndTiledRendering: per tile, predication and two resolves of the tile
  // rectangle; the count is reset to -1.
  InOrder(Body(g2, "sub_8217B430"),
          {"REX_FUNC_PROLOGUE(0x8217B430);", "// addi r22,r11,17164", "// lwz r11,-104(r22)",
           "// addi r24,r10,-27776", "// addi r30,r24,-296", "loc_8217B48C:",
           "// slw r4,r23,r27", "// bl 0x8241bd08", "// mr r5,r30", "// bl 0x82420ba8",
           "// mr r5,r30", "// bl 0x82420ba8", "// addi r30,r30,16", "// blt cr6,0x8217b48c",
           "// bl 0x8241c308", "// li r11,-1", "// stw r11,-104(r22)"},
          "EndTiled: each tile rectangle resolved in place (colour and depth), count -1");

  // The three callers.
  size_t calls = 0;
  for (int unit = 0; unit < 65; ++unit) {
    const std::string g = ReadFile(dir + "/mcla_recomp." + std::to_string(unit) + ".cpp");
    calls += Count(g, "sub_8217A470(ctx, base);");
  }
  Check(calls == 3 && Count(g15, "sub_8217A470(ctx, base);") == 2 &&
            Count(g46, "sub_8217A470(ctx, base);") == 1,
        "BeginTiled has three direct callers (two renderer passes, the collector)");
  InOrder(Body(g15, "sub_822E5ED8"),
          {"REX_FUNC_PROLOGUE(0x822E5ED8);", "// mr r31,r3", "// lwz r8,384(r31)",
           "// lwz r28,404(r31)", "// addi r9,r11,99", "// lwzx r29,r7,r31",
           "// ble cr6,0x822e604c", "// mr r6,r8", "// cmpwi cr6,r8,3", "// li r6,4",
           "loc_822E5F88:", "// lbz r10,393(r31)", "// li r6,4", "loc_822E5F98:",
           "// lbz r10,432(r31)", "// li r9,72", "// lbzx r7,r9,r10", "// lbz r10,433(r31)",
           "loc_822E5FC0:", "// lbz r10,373(r31)", "loc_822E5FC4:", "// li r6,4",
           "loc_822E6004:", "// li r11,12", "loc_822E6008:", "// li r8,0", "// mr r7,r11",
           "// addi r5,r1,80", "// mr r3,r29", "// bl 0x8217a470", "ctx.lr = 0x822E6020;"},
          "scene begin: +384 (3 -> 4), forced to 4 by +393 or the SD flag, overlap 0, "
          "the target +396 / +400 (return 0x822E6020)");
  InOrder(Body(g15, "sub_822E5358"),
          {"REX_FUNC_PROLOGUE(0x822E5358);", "// mr r28,r3", "// lwz r27,380(r28)",
           "// cmpwi cr6,r27,3", "// lbz r11,393(r28)", "// lbz r11,373(r28)",
           "loc_822E5390:", "// li r27,4", "loc_822E5394:", "// lbz r11,375(r28)",
           "// beq cr6,0x822e5598", "// mr r6,r27", "// li r4,0", "// bl 0x8217a470",
           "ctx.lr = 0x822E543C;"},
          "SD pass: +380, forced to 4 by +393 or +373, only with +375 (return 0x822E543C)");
  InOrder(Body(g46, "sub_825F48E8"),
          {"REX_FUNC_PROLOGUE(0x825F48E8);", "// lbz r11,56(r31)", "// lwz r4,64(r31)",
           "// li r6,4", "// lwz r3,60(r31)", "// bl 0x8217a470", "ctx.lr = 0x825F494C;"},
          "shadow / depth collector: always 4 (return 0x825F494C)");

  // The SD flag +373: the constructor stores 1 unless XGetVideoMode reports
  // HD (or the parameter at 0x8287E06C is set: also 1).
  InOrder(Body(g15, "sub_822E47E0"),
          {"// stb r30,393(r31)", "// addi r10,r11,-8084", "// lwz r11,4(r10)",
           "// bne cr6,0x822e4a3c", "// bl 0x82178880", "// clrlwi r11,r3,24",
           "// mr r11,r30", "// bne cr6,0x822e4a40", "loc_822E4A3C:", "// mr r11,r28",
           "loc_822E4A40:", "// stb r30,432(r31)", "// stb r30,433(r31)",
           "// stb r30,375(r31)", "// stb r11,373(r31)"},
          "constructor: +373 = not HD (or the parameter), +393 / +432 / +433 / +375 = 0");
  Check(Count(g15, "// stb r11,373(r31)") == 2, "+373 is stored in unit 15 only by the "
        "constructor and the deferred request");
  InOrder(Body(g15, "sub_822E5B20"), {"// li r11,1", "// stb r4,433(r3)", "// stb r11,432(r3)"},
          "the deferred request: +433 = value, +432 = 1");
  InOrder(Body(g15, "sub_822E5B60"),
          {"// lbz r7,432(r31)", "// beq cr6,0x822e5bb4", "// lbz r11,433(r31)",
           "// stb r30,432(r31)", "// stb r11,373(r31)"},
          "each frame the pending value becomes +373");
  InOrder(Body(g15, "sub_822E5B30"),
          {"// lbz r11,432(r3)", "// li r10,72", "// lbz r3,433(r3)", "loc_822E5B58:",
           "// lbz r3,373(r3)"},
          "the getter (sub_822E5B30): pending value or +373");
  InOrder(Body(g15, "sub_822E6408"),
          {"// lwz r17,364(r27)", "// lis r14,10240", "// lwz r10,380(r27)",
           "// lbz r11,373(r27)", "loc_822E6648:", "// cmpw cr6,r17,r14", "// stb r9,375(r27)"},
          "+375 (run the SD pass) = (+380 or +373) and +364 != 0x28000000");
  InOrder(Body(g16, "sub_82304648"),
          {"// lwz r11,-8092(r11)", "// stb r3,393(r11)"}, "+393 setter");
  // The SD flag also selects other values (not touched by the control).
  InOrder(Body(g15, "sub_822E60E0"),
          {"// lbz r11,373(r29)", "// li r4,16", "// li r3,9", "// bl 0x82189e20"},
          "+373 also picks state 9 = 16 (why the control does not clear it)");
  Check(Has(reg, "SetFunction(0x8217A470, sub_8217A470)"),
        "BeginTiled is registered by symbol (the override replaces it)");
}

}  // namespace

int main() {
  TestLayout();
  TestControl();
  TestCollector();
  TestRule();
  TestLinesAndState();
  TestGenerated();
  std::printf("mcla_guest_tiling_test: %d checks passed\n", checks);
  return 0;
}
