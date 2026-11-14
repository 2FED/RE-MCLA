// Guest-free source counterexamples, NOT a fix or attribution of the S miss.
// Recipe MUST hash the complete reviewed SDK core/clock.cpp before extracting
// exactly `uint64_t UpdateGuestClock() { ... }` into mcla_clock_update_body.inc.
// No substitutions within that include. Supply MCLA_CLOCK_SOURCE_SHA256 with
// the verified whole-file SHA below. Freeze/pin this test, extraction and recipe.
// Only the std mutex vocabulary and QPC input are mocked. No real threads,
// native clock reads, game, files, sleeps, SDK mutation or runtime patching.
#include <cstdint>
#include <initializer_list>
#include <iostream>
#include <stdexcept>
#include <string_view>
#include <utility>
#include <vector>

#ifndef MCLA_CLOCK_SOURCE_SHA256
#error Recipe must verify and declare the exact SDK clock.cpp source hash.
#endif
static_assert(std::string_view(MCLA_CLOCK_SOURCE_SHA256) ==
    "D72A7093E0029F85FFB6859B05DE440416EB2F66157E2C5BC994B538A29F7DE4");

namespace schedule {
using Hook = void (*)();
inline Hook before_try{}, on_wait{};
inline unsigned tries{}, waits{}, unlocks{};
inline bool held{};
inline std::vector<uint64_t> samples;
inline size_t next{};
void Need(bool condition, const char* label) {
  if (!condition) throw std::runtime_error(label);
}
uint64_t Sample() {
  Need(next < samples.size(), "unexpected clock read");
  return samples[next++];
}
void Reset(std::initializer_list<uint64_t> values) {
  before_try = on_wait = nullptr; tries = waits = unlocks = 0;
  held = false; samples = values; next = 0;
}
}

namespace mock_std {
struct defer_lock_t {};
inline constexpr defer_lock_t defer_lock{};
struct mutex {
  bool try_lock() {
    ++schedule::tries;
    if (schedule::held) return false;
    schedule::held = true; return true;
  }
  void lock() {
    ++schedule::waits;
    if (schedule::on_wait) {
      const auto hook = std::exchange(schedule::on_wait, nullptr); hook();
    }
    schedule::Need(!schedule::held, "mock lock would block without schedule");
    schedule::held = true;
  }
  void unlock() {
    schedule::Need(schedule::held, "unlock without ownership");
    schedule::held = false; ++schedule::unlocks;
  }
};
template<class Mutex> class unique_lock {
 public:
  unique_lock(Mutex& mutex, defer_lock_t) : mutex_(mutex) {
    // The actual source has already sampled QPC before constructing this lock.
    if (schedule::before_try) {
      const auto hook = std::exchange(schedule::before_try, nullptr); hook();
    }
  }
  bool try_lock() { owned_ = mutex_.try_lock(); return owned_; }
  void lock() { mutex_.lock(); owned_ = true; }
  ~unique_lock() { if (owned_) mutex_.unlock(); }
  unique_lock(const unique_lock&) = delete;
  unique_lock& operator=(const unique_lock&) = delete;
 private:
  Mutex& mutex_;
  bool owned_{};
};
}

namespace actual {
inline bool clock_no_scaling = false;
inline std::pair<uint64_t,uint64_t> guest_tick_ratio_{5,1};
inline uint64_t last_host_tick_count_{}, last_guest_tick_count_{};
inline mock_std::mutex tick_mutex_;
struct Clock { static uint64_t QueryHostTickCount() { return schedule::Sample(); } };
#define REXCVAR_GET(name) (name)
#define std mock_std
#include "mcla_clock_update_body.inc"
#undef std
#undef REXCVAR_GET
}

