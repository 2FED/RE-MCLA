#include "mcla_warp_probe.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <string_view>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#ifndef MCLA_WARP_GENERATED_RECOMP8
#error "MCLA_WARP_GENERATED_RECOMP8 (path of generated/default/mcla_recomp.8.cpp) is set by CMake"
#endif

namespace {
int checks = 0;
void Check(bool value, const char *message) {
  if (!value) {
    std::fprintf(stderr, "FAIL: %s\n", message);
    std::exit(1);
  }
  ++checks;
}

using mcla::warp::CameraFields;
using mcla::warp::Getter;
using mcla::warp::Sample;
using mcla::warp::Verdict;
using mcla::warp::Watchdog;

constexpr uint32_t kRequest = 0xBED35300u;  // the l67 freeze request
constexpr uint32_t kCamera = 0xBF1F0D90u;   // the l67 freeze camera
constexpr uint64_t kLimit = 15000;

Sample Stuck(uint64_t now_ms, float time) {
  Sample s;
  s.now_ms = now_ms;
  s.complete = true;
  s.state = mcla::warp::kStateCinematicZoom;
  s.request = kRequest;
  s.camera = kCamera;
  s.time_known = true;
  s.time = time;
  return s;
}

void TestGetters() {
  using namespace mcla::warp;
  Check(ReadyGetter(0x8221CE10u) == Getter::kReadyFlagsBit, "ready slot getter");
  Check(ReadyGetter(0x82130000u) == Getter::kUnknown, "blr stub is not a ready getter");
  Check(DurationGetter(0x823ABE90u) == Getter::kDurationField76, "duration +76");
  Check(DurationGetter(0x8221D060u) == Getter::kDurationComposite76, "duration composite");
  Check(DurationGetter(0x8221D130u) == Getter::kDurationSpline76, "duration spline");
  Check(DurationGetter(0x8221CFA0u) == Getter::kUnknown, "time getter is not a duration getter");
  Check(TimeGetter(0x8221CFA0u) == Getter::kTimeField96, "time +96");
  Check(TimeGetter(0x8221D108u) == Getter::kTimeSpline80, "time spline");
  Check(TimeGetter(0) == Getter::kUnknown, "null slot");

  CameraFields f;
  f.flags16 = 0x80;
  f.f76 = 3.0f;
  f.f80 = 1.5f;
  f.f96 = 2.0f;
  bool known = false;
  Check(Ready(Getter::kReadyFlagsBit, f, known) && known, "bit 0x80 set: ready");
  f.flags16 = 0xFFFFFF7Fu;
  Check(!Ready(Getter::kReadyFlagsBit, f, known) && known, "bit 0x80 clear: not ready");
  Check(!Ready(Getter::kUnknown, f, known) && !known, "unknown ready getter");

  Check(Duration(Getter::kDurationField76, f).known &&
            Duration(Getter::kDurationField76, f).value == 3.0f,
        "plain duration");
  Check(Duration(Getter::kDurationComposite76, f).value == 3.0f, "composite duration cached");
  CameraFields lazy = f;
  lazy.f76 = -1.0f;
  Check(!Duration(Getter::kDurationComposite76, lazy).known,
        "composite duration below zero is recomputed by the guest: unknown");
  Check(Duration(Getter::kDurationField76, lazy).known, "plain getter has no lazy path");
  Check(Duration(Getter::kDurationSpline76, f).value == 3.0f, "spline without +471");
  CameraFields spline = f;
  spline.b471 = 1;
  Check(std::fabs(Duration(Getter::kDurationSpline76, spline).value - 1.0f) < 1e-6f,
        "spline with +471: one third");
  Check(Time(Getter::kTimeField96, f).value == 2.0f, "time +96");
  Check(Time(Getter::kTimeSpline80, f).value == 1.5f, "spline time +80");
  Check(std::fabs(Time(Getter::kTimeSpline80, spline).value - 0.5f) < 1e-6f,
        "spline time with +471: one third");
  Check(!Time(Getter::kUnknown, f).known && !Duration(Getter::kTimeField96, f).known,
        "unknown getters stay unknown");
  Check(std::string_view(GetterName(Getter::kTimeSpline80)) == "f80-spline", "getter names");
  Check(std::string_view(TransitionClassName(0x82022FCCu)) == "UITransition_Zoom",
        "class by vtable");
  Check(std::string_view(TransitionClassName(0x82020A8Cu)) == "UITransition", "base class");
  Check(std::string_view(TransitionClassName(0x12345678u)) == "unknown", "unknown vtable");
  static_assert(DurationGetter(0x8221D130u) == Getter::kDurationSpline76);
  static_assert(kExitEpsilon == 0.0001f);
}

void TestWatchdogFiresOnce() {
  Watchdog w;
  Check(w.Observe(Stuck(1000, 0.5f), kLimit) == Verdict::kWatching, "entering state 20");
  Check(w.active() && w.entered_ms() == 1000, "episode started");
  Check(w.Observe(Stuck(15999, 0.5f), kLimit) == Verdict::kWatching, "just under the limit");
  Check(w.Observe(Stuck(16000, 0.5f), kLimit) == Verdict::kFire, "frozen for the limit: fire");
  Check(w.Observe(Stuck(17000, 0.5f), kLimit) == Verdict::kFired, "one shot per episode");
  Check(w.Observe(Stuck(99000, 0.5f), kLimit) == Verdict::kFired, "still one shot");
  // Leaving state 20 ends the episode; a new warp may fire again.
  Sample idle = Stuck(100000, 0.5f);
  idle.state = mcla::warp::kStateIdle;
  Check(w.Observe(idle, kLimit) == Verdict::kIdle && !w.active(), "idle ends the episode");
  Check(w.Observe(Stuck(101000, 0.5f), kLimit) == Verdict::kWatching, "new episode");
  Check(w.Observe(Stuck(116000, 0.5f), kLimit) == Verdict::kFire, "new episode can fire");
}

void TestWatchdogAdvancingTime() {
  Watchdog w;
  w.Observe(Stuck(0, 0.0f), kLimit);
  // A cinematic that advances, however slowly, never fires.
  float t = 0.0f;
  for (uint64_t ms = 33; ms < 120000; ms += 33) {
    t += 0.001f;
    Check(w.Observe(Stuck(ms, t), kLimit) == Verdict::kWatching, "advancing time never fires");
  }
  // It stops advancing: the limit counts from the last increase.
  const uint64_t last = w.frozen_since_ms();
  Check(w.Observe(Stuck(last + 14999, t), kLimit) == Verdict::kWatching, "frozen 14.999 s");
  Check(w.Observe(Stuck(last + 15000, t), kLimit) == Verdict::kFire, "frozen 15 s");
}

void TestWatchdogTimeGoingBackDoesNotReset() {
  Watchdog w;
  w.Observe(Stuck(0, 2.0f), kLimit);
  // A time that oscillates below its maximum is not progress.
  Check(w.Observe(Stuck(5000, 1.0f), kLimit) == Verdict::kWatching, "lower time");
  Check(w.Observe(Stuck(10000, 2.0f), kLimit) == Verdict::kWatching, "back to max");
  Check(w.Observe(Stuck(15000, 1.5f), kLimit) == Verdict::kFire, "no increase over max: fire");
}

void TestWatchdogNeverActsOutsideState20() {
  for (uint32_t state = 0; state <= 30; ++state) {
    if (state == mcla::warp::kStateCinematicZoom) {
      continue;
    }
    Watchdog w;
    for (uint64_t ms = 0; ms <= 60000; ms += 1000) {
      Sample s = Stuck(ms, 0.5f);
      s.state = state;
      Check(w.Observe(s, kLimit) == Verdict::kIdle, "other states are idle");
    }
  }
}

void TestWatchdogIncompleteOrUnknown() {
  {
    Watchdog w;
    w.Observe(Stuck(0, 0.5f), kLimit);
    Sample bad = Stuck(10000, 0.5f);
    bad.complete = false;
    Check(w.Observe(bad, kLimit) == Verdict::kIdle && !w.active(),
          "incomplete sample: idle and episode reset");
    Check(w.Observe(Stuck(20000, 0.5f), kLimit) == Verdict::kWatching,
          "restart after incomplete sample");
    Check(w.Observe(Stuck(34999, 0.5f), kLimit) == Verdict::kWatching,
          "the limit counts from the restart");
    Check(w.Observe(Stuck(35000, 0.5f), kLimit) == Verdict::kFire, "then fires");
  }
  {
    Watchdog w;
    for (uint64_t ms = 0; ms <= 60000; ms += 1000) {
      Sample s = Stuck(ms, 0.5f);
      s.time_known = false;
      Check(w.Observe(s, kLimit) == Verdict::kIdle, "unknown time getter: never acts");
    }
  }
  {
    Watchdog w;
    for (uint64_t ms = 0; ms <= 60000; ms += 1000) {
      Sample s = Stuck(ms, std::numeric_limits<float>::quiet_NaN());
      Check(w.Observe(s, kLimit) == Verdict::kIdle, "NaN time (the guest compare exits): idle");
    }
  }
  {
    Watchdog w;
    for (uint64_t ms = 0; ms <= 60000; ms += 1000) {
      Sample s = Stuck(ms, 0.5f);
      s.camera = 0;
      Check(w.Observe(s, kLimit) == Verdict::kIdle, "no camera: idle");
      s.camera = kCamera;
      s.request = 0;
      Check(w.Observe(s, kLimit) == Verdict::kIdle, "no request: idle");
    }
  }
}

void TestWatchdogPointersAndPause() {
  {
    Watchdog w;
    w.Observe(Stuck(0, 0.5f), kLimit);
    Sample other = Stuck(14000, 0.5f);
    other.request = kRequest + 0x200;
    Check(w.Observe(other, kLimit) == Verdict::kWatching, "new request: new episode");
    Check(w.entered_ms() == 14000, "episode restarted on the request change");
    other.now_ms = 28999;
    Check(w.Observe(other, kLimit) == Verdict::kWatching, "counted from the new request");
    other.camera = kCamera + 0x40;
    other.now_ms = 29000;
    Check(w.Observe(other, kLimit) == Verdict::kWatching, "new camera: new episode");
  }
  {
    Watchdog w;
    w.Observe(Stuck(0, 0.5f), kLimit);
    for (uint64_t ms = 1000; ms <= 60000; ms += 1000) {
      Sample s = Stuck(ms, 0.5f);
      s.paused = true;
      Check(w.Observe(s, kLimit) == Verdict::kWatching, "paused simulation does not count");
    }
    Check(w.Observe(Stuck(74999, 0.5f), kLimit) == Verdict::kWatching, "counted from unpause");
    Check(w.Observe(Stuck(75000, 0.5f), kLimit) == Verdict::kFire, "fires after unpause");
  }
  {
    Watchdog w;
    w.Observe(Stuck(0, 0.5f), 0);
    Check(w.Observe(Stuck(600000, 0.5f), 0) == Verdict::kWatching, "limit 0 never fires");
    Check(w.Observe(Stuck(100000, 0.6f), kLimit) == Verdict::kWatching, "time increased");
    Check(w.Observe(Stuck(5, 0.6f), kLimit) == Verdict::kWatching, "clock going back: no fire");
  }
}

void TestLimit() {
  using mcla::warp::WatchdogLimitMs;
  Check(WatchdogLimitMs(0) == 0 && WatchdogLimitMs(-5) == 0, "0 or less: off");
  Check(WatchdogLimitMs(15) == 15000, "15 s");
  Check(WatchdogLimitMs(1) == 1000, "1 s");
  Check(WatchdogLimitMs(600) == 600000 && WatchdogLimitMs(100000) == 600000, "clamped to 600 s");
  static_assert(mcla::warp::WatchdogLimitMs(15) == 15000);
}

void TestWaitPacer() {
  mcla::warp::WaitPacer p;
  Check(!p.Observe(0, false), "not in state 20");
  Check(!p.Observe(100, true), "entry is covered by the state line");
  Check(!p.Observe(5099, true), "under the interval");
  Check(p.Observe(5100, true), "first wait line after 5 s");
  Check(!p.Observe(10099, true), "interval from the last line");
  Check(p.Observe(10100, true), "second wait line");
  int lines = 2;
  for (uint64_t ms = 10200; ms < 400000; ms += 100) {
    lines += p.Observe(ms, true) ? 1 : 0;
  }
  Check(lines == static_cast<int>(mcla::warp::kMaxWaitLinesPerEpisode),
        "bounded wait lines per episode");
  Check(!p.Observe(400000, false), "leaving state 20");
  Check(!p.Observe(400100, true), "new episode entry");
  Check(p.Observe(405100, true), "new episode logs again");
  Check(!p.Observe(1, true), "clock going back: nothing");
}

void TestRateLimiter() {
  mcla::warp::RateLimiter r(3, 1000);
  Check(r.Allow(0) && r.suppressed() == 0, "first");
  Check(r.Allow(10) && r.Allow(20), "burst of three");
  Check(!r.Allow(30) && !r.Allow(40), "over the burst");
  Check(r.Allow(1000) && r.suppressed() == 2, "new window reports the dropped lines");
  Check(r.Allow(1001) && r.suppressed() == 0, "reset after one report");
  Check(r.Allow(5) && r.suppressed() == 0, "clock going back starts a new window");
  mcla::warp::RateLimiter focus(mcla::warp::kFocusBurst, mcla::warp::kFocusWindowMs);
  int allowed = 0;
  for (uint64_t ms = 0; ms < 60000; ms += 10) {
    allowed += focus.Allow(ms) ? 1 : 0;
  }
  Check(allowed == static_cast<int>(mcla::warp::kFocusBurst), "focus lines per minute");
  Check(focus.Allow(60000) && focus.suppressed() == 6000 - mcla::warp::kFocusBurst,
        "focus suppressed count");
}

// --- KI-035 finish-race fix -------------------------------------------------
using mcla::warp::FinishAction;
using mcla::warp::FinishSample;

constexpr uint32_t kRaceRequest = 0xBED351C0u;  // the l68 freeze request
constexpr uint32_t kRaceCamera = 0xBF1F0BF0u;   // the l68 freeze camera (Parallel)

// The l68 freeze at the moment of the camera's finish notification.
FinishSample Race() {
  FinishSample s;
  s.enabled = true;
  s.complete = true;
  s.global_warper = true;
  s.state = mcla::warp::kStateCinematicZoom;
  s.request = kRaceRequest;
  s.camera = kRaceCamera;
  s.source_match = true;
  s.request_camera_match = true;
  s.camera_copy_match = true;
  s.req0 = 1;
  s.req3 = 1;
  s.writable = true;
  return s;
}

void TestFinishDecision() {
  using mcla::warp::DecideFinish;
  Check(DecideFinish(Race()) == FinishAction::kFinish, "the l68 race: finish state 20");
  {
    FinishSample s = Race();
    s.enabled = false;
    Check(DecideFinish(s) == FinishAction::kPassThrough, "fix off: original unchanged");
  }
  // Every state except 20 passes through: the healthy trip arrives in 29.
  for (uint32_t state = 0; state <= 40; ++state) {
    FinishSample s = Race();
    s.state = state;
    const FinishAction a = DecideFinish(s);
    Check(state == mcla::warp::kStateCinematicZoom ? a == FinishAction::kFinish
                                                   : a == FinishAction::kPassThrough,
          "acts only in state 20");
  }
  {
    FinishSample healthy = Race();
    healthy.state = mcla::warp::kStateIdle;  // handler already exited (+276 = 1)
    Check(DecideFinish(healthy) == FinishAction::kPassThrough, "healthy trip: no action");
  }
  // Each of the matches is required (all combinations of the three flags).
  for (int mask = 0; mask < 8; ++mask) {
    FinishSample s = Race();
    s.source_match = (mask & 1) != 0;
    s.request_camera_match = (mask & 2) != 0;
    s.camera_copy_match = (mask & 4) != 0;
    Check((DecideFinish(s) == FinishAction::kFinish) == (mask == 7),
          "finish only when r4, request+108 and warper+300 all match");
    Check(mcla::warp::FinishMatched(s) == (mask == 7), "matched iff all three");
  }
  {
    FinishSample s = Race();
    s.request = 0;
    Check(DecideFinish(s) == FinishAction::kPassThrough, "no request: pass");
    s = Race();
    s.camera = 0;
    Check(DecideFinish(s) == FinishAction::kPassThrough, "null camera: pass");
    s = Race();
    s.complete = false;
    Check(DecideFinish(s) == FinishAction::kPassThrough, "incomplete reads: pass");
    s = Race();
    s.global_warper = false;
    Check(DecideFinish(s) == FinishAction::kPassThrough, "not the UILogic warper: pass");
  }
  {
    // The handler itself waits for req+0 and req+3 first; never skip that.
    FinishSample s = Race();
    s.req0 = 0;
    Check(DecideFinish(s) == FinishAction::kSkipRequestPending, "req+0 clear: skip");
    s = Race();
    s.req3 = 0;
    Check(DecideFinish(s) == FinishAction::kSkipRequestPending, "req+3 clear: skip");
    s.writable = false;
    Check(DecideFinish(s) == FinishAction::kSkipRequestPending, "pending wins over writable");
    s.enabled = false;
    Check(DecideFinish(s) == FinishAction::kPassThrough, "fix off: pass, even pending");
  }
  {
    FinishSample s = Race();
    s.writable = false;
    Check(DecideFinish(s) == FinishAction::kRefusedUnwritable, "unwritable: refused");
  }
  Check(std::string_view(mcla::warp::FinishActionName(FinishAction::kFinish)) ==
            "finish-state20",
        "action names");
  Check(std::string_view(mcla::warp::FinishActionName(FinishAction::kSkipRequestPending)) ==
            "skip-request-pending",
        "skip name");
  static_assert(mcla::warp::DecideFinish(FinishSample{}) == FinishAction::kPassThrough);
}

void TestState20ExitDue() {
  using mcla::warp::State20ExitDue;
  // The healthy l68 trip left state 20 at time 3.0210 of 3.0260 (dt ~ 5 ms
  // was enough); the frozen one never reached the compare with a ready camera.
  Check(State20ExitDue(3.0210f, 0.0050f, 3.0260f), "healthy exit");
  Check(!State20ExitDue(3.0000f, 0.0160f, 3.0450f), "one frame short: wait");
  Check(State20ExitDue(3.0290f, 0.0160f, 3.0450f), "time + dt reaches duration");
  Check(State20ExitDue(3.04495f, 0.0f, 3.0450f), "epsilon covers rounding");
  Check(State20ExitDue(std::numeric_limits<float>::quiet_NaN(), 0.016f, 3.0f),
        "NaN compares unordered: blt not taken, exit (as the guest)");
  // The race: T_N + dt_N + K < D <= T_N + dt_(N+1).
  const float d = 3.045f;
  const float t = 3.010f;
  Check(!State20ExitDue(t, 0.016f, d) && t + 0.050f >= d,
        "a larger next dt finishes the camera before the warper can exit");
}

// Reads the generated source the hook mirrors and checks every address and
// offset the fix uses against it (MCLA_WARP_GENERATED_RECOMP8, CMake).
std::string ReadFile(const char *path) {
  std::ifstream in(path, std::ios::binary);
  std::ostringstream text;
  text << in.rdbuf();
  return text.str();
}

std::string Body(const std::string &text, const std::string &name) {
  const std::string head = "DEFINE_REX_FUNC(" + name + ") {";
  const size_t begin = text.find(head);
  if (begin == std::string::npos) {
    return {};
  }
  const size_t end = text.find("DEFINE_REX_FUNC(", begin + head.size());
  return text.substr(begin, end == std::string::npos ? std::string::npos : end - begin);
}

// Each line in order, starting at `from`; returns the position after the last.
size_t InOrder(const std::string &body, size_t from, const std::vector<std::string> &lines,
               const char *what) {
  for (const auto &line : lines) {
    const size_t at = body.find(line, from);
    if (at == std::string::npos) {
      std::fprintf(stderr, "missing in %s: %s\n", what, line.c_str());
      Check(false, what);
    }
    from = at + line.size();
  }
  Check(true, what);
  return from;
}

std::string Fmt(const char *format, long long value) {
  char text[160];
  std::snprintf(text, sizeof(text), format, value);
  return text;
}

void TestGeneratedMirror() {
  using namespace mcla::warp;
  const std::string text = ReadFile(MCLA_WARP_GENERATED_RECOMP8);
  Check(!text.empty(), "generated mcla_recomp.8.cpp readable");

  // The callback: the three compares the original makes, the +300 clear, the
  // +276 finalise test and the +273 store.
  const std::string callback = Body(text, "sub_8221B8C8");
  Check(!callback.empty(), "sub_8221B8C8 present");
  Check(callback.find(Fmt("REX_FUNC_PROLOGUE(0x%llX);", kFinishCallback)) != std::string::npos,
        "callback address");
  InOrder(callback, 0,
          {"ctx.r31.u64 = ctx.r3.u64;",
           Fmt("ctx.r11.u64 = REX_LOAD_U32(ctx.r31.u32 + %lld);", kWarperEventSource),
           "ctx.cr6.compare<uint32_t>(ctx.r11.u32, ctx.r4.u32, ctx.xer);",
           "if (!ctx.cr6.eq) goto loc_8221B9A0;",
           Fmt("ctx.r11.u64 = REX_LOAD_U32(ctx.r31.u32 + %lld);", kWarperRequestField),
           "ctx.cr6.compare<uint32_t>(ctx.r11.u32, 0, ctx.xer);",
           "if (ctx.cr6.eq) goto loc_8221B9A0;",
           Fmt("ctx.r11.u64 = REX_LOAD_U32(ctx.r11.u32 + %lld);", kRequestCameraField),
           "ctx.cr6.compare<uint32_t>(ctx.r5.u32, ctx.r11.u32, ctx.xer);",
           "if (!ctx.cr6.eq) goto loc_8221B9A0;",
           Fmt("ctx.r11.u64 = REX_LOAD_U32(ctx.r31.u32 + %lld);", kWarperCameraCopyField),
           "ctx.cr6.compare<uint32_t>(ctx.r5.u32, ctx.r11.u32, ctx.xer);",
           Fmt("REX_STORE_U32(ctx.r31.u32 + %lld, ctx.r10.u32);", kWarperCameraCopyField),
           Fmt("ctx.r11.u64 = REX_LOAD_U8(ctx.r31.u32 + %lld);", kWarperFinishFlagField),
           "if (ctx.cr6.eq) goto loc_8221B998;",
           "sub_8221B790(ctx, base);",
           Fmt("REX_STORE_U8(ctx.r31.u32 + %lld, ctx.r11.u8);", kWarperNotifiedField)},
          "callback compares and stores");

  // The warper update: r28 = 1 and r26 = 0 in the prologue, and the state-20
  // wait and exit at loc_8221C120.
  const std::string update = Body(text, "sub_8221BE98");
  Check(!update.empty(), "sub_8221BE98 present");
  const size_t first_label = update.find("loc_8221");
  const size_t prologue = InOrder(update, 0,
                                  {"ctx.f31.f64 = ctx.f1.f64;", "ctx.r28.s64 = 1;",
                                   "ctx.r26.s64 = 0;"},
                                  "update prologue");
  Check(prologue < first_label, "r28/r26 set before any state branch");
  const size_t state20 = update.find("loc_8221C120:\n");
  const size_t state20_crlf = update.find("loc_8221C120:\r\n");
  const size_t start = state20 != std::string::npos ? state20 : state20_crlf;
  Check(start != std::string::npos, "state-20 label");
  const size_t next = update.find("loc_8221C1F0:", start);
  Check(next != std::string::npos, "state-26 label follows");
  const std::string handler = update.substr(start, next - start);
  const long long lis = static_cast<long long>(static_cast<int32_t>(
      (kExitGlobalPointer + 10000u) & 0xFFFF0000u));
  InOrder(handler, 0,
          {Fmt("ctx.r11.u64 = REX_LOAD_U32(ctx.r31.u32 + %lld);", kWarperRequestField),
           Fmt("ctx.r3.u64 = REX_LOAD_U32(ctx.r31.u32 + %lld);", kWarperCameraField),
           "ctx.r30.u64 = REX_LOAD_U8(ctx.r11.u32 + 3);",
           "ctx.r29.u64 = REX_LOAD_U8(ctx.r11.u32 + 0);",
           Fmt("ctx.r10.u64 = REX_LOAD_U32(ctx.r11.u32 + %lld);", kSlotReady),
           Fmt("ctx.r10.u64 = REX_LOAD_U32(ctx.r11.u32 + %lld);", kSlotDuration),
           Fmt("ctx.r8.u64 = REX_LOAD_U32(ctx.r9.u32 + %lld);", kSlotTime),
           "ctx.f13.f64 = double(float(ctx.f1.f64 + ctx.f31.f64));",
           "temp.u32 = REX_LOAD_U32(ctx.r7.u32 + 18612);",
           "ctx.f12.f64 = double(float(ctx.f13.f64 + ctx.f0.f64));",
           "ctx.cr6.compare(ctx.f12.f64, ctx.f30.f64);",
           "if (ctx.cr6.lt) goto loc_8221CB90;",
           Fmt("ctx.r11.s64 = %lld;", lis),
           Fmt("ctx.r10.s64 = %lld;", kExitStateValue),
           "ctx.r11.u64 = REX_LOAD_U32(ctx.r11.u32 + -10000);",
           Fmt("REX_STORE_U8(ctx.r11.u32 + %lld, ctx.r26.u8);", kExitGlobalByteField),
           Fmt("REX_STORE_U8(ctx.r31.u32 + %lld, ctx.r28.u8);", kWarperFinishFlagField),
           Fmt("REX_STORE_U32(ctx.r31.u32 + %lld, ctx.r10.u32);", kWarperStateField)},
          "state-20 wait and exit stores");
  Check(static_cast<uint32_t>(lis) + static_cast<uint32_t>(-10000) == kExitGlobalPointer,
        "lis/lwz pair gives 0x8286D8F0");
  Check(0x82000000u + 18612u == 0x820048B4u, "epsilon address");
  Check(kExitGlobalByteValue == 0 && kExitFinishFlagValue == 1 && kExitStateValue == 29,
        "exit values r26 = 0, r28 = 1, 29");
}
}  // namespace

int main() {
  TestGetters();
  TestWatchdogFiresOnce();
  TestWatchdogAdvancingTime();
  TestWatchdogTimeGoingBackDoesNotReset();
  TestWatchdogNeverActsOutsideState20();
  TestWatchdogIncompleteOrUnknown();
  TestWatchdogPointersAndPause();
  TestLimit();
  TestWaitPacer();
  TestRateLimiter();
  TestFinishDecision();
  TestState20ExitDue();
  TestGeneratedMirror();
  std::printf("mcla_warp_probe_test: %d checks passed\n", checks);
  return 0;
}
