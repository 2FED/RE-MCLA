#pragma once
// Private collector foundation, not a PPC/SDK hook. Keep Collector, Registry,
// clock state and callback code alive for the process lifetime (also on error).
// ONE actual SDK-owned Registry must be passed explicitly; no SDK helper include
// or cross-module singleton is declared here. Configure before guest execution.
// Clock providers must be bounded, non-reentrant and never call SDK/Collector.
// No observer mutex spans original work; caller owns original/context/LastError
// preservation. Coverage attestations are external claims, not native proof.
#include "mcla_semaphore_witness_api_win.h"
#include "mcla_semaphore_prefix_snapshot.h"
#include <mutex>

namespace mcla::semaphore_collector {
namespace api = rex::diagnostics::semaphore_witness;
namespace core = semaphore_observer;
namespace bridge = semaphore_prefix_snapshot;
inline constexpr uint32_t kCreationLR = 0x823062D8, kReleaseLR = 0x82305DDC;
inline constexpr bool kNativeAdmitted = false, kSimulationProven = false, kFrameToStepMappingProven = false;
struct ClockProvider {
  void* context = nullptr;
  uint64_t (*read)(void*) = Real;
  uint64_t (*frequency)(void*) = Frequency;
  static uint64_t Real(void*) noexcept { LARGE_INTEGER value{}; return QueryPerformanceCounter(&value) && value.QuadPart > 0 ? uint64_t(value.QuadPart) : 0; }
  static uint64_t Frequency(void*) noexcept { LARGE_INTEGER value{}; return QueryPerformanceFrequency(&value) && value.QuadPart > 0 ? uint64_t(value.QuadPart) : 0; }
};
struct CreateIntent { uint32_t outer_lr{}, initial_count{}, owner_guest{}; frame_context::Lane lane{}; };
struct OperationIntent {
  uint64_t cookie{}, phase_call_id{};
  std::optional<uint32_t> handle;
  frame_context::Lane lane{};
  uint32_t outer_lr{};
  api::Kind expected_kind = api::Kind::kWait;
  core::ReleaseContext release_context{};
};
struct WaitObservation { uint64_t op_id{}, phase_call_id{}; core::WaitResult outcome = core::WaitResult::kUnknown; bool annotated = false; };
struct AnnotationRecord {
  uint64_t op_id{}, phase_call_id{};
  uint32_t outer_lr{}, native_thread_id{};
  bool annotated = false;
};
struct CoverageAttestation {
  // Empty/default means UNKNOWN. A complete claim requires all exact bindings.
  std::string_view token;
  uint64_t observer_begin_qpc{}, qpc_frequency{}, selected_epoch{};
  bridge::Coverage inventory = bridge::Coverage::kUnknown;
  bridge::Coverage creation = bridge::Coverage::kUnknown, operations = bridge::Coverage::kUnknown;
  bool no_unknown_gap = false;
};
struct Counters { uint64_t failures{}, ignored_creations{}, ignored_operations{}, annotation_mismatches{}, late_callbacks{}; };
struct RawSummary { core::Counts counts{}; uint32_t issues{}, pending{}; bool post_stop_mutated{}, frozen{}; };
enum class Status { kOk, kDisabled, kAlreadyConfigured, kInvalidConfiguration, kRegistrationFailed,
                    kAllocation, kNotConfigured, kAlreadyFrozen, kInvalidClock, kNoSelection, kCaptureFailed };
class Collector;
class Frozen {
 public:
  ~Frozen() = default;
  const bridge::Snapshot& journal() const noexcept { return *snapshot_; }
  std::span<const AnnotationRecord> annotations() const noexcept { return {annotations_.data(), used_}; }
  uint64_t cookie() const noexcept { return cookie_; }
  uint64_t epoch() const noexcept { return epoch_; }
  uint64_t freeze_qpc() const noexcept { return freeze_; }
  uint32_t returned_handle() const noexcept { return handle_; }
  uint32_t owner_guest() const noexcept { return owner_; }
  Counters counters() const noexcept { return counters_; }
  Frozen(const Frozen&) = delete; Frozen& operator=(const Frozen&) = delete;
 private:
  friend class Collector;
  Frozen() = default;
  std::unique_ptr<const bridge::Snapshot> snapshot_;
  std::array<AnnotationRecord, core::kOperationCapacity> annotations_{};
  size_t used_ = 0;
  uint64_t cookie_ = 0, epoch_ = 0, freeze_ = 0;
  uint32_t handle_ = 0, owner_ = 0;
  Counters counters_{};
};
struct FreezeResult {
  Status status;
  bridge::Error snapshot_error;
  std::unique_ptr<const Frozen> frozen;
  FreezeResult(Status value, bridge::Error error = bridge::Error::kNone, std::unique_ptr<const Frozen> snapshot = {}) noexcept
      : status(value), snapshot_error(error), frozen(std::move(snapshot)) {}
};

class Collector {
 public:
  class ScopedCreate {
   public:
    ScopedCreate(Collector& owner, CreateIntent intent) noexcept : owner_(&owner), intent_(intent), previous_(create_scope_) { create_scope_ = this; }
    ~ScopedCreate() noexcept { create_scope_ = previous_; }
    ScopedCreate(const ScopedCreate&) = delete; ScopedCreate& operator=(const ScopedCreate&) = delete;
    ScopedCreate(ScopedCreate&&) = delete; ScopedCreate& operator=(ScopedCreate&&) = delete;
    uint64_t selected_cookie() const noexcept { return cookie_; }
    uint64_t selected_epoch() const noexcept { return epoch_; }
   private:
    friend class Collector;
    Collector* owner_; CreateIntent intent_; ScopedCreate* previous_;
    uint64_t cookie_ = 0, epoch_ = 0;
  };
  class ScopedOperation {
   public:
    ScopedOperation(Collector& owner, OperationIntent intent) noexcept : owner_(&owner), intent_(intent), previous_(operation_scope_) { operation_scope_ = this; }
    ~ScopedOperation() noexcept { operation_scope_ = previous_; }
    ScopedOperation(const ScopedOperation&) = delete; ScopedOperation& operator=(const ScopedOperation&) = delete;
    ScopedOperation(ScopedOperation&&) = delete; ScopedOperation& operator=(ScopedOperation&&) = delete;
    // Observation only. A later wait can occur without any render. Caller must
    // explicitly discard that omission; no Tracker/FIFO/latest state here.
    std::optional<WaitObservation> wait_observation() const noexcept { return ambiguous_ ? std::nullopt : wait_; }
    uint64_t sdk_admission_count() const noexcept { return begin_attempts_; }
   private:
    friend class Collector;
    Collector* owner_; OperationIntent intent_; ScopedOperation* previous_;
    uint64_t ticket_ = 0, begin_attempts_ = 0; bool ambiguous_ = false;
    std::optional<WaitObservation> wait_;
  };
  explicit Collector(api::Registry& registry, ClockProvider clock = {}) noexcept : registry_(registry), clock_(clock) {}
  Collector(const Collector&) = delete; Collector& operator=(const Collector&) = delete;
  Collector(Collector&&) = delete; Collector& operator=(Collector&&) = delete;
  Status Configure(core::Window window, std::string_view token, bool enabled = false) noexcept {
    api::detail::LastErrorGuard restore;
    if (!enabled || !api::kCompiledEnabled) return Status::kDisabled; // No allocation/registration.
    try {
      std::lock_guard lock(mutex_);
      if (configured_) return Status::kAlreadyConfigured;
      configured_ = true;
      if (!bridge::detail::Token(token) || !window.begin_qpc || window.begin_qpc >= window.end_qpc_exclusive ||
          window.end_qpc_exclusive > core::kMaxQpc || !window.qpc_frequency || window.qpc_frequency > core::kMaxQpc ||
          !clock_.read || !clock_.frequency || clock_.frequency(clock_.context) != window.qpc_frequency) {
        Increment(failures_); return Status::kInvalidConfiguration;
      }
      journal_ = std::make_unique<core::Journal>(window, true); window_ = window;
      std::copy(token.begin(), token.end(), token_.begin());
      if (!registry_.Register({this, CreateCallback, DestroyCallback, BeginCallback, EndCallback})) {
        FailLocked(); return Status::kRegistrationFailed;
      }
      return Status::kOk;
    } catch (const std::bad_alloc&) { Increment(failures_); return Status::kAllocation; }
      catch (...) { Increment(failures_); return Status::kInvalidConfiguration; }
  }
  // Called AFTER helper return, with cookie independently resolved from its
  // returned handle. +20 has not necessarily been stored by the guest yet.
  bool BindReturnedHandle(const ScopedCreate& scope, uint64_t resolved_cookie, uint32_t handle) noexcept {
    api::detail::LastErrorGuard restore;
    try {
      std::lock_guard lock(mutex_);
      if (!journal_) return false; // Default-off helper path remains inert.
      if (frozen_) { Increment(late_); return false; }
      if (!scope.cookie_ || scope.owner_ != this || create_scope_ != &scope ||
          scope.cookie_ != cookie_ || scope.epoch_ != epoch_ || resolved_cookie != cookie_ || !handle || bound_) {
        FailLocked(); return false;
      }
      handle_ = handle; bound_ = true; return true;
    } catch (...) { Increment(failures_); return false; }
  }
  // Own mutex is the host freeze lock. No phase Finish/Drain or SDK reentry
  // under it. Snapshot precedes drain; callbacks queued here recheck frozen.
  FreezeResult Freeze(const CoverageAttestation& attestation = {}) noexcept {
    api::detail::LastErrorGuard restore;
    try {
      std::lock_guard lock(mutex_);
      if (!journal_) return {Status::kNotConfigured};
      if (frozen_) return {Status::kAlreadyFrozen};
      if (registry_.failed() || !registry_.admitting()) FailLocked();
      const auto cut = ReadClockLocked(); // actual cut before Disable, under lock.
      frozen_ = true; journal_->stop_admissions(cut); registry_.Disable();
      if (!cut || cut <= window_.begin_qpc || cut >= window_.end_qpc_exclusive) return {Status::kInvalidClock};
      if (!cookie_ || journal_->epochs().empty()) return {Status::kNoSelection};
      std::array<bridge::EpochCoverage, core::kEpochCapacity> epochs{};
      const bool attested = attestation.token == Token() && attestation.observer_begin_qpc == window_.begin_qpc &&
          attestation.qpc_frequency == window_.qpc_frequency && attestation.selected_epoch == epoch_ &&
          bound_ && !failures_.load(std::memory_order_relaxed) && !registry_.failed();
      const auto rows = journal_->epochs();
      for (size_t i = 0; i < rows.size(); ++i) {
        epochs[i] = {rows[i].epoch_id, rows[i].object_cookie, rows[i].creation_qpc,
                     attested ? attestation.creation : bridge::Coverage::kUnknown,
                     attested ? attestation.operations : bridge::Coverage::kUnknown};
      }
      bridge::AdmissionCoverage coverage{Token(), window_.begin_qpc, window_.qpc_frequency, cut,
          attested ? attestation.inventory : bridge::Coverage::kUnknown,
          !attested || !attestation.no_unknown_gap, failures_.load(std::memory_order_relaxed) != 0 || registry_.failed(),
          {epochs.data(), rows.size()}};
      auto captured = bridge::Snapshot::Capture(*journal_, Token(), coverage);
      if (!captured.snapshot) return {Status::kCaptureFailed, captured.error};
      std::unique_ptr<Frozen> result(new Frozen);
      result->snapshot_ = std::move(captured.snapshot); result->used_ = size_t(journal_->counts().stored_operations);
      std::copy_n(annotations_.begin(), result->used_, result->annotations_.begin());
      result->cookie_ = cookie_; result->epoch_ = epoch_; result->handle_ = handle_; result->owner_ = owner_guest_;
      result->freeze_ = cut;
      result->counters_ = ReadCounters();
      return {Status::kOk, bridge::Error::kNone, std::move(result)};
    } catch (const std::bad_alloc&) { Increment(failures_); return {Status::kAllocation}; }
      catch (...) { Increment(failures_); return {Status::kCaptureFailed}; }
  }
  Counters counters() const noexcept { return ReadCounters(); }
  RawSummary raw_summary() const noexcept {
    api::detail::LastErrorGuard restore;
    try { std::lock_guard lock(mutex_); if (journal_) return {journal_->counts(), journal_->issues(), journal_->pending_operations(), journal_->post_stop_mutated(), frozen_}; }
    catch (...) {}
    return {};
  }