namespace {
unsigned checks{}, failures{};
void Check(bool condition, const char* label) {
  ++checks;
  if (!condition) { ++failures; std::cerr << "FAIL " << label << '\n'; }
}
bool Bracket(uint64_t ae, uint64_t ax, uint64_t be, uint64_t bx,
             uint64_t a, uint64_t b) {
  schedule::Need(ae <= ax && ax < be && be <= bx && a <= b,
                 "invalid synthetic bracket");
  return 5*(be-ax) <= b-a && b-a <= 5*(bx-ae);
}
void Reset(std::initializer_list<uint64_t> values) {
  schedule::Reset(values); actual::clock_no_scaling = false;
  actual::guest_tick_ratio_ = {5,1};
  actual::last_host_tick_count_ = 95; actual::last_guest_tick_count_ = 475;
}
void Uncontended() {
  Reset({100,201});
  const auto a = actual::UpdateGuestClock(), b = actual::UpdateGuestClock();
  Check(a == 500 && b == 1005, "uncontended exact 5:1 values");
  Check(Bracket(99,101,200,202,a,b), "uncontended outer bracket true");
  Check(schedule::next == 2 && schedule::tries == 2 && schedule::waits == 0 &&
        schedule::unlocks == 2 && !schedule::held, "uncontended exact schedule");
}
void ReleaseExistingOwner() {
  // Another caller sampled95 and finished updating475 before our entry100,
  // but was preempted while holding its mutex. Its unlock completes our wait.
  schedule::Need(schedule::held, "external owner must hold lock");
  schedule::held = false;
}
void CachedBeforeEntry() {
  Reset({100,201}); schedule::held = true; schedule::on_wait = ReleaseExistingOwner;
  const auto a = actual::UpdateGuestClock();
  Check(a == 475 && actual::last_host_tick_count_ == 95,
        "failed try-lock discards own100 sample, returns pre-entry95 state");
  Check(schedule::waits == 1 && schedule::next == 1, "one deterministic contention wait");
  const auto b = actual::UpdateGuestClock();
  Check(b == 1005 && b-a == 530, "cached endpoint delta530");
  Check(!Bracket(100,103,200,203,a,b), "cached endpoint exceeds unchanged upper515");
  Check(schedule::next == 2 && schedule::tries == 2 && schedule::unlocks == 2 &&
        !schedule::held, "cached path exact schedule");
}
uint64_t newer_return{};
void NewerCallerCompletes() { newer_return = actual::UpdateGuestClock(); }
void SampleBeforeLockReordering() {
  Reset({100,105,110,130});
  const auto a = actual::UpdateGuestClock();
  schedule::before_try = NewerCallerCompletes;
  const auto older_return = actual::UpdateGuestClock();
  Check(newer_return == 550 && older_return == 550, "newer110 commits before older105");
  Check(actual::last_host_tick_count_ == 105,
        "older sampled clock overwrites last host backwards despite zero delta");
  const auto b = actual::UpdateGuestClock();
  Check(a == 500 && b == 675 && b-a == 175, "stale sample produces extra25 ticks");
  Check(!Bracket(99,101,129,131,a,b), "stale order exceeds unchanged upper160");
  Check(schedule::next == 4 && schedule::tries == 4 && schedule::waits == 0 &&
        schedule::unlocks == 4 && !schedule::held, "stale order exact schedule");
  Reset({100,105,110,130});
  const auto control_a = actual::UpdateGuestClock();
  actual::UpdateGuestClock(); actual::UpdateGuestClock();
  const auto control_b = actual::UpdateGuestClock();
  Check(control_a == 500 && control_b == 650 &&
        Bracket(99,101,129,131,control_a,control_b), "ordered sample negative control");
}
void RecordedMissUnchanged() {
  // Accepted S raw ordinals17/18, copied numerically only. No cause attribution,
  // tolerance, corrected timestamp, omitted row or changed acceptance bound.
  constexpr uint64_t ae=6483650901703, ax=6483650901813;
  constexpr uint64_t be=6483651539420, bx=6483651539541;
  constexpr uint64_t a=7286802795, b=7289994030;
  Check(b-a == 3191235 && 5*(be-ax) == 3188035 && 5*(bx-ae) == 3189190,
        "recorded original integer delta and brackets");
  Check(!Bracket(ae,ax,be,bx,a,b) && (b-a)-5*(bx-ae) == 2045,
        "recorded original bracket remains false by2045");
}
}

int main(int argc, char**) {
  if (argc != 1) { std::cerr << "No arguments permitted\n"; std::cerr.flush(); return 2; }
  try { Uncontended(); CachedBeforeEntry(); SampleBeforeLockReordering(); RecordedMissUnchanged(); }
  catch (const std::exception& e) { ++failures; std::cerr << "FAIL exception " << e.what() << '\n'; }
  std::cout << "checks=" << checks << " failures=" << failures
            << " synthetic_source_counterexamples_only actual_S_cause_unproven\n";
  std::cout.flush(); std::cerr.flush();
  if (!std::cout || !std::cerr) return 2;
  return failures ? 1 : 0;
}
