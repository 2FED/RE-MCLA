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

// --- KI-026 stall fix -----------------------------------------------------------
using mcla::warp::StallAction;
using mcla::warp::StallDetector;
using mcla::warp::StallSample;
using mcla::warp::StallVerdict;

FinishSample Race19() {
  FinishSample s = Race();
  s.stall_enabled = true;
  s.state = mcla::warp::kStateZoomIn;
  s.req3 = 0;  // state 26 picks 19 exactly when req+3 == 0
  return s;
}

void TestFinishState19() {
  using mcla::warp::DecideFinish;
  Check(DecideFinish(Race19()) == FinishAction::kFinishState19,
        "callback in state 19 for the warper's own camera: finish state 19");
  {
    FinishSample s = Race19();
    s.stall_enabled = false;
    Check(DecideFinish(s) == FinishAction::kPassThrough, "stall fix off: original unchanged");
    s.enabled = false;
    Check(DecideFinish(s) == FinishAction::kPassThrough, "both fixes off: pass");
    s = Race19();
    s.enabled = false;
    Check(DecideFinish(s) == FinishAction::kFinishState19,
          "the KI-035 switch does not gate state 19");
  }
  {
    FinishSample s = Race();
    s.stall_enabled = true;
    Check(DecideFinish(s) == FinishAction::kFinish, "state 20 unchanged with the stall fix on");
    s.enabled = false;
    Check(DecideFinish(s) == FinishAction::kPassThrough,
          "the stall fix does not take over the state-20 callback");
  }
  for (uint32_t state = 0; state <= 40; ++state) {
    FinishSample s = Race19();
    s.enabled = true;
    s.state = state;
    const FinishAction a = DecideFinish(s);
    const bool expected = state == mcla::warp::kStateZoomIn
                              ? a == FinishAction::kFinishState19
                              : (state == mcla::warp::kStateCinematicZoom
                                     ? a == FinishAction::kSkipRequestPending
                                     : a == FinishAction::kPassThrough);
    Check(expected, "state 19 finishes, state 20 with req+3 clear waits, others pass");
  }
  {
    FinishSample healthy = Race19();
    healthy.state = mcla::warp::kStateIdle;  // healthy 19 exits while the camera is listed
    Check(DecideFinish(healthy) == FinishAction::kPassThrough, "healthy state-19 trip: pass");
  }
  for (int mask = 0; mask < 8; ++mask) {
    FinishSample s = Race19();
    s.source_match = (mask & 1) != 0;
    s.request_camera_match = (mask & 2) != 0;
    s.camera_copy_match = (mask & 4) != 0;
    Check((DecideFinish(s) == FinishAction::kFinishState19) == (mask == 7),
          "state 19 needs r4, request+108 and warper+300 to match");
  }
  {
    FinishSample s = Race19();
    s.req3 = 1;
    Check(DecideFinish(s) == FinishAction::kSkipRequestPending, "state 19 with req+3 set: skip");
    s = Race19();
    s.req0 = 0;
    Check(DecideFinish(s) == FinishAction::kSkipRequestPending, "state 19 with req+0 clear");
    s.writable = false;
    Check(DecideFinish(s) == FinishAction::kSkipRequestPending, "pending wins over writable");
    s = Race19();
    s.writable = false;
    Check(DecideFinish(s) == FinishAction::kRefusedUnwritable, "state 19 unwritable: refused");
    s = Race19();
    s.complete = false;
    Check(DecideFinish(s) == FinishAction::kPassThrough, "state 19 incomplete reads: pass");
    s = Race19();
    s.global_warper = false;
    Check(DecideFinish(s) == FinishAction::kPassThrough, "state 19 other warper: pass");
    s = Race19();
    s.request = 0;
    Check(DecideFinish(s) == FinishAction::kPassThrough, "state 19 no request: pass");
  }
  Check(std::string_view(mcla::warp::FinishActionName(FinishAction::kFinishState19)) ==
            "finish-cb19",
        "state-19 action name");
  static_assert(mcla::warp::kStateZoomIn == 19);
}

constexpr uint32_t kStallRequest = 0xBED35400u;
constexpr uint32_t kStallCamera = 0xBF1F0E00u;

// A running zoom camera (ready, listed) at the given time of a 3 s duration.
StallSample Running(uint32_t state, uint64_t now_ms, float dt, float time) {
  StallSample s;
  s.now_ms = now_ms;
  s.dt = dt;
  s.complete = true;
  s.global_warper = true;
  s.state = state;
  s.request = kStallRequest;
  s.camera = kStallCamera;
  s.request_camera = kStallCamera;
  s.req0 = 1;
  s.req3 = state == mcla::warp::kStateCinematicZoom ? 1 : 0;
  s.ready_known = true;
  s.ready = true;
  s.listed = true;
  s.time_known = true;
  s.time = time;
  s.duration_known = true;
  s.duration = 3.0f;
  return s;
}

