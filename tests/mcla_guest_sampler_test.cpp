#include "mcla_guest_sampler.h"

#include <cstdio>
#include <cstdlib>
#include <string>

namespace {
int checks = 0;
void Check(bool value, const char *message) {
  if (!value) {
    std::fprintf(stderr, "FAIL: %s\n", message);
    std::exit(1);
  }
  ++checks;
}

using mcla::guest_sampler::FormatImports;
using mcla::guest_sampler::FormatLine;
using mcla::guest_sampler::FormatTopFunctions;
using mcla::guest_sampler::Histogram;
using mcla::guest_sampler::kTopFunctions;
using mcla::guest_sampler::SanitizeName;

void TestEmptyHistogramHasNoLine() {
  Histogram h;
  h.name = "Main XThread";
  Check(FormatLine(h).empty(), "no samples, no line");
}

void TestTopFunctionsOrderAndLimit() {
  Histogram h;
  // 20 distinct functions with counts 1..20; 0x82000000+i has count i+1.
  for (uint32_t i = 0; i < 20; ++i) {
    for (uint32_t k = 0; k <= i; ++k) {
      h.Add(0x82000000u + i, nullptr);
    }
  }
  const auto text = FormatTopFunctions(h.functions, kTopFunctions);
  Check(text.starts_with(" 82000013:20 82000012:19"), "most frequent first");
  Check(text.ends_with(" 82000008:9"), "twelfth entry is the last one");
  size_t entries = 0;
  for (const char c : text) entries += c == ':' ? 1 : 0;
  Check(entries == kTopFunctions, "top list is limited to 12");
}

void TestTiesAreDeterministic() {
  Histogram h;
  h.Add(0x82300000u, nullptr);
  h.Add(0x82100000u, nullptr);
  h.Add(0x82200000u, nullptr);
  Check(FormatTopFunctions(h.functions, 12) == " 82100000:1 82200000:1 82300000:1",
        "ties sort by ascending address");
}

void TestImportsMergeAndSort() {
  // Two different pointers to equal text are merged.
  static const char kWaitA[] = "KeWaitForSingleObject";
  static const char kWaitB[] = "KeWaitForSingleObject";
  static const char kRead[] = "NtReadFile";
  Histogram h;
  h.Add(1, kWaitA);
  h.Add(1, kWaitB);
  h.Add(1, kWaitA);
  h.Add(1, kRead);
  h.Add(1, nullptr);
  h.Add(1, nullptr);
  Check(FormatImports(h.imports) == " KeWaitForSingleObject:3 none:2 NtReadFile:1",
        "imports merged by name, most frequent first");
}

void TestLineFormat() {
  Histogram h;
  h.name = "Main XThread";
  for (int i = 0; i < 3; ++i) h.Add(0x822C1FA8u, "KeWaitForSingleObject");
  h.Add(0x821C90C0u, nullptr);
  Check(FormatLine(h) ==
            "MCLA_GUEST_SAMPLE v=1 thread='Main XThread' samples=4 "
            "fn= 822C1FA8:3 821C90C0:1 last_import= KeWaitForSingleObject:3 none:1",
        "line matches the v=1 format");
}

void TestNameSanitizing() {
  Check(SanitizeName("Render 'A'\n") == "Render ?A??", "quotes and control characters replaced");
  Check(SanitizeName(std::string(100, 'x')).size() == 64, "names are bounded");
}
}  // namespace

int main() {
  TestEmptyHistogramHasNoLine();
  TestTopFunctionsOrderAndLimit();
  TestTiesAreDeterministic();
  TestImportsMergeAndSort();
  TestLineFormat();
  TestNameSanitizing();
  std::printf("mcla_guest_sampler_test: %d checks passed\n", checks);
  return 0;
}
