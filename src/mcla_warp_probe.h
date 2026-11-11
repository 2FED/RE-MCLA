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
//
// KI-026 (docs/evidence/M11-ki026-implementation-20261111.md) adds the
// state-19 callback decision (DecideFinish) and the stall detector below.

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
inline constexpr uint32_t kStateZoomIn = 19;         // loc_8221C044 (KI-026)
inline constexpr uint32_t kStateCinematicZoom = 20;  // loc_8221C120
inline constexpr uint32_t kStateIdle = 29;

// --- finish-race fix (KI-035 root cause) -------------------------------------
// docs/evidence/M10-l68-pc-ab-ki035-root-cause-20261105.md part 2 and
// docs/evidence/M10-ki035-finish-race-fix-20261105.md. Citations are
// generated/default/mcla_recomp.8.cpp.
//
// Each UI frame updates the transition list (sub_82221DB0) before the warper
// (sub_8221BE98). State 20 exits only while the camera is ready and
// time + dt + 0.0001 >= duration with the *current* dt. When dt grows on the
// last frame of the zoom-in, the camera finishes itself first (SetFinished
// clears ready, the stop path resets time) and state 20 never exits.
//
// The warper's camera-finished callback sub_8221B8C8 (r3 = warper, r4 = event
// source, r5 = finished transition) proceeds only when
//   0x8221B8DC..E0  lwz r11,256(r31); cmplw cr6,r11,r4    warper+256 == r4
//   0x8221B8E8..EC  lwz r11,68(r31);  cmplwi cr6,r11,0    request != 0
//   0x8221B8F4..F8  lwz r11,108(r11); cmplw cr6,r5,r11    r5 == request+108
// then clears warper+300 only when r5 == warper+300 (0x8221B900..0x8221B924),
// calls the finalise sub_8221B790 only when warper+276 != 0 (0x8221B984..94)
// and always sets warper+273 = 1 (0x8221B998..9C).
//
// The fix: when that callback arrives for the warper's own camera while the
// warper is still in state 20, perform the state-20 handler's own exit first
// (0x8221C1C8..0x8221C1DC: lis r11,-32121; li r10,29; lwz r11,-10000(r11);
// stb r26(=0),293(r11); stb r28(=1),276(r31); stw r10,288(r31)), then run the
// original callback, which now finalises exactly as on a healthy trip.
inline constexpr uint32_t kFinishCallback = 0x8221B8C8u;
inline constexpr uint32_t kWarperEventSource = 256;  // compared with r4
inline constexpr uint32_t kWarperRequestField = 68;
inline constexpr uint32_t kRequestCameraField = 108;  // compared with r5
inline constexpr uint32_t kWarperCameraField = 296;   // the state-20 wait camera
inline constexpr uint32_t kWarperCameraCopyField = 300;
inline constexpr uint32_t kWarperFinishFlagField = 276;
inline constexpr uint32_t kWarperNotifiedField = 273;
inline constexpr uint32_t kWarperStateField = 288;
// lis r11,-32121; lwz r11,-10000(r11)
inline constexpr uint32_t kExitGlobalPointer =
    static_cast<uint32_t>(-32121 * 65536) + static_cast<uint32_t>(-10000);
inline constexpr uint32_t kExitGlobalByteField = 293;
inline constexpr uint8_t kExitGlobalByteValue = 0;  // stb r26 (prologue li r26,0)
inline constexpr uint8_t kExitFinishFlagValue = 1;  // stb r28 (prologue li r28,1)
inline constexpr uint32_t kExitStateValue = kStateIdle;  // li r10,29
static_assert(kExitGlobalPointer == 0x8286D8F0u);

// What the callback hook reads before the original runs.
struct FinishSample {
  bool enabled = true;        // mcla_warp_finish_fix (state 20)
  bool stall_enabled = false; // mcla_warp_stall_fix (state 19, KI-026)
  bool complete = false;      // every guest read succeeded
  bool global_warper = false; // r3 == *(0x8286D8D4) + 0x51240
  uint32_t state = 0;         // warper+288
  uint32_t request = 0;       // warper+68
  uint32_t camera = 0;        // r5
  bool source_match = false;          // warper+256 == r4
  bool request_camera_match = false;  // r5 == request+108
  bool camera_copy_match = false;     // r5 == warper+300
  uint8_t req0 = 0;   // request+0 (the state-19 and state-20 waits need it)
  uint8_t req3 = 0;   // request+3 (state 20 needs it set, state 19 clear)
  bool writable = false;  // the three exit addresses are writable
};

