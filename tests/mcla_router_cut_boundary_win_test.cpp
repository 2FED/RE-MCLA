// Real phase/router/consumer adapters; synthetic originals and injected clocks.
// No game, GPU, windows, files, input, guest reads or native admission.
// Intentionally RED until a reviewed boundary policy distinguishes an Update
// which entered before the observation window from an in-window binding fault.
// Do not satisfy this test by resetting sticky errors or accepting an unbound
// consumer. The controls below require genuine faults to remain fail-closed.
// Offline mutant plan (primary-owned build copies only; not applied here):
// 1. Blanket early-return for any unbound router: initial missing/lane/mask
//    controls must fail their sticky-fault checks, even if the boundary is green.
// 2. Clear global failure on the next bound Update: follow-up controls must fail.
// 3. Borrow the nearest admitted ancestor across lexical masks: mask checks fail.
// 4. Accept invalid router access in the consumer: ancestry/admission checks fail.
// 5. Suppress mismatch/clock failures: genuine-fault controls must fail.
#if defined(MCLA_FRAME_HANDOFF_PROBE) || defined(MCLA_VIRTUAL_DETAIL_PROBE)
#error This bounded three-adapter test excludes handoff and virtual-detail modules.
#endif
#include "../src/mcla_phase_census_win.cpp"
#include "../src/mcla_router_dispatch_win.cpp"
#include "../src/mcla_consumer_detail_win.cpp"
#include <functional>
#include <iostream>
#include <thread>
#include <vector>

extern "C" uint32_t MclaRouterDispatchCallsiteVersion() noexcept { return 1; }
extern "C" uint32_t MclaConsumerDetailCallsiteVersion() noexcept { return 1; }

