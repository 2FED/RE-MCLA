#pragma once

// KI-065: the F10 diagnostics export copies the live save tree. A copy that
// overlaps a saved-game transaction of the SDK content layer (XamContentCreate
// ... XamContentClose: backup copy, delete/recreate, header write, commit) can
// hold a save file open while the title writes it. Pure decisions of the
// export's save-copy gate: wait while a transaction is active, skip the copy
// if it does not clear in time, retry one copy after a sharing violation.

#include <cstdint>
#include <string>
#include <string_view>

namespace mcla::save_copy_guard {

// Gate timing: poll every 50 ms for at most 2 s.
inline constexpr uint64_t kWaitTimeoutMs = 2000;
inline constexpr uint64_t kPollIntervalMs = 50;
// A copy that failed with a sharing/lock violation is retried once after this.
inline constexpr uint64_t kRetryDelayMs = 100;

// Value of the SDK indicator when the linked SDK does not export it.
inline constexpr uint32_t kIndicatorUnavailable = 0xFFFFFFFFu;

// Name of the note written into a package whose save copy was skipped.
inline constexpr std::string_view kSkippedNoteFile = "save-copy-skipped.txt";

// Windows error codes that mean another handle holds the file.
inline constexpr uint32_t kErrorSharingViolation = 32;
inline constexpr uint32_t kErrorLockViolation = 33;

// Transaction marker and backup names of the SDK content layer
// ("<save>.header.rexpending", "<save>.rexbackup", "<save>.header.rexbackup").
inline bool IsTransactionArtifactName(std::string_view name) {
  auto ends_with_ci = [name](std::string_view suffix) {
    if (name.size() < suffix.size()) return false;
    const auto tail = name.substr(name.size() - suffix.size());
    for (size_t i = 0; i < suffix.size(); ++i) {
      char c = tail[i];
      if (c >= 'A' && c <= 'Z') c = static_cast<char>(c - 'A' + 'a');
      if (c != suffix[i]) return false;
    }
    return true;
  };
  return ends_with_ci(".rexpending") || ends_with_ci(".rexbackup");
}

struct GateInput {
  uint32_t sdk_active = kIndicatorUnavailable;  // SDK transaction count
  bool pending_files = false;                   // markers/backups in the tree
  uint64_t waited_ms = 0;                       // time already spent waiting
};

enum class Gate { kCopy, kWait, kSkip };

inline bool IndicatorAvailable(uint32_t sdk_active) {
  return sdk_active != kIndicatorUnavailable;
}

inline bool Busy(const GateInput &input) {
  return (IndicatorAvailable(input.sdk_active) && input.sdk_active > 0) ||
         input.pending_files;
}

inline Gate Decide(const GateInput &input,
                   uint64_t timeout_ms = kWaitTimeoutMs) {
  if (!Busy(input)) return Gate::kCopy;
  return input.waited_ms >= timeout_ms ? Gate::kSkip : Gate::kWait;
}

// Reason text of the skipped note and the log lines. No paths, no profile ids.
inline std::string BusyReason(const GateInput &input) {
  std::string reason;
  if (IndicatorAvailable(input.sdk_active) && input.sdk_active > 0) {
    reason = "SDK reports " + std::to_string(input.sdk_active) +
             " active content transaction(s)";
  }
  if (input.pending_files) {
    if (!reason.empty()) reason += "; ";
    reason += "transaction marker/backup files (.rexpending/.rexbackup) are "
              "present in the save tree";
  }
  if (reason.empty()) reason = "no active content transaction";
  return reason;
}

inline std::string IndicatorText(uint32_t sdk_active) {
  return IndicatorAvailable(sdk_active)
             ? std::to_string(sdk_active)
             : std::string("unavailable (SDK without the indicator)");
}

inline std::string SkippedNote(const GateInput &input,
                               uint64_t timeout_ms = kWaitTimeoutMs) {
  return "The live save files were not copied into this package.\n"
         "Reason: " + BusyReason(input) + " after waiting " +
         std::to_string(input.waited_ms) + " ms (limit " +
         std::to_string(timeout_ms) + " ms).\n"
         "SDK transaction indicator: " + IndicatorText(input.sdk_active) +
         ".\n"
         "The export does not copy saves while the game is writing them, so "
         "it never races a save transaction (KI-065).\n";
}

// One retry, only for sharing/lock violations.
inline bool ShouldRetryCopy(uint32_t system_error, uint32_t retries_done) {
  return retries_done == 0 && (system_error == kErrorSharingViolation ||
                               system_error == kErrorLockViolation);
}

} // namespace mcla::save_copy_guard