enum class FinishAction : uint8_t {
  kPassThrough,        // run the original unchanged, nothing logged as a race
  kFinish,             // perform the state-20 exit, then run the original
  kSkipRequestPending, // matched but the state's own req+0/req+3 wait is not met: pass
  kRefusedUnwritable,  // matched but an exit address is not writable: pass through
  kFinishState19,      // KI-026: perform the state-19 exit, then run the original
};

constexpr const char *FinishActionName(FinishAction action) {
  switch (action) {
    case FinishAction::kPassThrough:
      return "pass";
    case FinishAction::kFinish:
      return "finish-state20";
    case FinishAction::kFinishState19:
      return "finish-cb19";
    case FinishAction::kSkipRequestPending:
      return "skip-request-pending";
    case FinishAction::kRefusedUnwritable:
      return "refused-unwritable";
  }
  return "unknown";
}

// True when the original callback would act on this notification (its own
// three compares) and the camera is also the one the warper still holds.
constexpr bool FinishMatched(const FinishSample &s) {
  return s.request != 0 && s.camera != 0 && s.source_match && s.request_camera_match &&
         s.camera_copy_match;
}

// KI-026 (docs/evidence/M11-ki026-warp-stall-fix-design-20261110.md 3.1):
// state 19 (loc_8221C044) waits for req+3 == 0 (bne at 0x8221C08C), req+0 != 0
// (beq at 0x8221C098) and a ready camera (vfunc +92 at 0x8221C068) before
// it accumulates +260 and exits through the same three stores as state 20
// (0x8221C0F8..0x8221C10C). The camera's finish notification clears ready,
// so a matching callback while the warper is still in 19 means state 19 can
// never exit; the fix performs that exit first, as for state 20.
constexpr FinishAction DecideFinish(const FinishSample &s) {
  if (!s.complete || !s.global_warper || !FinishMatched(s)) {
    return FinishAction::kPassThrough;
  }
  if (s.state == kStateCinematicZoom) {
    if (!s.enabled) {
      return FinishAction::kPassThrough;
    }
    // The state-20 handler waits for req+3 and req+0 before it looks at the
    // camera (0x8221C15C..0x8221C180); never skip that wait.
    if (!s.req0 || !s.req3) {
      return FinishAction::kSkipRequestPending;
    }
    return s.writable ? FinishAction::kFinish : FinishAction::kRefusedUnwritable;
  }
  if (s.state == kStateZoomIn) {
    if (!s.stall_enabled) {
      return FinishAction::kPassThrough;
    }
    if (!s.req0 || s.req3) {
      return FinishAction::kSkipRequestPending;
    }
    return s.writable ? FinishAction::kFinishState19 : FinishAction::kRefusedUnwritable;
  }
  return FinishAction::kPassThrough;
}

// The state-20 compare (0x8221C1B0..0x8221C1C0) with float rounding as the
// generated code does it: exit when float(float(time + dt) + K) >= duration.
constexpr bool State20ExitDue(float time, float dt, float duration) {
  const float sum = static_cast<float>(time + dt);
  return !(static_cast<float>(sum + kExitEpsilon) < duration);
}

// Probe log of every callback entry (--mcla_warp_probe), at most this many
// per process.
inline constexpr uint32_t kMaxFinishNotifyLines = 2000;

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

// --- KI-026 stall detector (mcla_warp_stall_fix) -------------------------------
// docs/evidence/M11-ki026-warp-stall-fix-design-20261110.md 3.2 and
// docs/evidence/M11-ki026-implementation-20261111.md. One sample per
// mcWarper::Update call, taken after the stock update, while the warper is in
// state 19 or 20.
//
// Mode A (completed unseen): the camera was seen ready and in the active
// transition list in this episode, and is now not ready and not listed (the
// post-finish signature: SetFinished clears ready, sub_82221DB0 drops the
// entry) for kStallStuckSamples consecutive samples and kStallStuckSeconds
// of their dt and wall time. Neither state can exit any more (both need
// ready).
//
// Mode B (never started / stalled): the episode outlived the camera duration
// by kStallTimeoutMarginSeconds (kStallTimeoutFallbackSeconds when the camera
// is null or its duration unknown), in dt and in wall time, and the camera
// time has not increased for kStallNoAdvanceSeconds of dt. A camera still in
// the active list is logged only (the abort path sub_8221E310 is not used).
//
// A paused simulation restarts the dt and wall counters. Requests whose flags
// do not match the state's own wait (19: req0 && !req3; 20: req0 && req3)
// are logged once and never acted on.
inline constexpr uint32_t kStallStuckSamples = 3;
inline constexpr float kStallStuckSeconds = 0.1f;
inline constexpr uint64_t kStallStuckMs = 100;
inline constexpr float kStallTimeoutMarginSeconds = 3.0f;
inline constexpr float kStallTimeoutFallbackSeconds = 10.0f;
inline constexpr float kStallNoAdvanceSeconds = 1.0f;
// MCLA_KI026_CAMFIX lines per process (actions, observations and skips).
inline constexpr uint32_t kMaxCamfixLines = 200;

