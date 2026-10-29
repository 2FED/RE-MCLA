#include "mcla_photo_capture.h"

#include <cstdio>
#include <cstdlib>

namespace {
int checks = 0;
void Check(bool value, const char *message) {
  if (!value) {
    std::fprintf(stderr, "FAIL: %s\n", message);
    std::exit(1);
  }
  ++checks;
}

using mcla::photo_capture::Event;
using mcla::photo_capture::kMaxLoggedTransitions;
using mcla::photo_capture::kTimeoutFrames;
using mcla::photo_capture::kTimeoutMs;
using mcla::photo_capture::Window;

void TestOpenAndCloseToggleTheWait() {
  Window w(true);
  Check(!w.open() && !w.Tick(0).transition, "closed window ignores ticks");
  auto d = w.Open(1000, false);
  Check(d.transition && d.log && d.event == Event::kOpen, "open is logged");
  Check(d.write_wait && *d.write_wait, "open turns the wait on");
  Check(w.open() && w.owning(), "state after open");
  d = w.Open(1001, true);
  Check(!d.transition && !d.write_wait, "repeated open is ignored");
  for (uint64_t i = 0; i < 4; ++i) {
    Check(!w.Tick(1010 + i).transition, "short window keeps running");
  }
  d = w.Close(1100);
  Check(d.transition && d.event == Event::kClose, "close is a transition");
  Check(d.write_wait && !*d.write_wait, "close turns the wait off");
  Check(d.frames == 4 && d.elapsed_ms == 100, "close reports frames and time");
  Check(!w.open() && !w.owning(), "state after close");
  d = w.Close(1200);
  Check(!d.transition && !d.write_wait, "repeated close is ignored");
}

void TestUserOwnedWaitIsLeftAlone() {
  Window w(true);
  auto d = w.Open(0, true);
  Check(d.transition && !d.write_wait, "wait already on: nothing written");
  Check(!w.owning(), "not owning a user value");
  d = w.Close(10);
  Check(d.transition && !d.write_wait, "close does not turn a user value off");
}

void TestDisabledNeverWrites() {
  Window w(false);
  Check(!w.Open(0, false).transition && !w.open(), "disabled never opens");
  Check(!w.Close(1).transition, "disabled close is ignored");
  Check(!w.Shutdown(2).transition, "disabled shutdown is ignored");
}

void TestFrameTimeout() {
  Window w(true);
  w.Open(0, false);
  for (uint32_t i = 1; i < kTimeoutFrames; ++i) {
    Check(!w.Tick(i).transition, "below the frame limit");
  }
  auto d = w.Tick(kTimeoutFrames);
  Check(d.transition && d.event == Event::kTimeout, "frame limit closes");
  Check(d.write_wait && !*d.write_wait, "timeout turns the wait off");
  Check(d.frames == kTimeoutFrames, "timeout reports frames");
  d = w.Close(kTimeoutFrames + 1);
  Check(!d.transition && !d.write_wait, "later unfreeze writes nothing");
  d = w.Open(kTimeoutFrames + 2, false);
  Check(d.transition && d.write_wait && *d.write_wait, "next capture opens again");
}

void TestWallTimeout() {
  Window w(true);
  w.Open(500, false);
  Check(!w.Tick(500 + kTimeoutMs - 1).transition, "below the time limit");
  auto d = w.Tick(500 + kTimeoutMs);
  Check(d.transition && d.event == Event::kTimeout && d.frames == 2,
        "time limit closes");
  Check(d.elapsed_ms == kTimeoutMs, "timeout reports time");
}

void TestShutdownClosesAndStops() {
  Window w(true);
  w.Open(0, false);
  auto d = w.Shutdown(30);
  Check(d.transition && d.event == Event::kShutdown, "shutdown closes");
  Check(d.write_wait && !*d.write_wait, "shutdown turns the wait off");
  Check(w.stopped(), "stopped after shutdown");
  Check(!w.Open(40, false).transition, "no opens after shutdown");
  Window idle(true);
  d = idle.Shutdown(0);
  Check(!d.transition && !d.write_wait && idle.stopped(),
        "shutdown with a closed window writes nothing");
}

void TestLoggingIsBounded() {
  Window w(true);
  int logged = 0;
  for (int i = 0; i < kMaxLoggedTransitions; ++i) {
    const auto open = w.Open(static_cast<uint64_t>(i) * 10, false);
    const auto close = w.Close(static_cast<uint64_t>(i) * 10 + 5);
    Check(open.write_wait && close.write_wait, "writes are never suppressed");
    logged += open.log + close.log;
  }
  Check(logged == kMaxLoggedTransitions, "transitions logged up to the cap");
  Check(!w.Open(100000, false).log, "transitions past the cap are not logged");
}

}  // namespace

int main() {
  TestOpenAndCloseToggleTheWait();
  TestUserOwnedWaitIsLeftAlone();
  TestDisabledNeverWrites();
  TestFrameTimeout();
  TestWallTimeout();
  TestShutdownClosesAndStops();
  TestLoggingIsBounded();
  std::printf("mcla_photo_capture_test: %d checks passed\n", checks);
  return 0;
}
