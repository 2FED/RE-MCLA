#include "mcla_motion_input.h"

#include <cstdint>
#include <initializer_list>
#include <iostream>
#include <limits>
#include <type_traits>
#include <utility>

namespace {
using namespace mcla::motion_input;
static_assert(!std::is_copy_constructible_v<MotionLatch>);
static_assert(!std::is_copy_assignable_v<MotionLatch>);
static_assert(!std::is_move_constructible_v<MotionLatch>);
static_assert(!std::is_move_assignable_v<MotionLatch>);
static_assert(std::is_same_v<decltype(std::declval<MotionLatch&>().Read(0)), uint8_t>);
static_assert(kRightTrigger == 96 && kHoldMilliseconds == 3000 && kNeutralAckMilliseconds == 5000);
unsigned checks{}, failures{};
void Check(bool value, const char* message) {
  ++checks;
  if (!value) { ++failures; std::cerr << "FAIL " << message << '\n'; }
}
void Failed(const MotionLatch& value, Error error) {
  Check(value.snapshot().status == Status::Failed, "terminal failed status");
  Check(value.snapshot().error == error, "first error retained");
}

void NormalAndPrerequisite() {
  MotionLatch value;
  Check(value.snapshot().status == Status::Idle, "initial idle");
  Check(value.Read(0) == 0 && value.Poll(50) == Status::Idle, "idle never autoarms");
  Check(!value.snapshot().neutral_ack_ms, "pre-arm neutral read is not protocol ACK");
  Check(value.Arm(100, true), "explicit successful START neutral prerequisite");
  const auto& s=value.snapshot();
  Check(s.published_ms == 100 && s.start_neutral_acknowledged, "publication records prerequisite");
  Check(s.release_deadline_ms == 3100 && s.neutral_ack_deadline_ms == 8100, "publication anchored bounds");
  Check(!s.down_ack_ms && !s.neutral_ack_ms, "Arm does not fabricate guest ACKs");
  Check(value.Read(100) == 96 && value.Read(3099) == 96, "fixed RT96 only before deadline");
  Check(s.down_ack_ms == 100, "first down ACK retained");
  Check(value.Poll(3100) == Status::WaitingNeutralAck, "Poll releases independently");
  Check(s.released_ms == 3100 && s.released_clock_valid && !s.neutral_ack_ms, "release is not neutral guest ACK");
  Check(value.Read(3101) == 0 && s.status == Status::Succeeded, "separate neutral guest ACK succeeds");
  Check(s.neutral_ack_ms == 3101, "actual neutral ACK time");
  Check(value.Read(20000) == 0 && value.Poll(21000) == Status::Succeeded, "terminal neutral success");
  Check(s.read_count == 5 && s.down_read_count == 2 && s.neutral_read_count == 3 && s.poll_count == 3,
        "exact public-call counters");
  MotionLatch missing;
  Check(!missing.Arm(0, false), "missing START neutral ACK rejects");
  Failed(missing, Error::StartNeutralNotAcknowledged);
  Check(!missing.snapshot().published_ms && !missing.snapshot().start_neutral_acknowledged,
        "missing prerequisite publishes nothing");
  Check(!missing.Arm(1, true) && missing.Read(2) == 0, "later approval cannot repair one-shot failure");
  Failed(missing, Error::StartNeutralNotAcknowledged);
}

void ZeroAndDeadlines() {
  MotionLatch zero;
  Check(zero.Arm(0, true) && zero.Read(0) == 96, "zero timestamp is valid publication and ACK");
  Check(zero.snapshot().down_ack_ms.has_value() && zero.snapshot().down_ack_ms == 0, "zero ACK not missing");
  Check(zero.Read(3000) == 0 && zero.snapshot().status == Status::Succeeded, "Read neutralizes exactly at release");
  Check(zero.snapshot().released_ms == 3000 && zero.snapshot().neutral_ack_ms == 3000, "same call truthful release and neutral");
  MotionLatch delayed;
  Check(delayed.Arm(0,true) && delayed.Read(2999) == 96, "last down slot accepted");
  Check(delayed.snapshot().release_deadline_ms == 3000 && delayed.snapshot().neutral_ack_deadline_ms == 8000,
        "delayed down ACK never shifts either bound");
  Check(delayed.Read(7999) == 0 && delayed.snapshot().status == Status::Succeeded, "last neutral slot accepted");
  Check(delayed.snapshot().released_ms == 7999, "late first release reports actual read");
  MotionLatch exact;
  Check(exact.Arm(0,true) && exact.Read(1)==96, "exact ACK deadline fixture");
  Check(exact.Read(8000)==0, "expired neutral ACK always returns zero");
  Failed(exact, Error::MissingNeutralAck);
  Check(!exact.snapshot().neutral_ack_ms, "expired ACK not fabricated");
}

void NoReadsAndLatePoll() {
  for (uint64_t time : {uint64_t{3000},uint64_t{8000},uint64_t{12000}}) {
    MotionLatch value;
    Check(value.Arm(0,true), "no down read arm");
    Check(value.Poll(2999)==Status::HoldingRightTrigger, "Poll cannot create down ACK");
    Check(value.Poll(time)==Status::Failed, "no-read timeout releases without guest");
    Failed(value,Error::MissingDownAck);
    Check(value.snapshot().released_ms==time && value.snapshot().released_clock_valid,
          "observed release never replaced by intended deadline");
    Check(value.Read(time)==0 && !value.snapshot().down_ack_ms && !value.snapshot().neutral_ack_ms,
          "failure cannot acquire either ACK afterward");
  }
  MotionLatch neutral;
  Check(neutral.Arm(0,true) && neutral.Read(1)==96, "neutral timeout arm");
  Check(neutral.Poll(3000)==Status::WaitingNeutralAck && neutral.Poll(7999)==Status::WaitingNeutralAck,
        "bounded neutral wait");
  Check(neutral.Poll(8000)==Status::Failed, "exact independent neutral timeout");
  Failed(neutral,Error::MissingNeutralAck);
  MotionLatch late;
  Check(late.Arm(0,true) && late.Read(1)==96, "late Poll fixture");
  Check(late.Poll(12000)==Status::Failed && late.snapshot().released_ms==12000, "late Poll no deadline extension");
  Failed(late,Error::MissingNeutralAck);
  for (uint64_t time : {uint64_t{3000},uint64_t{8000},uint64_t{12000}}) {
    MotionLatch first;
    Check(first.Arm(0,true) && first.Read(time)==0, "already expired first Read never emits RT");
    Failed(first,Error::MissingDownAck);
  }
}

void CancellationAndRepeatedArm() {
  for (bool cancel : {false,true}) {
    for (unsigned phase=0;phase!=4;++phase) {
      MotionLatch value;
      if (phase) Check(value.Arm(0,true) && value.Read(0)==96, "abort active fixture");
      if (phase>=2) value.Poll(3000);
      if (phase==3) value.Read(3001);
      const uint64_t now=phase<2?1:3002;
      if(cancel)value.Cancel(now);else value.Fail(now);
      Failed(value,cancel?Error::Cancelled:Error::ExplicitFailure);
      Check(value.Read(now)==0 && !value.Arm(now,true), "abort permanently neutral in every phase");
      Check(value.snapshot().first_failure_observed_ms==now && value.snapshot().first_failure_clock_valid,
            "actual first failure sample retained");
      value.Fail(now+1);value.Cancel(now+2);
      Failed(value,cancel?Error::Cancelled:Error::ExplicitFailure);
      Check(value.snapshot().first_failure_observed_ms==now, "later failures never overwrite first sample");
    }
  }
  for (unsigned phase=0;phase!=3;++phase) {
    MotionLatch value;
    Check(value.Arm(0,true) && value.Read(0)==96,"rearm fixture");
    if(phase)value.Poll(3000);
    if(phase==2)value.Read(3001);
    Check(!value.Arm(phase?3002:1,true),"rearm fails in hold, neutral wait, success");
    Failed(value,Error::RepeatedArm);
    Check(value.Read(4000)==0,"rearm never republishes RT");
  }
}

void ClockAndOverflow() {
  for(unsigned operation=0;operation!=5;++operation) {
    MotionLatch value;
    Check(value.Arm(100,true) && value.Read(200)==96,"rollback fixture");
    switch(operation) {
      case 0:Check(value.Read(199)==0,"rollback Read returns neutral");break;
      case 1:value.Poll(199);break;
      case 2:Check(!value.Arm(199,true),"rollback Arm rejects");break;
      case 3:value.Cancel(199);break;
      case 4:value.Fail(199);break;
    }
    Failed(value,Error::ClockRollback);
    Check(value.snapshot().last_ms==200 && value.snapshot().released_ms==199 &&
          !value.snapshot().released_clock_valid,"rollback sample retained explicitly invalid, no invented valid release");
    Check(value.snapshot().first_failure_observed_ms==199 && !value.snapshot().first_failure_clock_valid,
          "first failure marks invalid clock");
    Check(value.Read(201)==0 && !value.Arm(202,true),"rollback terminal neutral");
  }
  MotionLatch idle;idle.Poll(10);
  Check(!idle.Arm(9,true),"idle rollback also rejects");
  Check(!idle.snapshot().released_ms && !idle.snapshot().published_ms,"idle rollback does not invent publication or release");
  const uint64_t maximum=std::numeric_limits<uint64_t>::max();
  for(uint64_t now : {maximum-7999,maximum}) {
    MotionLatch value;
    Check(!value.Arm(now,true),"overflow rejects publication");
    Failed(value,Error::DeadlineOverflow);
    Check(!value.snapshot().published_ms && value.Read(maximum)==0,"overflow cannot emit RT");
  }
  MotionLatch edge;
  Check(edge.Arm(maximum-8000,true),"largest representable publication");
  Check(edge.snapshot().neutral_ack_deadline_ms==maximum,"maximum ACK deadline exact");
  Check(edge.Read(maximum-8000)==96 && edge.Read(maximum-5000)==0,"max-edge hold and neutral");
  Check(edge.snapshot().status==Status::Succeeded && edge.Read(maximum)==0,"max-edge terminal neutral");
}

void ExactAmplitudeCases() {
  Check(MotionLatch{}.snapshot().right_trigger == 96, "default amplitude remains RT96");
  for (uint32_t amplitude : {96u, 255u}) {
    MotionLatch value;
    Check(value.Arm(100, true, amplitude), "exact allowed amplitude arms");
    Check(value.snapshot().right_trigger == amplitude, "successful arm latches exact amplitude");
    Check(value.Read(100) == amplitude && value.Read(3099) == amplitude, "selected amplitude until fixed deadline");
    Check(value.Read(3100) == 0 && value.snapshot().status == Status::Succeeded,
          "both amplitudes neutralize at same three-second boundary");
    Check(value.snapshot().release_deadline_ms == 3100 && value.snapshot().neutral_ack_deadline_ms == 8100,
          "amplitude cannot change deadline or ACK budget");
    Check(!value.Arm(3101, true, amplitude == 96 ? 255 : 96), "amplitude switch cannot rearm");
    Failed(value, Error::RepeatedArm);
    Check(value.snapshot().right_trigger == amplitude && value.Read(3102) == 0,
          "latched amplitude immutable after success and attempted rearm");
    MotionLatch active;
    Check(active.Arm(0, true, amplitude) && active.Read(1) == amplitude, "active amplitude fixture");
    Check(!active.Arm(2, true, amplitude == 96 ? 255 : 96) && active.Read(3) == 0 &&
          active.snapshot().right_trigger == amplitude, "active amplitude replacement fails closed");
    MotionLatch no_read;
    Check(no_read.Arm(0, true, amplitude) && no_read.Read(3000) == 0,
          "no late first response at either amplitude");
    Failed(no_read, Error::MissingDownAck);
    MotionLatch late_ack;
    Check(late_ack.Arm(0, true, amplitude) && late_ack.Read(1) == amplitude && late_ack.Read(8000) == 0,
          "neutral ACK deadline unchanged at both amplitudes");
    Failed(late_ack, Error::MissingNeutralAck);
    MotionLatch overflow;
    Check(!overflow.Arm(std::numeric_limits<uint64_t>::max()-7999, true, amplitude),
          "deadline overflow rejected at both amplitudes");
    Failed(overflow, Error::DeadlineOverflow);
  }
  for (uint32_t invalid : {0u, 1u, 95u, 97u, 254u, 256u, 352u, 511u,
                           std::numeric_limits<uint32_t>::max()}) {
    MotionLatch value;
    Check(!value.Arm(0, true, invalid), "invalid amplitude rejected without byte truncation");
    Failed(value, Error::InvalidRightTrigger);
    Check(!value.snapshot().published_ms && value.snapshot().right_trigger == 96 && value.Read(1) == 0,
          "invalid amplitude never publishes or replaces default snapshot");
    Check(!value.Arm(2, true, 255) && value.Read(3) == 0, "invalid amplitude failure cannot be repaired");
    Failed(value, Error::InvalidRightTrigger);
  }
}
}  // namespace

int main() {
  NormalAndPrerequisite();ZeroAndDeadlines();NoReadsAndLatePoll();
  CancellationAndRepeatedArm();ClockAndOverflow();
  ExactAmplitudeCases();
  std::cout << "motion input: " << checks << " checks, " << failures << " failures\n";
  return failures?1:0;
}
