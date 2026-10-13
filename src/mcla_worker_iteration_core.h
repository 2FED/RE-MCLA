#pragma once
// Caller-serialized, per-worker inventory only. No clocks, guest reads, native
// operations or allocation. The caller witnesses every boundary and SDK event;
// this model cannot prove observation coverage or release/update causality.
#include <cstdint>
#include <limits>
#include <optional>

namespace mcla::worker_iteration {
struct Lane {
  uintptr_t context{}, base{};
  bool operator==(const Lane&) const = default;
};
struct Key {
  uint64_t thread_epoch{}, worker_serial{};
  Lane lane;
  uint32_t owner{};
  uint64_t object_epoch{};
  bool operator==(const Key&) const = default;
};
enum class Outcome { kSuccess, kTimeout, kApc, kFailed, kUnknown };
// Supplied only after the collector has validated its exact scoped SDK
// observation. Raw SDK entry/exit clocks stay in that journal; the core never
// synthesizes them from helper-return time. exactly_one covers ALL admissions
// in this helper, including unrelated/unsupported/extra admissions.
struct ValidatedWait {
  uint64_t operation_id{}, object_epoch{};
  Outcome outcome = Outcome::kUnknown;
  bool exactly_one = false;
};
enum class Event {
  kIgnored, kEntered, kWaitEntered,
  kWaitEligible, kWaitRejected, kDispatchEntered, kRenderConsumed,
  kDispatchCompleted, kExited, kFrozen, kFault
};
enum class Reason {
  kNone, kInactive, kTerminal, kBadKey, kClock, kState, kIdentity,
  kCaller, kObject, kExtraOperation, kMissingOperation, kOutcome,
  kNullHandle, kWaitUnsuccessful, kUnwind, kDuplicateRender,
  kNextWait, kNoRender, kWorkerExit, kFreeze, kObserverFault
};
struct Transition {
  Event event = Event::kIgnored;
  Reason reason = Reason::kNone;
  // On non-render events these are diagnostic scope IDs, not an association.
  // Only kRenderConsumed can report a consumed wait -> actual render edge.
  uint64_t helper_id{}, operation_id{}, dispatch_id{}, phase_call_id{};
  uint64_t retired_wait_operation{};
  Reason retirement_reason = Reason::kNone;
  bool eligible = false;
  // Exact phase ID supplied at actual render entry. Zero still consumes the
  // slot, but cannot bind a captured source to an admitted phase census row.
  bool phase_bound = false;
};
struct Counts {
  uint64_t waits{}, eligible{}, rejected{}, retired{}, renders{},
      unbound_renders{}, no_render_dispatches{};
};

class Worker {
 public:
  static constexpr uint64_t kWaitCaller = 0x82305EB8;
  static constexpr uint64_t kRenderCaller = 0x82305F80;
  static constexpr bool kNativeAdmitted = false, kUpdateCausalityProven = false,
      kReleaseCausalityProven = false, kFrameToStepMappingProven = false,
      kSimulationProven = false, kDisplayProven = false, kFpsProven = false;
  Worker() = default;
  Worker(const Worker&) = delete;
  Worker& operator=(const Worker&) = delete;
  Worker(Worker&&) = delete;
  Worker& operator=(Worker&&) = delete;
  bool failed() const noexcept { return failed_; }
  bool frozen() const noexcept { return frozen_; }
  bool active() const noexcept { return entered_ && !exited_ && !frozen_ && !failed_; }
  Counts counts() const noexcept { return counts_; }

