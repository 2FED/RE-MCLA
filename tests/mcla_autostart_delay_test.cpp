#include "mcla_autostart_delay.h"
#include "mcla_autostart_input.h"
#include <array>
#include <iostream>
#include <stdexcept>

namespace {
unsigned checks{};
void Need(bool ok, const char* reason) {
  ++checks; if (!ok) throw std::runtime_error(reason);
}
void Positive() {
  uint64_t now = 1000; unsigned arms = 0, sleeps = 0;
  mcla::autostart_input::StartLatch latch;
  mcla::autostart_delay::Observation observation;
  const bool ok = mcla::autostart_delay::WaitAndArm(100, 1000, 9000,
      [&] { return now; }, [&](uint64_t ms) {
        Need(ms > 0 && ms <= 50 && now < 9000, "bounded pre-arm sleep");
        Need(latch.Read(now) == 0 && !latch.snapshot().published_ms, "neutral throughout delay");
        now += ms; ++sleeps; return true;
      }, [] { return false; }, [&] { ++arms; return latch.Arm(now); }, observation);
  Need(ok && arms == 1 && sleeps == 160 && now == 9000, "one arm at exact due");
  Need(observation.arm_before_ms == 9000 && observation.arm_after_ms == 9000 &&
       latch.snapshot().published_ms == 9000, "actual publication bracket");
  Need(latch.Read(9000) == mcla::autostart_input::kStartButton, "original START down");
  Need(latch.Read(13999) == mcla::autostart_input::kStartButton, "original five-second hold");
  latch.Poll(14000);
  Need(latch.snapshot().status == mcla::autostart_input::Status::WaitingNeutralAck &&
       !latch.snapshot().neutral_ack_ms, "release does not invent ACK");
  Need(latch.Read(14001) == 0 && latch.snapshot().status == mcla::autostart_input::Status::Succeeded,
       "separate neutral ACK unchanged");
}
void Negatives() {
  for (unsigned mode = 0; mode < 12; ++mode) {
    uint64_t now = 1000; unsigned arms = 0, clocks = 0, cancels = 0;
    mcla::autostart_delay::Observation observation;
    const auto begin = mode == 6 ? uint64_t(0) : uint64_t(100);
    const auto title = mode == 6 ? uint64_t(87000) : uint64_t(1000);
    if (mode == 6) now = title;
    const bool ok = mcla::autostart_delay::WaitAndArm(begin, title, title + 8000,
      [&]() -> uint64_t {
        ++clocks;
        if (mode == 1 && clocks == 2) return 999;
        if (mode == 2) throw std::runtime_error("clock");
        if (mode == 7 && arms) return 95100;
        if (mode == 8 && arms) return 8999;
        return now;
      }, [&](uint64_t ms) {
        now += ms;
        if (mode == 3) throw std::runtime_error("sleep");
        return mode != 4;
      }, [&] {
        ++cancels;
        return mode == 0 || (mode == 5 && cancels == 2) || (mode == 9 && now >= 9000);
      }, [&] {
        ++arms;
        if (mode == 10) throw std::runtime_error("arm");
        return mode != 11;
      }, observation);
    Need(!ok, "every negative rejects");
    const bool reached_arm = mode == 7 || mode == 8 || mode == 10 || mode == 11;
    Need(arms == unsigned(reached_arm), "no arm on prepublication cancellation/budget/clock failure");
    Need(observation.arm_before_ms.has_value() == reached_arm, "actual arm entry retained only when reached");
    if (mode == 7) Need(observation.arm_after_ms == 95100, "late actual clock retained");
    if (mode == 8) Need(observation.arm_after_ms == 8999, "rollback actual clock retained");
    if (mode == 11) Need(observation.arm_after_ms == 9000, "failed Arm return bracket retained");
  }
  for (auto values : {std::array<uint64_t,3>{1001,1000,9000}, {100,1000,8999},
                      {0,UINT64_MAX-1,0}}) {
    unsigned callbacks = 0; mcla::autostart_delay::Observation observation;
    Need(!mcla::autostart_delay::WaitAndArm(values[0],values[1],values[2],
       [&] { ++callbacks; return uint64_t(0); }, [&](uint64_t) { ++callbacks; return true; },
       [&] { ++callbacks; return false; }, [&] { ++callbacks; return true; }, observation) &&
       callbacks == 0, "invalid anchor contract has no callbacks");
  }
}
}
int main() {
  try { Positive(); Negatives(); std::cout << "autostart delay: " << checks << " checks PASS\n"; return 0; }
  catch (const std::exception& e) { std::cerr << "FAIL " << e.what() << '\n'; return 1; }
}