// The post-finish signature: not ready, unlisted, time reset.
StallSample Finished(uint32_t state, uint64_t now_ms, float dt) {
  StallSample s = Running(state, now_ms, dt, 0.0f);
  s.ready = false;
  s.listed = false;
  return s;
}

bool Quiet(StallVerdict v) {
  return v == StallVerdict::kIdle || v == StallVerdict::kWatching;
}

void TestStallHealthyTrips() {
  // Healthy state 20 (l68: left 20 at 3.021 of 3.026, one frame before the
  // camera ends): never a verdict at 60, 144 or 240 Hz.
  for (const uint64_t step_ms : {16ull, 7ull, 4ull}) {
    StallDetector d;
    const float dt = static_cast<float>(step_ms) / 1000.0f;
    uint64_t ms = 1000;
    float t = 0.0f;
    for (; t + dt < 3.0f; t += dt, ms += step_ms) {
      Check(Quiet(d.Observe(Running(mcla::warp::kStateCinematicZoom, ms, dt, t))),
            "healthy state 20 is quiet");
    }
    Check(d.seen_running(), "the healthy camera was seen running");
    StallSample idle = Finished(mcla::warp::kStateIdle, ms + step_ms, dt);
    Check(d.Observe(idle) == StallVerdict::kIdle && !d.active(), "29 ends the episode");
  }
  {
    // Healthy state 19 (l68: exits at 0.881/3.0 with the camera listed).
    StallDetector d;
    uint64_t ms = 0;
    for (float t = 0.0f; t < 0.881f; t += 0.016f, ms += 16) {
      Check(Quiet(d.Observe(Running(mcla::warp::kStateZoomIn, ms, 0.016f, t))),
            "healthy state 19 is quiet");
    }
    Check(d.Observe(Finished(mcla::warp::kStateIdle, ms, 0.016f)) == StallVerdict::kIdle,
          "state 19 left while listed: idle");
  }
  {
    // sub_82219DA0 starts the camera in the same update that enters 19.
    StallDetector d;
    Check(d.Observe(Running(mcla::warp::kStateZoomIn, 0, 0.016f, 0.0f)) ==
                  StallVerdict::kWatching &&
              d.seen_running() && d.active(),
          "camera started in the same call: seen running from the first sample");
  }
  {
    // A camera that only becomes ready later is not a stall while it starts.
    StallDetector d;
    StallSample s = Finished(mcla::warp::kStateZoomIn, 0, 0.016f);
    for (uint64_t ms = 0; ms < 2000; ms += 16) {
      s.now_ms = ms;
      Check(Quiet(d.Observe(s)), "not yet running: no mode A");
    }
    Check(!d.seen_running(), "never seen running");
    Check(Quiet(d.Observe(Running(mcla::warp::kStateZoomIn, 2016, 0.016f, 0.016f))),
          "starts running");
    Check(d.seen_running(), "now seen running");
  }
}

