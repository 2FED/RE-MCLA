#pragma once

// M11 time of day (l93; MCLA_TOD v=1): pure parts of the always-on read-only
// log and the default-off dev controls of
// docs/evidence/M11-time-of-day-re-20261112.md (design) and
// docs/evidence/M11-l93-time-of-day-control-20261112.md (this build). The
// hooks are in mcla_tod.cpp; everything here is unit-tested
// (tests/mcla_tod_test.cpp, which also checks every offset, address and
// return address against the generated code).
//
// Static analysis (generated/default at the M11 codegen):
// - The lighting manager pointer is the word at 0x8287E26C (`lis r11,-32120`,
//   `lwz r3,-7572(r11)`); its only update caller, sub_823045B0 (TU16), loads
//   it into r3 and calls sub_822F1DB0 (return 0x823045D0).
// - sub_822F1DB0 (TU15): mgr +14545 (0x38D1) is the auto byte; with it set
//   and the warp flag +14552 (0x38D8) clear, and the simulation not paused
//   (word at 0x828CCEA0, `lis r9,-32115` / `lwz r11,-12640(r9)`: byte +4 set
//   and byte +10 clear means paused), the band rate is stored at +14548
//   (0x38D4, hours per second) and the hour +14540 (0x38CC) advances by
//   rate x the scaled frame delta 0x827D7508 (`fmadds f0,f12,f13,f0`), minus
//   24.0 (the float at 0x82007340, f3) once it reaches 24.0. With the warp
//   flag set the hour moves towards the warp target +14556 (0x38DC) at the
//   rate +14548 times the unscaled delta 0x827D7558 by the shorter way and
//   the flag is cleared on arrival. Every path then publishes the hour to
//   the render slot mgr + 4 * (3633 + *0x8288B9F0) (`addi r10,r11,3633`,
//   `rlwinm r9,r10,2,0,29`, `stfsx f0,r9,r31`), that is +14532 (0x38C4) or
//   +14536 (0x38C8), with no call after it.
// - sub_822F1518 (TU15) is the warp start: it returns when f1 equals the
//   current hour, otherwise stores f2 at +14548, f1 at +14556 and 1 at
//   +14552. The Arcade race preset caller sub_8265D2B8 (TU50) loads the
//   manager into r31, takes f1 from the preset table sub_8238EB90 (return
//   0x8265D60C) and f2 = 3.0 h/s (0x820077F8), and calls it with return
//   address 0x8265D61C.
// - sub_822F3BD8 (TU15) sets the night state when the hour from the getter
//   sub_822F10D0 is above +14568 (0x38E8, 18.0) or below +14564 (0x38E4,
//   6.0): the 06:00 / 18:00 render-pass switch.
//
// Stage 0 (always on, read-only): after each update the hook reads the hour,
// rate, auto byte, warp flag and target, the pause state and the two night
// thresholds, and logs one MCLA_TOD line per real minute plus one at every
// night flag change and every warp start and end.
//
// Controls (default off, init-only host cvars): mcla_tod_rate (1 = the game's
// rate; 10 / 30 multiply the advance, 0 freezes the hour) and
// mcla_tod_start_minutes (0 = off; 270 / 330 replace the Arcade preset warp
// target with 04:30 / 05:30). The rate controls act only after an Arcade
// preset warp has been seen in this session (the gate), only with the auto
// byte set, no warp pending before or after the update and the simulation
// not paused. Guest writes: the hour, its render slot copy and (start keys)
// the warp target register f1, all within 0..24.

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <optional>
#include <string>

