// Whole pre-window Update policy through the real six observer journals.
// Synthetic scalar callbacks only: no game, guest reads, GPU, files or input.
// This is NOT immediate-parent-stage exclusion: every observer independently
// requires the same exact lexical pre-window Update and matching window.
#if defined(MCLA_FRAME_HANDOFF_PROBE)
#error This bounded integration test excludes frame handoff.
#endif
#ifndef MCLA_PHASE_CENSUS_PROBE
#define MCLA_PHASE_CENSUS_PROBE
#endif
#ifndef MCLA_ROUTER_DISPATCH_PROBE
#define MCLA_ROUTER_DISPATCH_PROBE
#endif
#ifndef MCLA_CONSUMER_DETAIL_PROBE
#define MCLA_CONSUMER_DETAIL_PROBE
#endif
#ifndef MCLA_VIRTUAL_DETAIL_PROBE
#define MCLA_VIRTUAL_DETAIL_PROBE
#endif
#ifndef MCLA_WORK_EMISSION_PROBE
#define MCLA_WORK_EMISSION_PROBE
#endif
#ifndef MCLA_VECTOR_TARGET_PROBE
#define MCLA_VECTOR_TARGET_PROBE
#endif
#ifndef MCLA_VECTOR_STORE_PROBE
#define MCLA_VECTOR_STORE_PROBE
#endif
#include "../src/mcla_phase_census_win.cpp"
#include "../src/mcla_router_dispatch_win.cpp"
#include "../src/mcla_consumer_detail_win.cpp"
#include "../src/mcla_work_emission_win.cpp"
#include "../src/mcla_vector_target_win.cpp"
#include "../src/mcla_vector_store_win.cpp"
#include <functional>
#include <iostream>
#include <thread>

extern "C" uint32_t MclaRouterDispatchCallsiteVersion() noexcept { return 1; }
extern "C" uint32_t MclaConsumerDetailCallsiteVersion() noexcept { return 1; }
extern "C" uint32_t MclaVirtualDetailCallsiteVersion() noexcept { return 1; }
extern "C" uint32_t MclaWorkEmissionCallsiteVersion() noexcept { return 1; }
extern "C" uint32_t MclaWorkEmissionTailVersion() noexcept { return 1; }
extern "C" uint32_t MclaVectorTargetCallsiteVersion() noexcept { return 1; }
extern "C" uint32_t MclaVectorStoreCallsiteVersion() noexcept { return 1; }

