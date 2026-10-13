#pragma once
// Scalar bookkeeping only. Caller owns actual dispatch observation, exact
// Update binding, TLS lifetime identity and serialization. No memory reads,
// clocks, allocation, target resolution or consumer/simulation proof.
// One Model per independently identified native thread lifetime. The detail
// and router epoch namespaces are separate. Native integration must maintain
// their stable pairing. Router epoch0 at construction means not bound yet;
// the first successful positive-ID Enter fixes it permanently, never from TID.
// New router serials increase strictly even after exit;
// returns from nested detail invocations are not new entries.
#include "mcla_router_dispatch_record.h"
#include <array>
#include <cstddef>
#include <cstdint>

namespace mcla::consumer_detail {
using InvocationId = router_dispatch::InvocationId;
using UpdateBinding = router_dispatch::UpdateBinding;
constexpr uint32_t kTarget = 0x822C7870;
constexpr uint64_t kCaller = 0x822C5328;
constexpr std::array<uint64_t, 3> kChildReturnLr{0x822C78C0, 0x822C78CC, 0x822C78D8};
constexpr std::array<uint32_t, 3> kDirectTarget{0, 0x82588428, 0x82476238};

struct Entry {
  InvocationId invocation;
  InvocationId router;
  UpdateBinding update;  // Whole zero is explicitly unbound, never inherited.
  uint32_t actual_target = 0;
  uint64_t caller_lr = 0, r3 = 0, f1_bits = 0;
  bool operator==(const Entry&) const = default;
};
enum class Stage : uint8_t { kEntered, kBranched, kChildActive, kChildReturned, kChildException };
enum class ChildState : uint8_t { kAbsent, kActive, kReturned, kException };
enum class EventKind : uint8_t { kEntry, kBranch, kChildPre, kChildReturn, kChildException, kNormalExit, kExceptionalExit };
enum class Fault : uint8_t { kNone, kIdentity, kTransition, kCapacity, kDepth };
struct Child {
  uint64_t caller_lr = 0, r3 = 0, f1_bits = 0, indirect_ctr = 0;
  uint32_t requested_target = 0;
  ChildState state = ChildState::kAbsent;
  bool indirect = false;
  // Direct targets are fixed original instruction operands, never sampled CTR.
  // Indirect requested_target is low32 CTR, not a resolved host function.
  bool operator==(const Child&) const = default;
};
struct Frame {
  Entry entry;
  InvocationId parent;
  Stage stage = Stage::kEntered;
  uint64_t branch_r11 = 0;
  bool branch_observed = false, branch_nonzero = false;
  uint8_t returned_children = 0;
  std::array<Child, 3> children{};
  bool operator==(const Frame&) const = default;
};
struct Event {
  EventKind kind = EventKind::kEntry;
  Frame frame;  // Detached complete scalar snapshot.
  bool operator==(const Event&) const = default;
};

template <size_t Capacity = 4096, size_t Depth = 8>
class Model {
  static_assert(Capacity > 0 && Capacity <= 65536 && Depth > 0 && Depth <= 64);
 public:
  Model(uint64_t detail_thread_epoch, uint64_t router_thread_epoch = 0) noexcept
      : epoch_(detail_thread_epoch), router_epoch_(router_thread_epoch) {
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
    const bool bound = b.phase_call_id && b.thread_epoch && b.native_thread_id;
    if (entry.invocation.thread_epoch != epoch_ || !entry.invocation.serial ||
        entry.invocation.serial <= last_serial_ || !entry.router.thread_epoch ||
        (router_epoch_ && entry.router.thread_epoch != router_epoch_) ||
        !entry.router.serial || entry.router.serial <= last_router_serial_ ||
        entry.actual_target != kTarget || entry.caller_lr != kCaller || (!bound && !unbound))
      return Fail(Fault::kIdentity);
    if (depth_ == Depth) return Fail(Fault::kDepth);
    if (!Room()) return false;
    Frame f;
    f.entry = entry;
    if (depth_) f.parent = stack_[depth_-1].entry.invocation;
    stack_[depth_++] = f;
    if (!router_epoch_) router_epoch_ = entry.router.thread_epoch;
    last_serial_ = entry.invocation.serial;
    last_router_serial_ = entry.router.serial;
    Emit(EventKind::kEntry);
    return true;
  }
  bool Branch(InvocationId id, uint64_t raw_r11) noexcept {
    if (!At(id, Stage::kEntered)) return false;
    auto& f = stack_[depth_-1];
    f.branch_r11 = raw_r11;
    f.branch_observed = true;
    f.branch_nonzero = static_cast<uint32_t>(raw_r11) != 0;
    f.stage = Stage::kBranched;
    Emit(EventKind::kBranch); return true;
  }
  bool ChildPre(InvocationId id, uint8_t child, uint64_t caller_lr,
                uint64_t r3, uint64_t f1_bits, uint64_t indirect_ctr = 0) noexcept {
    if (!Top(id)) return false;
    auto& f = stack_[depth_-1];
    if (!f.branch_observed || !f.branch_nonzero || child >= 3 || child != f.returned_children ||
        (f.stage != Stage::kBranched && f.stage != Stage::kChildReturned) ||
        caller_lr != kChildReturnLr[child] || (child != 0 && indirect_ctr != 0))
      return Fail(Fault::kTransition);
    if (!Room()) return false;
    f.children[child] = {caller_lr, r3, f1_bits, indirect_ctr,
        child == 0 ? static_cast<uint32_t>(indirect_ctr) : kDirectTarget[child],
        ChildState::kActive, child == 0};
    f.stage = Stage::kChildActive;
    Emit(EventKind::kChildPre); return true;
  }
  bool ChildReturn(InvocationId id, uint8_t child) noexcept {
    return ChildEnd(id, child, true);
  }
  bool ChildException(InvocationId id, uint8_t child) noexcept {
    return ChildEnd(id, child, false);
  }
  bool Exit(InvocationId id, bool normal) noexcept {
    if (!Top(id)) return false;
    const auto& f = stack_[depth_-1];
    if (normal && (!f.branch_observed ||
        (f.branch_nonzero ? f.stage != Stage::kChildReturned || f.returned_children != 3
                         : f.stage != Stage::kBranched)))
      return Fail(Fault::kTransition);
    if (!Room()) return false;
    // Exceptional exit is valid before a selected child, including an exception
    // in the earlier unobserved call 825883C0. It never invents child outcomes.
    Emit(normal ? EventKind::kNormalExit : EventKind::kExceptionalExit);
    stack_[--depth_] = {};
    return true;
  }
  void Freeze() noexcept { frozen_ = true; }
  bool frozen() const noexcept { return frozen_; }
  Fault fault() const noexcept { return fault_; }
  size_t size() const noexcept { return size_; }
  size_t pending_count() const noexcept { return depth_; }
  uint64_t router_thread_epoch() const noexcept { return router_epoch_; }
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
  bool ChildEnd(InvocationId id, uint8_t child, bool normal) noexcept {
    if (!Top(id)) return false;
    auto& f = stack_[depth_-1];
    if (f.stage != Stage::kChildActive || child >= 3 || child != f.returned_children ||
        f.children[child].state != ChildState::kActive)
      return Fail(Fault::kTransition);
    if (!Room()) return false;
    f.children[child].state = normal ? ChildState::kReturned : ChildState::kException;
    f.stage = normal ? Stage::kChildReturned : Stage::kChildException;
    if (normal) ++f.returned_children;
    Emit(normal ? EventKind::kChildReturn : EventKind::kChildException); return true;
  }
  void Emit(EventKind kind) noexcept { events_[size_++] = {kind, stack_[depth_-1]}; }
  uint64_t epoch_ = 0, router_epoch_ = 0, last_serial_ = 0, last_router_serial_ = 0;
  size_t size_ = 0, depth_ = 0;
  Fault fault_ = Fault::kNone;
  bool frozen_ = false;
  std::array<Event, Capacity> events_{};
  std::array<Frame, Depth> stack_{};
};
}  // namespace mcla::consumer_detail
