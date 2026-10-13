#pragma once
// Supplied-observation bookkeeping only: no SDK, clocks, OS calls or
// release-to-wait association. The adapter MUST externally serialize EACH
// method, never an original operation's begin-to-end interval. Prefer heap
// storage; const spans are not independent snapshots and require that same
// serialization. Runtime coverage and all supplied identities remain unproven.
#include "mcla_frame_context_core.h"
#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <span>
#include <string_view>

namespace mcla::semaphore_observer {
inline constexpr size_t kEpochCapacity = 64, kOperationCapacity = 4096;
inline constexpr uint64_t kMaxQpc = uint64_t(std::numeric_limits<int64_t>::max());
inline constexpr uint32_t kMaxCount = uint32_t(std::numeric_limits<int32_t>::max());
inline constexpr bool kNativeAdmitted = false, kSimulationProven = false,
    kRenderProven = false, kFpsProven = false, kFrameToStepMappingProven = false,
    kObjectIdentityProven = false, kObjectLifetimeProven = false,
    kOnlineAssociationProven = false;

namespace detail {
// Constructor-only identity issuance. This inline counter must live for the
// process lifetime: never reset/unload its implementation while tickets exist.
// The helper's local-counter seam tests saturation, not a Journal injection API.
inline std::atomic<uint64_t> last_journal_generation{0};
inline uint64_t AllocateJournalGeneration(std::atomic<uint64_t>& last) noexcept {
  auto observed = last.load(std::memory_order_relaxed);
  while (observed != UINT64_MAX) {
    const auto next = observed + 1;
    if (last.compare_exchange_strong(observed, next, std::memory_order_relaxed)) return next;
  }
  return 0; // Saturated forever; zero is never an issued identity.
}
} // namespace detail

struct Window { uint64_t begin_qpc{}, end_qpc_exclusive{}, qpc_frequency{}; };
enum class Origin { kFresh, kNative, kRestored, kUnknown };
enum class Kind { kRelease, kWait, kWaitMultiple, kSignalAndWait, kSave, kRestore, kLifecycleGap, kUnknown };
enum class ContextKind { kUpdate, kControl, kUnknown };
enum class TimeoutKind { kZero, kFinite, kInfinite };
enum class WaitResult { kSuccess, kTimeout, kApc, kAlerted, kAbandoned, kFailed, kUnknown };
enum class Disposition { kRecorded, kDisabled, kStopped, kUnknownObject, kCapacityExhausted,
                         kInvalidArgument, kInvalidTicket, kAlreadyCompleted };
enum class Issue : uint32_t {
  kInvalidConfiguration = 1u << 0, kUnknownObject = 1u << 1,
  kInvalidArgument = 1u << 2, kInvalidClock = 1u << 3,
  kOutsideWindow = 1u << 4, kDuplicateCreation = 1u << 5,
  kAmbiguousReuse = 1u << 6, kEpochOverflow = 1u << 7,
  kOperationOverflow = 1u << 8, kIncompleteCut = 1u << 9,
  kUnsupported = 1u << 10, kCompletionMismatch = 1u << 11,
  kLifetimeBoundary = 1u << 12, kCounterOverflow = 1u << 13,
  kEarlyCut = 1u << 14, kStoppedObservation = 1u << 15,
  kNonfreshOrigin = 1u << 16, kInitialCredits = 1u << 17,
  kExternalFailure = 1u << 18, kIdentityExhausted = 1u << 19,
};
struct ReleaseContext {
  ContextKind kind = ContextKind::kUnknown;
  frame_context::Id generation{};  // Exact {thread_epoch, serial}, not a step.
};
struct Counts {
  // Known-epoch begins only. Overflow begins have no retained completion;
  // completed_operations counts real retained completions, never guessed ones.
  uint64_t observed_operations{}, stored_operations{}, completed_operations{}, overflow_operations{};
};
struct Diagnostics { uint64_t unknown_object_attempts{}, rejected_attempts{}, stopped_attempts{}; };
struct Epoch {
  uint64_t epoch_id{}, object_cookie{}, creation_qpc{};
  std::optional<uint64_t> destroy_qpc;
  uint32_t initial_count{}, maximum_count{}, pending_operations{};
  Origin origin = Origin::kUnknown;
  bool history_complete = false;
};
struct Operation {
  uint64_t op_id{}, epoch_id{}, entry_qpc{}, exit_qpc{};
  uint32_t native_thread_id{};
  std::optional<uint32_t> guest_handle;
  Kind kind = Kind::kUnknown;
  bool completed = false;
  uint32_t release_count{};
  ReleaseContext context{};
  bool release_success = false;
  std::optional<int32_t> previous_count;
  TimeoutKind timeout_kind = TimeoutKind::kInfinite;
  WaitResult wait_result = WaitResult::kUnknown;
};
class Journal;
class Ticket {
 public:
  constexpr Ticket() = default;
  constexpr bool valid() const noexcept { return owner_ && generation_ && op_id_ && epoch_id_; }
  uint64_t op_id() const noexcept { return op_id_; }
  uint64_t epoch_id() const noexcept { return epoch_id_; }
  uint64_t journal_generation() const noexcept { return generation_; }
 private:
  friend class Journal;
  Ticket(const Journal* owner, uint64_t generation, uint64_t op, uint64_t epoch) noexcept
      : owner_(owner), generation_(generation), op_id_(op), epoch_id_(epoch) {}
  const Journal* owner_ = nullptr;
  uint64_t generation_ = 0, op_id_ = 0, epoch_id_ = 0;
};
struct EpochResult { Disposition disposition; uint64_t epoch_id = 0; };
struct BeginResult { Disposition disposition; Ticket ticket{}; };

inline std::string_view Name(Origin value) noexcept {
  switch (value) { case Origin::kFresh: return "fresh"; case Origin::kNative: return "native";
    case Origin::kRestored: return "restored"; case Origin::kUnknown: return "unknown"; }
  return {};
}
inline std::string_view Name(Kind value) noexcept {
  switch (value) { case Kind::kRelease: return "release"; case Kind::kWait: return "wait";
    case Kind::kWaitMultiple: return "wait_multiple"; case Kind::kSignalAndWait: return "signal_and_wait";
    case Kind::kSave: return "save"; case Kind::kRestore: return "restore";
    case Kind::kLifecycleGap: return "lifecycle_gap"; case Kind::kUnknown: return "unknown"; }
  return {};
}
inline std::string_view Name(ContextKind value) noexcept {
  switch (value) { case ContextKind::kUpdate: return "update"; case ContextKind::kControl: return "control";
    case ContextKind::kUnknown: return "unknown"; }
  return {};
}
inline std::string_view Name(TimeoutKind value) noexcept {
  switch (value) { case TimeoutKind::kZero: return "zero"; case TimeoutKind::kFinite: return "finite";
    case TimeoutKind::kInfinite: return "infinite"; }
  return {};
}
inline std::string_view Name(WaitResult value) noexcept {
  switch (value) { case WaitResult::kSuccess: return "success"; case WaitResult::kTimeout: return "timeout";
    case WaitResult::kApc: return "apc"; case WaitResult::kAlerted: return "alerted";
    case WaitResult::kAbandoned: return "abandoned"; case WaitResult::kFailed: return "failed";
    case WaitResult::kUnknown: return "unknown"; }
  return {};
}

class Journal {
 public:
  // This lifetime window starts BEFORE creation, independently of image scouts.
  explicit Journal(Window window, bool enabled = false) noexcept : window_(window), enabled_(enabled) {
    configured_ = window.begin_qpc && window.begin_qpc < window.end_qpc_exclusive &&
                  window.end_qpc_exclusive <= kMaxQpc && window.qpc_frequency &&
                  window.qpc_frequency <= kMaxQpc;
    if (enabled && !configured_) mark(Issue::kInvalidConfiguration);
    if (enabled && configured_) {
      generation_ = detail::AllocateJournalGeneration(detail::last_journal_generation);
      if (!generation_) mark(Issue::kIdentityExhausted);
    }
  }
  Journal(const Journal&) = delete;
  Journal& operator=(const Journal&) = delete;
  Journal(Journal&&) = delete;
  Journal& operator=(Journal&&) = delete;