// The state-19 look-ahead: exit when +260 + 0.0001 >= duration - lead with
// lead = *(0x827D7558) * *(0x82006BF0) (0x8221C0C0..0x8221C0F4: lis r9,-32131;
// addi r7,r9,29952; lfs f0,88(r7); lis r8,-32256; lfs f13,27632(r8)).
inline constexpr uint32_t kLeadScaleAddress =
    static_cast<uint32_t>(-32131 * 65536) + 29952u + 88u;
inline constexpr uint32_t kLeadFactorAddress = static_cast<uint32_t>(-32256 * 65536) + 27632u;
static_assert(kLeadScaleAddress == 0x827D7558u);
static_assert(kLeadFactorAddress == 0x82006BF0u);

// The two-slot view override released by the callback (sub_82224A28 on
// UILogic + 0x10D50: lis r11,-32121; lwz r11,-10028(r11); addis r3,r11,1;
// addi r3,r3,3408). sub_82224A28 sets +64, +65, +144, +145 to 1; a slot is
// active while +64 == 0 or +65 == 0 (sub_82224930).
inline constexpr uint32_t kViewOverrideOffset = 65536u + 3408u;
inline constexpr uint32_t kUiLogicPointer =
    static_cast<uint32_t>(-32121 * 65536) + static_cast<uint32_t>(-10028);
static_assert(kViewOverrideOffset == 0x10D50u);
static_assert(kUiLogicPointer == 0x8286D8D4u);

struct StallSample {
  uint64_t now_ms = 0;
  float dt = 0.0f;              // f1 of this mcWarper::Update call
  bool complete = false;        // every guest read succeeded, active list included
  bool global_warper = false;   // warper == *(0x8286D8D4) + 0x51240
  uint32_t state = 0;           // warper+288 after the update
  uint32_t request = 0;         // warper+68
  uint32_t camera = 0;          // warper+296
  uint32_t request_camera = 0;  // request+108
  uint8_t req0 = 0;
  uint8_t req3 = 0;
  bool ready_known = false;
  bool ready = false;
  bool listed = false;          // camera found in the active transition list
  bool time_known = false;
  float time = 0.0f;
  bool duration_known = false;
  float duration = 0.0f;
  bool paused = false;          // simulation pause manager paused
};

enum class StallVerdict : uint8_t {
  kIdle,                    // not a 19/20 episode, or an incomplete sample
  kWatching,                // episode tracked, nothing to report
  kSkipFlagsPending,        // the request flags do not match the state's wait (once)
  kObservedStalledListed,   // mode B bounds met but the camera is still listed (once)
  kModeA,                   // completed unseen: act (once per episode)
  kModeB,                   // never started / stalled, camera unlisted or null: act
  kFired,                   // already acted in this episode
};

constexpr const char *StallVerdictName(StallVerdict verdict) {
  switch (verdict) {
    case StallVerdict::kIdle:
      return "idle";
    case StallVerdict::kWatching:
      return "watching";
    case StallVerdict::kSkipFlagsPending:
      return "skip-flags-pending";
    case StallVerdict::kObservedStalledListed:
      return "observed-stalled-listed";
    case StallVerdict::kModeA:
      return "A";
    case StallVerdict::kModeB:
      return "B";
    case StallVerdict::kFired:
      return "fired";
  }
  return "unknown";
}

constexpr bool StallFlagsMatch(uint32_t state, uint8_t req0, uint8_t req3) {
  return state == kStateZoomIn ? (req0 != 0 && req3 == 0)
                               : (state == kStateCinematicZoom && req0 != 0 && req3 != 0);
}

// The mode B bound in seconds.
constexpr float StallTimeoutSeconds(uint32_t camera, bool duration_known, float duration) {
  if (!camera || !duration_known || !(duration >= 0.0f) || !(duration < 1.0e6f)) {
    return kStallTimeoutFallbackSeconds;
  }
  return duration + kStallTimeoutMarginSeconds;
}

