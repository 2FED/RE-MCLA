// Standalone C++23 stdlib-only injected observer/lifecycle tests. No SDK/game
// hook, memory API, process, file or real QPC call. Threads only exercise the
// core's publication/freeze synchronization; primary owns compilation/execution.
#include "../src/mcla_timer_observer_core.h"

#include <iostream>
#include <latch>
#include <stdexcept>
#include <string_view>
#include <thread>

namespace {
namespace observer = mcla::timer_observer;
namespace journal = mcla::timer_journal;
unsigned checks = 0, failures = 0;
void Check(bool value, const char* label) {
  ++checks;
  if (!value) { ++failures; std::cerr << "FAIL " << label << '\n'; }
}
struct Context { uint32_t value = 0; };
struct Memory { uint32_t value = 0; };
using Ops = observer::Operations<Context, Memory>;
constexpr journal::Window window{100, 200, 1000};
constexpr journal::Identity identity{0x1000, 0x82130004, 1};

Ops Operations(std::atomic<uint64_t>& tick) {
  return {[&] { return tick.fetch_add(1); },
          [](const Context& context, const Memory& memory, journal::State& state) {
            state.words[0] = context.value; state.words[1] = memory.value;
            state.qwords[0] = 0xFEDCBA9876543210ull;
            return true;
          }};
}

template<size_t Capacity>
auto Freeze(observer::Observer<Capacity>& core) {
  auto snapshot = std::make_unique<typename observer::Observer<Capacity>::Snapshot>();
  if (!core.try_freeze_into(*snapshot)) snapshot.reset();
  return snapshot;
}

void OriginalAndDisabledTests() {
  observer::Observer<4> core(window, true);
  std::atomic<uint64_t> tick{110}; const auto ops = Operations(tick);
  Context context; Memory memory; unsigned calls = 0;
  auto original = [&](Context& actual_context, Memory& actual_memory) -> Context& {
    Check(&actual_context == &context && &actual_memory == &memory, "same original reference objects");
    ++calls; actual_context.value += 3; actual_memory.value += 5;
    return actual_context;
  };
  Context& result = core.invoke(identity, context, memory, ops, 0xA5, 0x0123456789ABCDEFull, original);
  Check(&result == &context && calls == 1 && context.value == 3 && memory.value == 5,
        "original once, return reference and memory effects unchanged");
  auto frozen = Freeze(core);
  Check(bool(frozen), "drained freeze returns snapshot");
  if (frozen) {
    const auto& row = frozen->records()[0].observation;
    Check(row.entry_qpc == 110 && row.exit_qpc == 111, "exact injected entry exit QPC");
    Check(row.before.words[0] == 0 && row.after.words[0] == 3 && row.after.words[1] == 5,
          "before after state effect observed");
    Check(row.incoming_r5_low_byte == 0xA5 && row.incoming_f1_bits == 0x0123456789ABCDEFull,
          "incoming bits preserved");
    const auto report = frozen->classify_offline();
    Check(report->raw_callback_issues == 0 && !(report->issues() & observer::kRecordedInvocationDiagnostic),
          "healthy invocation has no diagnostic callback aggregate");
  }
  core.invoke(identity, context, memory, ops, 0, 0, original);
  Check(calls == 2 && tick == 112 && context.value == 6, "post-freeze original still runs without instrumentation");
  auto second = Freeze(core);
  Check(second && second->counts().observed_entries == 1 && second->records().size() == 1,
        "snapshot immutable despite later unobserved original calls");
  observer::Observer<2> disabled(window);
  disabled.invoke(identity, context, memory, ops, 0, 0, original);
  Check(calls == 3 && tick == 112, "default disabled original once, callbacks absent");
  auto empty = Freeze(disabled);
  Check(empty && empty->records().empty(), "disabled empty snapshot");
}

void FailureTests() {
  observer::Observer<4> core(window, true);
  Context context; Memory memory; unsigned originals = 0;
  Ops throwing{[]() -> uint64_t { throw std::runtime_error("instrument clock"); },
      [](const Context&, const Memory&, journal::State& state) -> bool {
        state.words[0] = 42; throw std::runtime_error("instrument state");
      }};
  core.invoke(identity, context, memory, throwing, 0, 0,
              [&](Context& ctx, Memory& mem) { ++originals; ++ctx.value; ++mem.value; });
  auto frozen = Freeze(core);
  Check(originals == 1 && context.value == 1 && memory.value == 1, "instrumentation failures do not skip original");
  if (frozen) {
    const auto& row = frozen->records()[0];
    Check((row.callback_issues & observer::kEntryClockCallbackFailed) &&
          (row.callback_issues & observer::kExitClockCallbackFailed) &&
          (row.callback_issues & observer::kBeforeStateCallbackFailed) &&
          (row.callback_issues & observer::kAfterStateCallbackFailed), "callback failures explicitly recorded");
    Check(!row.observation.before_available && !row.observation.after_available &&
          row.observation.before.words[0] == 0 && row.observation.after.words[0] == 0,
          "partial failed states cleared");
    const auto classified = frozen->classify_offline();
    Check((classified->issues() & journal::kInvalidClock) && (classified->issues() & journal::kMissingState),
          "raw instrumentation failures classified only offline");
    Check(classified->raw_callback_issues == 15 &&
          (classified->issues() & observer::kRecordedInvocationDiagnostic), "all callback failure bits aggregated explicitly");
  } else Check(false, "instrumentation-failure freeze drained");

  observer::Observer<2> unwinding(window, true);
  std::atomic<uint64_t> tick{120}; const auto ops = Operations(tick);
  bool propagated = false;
  try {
    unwinding.invoke(identity, context, memory, ops, 0, 0, [&](Context& ctx, Memory& mem) {
      ++originals; ++ctx.value; ++mem.value; throw std::runtime_error("original error");
    });
  } catch (const std::runtime_error& error) { propagated = std::string_view(error.what()) == "original error"; }
  auto drained = Freeze(unwinding);
  Check(propagated && originals == 2, "original exception propagates, once");
  Check(drained && drained->counts().completed_entries == 1 &&
        (drained->records()[0].callback_issues & observer::kOriginalUnwound), "RAII drains original unwind");
  if (drained) {
    const auto report = drained->classify_offline();
    Check(report->retained_calls->issues() == 0 && report->raw_callback_issues == observer::kOriginalUnwound &&
          (report->issues() & observer::kRecordedInvocationDiagnostic),
          "original unwind cannot yield a zero-issues offline report");
  }

  observer::Observer<2> unavailable(journal::Window{}, true);
  Ops absent{[] { return uint64_t(0); }, [](const Context&, const Memory&, journal::State& state) {
    state.words.fill(99); return false;
  }};
  unavailable.invoke(identity, context, memory, absent, 0, 0,
                     [&](Context&, Memory&) { ++originals; });
  auto invalid = Freeze(unavailable);
  Check(originals == 3 && invalid, "invalid window/zero clock do not skip original");
  if (invalid) {
    const auto classified = invalid->classify_offline();
    Check((classified->issues() & journal::kInvalidWindow) && (classified->issues() & journal::kInvalidClock),
          "invalid supplied window and clock preserved");
    Check(invalid->records()[0].observation.before.words[0] == 0, "false reader result clears partial state");
  }
  observer::Observer<2> both(window, true);
  propagated = false;
  try {
    both.invoke(identity, context, memory, throwing, 0, 0, [&](Context&, Memory&) {
      ++originals; throw std::runtime_error("original error");
    });
  } catch (const std::runtime_error& error) { propagated = std::string_view(error.what()) == "original error"; }
  auto simultaneous = Freeze(both);
  Check(propagated && originals == 4 && simultaneous &&
        (simultaneous->records()[0].callback_issues & observer::kOriginalUnwound) &&
        (simultaneous->records()[0].callback_issues & observer::kAfterStateCallbackFailed),
        "instrumentation exception during unwind never shadows original exception");
}

void ReturnAndStopTests() {
  observer::Observer<2> core(window, true);
  std::atomic<uint64_t> tick{110}; const auto ops = Operations(tick);
  Context context; Memory memory; unsigned originals = 0;
  auto value = core.invoke(identity, context, memory, ops, 0, 0, [&](Context& ctx, Memory& mem) {
    ++originals; ++ctx.value; ++mem.value; return std::make_unique<unsigned>(77);
  });
  Check(value && *value == 77 && originals == 1, "move-only original return preserved");
  core.stop_admissions();
  core.invoke(identity, context, memory, ops, 0, 0, [&](Context&, Memory&) { ++originals; });
  auto frozen = Freeze(core);
  Check(originals == 2 && tick == 112 && frozen && frozen->records().size() == 1,
        "atomic stop alone disarms instrumentation without touching original");
}

void ReentrancyTests() {
  observer::Observer<4> core(window, true);
  std::atomic<uint64_t> tick{110}; const auto ops = Operations(tick);
  Context context; Memory memory; unsigned outer = 0, inner = 0;
  core.invoke(identity, context, memory, ops, 0, 0, [&](Context& ctx, Memory& mem) {
    ++outer; ++ctx.value;
    core.invoke(identity, ctx, mem, ops, 0, 0, [&](Context& nested_ctx, Memory& nested_mem) {
      ++inner; ++nested_ctx.value; ++nested_mem.value;
    });
    ++ctx.value;
  });
  auto frozen = Freeze(core);
  Check(outer == 1 && inner == 1 && context.value == 3 && memory.value == 1,
        "same-context reentrance without locking original");
  Check(frozen && frozen->records().size() == 2 && frozen->counts().completed_entries == 2,
        "reentrant admitted calls drained");
  if (frozen) {
    const auto classified = frozen->classify_offline();
    Check((classified->issues() & journal::kSharedAddressOverlapOrTouch) &&
          (classified->issues() & journal::kExitOrderRegressed), "nested raw witnesses preserved in entry reservation order");
  }
}

void ConcurrentFreezeTests() {
  observer::Observer<4> core(window, true);
  std::atomic<uint64_t> tick{110}; const auto ops = Operations(tick);
  Context first, second, after; Memory first_memory, second_memory, after_memory;
  std::latch entered(2), release(1);
  std::atomic<unsigned> calls{0};
  observer::Observer<4> donor(window, true);
  Context donor_context; Memory donor_memory;
  std::atomic<uint64_t> donor_tick{150}; const auto donor_ops = Operations(donor_tick);
  donor.invoke(identity, donor_context, donor_memory, donor_ops, 0, 0,
               [](Context& ctx, Memory&) { ctx.value = 7; });
  auto pending = Freeze(donor);
  auto original = [&](Context& ctx, Memory& mem) {
    ++calls; ++ctx.value; ++mem.value; entered.count_down(); release.wait();
  };
  std::thread one([&] { core.invoke(identity, first, first_memory, ops, 0, 0, original); });
  std::thread two([&] { core.invoke({0x2000, 0x82130008, 2}, second, second_memory, ops, 0, 0, original); });
  entered.wait();
  Check(pending && !core.try_freeze_into(*pending) && pending->records().size() == 1 &&
        pending->records()[0].observation.entry_qpc == 150 &&
        pending->records()[0].observation.after.words[0] == 7 &&
        pending->counts().stored_entries == 1,
        "in-flight freeze pending, leaves nonempty destination unchanged");
  core.invoke(identity, after, after_memory, ops, 0, 0, [&](Context& ctx, Memory& mem) {
    ++calls; ++ctx.value; ++mem.value;
  });
  Check(tick == 112 && calls == 3, "freeze disarms new observation, not original execution");
  release.count_down(); one.join(); two.join();
  auto frozen = Freeze(core);
  Check(frozen && frozen->counts().stored_entries == 2 && frozen->counts().completed_entries == 2,
        "concurrent admitted calls drain");
  Check(first.value == 1 && second.value == 1 && after.value == 1, "mixed-instance original effects");
  if (frozen) {
    const auto classified = frozen->classify_offline();
    Check((classified->issues() & journal::kWitnessIntervalsOverlapOrTouch) &&
          !(classified->issues() & journal::kSharedAddressOverlapOrTouch), "mixed instances overlap without merging addresses");
  }
}

void ConcurrentCapacityTests() {
  observer::Observer<2> core(window, true);
  std::atomic<uint64_t> tick{110}; const auto ops = Operations(tick);
  Context first, second, extra; Memory first_memory, second_memory, extra_memory;
  std::latch entered(2), release(1); std::atomic<unsigned> originals{0};
  auto blocking = [&](Context& ctx, Memory& mem) {
    ++originals; ++ctx.value; ++mem.value; entered.count_down(); release.wait();
  };
  std::thread one([&] { core.invoke(identity, first, first_memory, ops, 0, 0, blocking); });
  std::thread two([&] { core.invoke({0x2000, 0x82130008, 2}, second, second_memory, ops, 0, 0, blocking); });
  entered.wait();
  core.invoke({0x3000, 0x8213000C, 3}, extra, extra_memory, ops, 0, 0, [&](Context& ctx, Memory& mem) {
    ++originals; ++ctx.value; ++mem.value;
  });
  Check(tick == 112 && originals == 3 && extra.value == 1, "in-flight capacity overflow still invokes original, without callbacks");
  core.stop_admissions(); release.count_down(); one.join(); two.join();
  auto frozen = Freeze(core);
  Check(frozen && frozen->records().size() == 2 && frozen->counts().admitted_entries == 2 &&
        frozen->counts().completed_entries == 2 && frozen->counts().overflow_entries == 1,
        "concurrent reservations never exceed capacity or lose admitted calls");
  if (frozen) {
    const auto report = frozen->classify_offline();
    Check(report->issues() & journal::kStorageOverflow, "concurrent reservation overflow survives offline classification");
  }
}

void OuterWitnessTests() {
  observer::Observer<2> core(window, true);
  Context context; Memory memory; uint64_t tick = 110;
  Ops operations{[&] { return tick++; }, [&](const Context&, const Memory&, journal::State&) {
    tick += 3; return true;
  }};
  core.invoke(identity, context, memory, operations, 0, 0, [&](Context&, Memory&) { tick += 5; });
  auto frozen = Freeze(core);
  Check(frozen && frozen->records()[0].observation.entry_qpc == 110 &&
        frozen->records()[0].observation.exit_qpc == 122,
        "outer QPC bracket encloses before-state, original and after-state work");
}

void OverflowAndStraddleTests() {
  observer::Observer<1> core(window, true);
  std::atomic<uint64_t> tick{90}; auto ops = Operations(tick);
  unsigned clock_reads = 0, originals = 0;
  ops.read_qpc = [&] { ++clock_reads; return clock_reads == 1 ? uint64_t(90) : uint64_t(210); };
  Context context; Memory memory;
  auto original = [&](Context& ctx, Memory& mem) { ++originals; ++ctx.value; ++mem.value; };
  core.invoke(identity, context, memory, ops, 0, 0, original);
  core.invoke(identity, context, memory, ops, 0, 0, original);
  auto frozen = Freeze(core);
  Check(originals == 2 && context.value == 2 && memory.value == 2 && clock_reads == 2,
        "overflow original once, no extra instrumentation");
  Check(frozen && frozen->records().size() == 1 && frozen->counts().overflow_entries == 1 &&
        frozen->counts().observed_entries == 2, "overflow evidence/counts retained without overwrite");
  if (frozen) {
    const auto classified = frozen->classify_offline();
    Check((classified->issues() & journal::kStraddlesBegin) && (classified->issues() & journal::kStraddlesEnd),
          "straddle retained offline");
    Check(classified->issues() & journal::kStorageOverflow, "offline result keeps original storage overflow");
  }
  Check(!journal::kSimulationCorrectnessProven && !journal::kFpsProven && !journal::kNativeAdmitted,
        "synthetic seam provides no real hook or game acceptance");
}

void BeforeAndAfterWindowTests() {
  observer::Observer<2> core(window, true);
  std::atomic<uint64_t> tick{1}; const auto ops = Operations(tick);
  Context context; Memory memory; unsigned originals = 0;
  auto original = [&](Context&, Memory&) { ++originals; };
  for (unsigned i = 0; i < 20; ++i) core.invoke(identity, context, memory, ops, 0, 0, original);
  tick = 99;
  core.invoke(identity, context, memory, ops, 0, 0, original);  // [99,100] straddles start.
  tick = 200;
  for (unsigned i = 0; i < 20; ++i) core.invoke(identity, context, memory, ops, 0, 0, original);
  tick = 198;
  core.invoke(identity, context, memory, ops, 0, 0, original);  // Interior [198,199].
  auto frozen = Freeze(core);
  Check(originals == 42 && frozen, "preconfigured before window still calls every original");
  if (frozen) {
    const auto counts = frozen->counts();
    Check(counts.outside_before_entries == 20 && counts.outside_after_entries == 20 &&
          counts.stored_entries == 2 && counts.overflow_entries == 0, "outside counters do not exhaust retained capacity");
    Check(counts.completed_entries == counts.admitted_entries && counts.admitted_entries == 42,
          "admitted drained versus retained counts explicit");
    Check(frozen->records()[0].observation.entry_qpc == 99 && frozen->records()[0].entry_ordinal == 21,
          "entry before begin exiting inside retained as straddle");
    const auto report = frozen->classify_offline();
    Check(report->issues() & journal::kStraddlesBegin, "straddle survives outside-call filtering");
  }
}
}  // namespace

int main() {
  OriginalAndDisabledTests(); FailureTests(); ReentrancyTests(); ConcurrentFreezeTests();
  OverflowAndStraddleTests(); BeforeAndAfterWindowTests(); ReturnAndStopTests();
  ConcurrentCapacityTests(); OuterWitnessTests();
  std::cout << "checks=" << checks << " failures=" << failures << '\n';
  return failures ? 1 : 0;
}
