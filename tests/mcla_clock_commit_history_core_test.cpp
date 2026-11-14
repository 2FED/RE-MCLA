// Pure C++20 storage contract. Primary owns compilation/native execution.
// std::mutex models CALLER serialization only, not the real SDK/scheduler.
#include "../src/mcla_clock_commit_history_core.h"
#include <atomic>
#include <cstring>
#include <iostream>
#include <latch>
#include <limits>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <thread>
#include <type_traits>
#include <utility>
#include <vector>

namespace {
namespace h = mcla::clock_commit_history;
unsigned checks{}, failures{};
void Check(bool ok, const char* label) {
  ++checks; if (!ok) { ++failures; std::cerr << "FAIL " << label << '\n'; }
}
constexpr h::RunToken run{0x1122334455667788ull, 0x99AABBCCDDEEFF01ull};
h::Binding Bound(uint64_t timer = 7, uint64_t read = 0) {
  return {run.high, run.low, timer, read, 0, 0, 0, 0, 1, 0};
}
h::Metadata Advance(uint64_t sample = 100, uint64_t guest = 500) {
  return {sample, guest, sample - 1, guest - 5, sample, guest, 5, 1, 5, 1, h::kQueryFields, 0};
}
h::Metadata Cached() { return {98, 500, 100, 500, 100, 500, 5, 1, 5, 1, h::kQueryFields, 0}; }
h::Metadata Ratio() { return {0, 0, 100, 500, 100, 500, 5, 1, 3, 2, h::kRatioFields, 0}; }
h::Metadata Unordered() { return {100, 500, 0, 0, 0, 0, 5, 1, 0, 0, h::kUnorderedFields, 0}; }
bool Has(uint32_t mask, h::Issue flag) { return (mask & flag) != 0; }
bool Zero(const h::Record& r) {
  const h::Record zero{}; return std::memcmp(&r, &zero, sizeof(r)) == 0;
}
template<class T> bool SameBytes(const T& a, const T& b) { return std::memcmp(&a, &b, sizeof(T)) == 0; }

void PodDisabledAndLifecycle() {
  static_assert(std::is_trivial_v<h::RunToken> && std::is_standard_layout_v<h::RunToken>);
  static_assert(std::is_trivial_v<h::Binding> && std::is_standard_layout_v<h::Binding>);
  static_assert(std::is_trivial_v<h::Metadata> && std::is_standard_layout_v<h::Metadata>);
  static_assert(std::is_trivial_v<h::Record> && std::is_standard_layout_v<h::Record>);
  // The byte-retention assertions below also cover every field: these size
  // checks exclude hidden padding in this explicitly laid out POD inventory.
  static_assert(sizeof(h::Binding) == 64 && sizeof(h::Metadata) == 88);
  static_assert(sizeof(h::Record) == 184 && sizeof(h::Counts) == 80);
  static_assert(sizeof(h::Recorder<2>::Snapshot) == 104 + 2 * sizeof(h::Record));
  static_assert(std::is_trivially_copyable_v<h::Recorder<2>::Snapshot>);
  static_assert(!std::is_copy_constructible_v<h::Recorder<2>::Lease>);
  static_assert(!std::is_copy_assignable_v<h::Recorder<2>::Lease>);
  static_assert(std::is_nothrow_move_constructible_v<h::Recorder<2>::Lease>);
  h::Recorder<2> core;
  auto disabled = core.BeginLocked(h::Kind::Advance, Bound());
  Check(!disabled.accepted() && disabled.status() == h::StartStatus::Disabled, "default OFF admits no row");
  Check(!disabled.Publish(Advance()), "disabled publication is inert");
  h::Recorder<2>::Snapshot out, before;
  std::memset(&out, 0xA5, sizeof(out)); std::memcpy(&before, &out, sizeof(out));
  Check(!core.TryFreezeInto(out) && SameBytes(out, before), "unarmed freeze leaves every output byte unchanged");
  Check(!core.Arm({0, 0}) && core.Arm(run), "zero token rejects without consuming valid Arm");
  Check(!core.Arm(run), "Arm is once only");
  auto held = core.BeginLocked(h::Kind::Advance, Bound());
  core.Stop(); core.Stop();
  Check(held.accepted() && !core.TryFreezeInto(out) && SameBytes(out, before), "held lease after Stop leaves full output unchanged");
  Check(core.BeginUnordered().status() == h::StartStatus::Stopped && !core.Arm(run), "Stop is irreversible and rejects admissions");
  Check(held.Publish(Advance()), "pre-Stop held lease publishes after Stop");
  Check(!held.Publish(Cached()), "one lease cannot publish twice");
  Check(core.TryFreezeInto(out) && out.size == 1 && out.run.high == run.high && out.run.low == run.low,
        "drained epoch freezes its full token");
  Check(out.counts.observed_attempts == 1 && out.counts.mutex_ordinals == 1 && out.counts.stored_entries == 1 &&
        out.counts.published_entries == 1 && !out.counts.aborted_entries && !out.counts.diagnostic_entries,
        "one completed raw row has exact counters");
  Check(Zero(out.rows[1]), "unused frozen row is initialized");
  Check(!h::kNativeAdmitted && !h::kCallerMutexDisciplineProven && !h::kCompleteClockHistoryProven &&
        !h::kOriginalSemanticsProven && !h::kSimulationProven && !h::kFpsProven, "storage never promotes engineering proof");
  h::Recorder<1> stopped; stopped.Stop();
  h::Recorder<1>::Snapshot empty{};
  Check(!stopped.Arm(run) && stopped.TryFreezeInto(empty) && empty.size == 0, "Stop-before-Arm closes an empty recorder");
}

void OrdersAndKinds() {
  h::Recorder<8> core; Check(core.Arm(run), "order Arm");
  auto first = core.BeginUnordered();
  auto second = core.BeginLocked(h::Kind::Advance, Bound(91));
  auto third = core.BeginUnordered(Bound(92, 4));
  auto fourth = core.BeginLocked(h::Kind::Ratio);
  auto fifth = core.BeginLocked(h::Kind::Cached, Bound(91, 1));
  Check(fifth.Publish(Cached()) && fourth.Publish(Ratio()) && third.Publish(Unordered()) &&
        second.Publish(Advance()) && first.Publish(Unordered()), "reverse publication preserves admission identities");
  core.Stop(); h::Recorder<8>::Snapshot out{}, repeat{};
  Check(core.TryFreezeInto(out) && core.TryFreezeInto(repeat) && SameBytes(out, repeat), "repeat freeze is byte stable");
  Check(out.size == 5 && out.counts.observed_attempts == 5 && out.counts.unordered_attempts == 2 &&
        out.counts.mutex_ordinals == 3 && out.counts.published_entries == 5 && !out.counts.diagnostic_entries,
        "query and ratio share mutex ordinal while unordered does not consume it");
  Check(out.rows[0].storage_ordinal == 1 && out.rows[0].mutex_ordinal == 0 &&
        out.rows[2].storage_ordinal == 3 && out.rows[2].mutex_ordinal == 0, "unordered rows have explicit zero mutex ordinal");
  Check(out.rows[1].storage_ordinal == 2 && out.rows[1].mutex_ordinal == 1 &&
        out.rows[4].storage_ordinal == 5 && out.rows[4].mutex_ordinal == 3, "locked query ordinals differ from storage order");
  Check(out.rows[0].binding.present == 0 && out.rows[3].binding.present == 0 &&
        out.rows[1].binding.timer_invocation == 91 && out.rows[2].binding.timer_invocation == 92 &&
        out.rows[4].binding.read_ordinal == 1, "explicit unbound stays unbound beside bound requests");
  Check(out.rows[3].kind == h::Kind::Ratio && SameBytes(out.rows[3].metadata, Ratio()), "effective ratio pre/post tuple retained");
  Check(out.rows[4].kind == h::Kind::Cached && out.rows[4].metadata.host_sample == 98 &&
        out.rows[4].metadata.pre_host == 100 && out.rows[4].metadata.post_host == 100,
        "older cached sample survives unchanged");
  Check(out.rows[0].ordering == h::Ordering::Unordered && out.rows[0].kind == h::Kind::NoScaling &&
        out.rows[0].metadata.presence == h::kUnorderedFields && !out.rows[0].metadata.post_host,
        "NoScaling never fabricates locked state");
#if defined(MCLA_CLOCK_COMMIT_HISTORY_MUTEX_ORDINAL_MUTANT)
  // Targeted TEST-ONLY negative control: after recording, corrupt exactly one
  // known mutex ordinal to its storage ordinal. No live/core mutation or claim
  // of a production defect. The unchanged comparison below must fail once.
  out.rows[3].mutex_ordinal = out.rows[3].storage_ordinal;
#endif
  Check(out.rows[3].storage_ordinal == 4 && out.rows[3].mutex_ordinal == 2,
        "ratio mutex ordinal was confused with storage ordinal");
}

void RawContradictions() {
  h::Recorder<8> core; Check(core.Arm(run), "raw Arm");
  auto wrong = Bound(); wrong.present = 7; wrong.run_low ^= 1; wrong.reserved = 29;
  wrong.phase_present = 9; wrong.phase_call_id = 123;
  auto metadata = Advance(); metadata.returned_guest ^= 7; metadata.post_host ^= 2;
  metadata.post_ratio_denominator = 0; metadata.presence |= 0x80000000u; metadata.reserved = 0xAABB;
  Check(core.BeginLocked(h::Kind::Advance, wrong).Publish(metadata), "malformed raw metadata is retained not rejected");
  auto absent = h::Binding{}; absent.read_ordinal = 71;
  auto unknown = Advance(); unknown.presence = 0x40000000u;
  Check(core.BeginLocked(static_cast<h::Kind>(0xDEADBEEFu), absent).Publish(unknown), "unknown kind and presence retain all values");
  auto cached = Cached(); cached.post_guest += 1;
  Check(core.BeginLocked(h::Kind::Cached).Publish(cached), "contradictory cached state is stored");
  auto ratio = Ratio(); ratio.post_host += 1;
  Check(core.BeginLocked(h::Kind::Ratio).Publish(ratio), "contradictory ratio state is stored");
  auto no = Unordered(); no.presence |= h::kPostState; no.post_host = 12345;
  Check(core.BeginLocked(h::Kind::NoScaling).Publish(no), "wrong ordered NoScaling remains diagnostic raw");
  auto missing = Advance(); missing.presence = h::kHostSample;
  Check(core.BeginLocked(h::Kind::Advance).Publish(missing), "missing fields remain raw");
  core.Stop(); h::Recorder<8>::Snapshot out{}; Check(core.TryFreezeInto(out), "raw freeze");
  Check(out.size == 6 && out.counts.diagnostic_entries == 6 && out.counts.published_entries == 6,
        "diagnostics never filter published rows");
  Check(SameBytes(out.rows[0].binding, wrong) && SameBytes(out.rows[0].metadata, metadata), "all malformed binding and metadata bytes retained");
  const auto issues = out.rows[0].issues;
  Check(Has(issues, h::kInvalidBinding) && Has(issues, h::kUnknownPresence) && Has(issues, h::kInvalidRatio) &&
        Has(issues, h::kReturnedMismatch) && Has(issues, h::kAdvanceHostMismatch) &&
        Has(issues, h::kQueryRatioChanged) && Has(issues, h::kNonzeroReserved), "independent raw issue bits retained together");
  Check(static_cast<uint32_t>(out.rows[1].kind) == 0xDEADBEEFu && SameBytes(out.rows[1].metadata, unknown) &&
        out.rows[1].binding.read_ordinal == 71 && Has(out.rows[1].issues, h::kInvalidKind), "unknown enum and unavailable values not repaired");
  Check(Has(out.rows[2].issues, h::kCachedStateChanged) && Has(out.rows[3].issues, h::kRatioStateChanged) &&
        Has(out.rows[4].issues, h::kWrongOrdering) && Has(out.rows[4].issues, h::kUnexpectedFields) &&
        Has(out.rows[5].issues, h::kMissingFields), "branch shape diagnostics remain separate");
}

void MovesAbandonAndUnwind() {
  h::Recorder<8> core; Check(core.Arm(run), "move Arm");
  { auto abandoned = core.BeginLocked(h::Kind::Advance, Bound(1)); Check(abandoned.accepted(), "abandon admitted"); }
  try { auto unwound = core.BeginLocked(h::Kind::Cached, Bound(2)); (void)unwound; throw std::runtime_error("owned synthetic unwind"); }
  catch (const std::runtime_error&) {}
  auto a = core.BeginLocked(h::Kind::Advance, Bound(3));
  auto b = core.BeginUnordered(Bound(4));
  auto moved = std::move(a);
  Check(!a.accepted() && !a.Publish(Advance()), "moved-from lease is inert");
  b = std::move(moved); // Destination unordered row aborts before transfer.
  auto* same_lease = &b;
  b = std::move(*same_lease); // Self move through alias keeps the accepted row.
  Check(b.accepted() && b.Publish(Advance()), "move assignment and self move preserve source lease");
  core.Stop(); h::Recorder<8>::Snapshot out{}; Check(core.TryFreezeInto(out), "move freeze");
  Check(out.size == 4 && out.counts.aborted_entries == 3 && out.counts.published_entries == 1 &&
        out.counts.diagnostic_entries == 3, "destruction unwind and replaced destination all abort exactly once");
  for (size_t i : {size_t(0), size_t(1), size_t(3)})
    Check(out.rows[i].disposition == h::Disposition::Aborted && Has(out.rows[i].issues, h::kAborted) &&
          Has(out.rows[i].issues, h::kMetadataUnavailable), "aborted row remains explicit and unavailable");
  Check(out.rows[2].binding.timer_invocation == 3 && out.rows[2].disposition == h::Disposition::Published,
        "move never changes recorded binding");
}

void OverflowAndCounterLimits() {
  h::Recorder<2> core; Check(core.Arm(run), "overflow Arm");
  auto a = core.BeginUnordered(); auto b = core.BeginLocked(h::Kind::Ratio);
  Check(a.Publish(Unordered()) && b.Publish(Ratio()), "capacity fills without overwrite");
  Check(core.BeginLocked(h::Kind::Advance).status() == h::StartStatus::Overflow &&
        core.BeginUnordered().status() == h::StartStatus::Overflow &&
        core.BeginLocked(h::Kind::Cached).status() == h::StartStatus::Overflow, "locked and unordered excess attempts overflow");
  core.Stop(); h::Recorder<2>::Snapshot out{}, repeat{};
  Check(core.TryFreezeInto(out) && out.size == 2 && out.counts.observed_attempts == 5 &&
        out.counts.mutex_ordinals == 3 && out.counts.unordered_attempts == 2 && out.counts.overflow_entries == 3,
        "all overflow locked attempts consume mutex ordinals but no rows");
  Check(out.rows[0].kind == h::Kind::NoScaling && out.rows[1].kind == h::Kind::Ratio &&
        out.rows[1].mutex_ordinal == 1 && SameBytes(out.rows[1].metadata, Ratio()), "overflow preserves original first rows");
  Check(core.TryFreezeInto(repeat) && SameBytes(out, repeat), "overflow snapshot repeats identically");
  h::Recorder<1, 3> limited; Check(limited.Arm(run), "limit Arm");
  Check(limited.BeginUnordered().Publish(Unordered()), "limit keeps first row");
  Check(limited.BeginUnordered().status() == h::StartStatus::Overflow &&
        limited.BeginLocked(h::Kind::Ratio).status() == h::StartStatus::Overflow, "pre-limit overflow inventory explicit");
  for (unsigned i = 0; i < 5; ++i)
    Check(limited.BeginLocked(h::Kind::Advance).status() == h::StartStatus::CounterSaturated, "counter saturation never wraps or overwrites");
  limited.Stop(); h::Recorder<1, 3>::Snapshot lim{};
  Check(limited.TryFreezeInto(lim) && lim.size == 1 && lim.counts.observed_attempts == 3 &&
        lim.counts.mutex_ordinals == 3 && lim.counts.unordered_attempts == 2 &&
        lim.counts.overflow_entries == 2 && lim.counts.saturated_rejections == 3 && lim.counts.counter_saturated == 1,
        "saturated lower-bound counters and independent mutex ordinal retained");
  Check(lim.rows[0].mutex_ordinal == 0 && lim.rows[0].storage_ordinal == 1, "saturation never invents unordered mutex identity");
}

void HeldPublisherAndCallerMutex() {
  h::Recorder<8> core; Check(core.Arm(run), "held thread Arm");
  std::mutex original_mutex;
  std::latch reserved(1), allow_publish(1);
  std::atomic<bool> published{false};
  std::thread producer([&] {
    h::Recorder<8>::Lease lease;
    { std::lock_guard lock(original_mutex); lease = core.BeginLocked(h::Kind::Advance, Bound(101)); }
    reserved.count_down(); allow_publish.wait();
    published.store(lease.Publish(Advance()), std::memory_order_release);
  });
  reserved.wait();
  core.Stop(); // Returns while the owned producer is deliberately held.
  h::Recorder<8>::Snapshot out, before;
  std::memset(&out, 0x6D, sizeof(out)); std::memcpy(&before, &out, sizeof(out));
  Check(!published.load(std::memory_order_acquire) && !core.TryFreezeInto(out) && SameBytes(out, before),
        "nonblocking Stop does not expose pending publisher or modify snapshot");
  allow_publish.count_down(); producer.join();
  Check(published.load(std::memory_order_acquire) && core.TryFreezeInto(out) && out.size == 1 &&
        out.rows[0].binding.timer_invocation == 101, "publisher release drain exposes its entire row");
}

void ConcurrentLastSlotsAndStop() {
  constexpr size_t capacity = 64, workers = 8;
  h::Recorder<capacity> core; Check(core.Arm(run), "concurrent Arm");
  std::mutex original_mutex;
  std::latch begin(1), admitted(workers), release(workers);
  std::atomic<unsigned> published{0}, rejected{0};
  std::vector<std::thread> threads;
  for (size_t t = 0; t < workers; ++t) threads.emplace_back([&, t] {
    begin.wait();
    for (size_t i = 0; i < capacity / workers; ++i) {
      h::Recorder<capacity>::Lease lease;
      { std::lock_guard lock(original_mutex); lease = core.BeginLocked(h::Kind::Advance, Bound(1 + t * 8 + i)); }
      if (i + 1 == capacity / workers) { admitted.count_down(); release.wait(); }
      if (lease.Publish(Advance())) published.fetch_add(1, std::memory_order_relaxed);
      else rejected.fetch_add(1, std::memory_order_relaxed);
    }
  });
  begin.count_down(); admitted.wait(); core.Stop();
  h::Recorder<capacity>::Snapshot out{};
  Check(!core.TryFreezeInto(out), "concurrent last slots remain pending after Stop");
  for (size_t i = 0; i < workers; ++i) release.count_down();
  for (auto& thread : threads) thread.join();
  Check(core.TryFreezeInto(out) && out.size == capacity && published.load() == capacity && !rejected.load() &&
        out.counts.mutex_ordinals == capacity && out.counts.published_entries == capacity,
        "caller-serialized concurrent last slots drain without lost rows");
  bool exact = true;
  for (size_t i = 0; i < capacity; ++i)
    exact = exact && out.rows[i].storage_ordinal == i + 1 && out.rows[i].mutex_ordinal == i + 1 &&
        out.rows[i].disposition == h::Disposition::Published && !out.rows[i].issues;
  Check(exact, "all mutex-serialized native-test rows retain exact order");
}

void ArmStopRace() {
  for (unsigned i = 0; i < 32; ++i) {
    h::Recorder<2> core; std::latch go(1);
    std::atomic<bool> armed{false};
    std::thread a([&] { go.wait(); armed.store(core.Arm(run), std::memory_order_relaxed); });
    std::thread b([&] { go.wait(); core.Stop(); });
    go.count_down(); a.join(); b.join();
    h::Recorder<2>::Snapshot out{};
    Check(core.TryFreezeInto(out) && out.size == 0 && !core.Arm(run) &&
          core.BeginLocked(h::Kind::Advance).status() == h::StartStatus::Stopped,
          "Arm Stop race never resurrects or exposes initializing token");
    Check((out.run.high == 0 && out.run.low == 0) || (out.run.high == run.high && out.run.low == run.low),
          "Arm Stop race token is fully initialized or absent");
    (void)armed;
  }
}

void ConcurrentOrderedAndUnordered() {
  constexpr size_t each = 48;
  h::Recorder<2 * each> core; Check(core.Arm(run), "mixed concurrent Arm");
  std::mutex original_mutex;
  std::latch go(1);
  std::atomic<unsigned> published{0};
  std::thread locked([&] {
    go.wait();
    for (size_t i = 0; i < each; ++i) {
      h::Recorder<2 * each>::Lease lease;
      { std::lock_guard lock(original_mutex); lease = core.BeginLocked(h::Kind::Cached, Bound(i + 1)); }
      if (lease.Publish(Cached())) published.fetch_add(1, std::memory_order_relaxed);
    }
  });
  std::thread unordered([&] {
    go.wait();
    for (size_t i = 0; i < each; ++i)
      if (core.BeginUnordered().Publish(Unordered())) published.fetch_add(1, std::memory_order_relaxed);
  });
  go.count_down(); locked.join(); unordered.join(); core.Stop();
  h::Recorder<2 * each>::Snapshot out{};
  Check(core.TryFreezeInto(out) && out.size == 2 * each && published.load() == 2 * each &&
        out.counts.mutex_ordinals == each && out.counts.unordered_attempts == each &&
        !out.counts.overflow_entries, "unordered producers race safely with caller-serialized mutex producers");
  uint64_t next_mutex = 1, locked_count = 0, unordered_count = 0;
  bool all = true;
  for (size_t i = 0; i < 2 * each; ++i) {
    const auto& row = out.rows[i];
    all = all && row.storage_ordinal == i + 1 && !row.issues && row.disposition == h::Disposition::Published;
    if (row.ordering == h::Ordering::Mutex) {
      all = all && row.mutex_ordinal == next_mutex++ && row.kind == h::Kind::Cached;
      ++locked_count;
    } else {
      all = all && row.mutex_ordinal == 0 && row.kind == h::Kind::NoScaling && !row.binding.present;
      ++unordered_count;
    }
  }
  Check(all && locked_count == each && unordered_count == each,
        "mixed rows retain complete independent storage and mutex inventories");
}

void StopAdmissionRace() {
  for (unsigned iteration = 0; iteration < 16; ++iteration) {
    h::Recorder<64> core; Check(core.Arm(run), "Stop admission race Arm");
    std::mutex original_mutex;
    std::latch go(1);
    std::atomic<uint64_t> published{0}, stopped{0};
    std::thread producer([&] {
      go.wait();
      for (uint64_t i = 0; i < 64; ++i) {
        h::Recorder<64>::Lease lease;
        { std::lock_guard lock(original_mutex); lease = core.BeginLocked(h::Kind::Ratio); }
        if (lease.accepted()) {
          if (lease.Publish(Ratio())) published.fetch_add(1, std::memory_order_relaxed);
        } else if (lease.status() == h::StartStatus::Stopped)
          stopped.fetch_add(1, std::memory_order_relaxed);
      }
    });
    std::thread stop([&] { go.wait(); core.Stop(); });
    go.count_down(); producer.join(); stop.join();
    h::Recorder<64>::Snapshot out{};
    Check(core.TryFreezeInto(out) && published.load() + stopped.load() == 64 &&
          out.size == published.load() && out.counts.published_entries == published.load() &&
          out.counts.mutex_ordinals == published.load() && !out.counts.overflow_entries,
          "Stop admission race admits or rejects each attempt exactly once");
  }
}

void MaximumConfiguredCapacity() {
  using Core = h::Recorder<4096>;
  // Heap-owned storage; never an automatic full-capacity Snapshot temporary.
  std::unique_ptr<Core> core(new Core);
  std::unique_ptr<Core::Snapshot> out(new Core::Snapshot);
  Check(core->Arm(run), "maximum capacity Arm");
  bool admitted = true;
  for (size_t i = 0; i < 4096; ++i)
    admitted = core->BeginLocked(h::Kind::Advance, Bound(i + 1)).Publish(Advance()) && admitted;
  Check(admitted && core->BeginLocked(h::Kind::Ratio).status() == h::StartStatus::Overflow,
        "exact 4096 stored rows then explicit overflow");
  core->Stop(); Check(core->TryFreezeInto(*out) && out->size == 4096 && out->counts.mutex_ordinals == 4097 &&
        out->counts.overflow_entries == 1 && out->counts.published_entries == 4096,
        "maximum capacity freezes no truncated or overwritten row");
  bool all = true;
  for (size_t i = 0; i < 4096; ++i)
    all = all && out->rows[i].storage_ordinal == i + 1 && out->rows[i].mutex_ordinal == i + 1 &&
        out->rows[i].binding.timer_invocation == i + 1 && !out->rows[i].issues &&
        SameBytes(out->rows[i].metadata, Advance());
  Check(all, "all 4096 raw metadata and request rows survive");
}
} // namespace
int main() {
  PodDisabledAndLifecycle(); OrdersAndKinds(); RawContradictions(); MovesAbandonAndUnwind();
  OverflowAndCounterLimits(); HeldPublisherAndCallerMutex(); ConcurrentLastSlotsAndStop();
  ArmStopRace(); ConcurrentOrderedAndUnordered(); StopAdmissionRace(); MaximumConfiguredCapacity();
  std::cout << "checks=" << checks << " failures=" << failures << '\n';
  return failures ? 1 : 0;
}
