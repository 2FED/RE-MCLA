// KI-071 step 2: pure parts of the guest XMA consume ledger
// (src/mcla_xma_consume_probe.h) and the generated code its layout mirrors
// (MCLA_XMA_CONSUME_GENERATED_DIR).

#include "mcla_xma_consume_probe.h"

#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#ifndef MCLA_XMA_CONSUME_GENERATED_DIR
#error "MCLA_XMA_CONSUME_GENERATED_DIR (generated/default) is set by CMake"
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

namespace consume = mcla::xma_consume;

void TestLayout() {
  static_assert(consume::RecordAddress(0x40001000u, 0) == 0x40001000u);
  static_assert(consume::RecordAddress(0x40001000u, 2) == 0x40001000u + 192u);
  static_assert(consume::kStreamRecordSize == 3u * 32u, "3i << 5");
  Check(!consume::Stereo(0) && consume::Stereo(1u << 29) && !consume::Stereo(0x80000000u) &&
            !consume::Stereo(1u << 28) && consume::Stereo(0xFFFFFFFFu),
        "stereo is dword1 bit 29 only");
}

void TestFormat() {
  const consume::Entry entries[] = {{38, 44100, 88200, false}, {67, 44099, 88199, true}};
  Check(consume::FormatSecondLine(1000, 2, entries, 2, 0) ==
            "MCLA_XMA_CONSUME v=1 kind=second ms=1000 ids=2 ctxs=[38:44100,67:44099] "
            "run=[38:88200,67:88199] stereo=[67] more=0 bad=0",
        "second line format");
  Check(consume::FormatSecondLine(1003, 40, entries, 2, 3) ==
            "MCLA_XMA_CONSUME v=1 kind=second ms=1003 ids=40 ctxs=[38:44100,67:44099] "
            "run=[38:88200,67:88199] stereo=[67] more=38 bad=3",
        "second line with more ids than entries");
  Check(consume::FormatSecondLine(1000, 0, entries, 0, 1) ==
            "MCLA_XMA_CONSUME v=1 kind=second ms=1000 ids=0 ctxs=[] run=[] stereo=[] more=0 "
            "bad=1",
        "empty second line");
  Check(consume::FormatMinuteLine(60012, 7, 2646000, 12000, 0, 60) ==
            "MCLA_XMA_CONSUME v=1 kind=minute ms=60012 ids=7 samples=2646000 calls=12000 bad=0 "
            "lines=60",
        "minute line format");
}

