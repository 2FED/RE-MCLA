#pragma once
// Scalar downstream-body bookkeeping only. The caller supplies an independently
// witnessed active consumer child0, actual entry, TLS lifetime identities and
// synchronization. No memory reads, clocks, allocation, target resolution or
// original-work callback. This model does not prove execution or simulation.
// Own, parent-detail, parent-router and Update epochs are separate namespaces.
// First successful Enter fixes both parent lane epochs; all three invocation
// serials increase strictly, including after exit (no recycling or wrapping).
#include "mcla_consumer_detail_record.h"
#include <array>
#include <cstddef>
#include <cstdint>

namespace mcla::virtual_detail {
using InvocationId = consumer_detail::InvocationId;
constexpr uint32_t kTarget = 0x8258D1A0;
constexpr uint64_t kCaller = 0x822C78C0;
constexpr std::array<uint64_t, 3> kCallReturnLr{0x8258D1E8, 0x8258D1FC, 0x8258D214};
constexpr bool kNativeExecutionProven = false, kResolvedTargetsProven = false,
               kSimulationProven = false;

struct Entry {
  InvocationId invocation;
  consumer_detail::Entry parent_entry;
  consumer_detail::Child parent_child;
  uint32_t actual_target = 0;
  uint64_t caller_lr = 0, r3 = 0, f1_bits = 0;
  bool operator==(const Entry&) const = default;
};
enum class Stage : uint8_t { kEntered, kCallActive, kCallReturned, kCallException };
enum class CallState : uint8_t { kAbsent, kActive, kReturned, kException };
enum class EventKind : uint8_t { kEntry, kCallPre, kCallReturn, kCallException, kNormalExit, kExceptionalExit };
enum class Fault : uint8_t { kNone, kIdentity, kTransition, kCapacity, kDepth };
struct Call {
  uint64_t caller_lr = 0, r3 = 0, f1_bits = 0, indirect_ctr = 0;
  uint32_t requested_target = 0;  // Low32 CTR, never a resolved host function.
  CallState state = CallState::kAbsent;
  bool operator==(const Call&) const = default;
};
struct Frame {
  Entry entry;
  InvocationId parent;  // Nearest admitted model frame, not game-root ancestry.
  Stage stage = Stage::kEntered;
  uint8_t returned_calls = 0;
  std::array<Call, 3> calls{};
  bool operator==(const Frame&) const = default;
};
struct Event {
  EventKind kind = EventKind::kEntry;
  Frame frame;  // Detached complete raw scalar snapshot.
  bool operator==(const Event&) const = default;
};

// Large instantiations need caller-owned heap storage, not a default Windows
// stack. All mutations require caller serialization; Freeze does not drain.
template <size_t Capacity = 4096, size_t Depth = 8>
class Model {
  static_assert(Capacity > 0 && Capacity <= 65536 && Depth > 0 && Depth <= 64);
 public:
  explicit Model(uint64_t own_thread_epoch, uint64_t parent_detail_thread_epoch = 0) noexcept
      : epoch_(own_thread_epoch), detail_epoch_(parent_detail_thread_epoch) {
    if (!epoch_) fault_ = Fault::kIdentity;
  }
  Model(const Model&) = delete;
  Model& operator=(const Model&) = delete;
  Model(Model&&) = delete;
  Model& operator=(Model&&) = delete;