void TestStallModeA() {
  for (const uint32_t state : {mcla::warp::kStateZoomIn, mcla::warp::kStateCinematicZoom}) {
    StallDetector d;
    uint64_t ms = 0;
    for (float t = 0.0f; t < 1.0f; t += 0.025f, ms += 25) {
      d.Observe(Running(state, ms, 0.025f, t));
    }
    // 25 ms frames: samples 1-4 are under 100 ms of wall time, sample 5
    // reaches both 0.1 s of dt and 100 ms.
    for (int n = 1; n <= 4; ++n, ms += 25) {
      Check(d.Observe(Finished(state, ms, 0.025f)) == StallVerdict::kWatching,
            "mode A waits for 3 samples and 100 ms");
    }
    Check(d.Observe(Finished(state, ms, 0.025f)) == StallVerdict::kModeA,
          "mode A after the camera finished unseen");
    Check(d.Observe(Finished(state, ms + 25, 0.025f)) == StallVerdict::kFired,
          "once per episode");
  }
  {
    // 240 Hz: three samples are not enough, 0.1 s of dt is needed.
    StallDetector d;
    uint64_t ms = 0;
    for (int i = 0; i < 100; ++i, ms += 4) {
      d.Observe(Running(mcla::warp::kStateZoomIn, ms, 0.004f, 0.004f * i));
    }
    int fired_at = 0;
    for (int n = 1; n <= 40 && !fired_at; ++n, ms += 4) {
      if (d.Observe(Finished(mcla::warp::kStateZoomIn, ms, 0.004f)) == StallVerdict::kModeA) {
        fired_at = n;
      }
    }
    Check(fired_at >= 25 && fired_at <= 27, "240 Hz: mode A after about 0.1 s");
  }
  {
    // 2 stuck samples followed by a listed one (a list reshuffle): reset.
    StallDetector d;
    d.Observe(Running(mcla::warp::kStateZoomIn, 0, 0.05f, 0.0f));
    Check(Quiet(d.Observe(Finished(mcla::warp::kStateZoomIn, 50, 0.05f))), "stuck 1");
    Check(Quiet(d.Observe(Finished(mcla::warp::kStateZoomIn, 100, 0.05f))), "stuck 2");
    Check(Quiet(d.Observe(Running(mcla::warp::kStateZoomIn, 150, 0.05f, 0.15f))), "listed");
    Check(Quiet(d.Observe(Finished(mcla::warp::kStateZoomIn, 200, 0.05f))), "restart 1");
    Check(Quiet(d.Observe(Finished(mcla::warp::kStateZoomIn, 250, 0.05f))), "restart 2");
    Check(d.Observe(Finished(mcla::warp::kStateZoomIn, 300, 0.05f)) == StallVerdict::kModeA,
          "fires after 3 consecutive samples and 100 ms");
  }
  {
    // Not ready but still listed, ready unknown, or listed and not ready:
    // never mode A.
    StallDetector d;
    d.Observe(Running(mcla::warp::kStateZoomIn, 0, 0.05f, 0.0f));
    for (uint64_t ms = 50; ms < 2000; ms += 50) {
      StallSample s = Finished(mcla::warp::kStateZoomIn, ms, 0.05f);
      s.listed = true;
      Check(Quiet(d.Observe(s)), "still listed: no mode A");
    }
    StallDetector u;
    u.Observe(Running(mcla::warp::kStateZoomIn, 0, 0.05f, 0.0f));
    for (uint64_t ms = 50; ms < 2000; ms += 50) {
      StallSample s = Finished(mcla::warp::kStateZoomIn, ms, 0.05f);
      s.ready_known = false;
      Check(Quiet(u.Observe(s)), "ready unknown: no mode A");
    }
  }
  {
    // A paused simulation does not count.
    StallDetector d;
    d.Observe(Running(mcla::warp::kStateZoomIn, 0, 0.05f, 0.0f));
    for (uint64_t ms = 50; ms < 3000; ms += 50) {
      StallSample s = Finished(mcla::warp::kStateZoomIn, ms, 0.05f);
      s.paused = true;
      Check(d.Observe(s) == StallVerdict::kWatching, "paused: no mode A");
    }
    Check(Quiet(d.Observe(Finished(mcla::warp::kStateZoomIn, 3000, 0.05f))), "unpaused 1");
    Check(Quiet(d.Observe(Finished(mcla::warp::kStateZoomIn, 3050, 0.05f))), "unpaused 2");
    Check(d.Observe(Finished(mcla::warp::kStateZoomIn, 3100, 0.05f)) == StallVerdict::kModeA,
          "counted from the unpause");
  }
}

