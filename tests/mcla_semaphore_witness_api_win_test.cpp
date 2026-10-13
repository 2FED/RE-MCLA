// Synthetic transport only; no SDK, semaphore/kernel/game operation or files.
// Build once without the witness definition and once with it set to 1.
#include "../src/mcla_semaphore_witness_api_win.h"
#include <array>
#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <thread>
#include <type_traits>

namespace {
namespace sw = rex::diagnostics::semaphore_witness;
unsigned checks = 0, failures = 0;
constexpr DWORD kSentinel = 0xABCDEu;
constexpr uint64_t kCookie = 0x123456789u, kTicket = 0xFEDCBA987u;
void Check(bool value, const char* why) {
  ++checks; if (!value) { ++failures; std::cerr << "FAIL " << why << '\n'; }
}
struct State {
  std::atomic<unsigned> creates{0}, destroys{0}, begins{0}, ends{0};
  int throw_at = 0;
  sw::Creation creation{};
  sw::Admission admission{};
  sw::Completion completion{};
  uint64_t destroyed_cookie = 0;
  HANDLE entered = nullptr, continue_callback = nullptr;
  std::atomic<bool> gate_timeout{false};
};
void Create(void* opaque, const sw::Creation& value) {
  auto& state = *static_cast<State*>(opaque); ++state.creates; state.creation = value;
  SetLastError(1); if (state.throw_at == 1) throw std::runtime_error("synthetic create");
}
void Destroy(void* opaque, uint64_t cookie) {
  auto& state = *static_cast<State*>(opaque); ++state.destroys; state.destroyed_cookie = cookie;
  SetLastError(2); if (state.throw_at == 2) throw std::runtime_error("synthetic destroy");
}
uint64_t Begin(void* opaque, const sw::Admission& value) {
  auto& state = *static_cast<State*>(opaque); ++state.begins; state.admission = value;
  SetLastError(3); if (state.throw_at == 3) throw std::runtime_error("synthetic begin");
  if (state.entered) {
    SetEvent(state.entered);
    if (WaitForSingleObject(state.continue_callback, 1000) != WAIT_OBJECT_0) state.gate_timeout = true;
  }
  return kTicket;
}
void End(void* opaque, const sw::Completion& value) {
  auto& state = *static_cast<State*>(opaque); ++state.ends; state.completion = value;
  SetLastError(4); if (state.throw_at == 4) throw std::runtime_error("synthetic end");
}
sw::CallbackTable Table(State& state) { return {&state, Create, Destroy, Begin, End}; }
sw::Creation Creation() { return {kCookie, 0, 32767, sw::Origin::kFresh}; }
sw::Admission Release() { return {kCookie, sw::Kind::kRelease, 1, sw::TimeoutClass::kNotApplicable}; }
sw::Admission Wait() { return {kCookie, sw::Kind::kWait, 0, sw::TimeoutClass::kZero}; }
void JoinBounded(std::thread& thread) {
  // A test bug must fail the process, never perform an unbounded join or detach
  // a thread retaining stack references. No sleeps or guest operations occur.
  if (WaitForSingleObject(thread.native_handle(), 2000) != WAIT_OBJECT_0) std::abort();
  thread.join();
}
void CommonAndCompileOff() {
  static_assert(!std::is_copy_constructible_v<sw::Registry> && !std::is_copy_assignable_v<sw::Registry> &&
                !std::is_move_constructible_v<sw::Registry> && !std::is_move_assignable_v<sw::Registry>);
  Check(!sw::kNativeAdmitted && !sw::kSimulationProven && !sw::kRenderProven && !sw::kFpsProven &&
        !sw::kFrameToStepMappingProven, "all proof/admission flags false");
  sw::Registry absent;
  SetLastError(kSentinel);
  Check(absent.Begin(Wait()) == 0 && !absent.End({kTicket, sw::Kind::kWait, sw::Result::kSuccess, {}}) &&
        !absent.registered() && !absent.failed() && GetLastError() == kSentinel,
        "unregistered default is no-op and preserves LastError");
  if constexpr (!sw::kCompiledEnabled) {
    State state; sw::Registry registry;
    SetLastError(kSentinel);
    Check(!registry.Register(Table(state)) && GetLastError() == kSentinel, "compile-off registration no-op");
    Check(!registry.Create({0, 0, 0, static_cast<sw::Origin>(99)}) && !registry.Destroy(0) &&
          registry.Begin({0, static_cast<sw::Kind>(99), 0, sw::TimeoutClass::kNotApplicable}) == 0 &&
          !registry.End({0, static_cast<sw::Kind>(99), static_cast<sw::Result>(99), -1}),
          "compile-off malformed metadata no-op");
    registry.Disable();
    Check(!registry.registered() && !registry.admitting() && !registry.failed() && !registry.failure_count() &&
          !state.creates && !state.destroys && !state.begins && !state.ends && GetLastError() == kSentinel,
          "compile-off invokes no callbacks, fails no diagnostics and preserves LastError");
  }
}
[[maybe_unused]] void ImmutableAndDisable() {
  State state, replacement; sw::Registry registry; auto table = Table(state);
  SetLastError(kSentinel);
  Check(registry.Register(table) && registry.registered() && registry.admitting() &&
        GetLastError() == kSentinel, "explicit registration preserves LastError");
  table = Table(replacement); // Caller table storage is not subsequently read.
  Check(registry.Create(Creation()) && GetLastError() == kSentinel && state.creates == 1 &&
        state.creation.cookie == kCookie && state.creation.maximum_count == 32767 && !replacement.creates,
        "immutable copied table and exact metadata, not mutable caller table");
  Check(registry.Destroy(kCookie) && GetLastError() == kSentinel && state.destroys == 1 &&
        state.destroyed_cookie == kCookie, "destroy transported once and LastError restored");
  const auto ticket = registry.Begin(Release());
  Check(ticket == kTicket && state.begins == 1 && state.admission.kind == sw::Kind::kRelease &&
        state.admission.count == 1 && GetLastError() == kSentinel, "opaque host ticket returned unchanged once");
  registry.Disable();
  Check(!registry.admitting() && !registry.Create(Creation()) && !registry.Destroy(kCookie) &&
        registry.Begin(Wait()) == 0 && state.creates == 1 && state.destroys == 1 && state.begins == 1 &&
        GetLastError() == kSentinel, "disable suppresses only new lifecycle/begin observations");
  Check(registry.End({ticket, sw::Kind::kRelease, sw::Result::kSuccess, 0}) && state.ends == 1 &&
        state.completion.ticket == ticket && state.completion.previous_count == 0 && GetLastError() == kSentinel,
        "End after disable uses SAME table and exact nonzero ticket");
  Check(!registry.Register(Table(replacement)) && registry.failed() && registry.failure_count() == 1 &&
        registry.End({ticket, sw::Kind::kRelease, sw::Result::kFailed, {}}) && state.ends == 2 &&
        !replacement.ends && GetLastError() == kSentinel, "re-registration cannot replace even after disable");
  sw::Registry predisabled; predisabled.Disable();
  Check(predisabled.Register(Table(replacement)) && !predisabled.admitting() && predisabled.Begin(Wait()) == 0,
        "registration cannot rearm a pre-disabled registry");
}
[[maybe_unused]] void CallbackFailures() {
  for (int site = 1; site <= 4; ++site) {
    State state; state.throw_at = site; sw::Registry registry; registry.Register(Table(state));
    SetLastError(kSentinel); bool returned = true;
    switch (site) {
      case 1: returned = registry.Create(Creation()); break;
      case 2: returned = registry.Destroy(kCookie); break;
      case 3: returned = registry.Begin(Wait()) != 0; break;
      case 4: returned = registry.End({kTicket, sw::Kind::kWait, sw::Result::kTimeout, {}}); break;
    }
    unsigned original_calls = 0;
    const auto original = [&] { ++original_calls; return 42; };
    const auto original_result = original(); // Failure is diagnostic, not native-operation authority.
    Check(!returned && registry.failed() && registry.failure_count() == 1 && !registry.admitting() &&
          GetLastError() == kSentinel && original_calls == 1 && original_result == 42,
          "callback throw caught; LastError preserved; external original remains independent");
    Check(state.creates + state.destroys + state.begins + state.ends == 1,
          "throwing callback never retried or duplicated");
    state.throw_at = 0;
    Check(registry.End({kTicket, sw::Kind::kWait, sw::Result::kSuccess, {}}) && GetLastError() == kSentinel,
          "diagnostic failure does not strand subsequent in-flight End transport");
  }
}
[[maybe_unused]] void ValidationAndUnsupported() {
  for (const auto kind : {sw::Kind::kSave, sw::Kind::kRestore, sw::Kind::kSignalAndWait, sw::Kind::kWaitMultiple}) {
    State state; sw::Registry registry; registry.Register(Table(state)); SetLastError(kSentinel);
    const auto ticket = registry.Begin({kCookie, kind, 0, sw::TimeoutClass::kNotApplicable});
    Check(ticket == kTicket && registry.End({ticket, kind, sw::Result::kUnsupported, {}}) &&
          state.admission.kind == kind && state.completion.kind == kind && !registry.failed() &&
          GetLastError() == kSentinel, "unsupported admission explicit metadata, never inferred release/wait");
  }
  const std::array<sw::Admission, 6> bad_begins{{
    {0, sw::Kind::kWait, 0, sw::TimeoutClass::kZero},
    {kCookie, static_cast<sw::Kind>(99), 0, sw::TimeoutClass::kNotApplicable},
    {kCookie, sw::Kind::kRelease, 0, sw::TimeoutClass::kNotApplicable},
    {kCookie, sw::Kind::kRelease, sw::kMaxCount + 1, sw::TimeoutClass::kNotApplicable},
    {kCookie, sw::Kind::kWait, 1, sw::TimeoutClass::kZero},
    {kCookie, sw::Kind::kWait, 0, static_cast<sw::TimeoutClass>(99)}}};
  for (const auto& value : bad_begins) {
    State state; sw::Registry registry; registry.Register(Table(state)); SetLastError(kSentinel);
    Check(!registry.Begin(value) && !state.begins && registry.failed() && GetLastError() == kSentinel,
          "bad cookie/enum/count/timeout fail closed before callback");
  }
  const std::array<sw::Completion, 7> bad_ends{{
    {0, sw::Kind::kWait, sw::Result::kSuccess, {}},
    {kTicket, static_cast<sw::Kind>(99), sw::Result::kSuccess, {}},
    {kTicket, sw::Kind::kWait, static_cast<sw::Result>(99), {}},
    {kTicket, sw::Kind::kRelease, sw::Result::kSuccess, {}},
    {kTicket, sw::Kind::kRelease, sw::Result::kSuccess, -1},
    {kTicket, sw::Kind::kRelease, sw::Result::kFailed, 123},
    {kTicket, sw::Kind::kWait, sw::Result::kSuccess, 0}}};
  for (const auto& value : bad_ends) {
    State state; sw::Registry registry; registry.Register(Table(state)); SetLastError(kSentinel);
    Check(!registry.End(value) && !state.ends && registry.failed() && GetLastError() == kSentinel,
          "End requires nonzero ticket and release-only defined previous count");
  }
  const std::array<sw::Creation, 4> bad_creations{{
    {0, 0, 8, sw::Origin::kFresh}, {kCookie, 9, 8, sw::Origin::kFresh},
    {kCookie, 0, sw::kMaxCount + 1, sw::Origin::kFresh}, {kCookie, 0, 8, static_cast<sw::Origin>(99)}}};
  for (const auto& value : bad_creations) {
    State state; sw::Registry registry; registry.Register(Table(state)); SetLastError(kSentinel);
    Check(!registry.Create(value) && !state.creates && registry.failed() && GetLastError() == kSentinel,
          "invalid creation cannot publish fabricated lifecycle");
  }
  State state; sw::Registry bad_table; auto table = Table(state); table.end = nullptr; SetLastError(kSentinel);
  Check(!bad_table.Register(table) && bad_table.failed() && !bad_table.registered() &&
        GetLastError() == kSentinel, "incomplete callback table rejected without publication");
  sw::Registry bad_destroy; bad_destroy.Register(Table(state));
  Check(!bad_destroy.Destroy(0) && !state.destroys && bad_destroy.failed() && GetLastError() == kSentinel,
        "zero destruction cookie fail closed");
}
[[maybe_unused]] void RegistrationRace() {
  State first, second; sw::Registry registry;
  HANDLE start = CreateEventW(nullptr, TRUE, FALSE, nullptr);
  if (!start) std::abort();
  std::atomic<unsigned> winners{0}; std::atomic<bool> timeout{false};
  const auto worker = [&](State& state) {
    if (WaitForSingleObject(start, 1000) != WAIT_OBJECT_0) { timeout = true; return; }
    SetLastError(kSentinel);
    if (registry.Register(Table(state))) ++winners;
    if (GetLastError() != kSentinel) timeout = true;
  };
  std::thread one(worker, std::ref(first)), two(worker, std::ref(second));
  SetEvent(start); JoinBounded(one); JoinBounded(two); CloseHandle(start);
  Check(!timeout && winners == 1 && registry.registered() && registry.failed() &&
        registry.failure_count() == 1 && !registry.admitting(),
        "registration race publishes exactly one table and losing registration fails admissions closed");
  SetLastError(kSentinel);
  Check(registry.End({kTicket, sw::Kind::kWait, sw::Result::kSuccess, {}}) &&
        first.ends + second.ends == 1 && GetLastError() == kSentinel,
        "race never produces replacement/mixed table for End");
}
[[maybe_unused]] void InFlightDisable() {
  State state; sw::Registry registry;
  state.entered = CreateEventW(nullptr, TRUE, FALSE, nullptr);
  state.continue_callback = CreateEventW(nullptr, TRUE, FALSE, nullptr);
  if (!state.entered || !state.continue_callback) std::abort();
  registry.Register(Table(state)); std::atomic<uint64_t> returned{0}; std::atomic<bool> last_error_ok{false};
  std::thread worker([&] {
    SetLastError(kSentinel); returned = registry.Begin(Wait()); last_error_ok = GetLastError() == kSentinel;
  });
  const auto entered = WaitForSingleObject(state.entered, 1000);
  registry.Disable(); SetEvent(state.continue_callback); JoinBounded(worker);
  Check(entered == WAIT_OBJECT_0 && !state.gate_timeout && returned == kTicket && last_error_ok &&
        !registry.admitting() && !registry.failed(), "Disable is not drain: already admitted callback can finish");
  SetLastError(kSentinel);
  Check(registry.End({returned.load(), sw::Kind::kWait, sw::Result::kTimeout, {}}) && state.ends == 1 &&
        registry.Begin(Wait()) == 0 && state.begins == 1 && GetLastError() == kSentinel,
        "in-flight ticket completes through original table without new admissions");
  CloseHandle(state.entered); CloseHandle(state.continue_callback);
}
} // namespace
int main() {
  CommonAndCompileOff();
  if constexpr (sw::kCompiledEnabled) {
    ImmutableAndDisable(); CallbackFailures(); ValidationAndUnsupported(); RegistrationRace(); InFlightDisable();
  }
  std::cout << "semaphore witness transport " << (sw::kCompiledEnabled ? "ON" : "OFF") << ": "
            << checks << " checks, " << failures << " failures\n";
  return failures ? 1 : 0;
}
