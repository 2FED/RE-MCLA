#pragma once
// Scalar-only observation bookkeeping. Caller supplies lexical IDs and exact
// phase binding; this does not establish that binding, resolve a CTR target,
// execute a dispatch, read guest memory, or prove simulation/consumer internals.
// One Model per independently identified thread lifetime; no internal locking.
// No clocks are minted here. A transport may separately bracket each call.
#include <array>
#include <cstddef>
#include <cstdint>

namespace mcla::router_dispatch {
struct InvocationId {
  uint64_t thread_epoch = 0, serial = 0;
  bool operator==(const InvocationId&) const = default;
};
struct UpdateBinding {
  uint64_t phase_call_id = 0, thread_epoch = 0;
  uint32_t native_thread_id = 0;
  bool operator==(const UpdateBinding&) const = default;
};
struct Entry {
  InvocationId invocation;
  UpdateBinding update; // All zero means unbound, never a guessed phase.
  uint64_t caller_lr = 0, manager_r3 = 0, selector_r4 = 0, f1_bits = 0;
  bool operator==(const Entry&) const = default;
};
struct Dispatch {
  uint64_t r3 = 0, table = 0, ctr = 0, f1_bits = 0;
  bool present = false;
  // CTR's low 32 bits are the requested guest dispatch address, NOT a proven
  // resolved host function. Zero/raw upper bits remain observed, not repaired.
  uint32_t requested_target_u32() const noexcept { return static_cast<uint32_t>(ctr); }
  bool operator==(const Dispatch&) const = default;
};
enum class Stage : uint8_t {
  kEntered, kPredicateActive, kPredicateReturned, kSkipped,
  kConsumerActive, kConsumerReturned, kDispatchException
};
enum class EventKind : uint8_t {
  kEntry, kPredicatePre, kPredicateReturn, kPredicateException, kSkip,
  kConsumerPre, kConsumerReturn, kConsumerException, kNormalExit, kExceptionalExit
};
enum class Fault : uint8_t { kNone, kIdentity, kTransition, kCapacity, kDepth };
struct Frame {
  Entry entry;
  InvocationId parent;
  Stage stage = Stage::kEntered;
  Dispatch predicate, consumer; // predicate table=r10; consumer table=r11.
  uint64_t predicate_result_r3 = 0;
  uint8_t predicate_result_low8 = 0;
  bool predicate_returned = false;
  bool operator==(const Frame&) const = default;
};
struct Event {
  EventKind kind = EventKind::kEntry;
  Frame frame; // Full detached scalar snapshot, including exact invocation IDs.
  bool operator==(const Event&) const = default;
};

template <size_t Capacity = 4096, size_t Depth = 8>
class Model {
  static_assert(Capacity > 0 && Capacity <= 65536 && Depth > 0 && Depth <= 64);
 public:
  explicit Model(uint64_t thread_epoch) noexcept : epoch_(thread_epoch) {
    if (!epoch_) fault_ = Fault::kIdentity;
  }
  Model(const Model&) = delete;
  Model& operator=(const Model&) = delete;
  Model(Model&&) = delete;
  Model& operator=(Model&&) = delete;

