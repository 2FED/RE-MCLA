#include "mcla_autostart_input.h"

#include <cstdint>
#include <initializer_list>
#include <iostream>
#include <limits>
#include <type_traits>

namespace {
using namespace mcla::autostart_input;
static_assert(!std::is_copy_constructible_v<StartLatch>);
static_assert(!std::is_copy_assignable_v<StartLatch>);
static_assert(!std::is_move_constructible_v<StartLatch>);
static_assert(!std::is_move_assignable_v<StartLatch>);
unsigned checks = 0, failures = 0;
void Check(bool condition, const char* label) {
  ++checks;
  if (!condition) { ++failures; std::cerr << "FAIL " << label << '\n'; }
}
void CheckFailure(const StartLatch& latch, Error error) {
  Check(latch.snapshot().status == Status::Failed, "terminal failed status");
  Check(latch.snapshot().error == error, "exact first failure retained");
}

void Normal() {
  StartLatch latch;
  Check(latch.snapshot().status == Status::Idle, "initial idle");
  Check(latch.Read(0) == 0, "initial neutral");
  Check(latch.Poll(50) == Status::Idle, "pre-arm timer allowed");
  Check(latch.Arm(100), "one arm accepted");
  Check(latch.snapshot().published_ms == 100, "publication timestamp");
  Check(latch.snapshot().release_deadline_ms == 5100, "exact 5000ms hold bound");
  Check(latch.snapshot().neutral_ack_deadline_ms == 10100, "exact 5000ms neutral bound");
  Check(!latch.snapshot().down_ack_ms, "arm is not guest ACK");
  Check(latch.Read(100) == 0x0010, "only START at publication");
  Check(latch.Read(5099) == 0x0010, "START before deadline");
  Check(latch.snapshot().down_ack_ms == 100, "first down ACK retained");
  Check(latch.Poll(5100) == Status::WaitingNeutralAck, "timer releases at deadline");
  Check(latch.snapshot().released_ms == 5100, "actual release observed time");
  Check(!latch.snapshot().neutral_ack_ms, "timer release is not guest neutral ACK");
  Check(latch.Read(5101) == 0, "guest reads neutral");
  Check(latch.snapshot().status == Status::Succeeded, "both ACKs succeed");
  Check(latch.snapshot().neutral_ack_ms == 5101, "neutral ACK timestamp");
  Check(latch.Read(10100) == 0 && latch.Poll(20000) == Status::Succeeded, "success stays neutral beyond deadline");
  Check(latch.snapshot().read_count == 5 && latch.snapshot().down_read_count == 2 &&
        latch.snapshot().neutral_read_count == 3 && latch.snapshot().poll_count == 3, "exact counters");
}

void NoDownAck() {
  StartLatch latch;
  Check(latch.Arm(0), "noACK arm");
  Check(latch.Poll(4999) == Status::HoldingStart, "noACK hold still bounded");
  Check(latch.Poll(5000) == Status::Failed, "noACK release independent of guest reads");
  CheckFailure(latch, Error::MissingDownAck);
  Check(latch.snapshot().released_ms == 5000 && !latch.snapshot().down_ack_ms, "no manufactured down ACK");
  Check(latch.Read(5000) == 0 && !latch.snapshot().neutral_ack_ms, "failed neutral read cannot succeed");
  Check(!latch.Arm(5001), "failure cannot rearm");
  CheckFailure(latch, Error::MissingDownAck);
}

void NoNeutralAck() {
  StartLatch latch;
  Check(latch.Arm(0) && latch.Read(1) == 0x0010, "down ACK received");
  Check(latch.Poll(5000) == Status::WaitingNeutralAck, "neutral published");
  Check(latch.Poll(9999) == Status::WaitingNeutralAck, "neutral wait inside bound");
  Check(latch.Poll(10000) == Status::Failed, "neutral timeout exact deadline");
  CheckFailure(latch, Error::MissingNeutralAck);
  Check(latch.Read(10001) == 0 && !latch.snapshot().neutral_ack_ms, "late neutral cannot repair timeout");
}

void ReadBoundaries() {
  StartLatch latch;
  Check(latch.Arm(10) && latch.Read(11) == 0x0010, "boundary down ACK");
  Check(latch.Read(5010) == 0, "Read neutralizes at exact hold deadline without timer");
  Check(latch.snapshot().status == Status::Succeeded && latch.snapshot().released_ms == 5010 &&
        latch.snapshot().neutral_ack_ms == 5010, "same guest read supplies real neutral ACK");
  StartLatch late;
  Check(late.Arm(0), "late first read arm");
  Check(late.Read(5000) == 0, "first read at deadline never emits START");
  CheckFailure(late, Error::MissingDownAck);
  StartLatch delayed;
  Check(delayed.Arm(0) && delayed.Read(1) == 0x0010, "delayed timer down ACK");
  Check(delayed.Poll(12000) == Status::Failed, "delayed poll cannot extend neutral deadline");
  Check(delayed.snapshot().released_ms == 12000, "late release reports observed rather than fabricated time");
  CheckFailure(delayed, Error::MissingNeutralAck);
  StartLatch exact;
  Check(exact.Arm(0) && exact.Read(1) == 0x0010, "exact neutral deadline arm");
  Check(exact.Read(10000) == 0, "exact neutral deadline is expired before ACK");
  CheckFailure(exact, Error::MissingNeutralAck);
  StartLatch last;
  Check(last.Arm(0) && last.Read(1) == 0x0010, "last neutral slot arm");
  Check(last.Read(9999) == 0 && last.snapshot().status == Status::Succeeded, "last millisecond neutral ACK allowed");
}

void AbortAndRearm() {
  for (bool cancel : {false, true}) {
    StartLatch latch;
    Check(latch.Arm(100) && latch.Read(101) == 0x0010, "abort armed");
    if (cancel) latch.Cancel(102); else latch.Fail(102);
    CheckFailure(latch, cancel ? Error::Cancelled : Error::ExplicitFailure);
    Check(latch.snapshot().released_ms == 102 && latch.Read(103) == 0, "abort immediately neutral");
    Check(!latch.Arm(104) && latch.Read(105) == 0, "abort permanently one shot");
  }
  StartLatch before;
  before.Cancel(0);
  Check(!before.Arm(1) && before.Read(2) == 0, "pre-arm cancellation prevents future publication");
  CheckFailure(before, Error::Cancelled);
  StartLatch completed;
  Check(completed.Arm(0) && completed.Read(0) == 0x0010 && completed.Read(5000) == 0,
        "post-success cancellation fixture");
  completed.Cancel(5001);
  CheckFailure(completed, Error::Cancelled);
  Check(completed.Read(5002) == 0 && !completed.Arm(5003), "post-success cancellation remains neutral");
  for (uint64_t second_arm : {uint64_t{1}, uint64_t{5001}, uint64_t{10001}}) {
    StartLatch latch;
    Check(latch.Arm(0) && latch.Read(0) == 0x0010, "rearm fixture");
    if (second_arm >= 5001) latch.Poll(5000);
    if (second_arm >= 10001) latch.Read(5001);
    Check(!latch.Arm(second_arm), "repeated arm rejects in hold, neutral wait, success");
    CheckFailure(latch, Error::RepeatedArm);
    Check(latch.Read(second_arm) == 0, "repeated arm never reasserts START");
  }
}

void ClockSafety() {
  for (unsigned operation = 0; operation != 5; ++operation) {
    StartLatch latch;
    Check(latch.Arm(100) && latch.Read(200) == 0x0010, "rollback fixture");
    switch (operation) {
      case 0: Check(latch.Read(199) == 0, "rollback read neutral"); break;
      case 1: latch.Poll(199); break;
      case 2: Check(!latch.Arm(199), "rollback arm rejects"); break;
      case 3: latch.Cancel(199); break;
      case 4: latch.Fail(199); break;
    }
    CheckFailure(latch, Error::ClockRollback);
    Check(latch.snapshot().last_ms == 200 && latch.snapshot().released_ms == 200, "invalid time not recorded as release");
    Check(latch.Read(201) == 0 && !latch.Arm(202), "rollback failure permanent");
  }
  StartLatch idle;
  idle.Read(10);
  Check(!idle.Arm(9), "rollback against idle reading rejects arm");
  CheckFailure(idle, Error::ClockRollback);
  const auto max_time = std::numeric_limits<uint64_t>::max();
  StartLatch overflow;
  Check(!overflow.Arm(max_time - 9999), "overflow rejects before START");
  CheckFailure(overflow, Error::DeadlineOverflow);
  Check(!overflow.snapshot().published_ms && overflow.Read(max_time) == 0, "overflow cannot publish");
  StartLatch edge;
  Check(edge.Arm(max_time - 10000), "largest representable bounded publication");
  Check(edge.Read(max_time - 10000) == 0x0010, "edge START");
  Check(edge.Read(max_time - 5000) == 0 && edge.snapshot().status == Status::Succeeded, "edge neutral succeeds without overflow");
}
}  // namespace

int main() {
  Normal(); NoDownAck(); NoNeutralAck(); ReadBoundaries(); AbortAndRearm(); ClockSafety();
  std::cout << "autostart input: " << checks << " checks, " << failures << " failures\n";
  return failures ? 1 : 0;
}