  bool Enter(const Entry& entry) noexcept {
    if (!Writable()) return false;
    const auto& p = entry.parent_entry;
    const auto& c = entry.parent_child;
    const auto& b = p.update;
    const bool unbound = !b.phase_call_id && !b.thread_epoch && !b.native_thread_id;
    const bool bound = b.phase_call_id && b.thread_epoch && b.native_thread_id;
    if (entry.invocation.thread_epoch != epoch_ || !entry.invocation.serial ||
        entry.invocation.serial <= last_serial_ || !p.invocation.thread_epoch ||
        (detail_epoch_ && p.invocation.thread_epoch != detail_epoch_) ||
        !p.invocation.serial || p.invocation.serial <= last_detail_serial_ ||
        !p.router.thread_epoch || (router_epoch_ && p.router.thread_epoch != router_epoch_) ||
        !p.router.serial || p.router.serial <= last_router_serial_ || (!bound && !unbound) ||
        p.actual_target != consumer_detail::kTarget || p.caller_lr != consumer_detail::kCaller ||
        c.state != consumer_detail::ChildState::kActive || !c.indirect || c.caller_lr != kCaller ||
        c.requested_target != kTarget || static_cast<uint32_t>(c.indirect_ctr) != kTarget ||
        entry.actual_target != kTarget || entry.caller_lr != kCaller ||
        entry.r3 != c.r3 || entry.f1_bits != c.f1_bits)
      return Fail(Fault::kIdentity);
    if (depth_ == Depth) return Fail(Fault::kDepth);
    if (!Room()) return false;
    Frame f; f.entry = entry;
    if (depth_) f.parent = stack_[depth_-1].entry.invocation;
    stack_[depth_++] = f;
    if (!detail_epoch_) detail_epoch_ = p.invocation.thread_epoch;
    if (!router_epoch_) router_epoch_ = p.router.thread_epoch;
    last_serial_ = entry.invocation.serial;
    last_detail_serial_ = p.invocation.serial;
    last_router_serial_ = p.router.serial;
    Emit(EventKind::kEntry); return true;
  }
  bool CallPre(InvocationId id, uint8_t call, uint64_t caller_lr,
               uint64_t r3, uint64_t f1_bits, uint64_t indirect_ctr) noexcept {
    if (!Top(id)) return false;
    auto& f = stack_[depth_-1];
    if (call >= 3 || call != f.returned_calls || caller_lr != kCallReturnLr[call] ||
        (f.stage != Stage::kEntered && f.stage != Stage::kCallReturned))
      return Fail(Fault::kTransition);
    if (!Room()) return false;
    // Every call's F1/CTR is an opaque saved operand. In particular the middle
    // call need not have entry/first/last F1, and no virtual target is assumed.
    f.calls[call] = {caller_lr, r3, f1_bits, indirect_ctr,
                    static_cast<uint32_t>(indirect_ctr), CallState::kActive};
    f.stage = Stage::kCallActive; Emit(EventKind::kCallPre); return true;
  }
  bool CallReturn(InvocationId id, uint8_t call) noexcept { return CallEnd(id, call, true); }
  bool CallException(InvocationId id, uint8_t call) noexcept { return CallEnd(id, call, false); }
  bool Exit(InvocationId id, bool normal) noexcept {
    if (!Top(id)) return false;
    const auto& f = stack_[depth_-1];
    if (normal && (f.stage != Stage::kCallReturned || f.returned_calls != 3))
      return Fail(Fault::kTransition);
    if (!Room()) return false;
    // Optional earlier and later unobserved original calls can throw. An
    // exceptional exit at any valid stage never fabricates virtual outcomes.
    Emit(normal ? EventKind::kNormalExit : EventKind::kExceptionalExit);
    stack_[--depth_] = {}; return true;
  }
  void Freeze() noexcept { frozen_ = true; }
  bool frozen() const noexcept { return frozen_; }
  Fault fault() const noexcept { return fault_; }
  size_t size() const noexcept { return size_; }
  size_t pending_count() const noexcept { return depth_; }
  uint64_t parent_detail_thread_epoch() const noexcept { return detail_epoch_; }
  uint64_t parent_router_thread_epoch() const noexcept { return router_epoch_; }
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
  bool CallEnd(InvocationId id, uint8_t call, bool normal) noexcept {
    if (!Top(id)) return false;
    auto& f = stack_[depth_-1];
    if (call >= 3 || call != f.returned_calls || f.stage != Stage::kCallActive ||
        f.calls[call].state != CallState::kActive)
      return Fail(Fault::kTransition);
    if (!Room()) return false;
    f.calls[call].state = normal ? CallState::kReturned : CallState::kException;
    f.stage = normal ? Stage::kCallReturned : Stage::kCallException;
    if (normal) ++f.returned_calls;
    Emit(normal ? EventKind::kCallReturn : EventKind::kCallException); return true;
  }
  void Emit(EventKind kind) noexcept { events_[size_++] = {kind, stack_[depth_-1]}; }
  uint64_t epoch_ = 0, detail_epoch_ = 0, router_epoch_ = 0;
  uint64_t last_serial_ = 0, last_detail_serial_ = 0, last_router_serial_ = 0;
  size_t size_ = 0, depth_ = 0;
  Fault fault_ = Fault::kNone;
  bool frozen_ = false;
  std::array<Event, Capacity> events_{};
  std::array<Frame, Depth> stack_{};
};
}  // namespace mcla::virtual_detail
