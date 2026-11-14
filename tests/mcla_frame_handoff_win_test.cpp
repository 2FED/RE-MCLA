// Mock PPC originals + explicit registry callbacks only. No Runtime setup,
// game module, GPU, native semaphore, window, save or filesystem operations.
#include "../src/mcla_frame_handoff_win.cpp"
#include "../src/mcla_phase_census_win.cpp"
#include <nlohmann/json.hpp>
#include <iostream>
#include <new>
#include <condition_variable>
#include <thread>

namespace {
namespace handoff = mcla::frame_handoff;
namespace detail = handoff::detail;
namespace api = detail::api;
using Json = nlohmann::json;
std::atomic<unsigned> checks{}, failures{}, calls{};
constexpr uint64_t cookie = 123;
constexpr uint32_t handle = 500, owner = 0x4000;
constexpr DWORD incoming_error = 0x1234, original_error = 0x5678;
constexpr char token[] = "55555555555555555555555555555555";
std::atomic<uint64_t> stamp{101};
uint64_t phase_id = 10;
api::Registry* registry{};
bool throw_original{}, freeze_inside{}, dispatch_render = true, extra_null_operation{};
std::optional<handoff::FrozenOutput> inside_output;
struct Barrier {
  std::mutex mutex; std::condition_variable cv; bool reached = false, released = false;
  void Hold() {
    std::unique_lock lock(mutex); reached = true; cv.notify_all();
    if (!cv.wait_for(lock, std::chrono::seconds(5), [&] { return released; })) std::abort();
  }
  void Wait() {
    std::unique_lock lock(mutex);
    if (!cv.wait_for(lock, std::chrono::seconds(5), [&] { return reached; })) std::abort();
  }
  void Release() { std::lock_guard lock(mutex); released = true; cv.notify_all(); }
};
Barrier* held_original{};
void Check(bool value, const char* label) { ++checks; if (!value) { ++failures; std::cerr << "FAIL " << label << '\n'; } }
uint64_t Clock() { SetLastError(0x9999); return ++stamp; }
uint64_t CollectorClock(void*) { return Clock(); }
uint64_t Frequency(void*) { return 1000; }
uint64_t Resolve(detail::State&, uint32_t value) { return value == handle ? cookie : 0; }
struct Fixture {
  api::Registry transport;
  std::unique_ptr<detail::State> state = std::make_unique<detail::State>(transport,
      detail::sem::ClockProvider{nullptr, CollectorClock, Frequency});
  PPCContext context{};
  std::array<uint8_t, 64> memory{};
  explicit Fixture(bool full_horizon = false) {
    detail::local.~Local(); new (&detail::local) detail::Local;
    stamp = 101; registry = &transport; calls = 0;
    throw_original = freeze_inside = extra_null_operation = false; dispatch_render = true; phase_id = 10; inside_output.reset();
    held_original = nullptr;
    state->clock = Clock; state->test_resolver = Resolve; state->token = token; state->begin = 100; state->frequency = 1000;
    state->end = full_horizon ? 180100 : 10000;
    Check(state->collector.Configure({100, state->end, 1000}, token, true) == detail::sem::Status::kOk, "fixture collector configured");
    detail::configured.store(state.get());
  }
  ~Fixture() {
    detail::configured.store(nullptr); registry = nullptr;
    detail::local.~Local(); new (&detail::local) detail::Local;
  }
  void Select();
};
void OriginalCreate(PPCContext& context, uint8_t*) {
  ++calls; Check(GetLastError() == incoming_error, "create original receives LastError");
  registry->Create({cookie, 0, 32767, api::Origin::kFresh});
  if (held_original) held_original->Hold();
  context.r3.u32 = handle; context.r5.u64 = 0xAABB;
  if (freeze_inside) {
    // Creation observed but returned handle is not yet independently bound.
    inside_output = handoff::Freeze(token);
  }
  SetLastError(original_error);
  if (throw_original) throw std::runtime_error("original create");
}
void Fixture::Select() {
  context.lr = detail::sem::kCreationLR; context.r3.u32 = 0; context.r31.u32 = owner;
  SetLastError(incoming_error);
  detail::Create(OriginalCreate, context, memory.data());
  Check(calls == 1 && context.r3.u32 == handle && context.r5.u64 == 0xAABB && GetLastError() == original_error,
        "create runs exactly once and preserves original effects");
}
void OriginalWait(PPCContext& context, uint8_t*) {
  ++calls; Check(GetLastError() == incoming_error, "wait original receives LastError");
  if (context.r3.u32) {
    const auto id = registry->Begin({cookie, api::Kind::kWait, 0, api::TimeoutClass::kInfinite});
    if (held_original) held_original->Hold();
    if (freeze_inside) inside_output = handoff::Freeze(token);
    registry->End({id, api::Kind::kWait, api::Result::kSuccess, {}});
  } else if (extra_null_operation) {
    registry->Begin({cookie + 1, api::Kind::kWait, 0, api::TimeoutClass::kZero});
  }
  context.r3.u32 = 1; context.r6.u64 = 0xAABBCCDD;
  SetLastError(original_error);
  if (throw_original) throw std::runtime_error("original wait");
}
void OriginalDispatch(PPCContext& context, uint8_t* base) {
  ++calls; Check(GetLastError() == incoming_error, "dispatch original receives LastError");
  if (dispatch_render) {
    context.r3.u32 = 0;
    const auto ticket = handoff::Enter(handoff::Phase::kRender, phase_id, context, base);
    handoff::Leave(ticket, context, base, true);
  }
  SetLastError(original_error);
}
void Wait(PPCContext& context, uint8_t* base, uint32_t guest_handle = handle) {
  context.lr = 0x82305EB8; context.r31.u32 = owner; context.r3.u32 = guest_handle;
  SetLastError(incoming_error); detail::Operation(OriginalWait, context, base, true);
  Check(GetLastError() == original_error && context.r6.u64 == 0xAABBCCDD, "wait result state preserved");
}
void Dispatch(PPCContext& context, uint8_t* base) {
  context.lr = 0x82305F80; context.r31.u32 = owner; context.r3.u32 = owner + 88;
  SetLastError(incoming_error); detail::Dispatch(OriginalDispatch, context, base);
  Check(GetLastError() == original_error, "dispatch LastError preserved");
}
void SkippedWorker(PPCContext& context, uint8_t* base) {
  Wait(context, base); Wait(context, base); Wait(context, base);
  dispatch_render = false; Dispatch(context, base);
  Wait(context, base); dispatch_render = true; phase_id = 0; Dispatch(context, base);
  Wait(context, base); phase_id = 10; Dispatch(context, base);
}
void SimpleWorker(PPCContext& context, uint8_t* base) { Wait(context, base); if (!freeze_inside) Dispatch(context, base); }
void NullWorker(PPCContext& context, uint8_t* base) { Wait(context, base, 0); }
void BasicAndSkip() {
  Fixture fixture; fixture.Select(); calls = 0;
  fixture.context.r3.u32 = owner; SetLastError(incoming_error);
  detail::Worker(SkippedWorker, fixture.context, fixture.memory.data());
  Check(!fixture.state->failed.load() && !detail::local.tracker->failed(), "skips/no-op/phase0 do not poison Tracker");
  auto output = handoff::Freeze(token); const auto links = Json::parse(output.linkage);
  Check(links["scopes"].size() == 1 && links["scopes"][0]["phase_call_id"] == 10 &&
        links["scopes"][0]["wait_operation"] == 5, "actual render binds only fifth exact SDK wait");
  unsigned retired = 0, zero_consumed = 0;
  for (const auto& event : links["worker_events"]) {
    if (event["retired_operation"].get<uint64_t>()) ++retired;
    if (event["event"] == unsigned(detail::worker::Event::kRenderConsumed) && event["phase_call_id"] == 0 && event["operation"] == 4) ++zero_consumed;
  }
  Check(retired == 3 && zero_consumed == 1, "two skipped + no-op retired; phase0 consumed independently");
  Check(links["operation_annotations"].size() == 5 && links["freeze_qpc"] == Json::parse(output.raw_semaphore)["freeze_qpc"], "annotations and exact cut retained");
  Check(!Json::parse(output.prefix_semaphore)["admission_inventory_complete"].get<bool>(), "native coverage not inferred from integration");
}
void FreezeRaces() {
  { Fixture fixture; freeze_inside = true; fixture.Select();
    Check(inside_output.has_value(), "freeze in create original collected unbound selection");
    auto frozen = Json::parse(inside_output->linkage);
    Check(!frozen["observer_failed"].get<bool>() && frozen["selected_handle"] == 0,
          "late create binding cannot change pre-cut health or fabricate bound handle");
  }
  { Fixture fixture; fixture.Select(); freeze_inside = true; fixture.context.r3.u32 = owner;
    detail::Worker(SimpleWorker, fixture.context, fixture.memory.data());
    const auto frozen = Json::parse(inside_output->raw_semaphore);
    Check(frozen["pending_operations"] == 1 && frozen["counts"]["completed_operations"] == 0,
          "freeze inside wait retains pending native operation despite late End");
    Check(!fixture.state->collector.raw_summary().post_stop_mutated, "late wait helper/SDK completion leaves cut immutable");
  }
}
void FaultsAndExceptions() {
  { Fixture fixture; fixture.Select(); throw_original = true; fixture.context.r3.u32 = owner;
    bool threw = false; try { detail::Worker(SimpleWorker, fixture.context, fixture.memory.data()); } catch (...) { threw = true; }
    Check(threw && GetLastError() == original_error && fixture.state->failed.load(), "original unwind/error propagated, observer fails closed");
  }
  { Fixture fixture; fixture.Select(); extra_null_operation = true; fixture.context.r3.u32 = owner;
    detail::Worker(NullWorker, fixture.context, fixture.memory.data());
    Check(fixture.state->failed.load(), "null helper with hidden SDK admission cannot pass");
  }
  { Fixture fixture; fixture.Select(); fixture.context.lr = 1;
    auto ticket = handoff::Enter(handoff::Phase::kUpdate, 1, fixture.context, fixture.memory.data());
    handoff::Leave(ticket, fixture.context, fixture.memory.data(), true);
    Check(fixture.state->failed.load() && !ticket.entered, "unknown update caller fails observer only");
  }
  { Fixture fixture; fixture.Select(); fixture.context.r3.u32 = owner + 1;
    detail::Worker(SimpleWorker, fixture.context, fixture.memory.data());
    Check(fixture.state->failed.load(), "wrong worker owner cannot produce an eligible relation");
  }
}
void ConcurrentCuts() {
  for (bool creation : {true, false}) {
    Fixture fixture; if (!creation) fixture.Select();
    Barrier barrier; held_original = &barrier;
    std::atomic<bool> done{false};
    std::thread original([&] {
      if (creation) fixture.Select();
      else { fixture.context.r3.u32 = owner; detail::Worker(SimpleWorker, fixture.context, fixture.memory.data()); }
      done.store(true, std::memory_order_release);
    });
    barrier.Wait();
    auto output = handoff::Freeze(token);
    const auto raw = Json::parse(output.raw_semaphore), links = Json::parse(output.linkage);
    barrier.Release();
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
    while (!done.load(std::memory_order_acquire) && std::chrono::steady_clock::now() < deadline)
      std::this_thread::sleep_for(std::chrono::milliseconds(1));
    if (!done.load(std::memory_order_acquire)) std::abort();
    original.join(); held_original = nullptr;
    Check(!links["observer_failed"].get<bool>() && !fixture.state->failed.load(), "cross-thread cut cannot fabricate a pre-cut observer fault");
    Check(!fixture.state->collector.raw_summary().post_stop_mutated, "concurrent late completion leaves stopped core unchanged");
    if (creation) Check(links["selected_handle"] == 0 && raw["counts"]["stored_operations"] == 0, "concurrent create cut remains unbound");
    else Check(raw["pending_operations"] == 1 && raw["counts"]["completed_operations"] == 0, "concurrent wait cut remains pending");
  }
}
void OriginalRelease(PPCContext& context, uint8_t*) {
  Check(GetLastError() == incoming_error, "release original error input preserved");
  const auto id = registry->Begin({cookie, api::Kind::kRelease, 1, api::TimeoutClass::kNotApplicable});
  registry->End({id, api::Kind::kRelease, api::Result::kSuccess, 0});
  context.r3.u32 = 0; SetLastError(original_error);
}
void MainScopeAndPhaseGlue() {
  Fixture fixture; fixture.Select();
  namespace phase = mcla::phase_census;
  auto phase_state = std::make_unique<phase::detail::State>();
  phase_state->begin = 100; phase_state->end = 10000; phase_state->frequency = 1000;
  phase_state->clock = Clock; phase_state->token = token;
  phase::detail::local = {};
  auto noop = +[](PPCContext&, uint8_t*) { SetLastError(original_error); };
  fixture.context.lr = 0x821316A0; SetLastError(incoming_error);
  phase::detail::Invoke(phase_state.get(), phase::detail::Kind::Update, noop, fixture.context, fixture.memory.data());
  auto coordinator = +[](PPCContext& context, uint8_t* base) {
    context.lr = detail::sem::kReleaseLR; context.r3.u32 = handle;
    SetLastError(incoming_error); detail::Operation(OriginalRelease, context, base, false);
  };
  fixture.context.lr = 0x821316B8; fixture.context.r3.u32 = owner;
  phase::detail::Invoke(phase_state.get(), phase::detail::Kind::Coordinator, coordinator, fixture.context, fixture.memory.data());
  Check(!fixture.state->failed.load() && GetLastError() == original_error, "actual phase Invoke forwards main update/coordinator without error changes");
  fixture.context.lr = 0x82306048; fixture.context.r3.u32 = handle;
  SetLastError(incoming_error); detail::Operation(OriginalRelease, fixture.context, fixture.memory.data(), false);
  auto output = handoff::Freeze(token); const auto links = Json::parse(output.linkage), raw = Json::parse(output.raw_semaphore);
  Check(links["scopes"].size() == 2 && links["scopes"][0]["phase_call_id"] == phase_state->scopes[0].call &&
        links["scopes"][1]["phase_call_id"] == phase_state->scopes[1].call, "phase glue uses exact admitted IDs, not latest or equal-count mapping");
  Check(raw["operations"][0]["context_kind_enum"] == unsigned(mcla::semaphore_observer::ContextKind::kUpdate) &&
        raw["operations"][0]["serial"] == links["scopes"][0]["serial"] &&
        raw["operations"][0]["thread_epoch"] == links["scopes"][0]["thread_epoch"], "normal release carries exact completed main update identity");
  Check(raw["operations"][1]["context_kind_enum"] == unsigned(mcla::semaphore_observer::ContextKind::kControl), "worker self-release remains control, not a new update");
  Check(links["operation_annotations"][0]["phase_call_id"] == phase_state->scopes[1].call &&
        links["operation_annotations"][0]["caller_lr"] == detail::sem::kReleaseLR, "release sidecar binds exact coordinator call and LR");
  phase::detail::local = {};
}
void ExplicitCoverageClaim() {
  { Fixture fixture; fixture.Select(); fixture.state->coverage_claim_requested = true;
    const auto output = handoff::Freeze(token);
    Check(Json::parse(output.prefix_semaphore)["admission_inventory_complete"].get<bool>() &&
          Json::parse(output.linkage)["coverage_claim_requested"].get<bool>(), "explicit bound healthy package claim retained as admission attestation only");
  }
  { Fixture fixture; fixture.Select(); fixture.state->coverage_claim_requested = true; fixture.state->failed.store(true);
    const auto output = handoff::Freeze(token);
    Check(!Json::parse(output.prefix_semaphore)["admission_inventory_complete"].get<bool>(), "host fault prevents requested complete coverage");
  }
}
void ReadOnlyReadyWindow() {
  bool rejected=false;
  try { handoff::ObservationWindow(token); } catch(const std::runtime_error&) { rejected=true; }
  Check(rejected,"unconfigured readiness window rejects");
  Fixture fixture(true); fixture.Select();
  const auto first=handoff::ObservationWindow(token), second=handoff::ObservationWindow(token);
  Check(first.begin==100 && first.end==180100 && first.frequency==1000 && second.end==first.end,
        "readiness uses actual configured horizon without rebasing");
  fixture.state->failed=true; rejected=false;
  try { handoff::ObservationWindow(token); } catch(const std::runtime_error&) { rejected=true; }
  Check(rejected,"sticky failure prevents readiness");
  fixture.state->failed=false;
  const auto output=handoff::Freeze(token);
  Check(output.freeze_qpc==Json::parse(output.linkage)["freeze_qpc"].get<uint64_t>(),
        "ready sidecar receives authoritative retained freeze");
  rejected=false;
  try { handoff::ObservationWindow(token); } catch(const std::runtime_error&) { rejected=true; }
  Check(rejected,"frozen collector cannot supply new readiness");
}
void LongWorker(PPCContext& context, uint8_t* base) {
  phase_id = 0; // Lifetime events, outside the late phase census.
  for (unsigned i = 0; i != 4097; ++i) { Wait(context, base); Dispatch(context, base); }
}
void LifetimeCapacity() {
  {
    Fixture fixture(true); fixture.Select(); fixture.context.r3.u32 = owner;
    detail::Worker(LongWorker, fixture.context, fixture.memory.data());
    Check(!fixture.state->failed.load(), "helper 4097 retains exact SDK observation beyond old bound");
    const auto output = handoff::Freeze(token);
    const auto links = Json::parse(output.linkage), raw = Json::parse(output.raw_semaphore);
    Check(raw["counts"]["stored_operations"] == 4097 && raw["counts"]["overflow_operations"] == 0,
          "lifetime operations retained without reset or loss");
    Check(links["worker_events"].size() > 16384 && links["lost_events"] == 0 &&
          !links["observer_failed"].get<bool>(), "old event bound crossed without lost events");
    Check(links["operation_annotations"].size() == 4097 &&
          links["operation_annotations"].back()["operation_id"] == 4097,
          "companion annotation retains same exact operation ID");
  }
  {
    Fixture fixture(true); fixture.Select();
    for (size_t i = 0; i != detail::kEvents; ++i) detail::Record(*fixture.state, 200, {});
    Check(fixture.state->event_count == detail::kEvents && !fixture.state->failed.load(),
          "exact worker event capacity admitted");
    detail::Record(*fixture.state, 201, {});
    Check(fixture.state->event_count == detail::kEvents && fixture.state->lost_events == 1 &&
          fixture.state->failed.load(), "worker overflow remains sticky fail closed without overwrite");
    const auto output = handoff::Freeze(token); const auto links = Json::parse(output.linkage);
    Check(links["worker_events"].size() == detail::kEvents && links["lost_events"] == 1 &&
          links["observer_failed"].get<bool>(), "worker overflow serialized without dropping loss");
  }
  {
    // Serializer stress only: maximum-width fields are NOT a valid gameplay trace.
    Fixture fixture(true); fixture.Select();
    for (size_t i = 0; i != mcla::history_limits::kOperations; ++i) {
      const auto id = fixture.transport.Begin({cookie, api::Kind::kWait, 0, api::TimeoutClass::kInfinite});
      fixture.transport.End({id, api::Kind::kWait, api::Result::kTimeout, {}});
    }
    fixture.state->scope_count = detail::kScopes;
    for (auto& row : fixture.state->scopes) {
      row.phase = row.entry = row.exit = row.lr = UINT64_MAX; row.native_thread_id = UINT32_MAX;
      row.context = {{UINT64_MAX,UINT64_MAX},{UINT64_MAX,UINT64_MAX},UINT64_MAX,UINT64_MAX,
                     mcla::frame_context::Kind::kWorkerRender};
    }
    fixture.state->event_count = detail::kEvents;
    for (auto& row : fixture.state->events) {
      row.qpc = UINT64_MAX; row.native_thread_id = UINT32_MAX;
      row.key = {UINT64_MAX,UINT64_MAX,{UINTPTR_MAX,UINTPTR_MAX},UINT32_MAX,UINT64_MAX};
      row.transition = {detail::worker::Event::kFault,detail::worker::Reason::kObserverFault,
          UINT64_MAX,UINT64_MAX,UINT64_MAX,UINT64_MAX,UINT64_MAX,detail::worker::Reason::kObserverFault,false,false};
    }
    const auto output = handoff::Freeze(token); const auto links = Json::parse(output.linkage);
    Check(output.linkage.size() <= mcla::history_limits::kHandoffJsonBytes &&
          links["worker_events"].size() == detail::kEvents && links["scopes"].size() == detail::kScopes &&
          links["operation_annotations"].size() == mcla::history_limits::kOperations,
          "full-size serialization fits byte bound without truncation");
    std::cout << "HISTORY_MAX handoff_bytes=" << output.linkage.size()
              << " state_bytes=" << sizeof(detail::State) << " frozen_bytes=" << sizeof(detail::sem::Frozen) << '\n';
  }
}
}
REX_EXTERN(__imp__sub_821C9060) { (void)ctx; (void)base; }
REX_EXTERN(__imp__sub_821C90C0) { (void)ctx; (void)base; }
REX_EXTERN(__imp__sub_821C9108) { (void)ctx; (void)base; }
REX_EXTERN(__imp__sub_82305E38) { (void)ctx; (void)base; }
REX_EXTERN(__imp__sub_821C3048) { (void)ctx; (void)base; }
REX_EXTERN(__imp__sub_822C1FA8) { (void)ctx; (void)base; }
REX_EXTERN(__imp__sub_82305D00) { (void)ctx; (void)base; }
REX_EXTERN(__imp__sub_82131008) { (void)ctx; (void)base; }
int main() {
  BasicAndSkip(); FreezeRaces(); FaultsAndExceptions(); ConcurrentCuts(); MainScopeAndPhaseGlue(); ExplicitCoverageClaim(); ReadOnlyReadyWindow(); LifetimeCapacity();
  std::cout << "Frame handoff mock wrappers: " << checks << " checks, " << failures << " failures\n";
  return failures ? 1 : 0;
}
