// Synthetic supplied metadata only. No runtime callbacks, clocks, SDK or files.
// Optional --emit-fixtures writes JSONL to stdout for primary Python crosscheck.
#include "../src/mcla_semaphore_prefix_snapshot.h"
#include <cstring>
#include <iostream>
#include <type_traits>

namespace {
namespace core = mcla::semaphore_observer;
namespace bridge = mcla::semaphore_prefix_snapshot;
unsigned checks = 0, failures = 0;
bool emit = false;
constexpr std::string_view token = "44444444444444444444444444444444";
constexpr core::Window window{100, 10000, 10000};
constexpr uint64_t cookie = 200, freeze = 1000;
void Check(bool value, const char* why) {
  ++checks; if (!value) { ++failures; std::cerr << "FAIL " << why << '\n'; }
}
bool Has(const std::string& text, std::string_view fragment) { return text.find(fragment) != std::string::npos; }
struct Setup {
  std::unique_ptr<core::Journal> journal = std::make_unique<core::Journal>(window, true);
  std::array<bridge::EpochCoverage, 1> claims{{{1, cookie, 101, bridge::Coverage::kCompleteThroughFreeze,
                                                               bridge::Coverage::kCompleteThroughFreeze}}};
  explicit Setup(uint32_t initial = 0, core::Origin origin = core::Origin::kFresh) { journal->create(cookie, 101, initial, 8, origin); }
  bridge::AdmissionCoverage Coverage(uint64_t cut = freeze) const {
    return {token, window.begin_qpc, window.qpc_frequency, cut, bridge::Coverage::kCompleteThroughFreeze, false, false, claims};
  }
  bridge::CaptureResult Capture(uint64_t cut = freeze) {
    journal->stop_admissions(cut); return bridge::Snapshot::Capture(*journal, token, Coverage(cut));
  }
  void Pair() {
    auto release = journal->begin_release(cookie, 25, 12, 110, 1, {core::ContextKind::kUpdate, {1, 7}});
    journal->end_release(release.ticket, 120, true, 0);
    auto wait = journal->begin_wait(cookie, 25, 12, 130, core::TimeoutKind::kInfinite);
    journal->end_wait(wait.ticket, 140, core::WaitResult::kSuccess);
  }
};
void Emit(std::string_view name, const bridge::Snapshot& snapshot) {
  if (!emit) return;
  const auto prefix = snapshot.PrefixJson(); const auto raw = snapshot.RawCoreJson();
  if (prefix.error != bridge::Error::kNone || raw.error != bridge::Error::kNone) { Check(false, "fixture serializes"); return; }
  std::cout << "{\"name\":\"" << name << "\",\"prefix\":" << prefix.json << ",\"raw\":" << raw.json << "}\n";
}
void CleanPendingAndDetached() {
  static_assert(!std::is_copy_constructible_v<bridge::Snapshot> && !std::is_copy_assignable_v<bridge::Snapshot> &&
                !std::is_move_constructible_v<bridge::Snapshot> && !std::is_move_assignable_v<bridge::Snapshot>);
  Setup setup; setup.Pair();
  auto pending = setup.journal->begin_wait(cookie, 25, 12, 150, core::TimeoutKind::kInfinite);
  auto captured = setup.Capture();
  Check(captured.error == bridge::Error::kNone && captured.snapshot, "heap snapshot accepts exact frozen pending raw inventory");
  if (!captured.snapshot) return;
  const auto& snapshot = *captured.snapshot;
  const auto prefix = snapshot.PrefixJson(); const auto raw = snapshot.RawCoreJson();
  Check(prefix.error == bridge::Error::kNone && raw.error == bridge::Error::kNone &&
        snapshot.pending_operations() == 1 && snapshot.raw_counts().completed_operations == 2,
        "honest pending/completed counts, both serializers bounded");
  Check(Has(prefix.json, "\"op_id\":3") && Has(prefix.json, "\"exit_qpc\":null,\"kind\":\"wait\",\"completed\":false") &&
        Has(prefix.json, "\"result\":null,\"timeout_kind\":\"infinite\""), "pending exit and outcome null, never default-result synthesis");
  Check(!snapshot.raw_history_complete() && snapshot.raw_observer_failed() &&
        Has(raw.json, "\"history_complete\":false,\"observer_failed\":true") &&
        Has(prefix.json, "\"admission_inventory_complete\":true,\"observer_failed\":false"),
        "separate coverage meaning, original failed/history flags unchanged in companion");
  Check(snapshot.raw_issues() == setup.journal->issues() && snapshot.raw_operations().size() == 3 &&
        snapshot.raw_operations()[0].op_id == 1 && snapshot.raw_operations()[2].op_id == 3,
        "all rows and IDs copied in original order");
  setup.claims[0].operations = bridge::Coverage::kUnknown;
  setup.journal->end_wait(pending.ticket, 1001, core::WaitResult::kSuccess);
  Check(setup.journal->drained() && snapshot.pending_operations() == 1 &&
        !snapshot.raw_operations()[2].completed && snapshot.PrefixJson().json == prefix.json &&
        snapshot.RawCoreJson().json == raw.json, "late drain and caller-claim mutation cannot change immutable snapshot");
  Check(bridge::Snapshot::Capture(*setup.journal, token, setup.Coverage()).error == bridge::Error::kNotFreezeSnapshot,
        "post-drain recapture cannot masquerade as original pending freeze");
  Emit("pending", snapshot);
  setup.journal.reset();
  Check(snapshot.PrefixJson().json == prefix.json, "detached snapshot survives source Journal destruction");

  Setup released; released.Pair();
  released.journal->begin_release(cookie, 25, 12, 150, 1, {core::ContextKind::kControl, {}});
  auto pending_release = released.Capture();
  Check(pending_release.snapshot && Has(pending_release.snapshot->PrefixJson().json,
        "\"count\":1,\"success\":null,\"previous_count\":null"), "pending release has no fabricated success or previous count");
  if (pending_release.snapshot) Emit("pending_release", *pending_release.snapshot);
}
void PartialDrainCannotRecaptureFreeze() {
  for (const uint64_t after_cut_exit : {freeze, freeze + 1}) {
    Setup setup; setup.Pair();
    const auto first = setup.journal->begin_wait(cookie, 25, 12, 150, core::TimeoutKind::kInfinite);
    const auto second = setup.journal->begin_wait(cookie, 25, 12, 160, core::TimeoutKind::kInfinite);
    auto initial = setup.Capture();
    Check(initial.error == bridge::Error::kNone && initial.snapshot &&
          initial.snapshot->pending_operations() == 2 && initial.snapshot->raw_counts().completed_operations == 2,
          "two pending admissions captured at the original cut");
    if (!initial.snapshot) continue;
    const auto before_prefix = initial.snapshot->PrefixJson();
    const auto before_raw = initial.snapshot->RawCoreJson();
    setup.journal->end_wait(first.ticket, after_cut_exit, core::WaitResult::kTimeout);
    Check(setup.journal->pending_operations() == 1 && setup.journal->counts().completed_operations == 3 &&
          setup.journal->has(core::Issue::kIncompleteCut) && setup.journal->post_stop_mutated(),
          "partial post-cut completion leaves pending nonzero and original incomplete bit set");
    auto changed = bridge::Snapshot::Capture(*setup.journal, token, setup.Coverage());
    Check(changed.error == bridge::Error::kNotFreezeSnapshot && !changed.snapshot,
          "partial drain cannot be recaptured as frozen state, including equal-QPC completion");
    Check(initial.snapshot->pending_operations() == 2 &&
          !initial.snapshot->raw_post_stop_mutated() &&
          initial.snapshot->PrefixJson().json == before_prefix.json &&
          initial.snapshot->RawCoreJson().json == before_raw.json,
          "initial frozen snapshot stays unchanged through partial drain");
    setup.journal->stop_admissions(freeze + 50);
    Check(setup.journal->cut_qpc() == freeze,
          "repeated stop cannot replace the original real cut after partial drain");
    setup.journal->end_wait(second.ticket, after_cut_exit, core::WaitResult::kTimeout);
    auto drained = bridge::Snapshot::Capture(*setup.journal, token, setup.Coverage());
    Check(setup.journal->drained() && drained.error == bridge::Error::kNotFreezeSnapshot && !drained.snapshot,
          "fully drained state also cannot be recaptured as original freeze");
  }
}
void PostStopFaultsCannotRecaptureFreeze() {
  for (unsigned mutation = 0; mutation < 4; ++mutation) {
    Setup setup; setup.Pair();
    const auto completed = setup.journal->begin_wait(cookie, 25, 12, 150, core::TimeoutKind::kZero);
    setup.journal->end_wait(completed.ticket, 160, core::WaitResult::kTimeout);
    if (mutation == 2) setup.journal->fail(); // Repeated fail leaves raw issue bits unchanged.
    auto initial = setup.Capture();
    Check(initial.error == bridge::Error::kNone && initial.snapshot,
          "original stopped snapshot accepts retained pre-cut fault state");
    if (!initial.snapshot) continue;
    const auto initial_raw = initial.snapshot->RawCoreJson();
    const auto initial_completed = setup.journal->counts().completed_operations;
    if (mutation == 0) {
      Check(setup.journal->end_wait(completed.ticket, freeze, core::WaitResult::kTimeout) == core::Disposition::kAlreadyCompleted,
            "post-stop duplicate completion is rejected without changing completed count");
    } else if (mutation == 1 || mutation == 2) {
      setup.journal->fail();
    } else {
      setup.journal->begin_wait(cookie, 25, 12, window.end_qpc_exclusive + 1, core::TimeoutKind::kZero);
    }
    auto changed = bridge::Snapshot::Capture(*setup.journal, token, setup.Coverage());
    Check(setup.journal->counts().completed_operations == initial_completed &&
          setup.journal->post_stop_mutated() &&
          changed.error == bridge::Error::kNotFreezeSnapshot && !changed.snapshot,
          "post-stop duplicate/fail/repeated-fail/stopped-admission forbids changed-cut recapture");
    Check(initial.snapshot->RawCoreJson().json == initial_raw.json,
          "existing immutable snapshot unaffected by later diagnostic mutation attempts");
  }
  Setup unchanged; unchanged.Pair(); auto initial = unchanged.Capture();
  unchanged.journal->stop_admissions(freeze + 100);
  (void)unchanged.journal->counts(); (void)unchanged.journal->issues();
  auto repeated = bridge::Snapshot::Capture(*unchanged.journal, token, unchanged.Coverage());
  Check(initial.snapshot && repeated.snapshot && unchanged.journal->cut_qpc() == freeze &&
        !unchanged.journal->post_stop_mutated() &&
        initial.snapshot->RawCoreJson().json == repeated.snapshot->RawCoreJson().json,
        "repeat stop no-op and read-only queries do not invalidate same original cut");
  for (unsigned path = 0; path < 9; ++path) {
    Setup setup; setup.Pair(); auto original = setup.Capture();
    switch (path) {
      case 0: setup.journal->create(cookie + 1, freeze, 0, 8); break;
      case 1: setup.journal->destroy(cookie, freeze); break;
      case 2: setup.journal->begin_release(cookie, 25, {}, freeze, 1); break;
      case 3: setup.journal->begin_wait(cookie, 25, {}, freeze, core::TimeoutKind::kZero); break;
      case 4: setup.journal->begin_unsupported(cookie, 25, {}, freeze, static_cast<core::Kind>(99)); break;
      case 5: setup.journal->end_release({}, freeze, false); break;
      case 6: setup.journal->end_wait({}, freeze, core::WaitResult::kTimeout); break;
      case 7: setup.journal->end_unsupported({}, freeze); break;
      case 8: setup.journal->fail(); break;
    }
    auto changed = bridge::Snapshot::Capture(*setup.journal, token, setup.Coverage());
    Check(original.snapshot && !original.snapshot->raw_post_stop_mutated() && setup.journal->post_stop_mutated() &&
          changed.error == bridge::Error::kNotFreezeSnapshot && !changed.snapshot,
          "every mutating public path after stop poisons recapture, even invalid-kind/ticket early returns");
  }
  auto disabled = std::make_unique<core::Journal>(window); disabled->stop_admissions(freeze); disabled->fail();
  disabled->begin_unsupported(cookie, 0, {}, 0, static_cast<core::Kind>(99));
  Check(!disabled->post_stop_mutated() && !disabled->issues(), "default-off remains no-op for freeze mutation diagnostics");
}
void CoverageAndUnsupported() {
  Setup unknown; unknown.Pair(); unknown.journal->stop_admissions(freeze);
  auto coverage = unknown.Coverage(); coverage.inventory = bridge::Coverage::kUnknown;
  auto captured = bridge::Snapshot::Capture(*unknown.journal, token, coverage);
  Check(captured.snapshot && Has(captured.snapshot->PrefixJson().json, "\"admission_inventory_complete\":false") &&
        captured.snapshot->raw_issues() == uint32_t(core::Issue::kEarlyCut), "clean structural core is NOT independent coverage proof");
  if (captured.snapshot) Emit("coverage_unknown", *captured.snapshot);
  Setup unsupported; unsupported.Pair();
  auto saved = unsupported.journal->begin_unsupported(cookie, 25, {}, 160, core::Kind::kSave);
  unsupported.journal->end_unsupported(saved.ticket, 180);
  auto raw = unsupported.Capture();
  Check(raw.snapshot && !raw.snapshot->raw_epochs()[0].history_complete &&
        (raw.snapshot->raw_issues() & uint32_t(core::Issue::kUnsupported)) &&
        Has(raw.snapshot->PrefixJson().json, "\"kind\":\"save\",\"completed\":true,\"payload\":{}") &&
        Has(raw.snapshot->PrefixJson().json, "\"origin\":\"fresh\",\"history_complete\":true"),
        "unsupported suffix retained; independent epoch admission claim does not edit raw false flag");
  if (raw.snapshot) Emit("unsupported", *raw.snapshot);
  Setup missing; missing.Pair(); missing.claims[0].creation = bridge::Coverage::kUnknown;
  auto missing_capture = missing.Capture();
  Check(missing_capture.snapshot && Has(missing_capture.snapshot->PrefixJson().json,
        "\"origin\":\"fresh\",\"history_complete\":false"), "unknown per-epoch creation coverage remains unknown");
  if (missing_capture.snapshot) Emit("epoch_unknown", *missing_capture.snapshot);
}
void Bindings() {
  Setup setup; setup.Pair(); auto coverage = setup.Coverage();
  Check(bridge::Snapshot::Capture(*setup.journal, token, coverage).error == bridge::Error::kScopeBinding,
        "capture requires already stopped Journal, performs no runtime stop");
  setup.journal->stop_admissions(freeze);
  for (const auto bad_token : {std::string_view("short"), std::string_view("AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA")})
    Check(bridge::Snapshot::Capture(*setup.journal, bad_token, coverage).error == bridge::Error::kToken, "strict lower-hex token32");
  coverage.token = "55555555555555555555555555555555";
  Check(bridge::Snapshot::Capture(*setup.journal, token, coverage).error == bridge::Error::kToken, "coverage exact session binding");
  coverage = setup.Coverage(); ++coverage.qpc_frequency;
  Check(bridge::Snapshot::Capture(*setup.journal, token, coverage).error == bridge::Error::kScopeBinding, "QPF mismatch rejected");
  coverage = setup.Coverage(); ++coverage.observer_begin_qpc;
  Check(bridge::Snapshot::Capture(*setup.journal, token, coverage).error == bridge::Error::kScopeBinding, "observer begin mismatch rejected");
  coverage = setup.Coverage(); ++coverage.freeze_qpc;
  Check(bridge::Snapshot::Capture(*setup.journal, token, coverage).error == bridge::Error::kScopeBinding, "actual stop cut mismatch rejected");
  coverage = setup.Coverage(); coverage.epochs = {};
  Check(bridge::Snapshot::Capture(*setup.journal, token, coverage).error == bridge::Error::kCoverageBinding, "every raw epoch requires exact coverage binding");
  for (unsigned field = 0; field < 3; ++field) {
    auto wrong = setup.claims;
    if (field == 0) ++wrong[0].epoch_id;
    if (field == 1) ++wrong[0].cookie;
    if (field == 2) ++wrong[0].creation_qpc;
    coverage = setup.Coverage(); coverage.epochs = wrong;
    Check(bridge::Snapshot::Capture(*setup.journal, token, coverage).error == bridge::Error::kCoverageBinding,
          "epoch ID/cookie/creation QPC coverage mismatch rejected");
  }
  coverage = setup.Coverage(); coverage.inventory = static_cast<bridge::Coverage>(99);
  Check(bridge::Snapshot::Capture(*setup.journal, token, coverage).error == bridge::Error::kCoverageBinding, "unknown typed coverage enum rejected");
  Setup horizon; horizon.journal->stop_admissions(window.end_qpc_exclusive);
  Check(bridge::Snapshot::Capture(*horizon.journal, token, horizon.Coverage(window.end_qpc_exclusive)).error == bridge::Error::kScopeBinding,
        "real freeze must be strictly below configured maximum horizon");
}
void FailuresAndRepresentation() {
  Setup gap; gap.journal->begin_wait(999, 25, {}, 105, core::TimeoutKind::kZero); gap.Pair();
  auto unknown = gap.Capture();
  Check(unknown.snapshot && Has(unknown.snapshot->PrefixJson().json, "\"admission_inventory_complete\":false,\"observer_failed\":true,\"unknown_coverage_gap\":true") &&
        Has(unknown.snapshot->RawCoreJson().json, "\"unknown_object_attempts\":1"), "unlocalized unknown object disqualifies all prefixes without fabricated IDs");
  if (unknown.snapshot) Emit("unknown_object", *unknown.snapshot);
  Setup failed; failed.Pair(); failed.journal->fail(); auto fault = failed.Capture();
  Check(fault.snapshot && Has(fault.snapshot->PrefixJson().json, "\"observer_failed\":true"), "external failure never cleared into healthy coverage");
  Setup malformed;
  const auto bad = malformed.journal->begin_release(cookie, 0, 0, 110, 0, {core::ContextKind::kControl, {1, 2}});
  malformed.journal->end_release(bad.ticket, 120, true, {});
  auto bad_snapshot = malformed.Capture();
  Check(bad_snapshot.snapshot && bad_snapshot.snapshot->PrefixJson().error == bridge::Error::kUnrepresentable &&
        bad_snapshot.snapshot->PrefixJson().json.empty() && bad_snapshot.snapshot->RawCoreJson().error == bridge::Error::kNone &&
        Has(bad_snapshot.snapshot->RawCoreJson().json, "\"release_count\":0"), "malformed raw fields preserved in companion, v2 serialization refused");
  Setup impossible;
  const auto excessive = impossible.journal->begin_release(cookie, 25, {}, 110, 2);
  impossible.journal->end_release(excessive.ticket, 120, true, 7);
  auto excessive_snapshot = impossible.Capture();
  Check(excessive_snapshot.snapshot && excessive_snapshot.snapshot->PrefixJson().error == bridge::Error::kUnrepresentable,
        "successful previous plus release count cannot exceed declared maximum");
  Setup clock;
  const auto waited = clock.journal->begin_wait(cookie, 25, {}, 110, core::TimeoutKind::kZero);
  clock.journal->end_wait(waited.ticket, 0, core::WaitResult::kTimeout);
  auto clock_snapshot = clock.Capture();
  Check(clock_snapshot.snapshot && clock_snapshot.snapshot->PrefixJson().error == bridge::Error::kNone &&
        Has(clock_snapshot.snapshot->PrefixJson().json, "\"exit_qpc\":null") &&
        Has(clock_snapshot.snapshot->RawCoreJson().json, "\"exit_qpc\":0") &&
        Has(clock_snapshot.snapshot->PrefixJson().json, "\"observer_failed\":true"), "failed clock is explicit unknown in v2, raw zero remains retained and globally ineligible");
  if (clock_snapshot.snapshot) Emit("missing_clock", *clock_snapshot.snapshot);
  for (const uint64_t invalid : {uint64_t{109}, core::kMaxQpc + 1, freeze + 1}) {
    Setup malformed_clock;
    auto pending = malformed_clock.journal->begin_wait(cookie, 25, {}, 110, core::TimeoutKind::kZero);
    malformed_clock.journal->end_wait(pending.ticket, invalid, core::WaitResult::kTimeout);
    auto snapshot = malformed_clock.Capture();
    Check(snapshot.snapshot && snapshot.snapshot->PrefixJson().error == bridge::Error::kUnrepresentable &&
          snapshot.snapshot->RawCoreJson().error == bridge::Error::kNone,
          "regressing/high/after-cut clocks are not repaired to satisfy PREFIX-v2");
  }
  Setup nonfresh(0, core::Origin::kNative); auto native = nonfresh.Capture();
  Check(native.snapshot && Has(native.snapshot->PrefixJson().json, "\"origin\":\"native\""), "nonfresh origin preserved for v2 inconclusive outcome");
  if (native.snapshot) Emit("nonfresh", *native.snapshot);
  Setup initial(2); auto credits = initial.Capture();
  Check(credits.snapshot && Has(credits.snapshot->PrefixJson().json, "\"initial_count\":2"), "initial credits never converted to zero history");
  if (credits.snapshot) Emit("initial_credits", *credits.snapshot);
}
void OverflowAndBounds() {
  Setup setup;
  setup.journal = std::make_unique<core::Journal>(core::Window{100, 1000000, 10000}, true);
  setup.journal->create(cookie, 101, 0, 8);
  for (size_t i = 0; i < core::kOperationCapacity; ++i) {
    const auto wait = setup.journal->begin_wait(cookie, 25, {}, 200 + i, core::TimeoutKind::kZero);
    setup.journal->end_wait(wait.ticket, 201 + i, core::WaitResult::kTimeout);
  }
  setup.journal->begin_release(cookie, 25, {}, core::kOperationCapacity + 800, 1);
  auto captured = setup.Capture(core::kOperationCapacity + 1000);
  Check(captured.snapshot && captured.snapshot->raw_counts().observed_operations == core::kOperationCapacity + 1 &&
        captured.snapshot->raw_operations().size() == core::kOperationCapacity, "capacity overflow preserves all retained rows plus actual lost count");
  if (!captured.snapshot) return;
  const auto prefix = captured.snapshot->PrefixJson(); const auto raw = captured.snapshot->RawCoreJson();
  Check(prefix.error == bridge::Error::kNone && raw.error == bridge::Error::kNone &&
        prefix.json.size() <= bridge::kMaxJsonBytes && raw.json.size() <= bridge::kMaxJsonBytes &&
        Has(prefix.json, "\"lost_operations\":1") &&
        Has(prefix.json, "\"first_op_id\":" + std::to_string(core::kOperationCapacity + 1) +
            ",\"last_op_id\":" + std::to_string(core::kOperationCapacity + 1) + ",\"entry_lower_bound_qpc\":null"),
        "unlocalized overflow loss has null bound, not inferred from last retained row");
  Emit("overflow", *captured.snapshot);
  bridge::detail::Writer writer;
  writer.Raw(std::string(bridge::kMaxJsonBytes, 'x')); writer.Raw("y"); writer.Raw("z");
  Check(writer.too_large && writer.text.size() == bridge::kMaxJsonBytes,
        "writer never grows beyond hard bound after oversize append");
  for (const auto flag : {"native_admitted", "simulation_proven", "render_proven", "fps_proven",
                         "frame_to_step_mapping_proven", "object_identity_proven", "object_lifetime_proven",
                         "prefix_history_proven", "update_causality_proven"})
    Check(Has(prefix.json, std::string("\"") + flag + "\":false"), "every v2 proof flag remains false");
}
} // namespace
int main(int argc, char** argv) {
  if (argc == 2 && std::strcmp(argv[1], "--emit-fixtures") == 0) emit = true;
  else if (argc != 1) return 2;
  CleanPendingAndDetached(); PartialDrainCannotRecaptureFreeze(); PostStopFaultsCannotRecaptureFreeze();
  CoverageAndUnsupported(); Bindings(); FailuresAndRepresentation(); OverflowAndBounds();
  auto& report = emit ? std::cerr : std::cout;
  report << "semaphore prefix snapshot: " << checks << " checks, " << failures << " failures\n";
  return failures ? 1 : 0;
}