  Transition Enter(Key key, uint64_t qpc) noexcept {
    if (entered_ || failed_ || frozen_ || exited_) return Fault(Reason::kState);
    if (!Valid(key)) return Fault(Reason::kBadKey);
    if (!Clock(qpc)) return Fault(Reason::kClock);
    key_ = key; entered_ = true;
    return Record(Event::kEntered);
  }
  Transition WaitEnter(Key key, uint64_t qpc, uint64_t helper_id,
                       uint64_t caller_lr, uint32_t guest_handle) noexcept {
    if (auto failure = Check(key, qpc)) return *failure;
    if (state_ != State::kIdle) return Fault(Reason::kState);
    if (caller_lr != kWaitCaller) return Fault(Reason::kCaller);
    if (!helper_id || helper_id <= last_helper_) return Fault(Reason::kIdentity);
    auto result = Record(Event::kWaitEntered);
    Retire(result, Reason::kNextWait);
    helper_ = last_helper_ = helper_id;
    operation_ = 0;
    null_handle_ = guest_handle == 0;
    state_ = State::kWait; ++counts_.waits;
    result.helper_id = helper_;
    result.operation_id = 0; result.eligible = false;
    return result;
  }
  Transition WaitExit(Key key, uint64_t qpc, uint64_t helper_id,
                      bool normal_return, std::optional<ValidatedWait> observation = {}) noexcept {
    if (auto failure = Check(key, qpc)) return *failure;
    if (state_ != State::kWait || helper_id != helper_) return Fault(Reason::kState);
    if (!normal_return) return Fault(Reason::kUnwind);
    if (null_handle_ && observation) return Fault(Reason::kObject);
    if (!null_handle_) {
      if (!observation) return Fault(Reason::kMissingOperation);
      if (!observation->exactly_one) return Fault(Reason::kExtraOperation);
      if (observation->object_epoch != key_.object_epoch) return Fault(Reason::kObject);
      if (!observation->operation_id || observation->operation_id <= last_operation_)
        return Fault(Reason::kIdentity);
      switch (observation->outcome) {
        case Outcome::kSuccess: case Outcome::kTimeout: case Outcome::kApc: case Outcome::kFailed: break;
        default: return Fault(Reason::kOutcome);
      }
      operation_ = last_operation_ = observation->operation_id;
    }
    state_ = State::kIdle;
    if (null_handle_ || observation->outcome != Outcome::kSuccess) {
      ++counts_.rejected;
      auto result = Record(Event::kWaitRejected,
                           null_handle_ ? Reason::kNullHandle : Reason::kWaitUnsuccessful);
      ClearWait(); return result;
    }
    eligible_ = operation_; ++counts_.eligible;
    return Record(Event::kWaitEligible);
  }
  Transition DispatchEnter(Key key, uint64_t qpc, uint64_t dispatch_id,
                           uint64_t caller_lr, uint32_t closure_address) noexcept {
    if (auto failure = Check(key, qpc)) return *failure;
    if (state_ != State::kIdle) return Fault(Reason::kState);
    if (caller_lr != kRenderCaller) return Fault(Reason::kCaller);
    if (closure_address != key_.owner + 88u) return Fault(Reason::kObject);
    if (!dispatch_id || dispatch_id <= last_dispatch_) return Fault(Reason::kIdentity);
    dispatch_ = last_dispatch_ = dispatch_id;
    state_ = State::kDispatch; render_seen_ = false;
    // Ineligible iterations remain representable. Their render is explicitly
    // unlinked, rather than borrowing a preceding iteration's successful wait.
    return Record(Event::kDispatchEntered);
  }
  Transition Render(Key key, uint64_t qpc, uint64_t phase_call_id,
                    uint64_t caller_lr) noexcept {
    if (auto failure = Check(key, qpc)) return *failure;
    if (state_ != State::kDispatch) return Fault(Reason::kState);
    if (caller_lr != kRenderCaller) return Fault(Reason::kCaller);
    if (render_seen_) return Fault(Reason::kDuplicateRender);
    if (phase_call_id && phase_call_id <= last_phase_) return Fault(Reason::kIdentity);
    if (phase_call_id) last_phase_ = phase_call_id;
    render_seen_ = true; ++counts_.renders;
    auto result = Record(Event::kRenderConsumed);
    result.operation_id = eligible_;
    result.phase_call_id = phase_call_id;
    result.phase_bound = eligible_ != 0 && phase_call_id != 0;
    result.eligible = eligible_ != 0;
    if (!result.phase_bound) ++counts_.unbound_renders;
    eligible_ = 0; // Includes phase ID zero: never retain a consumed slot.
    return result;
  }
  Transition DispatchExit(Key key, uint64_t qpc, uint64_t dispatch_id,
                          bool normal_return) noexcept {
    if (auto failure = Check(key, qpc)) return *failure;
    if (state_ != State::kDispatch || dispatch_id != dispatch_) return Fault(Reason::kState);
    if (!normal_return) return Fault(Reason::kUnwind);
    auto result = Record(Event::kDispatchCompleted);
    if (!render_seen_) { ++counts_.no_render_dispatches; Retire(result, Reason::kNoRender); }
    state_ = State::kIdle; dispatch_ = 0; ClearWait();
    result.eligible = false;
    return result;
  }
  Transition Exit(Key key, uint64_t qpc, bool normal_return) noexcept {
    if (auto failure = Check(key, qpc)) return *failure;
    if (!normal_return) return Fault(Reason::kUnwind);
    if (state_ != State::kIdle) return Fault(Reason::kState);
    auto result = Record(Event::kExited);
    Retire(result, Reason::kWorkerExit); ClearWait(); exited_ = true;
    result.eligible = false;
    return result;
  }
  Transition Freeze(Key key, uint64_t qpc) noexcept {
    if (auto failure = Check(key, qpc)) return *failure;
    // Raw journal owns pending operations. Snapshot IDs identify them without
    // claiming that either the SDK operation or its helper returned.
    auto result = Record(Event::kFrozen);
    Retire(result, Reason::kFreeze); frozen_ = true;
    result.eligible = false;
    return result;
  }
  Transition ObserverFault() noexcept { return Fault(Reason::kObserverFault); }

