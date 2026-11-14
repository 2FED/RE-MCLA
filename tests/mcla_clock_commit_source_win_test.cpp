// Exact private SOURCE-seam prerequisite; primary owns include generation,
// compilation and execution. No SDK DLL is loaded. Clock samples, config,
// caller bindings and scheduling are SYNTHETIC. Source snippets retain their
// original math; a test-only std token redirect selects the SAME mutex wrapper
// (backed by real std::mutex) for baseline and instrumented source bodies.
#if !defined(_WIN32)
#error This prerequisite tests real Win32 native state and is Windows-only.
#endif
#if !defined(MCLA_CLOCK_COMMIT_SOURCE_SHA256)
#error The checked recipe must supply the exact original SDK source pin.
#endif
#if !defined(WIN32_LEAN_AND_MEAN)
#define WIN32_LEAN_AND_MEAN
#endif
#if !defined(NOMINMAX)
#define NOMINMAX
#endif
#include <Windows.h>
#include <xmmintrin.h>
#include "../src/mcla_clock_commit_history_sdk_bridge.h"
#include <atomic>
#include <cstring>
#include <iostream>
#include <latch>
#include <memory>
#include <mutex>
#include <thread>
#include <numeric>
#include <stdexcept>
#include <string_view>
#include <utility>

static_assert(std::string_view(MCLA_CLOCK_COMMIT_SOURCE_SHA256) ==
    "D72A7093E0029F85FFB6859B05DE440416EB2F66157E2C5BC994B538A29F7DE4");

