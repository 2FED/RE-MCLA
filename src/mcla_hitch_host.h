#pragma once

// l99 (M11, docs/evidence/M11-l99-sdk-hitch-20261113.md): the host's companion
// line to the SDK's MCLA_HITCH event lines. The SDK (rexruntime.dll, l99 and
// later) counts the MCLA_HITCH lines it writes (rex_hitch_logged_sequence,
// lock-free) and keeps the last hitch (rex_hitch_last). The time-of-day update
// hook (mcla_tod.cpp, once per simulation frame on the update thread) polls
// the count; when it moved, it writes one MCLA_HITCH_HOST line with the game
// state the SDK cannot see (hour, pause, warp). The SDK rate-limits its lines
// (20 per minute), so this line is bounded too. Pure; unit-tested in
// tests/mcla_tod_test.cpp. With an SDK without the entry points nothing is
// polled (one kind=entry line says so).

#include <cstdint>
#include <cstdio>
#include <string>

namespace mcla::hitch {

// The update-thread poller: Observe returns true when new MCLA_HITCH lines
// were written since the last call; missed = lines between two polls minus 1
// (several hitch lines before the next game update).
class Poller {
 public:
  bool Observe(uint64_t logged_seq, uint64_t &missed) {
    missed = 0;
    if (logged_seq <= last_) {
      return false;
    }
    missed = logged_seq - last_ - 1;
    last_ = logged_seq;
    return true;
  }
  uint64_t last() const { return last_; }

 private:
  uint64_t last_ = 0;
};

struct Last {
  uint64_t seq = 0;          // the SDK hitch sequence (MCLA_HITCH seq=)
  uint64_t logged_seq = 0;
  uint64_t frame = 0;        // the SDK command-processor frame (MCLA_HITCH frame=)
  uint64_t interval_us = 0;
};

struct GameState {
  bool tod_valid = false;
  float hour = 0.0f;
  bool paused = false;
  bool warp = false;
};

inline std::string HostLine(const Last &last, uint64_t missed, const GameState &game) {
  char hour[32];
  if (game.tod_valid) {
    std::snprintf(hour, sizeof(hour), "%.2f", static_cast<double>(game.hour));
  } else {
    std::snprintf(hour, sizeof(hour), "na");
  }
  char text[256];
  std::snprintf(text, sizeof(text),
                "MCLA_HITCH_HOST v=1 seq=%llu frame=%llu ms=%.1f tod_hour=%s paused=%s warp=%s "
                "pursuit=na missed=%llu",
                static_cast<unsigned long long>(last.seq),
                static_cast<unsigned long long>(last.frame),
                static_cast<double>(last.interval_us) / 1000.0, hour,
                game.tod_valid ? (game.paused ? "1" : "0") : "na",
                game.tod_valid ? (game.warp ? "1" : "0") : "na",
                static_cast<unsigned long long>(missed));
  return text;
}

inline std::string EntryLine(bool found) {
  return std::string("MCLA_HITCH_HOST v=1 kind=entry sdk_entry=") + (found ? "found" : "absent");
}

}  // namespace mcla::hitch
