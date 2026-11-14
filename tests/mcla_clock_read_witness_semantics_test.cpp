// OFFLINE source-body comparison, not an SDK patch or attribution of a real miss.
// Recipe supplies original906-byte and explicitly transformed includes and pins.
// Only predetermined clock values/mock mutex schedules execute. Trace methods
// have no OS APIs, clock queries, allocation, observer lock or guest calculation.
#include "../src/mcla_clock_read_witness_core.h"

#include <array>
#include <cstdint>
#include <initializer_list>
#include <iostream>
#include <limits>
#include <string_view>
#include <utility>
#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>
#endif
#if defined(_M_X64) || defined(__x86_64__)
#include <xmmintrin.h>
#endif

#ifndef MCLA_CLOCK_SOURCE_SHA256
#error Recipe must verify and declare the exact SDK clock.cpp source hash.
#endif
static_assert(std::string_view(MCLA_CLOCK_SOURCE_SHA256) ==
    "D72A7093E0029F85FFB6859B05DE440416EB2F66157E2C5BC994B538A29F7DE4");

namespace w = mcla::clock_read_witness;
using Core = w::Recorder<4>;
constexpr w::RunToken kRun{0x123456789ABCDEF0ull, 0xFEDCBA9876543210ull};

namespace native_state {
struct State { uint32_t error, mxcsr; constexpr bool operator==(const State&) const = default; };
#if !defined(_WIN32)
inline uint32_t emulated_error{};
#endif
State Read() noexcept {
  State value{};
#if defined(_WIN32)
  value.error = GetLastError();
#else
  value.error = emulated_error;
#endif
#if defined(_M_X64) || defined(__x86_64__)
  value.mxcsr = _mm_getcsr();
#endif
  return value;
}
void Write(State value) noexcept {
#if defined(_WIN32)
  SetLastError(value.error);
#else
  emulated_error = value.error;
#endif
#if defined(_M_X64) || defined(__x86_64__)
  _mm_setcsr(value.mxcsr);
#endif
}
}  // namespace native_state

namespace schedule {
struct Signal { unsigned code; };
enum class Tag : uint32_t { Sample, Try, Wait, Unlock, ExternalRelease, ThrowSample, ThrowTry, ThrowWait };
struct Event { Tag tag; uint64_t value; constexpr bool operator==(const Event&) const = default; };
using Hook = void (*)();
inline Hook before_try{}, on_wait{};
inline std::array<uint64_t, 8> samples{};
inline size_t sample_count{}, next{};
inline std::array<Event, 64> events{};
inline size_t event_count{};
inline std::array<native_state::State, 8> incoming_states{};
inline unsigned tries{}, waits{}, unlocks{}, owned_locks{}, fixture_errors{};
inline bool held{}, throw_sample{}, throw_try{}, throw_wait{};

void Log(Tag tag, uint64_t value = 0) noexcept {
  if (event_count == events.size()) { ++fixture_errors; return; }
  events[event_count++] = {tag, value};
}
void Reset(std::initializer_list<uint64_t> input) {
  before_try = on_wait = nullptr;
  samples = {}; sample_count = input.size(); next = 0;
  if (sample_count > samples.size()) throw Signal{900};
  size_t index = 0; for (auto value : input) samples[index++] = value;
  events = {}; incoming_states = {}; event_count = 0;
  tries = waits = unlocks = owned_locks = fixture_errors = 0;
  held = throw_sample = throw_try = throw_wait = false;
}
uint64_t Sample() {
  if (next >= sample_count) throw Signal{901};
  const auto index = next++;
  incoming_states[index] = native_state::Read();
  // Explicit original-produced effects, outside the Trace helper. Integer clock
  // arithmetic/metadata recording must leave these effects observable unchanged.
  native_state::Write({uint32_t(0xABCD1000u + index), index % 2 ? 0x5F80u : 0x3F80u});
  if (throw_sample) { Log(Tag::ThrowSample, index); throw Signal{101}; }
  Log(Tag::Sample, samples[index]);
  return samples[index];
}
}  // namespace schedule

