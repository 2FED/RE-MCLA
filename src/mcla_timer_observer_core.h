#pragma once

#include "mcla_timer_journal.h"

#include <algorithm>
#include <atomic>
#include <exception>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <utility>

// Injected, native-independent observer seam. No SDK hook, game-memory read or
// real clock exists here. Callback implementations and runtime hook/context
// preservation require separate review. An observed call is not an admitted
// simulation step. Lifecycle owners must keep this object/callbacks alive until
// freeze drains admitted calls AND the actual runtime hook is safely detached.
namespace mcla::timer_observer {
namespace journal = mcla::timer_journal;

template<class Context, class Memory>
struct Operations {
  // Both callbacks must be bounded and synchronous; failures are diagnostic.
  std::function<uint64_t()> read_qpc;
  // Must be observational, synchronous and not retain these references. A
  // const pointer-reference does not make its pointee immutable; actual reader
  // game-memory coherence and observational behavior are NOT established here.
  std::function<bool(const Context&, const Memory&, journal::State&)> read_state;
};

enum CallbackIssue : uint32_t {
  kNoCallbackIssue = 0,
  kEntryClockCallbackFailed = 1u << 0,
  kExitClockCallbackFailed = 1u << 1,
  kBeforeStateCallbackFailed = 1u << 2,
  kAfterStateCallbackFailed = 1u << 3,
  kOriginalUnwound = 1u << 4,
};
struct RawRecord {
  journal::Observation observation;
  uint64_t entry_ordinal = 0;  // Observer entry order, not clock fitting.
  uint32_t callback_issues = kNoCallbackIssue;
};
struct Counts {
  uint64_t observed_entries = 0;  // Only while enabled and not freezing.
  uint64_t admitted_entries = 0;
  uint64_t stored_entries = 0;
  uint64_t completed_entries = 0;
  uint64_t overflow_entries = 0;  // Original still runs, without instrumentation.
  uint64_t outside_before_entries = 0;
  uint64_t outside_after_entries = 0;
  bool counter_saturated = false;
  bool observer_failed = false;
};
inline constexpr uint32_t kObserverFailure = 1u << 30;
// Kept separate from journal's clock/window flags: a perfectly valid outer
// QPC interval must not hide a callback failure or original invocation unwind.
inline constexpr uint32_t kRecordedInvocationDiagnostic = 1u << 29;

template<size_t Capacity = 1024>
class Observer {
  static_assert(Capacity > 0 && Capacity <= 4096);
 public:
  struct OfflineReport {
    std::unique_ptr<journal::Journal<Capacity>> retained_calls;
    Counts counts;
    uint32_t raw_callback_issues = kNoCallbackIssue;
    OfflineReport(journal::Window window, Counts observed)
        : retained_calls(std::make_unique<journal::Journal<Capacity>>(window)), counts(observed) {}
    uint32_t issues() const noexcept {
      return retained_calls->issues() |
             (counts.overflow_entries ? uint32_t(journal::kStorageOverflow) : 0u) |
             (counts.counter_saturated ? uint32_t(journal::kCounterSaturated) : 0u) |
             (counts.observer_failed ? kObserverFailure : 0u) |
             (raw_callback_issues ? kRecordedInvocationDiagnostic : 0u);
    }
  };
  class Snapshot {
    friend class Observer;
   public:
    Snapshot() = default;  // Allocate once on the caller's heap before use.
    journal::Window window() const noexcept { return window_; }
    Counts counts() const noexcept { return counts_; }
    std::span<const RawRecord> records() const noexcept { return {records_.data(), size_}; }
    // Explicit post-drain/offline O(n^2) classification. It is never performed
    // during original work, inside the observer mutex or by try_freeze_into().
    std::unique_ptr<OfflineReport> classify_offline() const {
      // Heap allocation is only offline, after drain. Allocation failure
      // propagates to the diagnostic caller, never through an original call.
      auto output = std::make_unique<OfflineReport>(window_, counts_);
      // Preserve raw completion order in records(); classify by explicitly
      // recorded entry ordinal, NOT by a fitted/reordered QPC timestamp.
      std::array<size_t, Capacity> order{};
      for (size_t i = 0; i < size_; ++i) order[i] = i;
      std::sort(order.begin(), order.begin() + size_, [&](size_t a, size_t b) {
        return records_[a].entry_ordinal < records_[b].entry_ordinal;
      });
      for (size_t i = 0; i < size_; ++i) {
        const auto& row = records_[order[i]];
        output->raw_callback_issues |= row.callback_issues;
        output->retained_calls->observe(row.observation);
      }
      return output;
    }
   private:
    journal::Window window_{};
    std::array<RawRecord, Capacity> records_{};
    size_t size_ = 0;
    Counts counts_{};
  };