namespace {
namespace p = mcla::phase_census; namespace pd = p::detail;
namespace r = mcla::router_dispatch_win; namespace rd = r::detail;
namespace c = mcla::consumer_detail_win; namespace cd = c::detail;
namespace v = mcla::virtual_detail_win;
namespace w = mcla::work_emission_win; namespace wd = w::detail;
namespace t = mcla::vector_target_win; namespace td = t::detail;
namespace s = mcla::vector_store_win; namespace sd = s::detail;
namespace cc = mcla::consumer_detail; namespace vc = mcla::virtual_detail;
namespace wc = mcla::work_emission; namespace tc = mcla::vector_target;
constexpr char token[] = "0123456789abcdef0123456789abcdef";
constexpr uint64_t begin = 490000, end = 591000, frequency = 10000;
unsigned checks{}, failures{};
uint64_t tick = begin - 1, observer_tick = begin + 10;
void Check(bool ok, const char* label) {
  ++checks; if (!ok) { ++failures; std::cerr << "FAIL " << label << '\n'; }
}
uint64_t PhaseClock() { return tick; }
uint64_t ObserverClock() { return observer_tick; }
struct Fixture;
Fixture* current{};
struct Fixture {
  std::unique_ptr<pd::State> phase = std::make_unique<pd::State>();
  std::unique_ptr<rd::State> router = rd::Allocate(begin, end, frequency, token);
  std::unique_ptr<cd::State> consumer = cd::Allocate(begin, end, frequency, token, true);
  std::unique_ptr<wd::State> work = wd::Allocate(begin, end, frequency, token);
  std::unique_ptr<td::State> target = td::Allocate(begin, end, frequency, token);
  std::unique_ptr<sd::State> store = sd::Allocate(begin, end, frequency, token);
  std::array<uint8_t, 4> memory{};
  std::function<void(PPCContext&, uint8_t*)> update, render;
  unsigned updates{}, depth{}, leaves{};
  int expected_access = -1;
  unsigned invalid_from = 99;
  Fixture() {
    Check(!current && !pd::active_invocation && !rd::active && !cd::active &&
          !cd::virtual_active && !wd::active && !wd::tail_active && !td::active && !sd::active,
          "fixture starts without inherited lexical state");
    current = this; tick = begin - 1; observer_tick = begin + 10;
    pd::local = {}; rd::local = {}; cd::local = {}; wd::local = {}; td::local = {}; sd::local = {};
    rd::disarmed = false; cd::disarmed = false; wd::disarmed = false; td::disarmed = false; sd::disarmed = false;
    phase->begin = begin; phase->end = end; phase->frequency = frequency; phase->token = token;
    phase->clock = PhaseClock;
    router->clock = ObserverClock; consumer->clock = ObserverClock; work->clock = ObserverClock;
    target->clock = ObserverClock; store->clock = ObserverClock;
    // All ancestry readers retain their production defaults.
    pd::configured = phase.get(); rd::configured = router.get(); cd::configured = consumer.get();
    wd::configured = work.get(); td::configured = target.get(); sd::configured = store.get();
  }
  ~Fixture() {
    Check(!pd::active_invocation && !pd::local.depth && !phase->active && !rd::active &&
          !cd::active && !cd::virtual_active && !wd::active && !wd::tail_active && !td::active && !sd::active,
          "every actual lexical scope and phase ticket unwinds");
    pd::configured = nullptr; rd::configured = nullptr; cd::configured = nullptr;
    wd::configured = nullptr; td::configured = nullptr; sd::configured = nullptr;
    pd::local = {}; rd::local = {}; cd::local = {}; wd::local = {}; td::local = {}; sd::local = {};
    current = nullptr;
  }
  template<class F> void EachState(F&& fn) {
    fn(*router); fn(*consumer); fn(*work); fn(*target); fn(*store);
  }
  template<class F> void StateAt(unsigned i, F&& fn) {
    if (i == 0) fn(*router); else if (i == 1 || i == 2) fn(*consumer);
    else if (i == 3) fn(*work); else if (i == 4) fn(*target); else fn(*store);
  }
  mcla::boundary_exclusion::Journal& Boundary(unsigned i) {
    if (i == 0) return router->boundary;
    if (i == 1) return consumer->boundary;
    if (i == 2) return consumer->virtual_inventory->boundary;
    if (i == 3) return work->boundary;
    if (i == 4) return target->boundary;
    return store->boundary;
  }
  void Counts(uint64_t admitted, uint64_t boundary) {
    EachState([&](auto& state) {
      Check(state.admitted == admitted && state.boundary.count == boundary,
            "exact admitted versus boundary partition at each independent observer");
    });
    Check(consumer->virtual_inventory->admitted == admitted &&
          consumer->virtual_inventory->boundary.count == boundary,
          "subordinate virtual journal has its own exact partition");
  }
  void FaultCounts(uint64_t boundary, uint64_t raw_router_entries) {
    // Legacy router journaling preserves raw entries even after a sticky
    // fault. Those entries are not valid accessor identities or candidates.
    Check(router->admitted == raw_router_entries, "faulty router retains expected raw records");
    EachState([&](auto& state) {
      Check(state.boundary.count == boundary, "fault cannot add a healthy boundary exclusion");
    });
    Check(!consumer->admitted && !consumer->virtual_inventory->admitted && !work->admitted &&
          !target->admitted && !store->admitted && consumer->virtual_inventory->boundary.count == boundary,
          "faulty router ancestry cannot admit downstream candidates");
  }
  void Healthy() {
    Check(!phase->failed.load() && !phase->boundary_clock_failed, "phase remains healthy");
    EachState([](auto& state) {
      Check(!state.failed && !state.clock_failed && !state.saturated && !state.emergency_failure.load() &&
            state.dropped == 0 && state.errors == 0, "downstream observer healthy without reset");
    });
    Check(!consumer->virtual_inventory->dropped && !consumer->virtual_inventory->errors,
          "virtual journal has no rejection or error");
  }
};
void Access(bool valid, unsigned stage) {
  if (current->expected_access >= 0)
    Check(valid == bool(current->expected_access), "actual lexical accessor has expected admission");
  if (stage >= current->invalid_from)
    Check(!valid, "sticky observer fault cannot expose a valid downstream accessor");
}
void Set(PPCVRegister& reg, const tc::Vector& value) {
  for (unsigned k = 0; k != 4; ++k) reg.u32[k] = value[k];
}
void Store(PPCContext& ctx, uint8_t* base) {
  auto parent = t::ReadActiveSlot28(ctx, base); Access(parent.valid(), 4);
  if (!parent.valid()) Check(!parent.entry.invocation.serial && !parent.event_index,
                            "invalid target snapshot exposes no IDs");
  s::Scope store(ctx, base); const auto object = ctx.r3.u32;
  Set(ctx.v0, {0, 1, 0x80000000, 0x7fc12345}); store.OldLoaded((object + 256u) & ~15u);
  Set(ctx.v13, {1, 2, 3, 4}); store.RateLoaded((object + 272u) & ~15u);
  Set(ctx.v12, {0x7fcfffff, 0x80000000, 1, 0}); store.Computed();
  Set(ctx.v63, {4, 3, 2, 1}); store.AuxLoaded((object + 288u) & ~15u);
  Set(ctx.v1, {9, 8, 7, 6}); store.Scaled(); store.Stored((object + 256u) & ~15u);
  store.Tail([&] { ++current->leaves; ++ctx.r6.u64; SetLastError(4321); });
}
void Target(PPCContext& ctx, uint8_t* base) {
  const auto parent = w::ReadActiveVectorCall(ctx, base); Access(parent.valid(), 3);
  if (!parent.valid()) Check(!parent.entry.invocation.serial && !parent.event_index,
                            "invalid work snapshot exposes no IDs");
  t::Scope target(ctx, base); const auto object = ctx.r3.u64;
  const tc::Vector vec{ctx.v1.u32[0], ctx.v1.u32[1], ctx.v1.u32[2], ctx.v1.u32[3]};
  ctx.r31.u64 = object; Set(ctx.v126, {0x7fa00000, 1, 2, 3});
  target.InitialV126((ctx.r31.u32 + 400u) & ~15u);
  auto call = [&](tc::Site site) {
    ctx.r3.u64 = object; ctx.lr = tc::Caller(site);
    ctx.ctr.u64 = site == tc::Site::kSlot28 ? mcla::vector_store::kTarget : 0x12340000u + unsigned(site);
    if (site == tc::Site::kSlot48 || site == tc::Site::kSlot28 || site == tc::Site::kSlot104) Set(ctx.v1, vec);
    target.Call(site, [&] { if (site == tc::Site::kSlot28) Store(ctx, base); });
  };
  call(tc::Site::kDirectE790); call(tc::Site::kSlot48); call(tc::Site::kSlot28); call(tc::Site::kDirectD1E8);
  ctx.r3.u64 = 1; target.FlagStored(); ctx.r10.u64 = 0; target.Gate100();
  call(tc::Site::kSlot100); call(tc::Site::kSlot104);
  ctx.r11.u64 = 32; target.CounterLoaded(); ++ctx.r11.u64; target.CounterStored(); call(tc::Site::kSlot192);
}
void Work(PPCContext& ctx, uint8_t* base) {
  const auto parent = v::ReadActiveCall(ctx, base); Access(parent.valid(), 2);
  if (!parent.valid()) Check(!parent.entry.invocation.serial && !parent.call.caller_lr,
                            "invalid virtual snapshot exposes no IDs or operands");
  w::TailScope tail(ctx, base); w::Scope work(ctx, base);
  ctx.r7.u64 = 1; work.MainGate(); ctx.r9.u64 = 0; work.ScalarCount();
  ctx.r10.u64 = 1; work.ModeGate(); work.VectorCount();
  ctx.lr = wc::kVectorLr; ctx.r30.u64 = 0; ctx.r3.u64 = 0xffffff00;
  ctx.f1.u64 = 0x8000000012345678ull; ctx.ctr.u64 = tc::kTarget;
  Set(ctx.v1, {0x7fc01234, 0x80000000, 0xffffffff, 1});
  work.Vector([&] { Target(ctx, base); });
}
void Virtual(PPCContext& ctx, uint8_t* base) {
  const auto parent = c::ReadActiveChild(ctx, base); Access(parent.valid(), 1);
  if (!parent.valid()) Check(!parent.entry.invocation.serial && !parent.child.caller_lr,
                            "invalid consumer snapshot exposes no IDs or operands");
  v::Scope body(ctx, base);
  for (uint8_t i = 0; i != 3; ++i) {
    ctx.lr = vc::kCallReturnLr[i]; ctx.r3.u64 = 0x1000 + i; ctx.f1.u64 = 0x3ff0000000000000ull + i;
    ctx.ctr.u64 = i == 0 ? wc::kTail : 0x82111000u + i * 4;
    body.Dispatch(i, [&] { if (i == 0) Work(ctx, base); });
  }
}
void Chain(PPCContext& ctx, uint8_t* base) {
  ctx.lr = 0x822C24C4; ctx.r3.u64 = 0x12345678; ctx.r4.u64 = 1; ctx.f1.u64 = 0x3f91111120000000ull;
  r::RouterScope router(ctx, base);
  ctx.r3.u64 = 0x23456780; ctx.r10.u64 = 0x34567890; ctx.ctr.u64 = 0x82583630;
  router.PredicateDispatch([&] { ctx.r3.u64 = 1; });
  ctx.r3.u64 = 0x23456780; ctx.r11.u64 = 0x34567890; ctx.ctr.u64 = cc::kTarget;
  ctx.f1.u64 = 0x3f91111120000000ull;
  router.ConsumerDispatch([&] {
    const auto parent = r::ReadActiveConsumer(ctx, base); Access(parent.valid(), 0);
    if (!parent.valid()) Check(!parent.invocation.serial && !parent.update.phase_call_id && !parent.r3,
                              "invalid router snapshot exposes no ancestry or operands");
    ctx.lr = cc::kCaller; c::ConsumerScope consumer(ctx, base); ctx.r11.u64 = 1; consumer.Branch();
    for (uint8_t i = 0; i != 3; ++i) {
      ctx.lr = cc::kChildReturnLr[i]; ctx.r3.u64 = 0x2000 + i; ctx.f1.u64 = 0x3ff0000000000000ull + i;
      ctx.ctr.u64 = vc::kTarget;
      consumer.Child(i, [&] { if (i == 0) Virtual(ctx, base); });
    }
  });
}
void Complete(Fixture& f, PPCContext& ctx) {
  tick = observer_tick = begin + 400; f.expected_access = 1;
  ctx.lr = 0x821316A0;
  f.update = Chain; sub_822C1FA8(ctx, f.memory.data());
}
void Positive(bool emit = false) {
  Fixture f; PPCContext ctx{}; f.expected_access = 0;
  ctx.lr = 0x821316A0;
  f.update = [&](PPCContext& state, uint8_t* base) {
    tick = observer_tick = begin + 10;
    Check(p::ReadActiveUpdateBeforeWindow(state, base, begin, end, frequency, token).entry_qpc == begin - 1 &&
          !p::ReadActiveUpdate(state, base).bound(), "exact prebegin root remains non-admitted after begin");
    Chain(state, base);
  };
  sub_822C1FA8(ctx, f.memory.data());
  f.Counts(0, 1); f.Healthy();
  Check(!f.phase->used, "boundary fragment has no census row");
  f.EachState([](auto& state) { Check(!state.used, "boundary fragments allocate no downstream lane or IDs"); });
  for (unsigned i = 0; i != 6; ++i) {
    const auto& row = f.Boundary(i).rows[0];
    Check(row.entry_qpc == begin - 1 && row.qpc == begin + 10 && row.native_thread_id == GetCurrentThreadId(),
          "each boundary journal retains exact root timestamp and observer timestamp");
  }
  // No reset of counters, TLS, journals, health or configuration here.
  Complete(f, ctx); f.Counts(1, 1); f.Healthy();
  Check(f.phase->used == 1 && f.updates == 2 && f.leaves == 2 && ctx.r6.u64 == 2,
        "later complete Update admits whole chain and each synthetic leaf executes once");
  if (emit) {
    // Child cuts precede parents; virtual has no separate clock/cut and is
    // serialized only after its owning consumer has frozen. Stdout only.
    observer_tick = end + 1; const auto store = s::Freeze(token);
    ++observer_tick; const auto target = t::Freeze(token);
    ++observer_tick; const auto work = w::Freeze(token);
    ++observer_tick; const auto consumer = c::Freeze(token);
    const auto virtual_body = v::Freeze(token);
    ++observer_tick; const auto router = r::Freeze(token);
    const auto phase = p::Finish({}, token);
    std::cout << "{\"schema\":\"mcla-full-boundary-synthetic-v1\",\"phase\":" << phase
              << ",\"router\":" << router << ",\"consumer\":" << consumer
              << ",\"virtual\":" << virtual_body << ",\"work\":" << work
              << ",\"target\":" << target << ",\"store\":" << store << "}\n";
  }
}
void MissingRootAndLaneControls() {
  for (unsigned mode = 0; mode != 6; ++mode) {
    Fixture f; PPCContext ctx{}; f.expected_access = 0;
    const auto masked = [&](PPCContext& state, uint8_t* base) { Chain(state, base); };
    f.render = masked;
    f.update = [&](PPCContext& state, uint8_t* base) {
      tick = observer_tick = begin + 10;
      if (f.depth > 1) { Chain(state, base); return; }
      if (mode == 1) { PPCContext other = state; Chain(other, base); }
      if (mode == 2) Chain(state, base + 1);
      if (mode == 3) { std::thread worker([&] { Chain(state, base); }); worker.join(); }
      if (mode == 4) sub_82131008(state, base);
      if (mode == 5) { pd::configured = nullptr; sub_822C1FA8(state, base); pd::configured = f.phase.get(); }
    };
    if (!mode) Chain(ctx, f.memory.data()); else sub_822C1FA8(ctx, f.memory.data());
    f.FaultCounts(0, 1);
    f.EachState([](auto& state) { Check(state.failed, "missing exact root is a genuine sticky observer fault"); });
    tick = observer_tick = begin + 400; f.update = Chain; sub_822C1FA8(ctx, f.memory.data());
    f.FaultCounts(0, 2);
    f.EachState([](auto& state) { Check(state.failed, "later valid Update does not clear earlier root fault"); });
    Check(f.leaves == 2, "root faults never skip or duplicate synthetic work");
  }
}
void PerJournalControls() {
  // All six exclusions are independent. A mismatched window, prior fault or
  // full journal at one observer cannot be excused by a valid ancestor witness.
  for (unsigned kind = 0; kind != 3; ++kind) for (unsigned journal = 0; journal != 6; ++journal) {
    Fixture f; PPCContext ctx{};
    f.invalid_from = journal == 2 ? 1 : journal;
    if (kind == 0) f.StateAt(journal, [](auto& state) { ++state.begin; });
    if (kind == 1) f.StateAt(journal, [](auto& state) { state.failed = true; });
    if (kind == 2) {
      bool seeded = true;
      for (size_t n = 0; n != mcla::boundary_exclusion::kCapacity; ++n)
        seeded = f.Boundary(journal).Record(begin - 2, begin + 9, GetCurrentThreadId()) && seeded;
      Check(seeded, "bounded journal seed succeeds");
    }
    f.update = [&](PPCContext& state, uint8_t* base) { tick = observer_tick = begin + 10; Chain(state, base); };
    sub_822C1FA8(ctx, f.memory.data());
    f.StateAt(journal, [](auto& state) { Check(state.failed,
        "mismatch, prior fault or capacity exhaustion retains sticky failure"); });
    const auto& boundary = f.Boundary(journal);
    Check(boundary.count == (kind == 2 ? mcla::boundary_exclusion::kCapacity + 1 : 0),
          "invalid exclusion never silently contributes a healthy boundary row");
    if (kind == 2) Check(boundary.size == mcla::boundary_exclusion::kCapacity && boundary.overflow == 1 &&
                        boundary.rows[0].entry_qpc == begin - 2,
                        "full boundary journal retains original rows and explicit overflow");
    Check(f.leaves == 1, "per-journal failure never suppresses original synthetic leaf");
    tick = observer_tick = begin + 400; f.update = Chain; sub_822C1FA8(ctx, f.memory.data());
    f.StateAt(journal, [](auto& state) { Check(state.failed,
        "later complete root cannot clear this observer's sticky fault"); });
    if (journal == 5) Check(!f.store->admitted, "failed store cannot admit selected work");
    Check(f.leaves == 2, "sticky per-journal failure preserves later original leaf");
  }
}
void ClockRollback() {
  Fixture f; PPCContext ctx{}; f.expected_access = 0;
  f.update = [&](PPCContext& state, uint8_t* base) {
    tick = observer_tick = begin + 10; Chain(state, base);
    observer_tick = begin + 5; Chain(state, base);
  };
  sub_822C1FA8(ctx, f.memory.data()); f.Counts(0, 1);
  f.EachState([](auto& state) { Check(state.failed && state.clock_failed,
      "positive observer rollback cannot be hidden by prebegin root exclusion"); });
  tick = observer_tick = begin + 400; f.update = Chain; sub_822C1FA8(ctx, f.memory.data());
  f.FaultCounts(1, 1);
  Check(f.leaves == 3, "rollback and later sticky failure preserve every original leaf");
}
} // namespace
REX_EXTERN(__imp__sub_822C1FA8) {
  ++current->updates; ++current->depth;
  try { if (current->update) current->update(ctx, base); }
  catch (...) { --current->depth; throw; }
  --current->depth;
}
REX_EXTERN(__imp__sub_82305D00) { (void)ctx; (void)base; }
REX_EXTERN(__imp__sub_82131008) { if (current->render) current->render(ctx, base); }
int main(int argc, char** argv) {
  const bool emit = argc == 2 && std::string(argv[1]) == "--emit";
  if (argc != 1 && !emit) { std::cerr << "Expected no arguments or --emit\n"; return 2; }
  const char* flags[] = {"mcla_phase_census_probe", "mcla_router_dispatch_probe", "mcla_consumer_detail_probe",
    "mcla_virtual_detail_probe", "mcla_work_emission_probe", "mcla_vector_target_probe", "mcla_vector_store_probe"};
  for (auto flag : flags) Check(rex::cvar::SetFlagByName(flag, "true"), "enable actual accessor flag");
  Positive(emit);
  if (!emit) { MissingRootAndLaneControls(); PerJournalControls(); ClockRollback(); }
  for (auto flag : flags) Check(rex::cvar::SetFlagByName(flag, "false"), "restore default flag");
  if (!emit) std::cout << "full Update boundary: " << checks << " checks, " << failures << " failures\n";
  return failures ? 1 : 0;
}
