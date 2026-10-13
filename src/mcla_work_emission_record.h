#pragma once
// Compact scalar bookkeeping for selected 82588490 work emissions. Caller
// supplies immutable witnessed association, already-loaded operands, lifetime
// identities and serialization. No guest reads, clocks, allocation, resolver,
// original-work callback or simulation proof. The 825753B0 prelude is unobserved:
// zero selected emissions NEVER means zero total work. Native tail-marker and
// context preservation are separate prerequisites, not proved by this core.
#include "mcla_virtual_detail_record.h"
#include <array>
#include <cstddef>
#include <cstdint>

namespace mcla::work_emission {
using InvocationId = virtual_detail::InvocationId;
constexpr uint32_t kBody = 0x82588490, kTail = 0x82741010;
constexpr uint64_t kCaller = 0x8258D1E8;
constexpr uint64_t kScalarLr = 0x825885AC, kCleanupLr = 0x825885CC,
    kJobLr = 0x8258866C, kWaitLr = 0x82588694, kVectorLr = 0x825886F4;
constexpr bool kNativeExecutionProven = false, kResolvedTargetsProven = false,
    kTaskCompletionProven = false, kSimulationProven = false;
struct Entry {
  InvocationId invocation;
  virtual_detail::Entry virtual_entry;
  virtual_detail::Call active_call0;
  uint32_t actual_target = 0;
  uint64_t caller_lr = 0, r3 = 0, f1_bits = 0;
  bool operator==(const Entry&) const = default;
};
// Unused fields are zero. Gates/counts/returns use fields[0]. Pre layouts:
// scalar: LR,index,r3,F1,F2,CTR; cleanup: LR,r3,CTR;
// job: LR,index,r3,r4,r5,r6,r7,r8,descriptor_f32_bits;
// wait: LR,r3,r4,r5; vector: LR,index,r3,F1,CTR plus four raw V1 lanes.
// Plain return/exception/exit events have no sampled operand payload.
// Native adapters must supply observed LR explicitly; default LR arguments
// are supplied callsite claims for synthetic callers, not execution proof.
struct Payload {
  std::array<uint64_t, 10> fields{};
  std::array<uint32_t, 4> vector{};
  bool operator==(const Payload&) const = default;
};
enum class EventKind : uint8_t {
  kEntry, kMainGate, kScalarCount, kScalarPre, kScalarReturn, kScalarException,
  kCleanupPre, kCleanupReturn, kCleanupException, kModeGate, kJobInitialBound,
  kJobPre, kJobReturn, kJobException, kJobReloadBound, kWaitPre, kWaitReturn,
  kWaitException, kVectorCount, kVectorPre, kVectorReturn, kVectorException,
  kNormalExit, kExceptionalExit
};
struct Event {
  InvocationId invocation;
  uint32_t entry_index = 0;
  EventKind kind = EventKind::kEntry;
  Payload payload;
  bool operator==(const Event&) const = default;
};
enum class Stage : uint8_t {
  kEntered, kScalarCount, kScalarReady, kScalarActive, kCleanupRequired,
  kCleanupActive, kJobInitialBound, kJobReady, kJobActive, kJobReloadBound,
  kWaitRequired, kWaitActive, kVectorCount, kVectorReady, kVectorActive,
  kComplete, kEmissionException
};
enum class Fault : uint8_t { kNone, kIdentity, kTransition, kEntryCapacity, kEventCapacity, kDepth };
struct Frame {
  uint32_t entry_index = 0;
  InvocationId parent;  // Nearest admitted model frame, not game-root ancestry.
  Stage stage = Stage::kEntered;
  bool main_observed = false, scalar_count_observed = false, mode_observed = false,
       job_bound_observed = false, vector_count_observed = false;
  uint64_t main_raw = 0, mode_raw = 0, scalar_count = 0, scalar_upper = 0,
           scalar_index = 0, scalar_return = 0, next_job = 0, job_bound = 0,
           job_return = 0, vector_count = 0, next_vector = 0;
  uint32_t scalar_emissions = 0, cleanup_emissions = 0, job_emissions = 0,
           wait_emissions = 0, vector_emissions = 0;
  bool operator==(const Frame&) const = default;
};

// Large instances need caller-owned heap storage. No lock or drain in this core.
template <size_t Entries = 1024, size_t Events = 8192, size_t Depth = 8>
class Model {
  static_assert(Entries > 0 && Entries <= 4096 && Events > 0 && Events <= 65536 && Depth > 0 && Depth <= 64);
 public:
  explicit Model(uint64_t own_epoch, uint64_t parent_virtual_epoch = 0) noexcept
      : epoch_(own_epoch), virtual_epoch_(parent_virtual_epoch) { if (!epoch_) fault_ = Fault::kIdentity; }
  Model(const Model&) = delete;
  Model& operator=(const Model&) = delete;
  Model(Model&&) = delete;
  Model& operator=(Model&&) = delete;
  bool Enter(const Entry& e) noexcept {
    if (!Writable()) return false;
    const auto& v = e.virtual_entry; const auto& p = v.parent_entry;
    const auto& c = v.parent_child; const auto& call = e.active_call0; const auto& b = p.update;
    const bool unbound = !b.phase_call_id && !b.thread_epoch && !b.native_thread_id;
    const bool bound = b.phase_call_id && b.thread_epoch && b.native_thread_id;
    if (e.invocation.thread_epoch != epoch_ || !e.invocation.serial || e.invocation.serial <= last_serial_ ||
        !v.invocation.thread_epoch || (virtual_epoch_ && v.invocation.thread_epoch != virtual_epoch_) ||
        !v.invocation.serial || v.invocation.serial <= last_virtual_serial_ ||
        !p.invocation.thread_epoch || (detail_epoch_ && p.invocation.thread_epoch != detail_epoch_) ||
        !p.invocation.serial || p.invocation.serial <= last_detail_serial_ ||
        !p.router.thread_epoch || (router_epoch_ && p.router.thread_epoch != router_epoch_) ||
        !p.router.serial || p.router.serial <= last_router_serial_ || (!unbound && !bound) ||
        p.actual_target != consumer_detail::kTarget || p.caller_lr != consumer_detail::kCaller ||
        c.state != consumer_detail::ChildState::kActive || !c.indirect ||
        c.caller_lr != virtual_detail::kCaller || c.requested_target != virtual_detail::kTarget ||
        static_cast<uint32_t>(c.indirect_ctr) != virtual_detail::kTarget ||
        v.actual_target != virtual_detail::kTarget || v.caller_lr != virtual_detail::kCaller ||
        v.r3 != c.r3 || v.f1_bits != c.f1_bits || call.state != virtual_detail::CallState::kActive ||
        call.caller_lr != kCaller || call.requested_target != kTail || static_cast<uint32_t>(call.indirect_ctr) != kTail ||
        e.actual_target != kBody || e.caller_lr != kCaller || e.r3 != call.r3 || e.f1_bits != call.f1_bits)
      return Fail(Fault::kIdentity);
    if (depth_ == Depth) return Fail(Fault::kDepth);
    if (entry_size_ == Entries) return Fail(Fault::kEntryCapacity);
    if (!Room()) return false;
    Frame f; f.entry_index = static_cast<uint32_t>(entry_size_);
    if (depth_) f.parent = entries_[stack_[depth_-1].entry_index].invocation;
    entries_[entry_size_++] = e; stack_[depth_++] = f;
    if (!virtual_epoch_) virtual_epoch_ = v.invocation.thread_epoch;
    if (!detail_epoch_) detail_epoch_ = p.invocation.thread_epoch;
    if (!router_epoch_) router_epoch_ = p.router.thread_epoch;
    last_serial_ = e.invocation.serial; last_virtual_serial_ = v.invocation.serial;
    last_detail_serial_ = p.invocation.serial; last_router_serial_ = p.router.serial;
    Emit(EventKind::kEntry); return true;
  }
  bool MainGate(InvocationId id, uint64_t r7) noexcept {
    if (!At(id, Stage::kEntered)) return false;
    if (r7 > UINT8_MAX) return Fail(Fault::kTransition);
    if (!Room()) return false;
    auto& f = TopFrame(); f.main_observed = true; f.main_raw = r7;
    f.stage = r7 ? Stage::kScalarCount : Stage::kComplete;
    Emit(EventKind::kMainGate, One(r7)); return true;
  }
  bool ScalarCount(InvocationId id, uint64_t r9) noexcept {
    if (!At(id, Stage::kScalarCount)) return false;
    if (r9 > UINT16_MAX) return Fail(Fault::kTransition);
    if (!Room()) return false;
    auto& f = TopFrame(); f.scalar_count_observed = true; f.scalar_count = f.scalar_upper = r9;
    f.stage = Stage::kScalarReady; Emit(EventKind::kScalarCount, One(r9)); return true;
  }
  bool ScalarPre(InvocationId id, uint64_t index, uint64_t r3, uint64_t f1, uint64_t f2,
                 uint64_t ctr, uint64_t lr = kScalarLr) noexcept {
    if (!At(id, Stage::kScalarReady)) return false;
    auto& f = TopFrame();
    if (index >= f.scalar_upper || lr != kScalarLr) return Fail(Fault::kTransition);
    if (!Room()) return false;
    f.scalar_index = index; f.scalar_upper = index; ++f.scalar_emissions; f.stage = Stage::kScalarActive;
    Payload p; p.fields = {lr,index,r3,f1,f2,ctr}; Emit(EventKind::kScalarPre,p); return true;
  }
  bool ScalarReturn(InvocationId id, uint64_t raw_r3) noexcept {
    if (!At(id, Stage::kScalarActive) || !Room()) return false;
    auto& f = TopFrame(); f.scalar_return = raw_r3;
    f.stage = static_cast<uint8_t>(raw_r3) == 0 ? Stage::kCleanupRequired : Stage::kScalarReady;
    Emit(EventKind::kScalarReturn, One(raw_r3)); return true;
  }
  bool ScalarException(InvocationId id) noexcept { return Exception(id, Stage::kScalarActive, EventKind::kScalarException); }
  bool CleanupPre(InvocationId id, uint64_t r3, uint64_t ctr, uint64_t lr = kCleanupLr) noexcept {
    if (!At(id, Stage::kCleanupRequired)) return false;
    if (lr != kCleanupLr) return Fail(Fault::kTransition);
    if (!Room()) return false;
    auto& f = TopFrame(); ++f.cleanup_emissions; f.stage = Stage::kCleanupActive;
    Payload p; p.fields = {lr,r3,ctr}; Emit(EventKind::kCleanupPre,p); return true;
  }
  bool CleanupReturn(InvocationId id) noexcept { return End(id, Stage::kCleanupActive, Stage::kScalarReady, EventKind::kCleanupReturn); }
  bool CleanupException(InvocationId id) noexcept { return Exception(id, Stage::kCleanupActive, EventKind::kCleanupException); }
  bool ModeGate(InvocationId id, uint64_t r10) noexcept {
    if (!At(id, Stage::kScalarReady)) return false;
    if (r10 > UINT8_MAX) return Fail(Fault::kTransition);
    if (!Room()) return false;
    auto& f = TopFrame(); f.mode_observed = true; f.mode_raw = r10;
    f.stage = r10 ? Stage::kVectorCount : Stage::kJobInitialBound;
    Emit(EventKind::kModeGate, One(r10)); return true;
  }
  bool JobBound(InvocationId id, uint64_t raw_r11) noexcept {
    if (!Top(id)) return false;
    auto& f = TopFrame(); const bool initial = f.stage == Stage::kJobInitialBound;
    if ((!initial && f.stage != Stage::kJobReloadBound) || raw_r11 > UINT32_MAX) return Fail(Fault::kTransition);
    if (!Room()) return false;
    f.job_bound_observed = true; f.job_bound = raw_r11;
    const auto signed_bound = raw_r11 & 0x80000000ull ? static_cast<int64_t>(raw_r11)-0x100000000ll : static_cast<int64_t>(raw_r11);
    f.stage = signed_bound > 0 && f.next_job < static_cast<uint64_t>(signed_bound) ? Stage::kJobReady : Stage::kWaitRequired;
    Emit(initial ? EventKind::kJobInitialBound : EventKind::kJobReloadBound, One(raw_r11)); return true;
  }
  bool JobPre(InvocationId id, uint64_t index, const std::array<uint64_t,6>& r3_to_r8,
              uint32_t descriptor_f32_bits, uint64_t lr = kJobLr) noexcept {
    if (!At(id, Stage::kJobReady)) return false;
    auto& f = TopFrame(); if (index != f.next_job || lr != kJobLr) return Fail(Fault::kTransition);
    if (!Room()) return false;
    ++f.job_emissions; f.stage = Stage::kJobActive;
    Payload p; p.fields[0] = lr; p.fields[1] = index;
    for (size_t i=0;i<6;++i) p.fields[i+2] = r3_to_r8[i];
    p.fields[8] = descriptor_f32_bits; Emit(EventKind::kJobPre,p); return true;
  }
  bool JobReturn(InvocationId id, uint64_t raw_r3) noexcept {
    if (!At(id, Stage::kJobActive) || !Room()) return false;
    auto& f = TopFrame(); f.job_return = raw_r3; ++f.next_job; f.stage = Stage::kJobReloadBound;
    Emit(EventKind::kJobReturn, One(raw_r3)); return true;
  }
  bool JobException(InvocationId id) noexcept { return Exception(id, Stage::kJobActive, EventKind::kJobException); }
  bool WaitPre(InvocationId id, uint64_t r3, uint64_t r4, uint64_t r5, uint64_t lr = kWaitLr) noexcept {
    if (!At(id, Stage::kWaitRequired)) return false;
    auto& f = TopFrame(); if (r3 != f.job_bound || r5 != 1 || lr != kWaitLr) return Fail(Fault::kTransition);
    if (!Room()) return false;
    ++f.wait_emissions; f.stage = Stage::kWaitActive;
    Payload p; p.fields = {lr,r3,r4,r5}; Emit(EventKind::kWaitPre,p); return true;
  }
  bool WaitReturn(InvocationId id) noexcept { return End(id, Stage::kWaitActive, Stage::kComplete, EventKind::kWaitReturn); }
  bool WaitException(InvocationId id) noexcept { return Exception(id, Stage::kWaitActive, EventKind::kWaitException); }
  bool VectorCount(InvocationId id, uint64_t r10) noexcept {
    if (!At(id, Stage::kVectorCount)) return false;
    if (r10 > UINT16_MAX) return Fail(Fault::kTransition);
    if (!Room()) return false;
    auto& f = TopFrame(); f.vector_count_observed = true; f.vector_count = r10;
    f.next_vector = r10 ? r10-1 : 0; f.stage = r10 ? Stage::kVectorReady : Stage::kComplete;
    Emit(EventKind::kVectorCount, One(r10)); return true;
  }
  bool VectorPre(InvocationId id, uint64_t index, uint64_t r3, uint64_t f1, uint64_t ctr,
                 const std::array<uint32_t,4>& v1, uint64_t lr = kVectorLr) noexcept {
    if (!At(id, Stage::kVectorReady)) return false;
    auto& f = TopFrame(); if (index != f.next_vector || lr != kVectorLr) return Fail(Fault::kTransition);
    if (!Room()) return false;
    ++f.vector_emissions; f.stage = Stage::kVectorActive;
    Payload p; p.fields = {lr,index,r3,f1,ctr}; p.vector = v1; Emit(EventKind::kVectorPre,p); return true;
  }
  bool VectorReturn(InvocationId id) noexcept {
    if (!At(id, Stage::kVectorActive) || !Room()) return false;
    auto& f = TopFrame(); if (f.next_vector) { --f.next_vector; f.stage = Stage::kVectorReady; } else f.stage = Stage::kComplete;
    Emit(EventKind::kVectorReturn); return true;
  }
  bool VectorException(InvocationId id) noexcept { return Exception(id, Stage::kVectorActive, EventKind::kVectorException); }
  bool Exit(InvocationId id, bool normal) noexcept {
    if (!Top(id)) return false;
    if (normal && TopFrame().stage != Stage::kComplete) return Fail(Fault::kTransition);
    if (!Room()) return false;
    Emit(normal ? EventKind::kNormalExit : EventKind::kExceptionalExit);
    stack_[--depth_] = {}; return true;
  }
  void Freeze() noexcept { frozen_ = true; }
  bool frozen() const noexcept { return frozen_; }
  Fault fault() const noexcept { return fault_; }
  size_t entry_count() const noexcept { return entry_size_; }
  size_t size() const noexcept { return event_size_; }
  size_t pending_count() const noexcept { return depth_; }
  uint64_t parent_virtual_epoch() const noexcept { return virtual_epoch_; }
  uint64_t parent_detail_epoch() const noexcept { return detail_epoch_; }
  uint64_t parent_router_epoch() const noexcept { return router_epoch_; }
  const std::array<Entry,Entries>& entries() const noexcept { return entries_; }
  const std::array<Event,Events>& events() const noexcept { return events_; }
  const std::array<Frame,Depth>& pending() const noexcept { return stack_; }

