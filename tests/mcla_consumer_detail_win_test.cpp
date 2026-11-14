// Include-source, synthetic scalar contexts only: no game/GPU/input/files.
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
namespace a = mcla::consumer_detail_win;
namespace d = a::detail;
namespace core = mcla::consumer_detail;
using Json = nlohmann::json;
constexpr char token[] = "0123456789abcdef0123456789abcdef";
std::atomic<uint64_t> ticks{100}, calls{};
thread_local d::Access access{};
thread_local const PPCContext* bound_context{};
thread_local const uint8_t* bound_base = reinterpret_cast<uint8_t*>(1);
uint32_t version = 1;
bool phase_enabled = true, router_enabled = true;
unsigned checks{}, failures{};
void Check(bool value, const char* name) { ++checks; if (!value) { ++failures; std::cerr << "FAIL " << name << '\n'; } }
template<class F> void Rejects(F&& f, const char* name) {
  bool threw = false; try { f(); } catch (...) { threw = true; } Check(threw, name);
}
void Clobber() { SetLastError(909); _mm_setcsr(_mm_getcsr() ^ 0x6000); }
uint64_t Clock() { Clobber(); return ticks.fetch_add(1); }
uint64_t ZeroClock() { Clobber(); return 0; }
uint64_t ThrowClock() { Clobber(); throw 71; }
d::Access ThrowAccess(const PPCContext&, const uint8_t*) { Clobber(); throw 72; }
d::Access FreezeDuringAccess(const PPCContext&, const uint8_t*) { a::Freeze(token); return access; }
PPCContext Context() {
  PPCContext c{}; c.lr = core::kCaller; c.r3.u64 = 0xfedcba9812345678ull;
  c.f1.u64 = 0x7ff80000abcdef01ull; c.r5.u64 = 33; return c;
}
void Bind(const PPCContext& c, uint64_t serial = 1) {
  access = {}; access.status = mcla::router_dispatch_win::ConsumerStatus::kValid;
  access.invocation = {42, serial}; access.update = {99, 77, GetCurrentThreadId()};
  access.r3 = c.r3.u64; access.f1_bits = c.f1.u64;
  access.ctr = 0xabcdef00822c7870ull; access.requested_target_u32 = core::kTarget;
}
struct Fixture {
  std::unique_ptr<d::State> state = d::Allocate(100, 1000000, 100000, token);
  Fixture() {
    ticks = 100; calls = 0; d::local = {}; d::active = nullptr; d::disarmed = false;
    state->clock = Clock; d::configured.store(state.get()); access = {};
    bound_context = nullptr; bound_base = reinterpret_cast<uint8_t*>(1);
  }
  ~Fixture() { d::configured.store(nullptr); d::local = {}; d::active = nullptr; }
};
// No guest implementation copied: just exercise observations around lambdas.
void Synthetic(PPCContext& c, bool wrapped, bool nonzero = true, int throw_slot = -1, bool early = false) {
  const auto body = [&](a::ConsumerScope* scope) {
    c.r11.u64 = nonzero ? 0xffffffff00000001ull : 0xffffffff00000000ull;
    if (scope) scope->Branch();
    if (early) throw 80; // Unobserved original can throw before any selected child.
    if (!nonzero) { c.r5.u64 += 10; SetLastError(222); return; }
    for (uint8_t i = 0; i < 3; ++i) {
      c.lr = core::kChildReturnLr[i]; c.r3.u64 = 100+i;
      c.f1.u64 = 0x8000000000000000ull+i; c.ctr.u64 = 0xfedcba0081234560ull+i;
      const auto original = [&] {
        ++calls; c.r5.u64 += i+1; c.r3.u64 += 5; SetLastError(300+i);
        _mm_setcsr((_mm_getcsr() & ~0x6000u) | (uint32_t(i) << 13));
        if (int(i) == throw_slot) throw 81+int(i);
      };
      if (scope) scope->Child(i, original); else original();
    }
  };
  if (wrapped) { a::ConsumerScope scope(c, reinterpret_cast<uint8_t*>(1)); body(&scope); }
  else body(nullptr);
}
void Preservation(bool nonzero = true, int throw_slot = -1, bool early = false) {
  auto expected = Context(), actual = expected; Bind(actual);
  const auto initial = _mm_getcsr(); SetLastError(11);
  int e1 = 0, e2 = 0;
  try { Synthetic(expected, false, nonzero, throw_slot, early); } catch (int e) { e1 = e; }
  const auto err = GetLastError(); const auto csr = _mm_getcsr();
  SetLastError(11); _mm_setcsr(initial); const auto before = calls.load();
  try { Synthetic(actual, true, nonzero, throw_slot, early); } catch (int e) { e2 = e; }
  Check(std::memcmp(&expected, &actual, sizeof(actual)) == 0, "full original PPCContext preserved");
  Check(GetLastError() == err && _mm_getcsr() == csr, "original LastError and MXCSR preserved");
  Check(e1 == e2, "original exception unchanged");
  Check(calls-before == (!nonzero || early ? 0u : throw_slot < 0 ? 3u : uint64_t(throw_slot+1)), "original child called exactly once");
  _mm_setcsr(initial);
}
Json Cut() { return Json::parse(a::Freeze(token)); }