  bool Enter(const Entry& entry) noexcept {
    if (!Writable()) return false;
    const auto& b = entry.update;
    const bool unbound = !b.phase_call_id && !b.thread_epoch && !b.native_thread_id;
    // Shape only: lexical binding, lane, exit binding and phase health remain
    // the native integration's responsibility, not inferred from these IDs.
    const bool bound = b.phase_call_id && b.thread_epoch && b.native_thread_id;
    if (entry.invocation.thread_epoch != epoch_ || !entry.invocation.serial ||
        entry.invocation.serial <= last_serial_ || (!unbound && !bound))
      return Fail(Fault::kIdentity);
    if (depth_ == Depth) return Fail(Fault::kDepth);
    if (!Room()) return false;
    Frame frame;
    frame.entry = entry;
    if (depth_) frame.parent = stack_[depth_-1].entry.invocation;
    stack_[depth_++] = frame;
    last_serial_ = entry.invocation.serial;
    Emit(EventKind::kEntry);
    return true;
  }
  bool PredicatePre(InvocationId id, uint64_t r3, uint64_t r10, uint64_t ctr) noexcept {
    if (!At(id, Stage::kEntered)) return false;
    auto& f = stack_[depth_-1];
    f.predicate = {r3, r10, ctr, 0, true};
    f.stage = Stage::kPredicateActive;
    Emit(EventKind::kPredicatePre); return true;
  }
  bool PredicateReturn(InvocationId id, uint64_t r3) noexcept {
    if (!At(id, Stage::kPredicateActive)) return false;
    auto& f = stack_[depth_-1];
    f.predicate_result_r3 = r3;
    f.predicate_result_low8 = static_cast<uint8_t>(r3 & 0xff);
    f.predicate_returned = true;
    f.stage = Stage::kPredicateReturned;
    Emit(EventKind::kPredicateReturn); return true;
  }
  bool PredicateException(InvocationId id) noexcept {
    return DispatchException(id, Stage::kPredicateActive, EventKind::kPredicateException);
  }
  bool Skip(InvocationId id) noexcept {
    if (!At(id, Stage::kPredicateReturned)) return false;
    auto& f = stack_[depth_-1];
    if (f.predicate_result_low8) return Fail(Fault::kTransition);
    f.stage = Stage::kSkipped;
    Emit(EventKind::kSkip); return true;
  }
  bool ConsumerPre(InvocationId id, uint64_t r3, uint64_t r11, uint64_t ctr,
                   uint64_t f1_bits) noexcept {
    if (!At(id, Stage::kPredicateReturned)) return false;
    auto& f = stack_[depth_-1];
    if (!f.predicate_result_low8) return Fail(Fault::kTransition);
    f.consumer = {r3, r11, ctr, f1_bits, true};
    f.stage = Stage::kConsumerActive;
    Emit(EventKind::kConsumerPre); return true;
  }
  bool ConsumerReturn(InvocationId id) noexcept {
    if (!At(id, Stage::kConsumerActive)) return false;
    stack_[depth_-1].stage = Stage::kConsumerReturned;
    Emit(EventKind::kConsumerReturn); return true;
  }
  bool ConsumerException(InvocationId id) noexcept {
    return DispatchException(id, Stage::kConsumerActive, EventKind::kConsumerException);
  }
  bool Exit(InvocationId id, bool normal) noexcept {
    if (!Top(id) || !Room()) return false;
    const auto stage = stack_[depth_-1].stage;
    if (normal && stage != Stage::kSkipped && stage != Stage::kConsumerReturned)
      return Fail(Fault::kTransition);
    // An exceptional exit may precede either dispatch (for example a guest
    // load fault). It is NOT an inferred predicate/consumer exception.
    Emit(normal ? EventKind::kNormalExit : EventKind::kExceptionalExit);
    stack_[--depth_] = {};
    return true;
  }
  void Freeze() noexcept { frozen_ = true; } // No fake returns or draining.
  bool frozen() const noexcept { return frozen_; }
  Fault fault() const noexcept { return fault_; }
  size_t size() const noexcept { return size_; }
  size_t pending_count() const noexcept { return depth_; }
  const std::array<Event, Capacity>& events() const noexcept { return events_; }
  const std::array<Frame, Depth>& pending() const noexcept { return stack_; }

 private:
  bool Writable() const noexcept { return !frozen_ && fault_ == Fault::kNone; }
  bool Fail(Fault fault) noexcept { if (Writable()) fault_ = fault; return false; }
  bool Room() noexcept { return size_ < Capacity || Fail(Fault::kCapacity); }
  bool Top(InvocationId id) noexcept {
    if (!Writable()) return false;
    if (!depth_ || stack_[depth_-1].entry.invocation != id) return Fail(Fault::kIdentity);
    return true;
  }
  bool At(InvocationId id, Stage stage) noexcept {
    if (!Top(id)) return false;
    if (stack_[depth_-1].stage != stage) return Fail(Fault::kTransition);
    return Room();
  }
  bool DispatchException(InvocationId id, Stage stage, EventKind event) noexcept {
    if (!At(id, stage)) return false;
    stack_[depth_-1].stage = Stage::kDispatchException;
    Emit(event); return true;
  }
  void Emit(EventKind event) noexcept { events_[size_++] = {event, stack_[depth_-1]}; }
  uint64_t epoch_ = 0, last_serial_ = 0;
  size_t size_ = 0, depth_ = 0;
  Fault fault_ = Fault::kNone;
  bool frozen_ = false;
  std::array<Event, Capacity> events_{};
  std::array<Frame, Depth> stack_{};
};
} // namespace mcla::router_dispatch
