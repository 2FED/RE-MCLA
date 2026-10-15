// Synthetic supplied observations only. No SDK, OS, clocks, game, files or
// threads. Heap journals avoid large Windows-stack temporaries. Primary owns
// compilation/execution and any runtime integration.
#include "../src/mcla_semaphore_observer_core.h"
#include <iostream>
#include <memory>
#include <type_traits>

namespace {
namespace sem = mcla::semaphore_observer;
unsigned checks = 0, failures = 0;
void Check(bool value, const char* why) {
  ++checks;
  if (!value) { ++failures; std::cerr << "FAIL " << why << '\n'; }
}
constexpr sem::Window window{100, 10000, 1000};
constexpr uint64_t cookie = 0x1000;
std::unique_ptr<sem::Journal> New(bool enabled = true, sem::Window supplied = window) {
  return std::make_unique<sem::Journal>(supplied, enabled);
}
void Create(sem::Journal& journal, uint64_t object = cookie, uint64_t qpc = 101) {
  Check(journal.create(object, qpc, 0, 32767).disposition == sem::Disposition::kRecorded,
        "supplied fresh lifetime recorded");
}
sem::ReleaseContext Update(uint64_t thread = 1, uint64_t serial = 7) {
  return {sem::ContextKind::kUpdate, {thread, serial}};
}

void DisabledAndConfiguration() {
  const auto before_disabled = sem::detail::last_journal_generation.load(std::memory_order_relaxed);
  auto disabled = std::make_unique<sem::Journal>(window);
  Check(disabled->create(cookie, 101, 0, 8).disposition == sem::Disposition::kDisabled, "default off creation noop");
  const auto begin = disabled->begin_wait(cookie, 1, std::nullopt, 110, sem::TimeoutKind::kZero);
  Check(begin.disposition == sem::Disposition::kDisabled && !begin.ticket.valid(), "disabled operation has no ticket");
  Check(disabled->end_wait(begin.ticket, 111, sem::WaitResult::kSuccess) == sem::Disposition::kDisabled,
        "disabled completion noop");
  Check(disabled->begin_unsupported(cookie, 1, {}, 112, sem::Kind::kSave).disposition == sem::Disposition::kDisabled,
        "disabled unsupported observation noop");
  disabled->fail(); disabled->stop_admissions(window.end_qpc_exclusive);
  Check(disabled->issues() == 0 && disabled->epochs().empty() && disabled->operations().empty() &&
        !disabled->history_complete(), "disabled journal cannot claim history or mutate counts");
  Check(sem::detail::last_journal_generation.load(std::memory_order_relaxed) == before_disabled,
        "default-off construction and operations do not consume journal identities");
  for (const auto bad : {sem::Window{0, 2, 1}, sem::Window{3, 3, 1}, sem::Window{3, 2, 1},
                        sem::Window{1, sem::kMaxQpc + 1, 1}, sem::Window{1, 2, 0}}) {
    auto invalid = New(true, bad);
    Check(invalid->has(sem::Issue::kInvalidConfiguration) &&
          invalid->create(cookie, 101, 0, 8).disposition == sem::Disposition::kInvalidArgument,
          "invalid supplied lifetime window does not admit objects");
  }
  Check(!sem::kNativeAdmitted && !sem::kSimulationProven && !sem::kRenderProven && !sem::kFpsProven &&
        !sem::kFrameToStepMappingProven && !sem::kObjectIdentityProven && !sem::kObjectLifetimeProven &&
        !sem::kOnlineAssociationProven, "every proof/admission constant false");
}

void JournalIdentityAndSaturation() {
  static_assert(!std::is_copy_constructible_v<sem::Journal> && !std::is_copy_assignable_v<sem::Journal> &&
                !std::is_move_constructible_v<sem::Journal> && !std::is_move_assignable_v<sem::Journal>);
  std::atomic<uint64_t> local{0};
  Check(sem::detail::AllocateJournalGeneration(local) == 1 &&
        sem::detail::AllocateJournalGeneration(local) == 2, "local allocator issues distinct positive IDs");
  std::atomic<uint64_t> saturated{UINT64_MAX - 1};
  Check(sem::detail::AllocateJournalGeneration(saturated) == UINT64_MAX,
        "maximum identity issued exactly once without wrapping");
  Check(sem::detail::AllocateJournalGeneration(saturated) == 0 &&
        sem::detail::AllocateJournalGeneration(saturated) == 0 && saturated.load() == UINT64_MAX,
        "allocator exhaustion stays saturated and never returns a reused positive ID");
  struct Storage { alignas(sem::Journal) std::array<std::byte, sizeof(sem::Journal)> bytes; };
  auto storage = std::make_unique<Storage>();
  auto* first = std::construct_at(reinterpret_cast<sem::Journal*>(storage->bytes.data()), window, true);
  Create(*first);
  const auto stale = first->begin_wait(cookie, 1, {}, 110, sem::TimeoutKind::kZero).ticket;
  first->end_wait(stale, 111, sem::WaitResult::kTimeout);
  std::destroy_at(first);
  auto* replacement = std::construct_at(reinterpret_cast<sem::Journal*>(storage->bytes.data()), window, true);
  Create(*replacement);
  const auto current = replacement->begin_wait(cookie, 1, {}, 110, sem::TimeoutKind::kZero).ticket;
  Check(stale.op_id() == current.op_id() && stale.epoch_id() == current.epoch_id() &&
        stale.journal_generation() != current.journal_generation(),
        "same-storage Journal reconstruction has different generation despite matching operation/epoch IDs");
  Check(replacement->end_wait(stale, 112, sem::WaitResult::kSuccess) == sem::Disposition::kInvalidTicket &&
        replacement->pending_operations() == 1 && replacement->counts().completed_operations == 0 &&
        !replacement->operations()[0].completed,
        "stale generation cannot complete a reconstructed same-address journal");
  Check(replacement->end_wait(current, 113, sem::WaitResult::kTimeout) == sem::Disposition::kRecorded &&
        replacement->drained() && replacement->counts().completed_operations == 1,
        "correct generation still completes after rejected stale ticket");
  std::destroy_at(replacement);
}

void LifecycleAndAliases() {
  auto journal = New(); Create(*journal);
  auto release = journal->begin_release(cookie, 1, 100, 110, 1, Update());
  journal->end_release(release.ticket, 111, true, 0);
  auto wait = journal->begin_wait(cookie, 2, 200, 120, sem::TimeoutKind::kInfinite);
  journal->end_wait(wait.ticket, 130, sem::WaitResult::kSuccess);
  Check(release.ticket.epoch_id() == wait.ticket.epoch_id() && journal->epochs().size() == 1 &&
        journal->operations()[0].guest_handle == 100 && journal->operations()[1].guest_handle == 200,
        "different guest handles alias one resolved object lifetime");
  Check(journal->destroy(cookie, 140) == sem::Disposition::kRecorded && journal->lookup(cookie) == 0,
        "witnessed destruction closes lookup");
  const auto reused = journal->create(cookie, 150, 0, 32767);
  Check(reused.epoch_id == 2 && journal->lookup(cookie) == 2 && journal->epochs()[0].destroy_qpc == 140,
        "pointer reuse after witnessed strict boundary gets new epoch, old row retained");
  const auto counts = journal->counts();
  Check(counts.observed_operations == 2 && counts.stored_operations == 2 &&
        counts.completed_operations == 2 && counts.overflow_operations == 0, "raw operation count conservation");
  journal->stop_admissions(window.end_qpc_exclusive);
  Check(journal->history_complete() && journal->audit_v1_shape_compatible(),
        "clean complete supplied journal at fixed end, still no native proof");
  auto duplicate = New(); Create(*duplicate);
  Check(duplicate->create(cookie, 110, 0, 8).epoch_id == 0 &&
        duplicate->epochs().size() == 1 && duplicate->has(sem::Issue::kDuplicateCreation),
        "alias/duplicate initialize never invents a second lifetime");
  duplicate->destroy(cookie, 120);
  Check(duplicate->create(cookie, 120, 0, 8).epoch_id == 0 && duplicate->has(sem::Issue::kAmbiguousReuse),
        "touching lifetime timestamps not fitted into unique reuse");
}

void UnknownAndEpochOrigins() {
  auto unknown = New();
  const auto operation = unknown->begin_release(cookie, 1, 7, 110, 1, Update());
  Check(operation.disposition == sem::Disposition::kUnknownObject && !operation.ticket.valid() &&
        unknown->epochs().empty() && unknown->operations().empty(), "unknown object gets no fabricated epoch or history");
  Check(unknown->counts().observed_operations == 0 && unknown->diagnostics().unknown_object_attempts == 1,
        "unknown attempts separate from v1 known-epoch counters");
  Create(*unknown, cookie, 120); unknown->stop_admissions(window.end_qpc_exclusive);
  Check(!unknown->history_complete() && unknown->has(sem::Issue::kUnknownObject),
        "later creation cannot repair missed unknown operations");
  for (const auto origin : {sem::Origin::kNative, sem::Origin::kRestored, sem::Origin::kUnknown}) {
    auto journal = New(); journal->create(cookie, 101, 0, 8, origin);
    journal->stop_admissions(window.end_qpc_exclusive);
    Check(!journal->history_complete() && journal->epochs()[0].origin == origin &&
          !journal->epochs()[0].history_complete && journal->has(sem::Issue::kNonfreshOrigin),
          "nonfresh origins retained and inconclusive");
  }
  auto initial = New(); initial->create(cookie, 101, 2, 8); initial->stop_admissions(window.end_qpc_exclusive);
  Check(initial->epochs()[0].initial_count == 2 && initial->has(sem::Issue::kInitialCredits) &&
        !initial->history_complete(), "initial credits not assigned update identities");
}

void ReleaseAndWaitResults() {
  auto journal = New(); Create(*journal);
  const auto update = journal->begin_release(cookie, 1, {}, 110, 1, Update(11, 7));
  Check(journal->end_release(update.ticket, 111, true, 0) == sem::Disposition::kRecorded,
        "successful release records native result and previous count");
  const auto control = journal->begin_release(cookie, 1, {}, 120, 1, {sem::ContextKind::kControl, {}});
  journal->end_release(control.ticket, 121, false, 765);
  Check(journal->operations()[0].context.generation == mcla::frame_context::Id{11, 7} &&
        journal->operations()[1].context.kind == sem::ContextKind::kControl &&
        journal->operations()[1].context.generation == mcla::frame_context::Id{} &&
        !journal->operations()[1].previous_count, "control is not latest update; failed release clears undefined previous count");
  uint64_t qpc = 130;
  for (const auto result : {sem::WaitResult::kSuccess, sem::WaitResult::kTimeout, sem::WaitResult::kApc,
                           sem::WaitResult::kAlerted}) {
    const auto waited = journal->begin_wait(cookie, 2, {}, qpc, sem::TimeoutKind::kZero);
    journal->end_wait(waited.ticket, qpc + 1, result); qpc += 10;
    Check(journal->operations().back().wait_result == result &&
          journal->operations().back().timeout_kind == sem::TimeoutKind::kZero,
          "zero polls preserve native success/timeout/APC/alert, no online accounting");
  }
  Check(journal->issues() == 0 && journal->counts().completed_operations == 6,
        "ordinary failed release and non-consuming wait outcomes are not observer failures");
  for (const auto result : {sem::WaitResult::kAbandoned, sem::WaitResult::kFailed, sem::WaitResult::kUnknown}) {
    auto unsupported = New(); Create(*unsupported);
    auto waited = unsupported->begin_wait(cookie, 1, {}, 110, sem::TimeoutKind::kFinite);
    unsupported->end_wait(waited.ticket, 111, result);
    Check(unsupported->has(sem::Issue::kUnsupported) && unsupported->operations()[0].wait_result == result,
          "unsupported native wait results retained fail-closed");
  }
}

void OverlapAndExactlyOnceCompletion() {
  auto journal = New(); Create(*journal);
  auto released = journal->begin_release(cookie, 1, 7, 110, 1, Update());
  unsigned original_calls = 0;
  // External caller does original work here, with no journal lock spanning it.
  // Another operation can begin/complete while this release remains pending.
  const auto original = [&] {
    ++original_calls;
    auto waited = journal->begin_wait(cookie, 2, 8, 111, sem::TimeoutKind::kInfinite);
    journal->end_wait(waited.ticket, 130, sem::WaitResult::kSuccess);
    return 42;
  };
  const auto result = original();
  journal->end_release(released.ticket, 120, true, 0);
  Check(original_calls == 1 && result == 42 && journal->issues() == 0 &&
        journal->operations()[0].exit_qpc == 120 && journal->operations()[1].exit_qpc == 130,
        "overlap/completion order retained without serialization or FIFO fitting");
  Check(journal->end_release(released.ticket, 140, true, 0) == sem::Disposition::kAlreadyCompleted &&
        journal->counts().completed_operations == 2 && journal->has(sem::Issue::kCompletionMismatch),
        "duplicate end cannot count or bind again");
  auto foreign = New(); Create(*foreign);
  auto own = foreign->begin_release(cookie, 1, {}, 110, 1);
  Check(foreign->end_release(released.ticket, 120, true, 0) == sem::Disposition::kInvalidTicket &&
        foreign->pending_operations() == 1, "ticket from other journal rejected despite same numeric IDs");
  Check(foreign->end_wait(own.ticket, 120, sem::WaitResult::kSuccess) == sem::Disposition::kInvalidTicket,
        "wrong completion kind cannot consume ticket");
  foreign->end_release(own.ticket, 121, true, 0);
  Check(foreign->drained() && !foreign->history_complete(), "rejected completion never repaired into acceptance");
}

void ClockAndArguments() {
  for (const uint64_t bad : {uint64_t{0}, sem::kMaxQpc + 1, uint64_t{99}, uint64_t{10000}}) {
    auto journal = New(); Create(*journal);
    auto waited = journal->begin_wait(cookie, 1, {}, 110, sem::TimeoutKind::kZero);
    journal->end_wait(waited.ticket, bad, sem::WaitResult::kTimeout);
    Check(journal->operations()[0].exit_qpc == bad && journal->drained() && journal->observer_failed(),
          "raw zero/high/regression/outside exits retained, not interpolated or clamped");
    if (bad > sem::kMaxQpc) Check(!journal->audit_v1_shape_compatible(), "out-of-schema raw QPC cannot become v1 JSON");
  }
  auto zero_creation = New(); zero_creation->create(cookie, 0, 0, 8);
  Check(zero_creation->epochs()[0].creation_qpc == 0 && zero_creation->has(sem::Issue::kInvalidClock),
        "failed creation clock explicit, not fresh timestamp synthesis");
  auto malformed = New(); Create(*malformed);
  auto released = malformed->begin_release(cookie, 0, 0, 110, 0, {sem::ContextKind::kControl, {1, 2}});
  malformed->end_release(released.ticket, 111, true, std::nullopt);
  Check(malformed->has(sem::Issue::kInvalidArgument) && !malformed->audit_v1_shape_compatible() &&
        malformed->operations()[0].release_count == 0, "malformed raw fields retained but serializer must refuse v1 shape");
  auto touch = New(); Create(*touch);
  auto waited = touch->begin_wait(cookie, 1, {}, 101, sem::TimeoutKind::kZero);
  touch->end_wait(waited.ticket, 101, sem::WaitResult::kTimeout);
  Check(touch->has(sem::Issue::kLifetimeBoundary), "operation touching creation not fitted past lifetime boundary");
}

void UnsupportedAndDestruction() {
  for (const auto kind : {sem::Kind::kWaitMultiple, sem::Kind::kSignalAndWait, sem::Kind::kSave,
                          sem::Kind::kRestore, sem::Kind::kLifecycleGap, sem::Kind::kUnknown}) {
    auto journal = New(); Create(*journal);
    auto operation = journal->begin_unsupported(cookie, 1, {}, 110, kind);
    Check(operation.ticket.valid() && journal->has(sem::Issue::kUnsupported), "unsupported operation visible at begin");
    journal->end_unsupported(operation.ticket, 111);
    journal->stop_admissions(window.end_qpc_exclusive);
    Check(journal->operations()[0].kind == kind && !journal->epochs()[0].history_complete &&
          !journal->history_complete(), "unsupported operation never ignored or prefix-salvaged");
  }
  auto journal = New(); Create(*journal);
  auto waited = journal->begin_wait(cookie, 1, {}, 110, sem::TimeoutKind::kInfinite);
  journal->destroy(cookie, 120);
  journal->create(cookie, 130, 0, 8);
  journal->end_wait(waited.ticket, 140, sem::WaitResult::kSuccess);
  Check(journal->epochs().size() == 2 && journal->operations()[0].epoch_id == 1 &&
        journal->epochs()[1].pending_operations == 0 && journal->has(sem::Issue::kLifetimeBoundary),
        "pending destroyed lifetime never completes against reused cookie epoch");
  for (const uint64_t destruction : {uint64_t{119}, uint64_t{120}}) {
    auto regressing = New(); Create(*regressing);
    const auto completed = regressing->begin_wait(cookie, 1, {}, 110, sem::TimeoutKind::kZero);
    regressing->end_wait(completed.ticket, 120, sem::WaitResult::kTimeout);
    regressing->destroy(cookie, destruction);
    regressing->stop_admissions(window.end_qpc_exclusive);
    Check(regressing->epochs()[0].destroy_qpc == destruction &&
          regressing->has(sem::Issue::kLifetimeBoundary) && !regressing->history_complete(),
          "destruction cannot regress into or touch an already completed outer witness");
  }
}

void CapacitiesAndCuts() {
  auto epochs = New();
  for (size_t i = 0; i < sem::kEpochCapacity; ++i)
    Check(epochs->create(cookie + i, 101 + i, 0, 8).epoch_id == i + 1, "bounded64 lifetime IDs");
  Check(epochs->create(0x9000, 200, 0, 8).disposition == sem::Disposition::kCapacityExhausted &&
        epochs->epochs().size() == 64 && epochs->has(sem::Issue::kEpochOverflow), "epoch capacity cannot recycle/drop into a clean prefix");
  auto operations = New(true, {100, 1000000, 1000}); Create(*operations);
  for (size_t i = 0; i < sem::kOperationCapacity; ++i) {
    auto waited = operations->begin_wait(cookie, 1, {}, 200 + i, sem::TimeoutKind::kZero);
    operations->end_wait(waited.ticket, 201 + i, sem::WaitResult::kTimeout);
  }
  auto overflow = operations->begin_release(cookie, 1, {}, sem::kOperationCapacity + 1000, 1, Update());
  const auto counts = operations->counts();
  Check(overflow.disposition == sem::Disposition::kCapacityExhausted && !overflow.ticket.valid() &&
        counts.observed_operations == sem::kOperationCapacity + 1 && counts.stored_operations == sem::kOperationCapacity &&
        counts.completed_operations == sem::kOperationCapacity && counts.overflow_operations == 1,
        "overflow counts known attempt but never fabricates its completion");
  operations->stop_admissions(1000000);
  Check(!operations->history_complete() && operations->has(sem::Issue::kOperationOverflow), "all prefix associations forbidden after overflow");
  auto pending = New(); Create(*pending);
  auto waited = pending->begin_wait(cookie, 1, {}, 110, sem::TimeoutKind::kInfinite);
  Check(!pending->audit_v1_shape_compatible() && pending->counts().completed_operations == 0,
        "pending raw counts cannot satisfy existing audit v1, never inflated");
  pending->stop_admissions(window.end_qpc_exclusive);
  pending->end_wait(waited.ticket, window.end_qpc_exclusive + 1, sem::WaitResult::kSuccess);
  Check(pending->drained() && pending->has(sem::Issue::kIncompleteCut) && !pending->history_complete() &&
        pending->operations()[0].exit_qpc == window.end_qpc_exclusive + 1,
        "post-cut drain preserves straddle but cannot repair missing release history");
  auto early = New(); Create(*early); early->stop_admissions(500);
  Check(early->has(sem::Issue::kEarlyCut) && !early->history_complete(), "early cut not full fixed declared window");
  auto clean = New(); Create(*clean); clean->stop_admissions(window.end_qpc_exclusive);
  Check(clean->history_complete(), "clean supplied fixed-window cut permitted");
  Check(clean->begin_wait(cookie, 1, {}, 120, sem::TimeoutKind::kZero).disposition == sem::Disposition::kStopped &&
        !clean->history_complete() && clean->operations().empty(), "observed missed in-window operation invalidates stopped clean prefix");
  auto failed = New(); Create(*failed); failed->fail(); failed->stop_admissions(window.end_qpc_exclusive);
  Check(failed->has(sem::Issue::kExternalFailure) && !failed->history_complete(), "caller-supplied observer failure sticky");
}

void MalformedEnumsAndBounds() {
  auto origin = New();
  Check(origin->create(cookie, 101, 0, 8, static_cast<sem::Origin>(99)).disposition ==
        sem::Disposition::kInvalidArgument && origin->epochs().empty(), "invalid origin not invented");
  for (const auto count : {uint32_t{0}, sem::kMaxCount + 1}) {
    auto journal = New(); Create(*journal);
    auto ticket = journal->begin_release(cookie, 1, {}, 110, count, Update());
    journal->end_release(ticket.ticket, 111, false);
    Check(journal->has(sem::Issue::kInvalidArgument) && !journal->audit_v1_shape_compatible(),
          "out-of-schema release count cannot serialize");
  }
  for (const auto previous : {int32_t{-1}, int32_t{32768}}) {
    auto journal = New(); Create(*journal);
    auto ticket = journal->begin_release(cookie, 1, {}, 110, 1, Update());
    journal->end_release(ticket.ticket, 111, true, previous);
    Check(journal->has(sem::Issue::kInvalidArgument) && !journal->audit_v1_shape_compatible(),
          "invalid actual previous count retained but cannot serialize");
  }
  auto context = New(); Create(*context);
  const auto released = context->begin_release(cookie, 1, {}, 110, 1,
                                               {static_cast<sem::ContextKind>(99), {}});
  context->end_release(released.ticket, 111, false);
  Check(!context->audit_v1_shape_compatible(), "unknown context enum cannot serialize");
  auto timeout = New(); Create(*timeout);
  const auto waited = timeout->begin_wait(cookie, 1, {}, 110, static_cast<sem::TimeoutKind>(99));
  timeout->end_wait(waited.ticket, 111, static_cast<sem::WaitResult>(99));
  Check(!timeout->audit_v1_shape_compatible() && timeout->drained(), "invalid timeout/result enums remain rejected raw completion");
  auto kind = New(); Create(*kind);
  Check(!kind->begin_unsupported(cookie, 1, {}, 110, static_cast<sem::Kind>(99)).ticket.valid() &&
        !kind->audit_v1_shape_compatible(), "invalid operation kind not admitted");
  auto stopped = New(); Create(*stopped); stopped->stop_admissions(window.end_qpc_exclusive);
  stopped->begin_wait(cookie, 1, {}, window.end_qpc_exclusive, sem::TimeoutKind::kZero);
  stopped->create(0x8000, window.end_qpc_exclusive + 1, 0, 8);
  stopped->destroy(cookie, window.end_qpc_exclusive + 2);
  Check(stopped->history_complete() && stopped->diagnostics().stopped_attempts == 3,
        "post-window observations do not invalidate the earlier complete fixed window");
}
}  // namespace

int main() {
  DisabledAndConfiguration(); JournalIdentityAndSaturation(); LifecycleAndAliases(); UnknownAndEpochOrigins(); ReleaseAndWaitResults();
  OverlapAndExactlyOnceCompletion(); ClockAndArguments(); UnsupportedAndDestruction(); CapacitiesAndCuts();
  MalformedEnumsAndBounds();
  std::cout << "semaphore observer core: " << checks << " checks, " << failures << " failures\n";
  return failures ? 1 : 0;
}
