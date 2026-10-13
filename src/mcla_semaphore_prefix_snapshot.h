#pragma once
// Offline bridge only. CALLER must serialize host callbacks and freeze as:
// host lock -> real QPC cut -> host frozen -> Journal stop -> transport Disable
// -> Capture, BEFORE drain or normal shutdown. This API does none of those
// runtime actions. Coverage is a separately supplied claim, never native proof.
#include "mcla_semaphore_observer_core.h"
#include <algorithm>
#include <charconv>
#include <memory>
#include <new>
#include <string>
#include <utility>

namespace mcla::semaphore_prefix_snapshot {
namespace core = semaphore_observer;
inline constexpr size_t kMaxJsonBytes = 2 * 1024 * 1024;
enum class Coverage { kUnknown, kCompleteThroughFreeze };
struct EpochCoverage {
  uint64_t epoch_id{}, cookie{}, creation_qpc{};
  Coverage creation = Coverage::kUnknown, operations = Coverage::kUnknown;
};
struct AdmissionCoverage {
  std::string_view token;
  uint64_t observer_begin_qpc{}, qpc_frequency{}, freeze_qpc{};
  Coverage inventory = Coverage::kUnknown;
  bool unknown_coverage_gap = true, observer_failed = false;
  std::span<const EpochCoverage> epochs;
};
enum class Error { kNone, kToken, kScopeBinding, kCoverageBinding, kCounts,
                   kNotFreezeSnapshot, kUnrepresentable, kTooLarge, kAllocation };
class Snapshot;
struct CaptureResult { Error error; std::unique_ptr<const Snapshot> snapshot; };
struct JsonResult { Error error; std::string json; };

namespace detail {
inline bool Token(std::string_view token) noexcept {
  return token.size() == 32 && std::all_of(token.begin(), token.end(), [](char c) {
    return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f');
  });
}
inline bool Valid(Coverage value) noexcept { return value == Coverage::kUnknown || value == Coverage::kCompleteThroughFreeze; }
class Writer {
 public:
  void Raw(std::string_view value) {
    if (value.size() > kMaxJsonBytes - text.size()) { too_large = true; return; }
    if (!too_large) text.append(value);
  }
  // Only reviewed literal names, enum names and exact lower-hex token go here.
  void S(std::string_view value) { Raw("\""); Raw(value); Raw("\""); }
  void U(uint64_t value) { char buffer[21]; const auto result = std::to_chars(buffer, buffer + sizeof(buffer), value); Raw({buffer, size_t(result.ptr - buffer)}); }
  void I(int64_t value) { char buffer[21]; const auto result = std::to_chars(buffer, buffer + sizeof(buffer), value); Raw({buffer, size_t(result.ptr - buffer)}); }
  void B(bool value) { Raw(value ? "true" : "false"); }
  void Key(std::string_view name) { Raw(","); S(name); Raw(":"); }
  void UInt(std::string_view name, uint64_t value) { Key(name); U(value); }
  void Bool(std::string_view name, bool value) { Key(name); B(value); }
  void Text(std::string_view name, std::string_view value) { Key(name); S(value); }
  void Clock(uint64_t value) { if (value) U(value); else Raw("null"); }
  void Previous(const std::optional<int32_t>& value) { if (value) I(*value); else Raw("null"); }
  std::string text;
  bool too_large = false;
};
inline void Proofs(Writer& w, bool prefix) {
  w.Raw(",\"native_admitted\":false,\"simulation_proven\":false,\"render_proven\":false,\"fps_proven\":false,"
        "\"frame_to_step_mapping_proven\":false,\"object_identity_proven\":false,\"object_lifetime_proven\":false");
  if (prefix) w.Raw(",\"prefix_history_proven\":false,\"update_causality_proven\":false");
}
} // namespace detail

class Snapshot {
 public:
  ~Snapshot() = default;
  Snapshot(const Snapshot&) = delete;
  Snapshot& operator=(const Snapshot&) = delete;
  Snapshot(Snapshot&&) = delete;
  Snapshot& operator=(Snapshot&&) = delete;
  static CaptureResult Capture(const core::Journal& journal, std::string_view expected_token,
                               const AdmissionCoverage& coverage) noexcept {
    if (!detail::Token(expected_token) || coverage.token != expected_token) return {Error::kToken, {}};
    const auto window = journal.window(); const auto cut = journal.cut_qpc();
    if (!journal.stopped() || !cut || *cut != coverage.freeze_qpc ||
        coverage.observer_begin_qpc != window.begin_qpc || coverage.qpc_frequency != window.qpc_frequency ||
        !window.begin_qpc || window.begin_qpc >= *cut || *cut >= window.end_qpc_exclusive ||
        window.end_qpc_exclusive > core::kMaxQpc || !window.qpc_frequency || window.qpc_frequency > core::kMaxQpc)
      return {Error::kScopeBinding, {}};
    // No changed-cut recapture, even if pending remains nonzero or a later
    // completion shares the same QPC tick. Clock comparison cannot prove it.
    if (journal.post_stop_mutated()) return {Error::kNotFreezeSnapshot, {}};
    const auto counts = journal.counts(); const auto epochs = journal.epochs(); const auto ops = journal.operations();
    if (epochs.empty() || epochs.size() > core::kEpochCapacity || ops.size() > core::kOperationCapacity ||
        counts.stored_operations != ops.size() || counts.completed_operations > counts.stored_operations ||
        counts.observed_operations < counts.stored_operations ||
        counts.observed_operations - counts.stored_operations != counts.overflow_operations ||
        counts.stored_operations - counts.completed_operations != journal.pending_operations()) return {Error::kCounts, {}};
    if (journal.has(core::Issue::kIncompleteCut) != (journal.pending_operations() != 0))
      return {Error::kNotFreezeSnapshot, {}};
    if (!detail::Valid(coverage.inventory) || coverage.epochs.size() != epochs.size()) return {Error::kCoverageBinding, {}};
    std::array<EpochCoverage, core::kEpochCapacity> bound{};
    for (size_t i = 0; i < epochs.size(); ++i) {
      bool found = false;
      for (const auto& claim : coverage.epochs) {
        if (!detail::Valid(claim.creation) || !detail::Valid(claim.operations)) return {Error::kCoverageBinding, {}};
        if (claim.epoch_id != epochs[i].epoch_id) continue;
        if (found || claim.cookie != epochs[i].object_cookie || claim.creation_qpc != epochs[i].creation_qpc)
          return {Error::kCoverageBinding, {}};
        found = true; bound[i] = claim;
      }
      if (!found) return {Error::kCoverageBinding, {}};
    }
    try {
      std::unique_ptr<Snapshot> result(new Snapshot);
      std::copy(expected_token.begin(), expected_token.end(), result->token_.begin());
      result->window_ = window; result->freeze_ = *cut; result->counts_ = counts;
      result->diagnostics_ = journal.diagnostics(); result->issues_ = journal.issues();
      result->pending_ = journal.pending_operations(); result->raw_history_ = journal.history_complete();
      result->raw_post_stop_mutated_ = journal.post_stop_mutated();
      result->raw_failed_ = journal.observer_failed(); result->epoch_count_ = epochs.size();
      std::copy(epochs.begin(), epochs.end(), result->epochs_.begin());
      std::copy(ops.begin(), ops.end(), result->operations_.begin());
      result->coverage_ = bound; result->inventory_ = coverage.inventory;
      result->coverage_gap_ = coverage.unknown_coverage_gap; result->coverage_failed_ = coverage.observer_failed;
      return {Error::kNone, std::move(result)};
    } catch (const std::bad_alloc&) { return {Error::kAllocation, {}}; }
  }
  uint32_t raw_issues() const noexcept { return issues_; }
  bool raw_history_complete() const noexcept { return raw_history_; }
  bool raw_observer_failed() const noexcept { return raw_failed_; }
  bool raw_post_stop_mutated() const noexcept { return raw_post_stop_mutated_; }
  core::Counts raw_counts() const noexcept { return counts_; }
  uint32_t pending_operations() const noexcept { return pending_; }
  std::span<const core::Epoch> raw_epochs() const noexcept { return {epochs_.data(), epoch_count_}; }
  std::span<const core::Operation> raw_operations() const noexcept { return {operations_.data(), size_t(counts_.stored_operations)}; }