 private:
  bool Writable() const noexcept { return !frozen_ && fault_ == Fault::kNone; }
  bool Fail(Fault f) noexcept { if (Writable()) fault_ = f; return false; }
  bool Room() noexcept { return event_size_ < Events || Fail(Fault::kEventCapacity); }
  Frame& TopFrame() noexcept { return stack_[depth_-1]; }
  bool Top(InvocationId id) noexcept {
    if (!Writable()) return false;
    if (!depth_ || entries_[TopFrame().entry_index].invocation != id) return Fail(Fault::kIdentity);
    return true;
  }
  bool At(InvocationId id, Stage s) noexcept {
    if (!Top(id)) return false;
    return TopFrame().stage == s || Fail(Fault::kTransition);
  }
  static Payload One(uint64_t v) noexcept { Payload p; p.fields[0] = v; return p; }
  bool End(InvocationId id, Stage active, Stage next, EventKind kind) noexcept {
    if (!At(id,active) || !Room()) return false;
    TopFrame().stage = next; Emit(kind); return true;
  }
  bool Exception(InvocationId id, Stage active, EventKind kind) noexcept { return End(id,active,Stage::kEmissionException,kind); }
  void Emit(EventKind kind, const Payload& p = {}) noexcept {
    const auto index = TopFrame().entry_index;
    events_[event_size_++] = {entries_[index].invocation,index,kind,p};
  }
  uint64_t epoch_ = 0, virtual_epoch_ = 0, detail_epoch_ = 0, router_epoch_ = 0;
  uint64_t last_serial_ = 0, last_virtual_serial_ = 0, last_detail_serial_ = 0, last_router_serial_ = 0;
  size_t entry_size_ = 0, event_size_ = 0, depth_ = 0;
  Fault fault_ = Fault::kNone;
  bool frozen_ = false;
  std::array<Entry,Entries> entries_{};
  std::array<Event,Events> events_{};
  std::array<Frame,Depth> stack_{};
};
} // namespace mcla::work_emission