  explicit Observer(journal::Window window, bool enabled = false) noexcept
      : window_(window), accepting_(enabled) {}
  Observer(const Observer&) = delete;
  Observer& operator=(const Observer&) = delete;

  // Always invoke original exactly once with the SAME reference objects. All
  // instrumentation runs outside the mutex and only its exceptions are caught.
  // Original exceptions/return type propagate; the RAII completion guard still
  // publishes exit observations and releases the admitted in-flight reference.
  template<class Context, class Memory, class Original>
  decltype(auto) invoke(journal::Identity identity, Context& context, Memory& memory,
                        const Operations<Context, Memory>& operations,
                        uint8_t incoming_r5_low_byte, uint64_t incoming_f1_bits,
                        Original&& original) {
    std::optional<uint64_t> slot;
    try { slot = reserve(); }
    catch (...) { observer_failed_.store(true, std::memory_order_release); }
    if (!slot) return std::invoke(std::forward<Original>(original), context, memory);
    Completion<Context, Memory> completion(*this, *slot, identity, context, memory,
                                            operations, incoming_r5_low_byte, incoming_f1_bits);
    completion.capture_entry();
    return std::invoke(std::forward<Original>(original), context, memory);
  }

  // Nonblocking poll: irrevocably disarm first, then try the short publication
  // mutex. No wait/timeout loop and no guest-work lock. Caller owns a deadline,
  // heap-allocated destination and lifetime; false leaves destination unchanged.
  void stop_admissions() noexcept { accepting_.store(false, std::memory_order_release); }
  bool try_freeze_into(Snapshot& destination) noexcept {
    stop_admissions();
    try {
      std::unique_lock lock(mutex_, std::try_to_lock);
      if (!lock.owns_lock() || in_flight_.load(std::memory_order_acquire) != 0) return false;
      destination.window_ = window_;
      destination.records_ = records_;
      destination.size_ = size_;
      destination.counts_ = counts_;
      destination.counts_.observer_failed = observer_failed_.load(std::memory_order_acquire);
      return true;
    } catch (...) {
      observer_failed_.store(true, std::memory_order_release);
      return false;
    }
  }

