#pragma once

// KI-035 garage-entry freeze: pure logic of the warp camera probe and the
// state-20 watchdog. No SDK dependencies, so it is unit-tested
// (tests/mcla_warp_probe_test.cpp); the guest reads, the hook and the cvars
// live in mcla_ki026_probe.cpp. See
// docs/evidence/M10-ki035-warp-probe-20261104.md.
//
// mcWarper::Update (sub_8221BE98) state 20 (loc_8221C120) waits each UI frame
// until the warp camera (warper+296, a UITransition) reports ready
// (vfunc +92) and its time (vfunc +608) plus dt plus 0.0001 reaches its
// duration (vfunc +592). It has no timeout. The l67 freeze stayed in state
// 20 with the camera on screen and not moving.
//
// The UITransition getters are plain field reads in the recompiled code, so
// the host reads the fields instead of calling guest code. Each getter is
// recognised by the guest address the camera's vtable slot holds; an unknown
// getter leaves the value unknown (and the watchdog idle).

#include <cstdint>

namespace mcla::warp {

// --- warp camera getters (UITransition vtable slots) ------------------------
inline constexpr uint32_t kSlotReady = 92;
inline constexpr uint32_t kSlotDuration = 592;
inline constexpr uint32_t kSlotTime = 608;

// vfunc +92, every UITransition class: (u32 at +16 & 0x80) != 0.
inline constexpr uint32_t kReadyFlagsBit = 0x8221CE10u;
// vfunc +592: +76 (UITransition, _Rotate, _Lerp, _Value, _GPSJump,
// _LookAtTarget); +76 after a lazy recompute when +76 < 0 (_Parallel,
// _Serial, _Bounded, _FarAwayTarget, _Zoom, _Lean); +76, times 1/3 when byte
// +471 is set (_Translate_Spline*, _Translate_Axis).
inline constexpr uint32_t kDurationField76 = 0x823ABE90u;
inline constexpr uint32_t kDurationComposite76 = 0x8221D060u;
inline constexpr uint32_t kDurationSpline76 = 0x8221D130u;
// vfunc +608: +96 (most classes) or +80, times 1/3 when byte +471 is set
// (the spline/axis classes).
inline constexpr uint32_t kTimeField96 = 0x8221CFA0u;
inline constexpr uint32_t kTimeSpline80 = 0x8221D108u;
// The 1/3 factor of the spline getters (lfs 32688 from 0x82000000).
inline constexpr float kSplineScale = 1.0f / 3.0f;
// The epsilon the state-20 compare adds (0x820048B4).
inline constexpr float kExitEpsilon = 0.0001f;

enum class Getter : uint8_t {
  kUnknown,
  kReadyFlagsBit,
  kDurationField76,
  kDurationComposite76,
  kDurationSpline76,
  kTimeField96,
  kTimeSpline80,
};

constexpr Getter ReadyGetter(uint32_t function) {
  return function == kReadyFlagsBit ? Getter::kReadyFlagsBit : Getter::kUnknown;
}

constexpr Getter DurationGetter(uint32_t function) {
  switch (function) {
    case kDurationField76:
      return Getter::kDurationField76;
    case kDurationComposite76:
      return Getter::kDurationComposite76;
    case kDurationSpline76:
      return Getter::kDurationSpline76;
    default:
      return Getter::kUnknown;
  }
}

constexpr Getter TimeGetter(uint32_t function) {
  switch (function) {
    case kTimeField96:
      return Getter::kTimeField96;
    case kTimeSpline80:
      return Getter::kTimeSpline80;
    default:
      return Getter::kUnknown;
  }
}

constexpr const char *GetterName(Getter getter) {
  switch (getter) {
    case Getter::kUnknown:
      return "unknown";
    case Getter::kReadyFlagsBit:
      return "flags16&0x80";
    case Getter::kDurationField76:
      return "f76";
    case Getter::kDurationComposite76:
      return "f76-composite";
    case Getter::kDurationSpline76:
      return "f76-spline";
    case Getter::kTimeField96:
      return "f96";
    case Getter::kTimeSpline80:
      return "f80-spline";
  }
  return "unknown";
}

// The UITransition vtables of the title (MSVC RTTI complete-object locators
// of every class with UITransition as a base, offset 0). Used only to name
// the class in logs and F10 blocks; the getters above decide the values.
struct VtableName {
  uint32_t vtable;
  const char *name;
};
inline constexpr VtableName kTransitionVtables[] = {
    {0x82020A8Cu, "UITransition"},
    {0x82020DB4u, "UITransition_Parallel"},
    {0x820210DCu, "UITransition_Serial"},
    {0x82021404u, "UITransition_Translate_Spline"},
    {0x8202297Cu, "UITransition_Bounded"},
    {0x82022CA4u, "UITransition_FarAwayTarget"},
    {0x82022FCCu, "UITransition_Zoom"},
    {0x820232F4u, "UITransition_Translate_Spline_ExitSharpAngle"},
    {0x82025E04u, "UITransition_Translate_Axis"},
    {0x8202620Cu, "UITransition_Rotate"},
    {0x82026584u, "UITransition_Lerp"},
    {0x82026904u, "UITransition_Value"},
    {0x82026CB4u, "UITransition_GPSJump"},
    {0x82026FE4u, "UITransition_LookAtTarget"},
    {0x82027324u, "UITransition_Lean"},
};

constexpr const char *TransitionClassName(uint32_t vtable) {
  for (const VtableName &entry : kTransitionVtables) {
    if (entry.vtable == vtable) {
      return entry.name;
    }
  }
  return "unknown";
}

// Raw camera fields the getters read.
struct CameraFields {
  uint32_t flags16 = 0;
  float f76 = 0.0f;
  float f80 = 0.0f;
  float f96 = 0.0f;
  uint8_t b471 = 0;
};

struct Value {
  bool known = false;
  float value = 0.0f;
};

constexpr bool Ready(Getter getter, const CameraFields &fields, bool &known) {
  known = getter == Getter::kReadyFlagsBit;
  return known && (fields.flags16 & 0x80u) != 0;
}

// The duration the getter returns. A composite with +76 < 0 recomputes it
// through further virtual calls (sub_8221D060); the host does not, so that
// case is unknown.
constexpr Value Duration(Getter getter, const CameraFields &fields) {
  switch (getter) {
    case Getter::kDurationField76:
      return {true, fields.f76};
    case Getter::kDurationComposite76:
      if (fields.f76 < 0.0f) {
        return {};
      }
      return {true, fields.f76};
    case Getter::kDurationSpline76:
      return {true, fields.b471 ? fields.f76 * kSplineScale : fields.f76};
    default:
      return {};
  }
}

constexpr Value Time(Getter getter, const CameraFields &fields) {
  switch (getter) {
    case Getter::kTimeField96:
      return {true, fields.f96};
    case Getter::kTimeSpline80:
      return {true, fields.b471 ? fields.f80 * kSplineScale : fields.f80};
    default:
      return {};
  }
}

// --- warp object ------------------------------------------------------------
inline constexpr uint32_t kStateCinematicZoom = 20;  // loc_8221C120
inline constexpr uint32_t kStateIdle = 29;

// --- watchdog ---------------------------------------------------------------
// One sample per mcWarper::Update call, taken after the stock update.
struct Sample {
  uint64_t now_ms = 0;
  bool complete = false;  // every guest read succeeded
  uint32_t state = 0;
  uint32_t request = 0;
  uint32_t camera = 0;
  bool time_known = false;
  float time = 0.0f;
  bool paused = false;  // simulation pause manager paused (not frame-advance)
};

enum class Verdict : uint8_t {
  kIdle,      // not in state 20, or an incomplete sample
  kWatching,  // state 20, camera time advancing or not yet over the limit
  kFire,      // perform the handler's own exit now (once per episode)
  kFired,     // already fired in this episode
};

constexpr const char *VerdictName(Verdict verdict) {
  switch (verdict) {
    case Verdict::kIdle:
      return "idle";
    case Verdict::kWatching:
      return "watching";
    case Verdict::kFire:
      return "fire";
    case Verdict::kFired:
      return "fired";
  }
  return "unknown";
}

// State machine over the samples. An episode starts when the warper enters
// state 20 (or the request or camera pointer changes while in state 20) and
// ends with any other state or an incomplete sample. Within an episode the
// "frozen since" time restarts whenever the camera time increases, and while
// the simulation is paused. It fires once per episode, only in state 20,
// when the camera time is known and has not increased for limit_ms.
class Watchdog {
 public:
  Verdict Observe(const Sample &sample, uint64_t limit_ms) {
    if (!sample.complete || sample.state != kStateCinematicZoom || !sample.request ||
        !sample.camera || !sample.time_known || sample.time != sample.time) {
      active_ = false;
      return Verdict::kIdle;
    }
    if (!active_ || sample.request != request_ || sample.camera != camera_) {
      active_ = true;
      fired_ = false;
      request_ = sample.request;
      camera_ = sample.camera;
      entered_ms_ = sample.now_ms;
      frozen_since_ms_ = sample.now_ms;
      max_time_ = sample.time;
      return Verdict::kWatching;
    }
    if (fired_) {
      return Verdict::kFired;
    }
    if (sample.time > max_time_) {
      max_time_ = sample.time;
      frozen_since_ms_ = sample.now_ms;
    }
    if (sample.paused) {
      frozen_since_ms_ = sample.now_ms;
    }
    if (limit_ms == 0 || sample.now_ms < frozen_since_ms_ ||
        sample.now_ms - frozen_since_ms_ < limit_ms) {
      return Verdict::kWatching;
    }
    fired_ = true;
    return Verdict::kFire;
  }