  EpochResult create(uint64_t cookie, uint64_t qpc, uint32_t initial, uint32_t maximum,
                     Origin origin = Origin::kFresh) noexcept {
    note_mutation();
    if (auto rejected = admission(qpc)) return {*rejected, 0};
    if (!cookie || !maximum || maximum > kMaxCount || initial > maximum || Name(origin).empty()) {
      reject_argument(); return {Disposition::kInvalidArgument, 0};
    }
    for (size_t i = epoch_count_; i; --i) {
      const auto& old = epochs_[i - 1];
      if (old.object_cookie != cookie) continue;
      if (!old.destroy_qpc) { mark(Issue::kDuplicateCreation); return {Disposition::kInvalidArgument, 0}; }
      if (!valid_clock(*old.destroy_qpc) || *old.destroy_qpc <= old.creation_qpc || qpc <= *old.destroy_qpc) {
        mark(Issue::kAmbiguousReuse); return {Disposition::kInvalidArgument, 0};
      }
      break;
    }
    if (epoch_count_ == kEpochCapacity) { mark(Issue::kEpochOverflow); return {Disposition::kCapacityExhausted, 0}; }
    auto& row = epochs_[epoch_count_++];
    row.epoch_id = epoch_count_; row.object_cookie = cookie; row.creation_qpc = qpc;
    row.initial_count = initial; row.maximum_count = maximum; row.origin = origin;
    row.history_complete = clock(qpc) && origin == Origin::kFresh;
    if (origin != Origin::kFresh) mark(Issue::kNonfreshOrigin);
    if (initial) mark(Issue::kInitialCredits);
    return {Disposition::kRecorded, row.epoch_id};
  }

