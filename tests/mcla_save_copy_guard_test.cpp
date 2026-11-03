#include "mcla_save_copy_guard.h"

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

namespace g = mcla::save_copy_guard;

g::GateInput Input(uint32_t sdk_active, bool pending, uint64_t waited) {
  g::GateInput input;
  input.sdk_active = sdk_active;
  input.pending_files = pending;
  input.waited_ms = waited;
  return input;
}

bool Contains(const std::string &text, const char *needle) {
  return text.find(needle) != std::string::npos;
}
} // namespace

int main() {
  // Timing contract of the gate.
  Check(g::kWaitTimeoutMs == 2000, "gate waits at most 2 s");
  Check(g::kPollIntervalMs == 50, "gate polls every 50 ms");
  Check(g::kRetryDelayMs == 100, "sharing-violation retry after 100 ms");
  Check(g::kSkippedNoteFile == "save-copy-skipped.txt", "skipped note name");

  // Indicator states.
  Check(!g::IndicatorAvailable(g::kIndicatorUnavailable),
        "fallback value means unavailable");
  Check(g::IndicatorAvailable(0) && g::IndicatorAvailable(3),
        "real counts are available");
  Check(g::Decide(Input(0, false, 0)) == g::Gate::kCopy,
        "idle SDK and no marker files: copy now");
  Check(g::Decide(Input(g::kIndicatorUnavailable, false, 0)) == g::Gate::kCopy,
        "old SDK without indicator and no marker files: copy now");
  Check(g::Decide(Input(1, false, 0)) == g::Gate::kWait,
        "active transaction: wait");
  Check(g::Decide(Input(2, false, 1950)) == g::Gate::kWait,
        "active transaction before the limit: still wait");
  Check(g::Decide(Input(1, false, 2000)) == g::Gate::kSkip,
        "active transaction at the limit: skip");
  Check(g::Decide(Input(1, false, 5000)) == g::Gate::kSkip,
        "active transaction past the limit: skip");
  Check(g::Decide(Input(1, false, 1999), 2000) == g::Gate::kWait,
        "explicit timeout respected below it");
  Check(g::Decide(Input(0, false, 2500)) == g::Gate::kCopy,
        "cleared after a long wait: copy, never skip");

  // Pending marker/backup files.
  Check(g::Decide(Input(0, true, 0)) == g::Gate::kWait,
        "marker files with idle SDK: wait (spec: either signal is busy)");
  Check(g::Decide(Input(g::kIndicatorUnavailable, true, 100)) == g::Gate::kWait,
        "marker files with old SDK: wait");
  Check(g::Decide(Input(g::kIndicatorUnavailable, true, 2000)) == g::Gate::kSkip,
        "marker files that never clear: skip");
  Check(g::IsTransactionArtifactName("mc4.sav.header.rexpending"),
        "header marker");
  Check(g::IsTransactionArtifactName("mc4.sav.rexbackup"), "package backup");
  Check(g::IsTransactionArtifactName("mc4.sav.header.rexbackup"),
        "header backup");
  Check(g::IsTransactionArtifactName("X.REXPENDING"), "case-insensitive");
  Check(!g::IsTransactionArtifactName("mc4.sav"), "save file");
  Check(!g::IsTransactionArtifactName("mc4.sav.header"), "header file");
  Check(!g::IsTransactionArtifactName("rexpending"), "bare word");
  Check(!g::IsTransactionArtifactName(""), "empty name");
  Check(!g::IsTransactionArtifactName("a.rexbackup.tmp"), "suffix only");

  // Reasons and the skipped note: no paths, both signals named.
  const auto both = g::BusyReason(Input(2, true, 2000));
  Check(Contains(both, "2 active content transaction"), "reason names count");
  Check(Contains(both, ".rexpending/.rexbackup"), "reason names marker files");
  Check(Contains(g::BusyReason(Input(g::kIndicatorUnavailable, true, 0)),
                 "marker") &&
            !Contains(g::BusyReason(Input(g::kIndicatorUnavailable, true, 0)),
                      "SDK reports"),
        "old SDK reason does not invent a count");
  Check(g::BusyReason(Input(0, false, 0)) == "no active content transaction",
        "idle reason");
  const auto note = g::SkippedNote(Input(1, false, 2003));
  Check(Contains(note, "not copied"), "note says what happened");
  Check(Contains(note, "after waiting 2003 ms (limit 2000 ms)"),
        "note says how long");
  Check(Contains(note, "SDK transaction indicator: 1."), "note shows indicator");
  Check(Contains(g::SkippedNote(Input(g::kIndicatorUnavailable, true, 2000)),
                 "unavailable"),
        "note shows an unavailable indicator");
  Check(!Contains(note, ":\\") && !Contains(note, "Users"),
        "note carries no host path");

  // Copy retry: once, only for sharing/lock violations.
  Check(g::ShouldRetryCopy(32, 0), "sharing violation retried");
  Check(g::ShouldRetryCopy(33, 0), "lock violation retried");
  Check(!g::ShouldRetryCopy(32, 1), "never a second retry");
  Check(!g::ShouldRetryCopy(5, 0), "access denied not retried");
  Check(!g::ShouldRetryCopy(2, 0), "file not found not retried");
  Check(!g::ShouldRetryCopy(0, 0), "no error, no retry");

  // A simulated gate loop: busy for 3 polls, then clear -> copy after 150 ms.
  {
    uint64_t waited = 0;
    g::Gate gate = g::Gate::kWait;
    for (int poll = 0; gate == g::Gate::kWait; ++poll) {
      gate = g::Decide(Input(poll < 3 ? 1u : 0u, false, waited));
      if (gate == g::Gate::kWait) waited += g::kPollIntervalMs;
    }
    Check(gate == g::Gate::kCopy && waited == 150,
          "gate copies once the transaction clears");
  }
  // Never clears -> skip at the limit after 40 polls.
  {
    uint64_t waited = 0;
    int polls = 0;
    g::Gate gate = g::Gate::kWait;
    while (gate == g::Gate::kWait) {
      ++polls;
      gate = g::Decide(Input(1, true, waited));
      if (gate == g::Gate::kWait) waited += g::kPollIntervalMs;
    }
    Check(gate == g::Gate::kSkip && waited == 2000 && polls == 41,
          "gate skips at the 2 s limit");
  }

  std::printf("mcla_save_copy_guard_test: %d checks passed\n", checks);
  return 0;
}
