// Actual phase wrapper and ObserveSwap, synthetic clocks
// and originals only. No game, GPU, window, guest memory reads, files or input.
// v3 callback-time lexical witnesses do not invent scopes, completion or IDs.
// Genuine unknowns and all raw callbacks remain inventoried.
#if defined(MCLA_FRAME_HANDOFF_PROBE)
#error This narrow regression deliberately excludes frame handoff.
#endif
#ifndef MCLA_PHASE_CENSUS_PROBE
#define MCLA_PHASE_CENSUS_PROBE
#endif
#include "../src/mcla_phase_census_win.cpp"
#include <nlohmann/json.hpp>
#include <cstring>
#include <functional>
#include <iostream>
#include <thread>

namespace {
namespace phase = mcla::phase_census;
namespace d = phase::detail;
using Json = nlohmann::json;
constexpr char token[] = "0123456789abcdef0123456789abcdef";
// Matches the existing synthetic 91-slot scout schedule: anchor100000,
// source_begin500000, one-second pre-window and 1000-tick source cadence.
constexpr uint64_t begin = 490000, end = 591000, frequency = 10000;
uint64_t tick = begin - 1;
unsigned checks{}, failures{};
bool emit_mode{};
void Check(bool ok, const char* label) {
  ++checks; if (!ok) { ++failures; std::cerr << "FAIL " << label << '\n'; }
}
uint64_t Clock() { return tick; }
struct Fixture;
Fixture* current{};
struct Fixture {
  std::unique_ptr<d::State> state = std::make_unique<d::State>();
  std::array<uint8_t, 4> memory{};
  std::function<void(PPCContext&, uint8_t*)> render, update;
  unsigned renders{}, updates{}, callback_calls{};
  Fixture() {
    Check(!current && !d::active_invocation && !d::local.depth && !d::local.suppressed,
          "fixture has no inherited lexical phase state");
    current = this; tick = begin - 1; d::local = {};
    state->begin = begin; state->end = end; state->frequency = frequency;
    state->token = token; state->clock = Clock; d::configured = state.get();
  }
  ~Fixture() {
    Check(!d::active_invocation && !d::local.depth && !d::local.suppressed && !state->active,
          "every actual wrapper restores lexical state and active tickets");
    d::configured = nullptr; d::local = {}; current = nullptr;
  }
  Json Cut() { return Json::parse(phase::Finish({}, token)); }
};
uint64_t Callback(PPCContext& ctx, uint8_t* base) {
  ++current->callback_calls;
  const auto before = ctx; const auto csr = _mm_getcsr(); SetLastError(731);
  const auto id = phase::ObserveSwap(ctx, base);
  const auto error = GetLastError();
  Check(std::memcmp(&ctx, &before, sizeof(ctx)) == 0 && error == 731 && _mm_getcsr() == csr,
        "actual callback preserves all scalar context, LastError and MXCSR");
  return id;
}
size_t Unknown(const Json& value) {
  Check(value.at("schema") == "mcla-phase-census-v3", "explicit v3 phase schema");
  size_t count = 0;
  for (const auto& row : value.at("swap_callbacks")) {
    const auto& witness = row.at("prewindow_render");
    if (!row.at("has_scope").get<bool>()) {
      if (witness.is_null()) ++count;
      else {
        Check(witness.is_object() && witness.size() == 4 &&
              witness.at("exact_binding") == true && witness.at("entry_qpc") > 0 &&
              witness.at("entry_qpc") < value.at("begin_qpc") &&
              row.at("qpc") >= value.at("begin_qpc") && row.at("qpc") < value.at("end_qpc_exclusive") &&
              witness.at("entry_native_thread_id") == row.at("native_thread_id") &&
              witness.at("lexical_depth") >= 1 && witness.at("lexical_depth") <= d::kDepth &&
              row.at("suppressed_scope") == false,
              "explicit witness is exact, bounded, same-thread and genuinely crosses begin");
      }
      Check(row.at("active_scope_call_id") == 0 && row.at("context_same") == false &&
            row.at("base_same") == false, "every unscoped callback retains zero association fields");
    } else Check(witness.is_null(), "admitted callback cannot also claim prewindow witness");
  }
  return count;
}
void Render(PPCContext& ctx, uint8_t* base) {
  ctx.lr = 0x82305F80; sub_82131008(ctx, base);
}
Json KnownCrossingThenComplete() {
  Fixture f; PPCContext ctx{};
  const auto next_scope = d::next_call.load();
  f.render = [&](PPCContext& state, uint8_t* base) {
    tick = begin + 10; state.lr = 0x8241A0E8;
    Check(Callback(state, base) != 0, "crossing callback is retained, never silently dropped");
  };
  Render(ctx, f.memory.data());
  Check(f.state->used == 0 && d::next_call.load() == next_scope,
        "pre-window Render never acquires a fabricated admitted phase identity");
  // No resets of phase storage, clocks, TLS, counters or health between calls.
  tick = begin + 400;
  f.render = [&](PPCContext& state, uint8_t* base) {
    tick = begin + 410; state.lr = 0x8241A0E8; Callback(state, base);
  };
  Render(ctx, f.memory.data());
  const auto value = f.Cut();
  Check(f.renders == 2 && f.callback_calls == 2 && value["counts"]["stored_callbacks"] == 2,
        "both originals and callbacks execute exactly once and remain inventoried");
  Check(value["scopes"].size() == 1 && value["scopes"][0]["kind"] == "render" &&
        value["scopes"][0]["entry_qpc"] == begin + 400 && value["scopes"][0]["completed"] == true,
        "only the later wholly observed Render has an admitted scope");
  const auto& later = value["swap_callbacks"][1];
  Check(later["has_scope"] == true && later["context_same"] == true && later["base_same"] == true &&
        later["active_scope_call_id"] == value["scopes"][0]["call_id"],
        "later healthy callback retains its actual exact Render association");
  Check(value["observer_failed"] == false && value["invalid_qpc"] == false &&
        value["boundary_clock_failed"] == false &&
        value["counts"]["dropped_callbacks"] == 0 && value["counts"]["dropped_scopes"] == 0,
        "reproduced failure is missing boundary provenance, not drops or observer faults");
  const auto unknown = Unknown(value);
  if (!emit_mode)
    std::cout << "crossing diagnostic: retained_callbacks=" << value["swap_callbacks"].size()
              << " admitted_scopes=" << value["scopes"].size() << " unknown_callbacks=" << unknown << '\n';
  Check(unknown == 0,
        "REGRESSION: positively known pre-window Render fragment must not remain an unknown callback poisoning later observations");
  return value;
}
void UnknownControls() {
  // Actual missing root, wrong lane and masked root are deliberately NOT
  // classified by temporal proximity, callback LR, TID, or a later good scope.
  for (unsigned mode = 0; mode != 26; ++mode) {
    Fixture f; PPCContext ctx{};
    f.render = [&](PPCContext& state, uint8_t* base) {
      tick = begin + 10;
      if (mode == 1) { PPCContext other = state; Callback(other, base); }
      if (mode == 2) Callback(state, base + 1);
      if (mode == 3) { std::thread worker([&] { Callback(state, base); }); worker.join(); }
      if (mode == 4) {
        // An unconfigured nested wrapper must mask the outer lexical Render.
        d::configured = nullptr;
        f.update = [&](PPCContext& nested, uint8_t* memory) {
          d::configured = f.state.get(); Callback(nested, memory);
        };
        sub_822C1FA8(state, base);
      }
      if (mode == 5) Callback(state, base);
      if (mode >= 6 && mode <= 9) {
        if (mode == 6) ++f.state->begin;
        if (mode == 7) ++f.state->end;
        if (mode == 8) ++f.state->frequency;
        if (mode == 9) f.state->token[0] = 'f';
        Callback(state, base);
        f.state->begin = begin; f.state->end = end;
        f.state->frequency = frequency; f.state->token = token;
      }
      if (mode == 10) Callback(state, nullptr);
      if (mode >= 11 && mode <= 20) {
        if (mode == 11) f.state->failed = true;
        if (mode == 12) f.state->invalid_qpc = true;
        if (mode == 13) ++f.state->dropped_scopes;
        if (mode == 14) ++f.state->dropped_callbacks;
        if (mode == 15) ++f.state->stack_overflow;
        if (mode == 16) ++f.state->scope_overflow;
        if (mode == 17) ++f.state->callback_overflow;
        if (mode == 18) f.state->identity_exhausted = true;
        if (mode == 19) f.state->original_exception = true;
        if (mode == 20) f.state->drain_incomplete = true;
        Callback(state, base);
      }
      if (mode == 21) {
        if (f.renders <= d::kDepth) { tick = begin - 1; Render(state, base); }
        else Callback(state, base);
      }
      if (mode == 22) {
        tick = begin - 1;
        f.update = [&](PPCContext& nested, uint8_t* memory) {
          tick = begin + 10; Callback(nested, memory);
        };
        sub_822C1FA8(state, base);
      }
      if (mode >= 23) Callback(state, base);
    };
    if (mode == 0) { tick = begin + 10; Callback(ctx, f.memory.data()); }
    else {
      if (mode == 23) f.state->failed = true;
      if (mode == 24) f.state->frequency = 0;
      if (mode == 25) f.state->token[0] = 'G';
      if (mode == 5) {
        // Establish a real earlier high-water sample, then roll back to a
        // positive pre-window entry. It must not manufacture trusted provenance.
        tick = begin + 50; phase::ObserveSwap(ctx, f.memory.data());
        tick = begin - 1;
      }
      Render(ctx, f.memory.data());
    }
    f.state->frequency = frequency; f.state->token = token;
    const auto value = f.Cut();
    Check(value["scopes"].empty(), "unknown control cannot manufacture an admitted scope");
    Check(Unknown(value) == (mode == 5 ? 2u : 1u),
          "genuine unknown callbacks remain retained and globally blocking");
    Check(value["counts"]["dropped_callbacks"] == (mode == 14 ? 1 : 0) && f.callback_calls == 1,
          "unknown controls retain their callback without retries or silent drops");
    if (mode == 5) Check(f.state->boundary_clock_failed && value["boundary_clock_failed"] == true,
                         "positive rollback leaves sticky boundary provenance failure");
  }
}
void ReturnAndClockEdges() {
  Fixture f; PPCContext ctx{};
  f.render = [&](PPCContext& state, uint8_t* base) {
    // Callback before the measurement interval is not journalled.
    Check(Callback(state, base) == 0, "pre-window callback is outside callback inventory");
    tick = begin; Check(Callback(state, base) == 1, "exact begin permits lexical witness");
  };
  Render(ctx, f.memory.data());
  tick = begin + 1; Check(Callback(ctx, f.memory.data()) == 2,
                          "callback after root return remains retained but unknown");
  tick = begin; Check(Callback(ctx, f.memory.data()) == 3,
                      "later positive rollback remains an actual raw callback");
  const auto value = f.Cut();
  Check(Unknown(value) == 2 && value["swap_callbacks"][0]["prewindow_render"]["entry_qpc"] == begin - 1,
        "return edge and later fault do not erase earlier callback-time provenance");
  Check(value["boundary_clock_failed"] == true && value["counts"]["stored_callbacks"] == 3,
        "final sticky clock health is serialized despite earlier valid witness");
}
void StateMismatch() {
  Fixture f; PPCContext ctx{};
  auto other = std::make_unique<d::State>();
  other->begin = begin; other->end = end; other->frequency = frequency;
  other->token = token; other->clock = Clock;
  f.render = [&](PPCContext& state, uint8_t* base) {
    tick = begin + 10; d::configured = other.get();
    Check(Callback(state, base) == 1, "different configured state retains its own callback ID");
    d::configured = f.state.get();
  };
  Render(ctx, f.memory.data());
  other->stopped = true;
  const auto mismatch = Json::parse(d::Serialize(*other));
  Check(Unknown(mismatch) == 1 && other->used == 0 && f.state->callback_used == 0,
        "matching values in another State cannot borrow lexical provenance");
  f.Cut();
}
void CapacityAndLateCallback() {
  Fixture f; PPCContext ctx{};
  f.render = [&](PPCContext& state, uint8_t* base) {
    tick = begin + 10;
    for (size_t i = 0; i != d::kCallbacks; ++i)
      Check(Callback(state, base) == i + 1, "real bounded callbacks retain consecutive IDs");
    Check(Callback(state, base) == 0 && f.state->callback_overflow == 1,
          "full callback inventory fails boundedly instead of overwriting or inventing ID");
    tick = end; Check(Callback(state, base) == 0, "exclusive end rejects late callback");
  };
  Render(ctx, f.memory.data());
  const auto value = f.Cut();
  Check(Unknown(value) == 0 && value["counts"]["callback_overflow"] == 1 &&
        value["counts"]["stored_callbacks"] == d::kCallbacks &&
        value["counts"]["dropped_callbacks"] == 1,
        "all earlier witnesses retained but final capacity fault blocks acceptance");
}
void FinishDuringUnadmittedRoot() {
  Fixture f; PPCContext ctx{}; Json cut;
  f.render = [&](PPCContext& state, uint8_t* base) {
    tick = begin + 10; Callback(state, base);
    cut = f.Cut();
    tick = begin + 20;
    Check(Callback(state, base) == 0, "disarm prevents callback writes after immutable cut");
  };
  Render(ctx, f.memory.data());
  Check(cut == Json::parse(d::Serialize(*f.state)),
        "unadmitted Render return performs no untracked post-Finish writes");
  Check(Unknown(cut) == 0 && cut["counts"]["active_scopes"] == 0,
        "callback-time witness does not claim admission or completion of its root");
}
} // namespace
REX_EXTERN(__imp__sub_82131008) {
  ++current->renders; if (current->render) current->render(ctx, base);
}
REX_EXTERN(__imp__sub_822C1FA8) {
  ++current->updates; if (current->update) current->update(ctx, base);
}
REX_EXTERN(__imp__sub_82305D00) { (void)ctx; (void)base; }
int main(int argc, char** argv) {
  if (argc == 2 && std::strcmp(argv[1], "--emit") == 0) {
    emit_mode = true;
    const auto journal = KnownCrossingThenComplete();
    if (failures) return 1;
    std::cout << journal.dump() << '\n';
    return 0;
  }
  if (argc != 1) { std::cerr << "Expected no arguments or --emit\n"; return 2; }
  KnownCrossingThenComplete(); UnknownControls(); ReturnAndClockEdges();
  StateMismatch(); CapacityAndLateCallback(); FinishDuringUnadmittedRoot();
  std::cout << "Render boundary: " << checks << " checks, " << failures << " failures\n";
  return failures ? 1 : 0;
}