  // Aliased handles share the SDK-resolved object's cookie/epoch. Lookup does
  // not create an epoch or claim a handle lifetime.
  uint64_t lookup(uint64_t cookie) const noexcept {
    for (size_t i = epoch_count_; i; --i)
      if (epochs_[i - 1].object_cookie == cookie && !epochs_[i - 1].destroy_qpc) return epochs_[i - 1].epoch_id;
    return 0;
  }
  Disposition destroy(uint64_t cookie, uint64_t qpc) noexcept {
    note_mutation();
    if (auto rejected = admission(qpc)) return *rejected;
    auto* epoch = find(cookie);
    if (!epoch) return unknown();
    epoch->destroy_qpc = qpc;
    if (!clock(qpc) || qpc <= epoch->creation_qpc || epoch->pending_operations ||
        qpc <= last_completed_exit_[epoch->epoch_id - 1]) {
      epoch->history_complete = false; mark(Issue::kLifetimeBoundary);
    }
    return Disposition::kRecorded;
  }
  BeginResult begin_release(uint64_t cookie, uint32_t tid, std::optional<uint32_t> handle,
                            uint64_t qpc, uint32_t count, ReleaseContext context = {}) noexcept {
    note_mutation();
    auto result = begin(cookie, tid, handle, qpc, Kind::kRelease);
    if (result.ticket.valid()) {
      auto& row = operations_[result.ticket.op_id() - 1]; row.release_count = count; row.context = context;
      if (!count || count > kMaxCount || Name(context.kind).empty() ||
          (context.kind == ContextKind::kUpdate ? !context.generation.valid() :
           context.generation != frame_context::Id{})) reject_argument();
    }
    return result;
  }
  BeginResult begin_wait(uint64_t cookie, uint32_t tid, std::optional<uint32_t> handle,
                         uint64_t qpc, TimeoutKind timeout) noexcept {
    note_mutation();
    auto result = begin(cookie, tid, handle, qpc, Kind::kWait);
    if (result.ticket.valid()) {
      operations_[result.ticket.op_id() - 1].timeout_kind = timeout;
      if (Name(timeout).empty()) reject_argument();
    }
    return result;
  }
  BeginResult begin_unsupported(uint64_t cookie, uint32_t tid, std::optional<uint32_t> handle,
                                uint64_t qpc, Kind kind) noexcept {
    note_mutation();
    if (!enabled_) return {Disposition::kDisabled, {}};
    if (Name(kind).empty() || kind == Kind::kRelease || kind == Kind::kWait) {
      reject_argument(); return {Disposition::kInvalidArgument, {}};
    }
    auto result = begin(cookie, tid, handle, qpc, kind);
    if (result.ticket.valid()) {
      epochs_[result.ticket.epoch_id() - 1].history_complete = false; mark(Issue::kUnsupported);
    }
    return result;
  }
  Disposition end_release(Ticket ticket, uint64_t qpc, bool success,
                          std::optional<int32_t> previous = std::nullopt) noexcept {
    note_mutation();
    if (!enabled_) return Disposition::kDisabled;
    auto* row = completion(ticket, qpc, Kind::kRelease);
    if (!row) return completion_disposition_;
    row->release_success = success;
    row->previous_count = success ? previous : std::nullopt; // Failed native output is undefined.
    if (success && (!previous || *previous < 0 || uint32_t(*previous) > epochs_[row->epoch_id - 1].maximum_count))
      reject_argument();
    return Disposition::kRecorded;
  }
  Disposition end_wait(Ticket ticket, uint64_t qpc, WaitResult result) noexcept {
    note_mutation();
    if (!enabled_) return Disposition::kDisabled;
    auto* row = completion(ticket, qpc, Kind::kWait);
    if (!row) return completion_disposition_;
    row->wait_result = result;
    if (Name(result).empty()) reject_argument();
    if (result == WaitResult::kAbandoned || result == WaitResult::kFailed || result == WaitResult::kUnknown)
      mark(Issue::kUnsupported);
    return Disposition::kRecorded;
  }
  Disposition end_unsupported(Ticket ticket, uint64_t qpc) noexcept {
    note_mutation();
    if (!enabled_) return Disposition::kDisabled;
    if (!owns(ticket) || operations_[ticket.op_id() - 1].kind == Kind::kRelease ||
        operations_[ticket.op_id() - 1].kind == Kind::kWait) {
      mark(Issue::kCompletionMismatch); return Disposition::kInvalidTicket;
    }
    return completion(ticket, qpc, operations_[ticket.op_id() - 1].kind)
               ? Disposition::kRecorded : completion_disposition_;
  }
  void fail() noexcept { note_mutation(); if (enabled_) mark(Issue::kExternalFailure); }
  void stop_admissions(uint64_t qpc) noexcept {
    if (!enabled_) return;
    if (stopped_) return;
    stopped_ = true; cut_qpc_ = qpc;
    if (!valid_clock(qpc)) mark(Issue::kInvalidClock);
    if (qpc < window_.end_qpc_exclusive) mark(Issue::kEarlyCut);
    // A later drain cannot restore observations omitted during this cut.
    if (pending_) mark(Issue::kIncompleteCut);
  }
  bool drained() const noexcept { return pending_ == 0; }
  bool stopped() const noexcept { return stopped_; }
  // Separate freeze-identity guard, NOT a replacement for v1 issue/history
  // semantics. Even a duplicate completion or repeated fail changes the
  // observation attempt history after the cut. Repeat stop remains a no-op.
  bool post_stop_mutated() const noexcept { return post_stop_mutated_; }
  bool history_complete() const noexcept {
    return enabled_ && configured_ && generation_ && stopped_ && drained() && issues_ == 0;
  }
  bool observer_failed() const noexcept { return issues_ != 0; }
  bool has(Issue issue) const noexcept { return (issues_ & uint32_t(issue)) != 0; }
  uint32_t issues() const noexcept { return issues_; }
  uint32_t pending_operations() const noexcept { return pending_; }
  std::optional<uint64_t> cut_qpc() const noexcept { return cut_qpc_; }
  Window window() const noexcept { return window_; }
  Counts counts() const noexcept { return counts_; }
  Diagnostics diagnostics() const noexcept { return diagnostics_; }
  std::span<const Epoch> epochs() const noexcept { return {epochs_.data(), epoch_count_}; }
  std::span<const Operation> operations() const noexcept { return {operations_.data(), size_t(counts_.stored_operations)}; }
  // False for raw pending/faulted fields that the CURRENT v1 parser cannot
  // represent. An adapter must report failure, not change counts or schemas.
  bool audit_v1_shape_compatible() const noexcept {
    if (!enabled_ || !configured_ || !generation_ || !epoch_count_ || !drained() || malformed_fields_ ||
        counts_.completed_operations < counts_.stored_operations) return false;
    return true;
  }

