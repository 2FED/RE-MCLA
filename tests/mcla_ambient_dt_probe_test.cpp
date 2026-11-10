// KI-054 ambient-traffic frame delta: pure parts of the probe and the
// kick-time delta (src/mcla_ambient_dt_probe.h) and the generated code their
// hook sites, return addresses and registers mirror (MCLA_AMBDT_GENERATED_DIR).

#include "mcla_ambient_dt_probe.h"

#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#ifndef MCLA_AMBDT_GENERATED_DIR
#error "MCLA_AMBDT_GENERATED_DIR (generated/default) is set by CMake"
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

namespace amb = mcla::ambient_dt;

void TestDecisions() {
  static_assert(amb::kMainTimer + amb::kDeltaOffset == 0x827D7508u);
  static_assert(amb::kMainTimer + amb::kRateOffset == 0x827D750Cu);
  static_assert(amb::kMainTimer + amb::kUnscaledOffset == 0x827D7558u);
  static_assert(amb::kKickReturn == amb::kKickSite + 4 && amb::kJoinReturn == amb::kJoinSite + 4);
  Check(!amb::Differs(0.040f, 0.040f) && amb::Differs(0.040f, 0.020f) &&
            !amb::Differs(0.040f, 0.0400005f) && amb::Differs(0.0f, 0.001f),
        "Differs: 1e-6 tolerance");
  Check(amb::IsWorkerStep(amb::kWorkerReturn, true) &&
            !amb::IsWorkerStep(amb::kWorkerReturn, false) &&
            !amb::IsWorkerStep(amb::kInlineReturn, true),
        "only the worker-loop call inside an open window is the worker step");
  Check(amb::SiteAFromTimer(0) && amb::SiteAFromTimer(0x100) && !amb::SiteAFromTimer(1),
        "site A is the timer read when the gate byte is 0");
  Check(amb::SiteBFromTimer(0) && !amb::SiteBFromTimer(1), "site B on the r20 == 0 path");
  Check(amb::SiteCReads(0x40) && amb::SiteCReads(0xC1) && !amb::SiteCReads(0x80) &&
            !amb::SiteCReads(0),
        "site C reads with bit 0x40");
  Check(amb::RacerSplits(3, 0x80001000u) && !amb::RacerSplits(3, 0) &&
            !amb::RacerSplits(2, 0x80001000u),
        "racer splits in state 3 with a tune pointer");
  Check(amb::ShouldServe(true, true, amb::kSiteA, 0.040f, 0.020f) &&
            amb::ShouldServe(true, true, amb::kSiteB, 0.040f, 0.020f),
        "serve sites A and B on the worker when the value differs");
  Check(!amb::ShouldServe(false, true, amb::kSiteA, 0.040f, 0.020f) &&
            !amb::ShouldServe(true, false, amb::kSiteA, 0.040f, 0.020f) &&
            !amb::ShouldServe(true, true, amb::kSiteC, 0.040f, 0.020f) &&
            !amb::ShouldServe(true, true, amb::kSiteA, 0.040f, 0.040f),
        "never serve with the switch off, off the worker, at site C or when equal");
  // Pause: scale 0 -> +8 = 0 at the kick; an equal read is not replaced.
  Check(!amb::ShouldServe(true, true, amb::kSiteA, 0.0f, 0.0f), "pause passes through");
}