a::ActiveChild ReadChecked(PPCContext& c, const uint8_t* base = reinterpret_cast<uint8_t*>(1)) {
  PPCContext before; std::memcpy(&before, &c, sizeof(c));
  auto* state = d::configured.load();
  const auto journal = state ? d::Serialize(*state) : std::string{};
  const auto tick = ticks.load(); const auto csr = _mm_getcsr(); SetLastError(765);
  const auto result = a::ReadActiveChild(c, base);
  const bool host_preserved = GetLastError() == 765 && _mm_getcsr() == csr;
  Check(std::memcmp(&before, &c, sizeof(c)) == 0, "child accessor preserves every context byte");
  Check(host_preserved, "child accessor preserves LastError/MXCSR even parent read clobbers");
  Check(ticks == tick, "child accessor never samples clock");
  Check(!state || d::Serialize(*state) == journal, "child accessor allocates no IDs/events/counters and preserves pending");
  if (!result.valid()) Check(result.entry == core::Entry{} && result.child == core::Child{} && result.child_index == 0,
                            "invalid child accessor only exposes status");
  return result;
}
template<class F> void WithChild(F&& f) {
  Fixture fixture; auto c = Context(); Bind(c); bound_context = &c;
  a::ConsumerScope scope(c, reinterpret_cast<uint8_t*>(1)); c.r11.u64 = 2; scope.Branch();
  for (uint8_t i = 0; i != 3; ++i) {
    c.lr = core::kChildReturnLr[i]; c.r3.u64 = 0xffffffffbdf9d620ull;
    c.f1.u64 = 0x8000000000000000ull; c.ctr.u64 = 0xffffffff8258d1a0ull;
    scope.Child(i, [&] { f(fixture, c, i); });
  }
}
void AccessorTests() {
  { auto c = Context(); Check(ReadChecked(c).status == a::ChildStatus::kAbsent, "absent parent never inferred"); }
  WithChild([](Fixture&, PPCContext& c, uint8_t index) {
    const auto result = ReadChecked(c);
    if (index != 0) {
      Check(result.status == a::ChildStatus::kWrongStage, "accessor is narrowly child0 only"); return;
    }
    Check(result.valid() && result.child_index == 0 && result.entry.invocation == core::InvocationId{1,1} &&
          result.entry.router == core::InvocationId{42,1} && result.entry.update.phase_call_id == 99,
          "child0 saved independent detail/router/Update identities");
    Check(result.child.r3 == c.r3.u64 && result.child.f1_bits == c.f1.u64 &&
          result.child.indirect_ctr == c.ctr.u64 && result.child.requested_target == 0x8258d1a0,
          "child0 full saved operands copied without guest memory");
    c.r3.u64 ^= 0xdead; c.f1.u64 = 0x7ff80000abcdef01ull; c.ctr.u64 = 1; c.lr = 2;
    const auto again = ReadChecked(c);
    Check(again.valid() && again.entry == result.entry && again.child == result.child,
          "mutable callback arguments never substituted for saved parent operands");
    auto detached = result; detached.entry.r3 = 0; detached.child.r3 = 0;
    Check(detached.child.r3 == 0 && ReadChecked(c).child.r3 == result.child.r3, "returned child snapshot detached from model storage");
  });
  {
    Fixture fixture; auto c = Context(); Bind(c);
    a::ConsumerScope scope(c, reinterpret_cast<uint8_t*>(1));
    Check(ReadChecked(c).status == a::ChildStatus::kWrongStage, "entry without active child rejected");
    c.r11.u64 = 0; scope.Branch();
    Check(ReadChecked(c).status == a::ChildStatus::kWrongStage, "zero branch never invents child0");
  }
  WithChild([](Fixture& f, PPCContext& c, uint8_t index) {
    if (index) return;
    auto other = Context();
    Check(ReadChecked(other).status == a::ChildStatus::kLaneMismatch, "wrong context rejects");
    Check(ReadChecked(c, nullptr).status == a::ChildStatus::kLaneMismatch, "null base rejects");
    Check(ReadChecked(c, reinterpret_cast<uint8_t*>(2)).status == a::ChildStatus::kLaneMismatch, "wrong base rejects");
    const auto tid = d::local.lane->tid; d::local.lane->tid = tid+1;
    Check(ReadChecked(c).status == a::ChildStatus::kLaneMismatch, "wrong recorded native TID rejects");
    d::local.lane->tid = tid;
    const auto local = d::local; d::local = {};
    Check(ReadChecked(c).status == a::ChildStatus::kLaneMismatch, "wrong TLS lane cannot borrow identity");
    d::local = local;
    d::configured = nullptr;
    Check(ReadChecked(c).status == a::ChildStatus::kLaneMismatch, "different configuration cannot borrow identity");
    d::configured = f.state.get();
    std::atomic<bool> rejected{false};
    std::thread worker([&] { rejected = a::ReadActiveChild(c, reinterpret_cast<uint8_t*>(1)).status == a::ChildStatus::kAbsent; });
    worker.join(); Check(rejected, "another thread cannot read parent TLS scope");
  });
  for (unsigned field = 0; field != 7; ++field) WithChild([&](Fixture&, PPCContext& c, uint8_t index) {
    if (index) return;
    const auto saved = access;
    if (field == 0) access.r3 ^= 1;
    if (field == 1) access.f1_bits ^= 1;
    if (field == 2) access.ctr ^= 1ull << 40;
    if (field == 3) access.invocation.serial++;
    if (field == 4) access.update.phase_call_id++;
    if (field == 5) access.update.native_thread_id++;
    if (field == 6) access = {};
    Check(ReadChecked(c).status == a::ChildStatus::kBindingMismatch, "current parent router drift rejects detached binding");
    access = saved;
  });
  WithChild([](Fixture& f, PPCContext& c, uint8_t index) {
    if (index) return;
    f.state->stopped = true;
    { a::ConsumerScope rejected(c, reinterpret_cast<uint8_t*>(1));
      f.state->stopped = false;
      Check(ReadChecked(c).status == a::ChildStatus::kUnadmitted, "unadmitted lexical recursion masks outer active child");
    }
    Check(ReadChecked(c).valid(), "restored lexical parent valid after rejected shadow exits");
    auto inner = Context(); const auto saved = access; const auto saved_context = bound_context;
    Bind(inner, 2); bound_context = &inner;
    { a::ConsumerScope nested(inner, reinterpret_cast<uint8_t*>(1));
      Check(ReadChecked(inner).status == a::ChildStatus::kWrongStage, "admitted nested entry never searches parent child");
      Check(ReadChecked(c).status == a::ChildStatus::kLaneMismatch, "outer context not borrowed through nested scope");
      inner.r11.u64 = 0; nested.Branch();
    }
    access = saved; bound_context = saved_context;
    Check(ReadChecked(c).valid(), "nested scope unwind restores exact parent binding");
  });
  for (unsigned mode = 0; mode != 6; ++mode) WithChild([&](Fixture& f, PPCContext& c, uint8_t index) {
    if (index) return;
    if (mode == 0) f.state->failed = true;
    if (mode == 1) f.state->clock_failed = true;
    if (mode == 2) f.state->saturated = true;
    if (mode == 3) f.state->emergency_failure = true;
    if (mode == 4) d::local.lane->model.Freeze();
    if (mode == 5) d::local.lane->model.Branch({1,1}, 2);
    Check(ReadChecked(c).status == a::ChildStatus::kUnhealthy, "sticky state/model health blocks accessor");
  });
  for (unsigned mode = 0; mode != 3; ++mode) WithChild([&](Fixture& f, PPCContext& c, uint8_t index) {
    if (index) return;
    if (mode == 0) f.state->stopped = true;
    if (mode == 1) a::Disarm();
    if (mode == 2) a::Freeze(token);
    Check(ReadChecked(c).status == a::ChildStatus::kStopped, "stopped/disarmed/frozen parent never leased");
  });
  WithChild([](Fixture& f, PPCContext& c, uint8_t index) {
    if (index) return;
    f.state->read_consumer = FreezeDuringAccess;
    const auto result = a::ReadActiveChild(c, reinterpret_cast<uint8_t*>(1));
    Check(result.status == a::ChildStatus::kStopped && result.entry == core::Entry{} && result.child == core::Child{},
          "freeze during parent snapshot checked again under detail mutex without nested lock");
    const auto cut = a::Freeze(token);
    Check(ReadChecked(c).status == a::ChildStatus::kStopped && a::Freeze(token) == cut,
          "post-freeze accessor cannot backfill immutable parent journal");
  });
  WithChild([](Fixture& f, PPCContext& c, uint8_t index) {
    if (index) return;
    f.state->read_consumer = ThrowAccess;
    const auto events = d::local.lane->model.size(); const auto tick = ticks.load();
    PPCContext before; std::memcpy(&before, &c, sizeof(c));
    const auto csr = _mm_getcsr(); SetLastError(876);
    const auto result = a::ReadActiveChild(c, reinterpret_cast<uint8_t*>(1));
    const bool preserved = GetLastError() == 876 && _mm_getcsr() == csr;
    Check(result.status == a::ChildStatus::kObserverException && result.entry == core::Entry{} && result.child == core::Child{},
          "unexpected parent accessor exception clears all payload");
    Check(preserved && std::memcmp(&before, &c, sizeof(c)) == 0, "exception accessor preserves context and host state");
    Check(f.state->failed && f.state->errors == 1 && d::local.lane->model.size() == events && ticks == tick,
          "unexpected accessor exception records failure without event ID or clock allocation");
    f.state->read_consumer = mcla::router_dispatch_win::ReadActiveConsumer;
  });
}
}
extern "C" uint32_t MclaConsumerDetailCallsiteVersion() noexcept { return version; }
namespace mcla::phase_census { bool Enabled() { return phase_enabled; } }
namespace mcla::router_dispatch_win {
bool Enabled() { return router_enabled; }
ActiveConsumer ReadActiveConsumer(const PPCContext& c, const uint8_t* base) noexcept {
  Clobber(); return (!bound_context || bound_context == &c) && base == bound_base ? access : ActiveConsumer{};
}
}

