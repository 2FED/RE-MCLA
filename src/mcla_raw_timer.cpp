// Optional measured-time ("raw") game timer.
//
// Stock MCLA advances game time by a fixed 1/30 s per update (timer mode
// +60 = 1, set by sub_821BD9F8). When the host cannot sustain 30 updates/s the
// whole game runs in slow motion. The game also ships a raw policy
// (sub_821BD9B8) that uses the measured timebase delta, clamped per tick.
// sub_822D2478 picks the policy from its configuration object: +72 == 1
// selects raw, and +88 is an optional cap that is cleared here so raw time is
// not capped at 1/30 s. See docs/evidence/M8-001-timebox-20261025.md.
//
// Default ON (MCLA_RAW_TIMER_DEFAULT=0 builds default OFF). --mcla_raw_timer=false
// keeps the stock fixed timer for A/B comparison.

#include "mcla_init.h"
#include "mcla_logging.h"

#include <rex/cvar.h>

#include <atomic>

#ifndef MCLA_RAW_TIMER_DEFAULT
#define MCLA_RAW_TIMER_DEFAULT 1
#endif

REXCVAR_DEFINE_BOOL(mcla_raw_timer, MCLA_RAW_TIMER_DEFAULT != 0, "MCLA",
    "Use the game's own measured-time timer instead of fixed 1/30 s steps")
    .lifecycle(rex::cvar::Lifecycle::kInitOnly);

namespace {

constexpr uint32_t kOne = 0x3F800000u;     // 1.0f
constexpr uint32_t kThirty = 0x41F00000u;  // 30.0f

bool RawTimerWanted() {
  static const bool wanted = REXCVAR_GET(mcla_raw_timer);
  return wanted;
}

std::atomic<int> logged{0};

}  // namespace

// Strong override of the generated weak sub_822D2478; the original body runs
// unchanged after the optional two-word configuration edit.
REX_EXTERN(sub_822D2478) {
  const uint32_t config = ctx.r3.u32;
  const bool raw = RawTimerWanted();
  bool edited = false;
  // Only the exact stock fixed30 configuration is edited.
  if (raw && config != 0 && REX_LOAD_U32(config + 72) == 0 &&
      REX_LOAD_U32(config + 80) == kOne && REX_LOAD_U32(config + 84) == kThirty &&
      REX_LOAD_U32(config + 88) == kThirty && REX_LOAD_U32(config + 92) == 1) {
    REX_STORE_U32(config + 88, 0u);
    REX_STORE_U32(config + 72, 1u);
    edited = true;
  }
  // The configuration routine runs only a few times; log the first calls.
  if (logged.fetch_add(1) < 4) {
    MCLA_APP_INFO("MCLA timer: {} (configuration edit {})",
                  raw ? "raw measured time" : "stock fixed 1/30 s",
                  edited ? "applied" : "not applied");
  }
  __imp__sub_822D2478(ctx, base);
}