void TestStallModeB() {
  {
    // Never started: camera not ready, unlisted, time 0, duration 3 s; the
    // bound is 6 s of dt and of wall time.
    StallDetector d;
    StallVerdict v = StallVerdict::kWatching;
    uint64_t ms = 10000;
    for (; ms <= 15900; ms += 50) {
      v = d.Observe(Finished(mcla::warp::kStateZoomIn, ms, 0.05f));
      Check(Quiet(v), "never started: waits for duration + 3 s");
    }
    for (; ms <= 16200 && v != StallVerdict::kModeB; ms += 50) {
      v = d.Observe(Finished(mcla::warp::kStateZoomIn, ms, 0.05f));
    }
    Check(v == StallVerdict::kModeB, "never started: mode B after duration + 3 s");
    Check(d.Observe(Finished(mcla::warp::kStateZoomIn, ms, 0.05f)) == StallVerdict::kFired,
          "mode B once per episode");
  }
  {
    // Stalled but still listed: log once, then act only once it is unlisted.
    StallDetector d;
    int listed_lines = 0;
    uint64_t ms = 0;
    for (; ms <= 20000; ms += 50) {
      StallSample s = Finished(mcla::warp::kStateZoomIn, ms, 0.05f);
      s.listed = true;
      const StallVerdict v = d.Observe(s);
      listed_lines += v == StallVerdict::kObservedStalledListed ? 1 : 0;
      Check(v == StallVerdict::kWatching || v == StallVerdict::kObservedStalledListed,
            "listed and stalled: log only");
    }
    Check(listed_lines == 1, "observed-stalled-listed once per episode");
    Check(d.Observe(Finished(mcla::warp::kStateZoomIn, ms, 0.05f)) == StallVerdict::kModeB,
          "unlisted later: mode B");
  }
  {
    // Null camera: 10 s fallback bound.
    auto null_camera = [](uint64_t ms) {
      StallSample s = Finished(mcla::warp::kStateZoomIn, ms, 0.05f);
      s.camera = 0;
      s.request_camera = 0;
      s.ready_known = false;
      s.listed = false;
      s.time_known = false;
      s.duration_known = false;
      return s;
    };
    StallDetector d;
    StallVerdict v = StallVerdict::kWatching;
    uint64_t ms = 0;
    for (; ms <= 9900; ms += 50) {
      v = d.Observe(null_camera(ms));
      Check(Quiet(v), "null camera: waits 10 s");
    }
    for (; ms <= 10300 && v != StallVerdict::kModeB; ms += 50) {
      v = d.Observe(null_camera(ms));
    }
    Check(v == StallVerdict::kModeB, "null camera: mode B after 10 s");
  }
  {
    // Advancing camera time never fires; it stops at 8 s: 1 s more.
    StallDetector d;
    uint64_t ms = 0;
    float t = 0.0f;
    for (; ms < 8000; ms += 50, t += 0.05f) {
      StallSample s = Finished(mcla::warp::kStateZoomIn, ms, 0.05f);
      s.time = t;
      Check(Quiet(d.Observe(s)), "advancing time: no mode B");
    }
    StallVerdict v = StallVerdict::kWatching;
    uint64_t fired_ms = 0;
    for (; ms < 12000 && v != StallVerdict::kModeB; ms += 50) {
      StallSample s = Finished(mcla::warp::kStateZoomIn, ms, 0.05f);
      s.time = t;
      v = d.Observe(s);
      fired_ms = ms;
    }
    Check(v == StallVerdict::kModeB && fired_ms >= 8900 && fired_ms <= 9100,
          "mode B 1 s after the camera time stopped");
  }
  {
    // A host stall (one huge dt) does not beat the wall-clock floor.
    StallDetector d;
    d.Observe(Finished(mcla::warp::kStateZoomIn, 0, 0.016f));
    Check(Quiet(d.Observe(Finished(mcla::warp::kStateZoomIn, 100, 30.0f))),
          "30 s dt in 100 ms of wall time: no mode B");
    Check(Quiet(d.Observe(Finished(mcla::warp::kStateZoomIn, 5900, 0.016f))),
          "still under 6 s of wall time");
    Check(d.Observe(Finished(mcla::warp::kStateZoomIn, 6100, 0.016f)) == StallVerdict::kModeB,
          "both bounds met");
    // And a long wall time with little dt (a slowed game) does not either.
    StallDetector slow;
    for (uint64_t ms = 0; ms < 60000; ms += 100) {
      Check(Quiet(slow.Observe(Finished(mcla::warp::kStateZoomIn, ms, 0.001f))),
            "60 s of wall time but 0.6 s of dt: no mode B");
    }
  }
  {
    // A non-null camera with an unknown time getter is never mode B.
    StallDetector d;
    for (uint64_t ms = 0; ms < 60000; ms += 50) {
      StallSample s = Finished(mcla::warp::kStateZoomIn, ms, 0.05f);
      s.time_known = false;
      Check(Quiet(d.Observe(s)), "unknown time: no mode B");
    }
    // Unknown duration: the 10 s fallback.
    StallDetector u;
    StallVerdict v = StallVerdict::kWatching;
    uint64_t ms = 0;
    for (; ms <= 9900; ms += 50) {
      StallSample s = Finished(mcla::warp::kStateZoomIn, ms, 0.05f);
      s.duration_known = false;
      v = u.Observe(s);
      Check(Quiet(v), "unknown duration: waits 10 s");
    }
    for (; ms <= 10300 && v != StallVerdict::kModeB; ms += 50) {
      StallSample s = Finished(mcla::warp::kStateZoomIn, ms, 0.05f);
      s.duration_known = false;
      v = u.Observe(s);
    }
    Check(v == StallVerdict::kModeB, "unknown duration: mode B after 10 s");
  }
  {
    // A pause restarts the dt and wall counters.
    StallDetector d;
    uint64_t ms = 0;
    for (; ms <= 5000; ms += 50) {
      d.Observe(Finished(mcla::warp::kStateZoomIn, ms, 0.05f));
    }
    StallSample p = Finished(mcla::warp::kStateZoomIn, ms, 0.05f);
    p.paused = true;
    Check(d.Observe(p) == StallVerdict::kWatching, "paused");
    for (ms += 50; ms <= 10900; ms += 50) {
      Check(Quiet(d.Observe(Finished(mcla::warp::kStateZoomIn, ms, 0.05f))),
            "counted from the pause");
    }
  }
  using mcla::warp::StallTimeoutSeconds;
  Check(StallTimeoutSeconds(kStallCamera, true, 3.0f) == 6.0f, "duration + 3 s");
  Check(StallTimeoutSeconds(0, true, 3.0f) == 10.0f, "null camera: 10 s");
  Check(StallTimeoutSeconds(kStallCamera, false, 3.0f) == 10.0f, "unknown duration: 10 s");
  Check(StallTimeoutSeconds(kStallCamera, true, -1.0f) == 10.0f, "negative duration: 10 s");
  Check(StallTimeoutSeconds(kStallCamera, true, std::numeric_limits<float>::quiet_NaN()) ==
            10.0f,
        "NaN duration: 10 s");
}

