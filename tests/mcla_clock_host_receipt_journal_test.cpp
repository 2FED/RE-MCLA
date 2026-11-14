// Pure prerequisite tests; primary alone compiles/runs. No real clock, SDK,
// game, OS input or file write. Allocations below are preflight test storage,
// never a journal/lease operation; no OOM or private-state corruption is forced.
#include "../src/mcla_clock_host_receipt_journal.h"
#include <array>
#include <cstring>
#include <iostream>
#include <latch>
#include <memory>
#include <thread>
#include <type_traits>

namespace {
namespace journal = mcla::clock_read_witness::host_journal;
namespace host = mcla::clock_read_witness::host;
constexpr journal::RunToken kRun{0x123456789ABCDEF0ull,0xFEDCBA9876543210ull};
unsigned checks{}, failures{};
void Check(bool ok, const char* label) {
  ++checks; if (!ok) { ++failures; std::cerr << "FAIL " << label << '\n'; }
}
struct NoNative {
  static NoNative Capture() noexcept { return {}; }
  void Restore() const noexcept {}
};
struct Signal { unsigned code; };
static_assert(std::is_trivial_v<journal::Record> && std::is_standard_layout_v<journal::Record>);
static_assert(std::is_trivial_v<journal::Journal<4>::Snapshot>);
journal::Invocation Invocation(uint64_t timer) noexcept {
  return {timer,100+timer,7,31,0x827D7500,0x822C21B4,31,1};
}
void Healthy(host::Trace<16>& trace, uint64_t timer, uint64_t result = 77) noexcept {
  trace.observed_reads = 1; trace.size = 1;
  auto& row = trace.rows[0];
  row.scope_binding = {1,sizeof(row.scope_binding),kRun.high,kRun.low,timer,0,0,0,0,0,0,0};
  row.request = row.scope_binding;
  row.status = {1,sizeof(row.status),MCLA_CW_ACCEPTED,MCLA_CW_PUBLISHED,0,0};
  row.result = result; row.trace_ordinal = 1; row.result_available = 1;
  row.used_query = row.read_ordinal_available = row.status_available = 1;
}
template<size_t Capacity> auto Freeze(journal::Journal<Capacity>& core) {
  auto snapshot = std::make_unique<typename journal::Journal<Capacity>::Snapshot>();
  Check(core.TryFreezeInto(*snapshot),"sealed quiescent journal freezes"); return snapshot;
}

void DisabledOnceAndNormalZero() {
  auto core = std::make_unique<journal::Journal<4>>();
  auto disabled = core->Begin(Invocation(1));
  Check(!disabled.accepted() && disabled.status() == journal::StartStatus::Disabled && !disabled.trace() &&
      !disabled.Complete(journal::Outcome::Normal),"default OFF has no slot or fabricated admission");
  auto pending = std::make_unique<journal::Journal<4>::Snapshot>();
  std::memset(pending.get(),0xA5,sizeof(*pending));
  auto before = std::make_unique<journal::Journal<4>::Snapshot>();
  std::memcpy(before.get(),pending.get(),sizeof(*pending));
  Check(!core->TryFreezeInto(*pending) && std::memcmp(before.get(),pending.get(),sizeof(*pending)) == 0,
      "unsealed freeze leaves caller destination unchanged");
  Check(!core->Arm({0,0}) && core->Arm(kRun) && !core->Arm(kRun),"one valid full128 Arm without reset");
  auto first = core->Begin(Invocation(7));
  Check(first.accepted() && first.trace(),"bound original invocation obtains exclusive slot trace");
  Healthy(*first.trace(),7);
  Check(first.Complete(journal::Outcome::Normal) && !first.Complete(journal::Outcome::Normal),
      "Complete publishes exactly once");
  auto zero = core->Begin(Invocation(0));
  Check(zero.accepted() && zero.trace() && zero.Complete(journal::Outcome::Normal),
      "unadmitted timer zero is retained without inventing timer identity");
  core->Stop();
  auto snapshot = Freeze(*core);
  Check(snapshot->size == 2 && snapshot->run.high == kRun.high && snapshot->run.low == kRun.low &&
      snapshot->counts.observed_scopes == 2 && snapshot->counts.stored_scopes == 2 &&
      snapshot->counts.completed_scopes == 2 && snapshot->counts.normal_scopes == 2 &&
      snapshot->counts.zero_read_scopes == 1 && !snapshot->counts.aborted_scopes && !snapshot->counts.inconclusive,
      "normal and zero-read outcomes/counters explicit without physical claims");
  Check(snapshot->rows[0].storage_ordinal == 1 && snapshot->rows[0].invocation.timer_ordinal == 7 &&
      snapshot->rows[1].storage_ordinal == 2 && snapshot->rows[1].invocation.timer_ordinal == 0 &&
      snapshot->rows[1].flags == journal::kZeroReads && snapshot->rows[1].trace.size == 0,
      "storage ordinal and original timer ordinal remain distinct raw domains");
  for (unsigned i = 0; i < 100; ++i) {
    auto stopped = core->Begin(Invocation(i+100));
    Check(stopped.status() == journal::StartStatus::Stopped && !stopped.trace(),"post-seal original path receives no retained slot");
  }
  core->SealAdmissions();
  auto repeat = Freeze(*core);
  Check(!core->Arm(kRun) && std::memcmp(snapshot.get(),repeat.get(),sizeof(*snapshot)) == 0,
      "post-seal traffic/repeated Stop cannot mutate frozen bytes or rearm");

  auto stopped_before_arm = std::make_unique<journal::Journal<1>>();
  stopped_before_arm->Stop();
  Check(!stopped_before_arm->Arm(kRun),"Stop before Arm is terminal");
  auto stopped_empty = Freeze(*stopped_before_arm);
  Check(!stopped_empty->size && !stopped_empty->run.high && !stopped_empty->run.low,
      "never-armed stopped inventory stays canonical empty");
}

void MoveAndAbandon() {
  auto core = std::make_unique<journal::Journal<4>>(); core->Arm(kRun);
  auto first = core->Begin(Invocation(1)); Healthy(*first.trace(),1);
  auto second = core->Begin(Invocation(2));
  auto moved = std::move(first);
  Check(!first.trace() && !first.Complete(journal::Outcome::Normal) && moved.accepted(),"move construction transfers exclusive writer lease");
  moved = std::move(moved);
  Check(moved.Complete(journal::Outcome::Normal),"self move preserves lease");
  auto third = core->Begin(Invocation(3));
  second = std::move(third); // Abandons old second, then owns third.
  Check(!third.trace() && second.accepted() && second.Complete(journal::Outcome::Unwound),
      "move assignment retains abandoned destination and transfers source");
  {
    auto abandoned = core->Begin(Invocation(4)); Healthy(*abandoned.trace(),4,88);
  }
  core->Stop(); auto snapshot = Freeze(*core);
  Check(snapshot->size == 4 && snapshot->counts.completed_scopes == 2 && snapshot->counts.normal_scopes == 1 &&
      snapshot->counts.unwound_scopes == 1 && snapshot->counts.aborted_scopes == 2 &&
      snapshot->counts.diagnostic_scopes == 3 && snapshot->counts.inconclusive,
      "abandoned and original-unwound outcomes never counted as healthy normal scopes");
  Check(snapshot->rows[1].outcome == journal::Outcome::Abandoned &&
      snapshot->rows[1].disposition == journal::Disposition::Aborted &&
      (snapshot->rows[1].flags & journal::kMissingComplete) &&
      snapshot->rows[3].trace.rows[0].result == 88,
      "default abort preserves existing raw trace and marks missing Complete");
}

void NestedScopeUnwindAndPending() {
  auto core = std::make_unique<journal::Journal<4>>(); core->Arm(kRun);
  auto destination = std::make_unique<journal::Journal<4>::Snapshot>();
  auto before = std::make_unique<journal::Journal<4>::Snapshot>();
  std::memset(destination.get(),0x5A,sizeof(*destination));
  std::memcpy(before.get(),destination.get(),sizeof(*destination));
  auto outer = core->Begin(Invocation(10));
  {
    int context{}, base{};
    const host::Identity id{kRun.high,kRun.low,10,110,7,31,1};
    host::Scope<NoNative> scope(id,&context,&base,nullptr,*outer.trace(),true);
    host::Read<NoNative>(&context,&base,[] { return uint64_t(10); });
    auto inner = core->Begin(Invocation(0));
    {
      host::Scope<NoNative> child({kRun.high,kRun.low,0,0,0,0,0},&context,&base,nullptr,*inner.trace(),false);
      host::Read<NoNative>(&context,&base,[] { return uint64_t(20); });
    } // Scope MUST die before Complete.
    Check(inner.Complete(journal::Outcome::Normal),"nested host trace publishes after nested Scope destruction");
    core->Stop();
    Check(!core->TryFreezeInto(*destination) && std::memcmp(before.get(),destination.get(),sizeof(*destination)) == 0,
        "Seal cannot freeze a partially written active outer trace");
    host::Read<NoNative>(&context,&base,[] { return uint64_t(30); });
  }
  Check(outer.Complete(journal::Outcome::Normal),"pre-seal accepted Scope can finish every later read before release");
  Check(core->TryFreezeInto(*destination) && destination->size == 2 &&
      destination->rows[0].invocation.timer_ordinal == 10 && destination->rows[0].trace.size == 2 &&
      destination->rows[0].trace.rows[0].request.read_ordinal == 0 && destination->rows[0].trace.rows[1].request.read_ordinal == 1 &&
      destination->rows[1].invocation.timer_ordinal == 0 && destination->rows[1].trace.rows[0].request.timer_invocation == 0,
      "nested reverse completion retains raw admission order and unbound receipt");

  auto unwound = std::make_unique<journal::Journal<1>>(); unwound->Arm(kRun);
  unsigned originals{}; bool propagated{};
  try {
    auto lease = unwound->Begin(Invocation(11));
    try {
      int context{}, base{};
      host::Scope<NoNative> scope({kRun.high,kRun.low,11,0,0,0,0},&context,&base,nullptr,*lease.trace(),true);
      host::Read<NoNative>(&context,&base,[&] { ++originals; return uint64_t(44); });
      throw Signal{43};
    } catch (...) { lease.Complete(journal::Outcome::Unwound); throw; }
  } catch (const Signal& signal) { propagated = signal.code == 43; }
  unwound->Stop(); auto snapshot = Freeze(*unwound);
  Check(propagated && originals == 1 && snapshot->rows[0].outcome == journal::Outcome::Unwound &&
      snapshot->rows[0].trace.rows[0].result == 44 && (snapshot->rows[0].flags & journal::kOriginalUnwound),
      "explicit original unwind publishes retained status before propagating unchanged exception");
}

void RawFlagsAndOverflow() {
  auto core = std::make_unique<journal::Journal<2>>(); core->Arm(kRun);
  auto lease = core->Begin(Invocation(0));
  auto& trace = *lease.trace();
  Healthy(trace,0);
  trace.size = 16; trace.observed_reads = 20; trace.lost_rows = 4;
  trace.overflow = trace.ordinal_saturated = trace.counter_saturated = 1;
  auto& receipt = trace.rows[0];
  receipt.status = {99,17,MCLA_CW_STOPPED,MCLA_CW_ABORTED,0x80000000u,0x11223344u};
  receipt.decisions = host::kInvalidStatus | host::kQueryUnavailable | host::kTraceOverflow | host::kQueryThrew;
  receipt.result_available = 0; receipt.threw = 1; receipt.status_available = 0;
  std::array<unsigned char,sizeof(host::Trace<16>)> raw{};
  std::memcpy(raw.data(),&trace,sizeof(trace));
  lease.Complete(journal::Outcome::Normal);
  auto invalid = core->Begin(Invocation(9)); invalid.trace()->size = 17;
  invalid.Complete(static_cast<journal::Outcome>(999));
  core->Stop(); auto snapshot = Freeze(*core);
  Check(std::memcmp(raw.data(),&snapshot->rows[0].trace,sizeof(trace)) == 0 &&
      snapshot->counts.trace_loss_scopes == 1 && snapshot->counts.ordinal_saturated_scopes == 1 &&
      snapshot->counts.trace_counter_saturated_scopes == 1 && snapshot->counts.diagnostic_scopes == 2 && snapshot->counts.inconclusive,
      "all raw unavailable malformed thrown overflow and saturation fields survive verbatim");
  Check(snapshot->rows[1].trace.size == 17 && snapshot->rows[1].outcome == static_cast<journal::Outcome>(999) &&
      snapshot->rows[1].disposition == journal::Disposition::Aborted &&
      (snapshot->rows[1].flags & journal::kInvalidTraceSize) && (snapshot->rows[1].flags & journal::kInvalidOutcome),
      "invalid public POD shape/outcome retained without out-of-bounds scan or promotion");

  auto full = std::make_unique<journal::Journal<1>>(); full->Arm(kRun);
  auto kept = full->Begin(Invocation(1)); Healthy(*kept.trace(),1);
  auto lost = full->Begin(Invocation(2));
  Check(!lost.accepted() && !lost.trace() && lost.status() == journal::StartStatus::Overflow,
      "overflow exposes no journal trace or invented replacement identity");
  host::Trace<16> local{}; unsigned originals{};
  {
    int context{}, base{};
    host::Scope<NoNative> inert({kRun.high,kRun.low,0,0,0,0,0},&context,&base,nullptr,local,false);
    Check(host::Read<NoNative>(&context,&base,[&] { ++originals; return uint64_t(55); }) == 55,
        "future overflow adapter can continue one original using caller-owned inert trace");
  }
  kept.Complete(journal::Outcome::Normal); full->Stop(); auto frozen = Freeze(*full);
  Check(originals == 1 && frozen->size == 1 && frozen->counts.observed_scopes == 2 &&
      frozen->counts.stored_scopes == 1 && frozen->counts.lost_scopes == 1 && frozen->counts.inconclusive &&
      frozen->rows[0].invocation.timer_ordinal == 1 && local.rows[0].request.timer_invocation == 0,
      "overflow loss explicit, retained row unmodified and local status outside inventory");
}

void ActualScopeTraceOverflow() {
  auto core = std::make_unique<journal::Journal<1>>(); core->Arm(kRun);
  auto lease = core->Begin(Invocation(70));
  std::array<host::Receipt,20> receipts{};
  unsigned originals{};
  {
    int context{}, base{};
    host::Scope<NoNative> scope({kRun.high,kRun.low,70,170,7,31,1},
        &context,&base,nullptr,*lease.trace(),true);
    for (unsigned i = 0; i < 20; ++i) {
      const uint64_t value = host::Read<NoNative>(&context,&base,
          [&] { return uint64_t(originals++); },&receipts[i]);
      Check(value == i && receipts[i].result == i && receipts[i].result_available &&
          receipts[i].read_ordinal_available && receipts[i].request.read_ordinal == i &&
          receipts[i].trace_ordinal == i+1 && !receipts[i].used_query && !receipts[i].status_available &&
          (receipts[i].decisions & host::kQueryUnavailable),
          "actual Scope Read preserves each ordinal/result with explicit unavailable SDK status");
    }
  }
  lease.Complete(journal::Outcome::Normal); core->Stop();
  auto snapshot = Freeze(*core); const auto& row = snapshot->rows[0];
  Check(originals == 20 && snapshot->size == 1 && snapshot->counts.observed_scopes == 1 &&
      snapshot->counts.stored_scopes == 1 && snapshot->counts.completed_scopes == 1 &&
      snapshot->counts.trace_loss_scopes == 1 && snapshot->counts.diagnostic_scopes == 1 && snapshot->counts.inconclusive &&
      row.trace.size == 16 && row.trace.observed_reads == 20 && row.trace.lost_rows == 4 && row.trace.overflow &&
      !row.trace.ordinal_saturated && !row.trace.counter_saturated &&
      (row.flags & journal::kTraceLoss) && (row.flags & journal::kReceiptDiagnostic) &&
      (receipts[19].decisions & host::kTraceOverflow),
      "real Trace16 overflow keeps sixteen rows reports four lost reads and remains inconclusive");
  for (unsigned i = 0; i < 16; ++i)
    Check(std::memcmp(&row.trace.rows[i],&receipts[i],sizeof(host::Receipt)) == 0,
        "every retained actual Scope receipt survives journal freeze verbatim without filtering");
}

void MaximumConfiguredCapacity() {
  // Both large objects are preflight heap allocations, never automatic copies
  // on an original-call path. These are synthetic caller-attested records, not
  // evidence that1024 real timer/SDK invocations or clock reads occurred.
  auto core = std::make_unique<journal::Journal<1024>>(); core->Arm(kRun);
  for (uint64_t i = 0; i < 1024; ++i) {
    auto lease = core->Begin(Invocation(i ? 100000+i : 0));
    Check(lease.accepted() && lease.trace() && lease.Complete(journal::Outcome::Normal),
        "maximum configured journal admits each of1024 zero-read original scopes");
  }
  auto lost = core->Begin(Invocation(999999));
  Check(!lost.accepted() && !lost.trace() && lost.status() == journal::StartStatus::Overflow,
      "1025th scope is explicit bounded overflow without overwrite");
  core->Stop(); auto snapshot = Freeze(*core);
  Check(snapshot->size == 1024 && snapshot->counts.observed_scopes == 1025 &&
      snapshot->counts.stored_scopes == 1024 && snapshot->counts.completed_scopes == 1024 &&
      snapshot->counts.normal_scopes == 1024 && snapshot->counts.zero_read_scopes == 1024 &&
      snapshot->counts.lost_scopes == 1 && snapshot->counts.inconclusive &&
      !snapshot->counts.unwound_scopes && !snapshot->counts.aborted_scopes && !snapshot->counts.diagnostic_scopes &&
      !snapshot->counts.trace_loss_scopes && !snapshot->counts.ordinal_saturated_scopes &&
      !snapshot->counts.trace_counter_saturated_scopes && !snapshot->counts.counter_saturated,
      "maximum capacity freeze has exact complete scope/loss counts, no hidden filtering");
  Check(snapshot->rows[0].storage_ordinal == 1 && snapshot->rows[0].invocation.timer_ordinal == 0 &&
      snapshot->rows[1023].storage_ordinal == 1024 && snapshot->rows[1023].invocation.timer_ordinal == 101023,
      "first/last maximum-capacity storage IDs never replace raw timer IDs");
  const host::Receipt zero{}; bool all_unused_receipts_zero = true;
  for (uint64_t i = 0; i < 1024; ++i) {
    const auto& row = snapshot->rows[i];
    Check(row.storage_ordinal == i+1 && row.invocation.timer_ordinal == (i ? 100000+i : 0) &&
        row.disposition == journal::Disposition::Completed && row.outcome == journal::Outcome::Normal &&
        row.flags == journal::kZeroReads && !row.trace.size && !row.trace.observed_reads && !row.trace.lost_rows &&
        !row.trace.overflow && !row.trace.ordinal_saturated && !row.trace.counter_saturated,
        "all1024 raw zero-read records retained in admission order");
    for (const auto& receipt : row.trace.rows) {
      const bool canonical = std::memcmp(&receipt,&zero,sizeof(zero)) == 0;
      all_unused_receipts_zero = canonical && all_unused_receipts_zero;
    }
  }
  Check(all_unused_receipts_zero,"every unused receipt in all1024 zero-read records remains canonical zero");
}

void StopAdmissionRaces() {
  for (unsigned iteration = 0; iteration < 64; ++iteration) {
    auto core = std::make_unique<journal::Journal<1>>(); core->Arm(kRun);
    auto early = std::make_unique<journal::Journal<1>::Snapshot>();
    auto before = std::make_unique<journal::Journal<1>::Snapshot>();
    std::memset(early.get(),0xA5,sizeof(*early)); std::memcpy(before.get(),early.get(),sizeof(*early));
    std::latch ready(2), release(1);
    bool accepted{}, early_ready{};
    std::thread producer([&] {
      ready.count_down(); release.wait();
      auto lease = core->Begin(Invocation(iteration+1)); accepted = lease.accepted();
      if (accepted) { Healthy(*lease.trace(),iteration+1); lease.Complete(journal::Outcome::Normal); }
    });
    std::thread stopper([&] {
      ready.count_down(); release.wait(); core->Stop(); early_ready = core->TryFreezeInto(*early);
    });
    ready.wait(); release.count_down(); producer.join(); stopper.join();
    auto final = Freeze(*core);
    Check(final->counts.observed_scopes == uint64_t(accepted) && final->size == uint64_t(accepted) &&
        final->counts.completed_scopes == uint64_t(accepted),"Stop/admission linearization admits a complete row or no row");
    Check(early_ready ? std::memcmp(early.get(),final.get(),sizeof(*early)) == 0
        : std::memcmp(early.get(),before.get(),sizeof(*early)) == 0,
        "racing freeze is either byte-stable final evidence or unchanged pending destination");
  }
  for (unsigned iteration = 0; iteration < 32; ++iteration) {
    auto core = std::make_unique<journal::Journal<1>>(); std::latch ready(2), release(1);
    std::thread armer([&] { ready.count_down(); release.wait(); core->Arm(kRun); });
    std::thread stopper([&] { ready.count_down(); release.wait(); core->Stop(); });
    ready.wait(); release.count_down(); armer.join(); stopper.join();
    auto snapshot = Freeze(*core);
    Check(!core->Arm(kRun) && !snapshot->size && !snapshot->counts.observed_scopes &&
        ((!snapshot->run.high && !snapshot->run.low) || (snapshot->run.high == kRun.high && snapshot->run.low == kRun.low)),
        "Arm/Stop race cannot resurrect admissions or expose partial128 run metadata");
  }
}
}  // namespace

int main() {
  try {
    DisabledOnceAndNormalZero(); MoveAndAbandon(); NestedScopeUnwindAndPending();
    RawFlagsAndOverflow(); ActualScopeTraceOverflow(); MaximumConfiguredCapacity(); StopAdmissionRaces();
  } catch (...) { Check(false,"unexpected synthetic host journal exception"); }
  Check(!journal::kNativeAdmitted && !journal::kCompleteClockHistoryProven && !journal::kSimulationProven && !journal::kFpsProven,
      "receipt storage is not native admission full history simulation or FPS proof");
  std::cout << "checks=" << checks << " failures=" << failures
      << " pure_host_receipt_journal_only counterMAX_not_executed no_runtime_integration\n";
  return failures ? 1 : 0;
}
