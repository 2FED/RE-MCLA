#include "mcla_diagnostic_hotkey.h"

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
} // namespace

int main() {
  mcla::diagnostics::DiagnosticHotkey hotkey;
  bool active = false;
  bool available = true;
  bool busy = false;
  int starts = 0;
  int arm_calls = 0;
  int queue_calls = 0;
  auto queue = [&]() { ++queue_calls; return !busy; };
  auto arm = [&]() {
    ++arm_calls;
    if (active || !available) return false;
    active = true;
    ++starts;
    return true;
  };
  auto result = hotkey.Request(queue, arm);
  Check(result.queued && result.audit_attempted && result.audit_armed,
        "first accepted F10 arms audit");
  active = false; // SDK finishes its bounded audit before the next owner F10.
  result = hotkey.Request(queue, arm);
  Check(result.queued && result.audit_attempted && result.audit_armed && starts == 2,
        "later F10 after audit completion must rearm in the same process");
  result = hotkey.Request(queue, arm);
  Check(result.queued && result.audit_attempted && !result.audit_armed && starts == 2,
        "active SDK audit is not restarted or extended");
  busy = true;
  const int before_busy = arm_calls;
  result = hotkey.Request(queue, arm);
  Check(!result.queued && !result.audit_attempted && !result.audit_armed &&
            arm_calls == before_busy,
        "rejected snapshot must not arm an unrelated audit");
  busy = false;
  active = false;
  available = false;
  result = hotkey.Request(queue, arm);
  Check(result.queued && result.audit_attempted && !result.audit_armed,
        "unavailable GPU timing does not reject diagnostic package");
  available = true;
  result = hotkey.Request(queue, arm);
  Check(result.queued && result.audit_armed && starts == 3 && queue_calls == 6,
        "unavailable timing does not permanently suppress later audit");
  busy = true;
  const int before_rejection = arm_calls;
  result = hotkey.Request(queue, arm);
  Check(!result.queued && !result.audit_attempted && arm_calls == before_rejection,
        "busy rejection remains independent of active audit");
  busy = false;
  active = false;
  result = hotkey.Request(queue, arm);
  Check(result.queued && result.audit_armed && starts == 4 && queue_calls == 8,
        "busy rejection cannot suppress the next accepted request after completion");
  std::printf("PASS: %d F10 admission checks\n", checks);
}