void TestStallPreconditions() {
  {
    // Flags that do not match the state's own wait: logged once, never acted.
    StallDetector d;
    int skips = 0;
    d.Observe(Running(mcla::warp::kStateZoomIn, 0, 0.05f, 0.0f));
    for (uint64_t ms = 0; ms < 30000; ms += 50) {
      StallSample s = Finished(mcla::warp::kStateZoomIn, ms, 0.05f);
      s.req3 = 1;
      const StallVerdict v = d.Observe(s);
      skips += v == StallVerdict::kSkipFlagsPending ? 1 : 0;
      Check(v == StallVerdict::kWatching || v == StallVerdict::kSkipFlagsPending,
            "flags pending: never acts");
    }
    Check(skips == 1, "skip-flags-pending once per episode");
    StallDetector z;
    for (uint64_t ms = 0; ms < 30000; ms += 50) {
      StallSample s = Finished(mcla::warp::kStateCinematicZoom, ms, 0.05f);
      s.req3 = 0;
      const StallVerdict v = z.Observe(s);
      Check(v == StallVerdict::kWatching || v == StallVerdict::kSkipFlagsPending,
            "state 20 with req+3 clear: never acts");
    }
  }
  Check(mcla::warp::StallFlagsMatch(19, 1, 0) && !mcla::warp::StallFlagsMatch(19, 1, 1) &&
            !mcla::warp::StallFlagsMatch(19, 0, 0) && mcla::warp::StallFlagsMatch(20, 1, 1) &&
            !mcla::warp::StallFlagsMatch(20, 1, 0) && !mcla::warp::StallFlagsMatch(29, 1, 1),
        "the states' own request waits");
  for (uint32_t state = 0; state <= 40; ++state) {
    if (state == mcla::warp::kStateZoomIn || state == mcla::warp::kStateCinematicZoom) {
      continue;
    }
    StallDetector d;
    for (uint64_t ms = 0; ms < 30000; ms += 50) {
      StallSample s = Finished(state, ms, 0.05f);
      Check(d.Observe(s) == StallVerdict::kIdle, "other states are idle");
    }
  }
  {
    StallDetector d;
    for (uint64_t ms = 0; ms < 30000; ms += 50) {
      StallSample s = Finished(mcla::warp::kStateZoomIn, ms, 0.05f);
      s.request_camera = kStallCamera + 0x40;
      Check(d.Observe(s) == StallVerdict::kIdle, "warper camera != request+108: idle");
      s = Finished(mcla::warp::kStateZoomIn, ms, 0.05f);
      s.complete = (ms / 50) % 7 != 0;
      Check(Quiet(d.Observe(s)), "incomplete samples keep restarting the episode");
      s = Finished(mcla::warp::kStateZoomIn, ms, 0.05f);
      s.global_warper = false;
      Check(d.Observe(s) == StallVerdict::kIdle, "other warper: idle");
      s.global_warper = true;
      s.request = 0;
      Check(d.Observe(s) == StallVerdict::kIdle, "no request: idle");
    }
  }
  {
    // A new request or camera starts a new episode (seen_running resets).
    StallDetector d;
    d.Observe(Running(mcla::warp::kStateZoomIn, 0, 0.05f, 0.0f));
    StallSample s = Finished(mcla::warp::kStateZoomIn, 50, 0.05f);
    s.request = kStallRequest + 0x200;
    Check(d.Observe(s) == StallVerdict::kWatching && !d.seen_running(),
          "new request: new episode, not seen running");
    for (uint64_t ms = 100; ms < 3000; ms += 50) {
      s.now_ms = ms;
      Check(Quiet(d.Observe(s)), "the new episode never ran: no mode A");
    }
  }
}

