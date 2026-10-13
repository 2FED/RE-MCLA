// Pure offline protocol model. Does not execute SDK, Win32, game or clocks.
#include "../src/mcla_worker_iteration_core.h"
#include <iostream>
#include <type_traits>

namespace {
using namespace mcla::worker_iteration;
unsigned checks{}, failures{};
void Check(bool value, const char* label) {
  ++checks;
  if (!value) { ++failures; std::cerr << "FAIL: " << label << '\n'; }
}
Key GoodKey() { return {1, 2, {0x1000, 0x2000}, 0x3000, 4}; }
ValidatedWait Observed(uint64_t id, Outcome outcome = Outcome::kSuccess) {
  return {id, 4, outcome, true};
}
struct Fixture {
  Worker worker;
  Key key = GoodKey();
  uint64_t qpc = 10;
  Fixture() { Check(worker.Enter(key, qpc++).event == Event::kEntered, "worker entered"); }
  Transition BeginWait(uint64_t helper, uint32_t handle = 9) {
    return worker.WaitEnter(key, qpc++, helper, Worker::kWaitCaller, handle);
  }
  Transition Wait(uint64_t helper, uint64_t operation, Outcome outcome = Outcome::kSuccess) {
    Check(BeginWait(helper).event == Event::kWaitEntered, "wait entered");
    return worker.WaitExit(key, qpc++, helper, true, Observed(operation, outcome));
  }
  Transition Dispatch(uint64_t id) {
    return worker.DispatchEnter(key, qpc++, id, Worker::kRenderCaller, key.owner + 88);
  }
  Transition Render(uint64_t phase) {
    return worker.Render(key, qpc++, phase, Worker::kRenderCaller);
  }
  Transition EndDispatch(uint64_t id) { return worker.DispatchExit(key, qpc++, id, true); }
};
void HappyAndSkipped() {
  Fixture f;
  Check(f.Wait(1, 10).eligible, "first SDK success becomes eligible after helper return");
  auto next = f.BeginWait(2);
  Check(next.retired_wait_operation == 10 && next.retirement_reason == Reason::kNextWait,
        "skip-render retires exact prior operation at next wait entry");
  Check(!next.eligible && next.operation_id == 0 && next.helper_id == 2, "new wait is not stale");
  Check(f.worker.WaitExit(f.key, f.qpc++, 2, true, Observed(11)).eligible, "second wait eligible");
  next = f.BeginWait(3);
  Check(next.retired_wait_operation == 11, "second skipped iteration explicit");
  Check(f.worker.WaitExit(f.key, f.qpc++, 3, true, Observed(12)).eligible, "third wait eligible");
  Check(f.Dispatch(1).event == Event::kDispatchEntered, "dispatch entered");
  auto render = f.Render(77);
  Check(render.operation_id == 12 && render.helper_id == 3 && render.dispatch_id == 1 &&
        render.phase_call_id == 77 && render.phase_bound, "only current iteration binds exact phase ID");
  Check(f.EndDispatch(1).retired_wait_operation == 0, "consumed credit not retired twice");
  Check(f.worker.Exit(f.key, f.qpc++, true).event == Event::kExited, "normal worker exit");
  const auto counts = f.worker.counts();
  Check(counts.waits == 3 && counts.eligible == 3 && counts.retired == 2 && counts.renders == 1,
        "explicit iteration counts");
  Check(!f.worker.failed(), "control iterations do not poison valid worker");
}
void NoopAndPhaseZero() {
  Fixture f;
  f.Wait(1, 10); f.Dispatch(1);
  auto ended = f.EndDispatch(1);
  Check(ended.retired_wait_operation == 10 && ended.retirement_reason == Reason::kNoRender,
        "closure no-op retires credit");
  Check(f.worker.counts().no_render_dispatches == 1, "no-op counted");
  f.Dispatch(2);
  auto unlinked = f.Render(1);
  Check(!unlinked.phase_bound && unlinked.operation_id == 0, "later dispatch cannot borrow no-op credit");
  f.EndDispatch(2);
  f.Wait(2, 11); f.Dispatch(3);
  auto dropped = f.Render(0);
  Check(dropped.operation_id == 11 && !dropped.phase_bound && dropped.eligible,
        "unadmitted phase consumes exact credit without phase binding");
  Check(f.EndDispatch(3).retired_wait_operation == 0, "phase zero did consume slot");
  f.Dispatch(4);
  Check(f.Render(2).operation_id == 0, "phase zero cannot donate credit to later phase");
  f.EndDispatch(4);
  Check(f.worker.counts().unbound_renders == 3, "all unbound renders counted");
}
void RejectedWaits() {
  for (auto outcome : {Outcome::kTimeout, Outcome::kApc, Outcome::kFailed}) {
    Fixture f;
    auto result = f.Wait(1, 10, outcome);
    Check(result.event == Event::kWaitRejected && result.reason == Reason::kWaitUnsuccessful &&
          result.operation_id == 10 && !result.eligible, "failed wait is observed but never credit");
    f.Dispatch(1);
    Check(!f.Render(1).phase_bound, "failed wait render remains unlinked");
    f.EndDispatch(1);
    Check(f.Wait(2, 11).eligible && !f.worker.failed(), "ordinary failed wait permits next valid iteration");
  }
  Fixture null;
  null.BeginWait(1, 0);
  auto no_operation = null.worker.WaitExit(null.key, null.qpc++, 1, true);
  Check(no_operation.reason == Reason::kNullHandle && !null.worker.failed(), "null helper fast return is ineligible");
  Check(null.Wait(2, 10).eligible, "null helper does not invent an SDK operation ID");
}
void MalformedWaitEvidence() {
  for (unsigned test = 0; test != 8; ++test) {
    Fixture f; f.BeginWait(1);
    auto obs = Observed(10);
    if (test == 0) obs.exactly_one = false;
    if (test == 1) obs.object_epoch = 5;
    if (test == 2) obs.operation_id = 0;
    if (test == 3) obs.outcome = Outcome::kUnknown;
    if (test == 4) obs.outcome = static_cast<Outcome>(999);
    auto result = test == 5 ? f.worker.WaitExit(f.key, f.qpc++, 1, true) :
        f.worker.WaitExit(f.key, f.qpc++, test == 6 ? 2 : 1, test != 7, obs);
    Check(result.event == Event::kFault && f.worker.failed(), "malformed/missing/extra/unwound wait evidence poisons");
    Check(f.Dispatch(1).event == Event::kIgnored, "fault cannot be bypassed by dispatch");
  }
  Fixture null; null.BeginWait(1, 0);
  Check(null.worker.WaitExit(null.key, null.qpc++, 1, true, Observed(1)).reason == Reason::kObject,
        "null helper cannot contain purported SDK wait");
  Fixture repeated; repeated.Wait(1, 10); repeated.BeginWait(2);
  Check(repeated.worker.WaitExit(repeated.key, repeated.qpc++, 2, true, Observed(10)).reason == Reason::kIdentity,
        "SDK operation cannot be reused across iterations");
}
void IdentityAndOrdering() {
  for (unsigned test = 0; test != 6; ++test) {
    Fixture f; f.Wait(1, 10);
    auto key = f.key;
    if (test == 0) ++key.thread_epoch;
    if (test == 1) ++key.worker_serial;
    if (test == 2) ++key.lane.context;
    if (test == 3) ++key.lane.base;
    if (test == 4) ++key.owner;
    if (test == 5) ++key.object_epoch;
    auto result = f.worker.DispatchEnter(key, f.qpc++, 1, Worker::kRenderCaller, key.owner + 88);
    Check(result.reason == Reason::kBadKey && result.retired_wait_operation == 10,
          "lane/owner/object/thread/lifetime mismatch retires eligibility and faults");
  }
  Fixture duplicate; duplicate.Wait(1, 10); duplicate.Dispatch(1); duplicate.Render(1);
  Check(duplicate.Render(2).reason == Reason::kDuplicateRender, "one render per dispatch");
  Fixture old_phase; old_phase.Wait(1, 10); old_phase.Dispatch(1); old_phase.Render(2); old_phase.EndDispatch(1);
  old_phase.Wait(2, 11); old_phase.Dispatch(2);
  Check(old_phase.Render(2).reason == Reason::kIdentity, "reused nonzero phase ID rejected");
  Fixture no_dispatch; no_dispatch.Wait(1, 10);
  Check(no_dispatch.Render(1).reason == Reason::kState, "render requires actual dispatch scope");
  Fixture nested; nested.BeginWait(1);
  Check(nested.BeginWait(2).reason == Reason::kState, "nested target wait rejected");
  Fixture serial; serial.Wait(1, 10);
  Check(serial.BeginWait(1).reason == Reason::kIdentity, "helper IDs never reused");
  Fixture dispatcher; dispatcher.Dispatch(2); dispatcher.EndDispatch(2);
  Check(dispatcher.Dispatch(1).reason == Reason::kIdentity, "dispatch IDs never regress");
}
void CallerAndClock() {
  Fixture wait;
  Check(wait.worker.WaitEnter(wait.key, wait.qpc++, 1, 0x82305E2C, 9).reason == Reason::kCaller,
        "coordinator +24 wait is not worker +20 wait");
  Fixture sync; sync.Wait(1, 10);
  Check(sync.worker.DispatchEnter(sync.key, sync.qpc++, 1, 0x82305DFC, sync.key.owner + 88).reason == Reason::kCaller,
        "synchronous render route cannot borrow worker wait");
  Fixture owner; owner.Wait(1, 10);
  Check(owner.worker.DispatchEnter(owner.key, owner.qpc++, 1, Worker::kRenderCaller, owner.key.owner + 84).reason == Reason::kObject,
        "closure owner relation enforced");
  Fixture render; render.Dispatch(1);
  Check(render.worker.Render(render.key, render.qpc++, 1, 0x82305DFC).reason == Reason::kCaller,
        "actual callback caller is checked");
  for (auto qpc : {uint64_t(0), uint64_t(9), UINT64_MAX}) {
    Fixture f;
    Check(f.worker.WaitEnter(f.key, qpc, 1, Worker::kWaitCaller, 9).reason == Reason::kClock,
          "zero/backwards/out-of-range clocks rejected");
  }
  Fixture equal;
  Check(equal.worker.WaitEnter(equal.key, 10, 1, Worker::kWaitCaller, 9).event == Event::kWaitEntered,
        "equal QPC is allowed: order comes from explicit boundaries, not clock separation");
}
void FreezeFaultExit() {
  Fixture pending; pending.BeginWait(1);
  auto frozen = pending.worker.Freeze(pending.key, pending.qpc++);
  Check(frozen.event == Event::kFrozen && frozen.helper_id == 1 && frozen.operation_id == 0 && !frozen.eligible,
        "freeze midwait preserves helper identity without inventing completion");
  const auto before = pending.worker.counts();
  Check(pending.worker.WaitExit(pending.key, pending.qpc++, 1, true, Observed(10)).event == Event::kIgnored,
        "late helper completion cannot mutate frozen eligibility");
  Check(pending.worker.counts().eligible == before.eligible && pending.worker.counts().waits == before.waits,
        "frozen counters remain unchanged");
  Fixture eligible; eligible.Wait(1, 10);
  Check(eligible.worker.Freeze(eligible.key, eligible.qpc++).retired_wait_operation == 10,
        "freeze retires eligible wait");
  Fixture dispatch; dispatch.Wait(1, 10); dispatch.Dispatch(1);
  Check(dispatch.worker.Freeze(dispatch.key, dispatch.qpc++).retired_wait_operation == 10,
        "freeze mid-dispatch retires unconsumed wait");
  Check(dispatch.Render(1).event == Event::kIgnored, "frozen dispatch cannot render-bind");
  Fixture unwind; unwind.Wait(1, 10); unwind.Dispatch(1);
  Check(unwind.worker.DispatchExit(unwind.key, unwind.qpc++, 1, false).reason == Reason::kUnwind,
        "original dispatch exception fails closed");
  Fixture fault; fault.Wait(1, 10);
  auto failed = fault.worker.ObserverFault();
  Check(failed.retired_wait_operation == 10 && failed.reason == Reason::kObserverFault, "observer failure explicit");
  Check(fault.worker.ObserverFault().event == Event::kIgnored, "fault is terminal");
  Fixture exit; exit.Wait(1, 10);
  auto ended = exit.worker.Exit(exit.key, exit.qpc++, true);
  Check(ended.retired_wait_operation == 10 && ended.retirement_reason == Reason::kWorkerExit,
        "worker control exit retires wait");
  Check(exit.worker.Enter(exit.key, exit.qpc++).event == Event::kIgnored, "worker instance cannot be reconstructed");
}
void InitialValidationAndIndependence() {
  for (unsigned test = 0; test != 7; ++test) {
    Worker worker; auto key = GoodKey();
    if (test == 0) key.thread_epoch = 0;
    if (test == 1) key.worker_serial = 0;
    if (test == 2) key.lane.context = 0;
    if (test == 3) key.lane.base = 0;
    if (test == 4) key.owner = 0;
    if (test == 5) key.owner = UINT32_MAX;
    if (test == 6) key.object_epoch = 0;
    Check(worker.Enter(key, 1).reason == Reason::kBadKey, "invalid lifetime identity rejected");
  }
  Worker unopened;
  Check(unopened.WaitEnter(GoodKey(), 1, 1, Worker::kWaitCaller, 9).reason == Reason::kInactive,
        "entry coverage required");
  Fixture a, b;
  a.Wait(1, 10); b.Wait(1, 20); a.Dispatch(1); b.Dispatch(1);
  Check(a.Render(100).operation_id == 10 && b.Render(200).operation_id == 20,
        "independent worker slots never consult a global latest wait");
  // There is deliberately no Release API. A self/control release changes the
  // separate SDK journal, never creates, replaces or refreshes a worker slot.
  a.EndDispatch(1); a.Dispatch(2);
  Check(a.Render(101).operation_id == 0, "no implicit self-release credit");
  Check(!Worker::kNativeAdmitted && !Worker::kUpdateCausalityProven && !Worker::kSimulationProven &&
        !Worker::kDisplayProven && !Worker::kFpsProven, "no native/causal/performance claim");
}
} // namespace
int main() {
  static_assert(!std::is_copy_constructible_v<Worker> && !std::is_move_constructible_v<Worker>);
  HappyAndSkipped(); NoopAndPhaseZero(); RejectedWaits(); MalformedWaitEvidence();
  IdentityAndOrdering(); CallerAndClock(); FreezeFaultExit(); InitialValidationAndIndependence();
  std::cout << "Worker iteration core: " << checks << " checks, " << failures << " failures\n";
  return failures ? 1 : 0;
}
