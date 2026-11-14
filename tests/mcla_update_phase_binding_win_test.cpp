// Real phase/timing wrappers and accessor; synthetic originals, clock and RPM.
// No game, GPU, windows, files or native admission. Primary owns build/run.
#if defined(MCLA_FRAME_HANDOFF_PROBE)
#error This bounded integration test deliberately excludes frame handoff.
#endif
#include "../src/mcla_phase_census_win.cpp"
#include "../src/mcla_update_timing_win.cpp"
#include <nlohmann/json.hpp>
#include <functional>
#include <iostream>

namespace {
namespace phase = mcla::phase_census;
namespace pd = phase::detail;
namespace timing = mcla::update_timing;
namespace td = timing::detail;
using Json = nlohmann::json;
constexpr char token[] = "0123456789abcdef0123456789abcdef";
unsigned checks{}, failures{};
uint64_t phase_tick = 110, timing_tick = 110;
void Check(bool value, const char* message) {
  ++checks;
  if (!value) { ++failures; std::cerr << "FAIL " << message << '\n'; }
}
uint64_t PhaseClock() { SetLastError(901); return phase_tick; }
uint64_t TimingClock() { SetLastError(902); return timing_tick++; }
struct Fixture;
Fixture* current{};
bool Read(uintptr_t base, uint32_t guest, mcla::step_timer_memory::State& out);
struct Fixture {
  std::unique_ptr<pd::State> p = std::make_unique<pd::State>();
  std::unique_ptr<td::State> t = std::make_unique<td::State>();
  std::array<uint8_t, 4> memory{};
  unsigned depth{}, update_calls{}, render_calls{}, coordinator_calls{}, leaf_calls{}, reads{};
  std::function<void(PPCContext&, uint8_t*)> update, render, leaf;
  Fixture() {
    Check(current == nullptr && pd::active_invocation == nullptr && pd::local.depth == 0,
          "fixture starts without inherited invocation");
    current = this; phase_tick = timing_tick = 110;
    p->begin = 100; p->end = 1000; p->frequency = 100; p->token = token; p->clock = PhaseClock;
    t->data.begin = 100; t->data.end = 1000; t->data.frequency = 100; t->data.token = token;
    t->clock = TimingClock; t->reader = Read;
    pd::configured.store(p.get()); td::configured.store(t.get());
  }
  ~Fixture() {
    Check(pd::active_invocation == nullptr && pd::local.depth == 0 && p->active == 0,
          "real phase TLS and active scopes cleaned");
    pd::configured.store(nullptr); td::configured.store(nullptr); current = nullptr;
  }
  Json Cut() { return Json::parse(timing::Freeze(token)); }
};
bool Read(uintptr_t base, uint32_t guest, mcla::step_timer_memory::State& out) {
  Check(current && base == reinterpret_cast<uintptr_t>(current->memory.data()) && guest == td::kTimer,
        "RPM mock receives only owned lane and fixed timer");
  ++current->reads; out = {}; out.count = 9; out.timer.words[0] = 0x3d088889;
  SetLastError(903); return true;
}
void Probe(PPCContext& ctx, uint8_t* base, td::Kind kind = td::Kind::Timer) {
  ctx.r3.u64 = td::kTimer;
  ctx.lr = kind == td::Kind::Timer ? 0x822C21B4 : kind == td::Kind::Subdivision ? 0x822C246C : 0x822C24C4;
  ctx.r4.u64 = kind == td::Kind::Router ? 1 : 0;
  SetLastError(77);
  const auto calls = current->leaf_calls;
  auto* function = kind == td::Kind::Timer ? sub_821BDA90 :
      kind == td::Kind::Subdivision ? sub_821BD910 : sub_822C52C8;
  function(ctx, base);
  const auto error = GetLastError();
  Check(current->leaf_calls == calls + 1 && error == 1234, "timing wrapper calls original once and preserves LastError");
}
void Update(PPCContext& ctx, uint8_t* base) {
  SetLastError(77); sub_822C1FA8(ctx, base);
  const auto error = GetLastError();
  Check(error == 4321, "phase wrapper preserves update original LastError");
}
uint64_t BoundId(PPCContext& ctx, uint8_t* base) {
  const auto binding = phase::ReadActiveUpdate(ctx, base);
  Check(binding.bound() && binding.native_thread_id == GetCurrentThreadId(), "real accessor has exact admitted update identity");
  return binding.phase_call_id;
}
void CheckBinding(const Json& row, const pd::Scope& scope) {
  Check(row["binding"]["phase_call_id"] == scope.call && row["binding"]["thread_epoch"] == scope.thread_epoch &&
        row["binding"]["native_thread_id"] == scope.tid, "timing record equals actual phase scope identity");
  Check(row["completed"] == true && row["exit_binding_matches"] == true && row["before"].is_object() &&
        row["after"].is_object(), "bound record retains both observations and exact return identity");
}
void Basic() {
  Fixture f; PPCContext ctx{};
  f.update = [](PPCContext& c, uint8_t* b) {
    Probe(c, b); Probe(c, b, td::Kind::Subdivision); Probe(c, b, td::Kind::Router);
  };
  Update(ctx, f.memory.data()); const auto data = f.Cut();
  Check(f.update_calls == 1 && f.leaf_calls == 3 && ctx.r6.u64 == 3 && f.reads == 6,
        "all three real wrapper routes preserve original context effects");
  Check(f.p->used == 1 && data["records"].size() == 3, "three timing records within one actual update");
  for (const auto& row : data["records"]) CheckBinding(row, f.p->scopes[0]);
  Check(!phase::ReadActiveUpdate(ctx, f.memory.data()).bound(), "binding absent after update returns");
}
void Nested() {
  Fixture f; PPCContext ctx{}; uint64_t outer{}, inner{};
  f.update = [&](PPCContext& c, uint8_t* b) {
    if (f.depth == 1) {
      outer = BoundId(c, b); Probe(c, b); Update(c, b);
      Check(BoundId(c, b) == outer, "outer identity restored after nested update"); Probe(c, b);
    } else { inner = BoundId(c, b); Probe(c, b, td::Kind::Subdivision); }
  };
  Update(ctx, f.memory.data()); const auto data = f.Cut();
  Check(outer && inner && outer != inner && f.p->used == 2 && f.p->scopes[1].parent == outer,
        "nested admitted updates have distinct actual IDs and parent reference");
  CheckBinding(data["records"][0], f.p->scopes[0]);
  CheckBinding(data["records"][1], f.p->scopes[1]);
  CheckBinding(data["records"][2], f.p->scopes[0]);
}
void Masks() {
  for (bool render : {false, true}) {
    Fixture f; PPCContext ctx{}; uint64_t outer{};
    f.render = [](PPCContext& c, uint8_t* b) {
      Check(!phase::ReadActiveUpdate(c, b).bound(), "nested Render masks outer Update"); Probe(c, b);
    };
    f.update = [&](PPCContext& c, uint8_t* b) {
      if (f.depth == 1) {
        outer = BoundId(c, b);
        if (render) { SetLastError(77); sub_82131008(c, b); }
        else { phase_tick = 1000; Update(c, b); phase_tick = 110; }
        Check(BoundId(c, b) == outer, "outer restored after masking invocation"); Probe(c, b);
      } else {
        Check(!phase::ReadActiveUpdate(c, b).bound(), "unadmitted Update masks admitted ancestor"); Probe(c, b);
      }
    };
    Update(ctx, f.memory.data()); const auto data = f.Cut();
    Check(f.reads == 2 && data["records"].size() == 2 && data["records"][0]["binding"].is_null() &&
          data["records"][0]["before"].is_null() && data["records"][0]["after"].is_null(),
          "masked call retained unbound without RPM");
    CheckBinding(data["records"][1], f.p->scopes[0]);
  }
}
void WrongLanes() {
  Fixture f; PPCContext ctx{};
  f.update = [](PPCContext& c, uint8_t* b) {
    PPCContext other = c; Probe(other, b); Probe(c, b + 1); Probe(c, nullptr);
  };
  Update(ctx, f.memory.data()); const auto data = f.Cut();
  Check(f.reads == 0 && f.leaf_calls == 3 && data["counts"]["unbound_entries"] == 3,
        "different context/base/null lanes never read timer");
  for (const auto& row : data["records"]) Check(row["binding"].is_null(), "no lane mismatch can borrow parent ID");
}
void Exceptions() {
  for (bool caught_inside : {false, true}) {
    Fixture f; PPCContext ctx{}; uint64_t outer{};
    f.leaf = [&](PPCContext&, uint8_t*) { if (!caught_inside || f.depth == 2) { SetLastError(1234); throw 42; } };
    f.update = [&](PPCContext& c, uint8_t* b) {
      if (caught_inside && f.depth == 1) {
        outer = BoundId(c, b); bool caught = false;
        try { Update(c, b); } catch (int code) { caught = code == 42 && GetLastError() == 1234; }
        Check(caught && BoundId(c, b) == outer, "nested exception propagates unchanged and restores actual outer accessor");
        Probe(c, b);
      } else Probe(c, b);
    };
    bool caught = false;
    try { Update(ctx, f.memory.data()); } catch (int code) { caught = code == 42 && GetLastError() == 1234; }
    Check(caught != caught_inside, "top-level exception behavior unchanged");
    const auto data = f.Cut();
    Check(data["counts"]["original_exceptions"] == 1 && data["records"][0]["normal_return"] == false &&
          data["records"][0]["exit_binding_matches"] == true && f.p->original_exception,
          "timing observes unwind while real Update lexical identity still active");
    Check(!phase::ReadActiveUpdate(ctx, f.memory.data()).bound(), "unwind leaves no stale active binding");
    if (caught_inside) CheckBinding(data["records"][1], f.p->scopes[0]);
  }
}
void Cuts() {
  for (bool timing_first : {false, true}) {
    Fixture f; PPCContext ctx{}; std::string frozen;
    f.leaf = [&](PPCContext& c, uint8_t* b) {
      if (timing_first) frozen = timing::Freeze(token);
      SetLastError(314); phase::Disarm();
      Check(GetLastError() == 314 && !phase::ReadActiveUpdate(c, b).bound(), "actual phase Disarm invalidates accessor without LastError change");
    };
    f.update = [](PPCContext& c, uint8_t* b) { Probe(c, b); };
    Update(ctx, f.memory.data());
    const auto phase_json = Json::parse(phase::Finish({}, token));
    const auto data = f.Cut();
    Check(f.reads == 1 && phase_json["counts"]["active_scopes"] == 0 && phase_json["scopes"][0]["completed"] == true,
          "phase can complete after disarm without donating after-read");
    if (timing_first) {
      Check(timing::Freeze(token) == frozen && data["counts"]["pending_entries"] == 1 &&
            data["records"][0]["completed"] == false && data["records"][0]["exit_qpc"] == 0,
            "timing first cut remains pending despite later original and phase completion");
    } else {
      Check(data["counts"]["binding_mismatches"] == 1 && data["observer_failed"] == true &&
            data["records"][0]["after"].is_null(), "phase-first disarm fails closed on real exit binding");
    }
  }
}
} // namespace