void TestStallResolve() {
  using mcla::warp::ResolveStall;
  constexpr uint32_t c = kStallCamera;
  Check(ResolveStall(StallVerdict::kModeA, true, true, c, c) == StallAction::kPollHandler,
        "A, callback never ran: handler");
  Check(ResolveStall(StallVerdict::kModeA, true, true, c, 0) == StallAction::kPollFinalise,
        "A, callback ran earlier: finalise");
  Check(ResolveStall(StallVerdict::kModeB, true, true, c, c) == StallAction::kTimeoutHandler,
        "B unlisted: handler");
  Check(ResolveStall(StallVerdict::kModeB, true, true, 0, 0) == StallAction::kTimeoutFinalise,
        "B null camera: finalise");
  Check(ResolveStall(StallVerdict::kModeB, true, true, 0, c) == StallAction::kSkipCameraMismatch,
        "null camera with +300 set: skip");
  Check(ResolveStall(StallVerdict::kModeA, true, true, c, c + 4) ==
            StallAction::kSkipCameraMismatch,
        "+300 another camera: skip");
  Check(ResolveStall(StallVerdict::kModeA, false, true, c, c) == StallAction::kObservedFixOff,
        "fix off: observed");
  Check(ResolveStall(StallVerdict::kModeB, false, false, c, c + 4) ==
            StallAction::kObservedFixOff,
        "fix off wins over every refusal");
  Check(ResolveStall(StallVerdict::kModeA, true, false, c, c) == StallAction::kRefusedUnwritable,
        "unwritable: refused");
  Check(ResolveStall(StallVerdict::kObservedStalledListed, true, true, c, c) ==
            StallAction::kObservedStalledListed,
        "listed: observed");
  Check(ResolveStall(StallVerdict::kSkipFlagsPending, true, true, c, c) ==
            StallAction::kSkipFlagsPending,
        "flags pending: skip");
  for (const StallVerdict v : {StallVerdict::kIdle, StallVerdict::kWatching,
                               StallVerdict::kFired}) {
    Check(ResolveStall(v, true, true, c, c) == StallAction::kNone, "no verdict: no action");
  }
  int acting = 0;
  for (int a = 0; a <= static_cast<int>(StallAction::kSkipCameraMismatch); ++a) {
    acting += mcla::warp::StallActs(static_cast<StallAction>(a)) ? 1 : 0;
  }
  Check(acting == 4, "four acting actions");
  Check(mcla::warp::StallCallsHandler(StallAction::kPollHandler) &&
            mcla::warp::StallCallsHandler(StallAction::kTimeoutHandler) &&
            !mcla::warp::StallCallsHandler(StallAction::kPollFinalise) &&
            !mcla::warp::StallCallsHandler(StallAction::kTimeoutFinalise),
        "handler vs finalise");
  Check(std::string_view(mcla::warp::StallActionName(StallAction::kPollHandler)) ==
                "poll-handler" &&
            std::string_view(mcla::warp::StallActionName(StallAction::kTimeoutFinalise)) ==
                "timeout-finalise" &&
            std::string_view(mcla::warp::StallActionName(StallAction::kObservedFixOff)) ==
                "observed-fix-off" &&
            std::string_view(mcla::warp::StallActionName(StallAction::kObservedStalledListed)) ==
                "observed-stalled-listed",
        "action names (design 3.3)");
  Check(std::string_view(mcla::warp::StallVerdictName(StallVerdict::kModeA)) == "A",
        "verdict names");
}

void TestViewOverrideWatch() {
  using mcla::warp::ViewOverrideSample;
  using mcla::warp::ViewOverrideWatch;
  auto make = [](uint64_t ms) {
    ViewOverrideSample s;
    s.now_ms = ms;
    s.complete = true;
    s.state = mcla::warp::kStateIdle;
    s.queue = 0;
    s.racer_active = true;
    s.b64 = 0;
    s.b65 = 0;
    return s;
  };
  ViewOverrideWatch w;
  Check(!w.Observe(make(0)), "start");
  Check(!w.Observe(make(19999)), "under 20 s");
  Check(w.Observe(make(20000)), "20 s: report");
  Check(!w.Observe(make(40000)), "once per signature");
  ViewOverrideSample released = make(41000);
  released.b64 = 1;
  released.b65 = 1;
  Check(!w.Observe(released), "released ends the signature");
  Check(!w.Observe(make(42000)) && w.Observe(make(62000)), "a new signature reports again");
  for (int field = 0; field < 5; ++field) {
    ViewOverrideWatch x;
    for (uint64_t ms = 0; ms <= 60000; ms += 1000) {
      ViewOverrideSample s = make(ms);
      s.paused = field == 0;
      s.racer_active = field != 1;
      s.queue = field == 2 ? 1 : 0;
      s.state = field == 3 ? mcla::warp::kStateZoomIn : mcla::warp::kStateIdle;
      s.complete = field != 4;
      Check(!x.Observe(s), "paused, no racer, queued, warping or incomplete: no report");
    }
  }
  Check(mcla::warp::ViewOverrideActive(0, 1) && mcla::warp::ViewOverrideActive(1, 0) &&
            !mcla::warp::ViewOverrideActive(1, 1),
        "slot active while +64 == 0 or +65 == 0");
}