namespace mock_std {
struct defer_lock_t {};
inline constexpr defer_lock_t defer_lock{};
struct mutex {
  bool try_lock() {
    ++schedule::tries; schedule::Log(schedule::Tag::Try);
    if (schedule::throw_try) {
      native_state::Write({0xEEEE0001u, 0x7F80u});
      schedule::Log(schedule::Tag::ThrowTry); throw schedule::Signal{102};
    }
    if (schedule::held) return false;
    schedule::held = true; ++schedule::owned_locks; return true;
  }
  void lock() {
    ++schedule::waits; schedule::Log(schedule::Tag::Wait);
    if (schedule::throw_wait) {
      native_state::Write({0xEEEE0002u, 0x7F80u});
      schedule::Log(schedule::Tag::ThrowWait); throw schedule::Signal{103};
    }
    if (schedule::on_wait) {
      const auto hook = std::exchange(schedule::on_wait, nullptr); hook();
    }
    if (schedule::held) throw schedule::Signal{902};
    schedule::held = true; ++schedule::owned_locks;
  }
  void unlock() noexcept {
    if (!schedule::held || !schedule::owned_locks) ++schedule::fixture_errors;
    schedule::held = false;
    if (schedule::owned_locks) --schedule::owned_locks;
    ++schedule::unlocks; schedule::Log(schedule::Tag::Unlock);
  }
};
template<class Mutex> class unique_lock {
 public:
  unique_lock(Mutex& mutex, defer_lock_t) : mutex_(mutex) {
    // The original source already sampled BEFORE constructing this lock.
    if (schedule::before_try) {
      const auto hook = std::exchange(schedule::before_try, nullptr); hook();
    }
  }
  bool try_lock() { owned_ = mutex_.try_lock(); return owned_; }
  void lock() { mutex_.lock(); owned_ = true; }
  ~unique_lock() noexcept { if (owned_) mutex_.unlock(); }
  unique_lock(const unique_lock&) = delete;
  unique_lock& operator=(const unique_lock&) = delete;
 private:
  Mutex& mutex_;
  bool owned_{};
};
}  // namespace mock_std

namespace trace {
inline Core* recorder{};
inline w::BoundRead current{};
inline unsigned begun{}, published{}, aborted{}, overflow{}, unbound{}, disabled{};
inline unsigned publication_while_locked{}, bad_metadata_stage{};
void Reset(Core& core) noexcept {
  recorder = &core; current = {};
  begun = published = aborted = overflow = unbound = disabled = 0;
  publication_while_locked = bad_metadata_stage = 0;
}
}  // namespace trace

// A local helper constructed BEFORE the original query/unique_lock. Therefore
// return-expression evaluation marks data, then the original lock destructor
// unlocks, and only then this destructor publishes outside that existing lock.
class MclaClockWitnessCall {
 public:
  MclaClockWitnessCall() noexcept : lease_(trace::recorder->Begin(trace::current)) {
    switch (lease_.status()) {
      case w::StartStatus::Accepted: ++trace::begun; break;
      case w::StartStatus::Unbound: ++trace::unbound; break;
      case w::StartStatus::Disabled: ++trace::disabled; break;
      case w::StartStatus::Overflow: ++trace::overflow; break;
      default: ++trace::bad_metadata_stage; break;
    }
  }
  ~MclaClockWitnessCall() noexcept {
    if (!lease_.accepted()) return;
    if (!returned_) { lease_.Abort(); ++trace::aborted; return; }
    if ((data_.presence & w::kLockedState) && schedule::owned_locks) {
      ++trace::publication_while_locked; lease_.Abort(); ++trace::aborted; return;
    }
    if (!lease_.Publish(data_)) ++trace::bad_metadata_stage;
    else ++trace::published;
  }
  void Sampled(uint64_t sample) noexcept {
    data_.host_sample = sample; data_.presence |= w::kHostSample;
  }
  void LockedBefore(uint64_t numerator, uint64_t denominator, uint64_t host, uint64_t guest) noexcept {
    if (schedule::owned_locks != 1) ++trace::bad_metadata_stage;
    data_.ratio_numerator = numerator; data_.ratio_denominator = denominator;
    data_.pre_last_host = host; data_.pre_last_guest = guest;
    data_.presence |= w::kRatio | w::kLockedState;
  }
  void Advanced(uint64_t host, uint64_t guest) noexcept {
    if (schedule::owned_locks != 1) ++trace::bad_metadata_stage;
    data_.post_last_host = host; data_.post_last_guest = guest; data_.branch = w::Branch::Advance;
  }
  void Cached(uint64_t host, uint64_t guest) noexcept {
    if (schedule::owned_locks != 1) ++trace::bad_metadata_stage;
    data_.post_last_host = host; data_.post_last_guest = guest; data_.branch = w::Branch::Cached;
  }
  uint64_t Returned(uint64_t guest) noexcept {
    data_.returned_guest = guest; data_.presence |= w::kReturnedGuest; returned_ = true;
#if defined(MCLA_CLOCK_WITNESS_EARLY_PUBLISH_MUTANT)
    // Fixture-only negative control: retain the same original result while
    // deliberately violating the publication seam. The ordinary test MUST fail.
    if (lease_.accepted() && schedule::owned_locks) {
      ++trace::publication_while_locked;
      if (lease_.Publish(data_)) ++trace::published;
      else ++trace::bad_metadata_stage;
    }
#endif
    return guest;
  }
  void Unscaled(uint64_t numerator, uint64_t denominator, uint64_t guest) noexcept {
    data_.ratio_numerator = numerator; data_.ratio_denominator = denominator;
    data_.returned_guest = guest; data_.branch = w::Branch::NoScaling;
    data_.presence |= w::kRatio | w::kReturnedGuest; returned_ = true;
  }
 private:
  Core::Lease lease_;
  w::Metadata data_{};
  bool returned_{};
};