 private:
  static void increment(uint64_t& count, bool& saturated) noexcept {
    if (count == std::numeric_limits<uint64_t>::max()) saturated = true;
    else ++count;
  }
  std::optional<uint64_t> reserve() {
    if (!accepting_.load(std::memory_order_acquire)) return std::nullopt;
    std::lock_guard lock(mutex_);
    if (!accepting_.load(std::memory_order_acquire)) return std::nullopt;
    increment(counts_.observed_entries, counts_.counter_saturated);
    if (size_ + in_flight_.load(std::memory_order_relaxed) >= Capacity) {
      increment(counts_.overflow_entries, counts_.counter_saturated);
      return std::nullopt;
    }
    in_flight_.fetch_add(1, std::memory_order_relaxed);  // <= Capacity.
    increment(counts_.admitted_entries, counts_.counter_saturated);
    return counts_.observed_entries;
  }
  void complete(const RawRecord& row) noexcept {
    // The original has returned/unwound before this short publication lock.
    // A publication-lock failure is diagnostic only: leave explicit failed/
    // incomplete counts and ALWAYS release the atomic in-flight reference.
    try {
      std::lock_guard lock(mutex_);
      const auto& call = row.observation;
      const bool discardable = window_.valid() && call.entry_qpc &&
        call.entry_qpc <= call.exit_qpc && call.exit_qpc <= journal::kMaxQpc &&
        call.identity.valid() && call.before_available && call.after_available &&
        row.callback_issues == kNoCallbackIssue;
      if (discardable && call.exit_qpc < window_.begin_qpc) {
        increment(counts_.outside_before_entries, counts_.counter_saturated);
      } else if (discardable && call.entry_qpc >= window_.end_qpc) {
        increment(counts_.outside_after_entries, counts_.counter_saturated);
      } else {
      // Capacity was reserved before instrumentation/original work. Completion
      // can discard a fully outside valid call, freeing that reservation. Raw
      // invalid calls/straddles/unwinds remain, without guessing missing state.
        records_[size_++] = row;
        increment(counts_.stored_entries, counts_.counter_saturated);
      }
      increment(counts_.completed_entries, counts_.counter_saturated);
      // Publish/decrement atomically with respect to reservation's short lock;
      // otherwise retained+in_flight could transiently double-count a call.
      in_flight_.fetch_sub(1, std::memory_order_release);
    } catch (...) {
      observer_failed_.store(true, std::memory_order_release);
      in_flight_.fetch_sub(1, std::memory_order_release);
    }
  }

  template<class Context, class Memory>
  class Completion {
   public:
    Completion(Observer& owner, uint64_t ordinal, journal::Identity identity,
               Context& context, Memory& memory, const Operations<Context, Memory>& operations,
               uint8_t r5, uint64_t f1) noexcept
        : owner_(owner), context_(context), memory_(memory), operations_(operations),
          initial_uncaught_(std::uncaught_exceptions()) {
      row_.entry_ordinal = ordinal;
      row_.observation.identity = identity;
      row_.observation.incoming_r5_low_byte = r5;
      row_.observation.incoming_f1_bits = f1;
    }
    Completion(const Completion&) = delete;
    Completion& operator=(const Completion&) = delete;
    void capture_entry() noexcept {
      row_.observation.entry_qpc = clock(kEntryClockCallbackFailed);
      state(row_.observation.before, row_.observation.before_available, kBeforeStateCallbackFailed);
    }
    ~Completion() noexcept {
      if (std::uncaught_exceptions() > initial_uncaught_) row_.callback_issues |= kOriginalUnwound;
      state(row_.observation.after, row_.observation.after_available, kAfterStateCallbackFailed);
      // Conservative OUTER witness: entry QPC precedes before-state reading;
      // exit QPC follows after-state reading. Includes both snapshots, original
      // work and instrumentation/preemption; not exact original-only cost.
      row_.observation.exit_qpc = clock(kExitClockCallbackFailed);
      owner_.complete(row_);
    }
   private:
    uint64_t clock(CallbackIssue failed) noexcept {
      try { return operations_.read_qpc(); }
      catch (...) { row_.callback_issues |= failed; return 0; }
    }
    void state(journal::State& destination, bool& available, CallbackIssue failed) noexcept {
      // A callback that writes and then throws/returns false must not retain a
      // partially populated state. Availability remains an unverified claim.
      journal::State temporary;
      try {
        available = operations_.read_state(context_, memory_, temporary);
        if (available) destination = temporary;
      } catch (...) {
        row_.callback_issues |= failed;
        available = false;
      }
    }
    Observer& owner_;
    Context& context_;
    Memory& memory_;
    const Operations<Context, Memory>& operations_;
    int initial_uncaught_;
    RawRecord row_{};
  };

  const journal::Window window_;
  std::atomic<bool> accepting_{false};
  std::mutex mutex_;
  std::array<RawRecord, Capacity> records_{};
  size_t size_ = 0;
  std::atomic<size_t> in_flight_{0};
  std::atomic<bool> observer_failed_{false};
  Counts counts_;
};
}  // namespace mcla::timer_observer
