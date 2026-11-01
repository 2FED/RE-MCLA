#include "mcla_livery_diag.h"

#include <cstdio>
#include <cstdlib>

namespace {
int checks = 0;
void Check(bool value, const char *message) {
  if (!value) {
    std::fprintf(stderr, "FAIL: %s\n", message);
    std::exit(1);
  }
  ++checks;
}

using mcla::livery_diag::DecideDefault;
using mcla::livery_diag::DefaultAction;

void TestDefaultDecision() {
  Check(DecideDefault(false, true, false) == DefaultAction::kDisabledByOption,
        "option off never writes the cvar");
  Check(DecideDefault(false, false, false) == DefaultAction::kDisabledByOption,
        "option off wins over a missing cvar");
  Check(DecideDefault(false, true, true) == DefaultAction::kDisabledByOption,
        "option off with a user value");
  Check(DecideDefault(true, false, false) == DefaultAction::kUnavailable,
        "pinned SDK without the cvar");
  Check(DecideDefault(true, true, true) == DefaultAction::kUserSet,
        "a user value is left alone");
  Check(DecideDefault(true, true, false) == DefaultAction::kEnable,
        "option on and SDK support enable the diagnostics");
  static_assert(DecideDefault(true, true, false) == DefaultAction::kEnable);
}

void TestCvarName() {
  Check(mcla::livery_diag::kLiveryDiag2Cvar == "d3d12_livery_diag2",
        "cvar name matches the SDK");
}
}  // namespace

int main() {
  TestDefaultDecision();
  TestCvarName();
  std::printf("mcla_livery_diag_test: %d checks passed\n", checks);
  return 0;
}