std::string Section(const std::string &body, const std::string &from_label,
                    const std::string &to_label) {
  size_t start = body.find(from_label + ":\n");
  if (start == std::string::npos) {
    start = body.find(from_label + ":\r\n");
  }
  if (start == std::string::npos) {
    return {};
  }
  const size_t end = body.find(to_label + ":", start);
  return body.substr(start, end == std::string::npos ? std::string::npos : end - start);
}

void TestGeneratedMirrorKi026() {
  using namespace mcla::warp;
  const std::string text = ReadFile(MCLA_WARP_GENERATED_RECOMP8);
  const std::string update = Body(text, "sub_8221BE98");
  const std::string state19 = Section(update, "loc_8221C044", "loc_8221C120");
  Check(!state19.empty(), "state-19 handler loc_8221C044..loc_8221C120");
  const long long lead_hi =
      static_cast<int32_t>((kLeadScaleAddress - 29952u - 88u) & 0xFFFF0000u);
  const long long factor_hi = static_cast<int32_t>((kLeadFactorAddress - 27632u) & 0xFFFF0000u);
  const long long exit_hi = static_cast<int32_t>((kExitGlobalPointer + 10000u) & 0xFFFF0000u);
  InOrder(state19, 0,
          {Fmt("ctx.r11.u64 = REX_LOAD_U32(ctx.r31.u32 + %lld);", kWarperRequestField),
           Fmt("ctx.r3.u64 = REX_LOAD_U32(ctx.r31.u32 + %lld);", kWarperCameraField),
           "ctx.r30.u64 = REX_LOAD_U8(ctx.r11.u32 + 3);",
           "ctx.r29.u64 = REX_LOAD_U8(ctx.r11.u32 + 0);",
           Fmt("ctx.r10.u64 = REX_LOAD_U32(ctx.r11.u32 + %lld);", kSlotReady),
           "ctx.r10.u64 = ctx.r30.u32 & 0xFF;",
           "if (!ctx.cr6.eq) goto loc_8221CB90;",  // req+3 != 0: wait
           "ctx.r10.u64 = ctx.r29.u32 & 0xFF;",
           "if (ctx.cr6.eq) goto loc_8221CB90;",   // req+0 == 0: wait
           "if (ctx.cr6.eq) goto loc_8221CB90;",   // not ready: wait
           Fmt("ctx.r10.u64 = REX_LOAD_U32(ctx.r11.u32 + %lld);", kSlotDuration),
           Fmt("ctx.r9.s64 = %lld;", lead_hi), Fmt("ctx.r8.s64 = %lld;", factor_hi),
           "temp.u32 = REX_LOAD_U32(ctx.r31.u32 + 260);",
           "ctx.r7.s64 = ctx.r9.s64 + 29952;",
           "ctx.f12.f64 = double(float(ctx.f0.f64 + ctx.f31.f64));",
           "temp.u32 = REX_LOAD_U32(ctx.r8.u32 + 27632);",
           "temp.u32 = REX_LOAD_U32(ctx.r7.u32 + 88);",
           "ctx.f11.f64 = double(float(ctx.f0.f64 * ctx.f13.f64));",
           "temp.u32 = REX_LOAD_U32(ctx.r6.u32 + 18612);",
           "REX_STORE_U32(ctx.r31.u32 + 260, temp.u32);",
           "ctx.f10.f64 = double(float(ctx.f12.f64 + ctx.f0.f64));",
           "ctx.f9.f64 = double(float(ctx.f1.f64 - ctx.f11.f64));",
           "ctx.cr6.compare(ctx.f10.f64, ctx.f9.f64);",
           "if (ctx.cr6.lt) goto loc_8221CB90;",
           Fmt("ctx.r11.s64 = %lld;", exit_hi),
           Fmt("ctx.r10.s64 = %lld;", kExitStateValue),
           "ctx.r11.u64 = REX_LOAD_U32(ctx.r11.u32 + -10000);",
           Fmt("REX_STORE_U8(ctx.r11.u32 + %lld, ctx.r26.u8);", kExitGlobalByteField),
           Fmt("REX_STORE_U8(ctx.r31.u32 + %lld, ctx.r28.u8);", kWarperFinishFlagField),
           Fmt("REX_STORE_U32(ctx.r31.u32 + %lld, ctx.r10.u32);", kWarperStateField)},
          "state-19 wait (req+3 clear, req+0 set, ready) and exit stores");
  Check(state19.find("REX_STORE_U8(ctx.r31.u32 + 273") == std::string::npos &&
            state19.find("sub_8221B790") == std::string::npos,
        "the state-19 exit does not finalise itself");
  Check(0x827D0000u + 29952u + 88u == kLeadScaleAddress &&
            0x82000000u + 27632u == kLeadFactorAddress,
        "lead addresses 0x827D7558 and 0x82006BF0");

  // The callback releases the UILogic+0x10D50 view override when r5 == +300.
  const std::string callback = Body(text, "sub_8221B8C8");
  const long long ui_hi = static_cast<int32_t>((kUiLogicPointer + 10028u) & 0xFFFF0000u);
  InOrder(callback, 0,
          {Fmt("ctx.r11.u64 = REX_LOAD_U32(ctx.r31.u32 + %lld);", kWarperCameraCopyField),
           "ctx.cr6.compare<uint32_t>(ctx.r5.u32, ctx.r11.u32, ctx.xer);",
           "if (!ctx.cr6.eq) goto loc_8221B928;", Fmt("ctx.r11.s64 = %lld;", ui_hi),
           "ctx.r11.u64 = REX_LOAD_U32(ctx.r11.u32 + -10028);",
           "ctx.r3.s64 = ctx.r11.s64 + 65536;", "ctx.r3.s64 = ctx.r3.s64 + 3408;",
           "sub_82224A28(ctx, base);",
           Fmt("REX_STORE_U32(ctx.r31.u32 + %lld, ctx.r10.u32);", kWarperCameraCopyField)},
          "callback view override release");
  Check(static_cast<uint32_t>(ui_hi) + static_cast<uint32_t>(-10028) == kUiLogicPointer &&
            65536u + 3408u == kViewOverrideOffset,
        "UILogic 0x8286D8D4, override at +0x10D50");
  const std::string release = Body(text, "sub_82224A28");
  InOrder(release, 0,
          {"REX_STORE_U8(ctx.r3.u32 + 65, ctx.r11.u8);",
           "REX_STORE_U8(ctx.r3.u32 + 64, ctx.r11.u8);",
           "REX_STORE_U8(ctx.r3.u32 + 145, ctx.r11.u8);",
           "REX_STORE_U8(ctx.r3.u32 + 144, ctx.r11.u8);"},
          "sub_82224A28 sets +64, +65, +144, +145");

  // The finalise the poll may call: +276 = 0, state 29, request recycled.
  const std::string finalise = Body(text, "sub_8221B790");
  InOrder(finalise, 0,
          {Fmt("ctx.r11.u64 = REX_LOAD_U32(ctx.r3.u32 + %lld);", kWarperRequestField),
           "ctx.r5.s64 = 29;",
           Fmt("REX_STORE_U8(ctx.r3.u32 + %lld, ctx.r10.u8);", kWarperFinishFlagField),
           Fmt("REX_STORE_U32(ctx.r3.u32 + %lld, ctx.r5.u32);", kWarperStateField),
           Fmt("REX_STORE_U32(ctx.r11.u32 + %lld, ctx.r10.u32);", kRequestCameraField),
           Fmt("REX_STORE_U32(ctx.r3.u32 + %lld, ctx.r10.u32);", kWarperRequestField)},
          "sub_8221B790 finalise");

  // Warper_Abort finalises first and only then removes request+108 through
  // sub_8221E310 (which the stall fix does not use).
  const std::string abort_body = Body(text, "sub_8221BB90");
  InOrder(abort_body, 0,
          {"sub_8221B790(ctx, base);",
           Fmt("ctx.r11.u64 = REX_LOAD_U32(ctx.r31.u32 + %lld);", kWarperRequestField),
           Fmt("ctx.r4.u64 = REX_LOAD_U32(ctx.r11.u32 + %lld);", kRequestCameraField),
           "ctx.r3.u64 = REX_LOAD_U32(ctx.r11.u32 + 52);", "sub_8221E310(ctx, base);"},
          "Warper_Abort order");
  const std::string remove = Body(text, "sub_8221E310");
  InOrder(remove, 0,
          {"ctx.r9.u64 = ctx.r11.u64 | 2132;", "ctx.r31.s64 = ctx.r31.s64 + 2128;",
           "ctx.cr6.compare<uint32_t>(ctx.r4.u32, ctx.r7.u32, ctx.xer);",
           "ctx.r10.u64 = REX_LOAD_U32(ctx.r11.u32 + 96);",
           "ctx.r7.u64 = REX_LOAD_U32(ctx.r8.u32 + 64);",
           "ctx.r4.u64 = REX_LOAD_U32(ctx.r5.u32 + 112);",
           "ctx.r10.u64 = REX_LOAD_U32(ctx.r11.u32 + 388);",
           "ctx.r7.u64 = REX_LOAD_U32(ctx.r8.u32 + 128);"},
          "sub_8221E310: find in the active list, five virtual calls");
  Check(262144u + 2128u == 0x40850u, "sub_8221E310 scans the +0x40850 active list");
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
  TestFinishState19();
  TestStallHealthyTrips();
  TestStallModeA();
  TestStallModeB();
  TestStallPreconditions();
  TestStallResolve();
  TestViewOverrideWatch();
  TestGeneratedMirrorKi026();
  std::printf("mcla_warp_probe_test: %d checks passed\n", checks);
  return 0;
}
