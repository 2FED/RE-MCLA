// Synthetic regression for the ACTUAL public handoff wrappers, not a HostState
// helper test. Explicit Registry callbacks + mock PPC originals only: no Runtime,
// game, native semaphore/SDK-operation execution, window, input or filesystem.
// Paired SDK import-library linking and DLL loading are separately recipe-bound.
// The observer clock/resolver deliberately damages native state. The original
// never does that on behalf of the observer; it supplies its own produced state.
#include "../src/mcla_frame_handoff_win.cpp"
#include <xmmintrin.h>
#include <iomanip>
#include <iostream>
#include <memory>
#include <new>

namespace handoff_host_state_test {
namespace handoff = mcla::frame_handoff;
namespace detail = handoff::detail;
namespace api = detail::api;
static_assert(api::kCompiledEnabled, "Host-state fixture requires enabled semaphore callback transport");

constexpr uint64_t kCookie = 123;
constexpr uint32_t kHandle = 500, kOwner = 0x4000;
constexpr char kToken[] = "55555555555555555555555555555555";
constexpr uint64_t kProducedRegister = 0xA1B2C3D4E5F60718ull;
constexpr bool kRuntimeAdmitted = false, kSdkIntegrationProven = false;
constexpr bool kGameProven = false, kCausalityProven = false, kFpsProven = false;

struct Native {
  DWORD error{};
  unsigned mxcsr{};
  static Native Capture() noexcept { const auto error = GetLastError(); return {error, _mm_getcsr()}; }
  void Restore() const noexcept { _mm_setcsr(mxcsr); SetLastError(error); }
  bool operator==(const Native&) const = default;
};
// All exceptions masked; different rounding modes and sticky status bits.
constexpr Native kIncoming{0x13572468u, 0x3F80u}; // round down
constexpr Native kProduced{0x24681357u, 0x7F80u}; // round toward zero
constexpr Native kHostile{0xDEAD9999u, 0x5F81u};  // round up + invalid sticky flag
enum class Kind { Create, Wait, Release, Worker, Dispatch };
enum class Route { Configured, Cold, FrozenGuard, Unmatched };
struct Signal { unsigned code; };

struct Result {
  Native entry{}, produced{}, returned{};
  const PPCContext* actual_context{};
  const uint8_t* actual_base{};
  unsigned originals{}, clocks{}, original_clocks{}, resolutions{}, caught{}, unexpected{}, signal{}, progress{};
  int incoming_fp{}, produced_fp{};
};
struct Run {
  Result result{}; // All stores survive unwind: never captured automatic scalars.
  PPCContext context{};
  std::array<uint8_t, 64> memory{};
  api::Registry* transport{};
  volatile float operand = 1.75f;
  Kind kind{};
  Route route{};
  bool unwind{}, in_original{};
};
thread_local Run* active_run{};
uint64_t stamp = 101;
unsigned checks{}, failures{}, cases{};

const char* Name(Kind kind) noexcept {
  switch (kind) {
    case Kind::Create: return "create";
    case Kind::Wait: return "wait";
    case Kind::Release: return "release";
    case Kind::Worker: return "worker";
    case Kind::Dispatch: return "dispatch";
  }
  return "unknown";
}
const char* Name(Route route) noexcept {
  switch (route) {
    case Route::Configured: return "configured";
    case Route::Cold: return "cold";
    case Route::FrozenGuard: return "frozen-guard";
    case Route::Unmatched: return "unmatched";
  }
  return "unknown";
}
void Check(bool condition, const char* label, const Run* run = nullptr) {
  ++checks;
  if (condition) return;
  ++failures;
  std::cerr << "FAIL " << label;
  if (run) std::cerr << " case=" << Name(run->kind) << '/' << Name(run->route)
                     << '/' << (run->unwind ? "unwind" : "normal");
  std::cerr << '\n';
}

// These are the observer's actual injectable sources, not game originals.
// No hostile effect outside the measured boundary (setup/teardown is quiet).
__declspec(noinline) uint64_t ObserverClock() {
  if (active_run) {
    ++active_run->result.clocks;
    if (active_run->in_original) ++active_run->result.original_clocks;
    kHostile.Restore();
  }
  return ++stamp;
}
uint64_t CollectorClock(void*) { return ObserverClock(); }
uint64_t Frequency(void*) { return 1000; }
__declspec(noinline) uint64_t ObserverResolve(detail::State&, uint32_t handle) {
  if (active_run) { ++active_run->result.resolutions; kHostile.Restore(); }
  return handle == kHandle ? kCookie : 0;
}

// One identical original body is used by the direct baseline AND every public
// wrapper's __imp__ target. Actual registry callbacks model SDK metadata calls;
// they are not a claim of actual native semaphore/SDK-integration coverage.
__declspec(noinline) void Original(PPCContext& context, uint8_t* base) {
  auto& run = *active_run;
  struct OriginalBoundary {
    bool& active;
    explicit OriginalBoundary(bool& value) : active(value) { active = true; }
    ~OriginalBoundary() noexcept { active = false; }
  } boundary(run.in_original);
  auto& out = run.result;
  ++out.originals;
  out.actual_context = &context; out.actual_base = base;
  out.entry = Native::Capture(); out.progress = 1;
  const float incoming_operand = run.operand;
  out.incoming_fp = _mm_cvtss_si32(_mm_set_ss(incoming_operand));
  context.r6.u64 += 1; ++base[0]; out.progress = 2;

  if (run.kind == Kind::Create) {
    run.transport->Create({kCookie, 0, 32767, api::Origin::kFresh});
    context.r3.u32 = kHandle;
  } else if (run.kind == Kind::Wait || run.kind == Kind::Release) {
    const bool wait = run.kind == Kind::Wait;
    const auto kind = wait ? api::Kind::kWait : api::Kind::kRelease;
    const auto ticket = run.transport->Begin({kCookie, kind, wait ? 0u : 1u,
        wait ? api::TimeoutClass::kInfinite : api::TimeoutClass::kNotApplicable});
    if (ticket) run.transport->End({ticket, kind, api::Result::kSuccess,
        wait ? std::optional<int32_t>{} : std::optional<int32_t>{0}});
    context.r3.u32 = wait ? 1 : 0;
  }

  // Real original-produced native state is established AFTER its mock SDK
  // work. Any later change must come from the wrapper/observer or EH runtime.
  kProduced.Restore();
  const float produced_operand = run.operand;
  out.produced_fp = _mm_cvtss_si32(_mm_set_ss(produced_operand));
  context.r7.u64 = kProducedRegister;
  out.produced = Native::Capture(); out.progress = 3;
  if (run.unwind) throw Signal{43};
  out.progress = 4;
}

struct Fixture {
  api::Registry transport;
  std::unique_ptr<detail::State> state = std::make_unique<detail::State>(transport,
      detail::sem::ClockProvider{nullptr, CollectorClock, Frequency});
  Fixture() {
    // Internal include-source fixture only. No lease/native call survives a
    // synchronous case; production state is NOT reset/rearmed this way.
    detail::local.~Local(); new (&detail::local) detail::Local;
    stamp = 101;
    state->clock = ObserverClock; state->test_resolver = ObserverResolve;
    state->token = kToken; state->begin = 100; state->end = 10000; state->frequency = 1000;
    Check(state->collector.Configure({100,10000,1000},kToken,true) == detail::sem::Status::kOk,
          "host-state fixture registers fresh explicit collector");
    detail::configured.store(state.get());
  }
  ~Fixture() {
    active_run = nullptr;
    detail::configured.store(nullptr);
    detail::local.~Local(); new (&detail::local) detail::Local;
  }
  void Select(Run& run) {
    detail::sem::Collector::ScopedCreate scope(state->collector,
        {detail::sem::kCreationLR,0,kOwner,detail::Lane(run.context,run.memory.data())});
    Check(transport.Create({kCookie,0,32767,api::Origin::kFresh}),
          "host-state fixture observes explicit selected creation");
    Check(state->collector.BindReturnedHandle(scope,kCookie,kHandle),
          "host-state fixture binds selected handle independently");
    state->cookie = kCookie; state->epoch = scope.selected_epoch();
    state->owner = kOwner; state->handle = kHandle;
    Check(state->epoch != 0, "host-state fixture has selected object lifetime");
  }
  void SeedWorker(Run& run) {
    Check(detail::Ready(*state,run.context,run.memory.data()),
          "host-state fixture seeds exact worker lane");
    detail::local.key = {detail::local.thread,1,
        {reinterpret_cast<uintptr_t>(&run.context),reinterpret_cast<uintptr_t>(run.memory.data())},
        kOwner,state->epoch};
    detail::local.worker_active = true;
    const auto entered = detail::local.worker.Enter(detail::local.key,++stamp);
    Check(entered.event == detail::worker::Event::kEntered,
          "host-state fixture seeds actual worker automaton");
  }
};

void Prepare(Run& run, Fixture& fixture) {
  run.transport = &fixture.transport;
  if (run.kind != Kind::Create) fixture.Select(run);
  if (run.kind == Kind::Wait || run.kind == Kind::Dispatch) fixture.SeedWorker(run);
  run.context.r31.u32 = kOwner;
  switch (run.kind) {
    case Kind::Create: run.context.lr = detail::sem::kCreationLR; run.context.r3.u32 = 0; break;
    case Kind::Wait: run.context.lr = 0x82305EB8; run.context.r3.u32 = kHandle; break;
    case Kind::Release: run.context.lr = detail::sem::kReleaseLR; run.context.r3.u32 = kHandle; break;
    case Kind::Worker: run.context.lr = 0x82306300; run.context.r3.u32 = kOwner; break;
    case Kind::Dispatch: run.context.lr = 0x82305F80; run.context.r3.u32 = kOwner + 88; break;
  }
  if (run.route == Route::Cold) detail::configured.store(nullptr);
  if (run.route == Route::FrozenGuard) fixture.state->frozen.store(true);
  if (run.route == Route::Unmatched) run.context.lr = 1;
}

void Wrapped(Run& run) {
  switch (run.kind) {
    case Kind::Create: sub_821C9060(run.context,run.memory.data()); break;
    case Kind::Wait: sub_821C90C0(run.context,run.memory.data()); break;
    case Kind::Release: sub_821C9108(run.context,run.memory.data()); break;
    case Kind::Worker: sub_82305E38(run.context,run.memory.data()); break;
    case Kind::Dispatch: sub_821C3048(run.context,run.memory.data()); break;
  }
}
void Execute(Run& run, bool wrapped) {
  Fixture fixture; Prepare(run,fixture);
  active_run = &run;
  kIncoming.Restore();
  try {
    if (wrapped) Wrapped(run); else Original(run.context,run.memory.data());
    run.result.returned = Native::Capture(); // FIRST operation after measured return.
  } catch (const Signal& signal) {
    run.result.returned = Native::Capture(); // Heap store, before bookkeeping/output.
    run.result.caught = 1; run.result.signal = signal.code;
  } catch (...) {
    run.result.returned = Native::Capture(); run.result.unexpected = 1;
  }
  active_run = nullptr;
}
void PrintNative(const char* label, Native state) {
  std::cout << ' ' << label << "=0x" << std::hex << state.error << "/0x" << state.mxcsr << std::dec;
}
void Print(const Run& direct, const Run& wrapped) {
  std::cout << "CASE " << Name(wrapped.kind) << '/' << Name(wrapped.route)
            << '/' << (wrapped.unwind ? "unwind" : "normal")
            << " direct_originals=" << direct.result.originals
            << " wrapped_originals=" << wrapped.result.originals
            << " observer_clocks=" << wrapped.result.clocks
            << " clocks_inside_mock_sdk=" << wrapped.result.original_clocks
            << " observer_resolutions=" << wrapped.result.resolutions
            << " direct_fp=" << direct.result.incoming_fp
            << " wrapped_fp=" << wrapped.result.incoming_fp;
  PrintNative("direct_entry",direct.result.entry); PrintNative("wrapped_entry",wrapped.result.entry);
  PrintNative("direct_produced",direct.result.produced); PrintNative("wrapped_produced",wrapped.result.produced);
  PrintNative("direct_return",direct.result.returned); PrintNative("wrapped_return",wrapped.result.returned);
  std::cout << '\n';
}
void Case(Kind kind, Route route, bool unwind) {
  auto direct = std::make_unique<Run>(); auto wrapped = std::make_unique<Run>();
  direct->kind = wrapped->kind = kind; direct->route = wrapped->route = route;
  direct->unwind = wrapped->unwind = unwind;
  Execute(*direct,false); Execute(*wrapped,true); ++cases;
  Print(*direct,*wrapped);
  for (const auto* run : {direct.get(),wrapped.get()}) {
    Check(run->result.originals == 1, "handoff original executes exactly once",run);
    Check(run->result.actual_context == &run->context && run->result.actual_base == run->memory.data(),
          "handoff preserves exact original context and base",run);
    Check(run->context.r6.u64 == 1 && run->context.r7.u64 == kProducedRegister && run->memory[0] == 1,
          "handoff preserves original context and memory mutations",run);
    const uint32_t expected_r3 = kind == Kind::Create ? kHandle : kind == Kind::Wait ? 1u :
        kind == Kind::Release ? 0u : kind == Kind::Worker ? kOwner : kOwner + 88;
    Check(run->context.r3.u32 == expected_r3,
          "handoff preserves exact original result register",run);
    Check(run->result.caught == unsigned(unwind) && !run->result.unexpected &&
          run->result.signal == (unwind ? 43u : 0u) && run->result.progress == (unwind ? 3u : 4u),
          "handoff preserves exact typed exception and original progress",run);
    Check(run->result.produced_fp == 1 && run->result.produced.mxcsr == 0x7FA0u &&
          run->result.produced.error == kProduced.error,
          "shared original establishes real produced native state",run);
  }
  Check(direct->result.entry == kIncoming && direct->result.incoming_fp == 1,
        "direct baseline receives prescribed native input",direct.get());
  Check(wrapped->result.entry.error == direct->result.entry.error,
        "handoff original incoming LastError changed by observer",wrapped.get());
  Check(wrapped->result.entry.mxcsr == direct->result.entry.mxcsr,
        "handoff original incoming MXCSR changed by observer",wrapped.get());
  Check(wrapped->result.incoming_fp == direct->result.incoming_fp,
        "handoff original FP result changed by observer",wrapped.get());
  Check(wrapped->result.produced == direct->result.produced,
        "direct and wrapped original-produced native states agree",wrapped.get());
  // EH runtime may itself affect native state: compare ACTUAL direct catch,
  // never assume exception state equals the value immediately before throw.
  Check(wrapped->result.returned.error == direct->result.returned.error,
        "handoff produced LastError changed by observer",wrapped.get());
  Check(wrapped->result.returned.mxcsr == direct->result.returned.mxcsr,
        "handoff produced MXCSR changed by observer",wrapped.get());
  if (route == Route::Configured) {
    Check(wrapped->result.clocks + wrapped->result.resolutions != 0,
          "configured case actually executes hostile observer sources",wrapped.get());
  } else {
    // A direct-forwarded original can still perform its own mock SDK work;
    // those explicit Registry callbacks are NOT wrapper before/after work.
    Check(wrapped->result.clocks == wrapped->result.original_clocks && !wrapped->result.resolutions,
          "cold frozen or unmatched wrapper path adds no observer work",wrapped.get());
  }
}
} // namespace handoff_host_state_test