  bool active() const { return active_; }
  uint64_t entered_ms() const { return entered_ms_; }
  uint64_t frozen_since_ms() const { return frozen_since_ms_; }
  float max_time() const { return max_time_; }

 private:
  bool active_ = false;
  bool fired_ = false;
  uint32_t request_ = 0;
  uint32_t camera_ = 0;
  uint64_t entered_ms_ = 0;
  uint64_t frozen_since_ms_ = 0;
  float max_time_ = 0.0f;
};

// mcla_warp_watchdog_seconds: 0 = off; values are clamped to 1..600 s.
constexpr uint64_t WatchdogLimitMs(int32_t seconds) {
  if (seconds <= 0) {
    return 0;
  }
  return static_cast<uint64_t>(seconds > 600 ? 600 : seconds) * 1000u;
}

// --- probe log pacing ---------------------------------------------------------
// State changes are logged as they happen, at most kMaxStateLines per
// process. While the warper stays in state 20 a wait line is logged every
// kWaitIntervalMs, at most kMaxWaitLinesPerEpisode per episode.
inline constexpr uint32_t kMaxStateLines = 2000;
inline constexpr uint64_t kWaitIntervalMs = 5000;
inline constexpr uint32_t kMaxWaitLinesPerEpisode = 24;

class WaitPacer {
 public:
  // Returns true when a wait line should be logged for this sample.
  bool Observe(uint64_t now_ms, bool in_state20) {
    if (!in_state20) {
      active_ = false;
      return false;
    }
    if (!active_) {
      active_ = true;
      lines_ = 0;
      last_ms_ = now_ms;
      return false;  // the state-change line covers the entry
    }
    if (lines_ >= kMaxWaitLinesPerEpisode || now_ms < last_ms_ ||
        now_ms - last_ms_ < kWaitIntervalMs) {
      return false;
    }
    last_ms_ = now_ms;
    ++lines_;
    return true;
  }

