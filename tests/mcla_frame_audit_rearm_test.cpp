// Guest-free integration: real installed SDK audit state, synthetic samples.
// No graphics device, game, window, capture, profile, configuration or file I/O.
#include "mcla_diagnostic_hotkey.h"
#include <rex/perf/frame_audit.h>

#include <cstdio>
#include <cstdlib>

namespace {
int checks = 0;
void Check(bool condition, const char *message) {
  if (!condition) {
    std::fprintf(stderr, "FAIL: %s\n", message);
    std::exit(1);
  }
  ++checks;
}
void CompleteAudit(uint64_t base) {
  for (uint64_t i = 0; i < 300; ++i) {
    if (i == 150) {
      Check(!rex::perf::ArmFrameAuditForDiagnostics(),
            "mid-audit admission must not reset already collected samples");
    }
    Check(rex::perf::IsFrameAuditEnabled(), "audit active before each paired sample");
    const auto frame = base + i;
    rex::perf::BeginGuestFrame(frame);
    rex::perf::RecordCpuFrame(frame, 1000);
    Check(rex::perf::IsFrameAuditEnabled(), "CPU alone must not complete paired audit");
    rex::perf::RecordGpuFrame(frame, 2000);
    Check(rex::perf::IsFrameAuditEnabled() == (i != 299),
          "real SDK disables exactly after 300 paired samples");
  }
}
} // namespace

int main() {
  mcla::diagnostics::DiagnosticHotkey hotkey;
  int calls = 0;
  auto arm = [&]() { ++calls; return rex::perf::ArmFrameAuditForDiagnostics(); };
  auto accepted = []() { return true; };
  auto rejected = []() { return false; };
  Check(!rex::perf::InitializeFrameAudit(), "fresh SDK starts with audit off");
  rex::perf::SetFrameAuditGpuTimingAvailable(false);
  auto result = hotkey.Request(accepted, arm);
  Check(result.queued && result.audit_attempted && !result.audit_armed,
        "unavailable real SDK timing rejects arm without rejecting package");
  rex::perf::SetFrameAuditGpuTimingAvailable(true); // Simulated backend availability only.
  result = hotkey.Request(accepted, arm);
  Check(result.audit_armed, "first real SDK audit arms");
  result = hotkey.Request(accepted, arm);
  Check(!result.audit_armed && rex::perf::IsFrameAuditEnabled(), "active audit cannot reset");
  result = hotkey.Request(rejected, arm);
  Check(!result.queued && !result.audit_attempted && calls == 3,
        "busy package never calls SDK admission");
  CompleteAudit(1);
  result = hotkey.Request(accepted, arm);
  Check(result.audit_armed, "same hotkey instance rearms after real SDK completion");
  CompleteAudit(1001);
  rex::perf::SetFrameAuditGpuTimingAvailable(false);
  result = hotkey.Request(accepted, arm);
  Check(!result.audit_armed && !rex::perf::IsFrameAuditEnabled(),
        "unavailable timing after completion remains fail-closed");
  std::printf("PASS: %d real SDK admission/completion checks; synthetic samples, no GPU/game\n", checks);
  // Make the receipt observable before teardown of the real SDK's DLL state.
  return std::fflush(stdout) == 0 ? 0 : 2;
}