namespace fixture {
namespace h = mcla::clock_commit_history;
namespace b = mcla::clock_read_witness::bridge;
std::atomic<unsigned> checks{0}, failures{0};
void Check(bool value, const char* label) {
  checks.fetch_add(1, std::memory_order_relaxed);
  if (!value) { failures.fetch_add(1, std::memory_order_relaxed); std::cerr << "FAIL " << label << '\n'; }
}
constexpr h::RunToken run{0x1122334455667788ull, 0x99AABBCCDDEEFF01ull};
struct Native {
  uint32_t error{}, mxcsr{};
  static Native Capture() noexcept {
    const uint32_t error = ::GetLastError();
    return {error, uint32_t(_mm_getcsr())};
  }
  void Restore() const noexcept { ::SetLastError(error); _mm_setcsr(mxcsr); }
  bool operator==(const Native&) const = default;
};
struct PreserveNative {
  Native value = Native::Capture();
  ~PreserveNative() noexcept { value.Restore(); }
};
inline constexpr Native incoming{0x13572468u, 0x3F80u};
inline constexpr Native produced{0xABCD1000u, 0x5FA0u};
struct SamplePlan {
  uint64_t sample = 101;
  bool throw_before_lock{}, nested{};
  uint64_t nested_sample = 102, calls{}, nested_calls{}, nested_return{};
  std::latch* sampled = nullptr;
  std::latch* resume = nullptr;
  std::latch* try_failed = nullptr;
  Native sample_incoming{}, output = produced;
};
inline thread_local SamplePlan* plan = nullptr;
struct PlanScope {
  SamplePlan* old;
  explicit PlanScope(SamplePlan& value) noexcept : old(std::exchange(plan, &value)) {}
  ~PlanScope() noexcept { plan = old; }
};

class OwnedMutex {
 public:
  void lock() { native_.lock(); owned_ = this; }
  bool try_lock() {
    const bool result = native_.try_lock();
    if (result) owned_ = this;
    else if (plan && plan->try_failed) {
      // This notification is test scheduling, not production clock observation.
      const PreserveNative preserve;
      plan->try_failed->count_down();
    }
    return result;
  }
  void unlock() noexcept { owned_ = nullptr; native_.unlock(); }
  bool OwnedHere() const noexcept { return owned_ == this; }
 private:
  std::mutex native_;
  inline static thread_local const OwnedMutex* owned_ = nullptr;
};
using Core = h::Recorder<64>;
struct Proxy {
  Core core;
  OwnedMutex* mutex = nullptr;
  std::atomic<uint64_t> begin_without_mutex{0}, unordered_with_mutex{0}, publish_with_mutex{0}, abort_with_mutex{0};
  std::latch* publish_waiting = nullptr;
  std::latch* allow_publish = nullptr;
  class Lease {
   public:
    Lease() noexcept = default;
    Lease(Proxy& owner, Core::Lease value) noexcept : owner_(&owner), lease_(std::move(value)) {}
    Lease(const Lease&) = delete;
    Lease& operator=(const Lease&) = delete;
    Lease(Lease&& other) noexcept : owner_(std::exchange(other.owner_, nullptr)), lease_(std::move(other.lease_)) {}
    Lease& operator=(Lease&& other) noexcept {
      if (this != &other) { Abort(); owner_ = std::exchange(other.owner_, nullptr); lease_ = std::move(other.lease_); }
      return *this;
    }
    ~Lease() noexcept { Abort(); }
    bool Publish(const h::Metadata& value) noexcept {
      if (!lease_.accepted()) return false;
      if (owner_->mutex->OwnedHere()) owner_->publish_with_mutex.fetch_add(1, std::memory_order_relaxed);
      if (owner_->publish_waiting) {
        const PreserveNative preserve;
        owner_->publish_waiting->count_down(); owner_->allow_publish->wait();
      }
      const bool result = lease_.Publish(value); owner_ = nullptr; return result;
    }
    void Abort() noexcept {
      if (lease_.accepted()) {
        if (owner_->mutex->OwnedHere()) owner_->abort_with_mutex.fetch_add(1, std::memory_order_relaxed);
        lease_.Abort();
      }
      owner_ = nullptr;
    }
   private:
    Proxy* owner_ = nullptr;
    Core::Lease lease_;
  };
  Lease BeginLocked(h::Kind kind, h::Binding binding = {}) noexcept {
    if (!mutex->OwnedHere()) begin_without_mutex.fetch_add(1, std::memory_order_relaxed);
    return Lease(*this, core.BeginLocked(kind, binding));
  }
  Lease BeginUnordered(h::Binding binding = {}) noexcept {
    if (mutex->OwnedHere()) unordered_with_mutex.fetch_add(1, std::memory_order_relaxed);
    return Lease(*this, core.BeginUnordered(binding));
  }
};
inline thread_local Proxy* proxy = nullptr;
struct ProxyScope {
  Proxy* old;
  explicit ProxyScope(Proxy& value) noexcept : old(std::exchange(proxy, &value)) {}
  ~ProxyScope() noexcept { proxy = old; }
};
class CommitCall final : public h::sdk::BasicCall<Proxy> {
 public:
  explicit CommitCall(bool query) noexcept : BasicCall(*proxy, query) {}
};
__declspec(noinline) uint64_t Sample(uint64_t (*nested_query)()) {
  auto& value = *plan;
  value.sample_incoming = Native::Capture(); ++value.calls;
  if (value.nested) {
    SamplePlan nested; nested.sample = value.nested_sample;
    { PlanScope nested_scope(nested); value.nested_return = nested_query(); }
    value.nested_calls = nested.calls;
  }
  if (value.sampled) value.sampled->count_down();
  if (value.resume) value.resume->wait();
  value.output.Restore();
  if (value.throw_before_lock) throw std::runtime_error("fixture original sample throw before clock lock");
  return value.sample;
}
void Reduce(std::pair<uint64_t, uint64_t>& value) {
  const uint64_t gcd = std::gcd(value.first, value.second);
  value.first /= gcd; value.second /= gcd;
}
} // namespace fixture