// Public wrappers above must reach THESE distinct generated-original stand-ins
// exactly once. The shared body is also called directly as the EH/FP control.
REX_EXTERN(__imp__sub_821C9060) { handoff_host_state_test::Original(ctx,base); }
REX_EXTERN(__imp__sub_821C90C0) { handoff_host_state_test::Original(ctx,base); }
REX_EXTERN(__imp__sub_821C9108) { handoff_host_state_test::Original(ctx,base); }
REX_EXTERN(__imp__sub_82305E38) { handoff_host_state_test::Original(ctx,base); }
REX_EXTERN(__imp__sub_821C3048) { handoff_host_state_test::Original(ctx,base); }

int main() {
  using namespace handoff_host_state_test;
  const auto saved = Native::Capture();
  try { throw Signal{0}; } catch (const Signal&) {} // EH first-touch outside measurements.
  for (const auto route : {Route::Configured,Route::Cold,Route::FrozenGuard})
    for (const auto kind : {Kind::Create,Kind::Wait,Kind::Release,Kind::Worker,Kind::Dispatch})
      for (bool unwind : {false,true}) Case(kind,route,unwind);
  for (const auto kind : {Kind::Create,Kind::Dispatch})
    for (bool unwind : {false,true}) Case(kind,Route::Unmatched,unwind);
  saved.Restore();
  std::cout << "Frame handoff host-state wrappers: " << checks << " checks, "
            << failures << " failures, " << cases << " cases\n";
  std::cout << "PROOFS runtime_admitted=" << kRuntimeAdmitted
            << " sdk_integration_proven=" << kSdkIntegrationProven
            << " game_proven=" << kGameProven << " causality_proven=" << kCausalityProven
            << " fps_proven=" << kFpsProven << '\n';
  // Explicit transport completion, not a substitute for any failed assertion.
  // The separate leaf predecessor lost its stdout tail despite exiting0.
  std::cout.flush();
  if (!std::cout.good()) return 3;
  constexpr char marker[] = "FRAME_HANDOFF_HOST_STATE_STDOUT_FLUSH_EOF\n";
  DWORD marker_bytes = 0;
  if (!WriteFile(GetStdHandle(STD_ERROR_HANDLE), marker, sizeof(marker)-1, &marker_bytes, nullptr) ||
      marker_bytes != sizeof(marker)-1) return 4;
  saved.Restore();
  return failures ? 1 : 0;
}