 private:
  bool active_ = false;
  uint32_t lines_ = 0;
  uint64_t last_ms_ = 0;
};

// --- window focus log pacing -------------------------------------------------
// At most kFocusBurst lines per kFocusWindowMs; the next allowed line reports
// how many were suppressed in between.
inline constexpr uint32_t kFocusBurst = 20;
inline constexpr uint64_t kFocusWindowMs = 60000;

class RateLimiter {
 public:
  constexpr RateLimiter(uint32_t burst, uint64_t window_ms)
      : burst_(burst), window_ms_(window_ms) {}

  // True when a line may be logged now; suppressed() then holds the number of
  // lines dropped since the previous allowed one (reset by the next Allow).
  bool Allow(uint64_t now_ms) {
    if (!started_ || now_ms < window_start_ms_ || now_ms - window_start_ms_ >= window_ms_) {
      started_ = true;
      window_start_ms_ = now_ms;
      count_ = 0;
    }
    if (count_ >= burst_) {
      ++pending_suppressed_;
      return false;
    }
    ++count_;
    suppressed_ = pending_suppressed_;
    pending_suppressed_ = 0;
    return true;
  }

  uint32_t suppressed() const { return suppressed_; }

 private:
  uint32_t burst_;
  uint64_t window_ms_;
  bool started_ = false;
  uint64_t window_start_ms_ = 0;
  uint32_t count_ = 0;
  uint32_t suppressed_ = 0;
  uint32_t pending_suppressed_ = 0;
};

}  // namespace mcla::warp