class StallDetector {
 public:
  StallVerdict Observe(const StallSample &s) {
    const bool in_state = s.state == kStateZoomIn || s.state == kStateCinematicZoom;
    if (!s.complete || !s.global_warper || !in_state || !s.request ||
        (s.camera && s.camera != s.request_camera)) {
      active_ = false;
      return StallVerdict::kIdle;
    }
    if (!active_ || s.state != state_ || s.request != request_ || s.camera != camera_) {
      Start(s);
    }
    if (fired_) {
      return StallVerdict::kFired;
    }
    if (!StallFlagsMatch(s.state, s.req0, s.req3)) {
      if (!flags_logged_) {
        flags_logged_ = true;
        return StallVerdict::kSkipFlagsPending;
      }
      return StallVerdict::kWatching;
    }
    if (s.paused) {
      Restart(s.now_ms);
      return StallVerdict::kWatching;
    }
    const float dt = s.dt > 0.0f && s.dt < 1.0e6f ? s.dt : 0.0f;  // NaN and negatives: 0
    episode_dt_ += dt;
    if (s.camera && s.ready_known && s.ready && s.listed) {
      seen_running_ = true;
    }
    if (s.time_known && s.time > max_time_) {  // NaN never compares greater
      max_time_ = s.time;
      last_advance_dt_ = episode_dt_;
    }

    // Mode A.
    const bool finished = seen_running_ && s.camera && s.ready_known && !s.ready && !s.listed;
    if (finished) {
      if (stuck_samples_ == 0) {
        stuck_since_ms_ = s.now_ms;
        stuck_dt_ = 0.0f;
      }
      ++stuck_samples_;
      stuck_dt_ += dt;
    } else {
      stuck_samples_ = 0;
      stuck_dt_ = 0.0f;
    }
    if (finished && stuck_samples_ >= kStallStuckSamples && stuck_dt_ >= kStallStuckSeconds &&
        s.now_ms >= stuck_since_ms_ && s.now_ms - stuck_since_ms_ >= kStallStuckMs) {
      fired_ = true;
      return StallVerdict::kModeA;
    }

    // Mode B. A non-null camera needs a known time to prove it is not
    // advancing; a null camera has no time (state 19/20 treat it as not ready).
    const float bound = StallTimeoutSeconds(s.camera, s.duration_known, s.duration);
    const uint64_t bound_ms = static_cast<uint64_t>(bound * 1000.0f);
    const bool time_ok = !s.camera || s.time_known;
    if (time_ok && episode_dt_ > bound && s.now_ms >= wall_start_ms_ &&
        s.now_ms - wall_start_ms_ > bound_ms &&
        episode_dt_ - last_advance_dt_ >= kStallNoAdvanceSeconds) {
      if (s.camera && s.listed) {
        if (!listed_logged_) {
          listed_logged_ = true;
          return StallVerdict::kObservedStalledListed;
        }
        return StallVerdict::kWatching;
      }
      fired_ = true;
      return StallVerdict::kModeB;
    }
    return StallVerdict::kWatching;
  }

  bool active() const { return active_; }
  bool seen_running() const { return seen_running_; }
  uint32_t stuck_samples() const { return stuck_samples_; }
  float episode_dt() const { return episode_dt_; }
  uint64_t wall_ms(uint64_t now_ms) const {
    return now_ms >= wall_start_ms_ ? now_ms - wall_start_ms_ : 0;
  }

 private:
  void Start(const StallSample &s) {
    active_ = true;
    fired_ = false;
    flags_logged_ = false;
    listed_logged_ = false;
    seen_running_ = false;
    state_ = s.state;
    request_ = s.request;
    camera_ = s.camera;
    max_time_ = s.time_known && s.time == s.time ? s.time : -1.0f;
    Restart(s.now_ms);
  }

  void Restart(uint64_t now_ms) {
    wall_start_ms_ = now_ms;
    episode_dt_ = 0.0f;
    last_advance_dt_ = 0.0f;
    stuck_samples_ = 0;
    stuck_dt_ = 0.0f;
    stuck_since_ms_ = now_ms;
  }

  bool active_ = false;
  bool fired_ = false;
  bool flags_logged_ = false;
  bool listed_logged_ = false;
  bool seen_running_ = false;
  uint32_t state_ = 0;
  uint32_t request_ = 0;
  uint32_t camera_ = 0;
  uint64_t wall_start_ms_ = 0;
  float episode_dt_ = 0.0f;
  float last_advance_dt_ = 0.0f;
  float max_time_ = -1.0f;
  uint32_t stuck_samples_ = 0;
  float stuck_dt_ = 0.0f;
  uint64_t stuck_since_ms_ = 0;
};

