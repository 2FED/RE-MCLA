// Offline C++20 recorder contract. No SDK, guest, clocks, files or OS observation.
// Primary owns compilation/execution; std::threads below exercise the pure core.
#include "../src/mcla_clock_read_witness_core.h"

#include <atomic>
#include <cstring>
#include <iostream>
#include <latch>
#include <limits>
#include <thread>
#include <type_traits>
#include <utility>
#include <vector>

namespace {
namespace w = mcla::clock_read_witness;
unsigned checks{}, failures{};
void Check(bool ok, const char* label) {
  ++checks;
  if (!ok) { ++failures; std::cerr << "FAIL " << label << '\n'; }
}
constexpr w::RunToken run{0x1122334455667788ull, 0x99AABBCCDDEEFF01ull};
w::BoundRead Bound(uint64_t invocation = 7, uint64_t read = 0) {
  return {run, invocation, read, {0, 0, 0, 0}};
}
w::Metadata Advance(uint64_t sample = 100, uint64_t guest = 500) {
  return {sample, guest, 5, 1, sample - 1, guest - 5, sample, guest,
          w::kHostSample | w::kReturnedGuest | w::kRatio | w::kLockedState,
          w::Branch::Advance};
}
w::Metadata Cached() {
  return {98, 500, 5, 1, 100, 500, 100, 500,
          w::kHostSample | w::kReturnedGuest | w::kRatio | w::kLockedState,
          w::Branch::Cached};
}
w::Metadata NoScaling() {
  return {100, 500, 5, 1, 0, 0, 0, 0,
          w::kHostSample | w::kReturnedGuest | w::kRatio, w::Branch::NoScaling};
}
bool Has(uint32_t mask, w::Issue flag) { return (mask & flag) != 0; }
bool Same(const w::Metadata& a, const w::Metadata& b) {
  return a.host_sample == b.host_sample && a.returned_guest == b.returned_guest &&
      a.ratio_numerator == b.ratio_numerator && a.ratio_denominator == b.ratio_denominator &&
      a.pre_last_host == b.pre_last_host && a.pre_last_guest == b.pre_last_guest &&
      a.post_last_host == b.post_last_host && a.post_last_guest == b.post_last_guest &&
      a.presence == b.presence && a.branch == b.branch;
}
bool Same(const w::Record& a, const w::Record& b) {
  return a.admission_ordinal == b.admission_ordinal &&
      a.binding.run.high == b.binding.run.high && a.binding.run.low == b.binding.run.low &&
      a.binding.timer_invocation == b.binding.timer_invocation &&
      a.binding.read_ordinal == b.binding.read_ordinal &&
      a.binding.phase.call_id == b.binding.phase.call_id &&
      a.binding.phase.thread_epoch == b.binding.phase.thread_epoch &&
      a.binding.phase.native_thread_id == b.binding.phase.native_thread_id &&
      a.binding.phase.present == b.binding.phase.present && Same(a.metadata, b.metadata) &&
      a.disposition == b.disposition && a.issues == b.issues;
}
template<size_t Capacity>
bool SameSnapshot(const typename w::Recorder<Capacity>::Snapshot& a,
                  const typename w::Recorder<Capacity>::Snapshot& b) {
  const auto& x = a.counts; const auto& y = b.counts;
  if (a.run.high != b.run.high || a.run.low != b.run.low || a.size != b.size ||
      x.observed_bound_reads != y.observed_bound_reads || x.stored_entries != y.stored_entries ||
      x.published_entries != y.published_entries || x.aborted_entries != y.aborted_entries ||
      x.overflow_entries != y.overflow_entries || x.invalid_bindings != y.invalid_bindings ||
      x.diagnostic_entries != y.diagnostic_entries || x.saturated_rejections != y.saturated_rejections ||
      x.counter_saturated != y.counter_saturated) return false;
  for (size_t i = 0; i < Capacity; ++i) if (!Same(a.rows[i], b.rows[i])) return false;
  return true;
}

void PodAndDisabled() {
  static_assert(std::is_trivial_v<w::RunToken> && std::is_standard_layout_v<w::RunToken>);
  static_assert(std::is_trivially_copyable_v<w::BoundRead> && std::is_standard_layout_v<w::BoundRead>);
  static_assert(std::is_trivially_copyable_v<w::Metadata> && std::is_standard_layout_v<w::Metadata>);
  static_assert(std::is_trivially_copyable_v<w::Record> && std::is_standard_layout_v<w::Record>);
  static_assert(std::is_trivially_copyable_v<w::Recorder<2>::Snapshot>);
  static_assert(!std::is_copy_constructible_v<w::Recorder<2>::Lease>);
  static_assert(!std::is_copy_assignable_v<w::Recorder<2>::Lease>);
  static_assert(std::is_nothrow_move_constructible_v<w::Recorder<2>::Lease>);
  static_assert(std::is_nothrow_destructible_v<w::Recorder<2>::Lease>);
  w::Recorder<2> core;
  auto disabled = core.Begin(Bound());
  Check(!disabled.accepted() && disabled.status() == w::StartStatus::Disabled, "default disabled");
  Check(!disabled.Publish(Advance()), "disabled publication no-op");
  w::Recorder<2>::Snapshot out{}; out.size = 91;
  Check(!core.TrySnapshot(out) && out.size == 91, "unarmed snapshot not available or modified");
  core.Stop(); core.Stop();
  Check(core.TrySnapshot(out) && out.size == 0 && out.counts.observed_bound_reads == 0,
        "stop default recorder produces empty closed inventory");
  Check(!core.Arm(run), "stopped recorder cannot arm");
  Check(!w::kNativeAdmitted && !w::kAllClockHistoryProven && !w::kClockCommitOrderProven &&
        !w::kSimulationProven && !w::kFpsProven, "no scope or physical proof");
}

void LifecycleAndBranches() {
  w::Recorder<4> core;
  Check(!core.Arm({0, 0}), "zero run rejected");
  Check(core.Arm(run), "valid full run arm");
  Check(!core.Arm(run) && !core.Arm({run.high, run.low + 1}), "single configuration only");
  auto a = core.Begin(Bound(1)), b = core.Begin(Bound(2)), c = core.Begin(Bound(3));
  Check(a.accepted() && b.accepted() && c.accepted(), "three explicit bound reads admitted");
  Check(b.Publish(Cached()) && c.Publish(NoScaling()) && a.Publish(Advance()), "branches publish in reverse admission order");
  Check(!a.Publish(Cached()), "double publication rejected");
  core.Stop();
  Check(core.Begin(Bound()).status() == w::StartStatus::Stopped && !core.Arm(run), "stop one-way");
  w::Recorder<4>::Snapshot out{};
  Check(core.TrySnapshot(out) && out.size == 3, "closed snapshot");
  Check(out.rows[0].admission_ordinal == 1 && out.rows[1].admission_ordinal == 2 &&
        out.rows[2].admission_ordinal == 3, "unique admission order unaffected by completion order");
  Check(out.rows[0].binding.timer_invocation == 1 && out.rows[0].metadata.branch == w::Branch::Advance &&
        out.rows[1].binding.timer_invocation == 2 && out.rows[1].metadata.branch == w::Branch::Cached &&
        out.rows[2].binding.timer_invocation == 3 && out.rows[2].metadata.branch == w::Branch::NoScaling,
        "metadata remains attached to exact lease");
  Check(out.counts.published_entries == 3 && out.counts.aborted_entries == 0 &&
        out.counts.diagnostic_entries == 0, "healthy branch publication counts");
  Check(out.rows[1].metadata.host_sample == 98 && out.rows[1].metadata.pre_last_host == 100 &&
        out.rows[1].metadata.returned_guest == 500, "cached older host sample retained without repair");
  Check(out.rows[2].metadata.presence == (w::kHostSample | w::kReturnedGuest | w::kRatio) &&
        out.rows[2].metadata.pre_last_host == 0 && out.rows[2].metadata.post_last_guest == 0,
        "NoScaling has unavailable existing-lock fields");
}

void BoundTokensAndMoves() {
  w::Recorder<8> core; Check(core.Arm(run), "token/move arm");
  auto missing = Bound(); missing.timer_invocation = 0;
  Check(core.Begin(missing).status() == w::StartStatus::Unbound, "unbound invocation has no event");
  missing = Bound(); missing.run = {0, 0};
  Check(core.Begin(missing).status() == w::StartStatus::Unbound, "unbound run has no event");
  auto wrong = Bound(); wrong.run.high ^= 1;
  Check(core.Begin(wrong).status() == w::StartStatus::InvalidBinding, "full run high half checked");
  wrong = Bound(); wrong.run.low ^= 1;
  Check(core.Begin(wrong).status() == w::StartStatus::InvalidBinding, "full run low half checked");
  auto outer = core.Begin(Bound(11, 0));
  auto inner = core.Begin(Bound(12, 0));
  auto again = core.Begin(Bound(11, 0));
  auto moved = std::move(inner);
  Check(!inner.accepted() && !inner.Publish(Cached()), "moved-from lease cannot publish");
  Check(moved.Publish(Cached()) && again.Publish(Advance()) && outer.Publish(Advance()), "nested caller-provided tokens retained");
  auto first = core.Begin(Bound(21, 7));
  auto second = core.Begin(Bound(22, 7));
  second = std::move(first);  // Old destination lease is explicitly aborted.
  Check(second.Publish(NoScaling()), "move assignment transfers active slot");
  core.Stop(); w::Recorder<8>::Snapshot out{}; Check(core.TrySnapshot(out), "token/move closed");
  Check(out.size == 5 && out.counts.invalid_bindings == 2 && out.counts.observed_bound_reads == 5,
        "unbound and invalid tokens never fabricate rows or ordinals");
  Check(out.rows[0].binding.timer_invocation == 11 && out.rows[1].binding.timer_invocation == 12 &&
        out.rows[2].binding.timer_invocation == 11 && out.rows[0].binding.read_ordinal == 0 &&
        out.rows[1].binding.read_ordinal == 0 && out.rows[2].binding.read_ordinal == 0,
        "external repeated read ordinals are not admission identities");
  Check(out.rows[4].disposition == w::Disposition::Aborted &&
        Has(out.rows[4].issues, w::kAbortedRead) && out.counts.aborted_entries == 1,
        "move replacement preserves aborted row");
}

void AbortAndDrain() {
  w::Recorder<3> core; Check(core.Arm(run), "abort arm");
  auto a = core.Begin(Bound());
  w::Recorder<3>::Snapshot out{}; out.size = 47;
  core.Stop();
  Check(!core.TrySnapshot(out) && out.size == 47, "stop does not race active lease or modify destination");
  Check(a.Publish(Advance()), "already admitted lease may finish after stop");
  Check(core.TrySnapshot(out) && out.size == 1 && out.counts.published_entries == 1, "drain after active release");
  w::Recorder<3> unwind; Check(unwind.Arm(run), "unwind arm");
  try { auto pending = unwind.Begin(Bound()); Check(pending.accepted(), "unwind admission"); throw 9; }
  catch (int) {}
  auto explicit_abort = unwind.Begin(Bound(8)); explicit_abort.Abort(); explicit_abort.Abort();
  Check(!explicit_abort.Publish(Advance()), "aborted lease cannot become healthy later");
  unwind.Stop(); w::Recorder<3>::Snapshot aborted{};
  Check(unwind.TrySnapshot(aborted) && aborted.size == 2, "unwind and explicit abort retained");
  Check(aborted.counts.aborted_entries == 2 && aborted.counts.published_entries == 0 &&
        aborted.counts.diagnostic_entries == 2, "aborted not silently healthy");
  for (size_t i = 0; i < aborted.size; ++i)
    Check(aborted.rows[i].disposition == w::Disposition::Aborted &&
          Has(aborted.rows[i].issues, w::kMetadataUnavailable), "aborted metadata availability explicit");
}

void InvalidMetadata() {
  w::Recorder<16> core; Check(core.Arm(run), "invalid metadata arm");
  auto emit = [&](w::Metadata value) { auto lease = core.Begin(Bound(7)); Check(lease.Publish(value), "diagnostic metadata still published"); };
  auto value = Advance(); value.presence &= ~w::kHostSample; emit(value);
  value = Advance(); value.ratio_denominator = 0; emit(value);
  value = Advance(); value.presence &= ~w::kLockedState; emit(value);
  value = NoScaling(); value.pre_last_host = 9; emit(value);
  value = Cached(); value.post_last_guest += 1; emit(value);
  value = Advance(); value.returned_guest += 1; emit(value);
  value = Advance(); value.post_last_host += 1; emit(value);
  value = Advance(); value.branch = static_cast<w::Branch>(99); emit(value);
  value = Advance(); value.presence |= 1u << 30; emit(value);
  auto partial = Bound(); partial.phase = {1, 0, 9, 1};
  Check(core.Begin(partial).Publish(Advance()), "partial phase identity retained as diagnostic");
  // Backwards last-host replacement is a real source possibility; do not repair
  // or reject it by assuming monotonic commit order in this recorder.
  value = Advance(); value.pre_last_host = 200; emit(value);
  core.Stop(); w::Recorder<16>::Snapshot out{}; Check(core.TrySnapshot(out), "invalid metadata closed");
  const w::Issue expected[] = {w::kMissingHostSample, w::kInvalidRatio, w::kMissingLockedState,
      w::kUnexpectedLockedState, w::kCachedStateChanged, w::kReturnedGuestMismatch,
      w::kAdvanceHostMismatch, w::kInvalidBranch, w::kUnknownPresence, w::kInvalidPhase};
  for (size_t i = 0; i < 10; ++i) Check(Has(out.rows[i].issues, expected[i]), "shape contradiction explicit");
  Check(out.counts.published_entries == 11 && out.counts.diagnostic_entries == 10,
        "published diagnostics not erased or promoted");
  Check(out.rows[10].issues == 0 && out.rows[10].metadata.pre_last_host == 200 &&
        out.rows[10].metadata.post_last_host == 100, "backward actual host state retained, no invented causality");
}

void CapacityAndSaturation() {
  w::Recorder<2, 3> core; Check(core.Arm(run), "small counter arm");
  Check(core.Begin(Bound(1)).Publish(Advance()) && core.Begin(Bound(2)).Publish(Cached()), "bounded slots fill");
  Check(core.Begin(Bound(3)).status() == w::StartStatus::Overflow, "overflow explicit");
  for (unsigned i = 0; i < 8; ++i)
    Check(core.Begin(Bound(10 + i)).status() == w::StartStatus::CounterSaturated, "saturated ordinal never reused");
  core.Stop(); w::Recorder<2, 3>::Snapshot out{};
  Check(core.TrySnapshot(out) && out.size == 2, "overflow/saturation preserve old rows");
  Check(out.counts.observed_bound_reads == 3 && out.counts.overflow_entries == 1 &&
        out.counts.saturated_rejections == 3 && out.counts.counter_saturated == 1,
        "saturating counts explicitly lower bounds after cap");
  Check(out.rows[0].admission_ordinal == 1 && out.rows[1].admission_ordinal == 2,
        "stored admission ordinals remain unique");
}

void ConcurrentPublication() {
  constexpr unsigned threads = 8, per_thread = 64;
  w::Recorder<threads * per_thread> core; Check(core.Arm(run), "concurrent arm");
  std::atomic<unsigned> errors{};
  std::latch start(1);
  std::vector<std::thread> workers;
  for (unsigned t = 0; t < threads; ++t) workers.emplace_back([&, t] {
    start.wait();
    for (unsigned i = 0; i < per_thread; ++i) {
      auto lease = core.Begin(Bound(1 + t * per_thread + i, i % 3));
      if (!lease.accepted()) { ++errors; continue; }
      if (i % 7 == 0) lease.Abort();
      else if (!lease.Publish(Advance(1000 + t * per_thread + i, 10000 + i))) ++errors;
    }
  });
  start.count_down(); for (auto& thread : workers) thread.join();
  core.Stop(); w::Recorder<threads * per_thread>::Snapshot out{};
  Check(core.TrySnapshot(out) && out.size == threads * per_thread && errors == 0, "all concurrent slots drained");
  Check(out.counts.aborted_entries == 80 && out.counts.published_entries == 432 &&
        out.counts.diagnostic_entries == 80 && out.counts.overflow_entries == 0,
        "concurrent abort/publication accounting");
  bool seen[threads * per_thread]{};
  for (size_t i = 0; i < out.size; ++i) {
    const auto& record = out.rows[i];
    Check(record.admission_ordinal == i + 1 && record.binding.run.high == run.high &&
          record.binding.run.low == run.low, "concurrent admission identity remains attached");
    const auto invocation = record.binding.timer_invocation;
    Check(invocation > 0 && invocation <= threads * per_thread && !seen[invocation - 1], "no concurrent row collision");
    if (invocation > 0 && invocation <= threads * per_thread) seen[invocation - 1] = true;
  }
}

void ConcurrentStopAndArm() {
  w::Recorder<8> core; Check(core.Arm(run), "stop race arm");
  std::latch entered(8), release(1);
  std::atomic<unsigned> errors{};
  std::vector<std::thread> workers;
  for (unsigned i = 0; i < 8; ++i) workers.emplace_back([&, i] {
    auto lease = core.Begin(Bound(i + 1));
    if (!lease.accepted()) ++errors;
    entered.count_down(); release.wait();
    if (!lease.Publish(Cached())) ++errors;
  });
  entered.wait(); core.Stop(); w::Recorder<8>::Snapshot out{};
  Check(!core.TrySnapshot(out), "stop cannot expose rows owned by active producers");
  Check(core.Begin(Bound(99)).status() == w::StartStatus::Stopped, "late admission blocked atomically");
  release.count_down(); for (auto& thread : workers) thread.join();
  Check(core.TrySnapshot(out) && out.size == 8 && errors == 0, "active producers drain after stop");
  for (unsigned iteration = 0; iteration < 64; ++iteration) {
    w::Recorder<2> racing;
    std::latch start(1);
    std::thread armer([&] { start.wait(); racing.Arm(run); });
    std::thread stopper([&] { start.wait(); racing.Stop(); });
    start.count_down(); armer.join(); stopper.join();
    w::Recorder<2>::Snapshot frozen{};
    Check(racing.TrySnapshot(frozen) && frozen.size == 0 && !racing.Arm(run), "Arm/Stop cannot resurrect or race drain");
  }
}

void FrozenSnapshotAndAdmissionRace() {
  {
    w::Recorder<4> core; Check(core.Arm(run), "repeat snapshot arm");
    Check(core.Begin(Bound(1)).Publish(Advance()), "repeat snapshot publish");
    { auto aborted = core.Begin(Bound(2)); }
    core.Stop(); w::Recorder<4>::Snapshot first{}, second{};
    Check(core.TrySnapshot(first), "first frozen snapshot");
    for (unsigned repeat = 0; repeat < 32; ++repeat) {
      Check(core.Begin(Bound(90 + repeat)).status() == w::StartStatus::Stopped,
            "late calls do not mutate frozen counters");
      core.Stop();
      Check(core.TrySnapshot(second) && SameSnapshot<4>(first, second), "every closed field remains stable");
      if constexpr (std::has_unique_object_representations_v<w::Record>)
        Check(std::memcmp(first.rows.data(), second.rows.data(), sizeof(first.rows)) == 0,
              "unique-representation row bytes remain stable");
    }
  }
  for (unsigned iteration = 0; iteration < 64; ++iteration) {
    w::Recorder<129> core; Check(core.Arm(run), "admission race arm");
    auto anchor = core.Begin(Bound(1));
    std::latch start(1);
    std::atomic<unsigned> errors{};
    std::atomic<bool> stop_done{};
    auto producer = [&](unsigned base) {
      start.wait();
      for (unsigned i = 0; i < 64; ++i) {
        auto lease = core.Begin(Bound(base + i, i));
        if (lease.accepted()) {
          if (!lease.Publish(Advance(100 + i, 500 + i))) ++errors;
        } else if (lease.status() != w::StartStatus::Stopped) ++errors;
      }
    };
    std::thread one(producer, 2), two(producer, 66);
    std::thread stopper([&] { start.wait(); core.Stop(); stop_done.store(true, std::memory_order_release); });
    start.count_down();
    w::Recorder<129>::Snapshot observed{}, final{};
    while (!stop_done.load(std::memory_order_acquire)) {
      if (core.TrySnapshot(observed)) ++errors;  // Anchor is still active.
      std::this_thread::yield();
    }
    Check(!core.TrySnapshot(observed), "stop retains active anchor during competing Begin calls");
    Check(anchor.Publish(Cached()), "anchor completes after atomic stop");
    bool acquired = false;
    for (unsigned attempt = 0; attempt < 4096 && !acquired; ++attempt) {
      acquired = core.TrySnapshot(observed);
      if (!acquired) std::this_thread::yield();
    }
    one.join(); two.join(); stopper.join();
    if (!acquired) acquired = core.TrySnapshot(observed);
    Check(acquired && core.TrySnapshot(final) && errors == 0, "racing admissions drain without data race or lost ownership");
    Check(SameSnapshot<129>(observed, final), "successful snapshot stays frozen while late Begin calls finish");
    Check(final.size >= 1 && final.size <= 129 && final.counts.published_entries == final.size &&
          final.counts.observed_bound_reads == final.size, "stopped-race accounting covers exactly admitted reads");
    for (size_t i = 0; i < final.size; ++i)
      Check(final.rows[i].disposition == w::Disposition::Published &&
            final.rows[i].admission_ordinal == i + 1, "no in-flight cell copied during Begin/Stop race");
  }
}
}  // namespace

int main() {
  PodAndDisabled(); LifecycleAndBranches(); BoundTokensAndMoves(); AbortAndDrain();
  InvalidMetadata(); CapacityAndSaturation(); ConcurrentPublication(); ConcurrentStopAndArm();
  FrozenSnapshotAndAdmissionRace();
  std::cout << "checks=" << checks << " failures=" << failures << '\n';
  return failures ? 1 : 0;
}
