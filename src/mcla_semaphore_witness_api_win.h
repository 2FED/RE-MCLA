#pragma once
// Private in-process metadata transport only. Future SDK integration owns ONE
// Registry implementation; do not instantiate independent SDK/host singletons.
// Register before guest work. Registry, callback code and its context must
// remain alive for the process lifetime (no reconstruct/reset/unload). No
// replacement/unregister, allocation, observer
// mutex, clocks, game context, semaphore operation or journal lives here.
#if !defined(_WIN32)
#error This private transport is Windows-only.
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#include <atomic>
#include <cstdint>
#include <limits>
#include <optional>

namespace rex::diagnostics::semaphore_witness {
static_assert(std::atomic<uint64_t>::is_always_lock_free && std::atomic<unsigned>::is_always_lock_free &&
              std::atomic<bool>::is_always_lock_free, "No observer-mutex atomic fallback permitted.");
#if defined(REXGLUE_SEMAPHORE_WITNESS) && REXGLUE_SEMAPHORE_WITNESS
inline constexpr bool kCompiledEnabled = true;
#else
inline constexpr bool kCompiledEnabled = false;
#endif
inline constexpr bool kNativeAdmitted = false, kSimulationProven = false,
    kRenderProven = false, kFpsProven = false, kFrameToStepMappingProven = false;
inline constexpr uint32_t kMaxCount = uint32_t(std::numeric_limits<int32_t>::max());
enum class Origin { kFresh, kNative, kRestored, kUnknown };
enum class Kind { kRelease, kWait, kSave, kRestore, kSignalAndWait, kWaitMultiple };
enum class TimeoutClass { kNotApplicable, kZero, kFinite, kInfinite };
enum class Result { kSuccess, kTimeout, kApc, kAlerted, kAbandoned, kFailed, kUnknown, kUnsupported };
struct Creation { uint64_t cookie{}; uint32_t initial_count{}, maximum_count{}; Origin origin = Origin::kUnknown; };
struct Admission {
  uint64_t cookie{};
  Kind kind = Kind::kWait;
  uint32_t count{};
  TimeoutClass timeout = TimeoutClass::kNotApplicable;
};
struct Completion {
  uint64_t ticket{};
  Kind kind = Kind::kWait; // Caller claim; host validates its exact issued ticket.
  Result result = Result::kUnknown;
  std::optional<int32_t> previous_count;
};
struct CallbackTable {
  void* context = nullptr; // Opaque retained callback state, NOT game context.
  void (*create)(void*, const Creation&) = nullptr;
  void (*destroy)(void*, uint64_t cookie) = nullptr;
  uint64_t (*begin)(void*, const Admission&) = nullptr;
  void (*end)(void*, const Completion&) = nullptr;
};

namespace detail {
class LastErrorGuard {
 public:
  LastErrorGuard() noexcept : saved_(GetLastError()) {}
  ~LastErrorGuard() noexcept { SetLastError(saved_); }
  LastErrorGuard(const LastErrorGuard&) = delete;
  LastErrorGuard& operator=(const LastErrorGuard&) = delete;
 private:
  DWORD saved_;
};
inline bool Valid(Origin value) noexcept {
  switch (value) { case Origin::kFresh: case Origin::kNative: case Origin::kRestored: case Origin::kUnknown: return true; }
  return false;
}
inline bool Valid(Kind value) noexcept {
  switch (value) { case Kind::kRelease: case Kind::kWait: case Kind::kSave: case Kind::kRestore:
    case Kind::kSignalAndWait: case Kind::kWaitMultiple: return true; }
  return false;
}
inline bool Valid(const Creation& value) noexcept {
  return value.cookie && value.maximum_count && value.maximum_count <= kMaxCount &&
         value.initial_count <= value.maximum_count && Valid(value.origin);
}
inline bool Valid(const Admission& value) noexcept {
  if (!value.cookie || !Valid(value.kind)) return false;
  if (value.kind == Kind::kRelease)
    return value.count && value.count <= kMaxCount && value.timeout == TimeoutClass::kNotApplicable;
  if (value.kind == Kind::kWait)
    return !value.count && (value.timeout == TimeoutClass::kZero || value.timeout == TimeoutClass::kFinite ||
                            value.timeout == TimeoutClass::kInfinite);
  return !value.count && value.timeout == TimeoutClass::kNotApplicable;
}
inline bool Valid(const Completion& value) noexcept {
  if (!value.ticket || !Valid(value.kind)) return false;
  if (value.kind == Kind::kRelease) {
    if (value.result == Result::kSuccess) return value.previous_count && *value.previous_count >= 0;
    return value.result == Result::kFailed && !value.previous_count;
  }
  if (value.previous_count) return false; // Never inspect undefined native output.
  if (value.kind != Kind::kWait) return value.result == Result::kUnsupported;
  switch (value.result) { case Result::kSuccess: case Result::kTimeout: case Result::kApc: case Result::kAlerted:
    case Result::kAbandoned: case Result::kFailed: case Result::kUnknown: return true;
    case Result::kUnsupported: return false; }
  return false;
}
} // namespace detail

class Registry {
 public:
  Registry() noexcept = default;
  Registry(const Registry&) = delete;
  Registry& operator=(const Registry&) = delete;
  Registry(Registry&&) = delete;
  Registry& operator=(Registry&&) = delete;