void TestStats() {
  amb::Stats s;
  s.AddRead(amb::kSiteA, 7, 0.040f, 0.040f);
  Check(s.reads[0] == 1 && s.split_reads[0] == 0 && s.last_count == 0 && !s.has_ratio,
        "an equal read counts, no mismatch");
  s.AddRead(amb::kSiteB, 8, 0.040f, 0.020f);
  Check(s.reads[1] == 1 && s.split_reads[1] == 1 && s.last_count == 1 &&
            s.last[0].window == 8 && s.last[0].site == amb::kSiteB,
        "a split read counts and is recorded");
  Check(s.has_ratio && s.min_ratio > 0.4999 && s.min_ratio < 0.5001 &&
            s.max_abs_delta > 0.0199 && s.max_abs_delta < 0.0201,
        "ratio 0.5, delta 20 ms");
  s.AddRead(amb::kSiteC, 9, 0.040f, 0.0133333f);
  s.AddRead(amb::kSiteA, 10, 0.040f, 0.020f);
  s.AddRead(amb::kSiteA, 11, 0.050f, 0.025f);
  Check(s.last_count == 3 && s.last[0].window == 11 && s.last[1].window == 10 &&
            s.last[2].window == 9,
        "the last three mismatches, most recent first");
  Check(s.min_ratio < 0.34 && s.split_reads[0] == 2 && s.split_reads[2] == 1, "N = 3 ratio");
  s.AddRead(amb::kSiteCount, 12, 0.040f, 0.0f);
  Check(s.reads[0] + s.reads[1] + s.reads[2] == 5, "an invalid site is ignored");

  amb::Stats a;
  a.windows = 300;
  a.overlaps = 12;
  a.racer_splits = 40;
  a.AddRead(amb::kSiteA, 1, 0.040f, 0.020f);
  amb::Stats b;
  b.windows = 300;
  b.served = {5, 6};
  b.outside = {1, 2, 3};
  b.AddRead(amb::kSiteB, 2, 0.040f, 0.010f);
  b.AddRead(amb::kSiteB, 3, 0.040f, 0.030f);
  a.Merge(b);
  Check(a.windows == 600 && a.overlaps == 12 && a.racer_splits == 40 && a.served[0] == 5 &&
            a.served[1] == 6 && a.outside[2] == 3 && a.split_reads[1] == 2,
        "merge adds the counters");
  Check(a.last_count == 3 && a.last[0].window == 3 && a.last[1].window == 2 &&
            a.last[2].window == 1 && a.min_ratio < 0.2501,
        "merge keeps the newest mismatches first and the smallest ratio");
}