namespace baseline {
inline bool clock_no_scaling{};
inline std::pair<uint64_t,uint64_t> guest_tick_ratio_{5,1};
inline uint64_t last_host_tick_count_{}, last_guest_tick_count_{};
inline mock_std::mutex tick_mutex_;
struct Clock { static uint64_t QueryHostTickCount() { return schedule::Sample(); } };
#define REXCVAR_GET(name) (name)
#define std mock_std
#include "mcla_clock_update_body.inc"
#undef std
#undef REXCVAR_GET
}  // namespace baseline

namespace instrumented {
inline bool clock_no_scaling{};
inline std::pair<uint64_t,uint64_t> guest_tick_ratio_{5,1};
inline uint64_t last_host_tick_count_{}, last_guest_tick_count_{};
inline mock_std::mutex tick_mutex_;
struct Clock { static uint64_t QueryHostTickCount() { return schedule::Sample(); } };
#define REXCVAR_GET(name) (name)
#define std mock_std
#include "mcla_clock_update_witness_body.inc"
#undef std
#undef REXCVAR_GET
}  // namespace instrumented

namespace {
enum class Scenario { Uncontended, CachedOld, Reordered, NoScaling, GuestWrap, ClockThrow, TryThrow, WaitThrow };
enum class Mode { Admitted, Unbound, Disabled, Overflow };
struct Engine {
  uint64_t (*read)(); bool* no_scaling; std::pair<uint64_t,uint64_t>* ratio;
  uint64_t* host; uint64_t* guest;
};
const Engine original{baseline::UpdateGuestClock, &baseline::clock_no_scaling,
    &baseline::guest_tick_ratio_, &baseline::last_host_tick_count_, &baseline::last_guest_tick_count_};
const Engine observed{instrumented::UpdateGuestClock, &instrumented::clock_no_scaling,
    &instrumented::guest_tick_ratio_, &instrumented::last_host_tick_count_, &instrumented::last_guest_tick_count_};
inline Engine active{};
inline Mode active_mode{};
inline uint64_t nested_return{};
unsigned checks{}, failures{};
void Check(bool ok, const char* label) {
  ++checks; if (!ok) { ++failures; std::cerr << "FAIL " << label << '\n'; }
}
w::BoundRead Binding(uint64_t invocation, uint64_t read = 0) noexcept {
  return {kRun, active_mode == Mode::Unbound ? 0 : invocation, read,
          {100 + invocation, 9, 31, 1}};
}
void ReleaseExistingOwner() {
  if (!schedule::held || schedule::owned_locks) throw schedule::Signal{903};
  schedule::held = false; schedule::Log(schedule::Tag::ExternalRelease);
}
void NewerCallerCompletes() {
  const auto previous = trace::current;
  trace::current = Binding(3, 0);
  nested_return = active.read();
  trace::current = previous;
}
struct Outcome {
  std::array<uint64_t,4> returns{};
  std::array<native_state::State,4> produced_states{};
  std::array<native_state::State,8> incoming_states{};
  size_t return_count{}, samples{}, events_count{};
  std::array<schedule::Event,64> events{};
  unsigned tries{}, waits{}, unlocks{}, owned_locks{}, exception{}, fixture_errors{};
  bool held{};
  uint64_t host{}, guest{}, nested{};
  native_state::State final_state{};
  Core::Snapshot inventory{};
  unsigned trace_begun{}, trace_published{}, trace_aborted{}, trace_overflow{}, trace_unbound{}, trace_disabled{};
  unsigned trace_under_lock{}, trace_bad_stage{};
};

Outcome Run(const Engine& engine, Scenario scenario, Mode mode, bool with_trace) {
  active = engine; active_mode = mode; nested_return = 0;
  *engine.no_scaling = false; *engine.ratio = {5,1}; *engine.host = 95; *engine.guest = 475;
  Core core; trace::Reset(core);
  if (mode != Mode::Disabled) Check(core.Arm(kRun), "scenario arm outside original call");
  if (mode == Mode::Overflow) {
    const w::Metadata prefill{1,5,5,1,0,0,0,0,w::kHostSample|w::kReturnedGuest|w::kRatio,w::Branch::NoScaling};
    for (unsigned i = 0; i < 4; ++i)
      Check(core.Begin({kRun,900+i,0,{0,0,0,0}}).Publish(prefill), "overflow prefill outside original call");
  }
  switch (scenario) {
    case Scenario::Reordered: schedule::Reset({100,105,110,130}); break;
    case Scenario::GuestWrap:
      schedule::Reset({100}); *engine.guest = std::numeric_limits<uint64_t>::max()-9; break;
    case Scenario::ClockThrow: schedule::Reset({100}); schedule::throw_sample = true; break;
    case Scenario::TryThrow: schedule::Reset({100}); schedule::throw_try = true; break;
    case Scenario::WaitThrow: schedule::Reset({100}); schedule::held = schedule::throw_wait = true; break;
    default: schedule::Reset({100,201}); break;
  }
  if (scenario == Scenario::CachedOld) { schedule::held = true; schedule::on_wait = ReleaseExistingOwner; }
  if (scenario == Scenario::NoScaling) *engine.no_scaling = true;
  Outcome out;
  native_state::Write({0x24681357u,0x1F80u});
  auto call = [&](uint64_t invocation) {
    trace::current = Binding(invocation,0);
    const auto value = engine.read();
    const auto state = native_state::Read();
    if (out.return_count == out.returns.size()) throw schedule::Signal{904};
    out.returns[out.return_count] = value; out.produced_states[out.return_count++] = state;
  };
  try {
    call(1);
    if (scenario == Scenario::Reordered) {
      schedule::before_try = NewerCallerCompletes;
      call(2); call(4);
    } else if (scenario == Scenario::Uncontended || scenario == Scenario::CachedOld || scenario == Scenario::NoScaling) call(2);
  } catch (const schedule::Signal& error) { out.exception = error.code; }
  catch (...) { out.exception = 999; }
  // Capture produced native state BEFORE assertions/printing or other fixture work.
  out.final_state = native_state::Read();
  out.incoming_states = schedule::incoming_states;
  out.samples = schedule::next; out.events = schedule::events; out.events_count = schedule::event_count;
  out.tries = schedule::tries; out.waits = schedule::waits; out.unlocks = schedule::unlocks;
  out.held = schedule::held; out.owned_locks = schedule::owned_locks; out.fixture_errors = schedule::fixture_errors;
  out.host = *engine.host; out.guest = *engine.guest; out.nested = nested_return;
  out.trace_begun = trace::begun; out.trace_published = trace::published; out.trace_aborted = trace::aborted;
  out.trace_overflow = trace::overflow; out.trace_unbound = trace::unbound; out.trace_disabled = trace::disabled;
  out.trace_under_lock = trace::publication_while_locked; out.trace_bad_stage = trace::bad_metadata_stage;
  core.Stop(); Check(core.TrySnapshot(out.inventory), "closed recorder outside source call");
  if (!with_trace) Check(out.trace_begun == 0, "baseline source has no witness helper");
  trace::recorder = nullptr;
  return out;
}

void Compare(const Outcome& a, const Outcome& b) {
  Check(a.return_count == b.return_count && a.returns == b.returns && a.exception == b.exception,
        "original returns and exact exception signal unchanged");
  Check(a.host == b.host && a.guest == b.guest && a.nested == b.nested, "original guest-clock state unchanged");
  Check(a.samples == b.samples && a.tries == b.tries && a.waits == b.waits && a.unlocks == b.unlocks &&
        a.held == b.held && a.owned_locks == b.owned_locks, "single-query and try/wait/unlock counts unchanged");
  Check(a.events_count == b.events_count && a.events == b.events, "original sample/try/wait/unlock order unchanged");
  Check(a.incoming_states == b.incoming_states && a.produced_states == b.produced_states &&
        a.final_state == b.final_state, "query sees incoming and caller receives original-produced LastError/MXCSR");
  Check(a.fixture_errors == 0 && b.fixture_errors == 0 && b.trace_bad_stage == 0, "mock schedule and trace stages valid");
  Check(b.trace_under_lock == 0, "observer publication happened under existing clock lock");
}

void ScenarioAssertions(Scenario scenario, const Outcome& out) {
  if (scenario == Scenario::Uncontended)
    Check(out.returns[0] == 500 && out.returns[1] == 1005 && out.samples == 2 && out.tries == 2 &&
          out.waits == 0 && out.unlocks == 2, "uncontended source-derived golden");
  if (scenario == Scenario::CachedOld)
    Check(out.returns[0] == 475 && out.returns[1] == 1005 && out.samples == 2 && out.waits == 1 &&
          out.unlocks == 2, "cached old-state source-derived golden");
  if (scenario == Scenario::Reordered)
    Check(out.returns[0] == 500 && out.returns[1] == 550 && out.returns[2] == 675 && out.nested == 550 &&
          out.samples == 4 && out.tries == 4 && out.unlocks == 4, "pre-lock reordered samples preserve original extra25 result");
  if (scenario == Scenario::NoScaling)
    Check(out.returns[0] == 500 && out.returns[1] == 1005 && out.host == 95 && out.guest == 475 &&
          out.tries == 0 && out.waits == 0 && out.unlocks == 0, "NoScaling has no added mutex or state mutation");
  if (scenario == Scenario::GuestWrap)
    Check(out.returns[0] == 15 && out.guest == 15 && out.host == 100 && out.samples == 1,
          "guest unsigned wrap retains exact source arithmetic");
  if (scenario == Scenario::ClockThrow)
    Check(out.exception == 101 && out.return_count == 0 && out.samples == 1 && out.tries == 0 && out.unlocks == 0,
          "clock exception remains same before original mutex");
  if (scenario == Scenario::TryThrow)
    Check(out.exception == 102 && out.return_count == 0 && out.samples == 1 && out.tries == 1 && out.unlocks == 0,
          "try-lock exception unchanged without fake unlock");
  if (scenario == Scenario::WaitThrow)
    Check(out.exception == 103 && out.return_count == 0 && out.samples == 1 && out.waits == 1 &&
          out.unlocks == 0 && out.held, "wait-lock exception unchanged without unlocking other owner");
}

void InventoryAssertions(Scenario scenario, Mode mode, const Outcome& out) {
  const bool exception = scenario == Scenario::ClockThrow || scenario == Scenario::TryThrow || scenario == Scenario::WaitThrow;
  if (mode == Mode::Disabled || mode == Mode::Unbound) {
    Check(out.inventory.size == 0 && out.inventory.counts.observed_bound_reads == 0 &&
          (mode == Mode::Disabled ? out.trace_disabled : out.trace_unbound) == out.samples,
          "disabled/unbound source calls produce no fabricated identity or row");
    return;
  }
  if (mode == Mode::Overflow) {
    Check(out.inventory.size == 4 && out.inventory.counts.published_entries == 4 &&
          out.inventory.counts.observed_bound_reads == 4 + out.samples &&
          out.inventory.counts.overflow_entries == out.samples && out.trace_overflow == out.samples,
          "overflow retains prefills and explicit unrecorded admitted attempts");
    return;
  }
  Check(out.inventory.size == out.samples && out.trace_begun == out.samples, "one exact bound event per supplied read");
  if (exception) {
    Check(out.inventory.counts.aborted_entries == 1 && out.inventory.counts.published_entries == 0 &&
          out.trace_aborted == 1 && out.inventory.rows[0].disposition == w::Disposition::Aborted &&
          (out.inventory.rows[0].issues & w::kMetadataUnavailable), "unwind retained aborted rather than successful metadata");
    return;
  }
  Check(out.inventory.counts.published_entries == out.samples && out.inventory.counts.aborted_entries == 0 &&
        out.inventory.counts.diagnostic_entries == 0 && out.trace_published == out.samples,
        "successful source metadata is published after original unlock");
  for (size_t i = 0; i < out.inventory.size; ++i) {
    const auto& row = out.inventory.rows[i];
    Check(row.admission_ordinal == i + 1 && row.binding.run.high == kRun.high && row.binding.run.low == kRun.low &&
          row.binding.read_ordinal == 0 && row.binding.phase.present == 1 && row.binding.phase.thread_epoch == 9 &&
          row.binding.phase.native_thread_id == 31, "full caller binding retained without inferred read uniqueness");
    Check(row.disposition == w::Disposition::Published && row.issues == 0 &&
          (row.metadata.presence & (w::kHostSample|w::kReturnedGuest|w::kRatio)) ==
          (w::kHostSample|w::kReturnedGuest|w::kRatio), "actual host sample return and ratio all present");
  }
  if (scenario == Scenario::CachedOld) {
    const auto& m = out.inventory.rows[0].metadata;
    Check(m.branch == w::Branch::Cached && m.host_sample == 100 && m.pre_last_host == 95 &&
          m.post_last_host == 95 && m.pre_last_guest == 475 && m.post_last_guest == 475 &&
          m.returned_guest == 475, "cached sample and coherent locked state remain distinct raw facts");
  }
  if (scenario == Scenario::Reordered) {
    const auto& older = out.inventory.rows[1]; const auto& newer = out.inventory.rows[2];
    Check(older.binding.timer_invocation == 2 && newer.binding.timer_invocation == 3 &&
          older.metadata.host_sample == 105 && older.metadata.pre_last_host == 110 &&
          older.metadata.post_last_host == 105 && older.metadata.returned_guest == 550 &&
          newer.metadata.host_sample == 110 && newer.metadata.pre_last_host == 100 &&
          newer.metadata.post_last_host == 110, "nested distinct tokens capture actual backwards replacement without invented commit sequence");
  }
  if (scenario == Scenario::NoScaling)
    for (size_t i = 0; i < out.inventory.size; ++i) {
      const auto& m = out.inventory.rows[i].metadata;
      Check(m.branch == w::Branch::NoScaling && !(m.presence & w::kLockedState) &&
            m.pre_last_host == 0 && m.pre_last_guest == 0 && m.post_last_host == 0 && m.post_last_guest == 0,
            "NoScaling never claims coherent lock state");
    }
}
}  // namespace