  // Copies table values once. No callback is called during registration.
  // A racing/repeated/invalid registration fails closed, even if another wins.
  bool Register(const CallbackTable& table) noexcept {
    detail::LastErrorGuard restore;
    if constexpr (!kCompiledEnabled) return false;
    if (!table.create || !table.destroy || !table.begin || !table.end) { Fail(); return false; }
    unsigned expected = 0;
    if (!state_.compare_exchange_strong(expected, 1, std::memory_order_acq_rel)) { Fail(); return false; }
    table_ = table;
    state_.store(2, std::memory_order_release);
    return true;
  }
  // bool means only this metadata callback returned, never native success.
  bool Create(const Creation& value) noexcept {
    detail::LastErrorGuard restore;
    if (!Admitting()) return false;
    if (!detail::Valid(value)) { Fail(); return false; }
    try { table_.create(table_.context, value); return true; } catch (...) { Fail(); return false; }
  }
  bool Destroy(uint64_t cookie) noexcept {
    detail::LastErrorGuard restore;
    if (!Admitting()) return false;
    if (!cookie) { Fail(); return false; }
    try { table_.destroy(table_.context, cookie); return true; } catch (...) { Fail(); return false; }
  }
  uint64_t Begin(const Admission& value) noexcept {
    detail::LastErrorGuard restore;
    if (!Admitting()) return 0;
    if (!detail::Valid(value)) { Fail(); return 0; }
    try { return table_.begin(table_.context, value); } catch (...) { Fail(); return 0; }
  }
  bool End(const Completion& value) noexcept {
    detail::LastErrorGuard restore;
    if constexpr (!kCompiledEnabled) return false;
    // In-flight nonzero tickets always use the same immutable table, even
    // after Disable or another diagnostic failure. Host validates identity.
    if (state_.load(std::memory_order_acquire) != 2) return false;
    if (!detail::Valid(value)) { Fail(); return false; }
    try { table_.end(table_.context, value); return true; } catch (...) { Fail(); return false; }
  }
  // Not a drain/quiescence barrier: an already admitted callback may complete.
  // Suppresses new creation/destruction/begins. Host must serialize the actual
  // snapshot under its own lock BEFORE ordinary title shutdown.
  void Disable() noexcept {
    detail::LastErrorGuard restore;
    if constexpr (kCompiledEnabled) admissions_.store(false, std::memory_order_release);
  }
  bool registered() const noexcept { return kCompiledEnabled && state_.load(std::memory_order_acquire) == 2; }
  bool admitting() const noexcept { return Admitting(); }
  bool failed() const noexcept { return kCompiledEnabled && failures_.load(std::memory_order_relaxed) != 0; }
  uint64_t failure_count() const noexcept { return kCompiledEnabled ? failures_.load(std::memory_order_relaxed) : 0; }

 private:
  bool Admitting() const noexcept {
    return kCompiledEnabled && state_.load(std::memory_order_acquire) == 2 &&
           admissions_.load(std::memory_order_acquire);
  }
  void Fail() noexcept {
    admissions_.store(false, std::memory_order_release);
    auto count = failures_.load(std::memory_order_relaxed);
    while (count != UINT64_MAX && !failures_.compare_exchange_strong(count, count + 1, std::memory_order_relaxed)) {}
  }
  std::atomic<unsigned> state_{0}; // 0 absent, 1 publishing, 2 immutable ready.
  std::atomic<bool> admissions_{true}; // Registration is explicit runtime opt-in.
  std::atomic<uint64_t> failures_{0};
  CallbackTable table_{};
};
} // namespace rex::diagnostics::semaphore_witness