 private:
  static void Increment(std::atomic<uint64_t>& counter) noexcept {
    auto value = counter.load(std::memory_order_relaxed);
    while (value != UINT64_MAX && !counter.compare_exchange_strong(value, value + 1, std::memory_order_relaxed)) {}
  }
  Counters ReadCounters() const noexcept { return {failures_.load(), ignored_creations_.load(), ignored_operations_.load(), mismatches_.load(), late_.load()}; }
  std::string_view Token() const noexcept { return {token_.data(), token_.size()}; }
  void FailLocked() noexcept { Increment(failures_); if (journal_ && !frozen_) journal_->fail(); }
  uint64_t ReadClockLocked() noexcept {
    try { const auto value = clock_.read(clock_.context); if (value && value <= core::kMaxQpc) return value; }
    catch (...) {}
    FailLocked(); return 0;
  }
  static core::Origin Origin(api::Origin value) noexcept {
    switch (value) { case api::Origin::kFresh: return core::Origin::kFresh; case api::Origin::kNative: return core::Origin::kNative;
      case api::Origin::kRestored: return core::Origin::kRestored; case api::Origin::kUnknown: return core::Origin::kUnknown; }
    return core::Origin::kUnknown;
  }
  static core::Kind Kind(api::Kind value) noexcept {
    switch (value) { case api::Kind::kRelease: return core::Kind::kRelease; case api::Kind::kWait: return core::Kind::kWait;
      case api::Kind::kSave: return core::Kind::kSave; case api::Kind::kRestore: return core::Kind::kRestore;
      case api::Kind::kSignalAndWait: return core::Kind::kSignalAndWait; case api::Kind::kWaitMultiple: return core::Kind::kWaitMultiple; }
    return core::Kind::kUnknown;
  }
  static core::WaitResult Outcome(api::Result value) noexcept {
    switch (value) { case api::Result::kSuccess: return core::WaitResult::kSuccess; case api::Result::kTimeout: return core::WaitResult::kTimeout;
      case api::Result::kApc: return core::WaitResult::kApc; case api::Result::kAlerted: return core::WaitResult::kAlerted;
      case api::Result::kAbandoned: return core::WaitResult::kAbandoned; case api::Result::kFailed: return core::WaitResult::kFailed;
      case api::Result::kUnknown: case api::Result::kUnsupported: return core::WaitResult::kUnknown; }
    return core::WaitResult::kUnknown;
  }
  static void CreateCallback(void* owner, const api::Creation& value) { static_cast<Collector*>(owner)->OnCreate(value); }
  static void DestroyCallback(void* owner, uint64_t cookie) { static_cast<Collector*>(owner)->OnDestroy(cookie); }
  static uint64_t BeginCallback(void* owner, const api::Admission& value) { return static_cast<Collector*>(owner)->OnBegin(value); }
  static void EndCallback(void* owner, const api::Completion& value) { static_cast<Collector*>(owner)->OnEnd(value); }
  void OnCreate(const api::Creation& value) {
    std::lock_guard lock(mutex_);
    if (frozen_) { Increment(late_); return; }
    auto* scope = create_scope_;
    const bool selected_intent = scope && scope->owner_ == this && scope->intent_.outer_lr == kCreationLR;
    if (cookie_ && (value.cookie == cookie_ || selected_intent)) {
      FailLocked(); bound_ = false;
      if (value.cookie == cookie_) journal_->create(value.cookie, ReadClockLocked(), value.initial_count, value.maximum_count, Origin(value.origin));
      return;
    }
    if (!selected_intent) { Increment(ignored_creations_); return; }
    if (scope->intent_.initial_count || value.initial_count || value.maximum_count != 32767 ||
        !scope->intent_.owner_guest || !scope->intent_.lane.context || !scope->intent_.lane.base) { FailLocked(); return; }
    const auto created = journal_->create(value.cookie, ReadClockLocked(), value.initial_count, value.maximum_count, Origin(value.origin));
    if (!created.epoch_id) { FailLocked(); return; }
    cookie_ = scope->cookie_ = value.cookie; epoch_ = scope->epoch_ = created.epoch_id; owner_guest_ = scope->intent_.owner_guest;
    if (value.origin != api::Origin::kFresh) FailLocked();
  }
  void OnDestroy(uint64_t cookie) {
    std::lock_guard lock(mutex_);
    if (frozen_) { Increment(late_); return; }
    if (!cookie_ || cookie != cookie_) { Increment(ignored_operations_); return; }
    journal_->destroy(cookie, ReadClockLocked()); destroyed_ = true;
  }
  uint64_t OnBegin(const api::Admission& value) {
    std::lock_guard lock(mutex_);
    if (frozen_) { Increment(late_); return 0; }
    auto* scope = operation_scope_;
    // Helper eligibility accounts for ALL synchronous SDK admissions, even
    // ignored objects. Never mistake one selected wait among several for the
    // exact helper operation. This does not fabricate unrelated journal rows.
    for (auto* active = scope; active; active = active->previous_) {
      if (active->owner_ != this) continue;
      if (active->begin_attempts_ != UINT64_MAX) ++active->begin_attempts_; else active->ambiguous_ = true;
      if (active->begin_attempts_ != 1 || active->intent_.cookie != value.cookie || active->intent_.expected_kind != value.kind) {
        active->ambiguous_ = true; active->wait_.reset();
        if (active->ticket_) {
          // A previously admitted update label cannot be erased from immutable
          // raw history. Invalidate association integrity instead; PREFIX-v2
          // must never consume that label after the enclosing helper becomes
          // ambiguous, even when all native operations were still observed.
          if (journal_->operations()[active->ticket_ - 1].context.kind == core::ContextKind::kUpdate)
            FailLocked();
          auto& old = annotations_[active->ticket_ - 1]; old.annotated = false; old.phase_call_id = 0; old.outer_lr = 0;
        }
      }
    }
    if (!cookie_ || value.cookie != cookie_) { Increment(ignored_operations_); return 0; }
    bool annotated = scope && scope->owner_ == this && scope->intent_.cookie == value.cookie &&
        !scope->ambiguous_ && scope->begin_attempts_ == 1 &&
        scope->intent_.expected_kind == value.kind && scope->intent_.lane.context && scope->intent_.lane.base &&
        scope->intent_.outer_lr && scope->intent_.handle && *scope->intent_.handle && bound_ && !destroyed_ &&
        journal_->lookup(cookie_) == epoch_ && !scope->ticket_;
    if (!annotated) { Increment(mismatches_); if (scope && scope->owner_ == this && scope->ticket_) scope->ambiguous_ = true; }
    core::ReleaseContext context{};
    if (annotated && value.kind == api::Kind::kRelease) {
      context = scope->intent_.release_context;
      if (core::Name(context.kind).empty() || (context.kind == core::ContextKind::kUpdate &&
          (scope->intent_.outer_lr != kReleaseLR || !context.generation.valid())) ||
          (context.kind != core::ContextKind::kUpdate && context.generation != frame_context::Id{})) {
        context = {}; annotated = false; Increment(mismatches_);
      }
    }
    const auto qpc = ReadClockLocked(); const auto tid = GetCurrentThreadId();
    const auto handle = annotated ? scope->intent_.handle : std::nullopt;
    core::BeginResult admitted{core::Disposition::kInvalidArgument, {}};
    if (value.kind == api::Kind::kRelease) admitted = journal_->begin_release(value.cookie, tid, handle, qpc, value.count, context);
    else if (value.kind == api::Kind::kWait) {
      const auto timeout = value.timeout == api::TimeoutClass::kZero ? core::TimeoutKind::kZero :
          value.timeout == api::TimeoutClass::kFinite ? core::TimeoutKind::kFinite : core::TimeoutKind::kInfinite;
      admitted = journal_->begin_wait(value.cookie, tid, handle, qpc, timeout);
    } else admitted = journal_->begin_unsupported(value.cookie, tid, handle, qpc, Kind(value.kind));
    if (!admitted.ticket.valid()) return 0;
    const auto id = admitted.ticket.op_id(); tickets_[id - 1] = admitted.ticket;
    annotations_[id - 1] = {id, annotated ? scope->intent_.phase_call_id : 0, annotated ? scope->intent_.outer_lr : 0, tid, annotated};
    if (scope && scope->owner_ == this && !scope->ticket_) scope->ticket_ = id;
    return id;
  }
  void OnEnd(const api::Completion& value) {
    std::lock_guard lock(mutex_);
    if (frozen_) { Increment(late_); return; } // Never mutate stopped Journal.
    if (!value.ticket || value.ticket > journal_->counts().stored_operations) { FailLocked(); return; }
    if (journal_->operations()[value.ticket - 1].native_thread_id != GetCurrentThreadId()) {
      FailLocked(); return; // Synchronous SDK seam; never complete another thread's ticket.
    }
    const auto ticket = tickets_[value.ticket - 1]; core::Disposition result;
    if (value.kind == api::Kind::kRelease) result = journal_->end_release(ticket, ReadClockLocked(), value.result == api::Result::kSuccess, value.previous_count);
    else if (value.kind == api::Kind::kWait) result = journal_->end_wait(ticket, ReadClockLocked(), Outcome(value.result));
    else {
      if (journal_->operations()[value.ticket - 1].kind != Kind(value.kind)) { FailLocked(); return; }
      result = journal_->end_unsupported(ticket, ReadClockLocked());
    }
    if (result != core::Disposition::kRecorded) { FailLocked(); return; }
    auto* scope = operation_scope_;
    if (value.kind == api::Kind::kWait && scope && scope->owner_ == this && scope->ticket_ == value.ticket && !scope->ambiguous_)
      scope->wait_ = WaitObservation{value.ticket, annotations_[value.ticket - 1].phase_call_id, Outcome(value.result), annotations_[value.ticket - 1].annotated};
  }
  api::Registry& registry_; ClockProvider clock_;
  mutable std::mutex mutex_;
  bool configured_ = false, frozen_ = false, bound_ = false, destroyed_ = false;
  core::Window window_{}; std::array<char, 32> token_{};
  std::unique_ptr<core::Journal> journal_;
  uint64_t cookie_ = 0, epoch_ = 0; uint32_t handle_ = 0, owner_guest_ = 0;
  std::array<core::Ticket, core::kOperationCapacity> tickets_{};
  std::array<AnnotationRecord, core::kOperationCapacity> annotations_{};
  std::atomic<uint64_t> failures_{0}, ignored_creations_{0}, ignored_operations_{0}, mismatches_{0}, late_{0};
  inline static thread_local ScopedCreate* create_scope_ = nullptr;
  inline static thread_local ScopedOperation* operation_scope_ = nullptr;
};
} // namespace mcla::semaphore_collector
