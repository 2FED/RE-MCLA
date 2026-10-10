#pragma once

namespace mcla::diagnostics {

struct HotkeyResult {
  bool queued = false;
  bool audit_attempted = false;
  bool audit_armed = false;
};

// Called only by the UI-owned F10 binding. The SDK owns audit lifetime.
class DiagnosticHotkey {
public:
  template <typename Queue, typename Arm>
  HotkeyResult Request(Queue queue, Arm arm) {
    HotkeyResult result;
    result.queued = queue();
    if (result.queued) {
      // Each accepted F10 gets an attempt; the SDK rejects an already active
      // audit. Never retain a process-lifetime latch or reset an active audit.
      result.audit_attempted = true;
      result.audit_armed = arm();
    }
    return result;
  }
};

} // namespace mcla::diagnostics