namespace mcla::tod {

// The lighting manager and its fields.
inline constexpr uint32_t kManagerPtr = 0x8287E26Cu;
inline constexpr uint32_t kRenderSlotOffset = 0x38C4;    // 14532: two floats
inline constexpr uint32_t kRenderSlotWordBase = 3633;    // 14532 / 4
inline constexpr uint32_t kHourOffset = 0x38CC;          // 14540: float hour
inline constexpr uint32_t kAutoOffset = 0x38D1;          // 14545: byte
inline constexpr uint32_t kGarageOffset = 0x38D2;        // 14546: byte
inline constexpr uint32_t kRateOffset = 0x38D4;          // 14548: hours / s
inline constexpr uint32_t kWarpFlagOffset = 0x38D8;      // 14552: byte
inline constexpr uint32_t kWarpTargetOffset = 0x38DC;    // 14556: float hour
inline constexpr uint32_t kNightLowOffset = 0x38E4;      // 14564: 6.0
inline constexpr uint32_t kNightHighOffset = 0x38E8;     // 14568: 18.0
// Bytes of the manager the hook reads (the last field ends at +0x38EC).
inline constexpr uint32_t kManagerSpan = 0x3900;
// The update thread's render slot index (0 or 1).
inline constexpr uint32_t kUpdateSlotIndex = 0x8288B9F0u;
// The pause state object pointer and its two bytes.
inline constexpr uint32_t kPauseState = 0x828CCEA0u;
inline constexpr uint32_t kPauseActiveOffset = 4;
inline constexpr uint32_t kPauseReleaseOffset = 10;
// Constants and the main timer the update reads.
inline constexpr uint32_t kHoursPerDayConst = 0x82007340u;  // 24.0f (f3)
inline constexpr uint32_t kPresetRateConst = 0x820077F8u;   // 3.0f (f2)
inline constexpr uint32_t kMainTimer = 0x827D7500u;
inline constexpr uint32_t kScaledDeltaOffset = 8;
inline constexpr uint32_t kUnscaledDeltaOffset = 88;
// Functions and return addresses.
inline constexpr uint32_t kUpdate = 0x822F1DB0u;
inline constexpr uint32_t kUpdateCaller = 0x823045B0u;
inline constexpr uint32_t kUpdateReturn = 0x823045D0u;
inline constexpr uint32_t kWarp = 0x822F1518u;
inline constexpr uint32_t kPresetWarpCaller = 0x8265D2B8u;
inline constexpr uint32_t kPresetTable = 0x8238EB90u;
inline constexpr uint32_t kPresetTableReturn = 0x8265D60Cu;
inline constexpr uint32_t kPresetWarpReturn = 0x8265D61Cu;
inline constexpr uint32_t kNightState = 0x822F3BD8u;
inline constexpr uint32_t kHourGetter = 0x822F10D0u;

inline constexpr float kHoursPerDay = 24.0f;
// The largest forward step one update can make that the rate control still
// multiplies (hours): the fastest band rate is 1/120 h/s, so 0.05 h is 6 s
// of frame delta; anything larger is not an ordinary advance (a script or
// option set the hour) and is left alone.
inline constexpr float kMaxStepHours = 0.05f;
// The rate cvar: 1 = the game's own rate (off), 0 = freeze, 2..60 = factor.
inline constexpr int kRateOff = 1;
inline constexpr int kRateFreeze = 0;
inline constexpr int kMaxRateFactor = 60;
// One MCLA_TOD line per real minute (milliseconds).
inline constexpr uint64_t kMinuteMs = 60000;

// --- guest pointers and values ------------------------------------------------

// A manager (or pause object) pointer the hook may read kManagerSpan bytes at.
constexpr bool PointerPlausible(uint32_t p) {
  return p >= 0x00010000u && p <= 0xFFFFFFFFu - kManagerSpan && (p & 3u) == 0;
}

constexpr bool SlotIndexValid(uint32_t index) {
  return index <= 1;
}

// The render slot the game publishes the hour to (mgr + 4 * (3633 + index)).
constexpr uint32_t RenderSlotAddress(uint32_t manager, uint32_t index) {
  return manager + 4u * (kRenderSlotWordBase + index);
}

inline bool HourValid(float hour) {
  return std::isfinite(hour) && hour >= 0.0f && hour < kHoursPerDay;
}

// The night window the game compares against (6.0 / 18.0 at boot).
inline bool ThresholdsSane(float low, float high) {
  return std::isfinite(low) && std::isfinite(high) && low >= 0.0f && low < high &&
         high <= kHoursPerDay;
}

// sub_822F3BD8: night when the hour is above the high or below the low mark.
inline bool IsNight(float hour, float low, float high) {
  return hour > high || hour < low;
}

// 1 night, 0 day, -1 unknown (thresholds not sane).
inline int NightState(float hour, float low, float high) {
  if (!ThresholdsSane(low, high) || !std::isfinite(hour)) {
    return -1;
  }
  return IsNight(hour, low, high) ? 1 : 0;
}

// The pause gate of the update: byte +4 set and byte +10 clear.
constexpr bool Paused(uint8_t active, uint8_t release) {
  return active != 0 && release == 0;
}

// --- hour arithmetic ---------------------------------------------------------

// An hour wrapped into [0, 24).
inline float WrapHour(float hour) {
  if (!std::isfinite(hour)) {
    return hour;
  }
  float h = std::fmod(hour, kHoursPerDay);
  if (h < 0.0f) {
    h += kHoursPerDay;
  }
  return h >= kHoursPerDay ? 0.0f : h;
}

// The forward distance from `before` to `after` across 24:00 (in [0, 24)).
inline float ForwardDelta(float before, float after) {
  float d = after - before;
  if (d < 0.0f) {
    d += kHoursPerDay;
  }
  return d;
}

// One ordinary advance of the update (positive, at most kMaxStepHours).
inline bool StepPlausible(float delta) {
  return delta > 0.0f && delta <= kMaxStepHours;
}

// The rate cvar as a factor: 1 (off), 0 (freeze) or 2..60; anything else off.
constexpr int RateFactor(int cvar) {
  return cvar == kRateFreeze || (cvar >= 2 && cvar <= kMaxRateFactor) ? cvar : kRateOff;
}

// The hour after the control: the game advanced `before` to `after`; the
// rate control adds (k - 1) more steps, freeze writes `before` back. None
// when the control is off, an input is not a valid hour, or the step is not
// an ordinary advance.
inline std::optional<float> FixUp(int factor, float before, float after) {
  if (factor == kRateOff || !HourValid(before) || !HourValid(after)) {
    return std::nullopt;
  }
  const float delta = ForwardDelta(before, after);
  if (!StepPlausible(delta)) {
    return std::nullopt;
  }
  if (factor == kRateFreeze) {
    return before;
  }
  const float hour = WrapHour(after + float(factor - 1) * delta);
  return HourValid(hour) ? std::optional<float>(hour) : std::nullopt;
}

// The update made an ordinary advance the control may act on.
constexpr bool MayAct(bool auto_on, bool warp_before, bool warp_after, bool paused) {
  return auto_on && !warp_before && !warp_after && !paused;
}

// The Arcade-only gate: a control acts only after an Arcade race preset warp
// was seen in this session.
enum class Gate : uint8_t { kIdle, kBlocked, kAct };

constexpr Gate Decide(bool control_on, bool conditions, bool arcade_seen) {
  if (!control_on || !conditions) {
    return Gate::kIdle;
  }
  return arcade_seen ? Gate::kAct : Gate::kBlocked;
}

// --- start keys -----------------------------------------------------------------

// The start cvar in minutes after midnight: 1..1439, anything else off (0).
constexpr int StartMinutes(int cvar) {
  return cvar >= 1 && cvar <= 1439 ? cvar : 0;
}

inline float StartHour(int minutes) {
  return float(minutes) / 60.0f;
}

// The warp target is replaced only at the Arcade race preset call.
constexpr bool ReplaceWarpTarget(uint32_t lr, int start_minutes) {
  return lr == kPresetWarpReturn && start_minutes > 0;
}

constexpr bool IsPresetWarp(uint32_t lr) {
  return lr == kPresetWarpReturn;
}

// --- log lines --------------------------------------------------------------------

inline std::string RateLabel(int factor) {
  if (factor == kRateOff) {
    return "off";
  }
  if (factor == kRateFreeze) {
    return "freeze";
  }
  return "rate" + std::to_string(factor);
}

inline std::string StartLabel(int minutes) {
  if (minutes <= 0) {
    return "off";
  }
  char text[8];
  std::snprintf(text, sizeof(text), "%02d%02d", (minutes / 60) % 100, minutes % 60);
  return text;
}

// One update as read after the game's own code ran.
struct Sample {
  float hour = 0.0f;
  float rate = 0.0f;
  float target = 0.0f;
  float night_low = 0.0f;
  float night_high = 0.0f;
  uint8_t auto_on = 0;
  uint8_t warp = 0;
  uint8_t paused = 0;
  int night = -1;
};

// Why a line is written (bits; several may coincide in one update).
enum Why : uint8_t {
  kWhyNone = 0,
  kWhyFirst = 1,
  kWhyMinute = 2,
  kWhyNight = 4,
  kWhyWarpStart = 8,
  kWhyWarpEnd = 16,
};

inline std::string WhyText(uint8_t why) {
  static constexpr struct {
    uint8_t bit;
    const char *name;
  } kNames[] = {{kWhyFirst, "first"},
                {kWhyMinute, "minute"},
                {kWhyNight, "night"},
                {kWhyWarpStart, "warp_start"},
                {kWhyWarpEnd, "warp_end"}};
  std::string text;
  for (const auto &n : kNames) {
    if ((why & n.bit) != 0) {
      text += text.empty() ? "" : "+";
      text += n.name;
    }
  }
  return text.empty() ? "none" : text;
}

// The line schedule: the first sample, one line per kMinuteMs, and every
// night flag change and warp start / end (compared with the previous
// update). An unknown night state (-1) is not an edge.
struct Scheduler {
  bool has_previous = false;
  int night = -1;
  uint8_t warp = 0;
  uint64_t minute_ms = 0;