// Only the five checked private function snippets are expanded while `std` is
// redirected. These aliases retain the original unique_lock/lock_guard control
// flow; they merely expose ownership of the existing test clock mutex.
namespace clock_fixture_std {
using mutex = fixture::OwnedMutex;
template<class T> using lock_guard = std::lock_guard<T>;
template<class T> using unique_lock = std::unique_lock<T>;
using std::defer_lock;
using std::make_pair;
}

#define FIXTURE_CLOCK_STATE \
  double guest_time_scalar_ = 1.0; \
  uint64_t guest_tick_frequency_ = 50; \
  std::pair<uint64_t, uint64_t> guest_tick_ratio_{5, 1}; \
  uint64_t last_host_tick_count_ = 100, last_guest_tick_count_ = 500; \
  bool clock_no_scaling = false; \
  fixture::OwnedMutex tick_mutex_; \
  uint64_t UpdateGuestClock(); \
  struct Clock { \
    static uint64_t QueryHostTickCount() { return fixture::Sample(&UpdateGuestClock); } \
    static uint64_t QueryHostTickFrequency() { return 10; } \
  }; \
  void reduce_fraction(std::pair<uint64_t, uint64_t>& value) { fixture::Reduce(value); }

namespace baseline {
FIXTURE_CLOCK_STATE
#define REXCVAR_GET(value) (value)
#define std clock_fixture_std
#include "mcla-clock-commit-original-query.inc"
#include "mcla-clock-commit-original-ratio.inc"
#undef std
#undef REXCVAR_GET
}
namespace recorded {
FIXTURE_CLOCK_STATE
using MclaClockWitnessCall = fixture::b::Witness;
using MclaClockCommitCall = fixture::CommitCall;
#define REXCVAR_GET(value) (value)
#define std clock_fixture_std
#if defined(MCLA_CLOCK_COMMIT_EARLY_MUTEX_MUTANT)
#include "mcla-clock-commit-early-query.inc"
#else
#include "mcla-clock-commit-recorded-query.inc"
#endif
#include "mcla-clock-commit-recorded-ratio.inc"
#undef std
#undef REXCVAR_GET
}
#undef FIXTURE_CLOCK_STATE