// What the hook does with a mode A/B verdict.
enum class StallAction : uint8_t {
  kNone,
  kPollHandler,           // A: exit stores, then the callback (warper+300 still set)
  kPollFinalise,          // A: exit stores, then sub_8221B790 (the callback already ran)
  kTimeoutHandler,        // B: as kPollHandler
  kTimeoutFinalise,       // B: as kPollFinalise
  kObservedFixOff,        // A/B signature with mcla_warp_stall_fix off: log only
  kObservedStalledListed, // B bounds with the camera still listed: log only
  kSkipFlagsPending,      // request flags do not match the state's wait: log only
  kRefusedUnwritable,     // an exit address is not writable: log only
  kSkipCameraMismatch,    // warper+300 is neither the camera nor 0: log only
};

constexpr const char *StallActionName(StallAction action) {
  switch (action) {
    case StallAction::kNone:
      return "none";
    case StallAction::kPollHandler:
      return "poll-handler";
    case StallAction::kPollFinalise:
      return "poll-finalise";
    case StallAction::kTimeoutHandler:
      return "timeout-handler";
    case StallAction::kTimeoutFinalise:
      return "timeout-finalise";
    case StallAction::kObservedFixOff:
      return "observed-fix-off";
    case StallAction::kObservedStalledListed:
      return "observed-stalled-listed";
    case StallAction::kSkipFlagsPending:
      return "skip-flags-pending";
    case StallAction::kRefusedUnwritable:
      return "refused-unwritable";
    case StallAction::kSkipCameraMismatch:
      return "skip-camera-mismatch";
  }
  return "unknown";
}

constexpr bool StallActs(StallAction action) {
  return action == StallAction::kPollHandler || action == StallAction::kPollFinalise ||
         action == StallAction::kTimeoutHandler || action == StallAction::kTimeoutFinalise;
}

constexpr bool StallCallsHandler(StallAction action) {
  return action == StallAction::kPollHandler || action == StallAction::kTimeoutHandler;
}

// camera = warper+296, camera_copy = warper+300 (cleared by the callback when
// it ran for that camera, 0x8221B900..0x8221B924).
constexpr StallAction ResolveStall(StallVerdict verdict, bool fix_on, bool writable,
                                   uint32_t camera, uint32_t camera_copy) {
  switch (verdict) {
    case StallVerdict::kSkipFlagsPending:
      return StallAction::kSkipFlagsPending;
    case StallVerdict::kObservedStalledListed:
      return StallAction::kObservedStalledListed;
    case StallVerdict::kModeA:
    case StallVerdict::kModeB:
      break;
    default:
      return StallAction::kNone;
  }
  if (!fix_on) {
    return StallAction::kObservedFixOff;
  }
  const bool handler = camera != 0 && camera_copy == camera;
  if (!handler && camera_copy != 0) {
    return StallAction::kSkipCameraMismatch;
  }
  if (!writable) {
    return StallAction::kRefusedUnwritable;
  }
  if (verdict == StallVerdict::kModeA) {
    return handler ? StallAction::kPollHandler : StallAction::kPollFinalise;
  }
  return handler ? StallAction::kTimeoutHandler : StallAction::kTimeoutFinalise;
}

// Signature 3 (log only): warper idle (29) with an empty queue, racer active,
// simulation running and a view override slot active, for
// kViewOverrideLimitMs. Reported once per continuous signature.
inline constexpr uint64_t kViewOverrideLimitMs = 20000;

struct ViewOverrideSample {
  uint64_t now_ms = 0;
  bool complete = false;
  uint32_t state = 0;
  uint32_t queue = 0;      // warper+64
  bool racer_active = false;
  bool paused = false;
  uint8_t b64 = 1;
  uint8_t b65 = 1;
};

constexpr bool ViewOverrideActive(uint8_t b64, uint8_t b65) {
  return b64 == 0 || b65 == 0;
}

class ViewOverrideWatch {
 public:
  // True once when the signature has held for kViewOverrideLimitMs.
  bool Observe(const ViewOverrideSample &s) {
    const bool signature = s.complete && s.state == kStateIdle && s.queue == 0 &&
                           s.racer_active && !s.paused && ViewOverrideActive(s.b64, s.b65);
    if (!signature) {
      active_ = false;
      return false;
    }
    if (!active_ || s.now_ms < since_ms_) {
      active_ = true;
      reported_ = false;
      since_ms_ = s.now_ms;
      return false;
    }
    if (reported_ || s.now_ms - since_ms_ < kViewOverrideLimitMs) {
      return false;
    }
    reported_ = true;
    return true;
  }

  uint64_t held_ms(uint64_t now_ms) const {
    return active_ && now_ms >= since_ms_ ? now_ms - since_ms_ : 0;
  }

 private:
  bool active_ = false;
  bool reported_ = false;
  uint64_t since_ms_ = 0;
};

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