 private:
  void note_mutation() noexcept { if (enabled_ && stopped_) post_stop_mutated_ = true; }
  void mark(Issue issue) noexcept { issues_ |= uint32_t(issue); }
  void increment(uint64_t& value) noexcept {
    if (value == UINT64_MAX) mark(Issue::kCounterOverflow); else ++value;
  }
  void reject_argument() noexcept {
    malformed_fields_ = true; mark(Issue::kInvalidArgument); increment(diagnostics_.rejected_attempts);
  }
  bool valid_clock(uint64_t qpc) const noexcept { return qpc && qpc <= kMaxQpc; }
  bool clock(uint64_t qpc) noexcept {
    if (!valid_clock(qpc)) { mark(Issue::kInvalidClock); if (qpc > kMaxQpc) malformed_fields_ = true; return false; }
    if (qpc < window_.begin_qpc || qpc >= window_.end_qpc_exclusive) { mark(Issue::kOutsideWindow); return false; }
    return true;
  }
  std::optional<Disposition> admission(uint64_t qpc) noexcept {
    if (!enabled_) return Disposition::kDisabled;
    if (!configured_) return Disposition::kInvalidArgument;
    if (!generation_) return Disposition::kCapacityExhausted;
    if (stopped_) {
      increment(diagnostics_.stopped_attempts);
      if (!valid_clock(qpc) || qpc < window_.end_qpc_exclusive) mark(Issue::kStoppedObservation);
      return Disposition::kStopped;
    }
    return std::nullopt;
  }
  Epoch* find(uint64_t cookie) noexcept { auto id = lookup(cookie); return id ? &epochs_[id - 1] : nullptr; }
  Disposition unknown() noexcept {
    increment(diagnostics_.unknown_object_attempts); mark(Issue::kUnknownObject); return Disposition::kUnknownObject;
  }
  BeginResult begin(uint64_t cookie, uint32_t tid, std::optional<uint32_t> handle,
                    uint64_t qpc, Kind kind) noexcept {
    if (auto rejected = admission(qpc)) return {*rejected, {}};
    auto* epoch = find(cookie);
    if (!epoch) return {unknown(), {}};
    if (counts_.observed_operations == UINT64_MAX) { mark(Issue::kCounterOverflow); return {Disposition::kCapacityExhausted, {}}; }
    ++counts_.observed_operations;
    if (counts_.stored_operations == kOperationCapacity) {
      ++counts_.overflow_operations; epoch->history_complete = false; mark(Issue::kOperationOverflow);
      return {Disposition::kCapacityExhausted, {}};
    }
    auto& row = operations_[size_t(counts_.stored_operations++)];
    row.op_id = counts_.observed_operations; row.epoch_id = epoch->epoch_id; row.kind = kind;
    row.native_thread_id = tid; row.guest_handle = handle; row.entry_qpc = qpc;
    if (!tid || (handle && !*handle)) reject_argument();
    if (!clock(qpc) || qpc <= epoch->creation_qpc) { epoch->history_complete = false; mark(Issue::kLifetimeBoundary); }
    ++epoch->pending_operations; ++pending_;
    return {Disposition::kRecorded, Ticket{this, generation_, row.op_id, row.epoch_id}};
  }
  bool owns(Ticket ticket) const noexcept {
    return ticket.owner_ == this && generation_ && ticket.generation_ == generation_ &&
           ticket.op_id() && ticket.op_id() <= counts_.stored_operations &&
           operations_[ticket.op_id() - 1].epoch_id == ticket.epoch_id();
  }
  Operation* completion(Ticket ticket, uint64_t qpc, Kind kind) noexcept {
    if (!owns(ticket) || operations_[ticket.op_id() - 1].kind != kind) {
      mark(Issue::kCompletionMismatch); completion_disposition_ = Disposition::kInvalidTicket; return nullptr;
    }
    auto& row = operations_[ticket.op_id() - 1];
    if (row.completed) { mark(Issue::kCompletionMismatch); completion_disposition_ = Disposition::kAlreadyCompleted; return nullptr; }
    row.completed = true; row.exit_qpc = qpc;
    auto& epoch = epochs_[row.epoch_id - 1];
    ++counts_.completed_operations; --pending_; --epoch.pending_operations;
    // Destruction can be observed after this completion; retain its outer
    // bound so a later regressing/touching destruction cannot claim validity.
    auto& last_exit = last_completed_exit_[row.epoch_id - 1];
    if (qpc > last_exit) last_exit = qpc;
    if (!clock(qpc) || qpc < row.entry_qpc || (epoch.destroy_qpc && qpc >= *epoch.destroy_qpc)) {
      epoch.history_complete = false; mark(Issue::kLifetimeBoundary);
      if (qpc < row.entry_qpc) mark(Issue::kInvalidClock);
    }
    return &row;
  }
  Window window_{};
  uint64_t generation_ = 0;
  bool enabled_ = false, configured_ = false, stopped_ = false, malformed_fields_ = false;
  bool post_stop_mutated_ = false;
  uint32_t issues_ = 0, pending_ = 0;
  std::optional<uint64_t> cut_qpc_;
  Counts counts_{};
  Diagnostics diagnostics_{};
  size_t epoch_count_ = 0;
  Disposition completion_disposition_ = Disposition::kInvalidTicket;
  std::array<uint64_t, kEpochCapacity> last_completed_exit_{};
  std::array<Epoch, kEpochCapacity> epochs_{};
  std::array<Operation, kOperationCapacity> operations_{};
};
}  // namespace mcla::semaphore_observer
