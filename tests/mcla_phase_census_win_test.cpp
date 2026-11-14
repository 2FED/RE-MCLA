// Synthetic contexts/mock originals only. Never starts a game, window or GPU.
// Primary owns build/run; this source deliberately includes the private module.
#include "../src/mcla_phase_census_win.cpp"
#include <nlohmann/json.hpp>
#include <condition_variable>
#include <cstring>
#include <iostream>
#include <string_view>
#include <xmmintrin.h>

namespace {
namespace census = mcla::phase_census;
namespace detail = census::detail;
using Json = nlohmann::json;
unsigned checks{}, failures{};
std::atomic<uint64_t> stamp{110};
thread_local detail::State* test_state{};
thread_local unsigned recursion{};
unsigned original_calls{}, wrapper_calls[3]{};
DWORD observed_error{};
bool unwind{}, callback{}, cross_end{}, throw_completion{};
constexpr char token[] = "0123456789abcdef0123456789abcdef";
void Check(bool value, const char* label) {
  ++checks;
  if (!value) { ++failures; std::cerr << "FAIL " << label << '\n'; }
}
template<class Action> void Reject(Action&& action, const char* label) {
  bool threw = false; try { action(); } catch (...) { threw = true; } Check(threw, label);
}
uint64_t FakeClock() { SetLastError(0x9999); return stamp.load(); }
uint64_t ThrowClock() { SetLastError(0x8888); throw std::runtime_error("observer clock"); }
std::unique_ptr<detail::State> State() {
  auto state = std::make_unique<detail::State>();
  state->begin = 100; state->end = 200; state->frequency = 100;
  state->token = token; state->clock = FakeClock;
  stamp.store(110); return state;
}
void Original(PPCContext& context, uint8_t* base) {
  ++original_calls; observed_error = GetLastError();
  context.r3.u64 ^= 0xF000000012345678ull;
  context.r5.u64 ^= 0xA5A5A5A5A5A5A5A5ull;
  context.lr ^= 0x5555; context.f1.u64 ^= 0xCAFE;
  if (base) base[0] ^= 0xAA;
  if (callback && test_state) detail::Observe(test_state, context, base);
  if (cross_end) stamp.store(210);
  if (throw_completion && test_state) test_state->clock = ThrowClock;
  SetLastError(0x12345678);
  if (unwind) throw std::runtime_error("original exception");
}
void Nested(PPCContext& context, uint8_t* base) {
  if (++recursion < 12) detail::Invoke(test_state, detail::Kind::Render, Nested, context, base);
  else detail::Observe(test_state, context, base);
  --recursion;
}
void Child(PPCContext& context, uint8_t* base) { detail::Observe(test_state, context, base); }
void Parent(PPCContext& context, uint8_t* base) {
  detail::Invoke(test_state, detail::Kind::Render, Child, context, base);
  PPCContext different{};
  detail::Observe(test_state, different, base + 1);
}
void ContextTests() {
  for (bool throwing : {false, true}) {
    auto state = State(); test_state = state.get();
    PPCContext baseline{}, observed{}; baseline.r3.u64 = 5; baseline.lr = 0x81234567;
    std::memcpy(&observed, &baseline, sizeof(baseline));
    std::array<uint8_t, 4> baseline_memory{}, observed_memory{};
    unwind = throwing; callback = false; cross_end = false; throw_completion = false;
    const auto calls = original_calls;
    auto call = [&](detail::State* target, PPCContext& ctx, uint8_t* memory) {
      bool caught = false; SetLastError(77);
      try { detail::Invoke(target, detail::Kind::Update, Original, ctx, memory); }
      catch (const std::runtime_error& error) { caught = std::string_view(error.what()) == "original exception"; }
      const auto final_error = GetLastError();
      Check(caught == throwing && observed_error == 77 && final_error == 0x12345678,
            "exact original exception and LastError input/output");
    };
    call(nullptr, baseline, baseline_memory.data());
    call(state.get(), observed, observed_memory.data());
    Check(original_calls == calls + 2, "each original called once");
    Check(std::memcmp(&baseline, &observed, sizeof(baseline)) == 0 && baseline_memory == observed_memory,
          "all original context and memory effects preserved");
    Check(state->used == 1 && state->active == 0 && state->scopes[0].completed &&
          state->scopes[0].exception == throwing && state->scopes[0].lr == 0x81234567 &&
          state->scopes[0].r3 == 5 && detail::local.depth == 0, "entry snapshot and exception completion");
  }
  unwind = false; test_state = nullptr;
}
// Deliberately hostile observer callbacks prove host-state preservation at the
// real Invoke seam; a clean clock would not expose this regression.
constexpr unsigned kHostIncoming = 0x1F80, kHostEntry = 0x3F80,
                   kHostExit = 0x5F80, kHostProduced = 0x7F80;
unsigned host_clock_calls{}, host_clock_failure{}, host_original_calls{}, host_seen_mxcsr{};
DWORD host_seen_error{};
bool host_original_throws{};
uint64_t HostMutatingClock() {
  const auto call = ++host_clock_calls;
  _mm_setcsr(call == 1 ? kHostEntry : kHostExit);
  SetLastError(call == 1 ? 0xAB01 : 0xAB02);
  if (call == host_clock_failure) throw 734;
  return call == 1 ? 110 : 120;
}
void HostStateOriginal(PPCContext& context, uint8_t* base) {
  ++host_original_calls;
  host_seen_error = GetLastError(); host_seen_mxcsr = _mm_getcsr();
  context.r3.u64 ^= 0xF000000012345678ull;
  context.f1.u64 ^= 0xCAFE;
  base[0] ^= 0xAA;
  _mm_setcsr(kHostProduced); SetLastError(0x12345678);
  if (host_original_throws) throw 991;
}
void HostStatePreservationTests() {
  const auto saved_error = GetLastError(); const auto saved_mxcsr = _mm_getcsr();
  for (auto kind : {detail::Kind::Update, detail::Kind::Coordinator, detail::Kind::Render}) {
    for (bool throwing : {false, true}) {
      for (unsigned failed_clock : {0u, 1u, 2u}) {
        auto state = State(); state->clock = HostMutatingClock;
        PPCContext context{}; context.r3.u64 = 5; context.f1.u64 = 7;
        uint8_t memory = 9;
        host_clock_calls = 0; host_clock_failure = failed_clock;
        host_original_throws = throwing; const auto before_calls = host_original_calls;
        int caught = 0;
        _mm_setcsr(kHostIncoming); SetLastError(77);
        try { detail::Invoke(state.get(), kind, HostStateOriginal, context, &memory); }
        catch (int code) { caught = code; }
        const auto final_error = GetLastError(); const auto final_mxcsr = _mm_getcsr();
        Check(host_original_calls == before_calls + 1, "host-state original executes exactly once");
        Check(host_seen_error == 77 && host_seen_mxcsr == kHostIncoming,
              "original sees incoming LastError and MXCSR despite entry observer");
        Check(final_error == 0x12345678 && final_mxcsr == kHostProduced,
              "caller receives original-produced LastError and MXCSR despite exit observer");
        Check(caught == (throwing ? 991 : 0), "exact original unwind preserved; observer exception contained");
        Check(context.r3.u64 == (5 ^ 0xF000000012345678ull) && context.f1.u64 == (7 ^ 0xCAFE) &&
              memory == (9 ^ 0xAA), "host-state preservation leaves original context and memory effects");
        Check(host_clock_calls == (failed_clock == 1 ? 1u : 2u), "hostile clock exercised entry and available exit");
        Check(state->failed.load() == (failed_clock != 0), "hostile clock failure remains diagnostic");
        Check(state->active == 0 && detail::local.depth == 0, "host-state test leaves no active scope or TLS frame");
      }
    }
  }
  _mm_setcsr(saved_mxcsr); SetLastError(saved_error);
}
void FailureTests() {
  PPCContext ctx{}; std::array<uint8_t, 4> memory{};
  for (bool completion : {false, true}) {
    auto state = State(); test_state = state.get(); throw_completion = completion;
    if (!completion) state->clock = ThrowClock;
    const auto calls = original_calls; SetLastError(77);
    detail::Invoke(state.get(), detail::Kind::Coordinator, Original, ctx, memory.data());
    const auto error = GetLastError();
    Check(original_calls == calls + 1 && observed_error == 77 && error == 0x12345678,
          "throwing observer cannot change/skip/repeat original");
    Check(state->failed.load() && detail::local.depth == 0, "observer error explicit; TLS unwound");
    Check(state->active == 0 && state->invalid_qpc, "clock failure recorded without leaking active scope");
    if (completion) Check(state->scopes[0].completed && state->scopes[0].invalid_bracket &&
                          state->scopes[0].exit == 0, "failed completion clock remains explicit invalid record");
  }
  throw_completion = false;
  for (uint64_t time : {0ull, 99ull, 200ull, UINT64_MAX}) {
    auto state = State(); stamp.store(time); test_state = state.get();
    const auto calls = original_calls;
    detail::Invoke(state.get(), detail::Kind::Update, Original, ctx, memory.data());
    Check(original_calls == calls + 1 && state->used == 0, "invalid/outside clock passes through");
    Check(state->invalid_qpc == (time == 0 || time == UINT64_MAX), "invalid clock explicit");
  }
  auto state = State(); test_state = state.get(); cross_end = true;
  detail::Invoke(state.get(), detail::Kind::Update, Original, ctx, memory.data());
  Check(state->scopes[0].exit == 210 && state->scopes[0].completed && state->active == 0,
        "admitted scope completes beyond exclusive end");
  cross_end = false;
  state = State(); test_state = state.get();
  const auto ticket = detail::Enter(state.get(), detail::Kind::Update, ctx, memory.data());
  state->stopped.store(true); stamp.store(109); detail::Leave(ticket, false);
  Check(state->scopes[0].invalid_bracket && state->invalid_qpc && state->active == 0,
        "disarm still completes; backward completion flagged");
  test_state = nullptr;
}
void CallbackIdTests() {
  auto state = State(); PPCContext context{}; uint8_t memory{};
  const auto original_context = context;
  auto observe = [&](detail::State* target) {
    SetLastError(77);
    const auto id = detail::Observe(target, context, &memory);
    Check(GetLastError() == 77, "internal callback return preserves LastError on every path");
    return id;
  };
  Check(observe(nullptr) == 0, "unconfigured callback returns zero");
  stamp.store(99); Check(observe(state.get()) == 0, "pre-window callback returns zero");
  stamp.store(110); Check(observe(state.get()) == 1, "first recorded callback ID one");
  stamp.store(0); Check(observe(state.get()) == 0, "invalid clock returns zero without an ID hole");
  stamp.store(110); Check(observe(state.get()) == 2, "invalid-clock failure did not consume ID");
  state->clock = ThrowClock;
  Check(observe(state.get()) == 0 && state->failed.load(), "throwing observer returns zero and flags failure");
  state->clock = FakeClock;
  Check(observe(state.get()) == 3, "throwing observer did not consume ID");
  stamp.store(200); Check(observe(state.get()) == 0, "exclusive end returns zero");
  stamp.store(110);
  for (uint64_t id = 4; id <= detail::kCallbacks; ++id)
    Check(observe(state.get()) == id, "bounded consecutive callback IDs");
  Check(observe(state.get()) == 0 && state->callback_overflow == 1, "callback capacity returns zero, never wraps");
  state->stopped.store(true);
  const auto frozen = detail::Serialize(*state);
  Check(observe(state.get()) == 0 && detail::Serialize(*state) == frozen,
        "disarm returns zero without record/counter mutation");
  const auto data = Json::parse(frozen);
  Check(data["schema"] == "mcla-phase-census-v3", "callback ID plus lexical provenance schema is v3");
  for (size_t i = 0; i < data["swap_callbacks"].size(); ++i)
    Check(data["swap_callbacks"][i]["callback_id"] == i + 1 &&
          data["swap_callbacks"][i]["swap_token_bound"] == false, "JSON callback IDs unique, not token bindings");
  Check(std::memcmp(&context, &original_context, sizeof(context)) == 0 && memory == 0,
        "callback ID observation has no context/memory effects");
  state = State();
  std::array<detail::Ticket, detail::kDepth + 1> tickets;
  for (auto& ticket : tickets) ticket = detail::Enter(state.get(), detail::Kind::Render, context, &memory);
  Check(observe(state.get()) == 1 && state->callbacks[0].suppressed_scope &&
        !state->callbacks[0].has_scope && state->callbacks[0].call == 0,
        "recorded callback returns ID even when its active scope was suppressed");
  for (size_t i = tickets.size(); i > 0; --i) detail::Leave(tickets[i - 1], false);
  Check(observe(state.get()) == 2 && !state->callbacks[1].suppressed_scope,
        "suppression unwind preserves consecutive callback identity");
}
void NestingAndCapacityTests() {
  PPCContext ctx{}; std::array<uint8_t, 4> memory{};
  auto state = State(); test_state = state.get();
  detail::Invoke(state.get(), detail::Kind::Coordinator, Parent, ctx, memory.data());
  Check(state->used == 2 && state->scopes[1].parent == state->scopes[0].call &&
        state->scopes[1].parent_context_same && state->scopes[1].parent_base_same,
        "nested direct same-thread scopes joined by call ID");
  Check(state->callback_used == 2 && state->callbacks[0].call == state->scopes[1].call &&
        state->callbacks[0].context_same && state->callbacks[0].base_same &&
        state->callbacks[1].call == state->scopes[0].call &&
        !state->callbacks[1].context_same && !state->callbacks[1].base_same,
        "callback active scope and mismatched context/base recorded, not repaired");
  state = State(); test_state = state.get();
  detail::Invoke(state.get(), detail::Kind::Render, Nested, ctx, memory.data());
  Check(state->used == 8 && state->stack_overflow == 4 && state->dropped_scopes == 4 &&
        state->active == 0 && detail::local.depth == 0 && detail::local.suppressed == 0,
        "bounded stack with complete cleanup");
  Check(state->callbacks[0].suppressed_scope && !state->callbacks[0].has_scope && !state->callbacks[0].call,
        "overflow callback not falsely attributed to nearest retained ancestor");
  Check(state->callbacks[0].callback_id == 1,
        "scope suppression retains recorded callback identity, not false scope identity");
  state = State(); test_state = state.get();
  for (size_t i = 0; i < detail::kScopes + 1; ++i)
    detail::Invoke(state.get(), detail::Kind::Update, Child, ctx, memory.data());
  Check(state->used == detail::kScopes && state->scope_overflow == 1 && state->dropped_scopes == 1,
        "scope overflow bounded explicit");
  Check(state->callback_used == detail::kCallbacks && state->callback_overflow == detail::kScopes + 1 - detail::kCallbacks,
        "callback overflow bounded explicit");
  auto json = Json::parse(detail::Serialize(*state));
  Check(json["scopes"].size() == 2048 && json["swap_callbacks"].size() == 1024, "JSON arrays bounded");
  for (const auto* key : {"native_admitted", "simulation_proven", "render_proven", "fps_proven",
                           "frame_to_step_mapping_proven", "causal_join_proven"}) Check(json[key] == false, "no proof flags");
  Check(json["swap_callbacks"][0]["swap_token_bound"] == false, "callback not SDK token identity");
  test_state = nullptr;
}
void ThreadTests() {
  auto state = State();
  // Sequential thread lifetimes deliberately allow Windows TID reuse.
  for (unsigned i = 0; i < 4; ++i) {
    std::thread worker([&] {
      PPCContext context{};
      auto ticket = detail::Enter(state.get(), detail::Kind::Render, context, nullptr);
      detail::Observe(state.get(), context, nullptr);
      detail::Leave(ticket, false);
    }); worker.join();
  }
  Check(state->used == 4 && state->callback_used == 4, "all thread lifetimes recorded");
  for (size_t i = 0; i < 4; ++i) {
    Check(state->callbacks[i].callback_id == i + 1, "callback IDs unique across thread lifetimes");
    Check(state->scopes[i].thread_epoch && state->scopes[i].tid &&
          state->scopes[i].thread_epoch == state->callbacks[i].thread_epoch, "TLS epoch paired to callback");
    for (size_t j = 0; j < i; ++j)
      Check(state->scopes[i].thread_epoch != state->scopes[j].thread_epoch &&
            state->scopes[i].call != state->scopes[j].call, "call IDs and lifetime epochs never reuse TID identity");
  }
}
thread_local census::ActiveUpdate expected_update;
thread_local bool binding_throw{};
void ReadBinding(PPCContext& context, uint8_t* base) {
  const auto error = GetLastError();
  const auto found = census::ReadActiveUpdate(context, base);
  Check(found == expected_update && GetLastError() == error,
        "active update accessor returns exact expected ID without LastError mutation");
  if (binding_throw) throw std::runtime_error("binding original exception");
}
void ReadNestedUpdate(PPCContext& context, uint8_t* base) {
  const auto found = census::ReadActiveUpdate(context, base);
  Check(found.bound() && found.phase_call_id != expected_update.phase_call_id &&
        found.thread_epoch == expected_update.thread_epoch && found.native_thread_id == GetCurrentThreadId(),
        "nested admitted update has its own phase ID, same lifetime epoch");
}
void BindingParent(PPCContext& context, uint8_t* base) {
  auto* state = test_state;
  const auto outer = census::ReadActiveUpdate(context, base);
  Check(outer.bound() && outer.phase_call_id == state->scopes[0].call &&
        outer.thread_epoch == state->scopes[0].thread_epoch && outer.native_thread_id == state->scopes[0].tid,
        "accessor binds directly to admitted immutable phase row");
  const auto callbacks = state->callback_used, scopes = state->used;
  const auto saved_context = context;
  expected_update = outer; SetLastError(77); ReadBinding(context, base);
  PPCContext other{};
  Check(!census::ReadActiveUpdate(other, base).bound() &&
        !census::ReadActiveUpdate(context, base + 1).bound() &&
        !census::ReadActiveUpdate(context, nullptr).bound(), "wrong or null lane is unbound");
  Check(state->used == scopes && state->callback_used == callbacks &&
        std::memcmp(&context, &saved_context, sizeof(context)) == 0,
        "accessor creates no scope, callback, or guest register mutation");
  std::thread other_thread([&] {
    Check(!census::ReadActiveUpdate(context, base).bound(),
          "same pointer on another thread cannot borrow active update");
  }); other_thread.join();
  detail::Invoke(state, detail::Kind::Update, ReadNestedUpdate, context, base);
  ReadBinding(context, base);
  expected_update = {};
  for (auto kind : {detail::Kind::Coordinator, detail::Kind::Render}) {
    detail::Invoke(state, kind, ReadBinding, context, base);
    Check(census::ReadActiveUpdate(context, base) == outer, "non-update nested scope restores parent");
  }
  // Even when census Enter has no ticket, every lexical Invoke masks parent.
  for (bool throwing : {false, true}) {
    binding_throw = throwing;
    bool caught = false;
    try { detail::Invoke(nullptr, detail::Kind::Update, ReadBinding, context, base); }
    catch (const std::runtime_error&) { caught = true; }
    Check(caught == throwing && census::ReadActiveUpdate(context, base) == outer,
          "null-state nested invocation masks and restores parent on normal/exception");
  }
  binding_throw = false;
  for (uint64_t time : {99ull, 200ull, 0ull}) {
    stamp.store(time);
    detail::Invoke(state, detail::Kind::Update, ReadBinding, context, base);
    stamp.store(110);
    Check(census::ReadActiveUpdate(context, base) == outer,
          "outside-window or invalid-clock nested invocation never borrows parent");
  }
  state->failed.store(true);
  Check(!census::ReadActiveUpdate(context, base).bound(), "atomic failed observer masks current binding");
  state->failed.store(false); // Synthetic test only, production never clears failure.
  state->stopped.store(true);
  Check(!census::ReadActiveUpdate(context, base).bound(), "disarm masks current binding");
  detail::Invoke(state, detail::Kind::Update, ReadBinding, context, base);
  state->stopped.store(false); // Synthetic test only.
  expected_update = outer; ReadBinding(context, base);
}
void SuppressedBinding(PPCContext& context, uint8_t* base) {
  ++recursion;
  const auto found = census::ReadActiveUpdate(context, base);
  Check(found.bound() == (recursion <= detail::kDepth),
        "suppressed update does not expose retained ancestor ID");
  if (recursion < detail::kDepth + 3)
    detail::Invoke(test_state, detail::Kind::Update, SuppressedBinding, context, base);
  Check(census::ReadActiveUpdate(context, base) == found, "suppression nesting restores exact lexical binding");
  --recursion;
}
void ActiveUpdateTests() {
  auto state = State(); test_state = state.get(); PPCContext context{}; uint8_t memory[2]{};
  Check(!census::ReadActiveUpdate(context, memory).bound(), "outside invocation no active update");
  detail::Invoke(state.get(), detail::Kind::Update, BindingParent, context, memory);
  Check(!census::ReadActiveUpdate(context, memory).bound() && detail::active_invocation == nullptr,
        "normal outer return clears lexical binding");
  state = State(); test_state = state.get(); expected_update = {}; binding_throw = true;
  Reject([&] { detail::Invoke(nullptr, detail::Kind::Update, ReadBinding, context, memory); },
         "original unwind escapes unchanged");
  binding_throw = false;
  Check(detail::active_invocation == nullptr, "exception outer return clears lexical binding");
  detail::Invoke(state.get(), detail::Kind::Update, SuppressedBinding, context, memory);
  Check(detail::active_invocation == nullptr && detail::local.depth == 0 && detail::local.suppressed == 0,
        "overflow unwind clears both lexical and retained census stacks");
  test_state = nullptr;
}
void DrainTests() {
  using Clock = std::chrono::steady_clock;
  for (unsigned scenario = 0; scenario < 4; ++scenario) {
    auto state = State(); std::stop_source stop;
    if (scenario == 1) stop.request_stop();
    if (scenario == 2) state->active = 1;
    unsigned ticks{}, waits{}; bool rejected = false;
    auto now = [&] {
      ++ticks;
      return Clock::time_point{} + ((scenario == 2 && ticks >= 3) || (scenario == 3 && ticks >= 3)
                                    ? std::chrono::seconds(3) : std::chrono::seconds(0));
    };
    try { (void)detail::Drain(*state, stop.get_token(), now, [&] { ++waits; }); }
    catch (const std::runtime_error&) { rejected = true; }
    Check(rejected == (scenario != 0) && state->stopped.load(), "bounded drain cancellation/timeout/late serialization");
    Check(state->drain_incomplete.load() == (scenario != 0), "drain failure explicit");
    Check(waits == (scenario == 2 ? 1u : 0u), "drain only waits while active");
  }
}
struct HeldControl {
  detail::State* state{};
  std::mutex mutex;
  std::condition_variable changed;
  bool entered{}, drain_waiting{}, release{}, drained{}, throw_original{};
  std::atomic<unsigned> calls{}, timeouts{};
  uint64_t before_disarm_id{}, after_disarm_id{};
};
thread_local HeldControl* held_control{};
void HeldOriginal(PPCContext& context, uint8_t* base) {
  auto& control = *held_control;
  ++control.calls;
  const DWORD incoming = GetLastError();
  context.r3.u64 += 11; base[0] ^= 0xAA;
  control.before_disarm_id = detail::Observe(control.state, context, base);
  {
    std::unique_lock lock(control.mutex);
    control.entered = true; control.changed.notify_all();
    if (!control.changed.wait_for(lock, std::chrono::seconds(1), [&] { return control.release; }))
      ++control.timeouts;
  }
  control.after_disarm_id = detail::Observe(control.state, context, base); // Zero after disarm.
  if (incoming != 77) ++control.timeouts;
  SetLastError(0xABCDEF);
  if (control.throw_original) throw std::runtime_error("held original exception");
}
void ConcurrentHeldDrainTests() {
  for (bool throwing : {false, true}) {
    auto state = State(); HeldControl control; control.state = state.get(); control.throw_original = throwing;
    PPCContext context{}; uint8_t memory{}; DWORD result_error{}; bool caught{};
    std::string receipt; std::exception_ptr drain_error;
    std::thread original([&] {
      held_control = &control; SetLastError(77);
      try { detail::Invoke(state.get(), detail::Kind::Render, HeldOriginal, context, &memory); }
      catch (const std::runtime_error& error) { caught = std::string_view(error.what()) == "held original exception"; }
      result_error = GetLastError(); held_control = nullptr;
    });
    bool entered;
    {
      std::unique_lock lock(control.mutex);
      entered = control.changed.wait_for(lock, std::chrono::seconds(1), [&] { return control.entered; });
    }
    Check(entered, "real original held before concurrent drain");
    std::thread drainer([&] {
      try {
        receipt = detail::Drain(*state, {}, [] { return std::chrono::steady_clock::now(); }, [&] {
          std::unique_lock lock(control.mutex);
          control.drain_waiting = true; control.changed.notify_all();
          if (!control.changed.wait_for(lock, std::chrono::seconds(1), [&] { return control.release; }))
            ++control.timeouts;
          // After release, yield rather than sleeping or holding the original's gate.
          lock.unlock(); std::this_thread::yield();
        });
      } catch (...) { drain_error = std::current_exception(); }
      std::lock_guard lock(control.mutex); control.drained = true; control.changed.notify_all();
    });
    bool waiting, prematurely_drained;
    {
      std::unique_lock lock(control.mutex);
      waiting = control.changed.wait_for(lock, std::chrono::seconds(1),
                                         [&] { return control.drain_waiting || control.drained; });
      prematurely_drained = control.drained;
      control.release = true; control.changed.notify_all();
    }
    Check(waiting && !prematurely_drained, "drain cannot freeze while actual original remains held");
    original.join(); drainer.join();
    Check(!drain_error && control.timeouts == 0 && control.calls == 1,
          "concurrent held original drains without timeout or repetition");
    Check(result_error == 0xABCDEF && caught == throwing && context.r3.u64 == 11 && memory == 0xAA,
          "held original context/LastError/exception preserved");
    Check(control.before_disarm_id == 1 && control.after_disarm_id == 0,
          "held original callback identity before drain, zero after disarm");
    if (!receipt.empty()) {
      const auto data = Json::parse(receipt);
      Check(data["counts"]["active_scopes"] == 0 && data["scopes"].size() == 1 &&
            data["scopes"][0]["completed"] == true && data["scopes"][0]["original_exception"] == throwing,
            "freeze includes held scope completion, including exception");
      Check(data["swap_callbacks"].size() == 1 && data["unobserved_ancestry_possible"] == true,
            "post-disarm callback absent and ancestry limit explicit");
    }
  }
}
struct RaceLocal {
  unsigned calls{}, errors{}, id_count{};
  bool require_zero{};
  std::array<uint64_t, 256> ids{};
};
thread_local RaceLocal* race_local{};
void RememberRaceId(uint64_t id) {
  if (race_local->require_zero && id) ++race_local->errors;
  if (id) {
    if (race_local->id_count == race_local->ids.size()) ++race_local->errors;
    else race_local->ids[race_local->id_count++] = id;
  }
}
void RaceOriginal(PPCContext& context, uint8_t* base) {
  if (GetLastError() != 77) ++race_local->errors;
  ++race_local->calls; ++context.r3.u64; base[0] ^= 1;
  RememberRaceId(census::ObserveSwap(context, base));
  SetLastError(0xFEDCBA);
}
void ConcurrentDisarmFreezeTest(detail::State& state) {
  constexpr unsigned workers = 3, racing_calls = 64, frozen_calls = 16;
  std::mutex gate;
  std::condition_variable changed;
  unsigned ready{};
  bool start{}, frozen{};
  std::atomic<unsigned> timeouts{};
  std::array<RaceLocal, workers> results{};
  const auto callback_baseline = state.callback_used;
  std::array<std::thread, workers> threads;
  for (unsigned i = 0; i < workers; ++i) threads[i] = std::thread([&, i] {
    race_local = &results[i]; PPCContext context{}; uint8_t memory{};
    auto call = [&] {
      SetLastError(77);
      detail::Invoke(&state, detail::Kind::Update, RaceOriginal, context, &memory);
      if (GetLastError() != 0xFEDCBA) ++race_local->errors;
      SetLastError(99); RememberRaceId(census::ObserveSwap(context, &memory));
      if (GetLastError() != 99) ++race_local->errors;
    };
    call(); // At least one real admission per worker before the race.
    {
      std::unique_lock lock(gate); ++ready; changed.notify_all();
      if (!changed.wait_for(lock, std::chrono::seconds(1), [&] { return start; })) ++timeouts;
    }
    for (unsigned n = 0; n < racing_calls; ++n) call();
    {
      std::unique_lock lock(gate);
      if (!changed.wait_for(lock, std::chrono::seconds(1), [&] { return frozen; })) ++timeouts;
    }
    race_local->require_zero = true;
    for (unsigned n = 0; n < frozen_calls; ++n) call();
    if (context.r3.u64 != 1 + racing_calls + frozen_calls) ++race_local->errors;
    race_local = nullptr;
  });
  bool all_ready;
  {
    std::unique_lock lock(gate);
    all_ready = changed.wait_for(lock, std::chrono::seconds(1), [&] { return ready == workers; });
    start = true; changed.notify_all();
  }
  Check(all_ready, "concurrent race workers admitted before shared start");
  // These public operations race with the workers' admissions and callbacks.
  census::Disarm();
  std::string receipt; std::exception_ptr finish_error;
  try { receipt = census::Finish({}, token); } catch (...) { finish_error = std::current_exception(); }
  {
    std::lock_guard lock(gate); frozen = true; changed.notify_all();
  }
  for (auto& thread : threads) thread.join();
  Check(!finish_error && timeouts == 0, "concurrent disarm/freeze finishes without timing gate failure");
  for (const auto& result : results)
    Check(result.calls == 1 + racing_calls + frozen_calls && result.errors == 0,
          "racing and post-freeze originals execute once with intact context/LastError");
  std::array<bool, detail::kCallbacks + 1> seen{};
  size_t returned_ids{};
  for (const auto& result : results) for (unsigned i = 0; i < result.id_count; ++i) {
    const auto id = result.ids[i];
    const bool valid = id > callback_baseline && id <= state.callback_used && id <= detail::kCallbacks;
    Check(valid, "concurrent returned callback ID references a recorded row");
    if (valid) { Check(!seen[id], "concurrent callback return IDs never duplicate"); seen[id] = true; }
    ++returned_ids;
  }
  Check(returned_ids == state.callback_used - callback_baseline,
        "all concurrently admitted callback rows have exactly one returned ID; zeros create no holes");
  if (!receipt.empty()) {
    Check(detail::Serialize(state) == receipt, "no scope/callback or counter mutation after frozen receipt");
    const auto data = Json::parse(receipt);
    Check(data["counts"]["active_scopes"] == 0 && data["counts"]["stored_scopes"] == data["counts"]["completed_scopes"] &&
          data["observer_failed"] == false && data["invalid_qpc"] == false, "race freeze complete and observer healthy");
    for (const auto& row : data["scopes"]) Check(row["completed"] == true, "every race-admitted original completed");
  }
}
void PublicAndWrapperTests() {
  LARGE_INTEGER actual_frequency{};
  Check(QueryPerformanceFrequency(&actual_frequency) && actual_frequency.QuadPart > 0, "actual QPC frequency");
  const uint64_t frequency = uint64_t(actual_frequency.QuadPart);
  Check(!census::Enabled(), "default off");
  Reject([&] { census::Begin(100, 200, frequency, token); }, "disabled Begin rejected");
  PPCContext context{}; uint8_t memory{};
  SetLastError(77); const auto absent_id = census::ObserveSwap(context, &memory); census::Disarm();
  Check(GetLastError() == 77 && absent_id == 0, "unconfigured observer returns zero and preserves LastError");
  Check(rex::cvar::SetFlagByName("mcla_phase_census_probe", "true"), "enable diagnostic only explicitly");
  for (const auto* bad : {"", "ABCDEF01234567890123456789012345", "../012345678901234567890123456789"})
    Reject([&] { census::Begin(100, 200, frequency, bad); }, "malformed token rejected");
  Reject([&] { census::Begin(0, 200, frequency, token); }, "zero begin rejected");
  Reject([&] { census::Begin(100, 101 + frequency * 12, frequency, token); }, "over twelve seconds rejected");
  Reject([&] { census::Begin(100, 200, 0, token); }, "zero frequency rejected");
  Reject([&] { census::Begin(100, 200, frequency + 1, token); }, "actual frequency mismatch rejected");
  census::Begin(100, 200, frequency, token);
  auto* state = detail::configured.load(); state->clock = FakeClock; stamp.store(110);
  Reject([&] { census::Begin(100, 200, frequency, token); }, "no second configuration");
  for (auto* wrapper : {sub_822C1FA8, sub_82305D00, sub_82131008}) {
    SetLastError(77); wrapper(context, &memory);
    Check(GetLastError() == 0x12345678 && observed_error == 77, "strong wrapper native effects preserved");
  }
  Check(wrapper_calls[0] == 1 && wrapper_calls[1] == 1 && wrapper_calls[2] == 1 && state->used == 3,
        "three strong wrappers each forward exactly once");
  SetLastError(77); const auto callback_id = census::ObserveSwap(context, &memory);
  Check(GetLastError() == 77 && callback_id == 1 && state->callback_used == 1 && !state->callbacks[0].has_scope,
        "unscoped public callback remains unscoped");
  ConcurrentDisarmFreezeTest(*state);
  auto data = Json::parse(census::Finish({}, token));
  Check(data["scopes"].size() >= 6 && data["token"] == token && !data["native_admitted"].get<bool>(),
        "public Finish serializes census including concurrent admitted scopes only");
  const auto scopes = state->used, callbacks = state->callback_used;
  sub_822C1FA8(context, &memory); const auto disarmed_id = census::ObserveSwap(context, &memory);
  Check(wrapper_calls[0] == 2 && disarmed_id == 0 && state->used == scopes && state->callback_used == callbacks,
        "disarmed path forwards without admission or callback ID");
  Reject([&] { (void)census::Finish({}, std::string(32, 'f')); }, "Finish token mismatch rejected");
}
}
REX_EXTERN(__imp__sub_822C1FA8) { ++wrapper_calls[0]; Original(ctx, base); }
REX_EXTERN(__imp__sub_82305D00) { ++wrapper_calls[1]; Original(ctx, base); }
REX_EXTERN(__imp__sub_82131008) { ++wrapper_calls[2]; Original(ctx, base); }
int main() {
  ContextTests(); HostStatePreservationTests(); FailureTests(); CallbackIdTests(); NestingAndCapacityTests(); ThreadTests(); DrainTests();
  ActiveUpdateTests(); ConcurrentHeldDrainTests(); PublicAndWrapperTests();
  std::cout << "phase census: " << checks << " checks, " << failures << " failures\n";
  return failures ? 1 : 0;
}
