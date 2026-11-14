// Guest-free actual phase -> strong timer -> Arcade adapter include-source test.
// Compile ONLY this TU (not the three included cpp files separately), with
// MCLA_PHASE_CENSUS_PROBE, MCLA_UPDATE_TIMING_PROBE, MCLA_ARCADE_TIMER_PROBE,
// MCLA_ARCADE_UPDATE_BINDING_PROBE. Do not enable FRAME_HANDOFF or FAULT macros.
// Link the same SDK/cvar/runtime dependencies as the retained timer route test.
// Generated guest originals and the unused scout Enabled API are synthetic;
// ReadActiveUpdate, strong phase/timer routes, memory reader and serializers are real.
// No game, GPU, files, input, real threads, sleeps or dispatcher replacement.
// This does NOT test Runtime/FunctionDispatcher Install or production separate-TU linkage.
#include "../src/mcla_phase_census_win.cpp"
#include "../src/mcla_update_timing_win.cpp"
#include "../src/mcla_arcade_timer_win.cpp"
#include <nlohmann/json.hpp>
#include <cstring>
#include <iostream>
#include <vector>

#if !defined(MCLA_ARCADE_UPDATE_BINDING_PROBE) || defined(MCLA_FRAME_HANDOFF_PROBE) || \
    defined(MCLA_UPDATE_FAULT_CONTEXT_PROBE)
#error Compile the standalone binding route fixture without handoff/fault integration.
#endif