void TestLines() {
  amb::Stats s;
  s.windows = 300;
  s.overlaps = 4;
  s.racer_splits = 9;
  s.worker_steps = 300;
  s.AddRead(amb::kSiteA, 41, 0.040f, 0.020f);
  s.AddRead(amb::kSiteB, 42, 0.040f, 0.040f);
  const std::string line = amb::Line(s, 300, 0.040f, true, false);
  Check(line.rfind("MCLA_AMBDT v=1 probe=1 snapshot=0 window=300 frames=300 windows=300 "
                   "overlaps=4 racer_splits=9 ",
                   0) == 0,
        "line head");
  Check(Has(line, " split_reads=1 reads=1/1/0 split=1/0/0 outside=0/0/0 served=0/0 ") &&
            Has(line, " max_abs_dt_delta=0.020000 min_ratio=0.5000 kick_dt=0.040000 ") &&
            Has(line, " last=[41:a:0.040000:0.020000]"),
        "line counters and the last mismatches");
  Check(line.find('\n') == std::string::npos, "one line");
  const std::string json = amb::StateJson(s, 300, 0.040f, true, true);
  Check(json.front() == '{' && json.back() == '}' && Has(json, "\"probe\": true") &&
            Has(json, "\"snapshot\": true") && Has(json, "\"split_reads\": [1, 0, 0]") &&
            Has(json, "\"windows\": 300"),
        "F10 JSON object");
  const amb::Stats empty;
  Check(Has(amb::Line(empty, 0, 0.0f, true, true), " min_ratio=1.0000 ") &&
            Has(amb::Line(empty, 0, 0.0f, true, true), " last=[]"),
        "empty interval");
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

// The text between `from` and `to` (both included in the search) of body.
std::string Between(const std::string &body, const std::string &from, const std::string &to) {
  const size_t a = body.find(from);
  if (a == std::string::npos) {
    return {};
  }
  const size_t b = body.find(to, a + from.size());
  return b == std::string::npos ? std::string() : body.substr(a, b - a);
}

// Instruction comment lines ("\t// ") in a span of generated code.
size_t Instructions(const std::string &span) {
  size_t n = 0;
  for (size_t at = span.find("\t// "); at != std::string::npos; at = span.find("\t// ", at + 1)) {
    ++n;
  }
  return n;
}

void TestGenerated() {
  const std::string dir = MCLA_AMBDT_GENERATED_DIR;
  const std::string g5 = ReadFile(dir + "/mcla_recomp.5.cpp");
  const std::string g12 = ReadFile(dir + "/mcla_recomp.12.cpp");
  const std::string g13 = ReadFile(dir + "/mcla_recomp.13.cpp");
  const std::string g14 = ReadFile(dir + "/mcla_recomp.14.cpp");
  const std::string g54 = ReadFile(dir + "/mcla_recomp.54.cpp");
  const std::string g55 = ReadFile(dir + "/mcla_recomp.55.cpp");
  Check(!g5.empty() && !g12.empty() && !g13.empty() && !g14.empty() && !g54.empty() &&
            !g55.empty(),
        "generated files readable");

  // Game loop update: full delta restored, kick, join.
  const std::string update = Body(g13, "sub_822C1FA8");
  InOrder(update,
          {"// stfs f28,8(r27)", "// bl 0x826d9578", "ctx.lr = 0x822C26BC;",
           "sub_826D9578(ctx, base);", "// bl 0x826d95b0", "ctx.lr = 0x822C2D08;",
           "sub_826D95B0(ctx, base);"},
          "the update restores +8, kicks the worker and joins it");
  // Kick signals two events, join waits on the done event.
  InOrder(Body(g54, "sub_826D9578"),
          {"// lwz r3,16(r31)", "// bl 0x821c9108", "// lwz r3,0(r31)", "// bl 0x821c9108"},
          "the kick signals the slot-1 and wake events");
  InOrder(Body(g54, "sub_826D95B0"), {"// lwz r3,20(r3)", "// b 0x821c90c0"},
          "the join waits on the slot-1 done event");
  // Worker loop: slot 1 is the ambient step; then the done event.
  InOrder(Body(g54, "sub_826D9448"),
          {"case 1:", "goto loc_826D94EC;", "loc_826D94EC:", "// bl 0x826d75d8",
           "ctx.lr = 0x826D9500;", "loc_826D9500:", "// lwz r3,4(r29)", "// bl 0x821c9108"},
          "the worker runs the ambient step and signals done");
  Check(Has(Body(g14, "sub_822D3C20"), "ctx.lr = 0x822D3D20;\n\tsub_826D75D8(ctx, base);") ||
            Has(Body(g14, "sub_822D3C20"), "ctx.lr = 0x822D3D20;\r\n\tsub_826D75D8(ctx, base);"),
        "the inline path calls the ambient step with another return address");

  // Site A: gate predicate, the load into f30, the next predicate call.
  const std::string step = Body(g54, "sub_826D75D8");
  const std::string site_a = Between(step, "ctx.lr = 0x826D7958;", "ctx.lr = 0x826D7994;");
  InOrder(site_a,
          {"ctx.lr = 0x826D7958;", "sub_82387A18(ctx, base);", "// bne cr6,0x826d7974",
           "// addi r30,r11,29952", "// lfs f30,8(r30)", "// b 0x826d7990", "loc_826D7974:",
           "// fsel f30,f12,f13,f0", "loc_826D7990:", "// bl 0x82387a18"},
          "site A: the gate, the timer load (or the network step) and the next call");
  // The load address: the gate returns to 0x826D7958; clrlwi, cmplwi, bne,
  // lis, addi come first (no label in between), so the lfs is at +0x14.
  static_assert(amb::kSiteALoad == amb::kSiteAGateReturn + 5 * 4);
  InOrder(site_a,
          {"ctx.lr = 0x826D7958;", "// clrlwi r11,r3,24", "// cmplwi cr6,r11,0",
           "// bne cr6,0x826d7974", "// lis r11,-32131", "// addi r30,r11,29952",
           "// lfs f30,8(r30)"},
          "site A load address (gate return + 0x14)");
  Check(Instructions(Between(site_a, "// clrlwi r11,r3,24", "// lfs f30,8(r30)")) == 4,
        "site A: exactly 4 instructions between the gate return and the load");
  Check(!Has(site_a, "ctx.f30.f64 = ctx.") || Has(site_a, "// fsel f30,f12,f13,f0"),
        "f30 written only by the two paths");
  Check(Has(step, "// lis r11,-32131") && Has(step, "// fadds f13,f0,f30"),
        "f30 is used after the call (ambient clock)");

  // Site B: the r20 path, the load into f1 and the suspension call.
  const std::string collider = Body(g55, "sub_826FA0F8");
  InOrder(collider,
          {"// addi r19,r11,29952", "// cmplwi cr6,r20,0", "// beq cr6,0x826fa300",
           "// fmr f1,f31", "loc_826FA300:", "// lfs f1,8(r19)", "loc_826FA304:",
           "// bl 0x826fba60", "ctx.lr = 0x826FA378;"},
          "site B: the r20 path, the timer load into f1 and the suspension call");
  const std::string b_span = Between(collider, "loc_826FA304:", "ctx.lr = 0x826FA378;");
  Check(!b_span.empty() && !Has(b_span, "ctx.f1.") && !Has(b_span, "ctx.lr = 0x826FA3"),
        "f1 is not written and nothing is called between the load and the call");
  const size_t load_b = collider.find("// lfs f1,8(r19)");
  Check(load_b != std::string::npos &&
            collider.find("ctx.r20.u64 =") < load_b &&
            collider.find("ctx.r20.u64 =", load_b) == std::string::npos &&
            collider.find("ctx.r20.s64 =") == std::string::npos,
        "r20 is set once, before the load (non-volatile across the loop's calls)");
  size_t callers = 0;
  for (const std::string *g : {&g5, &g12, &g13, &g14, &g54, &g55}) {
    for (size_t at = g->find("\tsub_826FBA60(ctx, base);"); at != std::string::npos;
         at = g->find("\tsub_826FBA60(ctx, base);", at + 1)) {
      ++callers;
    }
  }
  Check(callers == 1, "sub_826FBA60 has one direct caller in the checked files");

  // Site C: the flag test, then the load from +8 with no call before it.
  const std::string pose = Body(g55, "sub_826FA8A8");
  InOrder(pose,
          {"// lbz r11,140(r31)", "// rlwinm r10,r11,0,25,25", "// addi r9,r11,29952",
           "// lfs f0,8(r9)", "// fsubs f12,f13,f0", "// stfs f12,132(r31)"},
          "site C: flag bit 0x40, then obj+132 -= +8");
  {
    // The only call before the load is the register-save helper.
    const std::string head = Between(pose, "DEFINE_REX_FUNC", "// lfs f0,8(r9)");
    const size_t bl = head.find("// bl ");
    Check(bl != std::string::npos && head.compare(bl, 16, "// bl 0x823d91fc") == 0 &&
              head.find("// bl ", bl + 1) == std::string::npos && !Has(head, "bctrl"),
          "no guest call before the site C load (only __savegprlr)");
  }
  // The load address: the 15th instruction of the function.
  static_assert(amb::kSiteCLoad == amb::kPoseCountdown + 14 * 4);
  InOrder(pose,
          {"// mflr r12", "// bl 0x823d91fc", "// stfd f31,-40(r1)", "// stwu r1,-128(r1)",
           "// mr r31,r3", "// lbz r11,140(r31)", "// rlwinm r10,r11,0,25,25",
           "// cmplwi cr6,r10,0", "// beq cr6,0x826fa980", "// lis r11,-32131",
           "// lfs f13,132(r31)", "// lis r10,-32256", "// addi r9,r11,29952",
           "// lfs f31,3796(r10)", "// lfs f0,8(r9)"},
          "site C load address (entry + 0x38)");
  Check(Instructions(Between(pose, "// mflr r12", "// lfs f0,8(r9)")) == 13,
        "site C: exactly 13 instructions between the entry and the load");

  // Racer splitter: entry condition and the timer split.
  const std::string splitter = Body(g12, "sub_822A7E28");
  InOrder(splitter,
          {"// mr r29,r3", "// lwz r11,656(r29)", "// cmpwi cr6,r11,3", "// lwz r11,48(r29)",
           "// cmplwi cr6,r11,0", "// addi r30,r11,29952", "// li r4,1", "// li r4,0",
           "// mr r3,r30", "// bl 0x821bd910"},
          "racer splitter: state 3, tune, sub_821BD910 on the main timer");
  // sub_821BD910: split saves +80, sets +73/+76 and +8 = dt / N; restore.
  InOrder(Body(g5, "sub_821BD910"),
          {"// stfs f12,80(r3)", "// stb r10,73(r3)", "// stw r5,76(r3)",
           "// fdivs f8,f13,f9", "// stfs f8,8(r3)", "loc_821BD980:", "// lfs f13,80(r3)",
           "// stfs f13,8(r3)", "// stb r11,73(r3)"},
          "the timer split and restore");
  InOrder(update, {"// lis r11,-32131", "// addi r27,r11,29952", "// stfs f28,8(r27)"},
          "the update's r27 is the main timer 0x827D7500");
}

}  // namespace

int main() {
  TestDecisions();
  TestStats();
  TestLines();
  TestGenerated();
  std::printf("mcla_ambient_dt_probe_test: %d checks passed\n", checks);
  return 0;
}