namespace fixture {
struct State {
  uint64_t host, guest, numerator, denominator, frequency, scalar_bits;
  bool no_scaling;
  bool operator==(const State&) const = default;
};
State Capture(bool is_recorded) {
  double scalar = is_recorded ? recorded::guest_time_scalar_ : baseline::guest_time_scalar_;
  uint64_t bits; std::memcpy(&bits, &scalar, sizeof(bits));
  if (is_recorded) return {recorded::last_host_tick_count_, recorded::last_guest_tick_count_,
      recorded::guest_tick_ratio_.first, recorded::guest_tick_ratio_.second,
      recorded::guest_tick_frequency_, bits, recorded::clock_no_scaling};
  return {baseline::last_host_tick_count_, baseline::last_guest_tick_count_,
      baseline::guest_tick_ratio_.first, baseline::guest_tick_ratio_.second,
      baseline::guest_tick_frequency_, bits, baseline::clock_no_scaling};
}
void Reset() {
  baseline::guest_time_scalar_ = recorded::guest_time_scalar_ = 1.0;
  baseline::guest_tick_frequency_ = recorded::guest_tick_frequency_ = 50;
  baseline::guest_tick_ratio_ = recorded::guest_tick_ratio_ = {5, 1};
  baseline::last_host_tick_count_ = recorded::last_host_tick_count_ = 100;
  baseline::last_guest_tick_count_ = recorded::last_guest_tick_count_ = 500;
  baseline::clock_no_scaling = recorded::clock_no_scaling = false;
}
struct Observation {
  uint64_t result{};
  bool returned{}, threw{};
  Native native{};
  State state{};
};
__declspec(noinline) void Observe(bool is_recorded, Observation& out) {
  incoming.Restore();
  try {
    out.result = is_recorded ? recorded::UpdateGuestClock() : baseline::UpdateGuestClock();
    out.native = Native::Capture(); out.returned = true;
  } catch (const std::runtime_error&) {
    out.native = Native::Capture(); out.threw = true;
  }
  out.state = Capture(is_recorded);
}
void Equal(const Observation& a, const Observation& b) {
  Check(a.result == b.result && a.returned == b.returned && a.threw == b.threw && a.state == b.state,
        "exact original and recorded complete state result and unwind agree");
  Check(a.native == b.native, "exact original and recorded produced LastError MXCSR agree");
  const auto print = [](const Observation& value) {
    const auto& s = value.state;
    std::cout << "{\"result\":" << value.result << ",\"returned\":" << (value.returned ? "true" : "false")
      << ",\"threw\":" << (value.threw ? "true" : "false") << ",\"last_error\":" << value.native.error
      << ",\"mxcsr\":" << value.native.mxcsr << ",\"state\":{\"host\":" << s.host << ",\"guest\":" << s.guest
      << ",\"numerator\":" << s.numerator << ",\"denominator\":" << s.denominator << ",\"frequency\":" << s.frequency
      << ",\"scalar_bits\":" << s.scalar_bits << ",\"no_scaling\":" << (s.no_scaling ? "true" : "false") << "}}";
  };
  std::cout << "OBS_COMMIT_PAIR {\"original\":"; print(a);
  std::cout << ",\"recorded\":"; print(b); std::cout << "}\n";
}
void Placement(const Proxy& value) {
  Check(value.begin_without_mutex.load() == 0, "commit admission occurred outside the original mutex");
  Check(value.unordered_with_mutex.load() == 0, "NoScaling admission acquired original mutex");
  Check(value.publish_with_mutex.load() == 0 && value.abort_with_mutex.load() == 0,
        "commit publication or abort happened before original mutex unlock");
}
void PrintSnapshot(const char* name, Proxy& value, Core::Snapshot& out) {
  value.core.Stop();
  Check(value.core.TryFreezeInto(out), "source-seam snapshot fully drained");
  Placement(value);
  const auto& c = out.counts;
  uint64_t mutex_rows = 0, unordered_rows = 0;
  bool complete = true;
  for (size_t i = 0; i < out.size; ++i) {
    const auto& row = out.rows[i];
    complete = complete && row.storage_ordinal == i + 1 && row.disposition == h::Disposition::Published && !row.issues;
    if (row.ordering == h::Ordering::Mutex) {
      ++mutex_rows; complete = complete && row.mutex_ordinal == mutex_rows;
    } else {
      ++unordered_rows; complete = complete && row.ordering == h::Ordering::Unordered && !row.mutex_ordinal;
    }
  }
  Check(complete && c.observed_attempts == out.size && c.stored_entries == out.size && c.published_entries == out.size &&
        c.mutex_ordinals == mutex_rows && c.unordered_attempts == unordered_rows && !c.aborted_entries &&
        !c.overflow_entries && !c.diagnostic_entries && !c.saturated_rejections && !c.counter_saturated,
        "source-seam complete row and count inventory is exact without loss or repair");
  std::cout << "RAW_COMMIT_CASE {\"case\":\"" << name << "\",\"run_high\":" << out.run.high
    << ",\"run_low\":" << out.run.low << ",\"size\":" << out.size
    << ",\"counts\":{\"observed_attempts\":" << c.observed_attempts << ",\"mutex_ordinals\":" << c.mutex_ordinals
    << ",\"unordered_attempts\":" << c.unordered_attempts << ",\"stored_entries\":" << c.stored_entries
    << ",\"published_entries\":" << c.published_entries << ",\"aborted_entries\":" << c.aborted_entries
    << ",\"overflow_entries\":" << c.overflow_entries << ",\"diagnostic_entries\":" << c.diagnostic_entries
    << ",\"saturated_rejections\":" << c.saturated_rejections << ",\"counter_saturated\":" << c.counter_saturated
    << ",\"reserved\":" << c.reserved << "},\"rows\":[";
  for (size_t i = 0; i < out.size; ++i) {
    const auto& r = out.rows[i]; const auto& q = r.binding; const auto& m = r.metadata;
    if (i) std::cout << ',';
    std::cout << "{\"storage_ordinal\":" << r.storage_ordinal << ",\"mutex_ordinal\":" << r.mutex_ordinal
      << ",\"kind\":" << uint32_t(r.kind) << ",\"ordering\":" << uint32_t(r.ordering)
      << ",\"disposition\":" << uint32_t(r.disposition) << ",\"issues\":" << r.issues
      << ",\"binding\":{\"run_high\":" << q.run_high << ",\"run_low\":" << q.run_low
      << ",\"timer_invocation\":" << q.timer_invocation << ",\"read_ordinal\":" << q.read_ordinal
      << ",\"phase_call_id\":" << q.phase_call_id << ",\"phase_thread_epoch\":" << q.phase_thread_epoch
      << ",\"phase_native_thread_id\":" << q.phase_native_thread_id << ",\"phase_present\":" << q.phase_present
      << ",\"present\":" << q.present << ",\"reserved\":" << q.reserved << "}"
      << ",\"metadata\":{\"host_sample\":" << m.host_sample << ",\"returned_guest\":" << m.returned_guest
      << ",\"pre_host\":" << m.pre_host << ",\"pre_guest\":" << m.pre_guest
      << ",\"post_host\":" << m.post_host << ",\"post_guest\":" << m.post_guest
      << ",\"pre_ratio_numerator\":" << m.pre_ratio_numerator << ",\"pre_ratio_denominator\":" << m.pre_ratio_denominator
      << ",\"post_ratio_numerator\":" << m.post_ratio_numerator << ",\"post_ratio_denominator\":" << m.post_ratio_denominator
      << ",\"presence\":" << m.presence << ",\"reserved\":" << m.reserved << "}}";
  }
  std::cout << "],\"engineering_proof\":{\"sdk_build_ready\":false,\"source_routed_coverage\":false,"
    "\"complete_clock_history\":false,\"native_state_preservation\":false,\"actual_clock_cause\":false,"
    "\"simulation\":false,\"fps\":false}}\n";
}
MclaClockWitnessBindingV1 Request(uint64_t read) {
  return {MCLA_CW_ABI_V1, sizeof(MclaClockWitnessBindingV1), run.high, run.low,
      7, read, 11, 13, 17, 1, 0, 0};
}

void BoundUnboundAndNested() {
  Reset(); Proxy value; value.mutex = &recorded::tick_mutex_;
  Check(value.core.Arm(run), "bound source Arm");
  b::Runtime old;
  Check(old.Arm(run.high, run.low), "bound witness Arm");
  Observation direct[3], actual[3];
  for (unsigned i = 0; i < 3; ++i) {
    SamplePlan p; p.sample = i == 2 ? 104 : 101 + i; p.nested = i == 2; p.nested_sample = 103;
    { PlanScope planned(p); Observe(false, direct[i]); }
    Check(p.calls == 1 && p.nested_calls == (i == 2 ? 1u : 0u), "baseline original sample exactly once per source invocation");
  }
  {
    ProxyScope active(value);
    for (unsigned i = 0; i < 3; ++i) {
      SamplePlan p; p.sample = i == 2 ? 104 : 101 + i; p.nested = i == 2; p.nested_sample = 103;
      PlanScope planned(p);
      if (i == 0) Observe(true, actual[i]);
      else {
        auto request = Request(i - 1); MclaClockWitnessStatusV1 status{};
        incoming.Restore();
        actual[i].result = old.QueryBound(&request, &status, [] { return recorded::UpdateGuestClock(); });
        actual[i].native = Native::Capture(); actual[i].returned = true; actual[i].state = Capture(true);
        Check(status.admission == MCLA_CW_ACCEPTED && status.completion == MCLA_CW_PUBLISHED,
              "old bound witness still completes one actual original source call");
      }
      Check(p.calls == 1 && p.nested_calls == (i == 2 ? 1u : 0u), "recorded original sample exactly once per source invocation");
      Check(p.sample_incoming == incoming, "source sample sees original incoming LastError MXCSR");
      Equal(direct[i], actual[i]);
    }
  }
  old.Stop(); MclaClockWitnessSnapshotV1 prior{};
  prior.abi_version = MCLA_CW_ABI_V1; prior.struct_size = sizeof(prior);
  Check(old.TrySnapshot(&prior) == MCLA_CW_OK && prior.row_count == 2 && !prior.missing_witnesses && !prior.duplicate_witnesses,
        "nested consumed source does not create an extra bound witness");
  Core::Snapshot out{}; PrintSnapshot("bound-unbound-nested", value, out);
  Check(out.size == 4 && !out.rows[0].binding.present && out.rows[1].binding.present == 1 &&
        out.rows[1].binding.read_ordinal == 0 && !out.rows[2].binding.present && out.rows[3].binding.present == 1 &&
        out.rows[3].binding.read_ordinal == 1, "nested consumed request is unbound without inheriting outer identity");
  for (size_t position : {size_t(1), size_t(3)}) {
    const auto& identity = out.rows[position].binding;
    Check(identity.run_high == run.high && identity.run_low == run.low && identity.timer_invocation == 7 &&
          identity.phase_call_id == 11 && identity.phase_thread_epoch == 13 && identity.phase_native_thread_id == 17 &&
          identity.phase_present == 1 && !identity.reserved, "entire copied request identity survives primary Witness claim");
  }
  Check(out.rows[0].metadata.returned_guest == 505 && out.rows[1].metadata.returned_guest == 510 &&
        out.rows[2].metadata.returned_guest == 515 && out.rows[3].metadata.returned_guest == 520,
        "all unbound bound and nested global commits retained");
}

void PreArmSampledStraddler() {
  Reset(); Proxy value; value.mutex = &recorded::tick_mutex_;
  Observation direct, actual;
  SamplePlan base; { PlanScope planned(base); Observe(false, direct); }
  std::latch sampled(1), resume(1);
  SamplePlan p; p.sampled = &sampled; p.resume = &resume;
  std::thread thread([&] { ProxyScope active(value); PlanScope planned(p); Observe(true, actual); });
  sampled.wait(); // Source constructor/sample happened while epoch was OFF.
  Check(value.core.Arm(run), "Arm after source sample before original mutex admission");
  resume.count_down(); thread.join(); Equal(direct, actual);
  Core::Snapshot out{}; PrintSnapshot("sampled-prearm-straddler", value, out);
  Check(p.calls == 1 && out.size == 1 && out.rows[0].mutex_ordinal == 1 && out.rows[0].metadata.host_sample == 101,
        "pre-Arm sampled call committing inside epoch is not omitted");
}

struct ReorderResult { Observation newer, older; State final; };
void Reorder(bool is_recorded, Proxy& value, ReorderResult& out) {
  std::latch sampled(1), resume(1);
  SamplePlan older; older.sample = 101; older.sampled = &sampled; older.resume = &resume;
  std::thread a([&] { ProxyScope active(value); PlanScope planned(older); Observe(is_recorded, out.older); });
  sampled.wait();
  SamplePlan newer; newer.sample = 103;
  { ProxyScope active(value); PlanScope planned(newer); Observe(is_recorded, out.newer); }
  resume.count_down(); a.join(); out.final = Capture(is_recorded);
  Check(older.calls == 1 && newer.calls == 1, "reorder runs each original sample once");
}
void SampleAndMutexOrder() {
  Reset(); Proxy value; value.mutex = &recorded::tick_mutex_;
  Check(value.core.Arm(run), "reorder Arm");
  ReorderResult direct, actual;
  Reorder(false, value, direct); Reorder(true, value, actual);
  Equal(direct.newer, actual.newer); Equal(direct.older, actual.older);
  Check(direct.final == actual.final && actual.final.host == 101 && actual.final.guest == 515,
        "older sampled call replaces newer host state without inventing guest delta");
  Core::Snapshot out{}; PrintSnapshot("sample-versus-mutex-order", value, out);
  Check(out.size == 2 && out.rows[0].mutex_ordinal == 1 && out.rows[0].metadata.host_sample == 103 &&
        out.rows[1].mutex_ordinal == 2 && out.rows[1].metadata.host_sample == 101 &&
        out.rows[1].metadata.pre_host == 103 && out.rows[1].metadata.post_host == 101,
        "existing mutex order distinguishes actual synthetic sampling reordering");
}

void Contended(bool is_recorded, Proxy& value, Observation& out) {
  auto& mutex = is_recorded ? recorded::tick_mutex_ : baseline::tick_mutex_;
  std::latch failed(1);
  SamplePlan p; p.sample = 104; p.try_failed = &failed;
  mutex.lock();
  std::thread thread([&] { ProxyScope active(value); PlanScope planned(p); Observe(is_recorded, out); });
  failed.wait(); // Native std::mutex::try_lock really failed before release.
  mutex.unlock(); thread.join(); Check(p.calls == 1, "contended original sample occurs once");
}
void CachedContention() {
  Reset(); Proxy value; value.mutex = &recorded::tick_mutex_;
  Check(value.core.Arm(run), "Cached Arm");
  Observation direct, actual; Contended(false, value, direct); Contended(true, value, actual); Equal(direct, actual);
  Core::Snapshot out{}; PrintSnapshot("native-wrapper-contention-cached", value, out);
  Check(out.size == 1 && out.rows[0].kind == h::Kind::Cached && out.rows[0].metadata.host_sample == 104 &&
        out.rows[0].metadata.pre_host == 100 && out.rows[0].metadata.post_host == 100 &&
        out.rows[0].metadata.returned_guest == 500, "actual try-lock failure retains cached state and original sample");
}

void RatioAndUnordered() {
  Reset(); Proxy value; value.mutex = &recorded::tick_mutex_;
  Check(value.core.Arm(run), "ratio source Arm");
  Observation direct[3], actual[3]; Native native_ratio[2]; State ratio_state[2];
  for (unsigned mode = 0; mode < 2; ++mode) {
    const bool recorded_mode = mode == 1; ProxyScope active(value);
    auto* observations = recorded_mode ? actual : direct;
    SamplePlan first; { PlanScope planned(first); Observe(recorded_mode, observations[0]); }
    incoming.Restore();
    if (recorded_mode) { recorded::guest_tick_frequency_ = 30; recorded::RecomputeGuestTickScalar(); }
    else { baseline::guest_tick_frequency_ = 30; baseline::RecomputeGuestTickScalar(); }
    native_ratio[mode] = Native::Capture(); ratio_state[mode] = Capture(recorded_mode);
    SamplePlan next; next.sample = 103; { PlanScope planned(next); Observe(recorded_mode, observations[1]); }
    if (recorded_mode) recorded::clock_no_scaling = true; else baseline::clock_no_scaling = true;
    SamplePlan unscaled; unscaled.sample = 104; { PlanScope planned(unscaled); Observe(recorded_mode, observations[2]); }
  }
  for (unsigned i = 0; i < 3; ++i) Equal(direct[i], actual[i]);
  Check(native_ratio[0] == native_ratio[1] && ratio_state[0] == ratio_state[1] && ratio_state[1].numerator == 3 &&
        ratio_state[1].denominator == 1, "exact original effective ratio math and native state retained");
  Core::Snapshot out{}; PrintSnapshot("effective-ratio-and-no-scaling", value, out);
  Check(out.size == 4 && out.rows[1].kind == h::Kind::Ratio && out.rows[1].mutex_ordinal == 2 &&
        !out.rows[1].binding.present && out.rows[1].metadata.pre_ratio_numerator == 5 &&
        out.rows[1].metadata.post_ratio_numerator == 3 && out.rows[1].metadata.pre_host == 101 &&
        out.rows[1].metadata.post_host == 101, "effective ratio assignment shares existing clock mutex order");
  Check(out.rows[2].metadata.returned_guest == 511 && out.rows[3].kind == h::Kind::NoScaling &&
        out.rows[3].ordering == h::Ordering::Unordered && !out.rows[3].mutex_ordinal &&
        out.rows[3].metadata.returned_guest == 312 && out.rows[3].metadata.presence == h::kUnorderedFields &&
        !out.rows[3].metadata.post_host && out.counts.mutex_ordinals == 3 && out.counts.unordered_attempts == 1,
        "NoScaling preserves lock-free source path and unavailable locked metadata");
}

void StopHeldPublisher() {
  Reset(); Proxy value; value.mutex = &recorded::tick_mutex_;
  Check(value.core.Arm(run), "held source Arm");
  Observation direct, actual; SamplePlan base;
  { PlanScope planned(base); Observe(false, direct); }
  std::latch waiting(1), allow(1); value.publish_waiting = &waiting; value.allow_publish = &allow;
  SamplePlan p;
  std::thread thread([&] { ProxyScope active(value); PlanScope planned(p); Observe(true, actual); });
  waiting.wait(); value.core.Stop();
  Core::Snapshot pending, saved;
  std::memset(&pending, 0xA7, sizeof(pending)); std::memcpy(&saved, &pending, sizeof(saved));
  Check(!value.core.TryFreezeInto(pending) && std::memcmp(&pending, &saved, sizeof(saved)) == 0,
        "source publisher after unlock remains pending across nonblocking Stop");
  allow.count_down(); thread.join(); Equal(direct, actual);
  Core::Snapshot out{}; PrintSnapshot("source-stop-held-publisher", value, out);
  Check(out.size == 1 && out.counts.published_entries == 1 && !out.counts.aborted_entries,
        "held source publisher drains exactly once");
}

void BeforeLockThrow() {
  Reset(); Proxy value; value.mutex = &recorded::tick_mutex_;
  Check(value.core.Arm(run), "throw source Arm");
  // Heap observations avoid relying on optimized captured automatic variables
  // across C++ EH. Catch captures native state FIRST, before other observation.
  auto direct = std::make_unique<Observation>(); auto actual = std::make_unique<Observation>();
  SamplePlan a; a.throw_before_lock = true;
  { PlanScope planned(a); Observe(false, *direct); }
  SamplePlan c; c.throw_before_lock = true;
  { ProxyScope active(value); PlanScope planned(c); Observe(true, *actual); }
  Equal(*direct, *actual);
  Core::Snapshot out{}; PrintSnapshot("source-before-lock-throw", value, out);
  Check(a.calls == 1 && c.calls == 1 && actual->threw && !actual->returned && actual->state.host == 100 &&
        actual->state.guest == 500 && out.size == 0 && !out.counts.observed_attempts,
        "throw before existing mutex does not invent an admitted commit");
}
} // namespace fixture

int main() {
  const fixture::PreserveNative preserve;
  fixture::Check(!fixture::h::sdk::kSdkBuildReady && !fixture::h::sdk::kSourceRoutedCoverageProven &&
      !fixture::h::sdk::kNativeStatePreservationProven, "fixture never claims SDK build or history admission");
  fixture::BoundUnboundAndNested(); fixture::PreArmSampledStraddler(); fixture::SampleAndMutexOrder();
  fixture::CachedContention(); fixture::RatioAndUnordered(); fixture::StopHeldPublisher(); fixture::BeforeLockThrow();
  std::cout << "checks=" << fixture::checks.load() << " failures=" << fixture::failures.load() << '\n';
  return fixture::failures.load() ? 1 : 0;
}