 private:
  enum class State { kIdle, kWait, kDispatch };
  static bool Valid(const Key& key) noexcept {
    return key.thread_epoch && key.worker_serial && key.lane.context && key.lane.base &&
           key.owner && key.owner <= UINT32_MAX - 88u && key.object_epoch;
  }
  bool Clock(uint64_t qpc) noexcept {
    if (!qpc || qpc > uint64_t(std::numeric_limits<int64_t>::max()) || qpc < last_qpc_)
      return false;
    last_qpc_ = qpc; return true;
  }
  std::optional<Transition> Check(Key key, uint64_t qpc) noexcept {
    if (failed_ || frozen_ || exited_) return Record(Event::kIgnored, Reason::kTerminal);
    if (!entered_) return Fault(Reason::kInactive);
    if (key != key_) return Fault(Reason::kBadKey);
    if (!Clock(qpc)) return Fault(Reason::kClock);
    return {};
  }
  Transition Record(Event event, Reason reason = Reason::kNone) const noexcept {
    Transition result;
    result.event = event; result.reason = reason; result.helper_id = helper_;
    result.operation_id = operation_; result.dispatch_id = dispatch_;
    result.eligible = eligible_ != 0;
    return result;
  }
  void Retire(Transition& result, Reason reason) noexcept {
    if (eligible_) {
      result.retired_wait_operation = eligible_; result.retirement_reason = reason;
      ++counts_.retired; eligible_ = 0;
    }
  }
  void ClearWait() noexcept { helper_ = operation_ = 0; null_handle_ = false; }
  Transition Fault(Reason reason) noexcept {
    if (failed_ || frozen_ || exited_) return Record(Event::kIgnored, Reason::kTerminal);
    auto result = Record(Event::kFault, reason);
    Retire(result, reason); failed_ = true; result.eligible = false;
    return result;
  }
  Key key_{};
  State state_ = State::kIdle;
  uint64_t last_qpc_{}, last_helper_{}, last_operation_{}, last_dispatch_{}, last_phase_{};
  uint64_t helper_{}, operation_{}, dispatch_{}, eligible_{};
  bool entered_{}, exited_{}, frozen_{}, failed_{}, null_handle_{}, render_seen_{};
  Counts counts_{};
};
} // namespace mcla::worker_iteration