namespace {
namespace phase = mcla::phase_census;
namespace pd = phase::detail;
namespace arcade = mcla::arcade_timer;
namespace ad = arcade::detail;
namespace core = mcla::timer_observer;
using Json = nlohmann::json;
constexpr char token[] = "0123456789abcdef0123456789abcdef";
constexpr DWORD incoming_error = 0x24681357, produced_error = 0xABCD1234;
constexpr unsigned incoming_mxcsr = 0x1F80, produced_mxcsr = 0x3F80;
unsigned checks{}, failures{}, timer_calls{}, unrelated_calls{}, phase_clock_calls{};
DWORD seen_error{};
unsigned seen_mxcsr{};
enum class Route { Update, Outside, RenderChild, CoordinatorChild, DifferentContext, DifferentBase };
Route selected{};
bool routed{}, unwind{}, binding_selected{};
phase::ActiveUpdate callee_binding{};
core::ScopeBinding expected_current{};
struct Expected { core::ScopeBinding binding; bool unwound; };
std::vector<Expected> expected;

void Check(bool value, const char* label) {
  ++checks; if (!value) { ++failures; std::cerr << "FAIL " << label << '\n'; }
}
void Flag(const char* name, bool value) {
  Check(rex::cvar::SetFlagByName(name, value ? "true" : "false"), "test-only pre-init flag selection");
}
uint64_t HostilePhaseClock() {
  ++phase_clock_calls;
  const auto value = pd::Qpc();
  SetLastError(0x99887766); _mm_setcsr(0x5F80);
  return value;
}
void TimerOriginal(PPCContext& ctx, uint8_t* base) {
  ++timer_calls; seen_error = GetLastError(); seen_mxcsr = _mm_getcsr();
  callee_binding = phase::ReadActiveUpdate(ctx, base);
  base[ctx.r3.u32 + 8] ^= 0x5A; base[7] ^= 0x39;
  ctx.r3.u64 = 0xFEEDFACE12345678ull; ctx.r31.u64 ^= 0x8877665544332211ull;
  ctx.r5.u64 ^= 0xDEADBEEFull; ctx.f1.u64 ^= 0x1122334455667788ull;
  ctx.f28.u64 ^= 0x8000000000000000ull; ctx.lr = 0x82221100;
  _mm_setcsr(produced_mxcsr); SetLastError(produced_error);
  if (unwind) throw 991;
}
void CallTimer(PPCContext& ctx, uint8_t* base) {
  if (routed) {
    const auto current = phase::ReadActiveUpdate(ctx, base);
    expected_current = binding_selected ? core::ScopeBinding{
        current.phase_call_id, current.thread_epoch, current.native_thread_id} : core::ScopeBinding{};
    sub_821BDA90(ctx, base);
  } else __imp__sub_821BDA90(ctx, base);
}
void UpdateOriginal(PPCContext& ctx, uint8_t* base) {
  switch (selected) {
    case Route::RenderChild:
      if (routed) sub_82131008(ctx, base); else __imp__sub_82131008(ctx, base);
      break;
    case Route::CoordinatorChild:
      if (routed) sub_82305D00(ctx, base); else __imp__sub_82305D00(ctx, base);
      break;
    case Route::DifferentContext: {
      PPCContext other{}; std::memcpy(&other, &ctx, sizeof ctx);
      CallTimer(other, base); std::memcpy(&ctx, &other, sizeof ctx); break;
    }
    case Route::DifferentBase: CallTimer(ctx, base + 1); break;
    default: CallTimer(ctx, base); break;
  }
}
void Case(Route route, bool throws, bool binding) {
  selected = route; unwind = throws; binding_selected = binding;
  Flag("mcla_arcade_update_binding_probe", binding);
  std::array<uint8_t, 512> direct_memory{}, observed_memory{};
  for (size_t i = 0; i < direct_memory.size(); ++i) direct_memory[i] = uint8_t(i);
  observed_memory = direct_memory;
  PPCContext direct{}, observed{};
  direct.r3.u32 = 128; direct.r5.u64 = 0xAABBCCDD; direct.r31.u64 = 0x12456789;
  direct.f1.u64 = 0x0123456789ABCDEFull; direct.f28.u64 = 0x8000000000000000ull;
  direct.lr = 0x822C21B4; std::memcpy(&observed, &direct, sizeof direct);
  const auto before_calls = timer_calls;
  auto run = [&](PPCContext& ctx, uint8_t* base, bool instrumented) {
    routed = instrumented; expected_current = {};
    _mm_setcsr(incoming_mxcsr); SetLastError(incoming_error);
    int caught = 0;
    try {
      if (route == Route::Outside) CallTimer(ctx, base);
      else if (routed) sub_822C1FA8(ctx, base);
      else __imp__sub_822C1FA8(ctx, base);
    } catch (int code) { caught = code; }
    const auto final_error = GetLastError(); const auto final_mxcsr = _mm_getcsr();
    Check(caught == (throws ? 991 : 0), "original exception preserved through both real wrappers");
    Check(seen_error == incoming_error && seen_mxcsr == incoming_mxcsr,
          "timer original receives caller LastError/MXCSR despite hostile phase clock");
    Check(final_error == produced_error && final_mxcsr == produced_mxcsr,
          "original-produced LastError/MXCSR survives timer and phase completion");
    if (instrumented) {
      expected.push_back({expected_current, throws});
      const bool bound = route == Route::Update;
      Check(callee_binding.bound() == bound, "actual timer original sees exact lexical Update only");
      Check(bool(expected_current.phase_call_id) == (bound && binding),
            "binding OFF and unknown scope remain unbound");
      Check(!phase::ReadActiveUpdate(ctx, base).bound(), "real phase binding unavailable after wrapper return");
    } else Check(!callee_binding.bound(), "direct baseline has no fabricated phase identity");
  };
  run(direct, direct_memory.data(), false); run(observed, observed_memory.data(), true);
  Check(timer_calls == before_calls + 2, "one timer original per direct/routed invocation");
  Check(std::memcmp(&direct, &observed, sizeof direct) == 0, "full PPC context differential");
  Check(direct_memory == observed_memory, "full memory differential including alternate base");
  Check(pd::local.depth == 0 && pd::active_invocation == nullptr, "all real phase TLS scopes unwound");
}
Json BindingJson(const core::ScopeBinding& value) {
  return {{"phase_call_id", value.phase_call_id}, {"thread_epoch", value.thread_epoch},
          {"native_thread_id", value.native_thread_id}};
}
} // namespace