namespace {
namespace phase = mcla::phase_census;
namespace pd = phase::detail;
namespace router = mcla::router_dispatch_win;
namespace rd = router::detail;
namespace consumer = mcla::consumer_detail_win;
namespace cd = consumer::detail;
constexpr char token[] = "0123456789abcdef0123456789abcdef";
constexpr uint64_t begin = 100, end = 10000;
unsigned checks{}, failures{};
uint64_t tick = 110;
bool drift_binding{}, rollback_router_clock{};
void Check(bool value, const char* label) {
  ++checks;
  if (!value) { ++failures; std::cerr << "FAIL " << label << '\n'; }
}
uint64_t PhaseClock() { return tick; }
uint64_t RouterClock() { return rollback_router_clock ? 1 : tick++; }
uint64_t ConsumerClock() { return tick++; }
phase::ActiveUpdate DriftingBinding(const PPCContext& c, const uint8_t* b) {
  auto value = phase::ReadActiveUpdate(c, b);
  if (drift_binding && value.bound()) ++value.phase_call_id;
  return value;
}
struct Fixture;
Fixture* current{};
struct Fixture {
  std::unique_ptr<pd::State> p = std::make_unique<pd::State>();
  std::unique_ptr<rd::State> r = rd::Allocate(begin, end, 1000, token);
  std::unique_ptr<cd::State> c = cd::Allocate(begin, end, 1000, token);
  std::array<uint8_t, 4> memory{};
  std::function<void(PPCContext&, uint8_t*)> update, render;
  std::vector<router::ConsumerStatus> access;
  unsigned updates{}, predicates{}, consumers{};
  Fixture() {
    Check(!current && !pd::active_invocation && !pd::local.depth &&
          !rd::active && !cd::active, "fixture has no inherited lexical scopes");
    current = this; tick = 110; drift_binding = rollback_router_clock = false;
    pd::local = {}; rd::local = {}; cd::local = {};
    rd::disarmed.store(false); cd::disarmed.store(false);
    p->begin = begin; p->end = end; p->frequency = 1000;
    p->token = token; p->clock = PhaseClock;
    r->clock = RouterClock; c->clock = ConsumerClock;
    pd::configured.store(p.get()); rd::configured.store(r.get()); cd::configured.store(c.get());
  }
  ~Fixture() {
    Check(!pd::active_invocation && !pd::local.depth && !p->active &&
          !rd::active && !cd::active, "all real lexical scopes and Update tickets unwind");
    pd::configured.store(nullptr); rd::configured.store(nullptr); cd::configured.store(nullptr);
    pd::local = {}; rd::local = {}; cd::local = {}; current = nullptr;
  }
};

// A valid observed router shape, with a synthetic consumer taking its false
// branch. Calls execute exactly once even when every observer rejects them.
void Route(PPCContext& c, uint8_t* b, const std::function<void()>& predicate_hook = {}) {
  c.lr = 0x822C24C4; c.r3.u64 = 0x12345678; c.r4.u64 = 1;
  c.f1.u64 = 0x3f91111120000000ull;
  router::RouterScope route(c, b);
  c.r3.u64 = 0x23456780; c.r10.u64 = 0x34567890; c.ctr.u64 = 0x82583630;
  route.PredicateDispatch([&] {
    ++current->predicates;
    if (predicate_hook) predicate_hook();
    c.r3.u64 = 1;
  });
  c.r3.u64 = 0x23456780; c.r11.u64 = 0x34567890;
  c.ctr.u64 = mcla::consumer_detail::kTarget;
  c.f1.u64 = 0x3f91111120000000ull;
  route.ConsumerDispatch([&] {
    ++current->consumers;
    const auto observed = router::ReadActiveConsumer(c, b);
    current->access.push_back(observed.status);
    if (!observed.valid()) {
      Check(!observed.invocation.serial && !observed.update.phase_call_id && !observed.r3,
            "invalid real router accessor exposes no borrowed operands");
    }
    c.lr = mcla::consumer_detail::kCaller;
    consumer::ConsumerScope body(c, b);
    c.r11.u64 = 0; body.Branch();
    ++c.r6.u64; SetLastError(4321);
  });
}
void Update(PPCContext& c, uint8_t* b) {
  sub_822C1FA8(c, b);
  Check(GetLastError() == 4321, "phase wrapper preserves synthetic original LastError");
}
void CompleteUpdate(Fixture& f, PPCContext& c) {
  tick = 500;
  f.update = [](PPCContext& context, uint8_t* base) { Route(context, base); };
  Update(c, f.memory.data());
}
void HealthyControl() {
  Fixture f; PPCContext c{};
  CompleteUpdate(f, c);
  Check(f.updates == 1 && f.predicates == 1 && f.consumers == 1 && c.r6.u64 == 1,
        "healthy synthetic originals run exactly once");
  Check(f.access.size() == 1 && f.access[0] == router::ConsumerStatus::kValid &&
        f.r->admitted == 1 && f.c->admitted == 1 && !f.r->failed && !f.c->failed,
        "healthy real Update/router/consumer chain admits");
}
void CrossingStartRegression() {
  Fixture f; PPCContext c{};
  tick = begin - 1;
  f.update = [](PPCContext& context, uint8_t* base) {
    Check(!phase::ReadActiveUpdate(context, base).bound(),
          "prebegin Update is not retrospectively assigned a census identity");
    tick = begin + 10; Route(context, base);
  };
  Update(c, f.memory.data());
  Check(f.p->used == 0 && f.c->admitted == 0 && f.access.size() == 1 &&
        f.access[0] != router::ConsumerStatus::kValid,
        "crossing fragment never gains accepted consumer ancestry");
  // Deliberately do NOT reset any adapter, TLS, journal, counter or failure flag
  // between the crossing fragment and this later, wholly observed Update.
  CompleteUpdate(f, c);
  Check(f.updates == 2 && f.predicates == 2 && f.consumers == 2 && c.r6.u64 == 2,
        "boundary rejection cannot skip or duplicate original work");
  Check(f.p->used == 1 && !f.p->failed.load() && !f.p->dropped_scopes,
        "later complete Update has a healthy real phase census row");
  Check(f.access.size() == 2 && f.access[1] == router::ConsumerStatus::kValid,
        "REGRESSION: prebegin Update fragment must not poison later complete Update accessor");
  Check(f.c->admitted == 1,
        "REGRESSION: only the later fully observed consumer must be admitted");
  std::cout << "boundary diagnostic: router_unbound=" << f.r->unbound
            << " router_mismatches=" << f.r->binding_mismatches
            << " router_failed=" << f.r->failed << " consumer_dropped=" << f.c->dropped
            << " consumer_admitted=" << f.c->admitted << '\n';
}
void EntirelyBeforeControl() {
  Fixture f; PPCContext c{};
  tick = 10;
  f.update = [](PPCContext& context, uint8_t* base) { Route(context, base); };
  Update(c, f.memory.data());
  Check(f.p->used == 0 && f.r->outside_before == 1 && f.c->outside_before == 1,
        "entirely prewindow original is observed only as outside-window work");
  CompleteUpdate(f, c);
  Check(f.access.size() == 2 && f.access[0] != router::ConsumerStatus::kValid &&
        f.access[1] == router::ConsumerStatus::kValid && f.c->admitted == 1 &&
        !f.r->failed && !f.c->failed,
        "prewindow work that does not cross begin leaves later chain healthy");
}
void RequireStickyMissingBinding(Fixture& f, PPCContext& c) {
  Check(f.r->failed && f.c->failed && f.r->unbound != 0,
        "genuine initial in-window missing binding marks both observers failed");
  // No reset: a valid later phase identity is not permission to erase a fault.
  const auto prior_scopes = f.p->used;
  CompleteUpdate(f, c);
  Check(f.p->used == prior_scopes + 1 && !f.p->failed.load(),
        "missing-binding control reaches a later healthy real Update");
  Check(f.r->failed && f.c->failed && f.access.size() == 2 &&
        f.access[0] != router::ConsumerStatus::kValid &&
        f.access[1] != router::ConsumerStatus::kValid && f.c->admitted == 0,
        "genuine initial missing binding stays sticky through later complete Update");
  Check(f.predicates == 2 && f.consumers == 2,
        "sticky missing-binding controls never skip synthetic original calls");
}
void InitialMissingUpdateControl() {
  Fixture f; PPCContext c{};
  Check(tick >= begin && !pd::active_invocation,
        "initial missing-Update control starts in-window with no enclosing Update");
  Route(c, f.memory.data());
  Check(f.p->used == 0 && f.c->admitted == 0 && f.access.size() == 1 &&
        f.access[0] != router::ConsumerStatus::kValid,
        "in-window router with no Update cannot invent initial ancestry");
  RequireStickyMissingBinding(f, c);
}
void WrongLaneControls() {
  for (int mode = 0; mode != 3; ++mode) {
    Fixture f; PPCContext c{};
    f.update = [&](PPCContext& context, uint8_t* base) {
      if (mode == 0) { PPCContext other = context; Route(other, base); }
      else if (mode == 1) Route(context, base + 1);
      else {
        // Parent stays active; worker has no Update TLS. No overlapping writes:
        // the parent does nothing until join, and fixture fields are joined.
        std::thread worker([&] { Route(context, base); }); worker.join();
      }
    };
    Update(c, f.memory.data());
    Check(f.access.size() == 1 && f.access[0] != router::ConsumerStatus::kValid &&
          f.c->admitted == 0, "wrong context/base/thread must never borrow Update ancestry");
    RequireStickyMissingBinding(f, c);
  }
}
void NestedMaskControl() {
  for (bool unadmitted_update : {false, true}) {
    Fixture f; PPCContext c{};
    const auto masked_body = [](PPCContext& context, uint8_t* base) {
      Check(!phase::ReadActiveUpdate(context, base).bound(),
            "nested Render or unadmitted Update masks actual outer Update");
      Route(context, base);
    };
    f.render = masked_body;
    f.update = [&](PPCContext& context, uint8_t* base) {
      if (f.updates > 1) { masked_body(context, base); return; }
      const auto outer = phase::ReadActiveUpdate(context, base);
      if (unadmitted_update) {
        pd::configured.store(nullptr); Update(context, base); pd::configured.store(f.p.get());
      } else sub_82131008(context, base);
      Check(outer.bound() && phase::ReadActiveUpdate(context, base).phase_call_id == outer.phase_call_id,
            "nested mask unwinds to the real outer Update without changing its identity");
    };
    Update(c, f.memory.data());
    Check(f.access.size() == 1 && f.access[0] != router::ConsumerStatus::kValid &&
          f.c->admitted == 0, "masked invocation cannot borrow admitted ancestor binding");
    RequireStickyMissingBinding(f, c);
  }
}
void GenuineFaultControls() {
  for (bool clock_fault : {false, true}) {
    Fixture f; PPCContext c{};
    if (!clock_fault) f.r->binding = DriftingBinding;
    f.update = [&](PPCContext& context, uint8_t* base) {
      Route(context, base, [&] {
        if (clock_fault) rollback_router_clock = true;
        else drift_binding = true;
      });
    };
    Update(c, f.memory.data());
    drift_binding = rollback_router_clock = false;
    CompleteUpdate(f, c);
    Check(f.r->failed && (!clock_fault || f.r->clock_failed),
          "genuine admitted binding drift or clock rollback remains sticky");
    Check(f.access.size() == 2 && f.access[0] != router::ConsumerStatus::kValid &&
          f.access[1] != router::ConsumerStatus::kValid && f.c->admitted == 0,
          "later complete Update cannot hide a genuine in-window observer fault");
    Check(f.predicates == 2 && f.consumers == 2 && c.r6.u64 == 2,
          "genuine faults still execute synthetic originals exactly once");
  }
}
} // namespace

REX_EXTERN(__imp__sub_822C1FA8) {
  ++current->updates;
  if (current->update) current->update(ctx, base);
  SetLastError(4321);
}
REX_EXTERN(__imp__sub_82305D00) { (void)ctx; (void)base; SetLastError(4321); }
REX_EXTERN(__imp__sub_82131008) {
  if (current->render) current->render(ctx, base);
  SetLastError(4321);
}
int main() {
  HealthyControl(); CrossingStartRegression(); EntirelyBeforeControl();
  InitialMissingUpdateControl(); WrongLaneControls();
  NestedMaskControl(); GenuineFaultControls();
  std::cout << "router cut boundary: " << checks << " checks, " << failures << " failures\n";
  return failures ? 1 : 0;
}