void TestLedger() {
  {
    consume::Ledger ledger;
    ledger.Add(67, 100, false);
    Check(ledger.Collect(10'000).second.empty(), "the first call only starts the clock");
    ledger.Add(70, 200, false);
    ledger.Add(38, 300, false);
    ledger.Add(67, 50, false);
    Check(ledger.Collect(10'999).second.empty(), "nothing before one second");
    const consume::Lines a = ledger.Collect(11'000);
    Check(a.second ==
              "MCLA_XMA_CONSUME v=1 kind=second ms=1000 ids=3 ctxs=[38:300,67:150,70:200] "
              "run=[38:300,67:150,70:200] stereo=[] more=0 bad=0",
          "sorted by id, sums of the interval (the first Add included)");
    Check(a.minute.empty(), "no minute line yet");
    Check(ledger.Collect(11'500).second.empty(), "one line per second");
    // Second interval: 38 and 67 continue, 70 is idle (its run resets), 72
    // starts stereo, an id beyond the context array is bad.
    ledger.Add(38, 10, false);
    ledger.Add(67, 20, false);
    ledger.Add(72, 5, true);
    ledger.Add(320, 99, false);
    ledger.AddBad();
    const consume::Lines b = ledger.Collect(12'100);
    Check(b.second ==
              "MCLA_XMA_CONSUME v=1 kind=second ms=1100 ids=3 ctxs=[38:10,67:20,72:5] "
              "run=[38:310,67:170,72:5] stereo=[72] more=0 bad=2",
          "run totals continue, a quiet id is absent, bad ids counted");
    ledger.Add(70, 7, false);
    const consume::Lines c = ledger.Collect(13'200);
    Check(c.second.find(" ctxs=[70:7] run=[70:7] ") != std::string::npos,
          "an id idle for an interval restarts its run at 0");
    // Nothing consumed in an interval: no second line.
    Check(ledger.Collect(14'300).second.empty(), "no line for a silent interval");
    // Minute: 60 s after the first call.
    ledger.Add(38, 1, false);
    const consume::Lines m = ledger.Collect(70'000);
    Check(m.second.find("ctxs=[38:1]") != std::string::npos, "second line before the minute");
    Check(m.minute ==
              "MCLA_XMA_CONSUME v=1 kind=minute ms=60000 ids=4 samples=693 calls=11 bad=2 "
              "lines=4",
          "minute line: distinct ids, total samples, calls, bad, second lines");
    ledger.Add(38, 1, false);
    const consume::Lines n = ledger.Collect(71'000);
    Check(!n.second.empty() && n.minute.empty(), "the minute restarts");
  }
  {
    // More than 32 ids in one second: the first 32 by id, more= the rest.
    consume::Ledger ledger;
    ledger.Collect(1);
    for (uint32_t id = 0; id < 40; ++id) {
      ledger.Add(319 - id, id + 1, false);
    }
    const consume::Lines a = ledger.Collect(1'001);
    Check(a.second.find(" ids=40 ") != std::string::npos &&
              a.second.find(" more=8 ") != std::string::npos,
          "at most 32 entries, the rest counted");
    Check(a.second.find("ctxs=[280:40,281:39,") != std::string::npos &&
              a.second.find(",311:9] run=") != std::string::npos &&
              a.second.find("312:") == std::string::npos,
          "the lowest 32 ids, in id order");
    ledger.Add(319, 1, false);
    Check(ledger.Collect(2'001).second.find("ctxs=[319:1] run=[319:2]") != std::string::npos,
          "ids beyond the 32 shown still keep their run");
  }
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
  return ReadFile(std::string(MCLA_XMA_CONSUME_GENERATED_DIR) + "/" + file);
}

void TestGenerated() {
  const std::string g25 = Gen("mcla_recomp.25.cpp");
  const std::string g26 = Gen("mcla_recomp.26.cpp");
  Check(!g25.empty() && !g26.empty(), "generated files readable");
  static_assert(consume::kConsumeFunction == 0x823ED700u);
  const std::string consume_body = Body(g25, "sub_823ED700");
  Check(!consume_body.empty(), "sub_823ED700 in mcla_recomp.25.cpp");
  InOrder(consume_body,
          {"REX_FUNC_PROLOGUE(0x823ED700);", "// rlwinm r11,r4,1,0,30", "// add r10,r4,r11",
           "// lwz r11," + std::to_string(consume::kStreamsOffset) + "(r3)",
           "// rlwinm r31,r10,5,0,26", "// add r11,r31,r11",
           "// lwz r7," + std::to_string(consume::kRecordDword1) + "(r11)",
           "// rlwinm r10,r7,3,31,31", "// addi r6,r10,1", "// slw r10,r5,r6"},
          "consume: record = [r3+8] + 96i, stereo = dword1 bit 29, bytes = r5 << (stereo+1)");
  // r3 (xma), r4 (stream) and r5 (samples) are inputs: r4 is overwritten
  // only after it was used for the record offset, r5 is read by slw.
  Check(consume_body.find("// li r4,0") > consume_body.find("// add r10,r4,r11"),
        "r4 consumed before it is cleared");
  static_assert(consume::kStereoBit == 29, "rlwinm r10,r7,3,31,31 = (dword1 >> 29) & 1");
  InOrder(Body(g25, "sub_823EDCD8"),
          {"__imp__XMACreateContext(ctx, base);", "__imp__MmGetPhysicalAddress(ctx, base);",
           "// subf r11,r11,r3", "// srawi r10,r11,6", "// clrlwi r9,r10,16",
           "// sth r9," + std::to_string(consume::kRecordContextId) + "(r30)"},
          "create: record +80 = (phys - array base) >> 6");
  InOrder(Body(g26, "sub_823FC9F8"),
          {"// bl 0x823ff5f8", "sub_823FF5F8(ctx, base);", "// mr r5,r3", "// addi r6,r1,80",
           "// lwz r3,184(r31)", "// mr r4,r27", "// bl 0x823ed700", "sub_823ED700(ctx, base);"},
          "pump: consume(voice+184, stream, resampler result, out)");
  // The pump is the only direct caller (a grep of all generated files at the
  // M11 codegen finds no other `bl 0x823ed700`; this test reads 25 and 26).
  size_t calls = 0;
  for (const std::string *g : {&g25, &g26}) {
    for (size_t at = g->find("// bl 0x823ed700"); at != std::string::npos;
         at = g->find("// bl 0x823ed700", at + 1)) {
      ++calls;
    }
  }
  Check(calls == 1, "one direct call of sub_823ED700 in 25/26 (the pump)");
}

}  // namespace

int main() {
  TestLayout();
  TestFormat();
  TestLedger();
  TestGenerated();
  std::printf("mcla_xma_consume_probe_test: %d checks passed\n", checks);
  return 0;
}