namespace mcla::guest_source_scout { bool Enabled() { return true; } }
REX_EXTERN(__imp__sub_822C1FA8) {
  Check(GetLastError() == 77, "Update original receives incoming LastError");
  ++current->update_calls; ++current->depth;
  try { if (current->update) current->update(ctx, base); }
  catch (...) { --current->depth; throw; }
  --current->depth; SetLastError(4321);
}
REX_EXTERN(__imp__sub_82305D00) { (void)ctx; (void)base; ++current->coordinator_calls; }
REX_EXTERN(__imp__sub_82131008) {
  ++current->render_calls; if (current->render) current->render(ctx, base); SetLastError(4321);
}
namespace {
void Leaf(PPCContext& ctx, uint8_t* base) {
  Check(GetLastError() == 77, "leaf original receives incoming LastError");
  ++current->leaf_calls; ++ctx.r6.u64;
  if (current->leaf) current->leaf(ctx, base);
  SetLastError(1234);
}
}
REX_EXTERN(__imp__sub_821BDA90) { Leaf(ctx, base); }
REX_EXTERN(__imp__sub_821BD910) { Leaf(ctx, base); }
REX_EXTERN(__imp__sub_822C52C8) { Leaf(ctx, base); }

int main() {
  Basic(); Nested(); Masks(); WrongLanes(); Exceptions(); Cuts();
  std::cout << "update/phase binding: " << checks << " checks, " << failures << " failures\n";
  return failures ? 1 : 0;
}
