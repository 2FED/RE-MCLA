// Include-source, synthetic contexts/lambdas only. No game, GPU, files or input.
#include "../src/mcla_router_dispatch_win.cpp"
#include "mcla_no_phase_boundary_stub.h"
#include <nlohmann/json.hpp>
#include <chrono>
#include <condition_variable>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <thread>

namespace { uint32_t callsite_version = 1; }
extern "C" uint32_t MclaRouterDispatchCallsiteVersion() noexcept { return callsite_version; }

namespace {
namespace adapter = mcla::router_dispatch_win;
namespace d = adapter::detail;
using Json = nlohmann::json;
constexpr char token[] = "0123456789abcdef0123456789abcdef";
thread_local mcla::phase_census::ActiveUpdate binding{};
thread_local const PPCContext* bound_context{};
thread_local const uint8_t* bound_base = reinterpret_cast<uint8_t*>(1);
std::atomic<uint64_t> ticks{100}, original_calls{};
bool phase_enabled = true;
unsigned checks{}, failures{};
void Check(bool value, const char* label) {
  ++checks; if (!value) { ++failures; std::cerr << "FAIL " << label << '\n'; }
}
template<class F> void Reject(F&& operation, const char* label) {
  bool rejected = false; try { operation(); } catch (...) { rejected = true; }
  Check(rejected, label);
}
void ClobberObserverState() { SetLastError(900); _mm_setcsr(_mm_getcsr() ^ 0x6000); }
uint64_t Clock() { ClobberObserverState(); return ticks.fetch_add(1); }
uint64_t ZeroClock() { ClobberObserverState(); return 0; }
uint64_t ThrowClock() { ClobberObserverState(); throw 55; }
d::Binding ThrowBinding(const PPCContext&, const uint8_t*) { ClobberObserverState(); throw 56; }
struct Fixture {
  std::unique_ptr<d::State> state;
  Fixture() : state(d::Allocate(100, 1000000, 100000, token)) {
    ticks.store(100); original_calls.store(0);
    d::local = {}; d::active = nullptr; d::disarmed.store(false);
    state->clock = Clock; d::configured.store(state.get());
    binding = {99, 77, GetCurrentThreadId()}; bound_context = nullptr;
    bound_base = reinterpret_cast<uint8_t*>(1);
  }
  ~Fixture() { d::configured.store(nullptr); d::local = {}; d::active = nullptr; }
};
PPCContext Context() {
  PPCContext ctx{}; ctx.lr = 0x822C24C4; ctx.r3.u64 = 0xffffffff12345678;
  ctx.r4.u64 = 0xeeeeeeee00000001; ctx.f1.u64 = 0x7ff80000deadbeef;
  ctx.r5.u64 = 99; return ctx;
}
// Synthetic observer exercise, not a copy of any guest implementation.
void Synthetic(PPCContext& ctx, bool observed, uint64_t result = 1, int throws = 0) {
  const auto predicate = [&] {
    ++original_calls; ctx.r3.u64 = result; ++ctx.r5.u64;
    SetLastError(321); _mm_setcsr((_mm_getcsr() & ~0x6000u) | 0x2000u);
    if (throws == 1) throw 41;
  };
  const auto consumer = [&] {
    ++original_calls; ctx.r5.u64 += 5; SetLastError(456);
    _mm_setcsr((_mm_getcsr() & ~0x6000u) | 0x4000u);
    if (throws == 2) throw 42;
  };
  const auto body = [&](adapter::RouterScope* scope) {
    ctx.r3.u64 = 0x1111111100000010; ctx.r10.u64 = 0x2222222200000020;
    ctx.ctr.u64 = 0xaaaaaaaa82123450;
    if (scope) scope->PredicateDispatch(predicate); else predicate();
    if ((ctx.r3.u64 & 0xff) == 0) { if (scope) scope->PredicateSkip(); return; }
    ctx.r3.u64 = 0x3333333300000030; ctx.r11.u64 = 0x4444444400000040;
    ctx.ctr.u64 = 0xbbbbbbbb82234560; ctx.f1.u64 = 0x8000000000000000;
    if (scope) scope->ConsumerDispatch(consumer); else consumer();
  };
  if (observed) { adapter::RouterScope scope(ctx, reinterpret_cast<uint8_t*>(1)); body(&scope); }
  else body(nullptr);
}
void Preservation(uint64_t result, int throws) {
  auto expected = Context(), actual = expected;
  const auto initial_csr = _mm_getcsr();
  int bare_exception = 0, wrapped_exception = 0;
  SetLastError(77);
  try { Synthetic(expected, false, result, throws); } catch (int code) { bare_exception = code; }
  const auto expected_error = GetLastError();
  const auto expected_csr = _mm_getcsr();
  SetLastError(77); _mm_setcsr(initial_csr);
  const auto before = original_calls.load();
  try { Synthetic(actual, true, result, throws); } catch (int code) { wrapped_exception = code; }
  const auto actual_error = GetLastError();
  const auto actual_csr = _mm_getcsr();
  Check(std::memcmp(&actual, &expected, sizeof(actual)) == 0, "full original PPCContext effects preserved");
  Check(actual_error == expected_error && actual_csr == expected_csr, "original LastError and MXCSR preserved");
  Check(wrapped_exception == bare_exception, "same original exception propagates");
  Check(original_calls-before == ((result & 0xff) && throws != 1 ? 2u : 1u), "original dispatches exactly once");
  _mm_setcsr(initial_csr);
}
Json Cut() { return Json::parse(adapter::Freeze(token)); }
void ActiveConsumerTests() {
  using S = adapter::ConsumerStatus;
  auto* base = reinterpret_cast<uint8_t*>(1);
  const auto empty = [](const adapter::ActiveConsumer& value) {
    return !value.valid() && value.invocation == mcla::router_dispatch::InvocationId{} &&
      value.update == mcla::router_dispatch::UpdateBinding{} && !value.r3 &&
      !value.f1_bits && !value.ctr && !value.requested_target_u32;
  };
  auto bare = Context();
  Check(adapter::ReadActiveConsumer(bare, base).status == S::kAbsent, "accessor absent by default");
  const auto exercise = [&](auto&& body) {
    auto ctx = Context();
    adapter::RouterScope scope(ctx, base);
    Check(adapter::ReadActiveConsumer(ctx, base).status == S::kWrongStage, "accessor excludes router entry");
    scope.PredicateDispatch([&] {
      Check(adapter::ReadActiveConsumer(ctx, base).status == S::kWrongStage, "accessor excludes predicate");
      ctx.r3.u64 = 1;
    });
    Check(adapter::ReadActiveConsumer(ctx, base).status == S::kWrongStage, "accessor excludes predicate return");
    ctx.r3.u64 = 0xabcdef0012345678ull;
    ctx.f1.u64 = 0x7ff80000deadbeefull; ctx.ctr.u64 = 0xfedcba98822c7870ull;
    scope.ConsumerDispatch([&] { body(ctx); });
  };
  {
    Fixture fixture;
    exercise([&](PPCContext& ctx) {
      // Actual body registers may already differ; accessor returns dispatch snapshot.
      ctx.r3.u64 = 17; ctx.f1.u64 = 0x8000000000000000ull; ctx.ctr.u64 = 99;
      const auto copy = ctx; const auto csr = _mm_getcsr(); SetLastError(771);
      const auto before = fixture.state->lanes[0]->model.size(); const auto clock = ticks.load();
      const auto value = adapter::ReadActiveConsumer(ctx, base);
      Check(GetLastError() == 771 && _mm_getcsr() == csr && std::memcmp(&copy, &ctx, sizeof(ctx)) == 0,
            "accessor preserves full PPCContext LastError MXCSR");
      Check(value.valid() && value.invocation.thread_epoch == 1 && value.invocation.serial == 1 &&
            value.update.phase_call_id == 99 && value.update.thread_epoch == 77 &&
            value.update.native_thread_id == GetCurrentThreadId(), "accessor exact detached identities");
      Check(value.r3 == 0xabcdef0012345678ull && value.f1_bits == 0x7ff80000deadbeefull &&
            value.ctr == 0xfedcba98822c7870ull && value.requested_target_u32 == 0x822c7870,
            "accessor saved full raw bits and explicit target truncation");
      Check(fixture.state->lanes[0]->model.size() == before && ticks.load() == clock,
            "accessor does not mint event clock or invocation");
      auto other = Context();
      for (const auto& result : {adapter::ReadActiveConsumer(other, base),
                               adapter::ReadActiveConsumer(ctx, reinterpret_cast<uint8_t*>(2)),
                               adapter::ReadActiveConsumer(ctx, nullptr)})
        Check(result.status == S::kLaneMismatch && empty(result), "accessor wrong lane clears payload");
      const auto saved = binding; binding.phase_call_id++;
      Check(adapter::ReadActiveConsumer(ctx, base).status == S::kBindingMismatch, "accessor changed exact phase rejects");
      binding = saved; binding.native_thread_id++;
      Check(adapter::ReadActiveConsumer(ctx, base).status == S::kBindingMismatch, "accessor changed phase TID rejects");
      binding = saved;
      const auto tid = d::local.lane->tid; d::local.lane->tid++;
      Check(adapter::ReadActiveConsumer(ctx, base).status == S::kLaneMismatch, "accessor stored lane TID rejects");
      d::local.lane->tid = tid;
      adapter::ActiveConsumer foreign;
      std::thread worker([&] { foreign = adapter::ReadActiveConsumer(ctx, base); }); worker.join();
      Check(foreign.status == S::kAbsent && empty(foreign), "accessor cannot transfer lexical scope across threads");
      ticks.store(fixture.state->end);
      { adapter::RouterScope shadow(ctx, base);
        const auto result = adapter::ReadActiveConsumer(ctx, base);
        Check(result.status == S::kUnadmitted && empty(result), "unadmitted nested scope masks active outer consumer"); }
      Check(adapter::ReadActiveConsumer(ctx, base).invocation == value.invocation, "shadow exit restores exact outer consumer");
      try { adapter::RouterScope shadow(ctx, base); throw 73; } catch (int) {}
      Check(adapter::ReadActiveConsumer(ctx, base).valid(), "exception restores lexical consumer");
    });
    Check(adapter::ReadActiveConsumer(bare, base).status == S::kAbsent, "router exit removes accessor binding");
  }
  for (int mode = 0; mode < 7; ++mode) {
    Fixture fixture;
    exercise([&](PPCContext& ctx) {
      if (mode == 0) fixture.state->failed = true;
      if (mode == 1) fixture.state->emergency_failure.store(true);
      if (mode == 2) d::local.lane->model.Freeze();
      if (mode == 3) { (void)adapter::Freeze(token); }
      if (mode == 4) adapter::Disarm();
      if (mode == 5) {
        const auto id = adapter::ReadActiveConsumer(ctx, base).invocation;
        Check(d::local.lane->model.ConsumerReturn(id) && d::local.lane->model.Exit(id, true),
              "test seam empties model with live lexical guard");
      }
      if (mode == 6) fixture.state->binding = ThrowBinding;
      const auto copy = ctx; const auto csr = _mm_getcsr(); SetLastError(772);
      const auto result = adapter::ReadActiveConsumer(ctx, base);
      Check(empty(result), "health cut empty-top or exception never exposes saved IDs");
      Check(result.status == (mode < 3 ? S::kUnhealthy : mode < 5 ? S::kStopped :
                              mode == 5 ? S::kLaneMismatch : S::kObserverException),
            "accessor explicit failure status");
      Check(GetLastError() == 772 && _mm_getcsr() == csr && std::memcmp(&copy, &ctx, sizeof(ctx)) == 0,
            "accessor failures preserve PPCContext LastError MXCSR");
      if (mode == 6) {
        fixture.state->binding = mcla::phase_census::ReadActiveUpdate;
        Check(adapter::ReadActiveConsumer(ctx, base).status == S::kUnhealthy,
              "throwing accessor leaves sticky observer fault");
      }
    });
  }
  {
    Fixture fixture;
    exercise([&](PPCContext& ctx) {
      const auto outer = adapter::ReadActiveConsumer(ctx, base);
      { auto child = Context(); adapter::RouterScope nested(child, base);
        Check(adapter::ReadActiveConsumer(ctx, base).status == S::kLaneMismatch,
              "admitted nested scope cannot expose outer lane");
        Check(adapter::ReadActiveConsumer(child, base).status == S::kWrongStage,
              "admitted nested nonconsumer does not borrow outer stage");
        nested.PredicateDispatch([&] { child.r3.u64 = 0; }); nested.PredicateSkip(); }
      Check(adapter::ReadActiveConsumer(ctx, base).invocation == outer.invocation,
            "admitted child restores parent consumer identity");
      // Explicit corrupt-transition seam: accessor must reject model health,
      // even if the top frame still describes an active consumer.
      Check(!d::local.lane->model.Skip(outer.invocation), "test seam creates sticky transition fault");
      Check(adapter::ReadActiveConsumer(ctx, base).status == S::kUnhealthy,
            "accessor rejects sticky model fault");
    });
  }
  {
    Fixture fixture; auto ctx = Context(); adapter::RouterScope scope(ctx, base);
    scope.PredicateDispatch([&] { ctx.r3.u64 = 1; }); scope.ConsumerDispatch([] {});
    Check(adapter::ReadActiveConsumer(ctx, base).status == S::kWrongStage,
          "returned consumer is no longer active");
  }
  {
    Fixture fixture; auto ctx = Context(); ctx.lr = 0x1234;
    adapter::RouterScope scope(ctx, base);
    scope.PredicateDispatch([&] { ctx.r3.u64 = 1; });
    scope.ConsumerDispatch([&] {
      const auto result = adapter::ReadActiveConsumer(ctx, base);
      Check(result.status == S::kUnexpectedRoute && empty(result), "unknown router route never exposes consumer binding");
    });
  }
  {
    Fixture fixture;
    exercise([&](PPCContext& ctx) {
      const auto text = adapter::Freeze(token);
      for (int n = 0; n < 3; ++n) Check(empty(adapter::ReadActiveConsumer(ctx, base)), "repeated postcut accessor unbound");
      Check(adapter::Freeze(token) == text, "accessor cannot mutate frozen evidence");
    });
  }
}
}
namespace mcla::phase_census {
bool Enabled() { return phase_enabled; }
ActiveUpdate ReadActiveUpdate(const PPCContext& ctx, const uint8_t* base) noexcept {
  ClobberObserverState();
  return (!bound_context || bound_context == &ctx) && base == bound_base ? binding : ActiveUpdate{};
}
}
int main() {
  Check(!adapter::Enabled(), "default off");
  Reject([] { adapter::Freeze(token); }, "unconfigured freeze rejects");
  ActiveConsumerTests();
  Preservation(1, 0); Preservation(0xffffffffffffff00, 0); Preservation(1, 1); Preservation(1, 2);
  for (const int mode : {0, 1, 2}) {
    Fixture fixture; Preservation(1, mode);
    const auto data = Cut(); const auto& lane = data["threads"][0];
    Check(lane["pending_count"] == 0 && lane["model_fault"] == 0, "complete normal/exception path");
    Check(data["observer_failed"] == false && data["clock_failed"] == false, "healthy observations");
    Check(data["counts"]["original_exceptions"] == (mode ? 1 : 0), "exception count");
    const auto& first = lane["events"][0]["frame"];
    Check(first["caller_lr"] == 0x822C24C4 && first["entry_binding"]["phase_call_id"] == 99,
          "exact entry route and update ID");
    Check(first["selector_r4"] == 0xeeeeeeee00000001ull && first["entry_f1_bits"] == 0x7ff80000deadbeefull,
          "opaque bits retained");
    if (mode != 1) {
      const auto& record = lane["events"][3]["frame"];
      Check(record["predicate"]["r3"] != record["consumer"]["r3"] &&
            record["predicate"]["ctr"] != record["consumer"]["ctr"], "reloaded consumer not assumed same object/target");
      Check(record["predicate"]["requested_target_u32"] == 0x82123450 &&
            record["consumer"]["f1_bits"] == 0x8000000000000000ull, "actual low32 target and raw f1");
    }
    for (const auto* key : {"native_admitted", "coverage_proven", "resolved_target_proven", "consumer_internals_proven",
         "simulation_proven", "state_coherence_proven", "object_lifetime_proven", "frame_to_step_mapping_proven", "fps_proven"})
      Check(data[key] == false, "proof flag false");
    const auto text = adapter::Freeze(token); Preservation(1, 0);
    Check(adapter::Freeze(token) == text, "frozen evidence immune to late calls");
  }
  {
    Fixture fixture; Preservation(0xffffffffffffff00, 0);
    const auto data = Cut(); const auto& events = data["threads"][0]["events"];
    Check(events.size() == 5 && events[3]["event"] == 4 && events[3]["frame"]["predicate_result_low8"] == 0,
          "explicit low8 false skip, not full64 truth");
    Check(events[4]["frame"]["consumer"]["present"] == false, "skip cannot claim consumer dispatch");
  }
  for (int mode = 0; mode < 5; ++mode) {
    Fixture fixture; auto ctx = Context(), other = Context();
    if (mode == 0) binding = {};
    if (mode == 1) bound_context = &other;
    if (mode == 2) bound_base = reinterpret_cast<uint8_t*>(2);
    if (mode == 3) binding.native_thread_id++;
    if (mode == 4) ctx.lr = 0x821315a4;
    Synthetic(ctx, true);
    const auto data = Cut();
    Check(original_calls == 2, "unbound/wrong lane/unknown route original once");
    Check(data["counts"][mode == 4 ? "unexpected_route_entries" : "unbound_entries"] == 1,
          "unknown binding or route is explicit");
  }
  {
    Fixture fixture; auto ctx = Context();
    { adapter::RouterScope scope(ctx, reinterpret_cast<uint8_t*>(1));
      scope.PredicateDispatch([&] { ++original_calls; binding.phase_call_id++; ctx.r3.u64 = 0; });
      scope.PredicateSkip(); }
    const auto data = Cut();
    Check(data["observer_failed"] == true && data["counts"]["binding_mismatches"] == 3,
          "changed exact binding cannot be borrowed");
    Check(data["threads"][0]["events"][2]["observed_binding"]["phase_call_id"] == 100,
          "mismatching raw identity preserved");
  }
  {
    Fixture fixture; auto ctx = Context();
    { adapter::RouterScope outer(ctx, reinterpret_cast<uint8_t*>(1));
      outer.PredicateDispatch([&] {
        auto child = Context(); const auto parent_binding = binding;
        binding.phase_call_id = 123; Synthetic(child, true, 0); binding = parent_binding;
        ctx.r3.u64 = 0;
      }); outer.PredicateSkip(); }
    const auto data = Cut(); const auto& events = data["threads"][0]["events"];
    Check(events[2]["frame"]["invocation"]["serial"] == 2 &&
          events[2]["frame"]["model_parent"]["serial"] == 1, "nested lexical IDs");
    Check(events[2]["frame"]["entry_binding"]["phase_call_id"] == 123 &&
          events.back()["observed_binding"]["phase_call_id"] == 99, "nested update then parent restoration");
    Check(data["observer_failed"] == false, "normal nested adapter healthy");
  }
  {
    Fixture fixture; auto ctx = Context(); ticks.store(99);
    { adapter::RouterScope outer(ctx, reinterpret_cast<uint8_t*>(1));
      Check(d::active == &outer, "outside-window guard still lexical");
      auto child = Context(); Synthetic(child, true, 0);
      Check(d::active == &outer, "admitted child restores unadmitted parent");
      outer.PredicateDispatch([&] { ++original_calls; ctx.r3.u64 = 0; }); outer.PredicateSkip(); }
    const auto data = Cut();
    Check(data["counts"]["outside_before_entries"] == 1 && data["counts"]["admitted_entries"] == 1,
          "outside parent never contributes fake model ticket");
    Check(data["threads"][0]["events"][0]["parent_admitted"] == false, "unobserved ancestry explicit");
  }
  {
    Fixture fixture; ticks.store(fixture.state->end); Preservation(1, 0);
    const auto data = Cut();
    Check(data["counts"]["outside_after_entries"] == 1 && data["threads"].empty(),
          "end boundary excludes new admission, not original execution");
  }
  for (const auto clock : {ZeroClock, ThrowClock}) {
    Fixture fixture; fixture.state->clock = clock; Preservation(1, 0);
    const auto data = Cut();
    Check(data["observer_failed"] == true && data["clock_failed"] == true && data["threads"].empty(),
          "clock failures no fake event and original survives");
  }
  {
    Fixture fixture; fixture.state->binding = ThrowBinding; Preservation(1, 0);
    const auto data = Cut(); Check(data["observer_failed"] == true && data["counts"]["observation_errors"] == 1,
                                  "throwing observer contained");
  }
  {
    Fixture fixture; auto ctx = Context();
    { adapter::RouterScope scope(ctx, reinterpret_cast<uint8_t*>(1));
      scope.PredicateDispatch([&] {
        ticks.store(10); ++original_calls; ctx.r3.u64 = 0;
      }); scope.PredicateSkip(); }
    Check(Cut()["clock_failed"] == true, "regressing real-observation seam flagged");
  }
  {
    Fixture fixture; auto ctx = Context();
    auto recurse = [&](auto&& self, unsigned depth) -> void {
      auto child = Context(); adapter::RouterScope scope(child, reinterpret_cast<uint8_t*>(1));
      scope.PredicateDispatch([&] { ++original_calls; if (depth) self(self, depth-1); child.r3.u64 = 0; });
      scope.PredicateSkip();
    };
    recurse(recurse, 8);
    Check(original_calls == 9, "depth overflow every original once");
    Check(d::active == nullptr, "depth overflow all lexical guards restored");
    const auto data = Cut();
    Check(data["observer_failed"] == true && data["threads"][0]["model_fault"] == 4 &&
          data["threads"][0]["pending_count"] == 8, "overflow leaves explicit pending not invented returns");
    (void)ctx;
  }
  {
    Fixture fixture;
    for (size_t i = 0; i < d::kEvents/5+2; ++i) { auto ctx = Context(); Synthetic(ctx, true, 0); }
    Check(original_calls == d::kEvents/5+2, "capacity exhaustion original once per invocation");
    const auto data = Cut(); Check(data["observer_failed"] == true && data["threads"][0]["event_count"] == d::kEvents &&
      data["threads"][0]["model_fault"] == 3, "fixed event capacity explicit failure");
  }
  {
    Fixture fixture;
    for (size_t i = 0; i < d::kLanes+1; ++i) {
      std::thread worker([&] { binding = {200, 88, GetCurrentThreadId()}; auto ctx = Context(); Synthetic(ctx, true, 0); });
      worker.join();
    }
    const auto data = Cut();
    Check(data["threads"].size() == d::kLanes && data["observer_failed"] == true && original_calls == d::kLanes+1,
          "bounded thread lifetimes including possible native TID reuse");
    for (size_t i = 0; i < d::kLanes; ++i)
      Check(data["threads"][i]["thread_epoch"] == i+1, "distinct lifetime identity");
  }
  {
    Fixture fixture;
    std::mutex mutex; std::condition_variable cv; bool entered = false, resume = false;
    std::thread worker([&] {
      binding = {200, 88, GetCurrentThreadId()}; auto ctx = Context();
      { adapter::RouterScope scope(ctx, reinterpret_cast<uint8_t*>(1));
        scope.PredicateDispatch([&] {
          ++original_calls;
          std::unique_lock lock(mutex); entered = true; cv.notify_all();
          if (!cv.wait_for(lock, std::chrono::seconds(5), [&] { return resume; })) std::abort();
          ctx.r3.u64 = 0; SetLastError(765);
        }); scope.PredicateSkip(); }
      if (GetLastError() != 765) std::abort();
    });
    { std::unique_lock lock(mutex);
      if (!cv.wait_for(lock, std::chrono::seconds(5), [&] { return entered; })) std::abort(); }
    const auto text = adapter::Freeze(token); const auto data = Json::parse(text);
    Check(data["threads"][0]["pending_count"] == 1 && data["threads"][0]["pending"][0]["stage"] == 1,
          "live freeze holds actual pending predicate without waiting original");
    adapter::Disarm();
    { std::lock_guard lock(mutex); resume = true; } cv.notify_all(); worker.join();
    Check(original_calls == 1 && adapter::Freeze(token) == text, "late return never backfills live cut");
  }
  {
    Fixture fixture;
    std::mutex mutex; std::condition_variable cv; size_t ready = 0; bool start = false;
    std::atomic<unsigned> started{0}; std::array<std::thread, 4> workers;
    for (size_t i = 0; i < workers.size(); ++i) workers[i] = std::thread([&, i] {
      binding = {300+i, 90+i, GetCurrentThreadId()};
      { std::unique_lock lock(mutex); ++ready; cv.notify_all();
        if (!cv.wait_for(lock, std::chrono::seconds(5), [&] { return start; })) std::abort(); }
      for (unsigned n = 0; n < 64; ++n) {
        auto ctx = Context(); Synthetic(ctx, true, 0); started.fetch_add(1);
      }
    });
    { std::unique_lock lock(mutex);
      if (!cv.wait_for(lock, std::chrono::seconds(5), [&] { return ready == workers.size(); })) std::abort();
      start = true;
    } cv.notify_all();
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
    while (!started.load()) { if (std::chrono::steady_clock::now() >= deadline) std::abort(); std::this_thread::yield(); }
    const auto text = adapter::Freeze(token); adapter::Disarm();
    for (auto& worker : workers) worker.join();
    Check(original_calls == 4*64 && adapter::Freeze(token) == text, "concurrent admission/events race cut without original loss or frozen mutation");
    const auto data = Json::parse(text);
    Check(data["observer_failed"] == false && data["clock_failed"] == false,
          "concurrent first cut retains healthy completed or pending rows");
  }
  Check(rex::cvar::SetFlagByName("mcla_router_dispatch_probe", "true"), "explicit opt-in exists");
  LARGE_INTEGER qpf{}; Check(QueryPerformanceFrequency(&qpf) != FALSE, "QPF available");
  const auto frequency = uint64_t(qpf.QuadPart);
  d::disarmed.store(false);
  callsite_version = 0;
  Reject([&] { adapter::Begin(100, 200, frequency, token); }, "unprepared callsites reject");
  callsite_version = 1;
  Reject([&] { adapter::Begin(100, 200, frequency+1, token); }, "QPF mismatch");
  Reject([&] { adapter::Begin(100, 101+12*frequency, frequency, token); }, "window bound");
  Reject([&] { adapter::Begin(100, 200, frequency, "bad"); }, "token bound");
  phase_enabled = false;
  Reject([&] { adapter::Begin(100, 200, frequency, token); }, "phase prerequisite"); phase_enabled = true;
  adapter::Begin(100, 200, frequency, token);
  Reject([&] { adapter::Begin(100, 200, frequency, token); }, "one-shot configuration");
  Reject([&] { adapter::Freeze("bad"); }, "wrong token leaves live configuration");
  Check(!d::configured.load()->frozen, "wrong token does not create cut");
  SetLastError(777); const auto csr = _mm_getcsr(); adapter::Disarm();
  Check(GetLastError() == 777 && _mm_getcsr() == csr, "Disarm preserves host state");
  Reject([&] { adapter::Begin(100, 200, frequency, token); }, "terminal disarm");
  const auto final = adapter::Freeze(token);
  Check(GetLastError() == 777 && _mm_getcsr() == csr && adapter::Freeze(token) == final, "public immutable cut host-state preservation");
  std::cout << "router adapter: " << checks << " checks, " << failures << " failures\n";
  return failures ? 1 : 0;
}