int main() {
  AccessorTests();
  for (unsigned field = 0; field != 3; ++field) {
    Fixture f; auto c = Context(); Bind(c);
    { a::ConsumerScope scope(c, reinterpret_cast<uint8_t*>(1));
      if (field == 0) access.r3 ^= 1;
      if (field == 1) access.f1_bits ^= 1;
      if (field == 2) access.ctr ^= 1ull << 40;
      c.r11.u64 = 0; scope.Branch();
    }
    const auto j = Cut();
    Check(j["observer_failed"] == true && j["counts"]["binding_mismatches"] == 2 &&
          j["threads"][0]["events"][1]["meta"]["binding_matches"] == false,
          "same identity cannot silently change saved dispatch operands");
  }
  Check(!a::Enabled(), "default disabled");
  Rejects([] { a::Freeze(token); }, "unconfigured cut rejects");
  Preservation(); Preservation(false); Preservation(true, 1);
  for (int mode = -2; mode < 4; ++mode) {
    Fixture f; const bool early = mode == 3, nonzero = mode != -2;
    Preservation(nonzero, mode >= 0 && mode < 3 ? mode : -1, early);
    const auto j = Cut(); const auto& lane = j["threads"][0];
    Check(!j["observer_failed"].get<bool>() && lane["model_fault"] == 0 && lane["pending_count"] == 0,
          "completed selected routes and exceptions healthy");
    Check(j["counts"]["admitted_entries"] == 1 && j["counts"]["dropped_entries"] == 0, "entry inventory");
    Check(lane["events"][0]["frame"]["entry"]["router"]["serial"] == 1 &&
          lane["router_thread_epoch"] == 42, "detail/router namespaces explicit");
    Check(lane["events"][1]["frame"]["branch_nonzero"] == nonzero, "branch low32 only");
    if (mode == -1) {
      const auto& frame = lane["events"].back()["frame"];
      Check(frame["returned_children"] == 3 && frame["children"][0]["indirect_ctr"] == 0xfedcba0081234560ull,
            "three children with full indirect CTR");
      Check(frame["children"][1]["indirect_ctr"] == 0 && frame["children"][1]["requested_target"] == 0x82588428 &&
            frame["children"][2]["requested_target"] == 0x82476238, "direct target is instruction operand not stale CTR");
      Check(frame["children"][0]["f1_bits"] == 0x8000000000000000ull && frame["children"][2]["r3"] == 102,
            "actual child pre arguments not original target arguments");
    }
    if (early) Check(lane["events"].size() == 3 && lane["events"].back()["event"] == 6,
                     "unobserved early exception creates no fake child");
    for (const auto* key : {"native_admitted", "coverage_proven", "actual_execution_association_proven",
         "resolved_child_target_proven", "consumer_internals_proven", "simulation_proven", "state_coherence_proven",
         "object_lifetime_proven", "frame_to_step_mapping_proven", "fps_proven"}) Check(j[key] == false, "proofs false");
  }
  for (int mode = 0; mode < 8; ++mode) {
    Fixture f; auto c = Context(), other = Context(); Bind(c);
    if (mode == 0) access = {};
    if (mode == 1) bound_context = &other;
    if (mode == 2) bound_base = reinterpret_cast<uint8_t*>(2);
    if (mode == 3) access.update.native_thread_id++;
    if (mode == 4) access.r3++;
    if (mode == 5) access.f1_bits++;
    if (mode == 6) c.lr++;
    if (mode == 7) { access.ctr++; access.requested_target_u32++; }
    Synthetic(c, true); const auto j = Cut();
    Check(calls == 3 && j["observer_failed"] == true && j["counts"]["dropped_entries"] == 1 &&
          j["rejected_entries"].size() == 1 && j["threads"].empty(), "bad entry preserved rejected without fabricated ID");
    Check(j["rejected_entries"][0]["entry"]["invocation"]["serial"] == 0, "rejected pre-admission ID remains zero");
  }
  {
    Fixture f; auto c = Context(); Bind(c);
    { a::ConsumerScope s(c, reinterpret_cast<uint8_t*>(1), 0x1234); c.r11.u64 = 0; s.Branch(); }
    Check(Cut()["rejected_entries"][0]["entry"]["actual_target"] == 0x1234, "wrong actual target retained");
  }
  for (int mode = 0; mode < 3; ++mode) {
    Fixture f; if (mode == 0) f.state->clock = ZeroClock;
    if (mode == 1) f.state->clock = ThrowClock;
    if (mode == 2) f.state->read_consumer = ThrowAccess;
    Preservation(); const auto j = Cut();
    Check(j["observer_failed"] == true && j["threads"].empty(), "observer failures never skip originals");
    if (mode < 2) Check(j["clock_failed"] == true, "clock fault explicit");
    else Check(j["counts"]["observation_errors"] == 1, "throwing accessor explicit error");
  }
  {
    Fixture f; auto c = Context(); Bind(c);
    { a::ConsumerScope s(c, reinterpret_cast<uint8_t*>(1)); access.update.phase_call_id++;
      c.r11.u64 = 0; s.Branch(); }
    const auto j = Cut();
    Check(j["observer_failed"] == true && j["counts"]["binding_mismatches"] == 2 &&
      j["threads"][0]["events"][1]["meta"]["observed_consumer"]["update"]["phase_call_id"] == 100,
      "later mismatching phase raw metadata retained no false binding");
  }
  {
    Fixture f; auto c = Context(); Bind(c); Synthetic(c, true, false);
    c = Context(); Synthetic(c, true, false); const auto j = Cut();
    Check(j["observer_failed"] == true && j["threads"][0]["model_fault"] == 1 &&
          j["counts"]["admitted_entries"] == 1 && j["counts"]["dropped_entries"] == 1,
          "sequential duplicate router invocation rejected after prior exit");
  }
  {
    Fixture f; auto c = Context(); Bind(c);
    { a::ConsumerScope outer(c, reinterpret_cast<uint8_t*>(1));
      { a::ConsumerScope duplicate(c, reinterpret_cast<uint8_t*>(1));
        Check(d::active == &duplicate, "rejected recursive duplicate shadows outer");
        duplicate.Child(0, [&] { ++calls; }); }
      Check(d::active == &outer, "duplicate child restores lexical outer"); c.r11.u64 = 0; outer.Branch(); }
    Check(calls == 1 && Cut()["observer_failed"] == true, "recursive duplicate original once and no accepted association");
  }
  {
    Fixture f; auto c = Context(); Bind(c);
    { a::ConsumerScope outer(c, reinterpret_cast<uint8_t*>(1)); const auto parent = access;
      auto child = Context(); Bind(child, 2); Synthetic(child, true, false); access = parent;
      c.r11.u64 = 0; outer.Branch(); }
    const auto j = Cut();
    Check(j["observer_failed"] == false && j["threads"][0]["events"][1]["frame"]["model_parent"]["serial"] == 1,
          "nested distinct router invocation then restored outer accepted");
  }
  {
    Fixture f; auto c = Context(); Bind(c); ticks = 99;
    { a::ConsumerScope outer(c, reinterpret_cast<uint8_t*>(1));
      Check(d::active == &outer, "prewindow scope shadows independently");
      Synthetic(c, true, false); Check(d::active == &outer, "admitted child restores prewindow parent"); }
    const auto j = Cut(); Check(j["counts"]["outside_before_entries"] == 1 &&
      j["counts"]["admitted_entries"] == 1 && j["threads"][0]["events"][0]["meta"]["parent_admitted"] == false,
      "unobserved ancestry not borrowed");
  }
  {
    Fixture f; ticks = f.state->end; Preservation();
    Check(Cut()["counts"]["outside_after_entries"] == 1, "end exclusive only for new admissions");
  }
  {
    Fixture f;
    for (size_t i = 0; i < d::kRejected+2; ++i) { auto c = Context(); Synthetic(c, true, false); }
    const auto j = Cut(); Check(j["rejected_entries"].size() == d::kRejected && j["counts"]["rejection_overflow"] == 2 &&
       j["observer_failed"] == true, "bounded rejection inventory overflow explicit");
  }
  {
    Fixture f;
    for (size_t i = 0; i < d::kEvents/3+2; ++i) { auto c = Context(); Bind(c, i+1); Synthetic(c, true, false); }
    Check(f.state->failed && f.state->lanes[0]->model.size() == d::kEvents &&
      f.state->lanes[0]->model.fault() == core::Fault::kCapacity, "fixed event capacity sticky");
  }
  {
    Fixture f; uint64_t serial = 0;
    auto recurse = [&](auto&& self, unsigned depth) -> void {
      auto c = Context(); const auto previous = access; Bind(c, ++serial);
      { a::ConsumerScope s(c, reinterpret_cast<uint8_t*>(1));
        if (depth) self(self, depth-1);
        c.r11.u64 = 0; s.Branch(); s.Child(0, [&] { ++calls; }); }
      access = previous;
    };
    recurse(recurse, 8);
    Check(calls == 9 && d::active == nullptr && f.state->lanes[0]->model.fault() == core::Fault::kDepth,
          "depth overflow independent lexical guard all originals once");
  }
  {
    Fixture f;
    for (size_t i = 0; i < d::kLanes+1; ++i) {
      std::thread worker([] { auto c = Context(); Bind(c); Synthetic(c, true); }); worker.join();
    }
    Check(calls == 3*(d::kLanes+1) && f.state->used == d::kLanes && f.state->failed,
          "bounded thread lifetimes no original loss");
  }
  {
    Fixture f; std::mutex mutex; std::condition_variable cv; bool ready = false, resume = false;
    std::thread worker([&] {
      auto c = Context(); Bind(c);
      { a::ConsumerScope s(c, reinterpret_cast<uint8_t*>(1)); c.r11.u64 = 1; s.Branch(); c.lr = core::kChildReturnLr[0];
        s.Child(0, [&] {
          ++calls; std::unique_lock lock(mutex); ready = true; cv.notify_all();
          if (!cv.wait_for(lock, std::chrono::seconds(5), [&] { return resume; })) std::abort();
          SetLastError(444);
        }); }
      if (GetLastError() != 444) std::abort();
    });
    { std::unique_lock lock(mutex); if (!cv.wait_for(lock, std::chrono::seconds(5), [&] { return ready; })) std::abort(); }
    const auto text = a::Freeze(token); const auto j = Json::parse(text); a::Disarm();
    Check(j["threads"][0]["pending_count"] == 1 && j["threads"][0]["pending"][0]["stage"] == 2,
          "live cut retains actual pending child");
    { std::lock_guard lock(mutex); resume = true; } cv.notify_all(); worker.join();
    Check(calls == 1 && a::Freeze(token) == text, "late normal return cannot backfill cut");
  }
  {
    Fixture f; auto c = Context(); Bind(c);
    { a::ConsumerScope s(c, reinterpret_cast<uint8_t*>(1)); const auto text = a::Freeze(token);
      c.r11.u64 = 0; s.Branch(); s.Child(2, [&] { ++calls; });
      Check(a::Freeze(token) == text && calls == 1, "frozen unadmitted transition still original once"); }
  }
  {
    Fixture f; std::mutex mutex; std::condition_variable cv; size_t ready = 0; bool start = false;
    std::atomic<unsigned> completed{0}; std::array<std::thread, 4> workers;
    for (size_t i = 0; i < workers.size(); ++i) workers[i] = std::thread([&, i] {
      { std::unique_lock lock(mutex); ++ready; cv.notify_all();
        if (!cv.wait_for(lock, std::chrono::seconds(5), [&] { return start; })) std::abort(); }
      for (uint64_t n = 1; n <= 32; ++n) {
        auto c = Context(); Bind(c, n); access.invocation.thread_epoch = 42+i;
        Synthetic(c, true); completed.fetch_add(1);
      }
    });
    { std::unique_lock lock(mutex);
      if (!cv.wait_for(lock, std::chrono::seconds(5), [&] { return ready == workers.size(); })) std::abort(); start = true;
    } cv.notify_all();
    const auto deadline = std::chrono::steady_clock::now()+std::chrono::seconds(5);
    while (!completed.load()) { if (std::chrono::steady_clock::now() >= deadline) std::abort(); std::this_thread::yield(); }
    const auto text = a::Freeze(token); a::Disarm();
    for (auto& worker : workers) worker.join();
    const auto j = Json::parse(text);
    Check(calls == 4*32*3 && a::Freeze(token) == text, "concurrent admissions and events race cut without original loss or backfill");
    Check(j["observer_failed"] == false && j["clock_failed"] == false, "concurrent cut retains healthy completed or pending inventory");
  }
  Check(rex::cvar::SetFlagByName("mcla_consumer_detail_probe", "true"), "explicit init-only opt in");
  LARGE_INTEGER qpf{}; Check(QueryPerformanceFrequency(&qpf) != FALSE, "QPF available"); const auto freq = uint64_t(qpf.QuadPart);
  d::disarmed = false; version = 0; Rejects([&] { a::Begin(100, 200, freq, token); }, "sentinel required"); version = 1;
  Rejects([&] { a::Begin(100, 200, freq+1, token); }, "actual QPF required");
  Rejects([&] { a::Begin(100, 101+12*freq, freq, token); }, "window bound");
  Rejects([&] { a::Begin(100, 200, freq, "bad"); }, "token bound");
  phase_enabled = false; Rejects([&] { a::Begin(100, 200, freq, token); }, "phase prerequisite"); phase_enabled = true;
  router_enabled = false; Rejects([&] { a::Begin(100, 200, freq, token); }, "router prerequisite"); router_enabled = true;
  a::Begin(100, 200, freq, token); Rejects([&] { a::Begin(100, 200, freq, token); }, "single configuration");
  Rejects([] { a::Freeze("bad"); }, "wrong token does not cut"); Check(!d::configured.load()->frozen, "still live after wrong token");
  SetLastError(777); const auto csr = _mm_getcsr(); a::Disarm(); const auto final = a::Freeze(token);
  Check(GetLastError() == 777 && _mm_getcsr() == csr && a::Freeze(token) == final, "public cut/disarm preserve host state");
  Rejects([&] { a::Begin(100, 200, freq, token); }, "terminal disarm");
  std::cout << "consumer detail adapter: " << checks << " checks, " << failures << " failures\n";
  return failures ? 1 : 0;
}