  JsonResult PrefixJson() const noexcept {
    if (!Representable()) return {Error::kUnrepresentable, {}};
    try {
      detail::Writer w;
      Header(w, "mcla-semaphore-prefix-journal-v2");
      w.UInt("observer_begin_qpc", window_.begin_qpc); w.UInt("freeze_qpc", freeze_);
      w.UInt("capacity", core::kOperationCapacity);
      const bool fatal = Fatal();
      w.Bool("admission_inventory_complete", inventory_ == Coverage::kCompleteThroughFreeze && !fatal);
      w.Bool("observer_failed", coverage_failed_ || fatal);
      w.Bool("unknown_coverage_gap", coverage_gap_ || fatal);
      detail::Proofs(w, true);
      w.Raw(",\"counts\":{\"observed_operations\":"); w.U(counts_.observed_operations);
      w.UInt("stored_operations", counts_.stored_operations); w.UInt("completed_operations", counts_.completed_operations);
      w.UInt("pending_operations", pending_); w.UInt("lost_operations", counts_.overflow_operations); w.Raw("}");
      // Claims remain descriptive: full native hook/coverage proof is outside
      // this bridge. They never overwrite the copied raw-core history flags.
      w.Raw(",\"epochs\":[");
      for (size_t i = 0; i < epoch_count_; ++i) {
        if (i) w.Raw(","); Epoch(w, epochs_[i], coverage_[i].creation == Coverage::kCompleteThroughFreeze &&
                                                   coverage_[i].operations == Coverage::kCompleteThroughFreeze);
      }
      w.Raw("],\"operations\":[");
      for (size_t i = 0; i < counts_.stored_operations; ++i) { if (i) w.Raw(","); PrefixOperation(w, operations_[i]); }
      w.Raw("],\"loss_ranges\":[");
      if (counts_.overflow_operations) {
        w.Raw("{\"first_op_id\":"); w.U(counts_.stored_operations + 1); w.UInt("last_op_id", counts_.observed_operations);
        // Core has NO localized overflow clock. Never infer one from a neighbor.
        w.Raw(",\"entry_lower_bound_qpc\":null,\"kind\":\"overflow\"}");
      }
      w.Raw("]}"); return Finish(std::move(w));
    } catch (const std::bad_alloc&) { return {Error::kAllocation, {}}; }
  }
  JsonResult RawCoreJson() const noexcept {
    try {
      detail::Writer w; Header(w, "mcla-semaphore-raw-core-snapshot-v1");
      w.UInt("begin_qpc", window_.begin_qpc); w.UInt("end_qpc_exclusive", window_.end_qpc_exclusive);
      w.UInt("freeze_qpc", freeze_); w.UInt("issues", issues_);
      w.Bool("history_complete", raw_history_); w.Bool("observer_failed", raw_failed_);
      w.Bool("post_stop_mutated", raw_post_stop_mutated_);
      w.UInt("pending_operations", pending_); detail::Proofs(w, false);
      w.Raw(",\"counts\":{\"observed_operations\":"); w.U(counts_.observed_operations);
      w.UInt("stored_operations", counts_.stored_operations); w.UInt("completed_operations", counts_.completed_operations);
      w.UInt("overflow_operations", counts_.overflow_operations); w.Raw("}");
      w.Raw(",\"diagnostics\":{\"unknown_object_attempts\":"); w.U(diagnostics_.unknown_object_attempts);
      w.UInt("rejected_attempts", diagnostics_.rejected_attempts); w.UInt("stopped_attempts", diagnostics_.stopped_attempts); w.Raw("}");
      w.Raw(",\"coverage_claims\":{\"inventory_complete\":"); w.B(inventory_ == Coverage::kCompleteThroughFreeze);
      w.Bool("unknown_coverage_gap", coverage_gap_); w.Bool("observer_failed", coverage_failed_);
      w.Raw(",\"epochs\":[");
      for (size_t i = 0; i < epoch_count_; ++i) {
        if (i) w.Raw(","); w.Raw("{\"epoch_id\":"); w.U(coverage_[i].epoch_id);
        w.UInt("cookie", coverage_[i].cookie); w.UInt("creation_qpc", coverage_[i].creation_qpc);
        w.Bool("creation_covered", coverage_[i].creation == Coverage::kCompleteThroughFreeze);
        w.Bool("operations_covered", coverage_[i].operations == Coverage::kCompleteThroughFreeze); w.Raw("}");
      }
      w.Raw("]},\"epochs\":[");
      for (size_t i = 0; i < epoch_count_; ++i) {
        if (i) w.Raw(","); const auto& row = epochs_[i];
        w.Raw("{\"epoch_id\":"); w.U(row.epoch_id); w.UInt("object_cookie", row.object_cookie);
        w.UInt("creation_qpc", row.creation_qpc); w.Key("destroy_qpc"); if (row.destroy_qpc) w.U(*row.destroy_qpc); else w.Raw("null");
        w.UInt("initial_count", row.initial_count); w.UInt("maximum_count", row.maximum_count);
        w.Key("origin_enum"); w.I(int(row.origin)); w.UInt("pending_operations", row.pending_operations);
        w.Bool("history_complete", row.history_complete); w.Raw("}");
      }
      w.Raw("],\"operations\":[");
      for (size_t i = 0; i < counts_.stored_operations; ++i) {
        if (i) w.Raw(","); const auto& row = operations_[i];
        w.Raw("{\"op_id\":"); w.U(row.op_id); w.UInt("epoch_id", row.epoch_id);
        w.UInt("entry_qpc", row.entry_qpc); w.UInt("exit_qpc", row.exit_qpc); w.UInt("native_thread_id", row.native_thread_id);
        w.Key("guest_handle"); if (row.guest_handle) w.U(*row.guest_handle); else w.Raw("null");
        w.Key("kind_enum"); w.I(int(row.kind)); w.Bool("completed", row.completed);
        w.UInt("release_count", row.release_count); w.Key("context_kind_enum"); w.I(int(row.context.kind));
        w.UInt("thread_epoch", row.context.generation.thread_epoch); w.UInt("serial", row.context.generation.serial);
        w.Bool("release_success", row.release_success); w.Key("previous_count"); w.Previous(row.previous_count);
        w.Key("timeout_kind_enum"); w.I(int(row.timeout_kind)); w.Key("wait_result_enum"); w.I(int(row.wait_result)); w.Raw("}");
      }
      w.Raw("]}"); return Finish(std::move(w));
    } catch (const std::bad_alloc&) { return {Error::kAllocation, {}}; }
  }

