// Synthetic callback metadata only; no SDK, guest, semaphore, window or files.
// Build OFF and ON (/DREXGLUE_SEMAPHORE_WITNESS=1). Primary owns execution.
#include "../src/mcla_semaphore_collector_win.h"
#include <iostream>
#include <stdexcept>
#include <type_traits>
#include <thread>
#include <chrono>
#include <cstdlib>

namespace {
namespace col = mcla::semaphore_collector;
namespace api = col::api;
namespace core = col::core;
namespace bridge = col::bridge;
unsigned checks = 0, failures = 0;
constexpr DWORD sentinel = 0xABCDEFu;
constexpr uint64_t cookie = 123;
constexpr uint32_t handle = 500;
void Check(bool value, const char* reason) { ++checks; if (!value) { ++failures; std::cerr << "FAIL " << reason << '\n'; } }
[[maybe_unused]] bool Has(const std::string& text, std::string_view value) { return text.find(value) != std::string::npos; }
struct Clock {
  uint64_t now = 101, frequency = 1000;
  bool throw_read = false, throw_frequency = false;
  static uint64_t Read(void* opaque) {
    auto& state = *static_cast<Clock*>(opaque); SetLastError(1234);
    if (state.throw_read) throw std::runtime_error("synthetic clock"); return state.now;
  }
  static uint64_t Frequency(void* opaque) {
    auto& state = *static_cast<Clock*>(opaque); SetLastError(5678);
    if (state.throw_frequency) throw std::runtime_error("synthetic frequency"); return state.frequency;
  }
};
constexpr core::Window window{100, 20000, 1000};
constexpr mcla::frame_context::Lane lane{0x1000, 0x2000};
struct Setup {
  api::Registry registry; Clock clock;
  std::unique_ptr<col::Collector> collector = std::make_unique<col::Collector>(registry, col::ClockProvider{&clock, Clock::Read, Clock::Frequency});
  explicit Setup(core::Window supplied = window) { Check(collector->Configure(supplied, "44444444444444444444444444444444", true) == col::Status::kOk, "explicit ON configuration"); }
  void Select(bool bind = true) {
    col::Collector::ScopedCreate scope(*collector, {col::kCreationLR, 0, 0x4000, lane});
    clock.now = 101; registry.Create({cookie, 0, 32767, api::Origin::kFresh});
    Check(scope.selected_cookie() == cookie && scope.selected_epoch() == 1, "selected actual creation cookie and epoch returned in helper scope");
    if (bind) Check(collector->BindReturnedHandle(scope, cookie, handle), "returned handle independently resolved to exact selected cookie");
  }
  col::CoverageAttestation Attest() const {
    return {"44444444444444444444444444444444", window.begin_qpc, window.qpc_frequency, 1,
            bridge::Coverage::kCompleteThroughFreeze, bridge::Coverage::kCompleteThroughFreeze,
            bridge::Coverage::kCompleteThroughFreeze, true};
  }
  col::FreezeResult Freeze(bool attest = false, uint64_t cut = 1000) {
    clock.now = cut; return attest ? collector->Freeze(Attest()) : collector->Freeze();
  }
};
void DefaultAndOff() {
  static_assert(!std::is_copy_constructible_v<col::Collector> && !std::is_move_constructible_v<col::Collector>);
  Check(!col::kNativeAdmitted && !col::kSimulationProven && !col::kFrameToStepMappingProven, "no native/simulation/frame-step proof");
  api::Registry registry; Clock clock;
  auto collector = std::make_unique<col::Collector>(registry, col::ClockProvider{&clock, Clock::Read, Clock::Frequency});
  SetLastError(sentinel);
  Check(collector->Configure(window, "44444444444444444444444444444444") == col::Status::kDisabled &&
        !registry.registered() && collector->counters().failures == 0 && GetLastError() == sentinel,
        "runtime default false allocates no journal/registers no table");
  { col::Collector::ScopedCreate scope(*collector, {col::kCreationLR, 0, 0x4000, lane});
    Check(!collector->BindReturnedHandle(scope, cookie, handle) && !collector->counters().failures,
          "default-off helper binding attempt is inert, not a false observer fault"); }
  if constexpr (!api::kCompiledEnabled) {
    Check(collector->Configure(window, "44444444444444444444444444444444", true) == col::Status::kDisabled &&
          !registry.registered() && collector->Freeze().status == col::Status::kNotConfigured &&
          GetLastError() == sentinel, "compile OFF remains inert even with runtime enable");
  }
}
[[maybe_unused]] void SelectionAndBindings() {
  Setup setup;
  { col::Collector::ScopedCreate auxiliary(*setup.collector, {0x823060B8, 0, 0x9999, lane});
    setup.registry.Create({900, 0, 32767, api::Origin::kFresh}); Check(!auxiliary.selected_cookie(), "auxiliary +20 never selected"); }
  setup.registry.Create({901, 0, 32767, api::Origin::kFresh});
  setup.Select();
  Check(setup.registry.Begin({902, api::Kind::kWait, 0, api::TimeoutClass::kZero}) == 0 &&
        setup.collector->raw_summary().counts.observed_operations == 0,
        "unrelated resolved cookie ignored without polluting selected inventory");
  auto frozen = setup.Freeze(true);
  Check(frozen.frozen && frozen.frozen->cookie() == cookie && frozen.frozen->epoch() == 1 && frozen.frozen->freeze_qpc() == 1000 &&
        frozen.frozen->returned_handle() == handle && frozen.frozen->owner_guest() == 0x4000 &&
        frozen.frozen->counters().ignored_creations == 2 && frozen.frozen->counters().ignored_operations == 1,
        "selected binding and ignored-object census detached at actual cut");
  if (frozen.frozen) Check(Has(frozen.frozen->journal().PrefixJson().json, "\"admission_inventory_complete\":true"),
                           "only explicit externally supplied bound attestation can claim coverage");
  Setup unbound; unbound.Select(false); auto unknown = unbound.Freeze(true);
  Check(unknown.frozen && Has(unknown.frozen->journal().PrefixJson().json, "\"admission_inventory_complete\":false"),
        "attestation cannot cover unbound returned handle");
  Setup mismatch;
  { col::Collector::ScopedCreate scope(*mismatch.collector, {col::kCreationLR, 0, 0x4000, lane});
    mismatch.registry.Create({cookie, 0, 32767, api::Origin::kFresh});
    Check(!mismatch.collector->BindReturnedHandle(scope, cookie + 1, handle), "independent resolved-cookie mismatch fails closed"); }
  auto mismatched = mismatch.Freeze(true);
  Check(mismatched.frozen && mismatch.collector->counters().failures &&
        Has(mismatched.frozen->journal().PrefixJson().json, "\"observer_failed\":true"), "mismatched binding remains raw failed/coverage unknown");
  Setup no_selection; auto empty = no_selection.Freeze();
  Check(empty.status == col::Status::kNoSelection && !empty.frozen && !no_selection.registry.admitting(), "missing selected creation never fabricated");
}
[[maybe_unused]] void OperationsAndContext() {
  Setup setup; setup.Select();
  col::OperationIntent release{cookie, 11, handle, lane, col::kReleaseLR, api::Kind::kRelease,
                               {core::ContextKind::kUpdate, {1, 7}}};
  { col::Collector::ScopedOperation scope(*setup.collector, release);
    setup.clock.now = 110; const auto ticket = setup.registry.Begin({cookie, api::Kind::kRelease, 1, api::TimeoutClass::kNotApplicable});
    unsigned original_calls = 0; ++original_calls; SetLastError(sentinel);
    setup.clock.now = 120; Check(setup.registry.End({ticket, api::Kind::kRelease, api::Result::kSuccess, 0}) &&
          original_calls == 1 && GetLastError() == sentinel && !scope.wait_observation(), "release original seam independent, LastError preserved, no fake wait credit"); }
  { col::Collector::ScopedOperation scope(*setup.collector, {cookie, 22, handle + 1, lane, 0x82305EB8, api::Kind::kWait, {}});
    setup.clock.now = 130; const auto ticket = setup.registry.Begin({cookie, api::Kind::kWait, 0, api::TimeoutClass::kInfinite});
    setup.clock.now = 140; setup.registry.End({ticket, api::Kind::kWait, api::Result::kSuccess, {}});
    const auto observation = scope.wait_observation();
    Check(observation && observation->op_id == ticket && observation->phase_call_id == 22 && observation->annotated &&
          observation->outcome == core::WaitResult::kSuccess, "exact observed wait ID/outcome returned; aliased handle not a new epoch"); }
  setup.clock.now = 150; const auto unannotated = setup.registry.Begin({cookie, api::Kind::kRelease, 1, api::TimeoutClass::kNotApplicable});
  setup.clock.now = 160; setup.registry.End({unannotated, api::Kind::kRelease, api::Result::kFailed, {}});
  { auto wrong = release; wrong.expected_kind = api::Kind::kWait;
    col::Collector::ScopedOperation scope(*setup.collector, wrong);
    setup.clock.now = 170; const auto ticket = setup.registry.Begin({cookie, api::Kind::kRelease, 1, api::TimeoutClass::kNotApplicable});
    setup.clock.now = 180; setup.registry.End({ticket, api::Kind::kRelease, api::Result::kFailed, {}}); }
  auto frozen = setup.Freeze(true);
  Check(frozen.frozen && frozen.frozen->annotations().size() == 4 && frozen.frozen->annotations()[0].phase_call_id == 11 &&
        frozen.frozen->annotations()[1].phase_call_id == 22 && !frozen.frozen->annotations()[2].annotated &&
        !frozen.frozen->annotations()[3].annotated, "phase IDs preserved only for matching per-thread annotation");
  if (frozen.frozen) {
    const auto rows = frozen.frozen->journal().raw_operations();
    Check(rows[0].context.generation == mcla::frame_context::Id{1, 7} && rows[2].context.kind == core::ContextKind::kUnknown &&
          rows[3].context.kind == core::ContextKind::kUnknown && frozen.frozen->counters().failures == 0,
          "unannotated/cross-kind accesses remain recorded Unknown, NOT operation coverage gaps");
  }
  Setup wrong_lane; wrong_lane.Select();
  { col::Collector::ScopedOperation scope(*wrong_lane.collector, {cookie, 99, handle, {}, col::kReleaseLR, api::Kind::kRelease,
                                                               {core::ContextKind::kUpdate, {1, 7}}});
    wrong_lane.clock.now = 110; const auto ticket = wrong_lane.registry.Begin({cookie, api::Kind::kRelease, 1, api::TimeoutClass::kNotApplicable});
    wrong_lane.clock.now = 120; wrong_lane.registry.End({ticket, api::Kind::kRelease, api::Result::kFailed, {}}); }
  auto unknown = wrong_lane.Freeze(true);
  Check(unknown.frozen && unknown.frozen->journal().raw_operations().size() == 1 &&
        unknown.frozen->journal().raw_operations()[0].context.kind == core::ContextKind::kUnknown,
        "absent lane removes context credit but never drops a real selected access");
}
[[maybe_unused]] void PendingAndFreeze() {
  Setup setup; setup.Select();
  col::Collector::ScopedOperation scope(*setup.collector, {cookie, 44, handle, lane, 0x82305EB8, api::Kind::kWait, {}});
  setup.clock.now = 150; const auto ticket = setup.registry.Begin({cookie, api::Kind::kWait, 0, api::TimeoutClass::kInfinite});
  SetLastError(sentinel); auto frozen = setup.Freeze();
  Check(frozen.frozen && GetLastError() == sentinel && frozen.frozen->journal().pending_operations() == 1 &&
        !scope.wait_observation(), "actual cut captures pending without drain or inferred outcome");
  if (!frozen.frozen) return;
  const auto raw = frozen.frozen->journal().RawCoreJson().json;
  const auto prefix = frozen.frozen->journal().PrefixJson().json;
  setup.clock.now = 1100; setup.registry.End({ticket, api::Kind::kWait, api::Result::kSuccess, {}});
  const auto live = setup.collector->raw_summary();
  Check(live.pending == 1 && !live.post_stop_mutated && setup.collector->counters().late_callbacks == 1 &&
        frozen.frozen->journal().RawCoreJson().json == raw && frozen.frozen->journal().PrefixJson().json == prefix &&
        !scope.wait_observation(), "late End only increments separate counter; stopped Journal and snapshot untouched");
  Check(setup.collector->Freeze().status == col::Status::kAlreadyFrozen &&
        setup.registry.Begin({cookie, api::Kind::kWait, 0, api::TimeoutClass::kZero}) == 0,
        "one freeze, no new admissions; no recapture/fake drain");
}
[[maybe_unused]] void ExtraAdmissionInvalidatesHelper() {
  for (bool other_first : {false, true}) {
    Setup setup; setup.Select();
    { col::Collector::ScopedOperation scope(*setup.collector, {cookie, 77, handle, lane, 0x82305EB8, api::Kind::kWait, {}});
      if (other_first) Check(setup.registry.Begin({cookie + 1, api::Kind::kWait, 0, api::TimeoutClass::kZero}) == 0,
                             "other-cookie admission before selected wait ignored in journal");
      setup.clock.now = 110; const auto ticket = setup.registry.Begin({cookie, api::Kind::kWait, 0, api::TimeoutClass::kZero});
      setup.clock.now = 120; setup.registry.End({ticket, api::Kind::kWait, api::Result::kSuccess, {}});
      if (!other_first) Check(scope.wait_observation().has_value(), "single selected wait initially eligible before extra admission");
      if (!other_first) Check(setup.registry.Begin({cookie + 1, api::Kind::kWait, 0, api::TimeoutClass::kZero}) == 0,
                             "other-cookie admission after selected wait ignored in journal");
      Check(scope.sdk_admission_count() == 2 && !scope.wait_observation(),
            "any extra other-cookie Begin removes helper eligibility in either order"); }
    auto frozen = setup.Freeze(true);
    Check(frozen.frozen && frozen.frozen->journal().raw_counts().observed_operations == 1 &&
          frozen.frozen->annotations().size() == 1 && !frozen.frozen->annotations()[0].annotated &&
          frozen.frozen->annotations()[0].phase_call_id == 0 && frozen.frozen->counters().failures == 0,
          "selected row still retained; helper multiplicity is not an operation-coverage gap");
  }
  Setup nested; nested.Select();
  { col::Collector::ScopedOperation outer(*nested.collector, {cookie, 88, handle, lane, 0x82305EB8, api::Kind::kWait, {}});
    nested.clock.now = 110; const auto ticket = nested.registry.Begin({cookie, api::Kind::kWait, 0, api::TimeoutClass::kZero});
    nested.clock.now = 120; nested.registry.End({ticket, api::Kind::kWait, api::Result::kSuccess, {}});
    { col::Collector::ScopedOperation inner(*nested.collector, {cookie + 1, 89, handle + 1, lane, 1, api::Kind::kWait, {}});
      nested.registry.Begin({cookie + 1, api::Kind::kWait, 0, api::TimeoutClass::kZero}); }
    Check(outer.sdk_admission_count() == 2 && !outer.wait_observation(),
          "nested helper's other-cookie admission also invalidates enclosing selected helper eligibility"); }
}
[[maybe_unused]] void DuplicatesUnsupportedAndClocks() {
  Setup duplicate; duplicate.Select(); duplicate.clock.now = 200;
  duplicate.registry.Create({cookie, 0, 32767, api::Origin::kFresh});
  auto poisoned = duplicate.Freeze(true);
  Check(poisoned.frozen && (poisoned.frozen->journal().raw_issues() & uint32_t(core::Issue::kDuplicateCreation)) &&
        duplicate.collector->counters().failures && Has(poisoned.frozen->journal().PrefixJson().json, "\"admission_inventory_complete\":false"),
        "selected-cookie duplicate is not ignored as unrelated creation");
  Setup reuse; reuse.Select(); reuse.clock.now = 200; reuse.registry.Destroy(cookie);
  reuse.clock.now = 250; reuse.registry.Create({cookie, 0, 32767, api::Origin::kFresh});
  auto reused = reuse.Freeze(true);
  Check(reused.frozen && reused.frozen->journal().raw_epochs().size() == 2 && reuse.collector->counters().failures,
        "witnessed selected-cookie reuse retained with fresh raw epoch but selection fails closed");
  Setup unsupported; unsupported.Select(); unsupported.clock.now = 110;
  const auto ticket = unsupported.registry.Begin({cookie, api::Kind::kWaitMultiple, 0, api::TimeoutClass::kNotApplicable});
  unsupported.clock.now = 120; unsupported.registry.End({ticket, api::Kind::kWaitMultiple, api::Result::kUnsupported, {}});
  auto suffix = unsupported.Freeze(true);
  Check(suffix.frozen && suffix.frozen->journal().raw_operations()[0].kind == core::Kind::kWaitMultiple &&
        (suffix.frozen->journal().raw_issues() & uint32_t(core::Issue::kUnsupported)), "unsupported access visible, never omitted/FIFO interpreted");
  for (bool throwing : {false, true}) {
    Setup failed; failed.Select(); failed.clock.now = 0; failed.clock.throw_read = throwing;
    const auto observed = failed.registry.Begin({cookie, api::Kind::kWait, 0, api::TimeoutClass::kZero});
    failed.clock.throw_read = false; failed.clock.now = 120;
    failed.registry.End({observed, api::Kind::kWait, api::Result::kTimeout, {}});
    auto invalid = failed.Freeze(true);
    Check(invalid.frozen && invalid.frozen->journal().raw_operations()[0].entry_qpc == 0 && failed.collector->counters().failures &&
          Has(invalid.frozen->journal().PrefixJson().json, "\"observer_failed\":true"), "zero/throwing clock retains diagnostic row and prevents coverage admission");
  }
  Setup invalid_cut; invalid_cut.Select(); invalid_cut.clock.now = 0;
  auto cut = invalid_cut.collector->Freeze();
  Check(cut.status == col::Status::kInvalidClock && !cut.frozen && !invalid_cut.registry.admitting(), "failed freeze clock disarms, never synthesizes cutoff");
}
[[maybe_unused]] void ExtraAdmissionInvalidatesUpdateRelease() {
  for (unsigned mode = 0; mode < 3; ++mode) {
    Setup setup; setup.Select();
    {
      col::Collector::ScopedOperation outer(*setup.collector,
          {cookie, 77, handle, lane, col::kReleaseLR, api::Kind::kRelease,
           {core::ContextKind::kUpdate, {1, 7}}});
      setup.clock.now = 110;
      const auto release = setup.registry.Begin({cookie, api::Kind::kRelease, 1, api::TimeoutClass::kNotApplicable});
      setup.clock.now = 120;
      setup.registry.End({release, api::Kind::kRelease, api::Result::kSuccess, 0});
      setup.clock.now = 130;
      if (mode == 1) {
        col::Collector::ScopedOperation inner(*setup.collector,
            {cookie + 1, 0, handle + 1, lane, 1, api::Kind::kWait, {}});
        setup.registry.Begin({cookie + 1, api::Kind::kWait, 0, api::TimeoutClass::kZero});
      } else {
        const auto extra = setup.registry.Begin({mode == 2 ? cookie : cookie + 1,
            api::Kind::kWait, 0, api::TimeoutClass::kZero});
        if (extra) { setup.clock.now = 140; setup.registry.End({extra, api::Kind::kWait, api::Result::kSuccess, {}}); }
      }
    }
    auto frozen = setup.Freeze(true);
    Check(frozen.frozen && !frozen.frozen->annotations()[0].annotated &&
          frozen.frozen->journal().raw_operations()[0].context.kind == core::ContextKind::kUpdate,
          "ambiguous release retains original raw annotation without rewriting history");
    Check(frozen.frozen && frozen.frozen->counters().failures &&
          Has(frozen.frozen->journal().PrefixJson().json, "\"observer_failed\":true") &&
          Has(frozen.frozen->journal().PrefixJson().json, "\"admission_inventory_complete\":false"),
          "extra admission after update release prevents any update association");
  }
}
[[maybe_unused]] void WrongThreadCompletion() {
  Setup setup; setup.Select();
  col::Collector::ScopedOperation scope(*setup.collector,
      {cookie, 91, handle, lane, 0x82305EB8, api::Kind::kWait, {}});
  setup.clock.now = 110;
  const auto ticket = setup.registry.Begin({cookie, api::Kind::kWait, 0, api::TimeoutClass::kZero});
  setup.clock.now = 120;
  bool preserved = false;
  std::atomic<bool> done{false};
  auto join = [&](std::thread& thread) {
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
    while (!done.load(std::memory_order_acquire) && std::chrono::steady_clock::now() < deadline)
      std::this_thread::sleep_for(std::chrono::milliseconds(1));
    if (!done.load(std::memory_order_acquire)) std::abort(); // Fixture process only; no unbounded join.
    thread.join();
  };
  std::thread other([&] { SetLastError(sentinel);
    setup.registry.End({ticket, api::Kind::kWait, api::Result::kSuccess, {}});
    preserved = GetLastError() == sentinel;
    done.store(true, std::memory_order_release);
  });
  join(other);
  Check(preserved && setup.collector->counters().failures &&
        setup.collector->raw_summary().pending == 1 && !scope.wait_observation(),
        "wrong-thread End fails closed, preserves pending row and LastError");
  setup.clock.now = 130; setup.registry.End({ticket, api::Kind::kWait, api::Result::kSuccess, {}});
  Check(setup.collector->raw_summary().pending == 0 && setup.collector->counters().failures,
        "correct-thread End can finish but cannot clear prior integrity fault");
  auto frozen = setup.Freeze(true);
  Check(frozen.frozen && Has(frozen.frozen->journal().PrefixJson().json, "\"observer_failed\":true"),
        "wrong-thread fault remains in frozen evidence");
  const auto before = frozen.frozen->journal().RawCoreJson().json;
  done.store(false);
  std::thread late([&] { setup.registry.End({ticket, api::Kind::kWait, api::Result::kSuccess, {}}); done.store(true, std::memory_order_release); });
  join(late);
  Check(before == frozen.frozen->journal().RawCoreJson().json && !setup.collector->raw_summary().post_stop_mutated,
        "late wrong-thread End cannot mutate frozen journal");
}
[[maybe_unused]] void CapacityAndConfiguration() {
  Setup setup({100, 1000000, 1000}); setup.Select();
  for (size_t i = 0; i < core::kOperationCapacity; ++i) {
    setup.clock.now = 200 + i; const auto ticket = setup.registry.Begin({cookie, api::Kind::kWait, 0, api::TimeoutClass::kZero});
    setup.clock.now = 201 + i; setup.registry.End({ticket, api::Kind::kWait, api::Result::kTimeout, {}});
  }
  setup.clock.now = core::kOperationCapacity + 800; Check(!setup.registry.Begin({cookie, api::Kind::kWait, 0, api::TimeoutClass::kZero}), "fixed ticket capacity cannot recycle IDs");
  auto frozen = setup.Freeze(true, core::kOperationCapacity + 1000);
  Check(frozen.frozen && frozen.frozen->journal().raw_counts().observed_operations == core::kOperationCapacity + 1 &&
        frozen.frozen->journal().raw_counts().stored_operations == core::kOperationCapacity && frozen.frozen->annotations().size() == core::kOperationCapacity &&
        Has(frozen.frozen->journal().PrefixJson().json, "\"entry_lower_bound_qpc\":null"), "unlocalized capacity loss is explicit, never prefix success");
  for (unsigned bad = 0; bad < 3; ++bad) {
    api::Registry registry; Clock clock; if (bad == 1) clock.frequency = 999; if (bad == 2) clock.throw_frequency = true;
    auto collector = std::make_unique<col::Collector>(registry, col::ClockProvider{&clock, Clock::Read, Clock::Frequency});
    SetLastError(sentinel);
    Check(collector->Configure(window, bad ? "44444444444444444444444444444444" : "BAD", true) == col::Status::kInvalidConfiguration &&
          !registry.registered() && GetLastError() == sentinel, "strict token/QPF/throwing frequency rejected before registration");
  }
}
} // namespace
int main() {
  DefaultAndOff();
  if constexpr (api::kCompiledEnabled) {
    SelectionAndBindings(); OperationsAndContext(); PendingAndFreeze(); ExtraAdmissionInvalidatesHelper();
    DuplicatesUnsupportedAndClocks(); CapacityAndConfiguration(); ExtraAdmissionInvalidatesUpdateRelease();
    WrongThreadCompletion();
  }
  std::cout << "semaphore collector " << (api::kCompiledEnabled ? "ON" : "OFF") << ": " << checks << " checks, " << failures << " failures\n";
  return failures ? 1 : 0;
}
