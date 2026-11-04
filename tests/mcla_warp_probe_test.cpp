#include "mcla_warp_probe.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <string_view>

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
  std::printf("mcla_warp_probe_test: %d checks passed\n", checks);
  return 0;
}