  uint8_t Observe(const Sample &s, uint64_t now_ms) {
    if (!has_previous) {
      has_previous = true;
      night = s.night;
      warp = s.warp != 0 ? 1 : 0;
      minute_ms = now_ms;
      return kWhyFirst;
    }
    uint8_t why = kWhyNone;
    const uint8_t w = s.warp != 0 ? 1 : 0;
    if (w != warp) {
      why |= w != 0 ? kWhyWarpStart : kWhyWarpEnd;
    }
    if (s.night >= 0 && night >= 0 && s.night != night) {
      why |= kWhyNight;
    }
    if (now_ms >= minute_ms + kMinuteMs) {
      why |= kWhyMinute;
      minute_ms = now_ms;
    }
    warp = w;
    if (s.night >= 0) {
      night = s.night;
    }
    return why;
  }
};

// Session counters (F10).
struct Counters {
  uint64_t updates = 0;         // updates observed with a valid manager
  uint64_t lines = 0;           // MCLA_TOD lines written
  uint64_t night_changes = 0;   // night flag edges
  uint64_t warp_starts = 0;     // warp flag 0 -> 1 edges
  uint64_t warp_calls = 0;      // sub_822F1518 calls (any caller)
  uint64_t preset_warps = 0;    // ... from the Arcade race preset
  uint64_t start_replaced = 0;  // preset warp targets replaced
  uint64_t acts = 0;            // control fix-ups written
  uint64_t blocked = 0;         // control would act, gate closed
};

inline std::string Line(uint8_t why, const Sample &s, const std::string &ctl,
                        const std::string &start, bool gate, uint32_t warp_lr, uint64_t acts) {
  char text[384];
  std::snprintf(text, sizeof(text),
                "MCLA_TOD v=1 why=%s hour=%06.3f rate_hps=%.6f auto=%u warp=%u target=%06.3f "
                "paused=%u night=%d ctl=%s start=%s gate=%d warp_lr=0x%08X acts=%llu",
                WhyText(why).c_str(), double(s.hour), double(s.rate), unsigned(s.auto_on),
                unsigned(s.warp), double(s.target), unsigned(s.paused), s.night, ctl.c_str(),
                start.c_str(), gate ? 1 : 0, unsigned(warp_lr),
                static_cast<unsigned long long>(acts));
  return text;
}

// The F10 "time_of_day" object.
inline std::string StateJson(bool valid, const Sample &s, const std::string &ctl,
                             const std::string &start, bool gate, uint32_t warp_lr,
                             const Counters &c) {
  char head[512];
  std::snprintf(head, sizeof(head),
                "{\"version\": 1, \"valid\": %s, \"hour\": %.4f, \"rate_hps\": %.6f, "
                "\"auto\": %s, \"warp\": %s, \"target\": %.4f, \"paused\": %s, \"night\": %d, "
                "\"night_low\": %.3f, \"night_high\": %.3f, \"ctl\": \"%s\", \"start\": \"%s\", "
                "\"gate\": %s, \"warp_lr\": \"0x%08X\", ",
                valid ? "true" : "false", double(s.hour), double(s.rate),
                s.auto_on ? "true" : "false", s.warp ? "true" : "false", double(s.target),
                s.paused ? "true" : "false", s.night, double(s.night_low), double(s.night_high),
                ctl.c_str(), start.c_str(), gate ? "true" : "false", unsigned(warp_lr));
  char tail[512];
  std::snprintf(tail, sizeof(tail),
                "\"updates\": %llu, \"lines\": %llu, \"night_changes\": %llu, "
                "\"warp_starts\": %llu, \"warp_calls\": %llu, \"preset_warps\": %llu, "
                "\"start_replaced\": %llu, \"acts\": %llu, \"blocked\": %llu}",
                static_cast<unsigned long long>(c.updates),
                static_cast<unsigned long long>(c.lines),
                static_cast<unsigned long long>(c.night_changes),
                static_cast<unsigned long long>(c.warp_starts),
                static_cast<unsigned long long>(c.warp_calls),
                static_cast<unsigned long long>(c.preset_warps),
                static_cast<unsigned long long>(c.start_replaced),
                static_cast<unsigned long long>(c.acts),
                static_cast<unsigned long long>(c.blocked));
  return std::string(head) + tail;
}

}  // namespace mcla::tod

// The F10 state block "time_of_day" (always present once the module is
// linked; "valid": false before the first update).
std::string MclaTodStateJson();
