#pragma once
// Offline-tested scope bookkeeping only. Native wrappers must independently
// establish callers, per-thread lifetimes, object identity and QPC brackets.
// IDs are not simulation steps; this header cannot admit a game or FPS claim.
#include <array>
#include <cstdint>
#include <limits>

namespace mcla::frame_context {
struct Id {
  uint64_t thread_epoch = 0;
  uint64_t serial = 0;
  constexpr bool valid() const noexcept { return thread_epoch && serial; }
  bool operator==(const Id&) const = default;
};
struct Lane {
  uintptr_t context = 0;
  uintptr_t base = 0;
  bool operator==(const Lane&) const = default;
};
enum class Kind { kUpdate, kCoordinator, kSyncRender, kWorkerRender };
struct Context {
  Id invocation;
  Id update;
  uint64_t wait_operation = 0;
  uint64_t coordinator_epoch = 0;
  Kind kind = Kind::kUpdate;
  bool operator==(const Context&) const = default;
};
struct Lease {
  Id id;
  uint64_t begin = 0;
  uint32_t depth = 0;
  bool valid() const noexcept { return id.valid(); }
};

// One instance per independently identified native thread lifetime. The caller
// owns serialization on that thread; no global "latest update" is consulted.
// A protocol violation poisons this instance permanently, never game execution.
class Tracker {
 public:
  explicit Tracker(uint64_t thread_epoch, Lane lane, uint64_t initial_serial = 0) noexcept
      : epoch_(thread_epoch), lane_(lane), next_(initial_serial) {
    failed_ = !epoch_ || !lane_.context || !lane_.base;
  }
  Tracker(const Tracker&) = delete;
  Tracker& operator=(const Tracker&) = delete;
  Tracker(Tracker&&) = delete;
  Tracker& operator=(Tracker&&) = delete;
  bool failed() const noexcept { return failed_; }
  Context current() const noexcept {
    return !failed_ && depth_ ? stack_[depth_ - 1].context : Context{};
  }
  Id pending_update() const noexcept { return failed_ ? Id{} : update_; }

  // Called only after a separately witnessed successful worker wait. Context
  // and object proof do not originate here. Each credit is consumed at most
  // once by the next expected worker-render entry on this exact lane. Journal
  // operation IDs must increase on the worker; ambiguous/reordered completions
  // invalidate the observer instead of recycling an earlier credit.
  bool worker_credit(Lane lane, uint64_t operation, uint64_t coordinator_epoch,
                     bool expected_success) noexcept {
    if (!check_lane(lane) || depth_ || update_.valid() || !expected_success || operation <= last_wait_ ||
        !coordinator_epoch || wait_) return poison();
    wait_ = operation;
    last_wait_ = operation;
    wait_coordinator_ = coordinator_epoch;
    return true;
  }

  Lease enter(Lane lane, Kind kind, bool expected_caller, uint64_t qpc,
              uint64_t coordinator_epoch = 0) noexcept {
    if (!check_lane(lane) || !expected_caller || !qpc || qpc > kMaxQpc ||
        qpc < last_qpc_ || depth_ == stack_.size() || next_ == UINT64_MAX)
      return reject();
    Context context;
    context.kind = kind;
    switch (kind) {
      case Kind::kUpdate:
        if (depth_ || update_.valid() || wait_ || coordinator_epoch) return reject();
        break;
      case Kind::kCoordinator:
        if (depth_ || !update_.valid() || wait_ || !coordinator_epoch) return reject();
        context.update = update_;
        context.coordinator_epoch = coordinator_epoch;
        update_ = {};
        break;
      case Kind::kSyncRender:
        if (depth_ != 1 || stack_[0].context.kind != Kind::kCoordinator ||
            !coordinator_epoch || coordinator_epoch != stack_[0].context.coordinator_epoch)
          return reject();
        context.update = stack_[0].context.update;
        context.coordinator_epoch = coordinator_epoch;
        break;
      case Kind::kWorkerRender:
        if (depth_ || update_.valid() || !wait_ || !coordinator_epoch ||
            coordinator_epoch != wait_coordinator_) return reject();
        context.wait_operation = wait_;
        context.coordinator_epoch = wait_coordinator_;
        wait_ = wait_coordinator_ = 0;
        break;
      default: return reject();
    }
    context.invocation = {epoch_, ++next_};
    stack_[depth_++] = {context, qpc};
    last_qpc_ = qpc;
    return {context.invocation, qpc, depth_};
  }

  bool leave(Lane lane, Lease lease, uint64_t qpc, bool completed) noexcept {
    if (!check_lane(lane) || !lease.valid() || !depth_ || lease.depth != depth_ ||
        lease.id != stack_[depth_ - 1].context.invocation ||
        lease.begin != stack_[depth_ - 1].begin || qpc < last_qpc_ || qpc > kMaxQpc)
      return poison();
    const Context context = stack_[depth_ - 1].context;
    stack_[--depth_] = {};
    last_qpc_ = qpc;
    if (!completed) return poison(); // Original unwind is not a completed update.
    if (context.kind == Kind::kUpdate) update_ = context.invocation;
    return true;
  }

 private:
  static constexpr uint64_t kMaxQpc = uint64_t(std::numeric_limits<int64_t>::max());
  struct Entry { Context context; uint64_t begin = 0; };
  bool poison() noexcept {
    failed_ = true;
    update_ = {};
    wait_ = wait_coordinator_ = 0;
    return false;
  }
  Lease reject() noexcept { poison(); return {}; }
  bool check_lane(Lane lane) noexcept {
    if (failed_ || lane != lane_) return poison();
    return true;
  }
  uint64_t epoch_ = 0;
  Lane lane_;
  uint64_t next_ = 0;
  uint64_t last_qpc_ = 0;
  Id update_;
  uint64_t wait_ = 0, wait_coordinator_ = 0;
  uint64_t last_wait_ = 0;
  std::array<Entry, 2> stack_{};
  uint32_t depth_ = 0;
  bool failed_ = false;
};
}  // namespace mcla::frame_context