 private:
  Snapshot() = default;
  bool Fatal() const noexcept {
    constexpr uint32_t expected = uint32_t(core::Issue::kEarlyCut) | uint32_t(core::Issue::kIncompleteCut) |
        uint32_t(core::Issue::kUnsupported) | uint32_t(core::Issue::kNonfreshOrigin) |
        uint32_t(core::Issue::kInitialCredits) | uint32_t(core::Issue::kOperationOverflow);
    return (issues_ & ~expected) != 0 || diagnostics_.unknown_object_attempts || diagnostics_.rejected_attempts || diagnostics_.stopped_attempts;
  }
  bool Representable() const noexcept {
    uint64_t completed = 0;
    for (size_t i = 0; i < epoch_count_; ++i) {
      const auto& row = epochs_[i];
      if (!row.epoch_id || !row.object_cookie || row.creation_qpc >= freeze_ ||
          !row.maximum_count || row.maximum_count > core::kMaxCount || row.initial_count > row.maximum_count || core::Name(row.origin).empty() ||
          (row.destroy_qpc && (*row.destroy_qpc <= row.creation_qpc || *row.destroy_qpc > freeze_))) return false;
    }
    uint64_t last_entry = 0;
    for (size_t i = 0; i < counts_.stored_operations; ++i) {
      const auto& row = operations_[i];
      if (row.op_id != i + 1 || !row.epoch_id || row.epoch_id > epoch_count_ || !row.native_thread_id ||
          (row.guest_handle && !*row.guest_handle) || core::Name(row.kind).empty() ||
          row.entry_qpc > core::kMaxQpc || row.exit_qpc > core::kMaxQpc) return false;
      const auto& epoch = epochs_[row.epoch_id - 1];
      if (row.entry_qpc && (row.entry_qpc < window_.begin_qpc || row.entry_qpc > freeze_ ||
          row.entry_qpc <= epoch.creation_qpc || row.entry_qpc < last_entry ||
          (epoch.destroy_qpc && row.entry_qpc >= *epoch.destroy_qpc))) return false;
      if (row.entry_qpc) last_entry = row.entry_qpc;
      if (!row.completed && (row.exit_qpc || epoch.destroy_qpc)) return false;
      if (row.completed) {
        ++completed;
        if (row.exit_qpc > freeze_ || (row.exit_qpc && ((row.entry_qpc && row.exit_qpc < row.entry_qpc) ||
            (epoch.destroy_qpc && row.exit_qpc >= *epoch.destroy_qpc)))) return false;
      }
      if (row.kind == core::Kind::kRelease) {
        if (!row.release_count || row.release_count > core::kMaxCount || core::Name(row.context.kind).empty() ||
            (row.context.kind == core::ContextKind::kUpdate ? !row.context.generation.valid() : row.context.generation != frame_context::Id{})) return false;
        if (row.completed && row.release_success && (!row.previous_count || *row.previous_count < 0 ||
            uint32_t(*row.previous_count) > epoch.maximum_count || row.release_count > epoch.maximum_count - uint32_t(*row.previous_count))) return false;
        if ((!row.completed || !row.release_success) && row.previous_count) return false;
      } else if (row.kind == core::Kind::kWait && (core::Name(row.timeout_kind).empty() || core::Name(row.wait_result).empty())) return false;
    }
    return completed == counts_.completed_operations;
  }
  void Header(detail::Writer& w, std::string_view schema) const {
    w.Raw("{\"schema\":"); w.S(schema); w.Text("token", {token_.data(), 32});
    w.Text("clock", "win32-query-performance-counter"); w.UInt("qpc_frequency", window_.qpc_frequency);
  }
  static JsonResult Finish(detail::Writer&& w) {
    if (w.too_large) return {Error::kTooLarge, {}};
    return {Error::kNone, std::move(w.text)};
  }
  static void Epoch(detail::Writer& w, const core::Epoch& row, bool admission_covered) {
    w.Raw("{\"epoch_id\":"); w.U(row.epoch_id); w.UInt("object_cookie", row.object_cookie);
    w.UInt("creation_qpc", row.creation_qpc); w.Key("destroy_qpc"); if (row.destroy_qpc) w.U(*row.destroy_qpc); else w.Raw("null");
    w.UInt("initial_count", row.initial_count); w.UInt("maximum_count", row.maximum_count); w.Text("origin", core::Name(row.origin));
    // Different v2 meaning: independent admission coverage, NOT raw core flag.
    w.Bool("history_complete", admission_covered); w.Raw("}");
  }
  static void PrefixOperation(detail::Writer& w, const core::Operation& row) {
    w.Raw("{\"op_id\":"); w.U(row.op_id); w.UInt("epoch_id", row.epoch_id); w.UInt("native_thread_id", row.native_thread_id);
    w.Key("guest_handle"); if (row.guest_handle) w.U(*row.guest_handle); else w.Raw("null");
    w.Key("entry_qpc"); w.Clock(row.entry_qpc); w.Key("exit_qpc"); if (row.completed) w.Clock(row.exit_qpc); else w.Raw("null");
    w.Text("kind", core::Name(row.kind)); w.Bool("completed", row.completed); w.Raw(",\"payload\":{");
    if (row.kind == core::Kind::kRelease) {
      w.Raw("\"count\":"); w.U(row.release_count); w.Key("success"); if (row.completed) w.B(row.release_success); else w.Raw("null");
      w.Key("previous_count"); if (row.completed) w.Previous(row.previous_count); else w.Raw("null");
      w.Raw(",\"context\":{\"kind\":"); w.S(core::Name(row.context.kind)); w.Key("generation");
      if (row.context.kind == core::ContextKind::kUpdate) {
        w.Raw("{\"thread_epoch\":"); w.U(row.context.generation.thread_epoch); w.UInt("serial", row.context.generation.serial); w.Raw("}");
      } else w.Raw("null");
      w.Raw("}");
    } else if (row.kind == core::Kind::kWait) {
      w.Raw("\"result\":"); if (row.completed) w.S(core::Name(row.wait_result)); else w.Raw("null");
      w.Text("timeout_kind", core::Name(row.timeout_kind));
    }
    w.Raw("}}");
  }
  std::array<char, 32> token_{};
  core::Window window_{};
  uint64_t freeze_ = 0;
  core::Counts counts_{};
  core::Diagnostics diagnostics_{};
  uint32_t issues_ = 0, pending_ = 0;
  bool raw_history_ = false, raw_failed_ = false, coverage_gap_ = true, coverage_failed_ = false;
  bool raw_post_stop_mutated_ = false;
  Coverage inventory_ = Coverage::kUnknown;
  size_t epoch_count_ = 0;
  std::array<EpochCoverage, core::kEpochCapacity> coverage_{};
  std::array<core::Epoch, core::kEpochCapacity> epochs_{};
  std::array<core::Operation, core::kOperationCapacity> operations_{};
};
} // namespace mcla::semaphore_prefix_snapshot
