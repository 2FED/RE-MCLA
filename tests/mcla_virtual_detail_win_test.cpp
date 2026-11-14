// Include-source synthetic host-only tests. No game, GPU, guest memory or files.
#include "../src/mcla_consumer_detail_win.cpp"
#include "mcla_no_phase_boundary_stub.h"
#include <nlohmann/json.hpp>
#include <chrono>
#include <condition_variable>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <thread>

namespace {
namespace a = mcla::virtual_detail_win;
namespace cd = mcla::consumer_detail_win;
namespace d = cd::detail;
namespace core = mcla::virtual_detail;
namespace parent = mcla::consumer_detail;
using Json = nlohmann::json;
constexpr char token[] = "0123456789abcdef0123456789abcdef";
std::atomic<uint64_t> ticks{100}, calls{}, reads{};
thread_local cd::ActiveChild child{};
thread_local mcla::router_dispatch_win::ActiveConsumer router{};
uint32_t version = 1;
unsigned checks{}, failures{};
void Check(bool value, const char* label) {
  ++checks; if (!value) { ++failures; std::cerr << "FAIL " << label << '\n'; }
}
template<class F> void Rejects(F&& fn, const char* label) {
  bool threw = false; try { fn(); } catch (...) { threw = true; } Check(threw, label);
}
void Clobber() { SetLastError(909); _mm_setcsr(_mm_getcsr() ^ 0x6000); }
uint64_t Clock() { Clobber(); return ticks.fetch_add(1); }
uint64_t ZeroClock() { Clobber(); return 0; }
cd::ActiveChild ReadChild(const PPCContext&, const uint8_t*) { ++reads; Clobber(); return child; }
cd::ActiveChild ThrowChild(const PPCContext&, const uint8_t*) { ++reads; Clobber(); throw 47; }
cd::ActiveChild CutDuringRead(const PPCContext&, const uint8_t*) { ++reads; cd::Freeze(token); return child; }
PPCContext Context() {
  PPCContext c{}; c.lr = core::kCaller; c.r3.u64 = 0xfedcba9876543210ull;
  c.f1.u64 = 0x7ff8000012345678ull; c.r5.u64 = 81; return c;
}
void Bind(const PPCContext& c, uint64_t serial = 1, uint64_t lane = 1) {
  child = {}; child.status = cd::ChildStatus::kValid;
  child.entry = {{lane, serial}, {42, serial}, {99, 77, GetCurrentThreadId()},
                 parent::kTarget, parent::kCaller, 0xabcdef, 0x3ff0000000000000ull};
  child.child = {core::kCaller, c.r3.u64, c.f1.u64, 0xabcdef008258d1a0ull,
                 core::kTarget, parent::ChildState::kActive, true};
}
struct Fixture {
  std::unique_ptr<d::State> state;
  explicit Fixture(bool enabled = true, bool lane = true) : state(d::Allocate(100, 1000000, 100000, token, enabled)) {
    ticks = 100; calls = 0; reads = 0; d::local = {}; d::active = nullptr; d::virtual_active = nullptr;
    d::disarmed = false; child = {}; router = {}; state->clock = Clock; state->read_child = ReadChild;
    d::configured.store(state.get()); if (lane) Attach();
  }
  void Attach() {
    state->used = 1; state->lanes[0]->tid = GetCurrentThreadId(); d::local = {state.get(), state->lanes[0].get()};
  }
  ~Fixture() { d::configured.store(nullptr); d::local = {}; d::active = nullptr; d::virtual_active = nullptr; }
};
void Body(PPCContext& c, a::Scope* scope, int throwing = -1, bool early = false, bool late = false) {
  if (early) throw 70;
  for (uint8_t i = 0; i != 3; ++i) {
    c.lr = core::kCallReturnLr[i]; c.r3.u64 = 0x1111000000000000ull+i;
    c.f1.u64 = 0x8000000000000000ull+i; c.ctr.u64 = 0xabcd123482111000ull+i*4;
    auto original = [&] {
      ++calls; c.r5.u64 += i+1; c.f1.u64 ^= 0x0010000000000000ull;
      SetLastError(200+i); _mm_setcsr((_mm_getcsr() & ~0x6000u) | (unsigned(i) << 13));
      if (int(i) == throwing) throw 80+int(i);
    };
    if (scope) scope->Dispatch(i, original); else original();
  }
  if (late) throw 71;
}
void Run(PPCContext& c, bool wrapped = true, int throwing = -1, bool early = false, bool late = false) {
  if (wrapped) { a::Scope scope(c, reinterpret_cast<uint8_t*>(1)); Body(c, &scope, throwing, early, late); }
  else Body(c, nullptr, throwing, early, late);
}
void Preservation(int throwing = -1, bool early = false, bool late = false) {
  PPCContext expected{}, actual{}; auto seed = Context();
  std::memcpy(&expected, &seed, sizeof(seed)); std::memcpy(&actual, &seed, sizeof(seed)); Bind(actual);
  const auto csr = _mm_getcsr(); SetLastError(10); int first = 0, second = 0;
  try { Run(expected, false, throwing, early, late); } catch (int e) { first = e; }
  const auto expected_error = GetLastError(); const auto expected_csr = _mm_getcsr();
  SetLastError(10); _mm_setcsr(csr); const auto before = calls.load();
  try { Run(actual, true, throwing, early, late); } catch (int e) { second = e; }
  Check(std::memcmp(&expected, &actual, sizeof(actual)) == 0, "all PPC bytes equal direct original");
  Check(GetLastError() == expected_error && _mm_getcsr() == expected_csr, "original LastError and MXCSR preserved");
  Check(first == second, "original exception propagated unchanged");
  Check(calls-before == (early ? 0u : throwing < 0 ? 3u : uint64_t(throwing+1)), "each selected original exactly once");
  Check(d::virtual_active == nullptr, "lexical top restored after normal or exceptional exit"); _mm_setcsr(csr);
}
Json Cut() { cd::Freeze(token); return Json::parse(a::Freeze(token)); }
void PreservationAndShape() {
  for (int throwing = -1; throwing < 3; ++throwing) {
    Fixture f; Preservation(throwing); const auto j = Cut();
    Check(j["observer_failed"] == false && j["clock_failed"] == false, "original exception is not observer failure");
    Check(j["counts"]["original_exceptions"] == (throwing < 0 ? 0 : 1), "separate original exception count");
    Check(f.state->original_exceptions == 0, "virtual original exception never changes parent exception count");
    Check(j["threads"][0]["pending_count"] == 0, "completed lexical invocation not pending");
    Check(j["counts"]["admitted_entries"] == 1 && j["counts"]["observed_entries"] == 1, "entry partition");
    Check(j["native_admitted"] == false && j["simulation_proven"] == false &&
          j["resolved_virtual_target_proven"] == false, "no derived proof claims");
    if (throwing < 0) {
      const auto& frame = j["threads"][0]["events"].back()["frame"];
      Check(frame["returned_calls"] == 3 && j["threads"][0]["event_count"] == 8, "three pre-return pairs and lexical boundaries");
      for (unsigned i = 0; i < 3; ++i) {
        Check(frame["calls"][i]["f1_bits"] == 0x8000000000000000ull+i, "independent raw f1 per call");
        Check(frame["calls"][i]["indirect_ctr"] == 0xabcd123482111000ull+i*4 &&
              frame["calls"][i]["requested_target"] == 0x82111000u+i*4, "full CTR plus explicit low32 target");
      }
    }
  }
  for (bool early : {false, true}) {
    Fixture f; Preservation(-1, early, !early); auto j = Cut();
    Check(j["observer_failed"] == false && j["counts"]["original_exceptions"] == 1,
          "unobserved early/late original exception retained without manufactured call");
    Check(j["threads"][0]["event_count"] == (early ? 2 : 8), "early/late exact event count");
  }
}
void Rejections() {
  { Fixture f(false); Preservation(); Check(reads == 0 && !f.state->virtual_inventory, "disabled no allocation or accessor read");
    Rejects([] { a::Freeze(token); }, "disabled subordinate cannot serialize"); }
  { Fixture f; ticks = 1; Preservation(); ticks = 100; auto j = Cut(); Check(j["counts"]["outside_before_entries"] == 1 && j["observer_failed"] == false, "before window no admission"); }
  { Fixture f; ticks = f.state->end; Preservation(); auto j = Cut(); Check(j["counts"]["outside_after_entries"] == 1, "exclusive end admission"); }
  { Fixture f; f.state->read_child = ThrowChild; Preservation(); Check(f.state->failed && f.state->virtual_inventory->errors == 1, "accessor throws contained and parent fails"); }
  { Fixture f; f.state->clock = ZeroClock; Preservation(); auto j = Cut(); Check(j["clock_failed"] == true && j["counts"]["dropped_entries"] == 1, "bad real clock explicit"); }
  { Fixture f(true, false); auto c = Context(); Bind(c); Run(c); const auto j = Cut();
    Check(j["threads"].empty() && j["rejected_entries"].size() == 1 && j["observer_failed"] == true, "no lane header-only rejection never creates parent lane"); }
  for (unsigned bad = 0; bad != 8; ++bad) {
    Fixture f; auto c = Context(); Bind(c);
    if (bad == 0) child.status = cd::ChildStatus::kAbsent;
    if (bad == 1) child.entry.update.native_thread_id ^= 1;
    if (bad == 2) child.child_index = 1;
    if (bad == 3) child.entry.invocation.thread_epoch = 2;
    if (bad == 4) c.r3.u64 ^= 1;
    if (bad == 5) c.f1.u64 ^= 1;
    if (bad == 6) c.lr ^= 1;
    if (bad == 7) child.child.indirect_ctr ^= 1;
    Run(c); auto j = Cut(); Check(j["counts"]["dropped_entries"] == 1 && calls == 3 && j["observer_failed"] == true,
                                "bad route/identity/entry rejects but preserves originals");
  }
  { Fixture f; auto c = Context(); Bind(c);
    { a::Scope scope(c, reinterpret_cast<uint8_t*>(1)); child.child.indirect_ctr ^= 1ull << 40; Body(c, &scope); }
    auto j = Cut(); Check(j["counts"]["binding_mismatches"] == 7 && j["observer_failed"] == true, "full saved child rechecked not just target"); }
  { Fixture f; auto c = Context(); Bind(c);
    { a::Scope scope(c, reinterpret_cast<uint8_t*>(1)); child.entry.r3 ^= 1; Body(c, &scope); }
    Check(Cut()["counts"]["binding_mismatches"] == 7, "full saved parent entry rechecked"); }
  { Fixture f;
    for (size_t i = 0; i < d::kRejected+1; ++i) { auto c = Context(); Run(c); }
    const auto j = Cut(); Check(j["rejected_entries"].size() == d::kRejected && j["counts"]["rejection_overflow"] == 1,
                              "bounded rejected headers no fabricated IDs"); }
}
void NestingAndLimits() {
  { Fixture f; uint64_t serial = 0;
    auto recurse = [&](auto&& self, unsigned depth) -> void {
      auto c = Context(); const auto previous = child; Bind(c, ++serial);
      { a::Scope scope(c, reinterpret_cast<uint8_t*>(1)); if (depth) self(self, depth-1); Body(c, &scope); }
      child = previous;
    };
    recurse(recurse, 2); auto j = Cut();
    Check(j["observer_failed"] == false && j["counts"]["admitted_entries"] == 3 && calls == 9, "nested distinct children restore admitted parent");
    Check(j["threads"][0]["events"][1]["frame"]["model_parent"]["serial"] == 1, "explicit model ancestry"); }
  { Fixture f; auto c = Context(); Bind(c); Run(c); c = Context(); Run(c);
    auto j = Cut(); Check(j["observer_failed"] == true && j["counts"]["admitted_entries"] == 1 && calls == 6,
                         "same child cannot grant a second actual entry"); }
  { Fixture f; auto c = Context(); Bind(c);
    { a::Scope outer(c, reinterpret_cast<uint8_t*>(1));
      { a::Scope rejected(c, nullptr); Check(d::virtual_active == &rejected, "rejected lexical scope shadows parent");
        rejected.Dispatch(0, [&] { ++calls; }); }
      Check(d::virtual_active == &outer, "parent TLS restored after rejected shadow"); Body(c, &outer); }
    Check(calls == 4 && Cut()["observer_failed"] == true, "unadmitted originals not confused with parent"); }
  { Fixture f; uint64_t serial = 0;
    auto recurse = [&](auto&& self, unsigned depth) -> void {
      auto c = Context(); const auto previous = child; Bind(c, ++serial);
      { a::Scope scope(c, reinterpret_cast<uint8_t*>(1)); if (depth) self(self, depth-1); Body(c, &scope); }
      child = previous;
    };
    recurse(recurse, 8);
    Check(calls == 27 && f.state->lanes[0]->virtual_journal->model.fault() == core::Fault::kDepth,
          "depth overflow sticky yet all originals execute once"); }
  { Fixture f;
    for (size_t n = 0; n < d::kEvents/8+1; ++n) { auto c = Context(); Bind(c, n+1); Run(c); }
    Check(f.state->lanes[0]->virtual_journal->model.size() == d::kEvents &&
          f.state->lanes[0]->virtual_journal->model.fault() == core::Fault::kCapacity && f.state->failed,
          "fixed capacity terminal without overwriting prior events"); }
  { Fixture f; auto c = Context(); Bind(c);
    { a::Scope scope(c, reinterpret_cast<uint8_t*>(1)); scope.Dispatch(2, [&] { ++calls; }); }
    Check(f.state->failed && calls == 1 && f.state->lanes[0]->virtual_journal->model.fault() == core::Fault::kTransition,
          "invalid call transition never suppresses original"); }
}
void FreezeTests() {
  { Fixture f; Rejects([] { a::Freeze(token); }, "virtual freeze cannot choose its own cut");
    Check(!f.state->stopped && !f.state->frozen, "premature serialization does not stop admissions"); }
  { Fixture f; f.state->read_child = CutDuringRead; Preservation();
    const auto j = Json::parse(a::Freeze(token)); Check(j["counts"]["observed_entries"] == 0 && j["observer_failed"] == false,
      "accessor snapshot is not admission lease; shared cut wins"); }
  { Fixture f; auto c = Context(); Bind(c); std::string cut;
    { a::Scope scope(c, reinterpret_cast<uint8_t*>(1)); c.lr = core::kCallReturnLr[0];
      scope.Dispatch(0, [&] { ++calls; cd::Freeze(token); cut = a::Freeze(token); }); }
    Check(a::Freeze(token) == cut && Json::parse(cut)["threads"][0]["pending"][0]["stage"] == 1,
          "freeze inside original keeps pending call without late completion"); }
  { Fixture f; auto c = Context(); Bind(c); cd::Disarm(); Run(c);
    Check(reads == 0 && calls == 3 && Cut()["disarmed_at_cut"] == true, "disarm blocks observations not originals"); }
  { Fixture f(true, false); std::mutex mutex; std::condition_variable cv; bool ready = false, resume = false;
    std::thread worker([&] {
      f.Attach(); auto c = Context(); Bind(c);
      { a::Scope scope(c, reinterpret_cast<uint8_t*>(1)); c.lr = core::kCallReturnLr[0];
        scope.Dispatch(0, [&] {
          ++calls; std::unique_lock lock(mutex); ready = true; cv.notify_all();
          if (!cv.wait_for(lock, std::chrono::seconds(5), [&] { return resume; })) std::abort();
          SetLastError(444);
        }); }
      if (GetLastError() != 444 || d::virtual_active != nullptr) std::abort();
    });
    { std::unique_lock lock(mutex); if (!cv.wait_for(lock, std::chrono::seconds(5), [&] { return ready; })) std::abort(); }
    const auto parent_cut = cd::Freeze(token); const auto virtual_cut = a::Freeze(token); cd::Disarm();
    { std::lock_guard lock(mutex); resume = true; } cv.notify_all(); worker.join();
    Check(calls == 1 && cd::Freeze(token) == parent_cut && a::Freeze(token) == virtual_cut,
          "concurrent held original finishes once; neither frozen journal mutates");
    Check(Json::parse(parent_cut)["freeze_qpc"] == Json::parse(virtual_cut)["freeze_qpc"], "one shared actual cut"); }
}
void ActualParentAccessor() {
  Fixture f(true, false); f.state->read_child = cd::ReadActiveChild;
  auto c = Context(); c.lr = parent::kCaller;
  router = {}; router.status = mcla::router_dispatch_win::ConsumerStatus::kValid;
  router.invocation = {42, 1}; router.update = {99, 77, GetCurrentThreadId()};
  router.r3 = c.r3.u64; router.f1_bits = c.f1.u64;
  router.ctr = 0xabcdef00822c7870ull; router.requested_target_u32 = parent::kTarget;
  { cd::ConsumerScope outer(c, reinterpret_cast<uint8_t*>(1)); c.r11.u64 = 1; outer.Branch();
    c.lr = parent::kChildReturnLr[0]; c.ctr.u64 = core::kTarget;
    outer.Child(0, [&] { a::Scope inner(c, reinterpret_cast<uint8_t*>(1)); Body(c, &inner); });
    for (uint8_t i = 1; i != 3; ++i) { c.lr = parent::kChildReturnLr[i]; outer.Child(i, [] {}); }
  }
  const auto j = Cut();
  Check(j["counts"]["admitted_entries"] == 1 && j["observer_failed"] == false,
        "real consumer lexical ChildActive accessor supplies exact parent without deadlock");
  Check(f.state->admitted == 1 && f.state->errors == 0 && f.state->lanes[0]->model.pending_count() == 0,
        "subordinate journal leaves original parent model transitions intact");
}
void CheckCallInvalid(const a::ActiveCall& value, a::CallStatus status, const char* label) {
  Check(value.status == status && !value.valid(), label);
  Check(value.entry == core::Entry{} && value.call == core::Call{} && value.call_index == 0,
        "invalid call accessor has status only");
}
void CallAccessorTests() {
  auto* base = reinterpret_cast<uint8_t*>(1);
  { auto c = Context(); CheckCallInvalid(a::ReadActiveCall(c, base), a::CallStatus::kAbsent, "no lexical call ancestor"); }
  { Fixture f; auto c = Context(); Bind(c); a::Scope scope(c, base);
    CheckCallInvalid(a::ReadActiveCall(c, base), a::CallStatus::kWrongStage, "entry is not active call0");
    for (uint8_t i = 0; i != 3; ++i) {
      c.lr = core::kCallReturnLr[i]; c.ctr.u64 = 0xabcdef0082741010ull;
      c.r3.u64 = 0xf000000000012345ull; c.f1.u64 = 0x8000000000000000ull;
      const auto saved_r3 = c.r3.u64, saved_f1 = c.f1.u64, saved_ctr = c.ctr.u64;
      scope.Dispatch(i, [&] {
        const auto before_ticks = ticks.load();
        const auto size = f.state->lanes[0]->virtual_journal->model.size();
        const auto serial = f.state->lanes[0]->virtual_journal->next_serial;
        const auto before_json = d::SerializeVirtual(*f.state);
        // Real original callees mutate these; the accessor must report the
        // original call operands and must not reject mutable current values.
        c.r3.u64 = 1; c.f1.u64 = 0x7ff80000abcdef01ull; c.ctr.u64 = 9; c.lr = 4;
        PPCContext saved{}; std::memcpy(&saved, &c, sizeof(c));
        SetLastError(731); const auto csr = _mm_getcsr(); const auto got = a::ReadActiveCall(c, base);
        Check(GetLastError() == 731 && _mm_getcsr() == csr && std::memcmp(&saved, &c, sizeof(c)) == 0,
              "accessor preserves complete PPC bytes LastError and MXCSR");
        if (i == 0) {
          Check(got.valid() && got.call_index == 0 && got.call.r3 == saved_r3 && got.call.f1_bits == saved_f1 &&
                got.call.indirect_ctr == saved_ctr && got.call.requested_target == 0x82741010u,
                "exact active call0 saved operands including full CTR and negative zero");
          Check(got.entry.parent_entry == child.entry && got.entry.parent_child == child.child &&
                got.entry.invocation.serial == 1, "detached full virtual entry and parent association");
        } else CheckCallInvalid(got, a::CallStatus::kWrongStage, "later calls cannot masquerade as call0");
        Check(ticks == before_ticks && f.state->lanes[0]->virtual_journal->model.size() == size &&
              f.state->lanes[0]->virtual_journal->next_serial == serial && d::SerializeVirtual(*f.state) == before_json,
              "accessor has no clocks IDs events counters or journal writes");
      });
      CheckCallInvalid(a::ReadActiveCall(c, base), a::CallStatus::kWrongStage, "completed call0 not active");
    }
  }
  for (unsigned bad = 0; bad != 15; ++bad) {
    Fixture f; auto c = Context(); Bind(c); a::Scope scope(c, base);
    c.lr = core::kCallReturnLr[0]; c.ctr.u64 = 0xabcdef0082741010ull;
    scope.Dispatch(0, [&] {
      auto* lane = f.state->lanes[0].get(); const auto old_local = d::local;
      const auto old_child = child; const auto old_tid = lane->tid; const auto old_epoch = lane->epoch;
      a::CallStatus expected = a::CallStatus::kLaneMismatch;
      PPCContext other{}; const PPCContext* supplied = &c; const uint8_t* supplied_base = base;
      if (bad == 0) supplied = &other;
      if (bad == 1) supplied_base = nullptr;
      if (bad == 2) supplied_base = reinterpret_cast<uint8_t*>(2);
      if (bad == 3) d::local = {};
      if (bad == 4) lane->tid ^= 1;
      if (bad == 5) ++lane->epoch;
      if (bad == 6) { child.entry.r3 ^= 1ull << 40; expected = a::CallStatus::kBindingMismatch; }
      if (bad == 7) { child.child.indirect_ctr ^= 1ull << 40; expected = a::CallStatus::kBindingMismatch; }
      if (bad == 8) { child.entry.invocation.thread_epoch += 1; expected = a::CallStatus::kBindingMismatch; }
      if (bad == 9) { child.child.f1_bits ^= 1; expected = a::CallStatus::kBindingMismatch; }
      if (bad == 10) { f.state->failed = true; expected = a::CallStatus::kUnhealthy; }
      if (bad == 11) { f.state->clock_failed = true; expected = a::CallStatus::kUnhealthy; }
      if (bad == 12) { f.state->saturated = true; expected = a::CallStatus::kUnhealthy; }
      if (bad == 13) { f.state->stopped = true; expected = a::CallStatus::kStopped; }
      if (bad == 14) { d::disarmed = true; expected = a::CallStatus::kStopped; }
      const auto count = lane->virtual_journal->model.size(); const auto before_ticks = ticks.load();
      CheckCallInvalid(a::ReadActiveCall(*supplied, supplied_base), expected, "rejected identity/health/stage no fallback");
      Check(ticks == before_ticks && lane->virtual_journal->model.size() == count, "rejection never samples clock or emits event");
      d::local = old_local; child = old_child; lane->tid = old_tid; lane->epoch = old_epoch;
      f.state->failed = f.state->clock_failed = f.state->saturated = false; f.state->stopped = false; d::disarmed = false;
    });
  }
  { Fixture f; auto c = Context(); Bind(c); a::Scope scope(c, base);
    c.lr = core::kCallReturnLr[0]; c.ctr.u64 = 0xabcdef0082741014ull;
    scope.Dispatch(0, [&] { CheckCallInvalid(a::ReadActiveCall(c, base), a::CallStatus::kBindingMismatch,
                                           "other requested target not accepted"); }); }
  { Fixture f; auto c = Context(); Bind(c); a::Scope scope(c, base);
    c.lr = core::kCallReturnLr[0]; c.ctr.u64 = 0x82741010;
    scope.Dispatch(0, [&] {
      { a::Scope rejected(c, nullptr); CheckCallInvalid(a::ReadActiveCall(c, base), a::CallStatus::kUnadmitted,
                                                       "rejected lexical shadow cannot expose active ancestor"); }
      CheckCallInvalid(a::ReadActiveCall(c, base), a::CallStatus::kUnhealthy, "sticky rejected shadow failure not healed");
    }); }
  { Fixture f; auto c = Context(); Bind(c); a::Scope scope(c, base);
    c.lr = core::kCallReturnLr[0]; c.ctr.u64 = 0x82741010;
    scope.Dispatch(0, [&] {
      f.state->read_child = CutDuringRead;
      CheckCallInvalid(a::ReadActiveCall(c, base), a::CallStatus::kStopped, "parent read cut wins without nested mutex");
      const auto frozen = a::Freeze(token);
      CheckCallInvalid(a::ReadActiveCall(c, base), a::CallStatus::kStopped, "frozen accessor rejected");
      Check(a::Freeze(token) == frozen, "frozen payload unchanged after accessor");
    }); }
  { Fixture f; auto c = Context(); Bind(c); a::Scope scope(c, base);
    c.lr = core::kCallReturnLr[0]; c.ctr.u64 = 0x82741010;
    scope.Dispatch(0, [&] {
      f.state->read_child = ThrowChild; const auto size = f.state->lanes[0]->virtual_journal->model.size();
      const auto tick = ticks.load(); SetLastError(919); const auto csr = _mm_getcsr();
      CheckCallInvalid(a::ReadActiveCall(c, base), a::CallStatus::kObserverException, "accessor exception contained");
      Check(f.state->failed && f.state->emergency_failure && f.state->virtual_inventory->errors == 1 &&
            GetLastError() == 919 && _mm_getcsr() == csr, "unexpected exception sticky shared failure and preservation");
      Check(ticks == tick && f.state->lanes[0]->virtual_journal->model.size() == size, "exception no clock/event");
      f.state->read_child = ReadChild;
    }); }
}
void ActualCallAccessor() {
  Fixture f(true, false); f.state->read_child = cd::ReadActiveChild;
  auto c = Context(); c.lr = parent::kCaller;
  router = {}; router.status = mcla::router_dispatch_win::ConsumerStatus::kValid;
  router.invocation = {42, 1}; router.update = {99, 77, GetCurrentThreadId()};
  router.r3 = c.r3.u64; router.f1_bits = c.f1.u64;
  router.ctr = 0xabcdef00822c7870ull; router.requested_target_u32 = parent::kTarget;
  { cd::ConsumerScope outer(c, reinterpret_cast<uint8_t*>(1)); c.r11.u64 = 1; outer.Branch();
    c.lr = parent::kChildReturnLr[0]; c.ctr.u64 = core::kTarget;
    outer.Child(0, [&] {
      a::Scope scope(c, reinterpret_cast<uint8_t*>(1));
      for (uint8_t i = 0; i != 3; ++i) {
        c.lr = core::kCallReturnLr[i]; c.ctr.u64 = 0xabcdef0082741010ull;
        scope.Dispatch(i, [&] { if (!i) Check(a::ReadActiveCall(c, reinterpret_cast<uint8_t*>(1)).valid(),
                                             "actual consumer-to-virtual-to-call accessor chain no nested lock"); });
      }
    });
    for (uint8_t i = 1; i != 3; ++i) { c.lr = parent::kChildReturnLr[i]; outer.Child(i, [] {}); }
  }
  Check(Cut()["observer_failed"] == false, "actual chain accessor leaves journals healthy");
}
} // namespace
extern "C" uint32_t MclaConsumerDetailCallsiteVersion() noexcept { return 1; }
extern "C" uint32_t MclaVirtualDetailCallsiteVersion() noexcept { return version; }
namespace mcla::phase_census { bool Enabled() { return true; } }
namespace mcla::router_dispatch_win {
bool Enabled() { return true; }
ActiveConsumer ReadActiveConsumer(const PPCContext&, const uint8_t*) noexcept { Clobber(); return router; }
}
int main() {
  Check(!a::Enabled(), "virtual runtime option defaults off");
  PreservationAndShape(); Rejections(); NestingAndLimits(); FreezeTests(); ActualParentAccessor();
  CallAccessorTests(); ActualCallAccessor();
  LARGE_INTEGER qpf{}; Check(QueryPerformanceFrequency(&qpf) != FALSE, "QPF available");
  const auto freq = uint64_t(qpf.QuadPart); d::disarmed = false;
  Check(rex::cvar::SetFlagByName("mcla_consumer_detail_probe", "true"), "enable parent explicitly");
  version = 0; cd::Begin(100, 200, freq, token);
  auto* disabled = d::configured.exchange(nullptr); Check(disabled && !disabled->virtual_inventory,
      "virtual runtime OFF does not consult failing sentinel or allocate journals"); delete disabled;
  Check(rex::cvar::SetFlagByName("mcla_virtual_detail_probe", "true"), "separate explicit virtual opt in");
  Rejects([&] { cd::Begin(100, 200, freq, token); }, "virtual sentinel required only when enabled");
  Check(d::configured.load() == nullptr, "sentinel failure publishes no state"); version = 1;
  cd::Begin(100, 200, freq, token); Check(d::configured.load()->virtual_inventory != nullptr, "opt-in preallocates bounded journals");
  Rejects([] { a::Freeze(token); }, "parent cut required on public configured path");
  cd::Freeze(token); const auto before = ticks.load(); const auto saved = a::Freeze(token);
  Rejects([] { a::Freeze("bad"); }, "virtual token must match");
  Check(ticks == before && a::Freeze(token) == saved, "virtual serializer never reads clock or changes cut");
  std::cout << "virtual detail adapter: " << checks << " checks, " << failures << " failures\n";
  return failures ? 1 : 0;
}