int main(int argc, char**) {
  if (argc != 1) { std::cerr << "No arguments permitted\n"; return 2; }
  const auto saved_state = native_state::Read();
  constexpr Scenario scenarios[] = {Scenario::Uncontended,Scenario::CachedOld,Scenario::Reordered,Scenario::NoScaling,
      Scenario::GuestWrap,Scenario::ClockThrow,Scenario::TryThrow,Scenario::WaitThrow};
  constexpr Mode modes[] = {Mode::Admitted,Mode::Unbound,Mode::Disabled,Mode::Overflow};
  try {
    for (const auto scenario : scenarios) {
      const auto control = Run(original,scenario,Mode::Admitted,false);
      ScenarioAssertions(scenario,control);
      for (const auto mode : modes) {
        const auto captured = Run(observed,scenario,mode,true);
        Compare(control,captured); ScenarioAssertions(scenario,captured); InventoryAssertions(scenario,mode,captured);
      }
    }
  } catch (...) { ++failures; std::cerr << "FAIL unexpected fixture exception\n"; }
  native_state::Write(saved_state);
  Check(!w::kNativeAdmitted && !w::kAllClockHistoryProven && !w::kClockCommitOrderProven &&
        !w::kSimulationProven && !w::kFpsProven, "no integration complete-history or physical claims");
  std::cout << "checks=" << checks << " failures=" << failures
            << " synthetic_source_semantics_only actual_clock_cause_unproven\n";
  std::cout.flush(); std::cerr.flush();
  return failures ? 1 : 0;
}
