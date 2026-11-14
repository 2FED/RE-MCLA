#include "mcla_arcade_input.h"

#include <cstdint>
#include <initializer_list>
#include <iostream>
#include <limits>
#include <type_traits>

namespace {
using namespace mcla::arcade_input;
static_assert(!std::is_copy_constructible_v<MenuLatch>);
static_assert(!std::is_copy_assignable_v<MenuLatch>);
static_assert(!std::is_move_constructible_v<MenuLatch>);
static_assert(!std::is_move_assignable_v<MenuLatch>);
unsigned checks = 0, failures = 0;
void Check(bool condition, const char* label) {
  ++checks;
  if (!condition) { ++failures; std::cerr << "FAIL " << label << '\n'; }
}
void Failed(const MenuLatch& latch, Error error) {
  Check(latch.snapshot().status == Status::Failed, "failed status");
  Check(latch.snapshot().error == error, "first exact error retained");
}
void Sticky(MenuLatch& latch, uint64_t now, Error error) {
  const auto first_failure = latch.snapshot().first_failure_observed_ms;
  const bool first_clock_valid = latch.snapshot().first_failure_clock_valid;
  Check(latch.Read(now) == 0, "failed read neutral");
  Check(!latch.Arm(now, 1, true), "failed cannot restart");
  latch.Fail(now);
  Check(latch.Poll(now) == Status::Failed, "failed poll terminal");
  Failed(latch, error);
  Check(latch.snapshot().first_failure_observed_ms == first_failure &&
        latch.snapshot().first_failure_clock_valid == first_clock_valid,
        "first failure sample and validity retained");
}

void Normal() {
  MenuLatch latch;
  Check(latch.snapshot().status == Status::Idle && latch.snapshot().sequence == 0, "idle state");
  Check(latch.Read(0) == 0 && latch.Poll(0) == Status::Idle, "idle output neutral");
  constexpr uint16_t expected[] = {0x10, 0x200, 0x1000, 0x1000, 0x1000, 0x1000};
  uint64_t now = 100;
  for (uint32_t seq = 1; seq <= 6; ++seq) {
    Check(latch.Arm(now, seq, seq == 1), "next exact command accepted");
    Check(latch.snapshot().sequence == seq && latch.snapshot().published_ms == now, "sequence publication");
    Check(latch.snapshot().release_deadline_ms == now + 200 &&
          latch.snapshot().neutral_ack_deadline_ms == now + 5200, "fixed deadlines");
    Check(!latch.snapshot().down_ack_ms && !latch.snapshot().neutral_ack_ms &&
          !latch.snapshot().released_ms && !latch.snapshot().released_clock_valid,
          "new command clears old acknowledgements");
    Check(latch.Read(now) == expected[seq - 1], "exact button only");
    Check(latch.Read(now + 199) == expected[seq - 1], "hold strictly before release");
    Check(latch.snapshot().down_ack_ms == now, "first down acknowledgement retained");
    Check(latch.Poll(now + 200) == Status::WaitingNeutralAck, "poll releases");
    Check(latch.snapshot().released_ms == now + 200 && !latch.snapshot().neutral_ack_ms,
          "poll records actual release but is not guest ACK");
    Check(latch.snapshot().released_clock_valid && !latch.snapshot().first_failure_observed_ms,
          "normal release clock valid without failure");
    Check(latch.Read(now + 201) == 0 && latch.snapshot().status == Status::Succeeded, "neutral ACK completes");
    Check(latch.snapshot().neutral_ack_ms == now + 201, "neutral ACK observed");
    Check(latch.Read(now + 202) == 0 && latch.snapshot().neutral_ack_ms == now + 201, "completion neutral and immutable ACK");
    now += 2201;
  }
  Check(latch.Poll(now + 10000) == Status::Succeeded && latch.Read(now + 10000) == 0, "final success never reasserts");
  Check(!latch.Arm(now + 10000, 7, true), "no seventh command");
  Sticky(latch, now + 10000, Error::ArmOrder);
}

void Ordering() {
  for (uint32_t seq : {0u, 2u, 6u, 7u, std::numeric_limits<uint32_t>::max()}) {
    MenuLatch latch;
    Check(!latch.Arm(0, seq, true), "wrong initial sequence rejects");
    Check(!latch.snapshot().published_ms, "wrong sequence never publishes");
    Sticky(latch, 0, Error::ArmOrder);
  }
  MenuLatch missing_start;
  Check(!missing_start.Arm(0, 1, false), "requires prior title START neutral ACK");
  Sticky(missing_start, 1, Error::MissingStartNeutralAck);
  for (unsigned phase = 0; phase != 3; ++phase) {
    MenuLatch latch;
    Check(latch.Arm(0, 1, true) && latch.Read(0) == 0x10, "rearm fixture");
    if (phase >= 1) latch.Poll(200);
    if (phase >= 2) latch.Read(200);
    Check(!latch.Arm(2200, 1, true), "same sequence cannot rearm in any phase");
    Sticky(latch, 2200, Error::ArmOrder);
  }
  MenuLatch skipped;
  Check(skipped.Arm(0, 1, true) && skipped.Read(0) == 0x10 && skipped.Read(200) == 0, "skip fixture");
  Check(!skipped.Arm(2200, 3, true), "cannot skip successful sequence");
  Sticky(skipped, 2200, Error::ArmOrder);
  MenuLatch early;
  Check(early.Arm(0, 1, true) && early.Read(0) == 0x10 && early.Read(201) == 0, "early fixture");
  Check(!early.Arm(2200, 2, true), "1999ms since neutral ACK rejects");
  Sticky(early, 2201, Error::TooEarly);
  MenuLatch active;
  Check(active.Arm(0, 1, true), "active sequence fixture");
  Check(!active.Arm(1, 2, true), "cannot advance while holding");
  Check(active.snapshot().released_ms == 1, "invalid arm releases immediately");
  Sticky(active, 2, Error::ArmOrder);
}

void Boundaries() {
  MenuLatch no_down;
  Check(no_down.Arm(0, 1, true), "no-down arm");
  Check(no_down.Poll(199) == Status::Holding, "no-down still bounded");
  Check(no_down.Read(200) == 0, "first read at release never emits button");
  Check(no_down.snapshot().released_ms == 200 && !no_down.snapshot().down_ack_ms &&
        !no_down.snapshot().neutral_ack_ms, "no invented acknowledgements");
  Sticky(no_down, 201, Error::MissingDownAck);
  MenuLatch no_neutral;
  Check(no_neutral.Arm(0, 1, true) && no_neutral.Read(0) == 0x10, "no-neutral fixture");
  Check(no_neutral.Poll(200) == Status::WaitingNeutralAck, "no-neutral releases");
  Check(no_neutral.Poll(5199) == Status::WaitingNeutralAck, "neutral last waiting slot");
  Check(no_neutral.Read(5200) == 0 && !no_neutral.snapshot().neutral_ack_ms, "exact ACK deadline rejects");
  Sticky(no_neutral, 5201, Error::MissingNeutralAck);
  MenuLatch last;
  Check(last.Arm(0, 1, true) && last.Read(0) == 0x10, "last-slot fixture");
  Check(last.Read(5199) == 0 && last.snapshot().status == Status::Succeeded, "last ACK slot accepted");
  Check(last.snapshot().released_ms == 5199 && last.snapshot().neutral_ack_ms == 5199, "late real release retained");
  MenuLatch late;
  Check(late.Arm(0, 1, true) && late.Read(0) == 0x10, "late-poll fixture");
  Check(late.Poll(8000) == Status::Failed && late.snapshot().released_ms == 8000,
        "late poll cannot extend deadline or fabricate release time");
  Sticky(late, 8001, Error::MissingNeutralAck);
  MenuLatch late_arm;
  Check(late_arm.Arm(0, 1, true), "late-arm fixture");
  Check(!late_arm.Arm(6000, 2, true), "arm observes expired current command");
  Sticky(late_arm, 6001, Error::MissingDownAck);
}

void Cancellation() {
  for (unsigned phase = 0; phase != 4; ++phase) {
    MenuLatch latch;
    if (phase >= 1) Check(latch.Arm(0, 1, true) && latch.Read(0) == 0x10, "cancel fixture");
    if (phase >= 2) latch.Poll(200);
    if (phase >= 3) latch.Read(200);
    latch.Fail(201);
    Check(latch.snapshot().first_failure_observed_ms == 201 &&
          latch.snapshot().first_failure_clock_valid, "explicit failure preserves valid observed sample");
    Check(latch.snapshot().released_ms == (phase == 0 ? std::optional<uint64_t>{} :
          std::optional<uint64_t>{phase == 1 ? 201u : 200u}), "cancellation keeps actual release");
    Sticky(latch, 202, Error::ExplicitFailure);
  }
}

void ClockSafety() {
  for (unsigned operation = 0; operation != 4; ++operation) {
    MenuLatch latch;
    Check(latch.Arm(100, 1, true) && latch.Read(150) == 0x10, "rollback fixture");
    switch (operation) {
      case 0: Check(latch.Read(149) == 0, "rollback read neutral"); break;
      case 1: latch.Poll(149); break;
      case 2: Check(!latch.Arm(149, 2, true), "rollback arm rejects"); break;
      case 3: latch.Fail(149); break;
    }
    Check(latch.snapshot().last_ms == 150 && latch.snapshot().released_ms == 149 &&
          !latch.snapshot().released_clock_valid,
          "rollback preserves actual invalid release and last accepted clock separately");
    Check(latch.snapshot().first_failure_observed_ms == 149 &&
          !latch.snapshot().first_failure_clock_valid, "rollback first failure marked invalid");
    Sticky(latch, 151, Error::ClockRollback);
  }
  MenuLatch idle;
  idle.Read(1);
  Check(!idle.Arm(0, 1, true), "idle rollback also rejects");
  Sticky(idle, 2, Error::ClockRollback);
  MenuLatch released;
  Check(released.Arm(0, 1, true) && released.Read(0) == 0x10 &&
        released.Poll(200) == Status::WaitingNeutralAck, "released rollback fixture");
  Check(released.Read(199) == 0 && released.snapshot().released_ms == 200 &&
        released.snapshot().released_clock_valid, "rollback after release preserves earlier valid release");
  Check(released.snapshot().first_failure_observed_ms == 199 &&
        !released.snapshot().first_failure_clock_valid, "post-release rollback retains invalid failure sample");
  Sticky(released, 201, Error::ClockRollback);
  const auto max_time = std::numeric_limits<uint64_t>::max();
  MenuLatch overflow;
  Check(!overflow.Arm(max_time - 5199, 1, true), "deadline overflow rejects");
  Check(!overflow.snapshot().published_ms, "overflow has no publication");
  Sticky(overflow, max_time, Error::DeadlineOverflow);
  MenuLatch edge;
  Check(edge.Arm(max_time - 5200, 1, true), "largest representable publication");
  Check(edge.Read(max_time - 5200) == 0x10 && edge.Read(max_time - 5000) == 0,
        "edge command succeeds");
  Check(edge.snapshot().status == Status::Succeeded, "edge successful");
  Check(!edge.Arm(max_time - 3000, 2, true), "next command overflow rejects after legal gap");
  Sticky(edge, max_time, Error::DeadlineOverflow);
}
}  // namespace

int main() {
  Normal(); Ordering(); Boundaries(); Cancellation(); ClockSafety();
  std::cout << "arcade input: " << checks << " checks, " << failures << " failures\n" << std::flush;
  return failures ? 1 : 0;
}