namespace mcla::guest_source_scout { bool Enabled() { return false; } }
REX_EXTERN(__imp__sub_821BDA90) { TimerOriginal(ctx, base); }
REX_EXTERN(__imp__sub_822C1FA8) { UpdateOriginal(ctx, base); }
REX_EXTERN(__imp__sub_82131008) { CallTimer(ctx, base); }
REX_EXTERN(__imp__sub_82305D00) { CallTimer(ctx, base); }
REX_EXTERN(__imp__sub_821BD910) { ++unrelated_calls; ctx.r0.u64 = 1; }
REX_EXTERN(__imp__sub_822C52C8) { ++unrelated_calls; ctx.r0.u64 = 2; }

int main(int argc, char**) {
  if (argc != 1) return 2;
  const auto saved_host = ad::HostState::Capture();
  try {
    const auto* flag = rex::cvar::GetFlagInfo("mcla_arcade_update_binding_probe");
    Check(flag && flag->default_value == "false" && flag->getter && flag->getter() == "false" &&
          flag->lifecycle == rex::cvar::Lifecycle::kInitOnly, "actual production binding flag default OFF/init-only");
    Check(!arcade::Enabled() && !phase::Enabled() && !mcla::update_timing::Enabled(),
          "all actual observer defaults OFF");
    Flag("mcla_arcade_timer_probe", true); Flag("mcla_phase_census_probe", true);
    Flag("mcla_arcade_update_binding_probe", true);
    Check(sub_821BDA90 != __imp__sub_821BDA90 && sub_822C1FA8 != __imp__sub_822C1FA8 &&
          sub_82131008 != __imp__sub_82131008 && sub_82305D00 != __imp__sub_82305D00,
          "strong public addresses distinct from synthetic generated originals (not dispatcher mapping)");
    LARGE_INTEGER frequency{};
    if (!QueryPerformanceFrequency(&frequency) || frequency.QuadPart <= 0)
      throw std::runtime_error("native QPF unavailable");
    const auto begin = ad::Qpc(), qpf = uint64_t(frequency.QuadPart), end = begin + 12 * qpf;
    // Explicit include-source install seam: no Runtime constructed/dispatcher modified.
    ad::InstallState(*ad::configured, true, sub_821BDA90);
    phase::Begin(begin, end, qpf, token); arcade::Begin(begin, end, qpf, token);
    auto* phase_state = pd::configured.load();
    phase_state->clock = HostilePhaseClock;
    Case(Route::Update, false, true); Case(Route::Update, true, true);
    Case(Route::Outside, false, true); Case(Route::RenderChild, false, true);
    Case(Route::CoordinatorChild, false, true); Case(Route::DifferentContext, false, true);
    Case(Route::DifferentBase, false, true); Case(Route::Update, false, false);
    Check(ad::Qpc() < end, "all synthetic work completed inside declared timer window");
    Check(timer_calls == 16 && unrelated_calls == 0, "eight direct/routed cases, no unrelated guest calls");
    Check(mcla::update_timing::detail::configured.load() == nullptr,
          "binding route never implicitly creates legacy update-timing observer");
    Check(phase_state->active == 0 && phase_state->used == 9 && phase_clock_calls == 18,
          "nine completed lexical phases and hostile entry/exit clocks");
    Check(!phase_state->failed.load() && !phase_state->invalid_qpc && phase_state->original_exception,
          "intentional original unwind retained without observer failure");

    // Timer snapshot first; phase is still live and cannot erase exit bindings.
    arcade::Disarm();
    auto snapshot = std::make_unique<ad::Observer::Snapshot>();
    Check(ad::configured->observer.load()->try_freeze_into(*snapshot), "timer drain snapshot succeeds without sleeps");
    Check(!phase_state->stopped.load(), "phase stays live throughout timer snapshot");
    Check(snapshot->records().size() == 8 && snapshot->counts().completed_entries == 8 &&
          snapshot->counts().stored_entries == 8 && !snapshot->counts().observer_failed &&
          !snapshot->counts().overflow_entries, "all eight timer rows retained");
    Flag("mcla_arcade_update_binding_probe", true);
    const auto v2 = Json::parse(ad::Serialize(*snapshot, token));
    Check(v2["schema"] == "mcla-arcade-timer-observer-v2" &&
          v2["timer_to_update_binding_proven"] == false, "v2 raw scope witness is not an accepted binding proof");
    if (snapshot->records().size() == expected.size()) {
      for (size_t i = 0; i < expected.size(); ++i) {
        const auto& row = snapshot->records()[i]; const auto& wanted = expected[i];
        Check(row.entry_ordinal == i + 1, "every row kept in entry order without matching search");
        Check(row.entry_binding == wanted.binding && row.exit_binding == wanted.binding,
              "entry and exit raw tuples equal exact actual lexical scope");
        Check(row.callback_issues == (wanted.unwound ? core::kOriginalUnwound : 0u),
              "no callback errors hidden; original unwind bit exact");
        Check(row.observation.identity.timer_address == 128 && row.observation.before_available &&
              row.observation.after_available &&
              (row.observation.before.words[0] ^ row.observation.after.words[0]) == 0x5A000000,
              "real memory reader uses original timer address despite mutated r3");
        Check(v2["records"][i]["entry_binding"] == BindingJson(wanted.binding) &&
              v2["records"][i]["exit_binding"] == BindingJson(wanted.binding), "raw tuple serialization exact");
        if (wanted.binding.phase_call_id) {
          unsigned matches = 0;
          for (size_t j = 0; j < phase_state->used; ++j) {
            const auto& scope = phase_state->scopes[j];
            if (scope.call != wanted.binding.phase_call_id) continue;
            ++matches;
            Check(scope.kind == pd::Kind::Update && scope.completed &&
                  scope.exception == wanted.unwound && scope.thread_epoch == wanted.binding.thread_epoch &&
                  scope.tid == wanted.binding.native_thread_id, "raw tuple names one completed actual Update scope");
          }
          Check(matches == 1, "exact unique phase ID, no nearest-frame inference");
        }
      }
    }
    // V1 emits exactly the legacy shape, even when asked to serialize this
    // diagnostic mixed-mode snapshot. Production init-only selection never toggles.
    Flag("mcla_arcade_update_binding_probe", false);
    const auto v1 = Json::parse(ad::Serialize(*snapshot, token));
    auto projected = v2; projected["schema"] = "mcla-arcade-timer-observer-v1";
    projected.erase("timer_to_update_binding_proven");
    for (auto& row : projected["records"]) { row.erase("entry_binding"); row.erase("exit_binding"); }
    Check(v1 == projected, "binding OFF preserves all legacy v1 fields without new keys");
    for (const char* proof : {"simulation_proven", "fps_proven", "hud_binding_proven", "native_admitted",
                              "update_count_proven", "frame_to_step_mapping_proven"})
      Check(v2[proof] == false && v1[proof] == false, "no proof upgrade in either schema");
    unsigned waits = 0, clock_reads = 0;
    const auto frozen_time = std::chrono::steady_clock::time_point{};
    const auto phases = Json::parse(pd::Drain(*phase_state, {},
        [&] { return frozen_time + std::chrono::milliseconds(clock_reads++); }, [&] { ++waits; }));
    Check(waits == 0 && phases["scopes"].size() == 9 && phase_state->stopped.load(),
          "phase drains only after timer snapshot, without wait or missing scopes");
    Check(pd::active_invocation == nullptr && pd::local.depth == 0, "terminal has no outstanding lexical scopes");
    phase::Disarm();
    saved_host.Restore();
    std::cout << "ARCADE_BINDING_ROUTE checks=" << checks << " failures=" << failures << '\n';
    std::cout.flush(); std::cerr.flush();
    if (!std::cout || !std::cerr) return 2;
    return failures ? 1 : 0;
  } catch (const std::exception& e) {
    arcade::Disarm(); phase::Disarm(); saved_host.Restore();
    std::cerr << "FAIL " << e.what() << '\n'; std::cerr.flush(); return std::cerr ? 1 : 2;
  }
}
