// Pure additive seam tests. No SDK, clock, OS input or game. Primary owns all
// compilation/execution. No artificial OOM or unsafe mutex corruption is used:
// reserve() exception behavior shares the zero-admission path but is not forced.
// Counter MAX is tested through the constexpr normalizer, not 2^64 iterations.
#include "../src/mcla_timer_observer_core.h"
#include <array>
#include <iostream>
#include <latch>
#include <limits>
#include <memory>
#include <thread>
#include <utility>

namespace {
namespace observer = mcla::timer_observer;
namespace journal = mcla::timer_journal;
struct Context { uint32_t value{}; };
struct Memory { uint32_t value{}; };
struct Signal { unsigned code; };
using Ops = observer::Operations<Context,Memory>;
constexpr journal::Window kWindow{100,200,1000};
constexpr journal::Identity kIdentity{0x1000,0x822C21B4,31};
constexpr uint64_t kMax = std::numeric_limits<uint64_t>::max();
static_assert(observer::NormalizeInvocationOrdinal(0) == 0);
static_assert(observer::NormalizeInvocationOrdinal(1) == 1);
static_assert(observer::NormalizeInvocationOrdinal(kMax-1) == kMax-1);
static_assert(observer::NormalizeInvocationOrdinal(kMax) == 0);
unsigned checks{}, failures{};
void Check(bool ok, const char* label) {
  ++checks; if (!ok) { ++failures; std::cerr << "FAIL " << label << '\n'; }
}
void Ordinal(uint64_t forwarded, uint64_t raw) {
  Check(forwarded == raw, "forwarded timer ordinal does not match raw entry ordinal");
}
Ops Operations(std::atomic<uint64_t>& tick) {
  return {[&] { return tick.fetch_add(1); },
      [](const Context& ctx, const Memory& mem, journal::State& output) {
        output.words[0] = ctx.value; output.words[1] = mem.value; return true;
      }};
}
template<size_t Capacity> auto Freeze(observer::Observer<Capacity>& core) {
  auto output = std::make_unique<typename observer::Observer<Capacity>::Snapshot>();
  if (!core.try_freeze_into(*output)) output.reset();
  Check(bool(output), "bounded completed observer drains"); return output;
}

void ReturnTypesAndLegacy() {
  observer::Observer<5> core(kWindow,true);
  Context context; Memory memory; std::atomic<uint64_t> tick{110};
  const auto ops = Operations(tick);
  std::array<uint64_t,3> forwarded{}; unsigned calls{};
  auto reference = [&](Context& actual, Memory& mem, uint64_t ordinal) -> Context& {
    Check(&actual == &context && &mem == &memory, "same original context/memory reference objects");
    forwarded[0] = ordinal; ++calls; ++actual.value; ++mem.value; return actual;
  };
  Context& result = core.invoke_with_ordinal(kIdentity,context,memory,ops,0xA5,0xFEDCBA9876543210ull,reference);
  Check(&result == &context, "decltype(auto) preserves original reference result");
  core.invoke_with_ordinal(kIdentity,context,memory,ops,0,0,
      [&](Context&, Memory&, uint64_t ordinal) { forwarded[1] = ordinal; ++calls; });
  struct MoveOnly {
    std::unique_ptr<unsigned> value;
    uint64_t& forwarded;
    unsigned& calls;
    MoveOnly(std::unique_ptr<unsigned> input, uint64_t& ordinal, unsigned& count)
        : value(std::move(input)), forwarded(ordinal), calls(count) {}
    MoveOnly(const MoveOnly&) = delete;
    std::unique_ptr<unsigned> operator()(Context&, Memory&, uint64_t ordinal) && {
      forwarded = ordinal; ++calls; return std::move(value);
    }
  };
  const auto moved = core.invoke_with_ordinal(kIdentity,context,memory,ops,0,0,
      MoveOnly(std::make_unique<unsigned>(77),forwarded[2],calls));
  Check(calls == 3 && moved && *moved == 77, "void and move-only callable/result preserve original once");
  auto frozen = Freeze(core);
  if (frozen && frozen->records().size() == 3) {
    for (size_t i = 0; i < 3; ++i) Ordinal(forwarded[i],frozen->records()[i].entry_ordinal);
    const auto& row = frozen->records()[0];
    Check(row.observation.incoming_r5_low_byte == 0xA5 &&
        row.observation.incoming_f1_bits == 0xFEDCBA9876543210ull &&
        row.observation.entry_qpc == 110 && row.observation.exit_qpc == 111 &&
        row.observation.before.words[0] == 0 && row.observation.after.words[0] == 1,
        "existing callbacks incoming bits and snapshots unchanged");
  } else Check(false, "all ordinary admitted rows retained");

  observer::Observer<1> legacy(kWindow,true);
  const int legacy_value = legacy.invoke(kIdentity,context,memory,ops,0,0,
      [](Context&, Memory&) { return 9; });
  auto old = Freeze(legacy);
  Check(legacy_value == 9 && old && old->records().size() == 1,
      "legacy two-argument original API remains available unchanged");
}

void DisabledOverflowAndStop() {
  Context context; Memory memory; std::atomic<uint64_t> tick{110}; const auto ops = Operations(tick);
  observer::Observer<1> disabled(kWindow);
  uint64_t disabled_ordinal = 99; unsigned calls{};
  const auto value = disabled.invoke_with_ordinal(kIdentity,context,memory,ops,0,0,
      [&](Context&, Memory&, uint64_t ordinal) { disabled_ordinal = ordinal; ++calls; return 7; });
  auto empty = Freeze(disabled);
  Check(value == 7 && calls == 1 && disabled_ordinal == 0 && tick == 110 && empty && empty->records().empty(),
      "disabled original once with zero ordinal and no callbacks");

  observer::Observer<1> core(kWindow,true);
  uint64_t outer = 0, nested = 99, full = 99, stopped = 99;
  core.invoke_with_ordinal(kIdentity,context,memory,ops,0,0,
      [&](Context& ctx, Memory& mem, uint64_t ordinal) {
        outer = ordinal; ++calls;
        core.invoke_with_ordinal(kIdentity,ctx,mem,ops,0,0,
            [&](Context& actual, Memory& bytes, uint64_t child) {
              nested = child; ++calls; ++actual.value; ++bytes.value;
            });
      });
  core.invoke_with_ordinal(kIdentity,context,memory,ops,0,0,
      [&](Context&, Memory&, uint64_t ordinal) { full = ordinal; ++calls; });
  core.stop_admissions();
  core.invoke_with_ordinal(kIdentity,context,memory,ops,0,0,
      [&](Context&, Memory&, uint64_t ordinal) { stopped = ordinal; ++calls; });
  auto frozen = Freeze(core);
  Check(calls == 5 && nested == 0 && full == 0 && stopped == 0 && context.value == 1 && memory.value == 1,
      "nested/full/stopped unadmitted originals each run once with zero ordinal");
  Check(frozen && frozen->records().size() == 1 && frozen->counts().observed_entries == 3 &&
      frozen->counts().overflow_entries == 2 && frozen->counts().completed_entries == 1 && tick == 112,
      "overflow counts and existing callback admission policy unchanged");
  if (frozen && !frozen->records().empty()) Ordinal(outer,frozen->records()[0].entry_ordinal);
}

void NestedAndReordered() {
  observer::Observer<4> nested(kWindow,true);
  Context context; Memory memory; std::atomic<uint64_t> tick{110}; const auto ops = Operations(tick);
  uint64_t outer{}, inner{}; unsigned calls{};
  nested.invoke_with_ordinal(kIdentity,context,memory,ops,0,0,
      [&](Context& ctx, Memory& mem, uint64_t ordinal) {
        outer = ordinal; ++calls;
        nested.invoke_with_ordinal({0x2000,0x822C21B8,31},ctx,mem,ops,0,0,
            [&](Context&, Memory&, uint64_t child) { inner = child; ++calls; });
      });
  auto frozen = Freeze(nested);
  Check(calls == 2 && frozen && frozen->records().size() == 2,
      "nested admitted originals run without observer lock across work");
  if (frozen && frozen->records().size() == 2) {
    Check(frozen->records()[0].entry_ordinal == 2 && frozen->records()[1].entry_ordinal == 1,
        "raw completion order is retained rather than sorted");
    Ordinal(inner,frozen->records()[0].entry_ordinal); Ordinal(outer,frozen->records()[1].entry_ordinal);
  }

  observer::Observer<4> concurrent(kWindow,true);
  Context first, second; Memory first_memory, second_memory;
  std::atomic<uint64_t> parallel_tick{130}; const auto parallel_ops = Operations(parallel_tick);
  std::latch entered(1), release(1);
  std::array<uint64_t,2> forwarded{};
  std::thread one([&] {
    concurrent.invoke_with_ordinal(kIdentity,first,first_memory,parallel_ops,0,0,
        [&](Context&, Memory&, uint64_t ordinal) { forwarded[0] = ordinal; entered.count_down(); release.wait(); });
  });
  entered.wait();
  std::thread two([&] {
    concurrent.invoke_with_ordinal({0x2000,0x822C21B8,32},second,second_memory,parallel_ops,0,0,
        [&](Context&, Memory&, uint64_t ordinal) { forwarded[1] = ordinal; });
  });
  two.join(); release.count_down(); one.join();
  auto reordered = Freeze(concurrent);
  Check(reordered && reordered->records().size() == 2 && reordered->counts().completed_entries == 2,
      "joined reordered originals fully drain");
  if (reordered && reordered->records().size() == 2) {
    Check(reordered->records()[0].observation.identity.thread_identity == 32 &&
        reordered->records()[1].observation.identity.thread_identity == 31, "controlled reverse completion order");
    Ordinal(forwarded[1],reordered->records()[0].entry_ordinal);
    Ordinal(forwarded[0],reordered->records()[1].entry_ordinal);
  }
}

void CallbackDiagnostics() {
  observer::Observer<2> core(kWindow,true); Context context; Memory memory;
  std::array<uint64_t,2> forwarded{}; unsigned originals{};
  for (unsigned which = 0; which < 2; ++which) {
    unsigned clock_calls{}, state_calls{}, binding_calls{};
    const unsigned failed_call = which ? 2 : 1;
    Ops ops{[&]() -> uint64_t {
        if (++clock_calls == failed_call) throw Signal{1}; return 110 + clock_calls;
      }, [&](const Context&, const Memory&, journal::State& output) {
        output.words[0] = 99;
        if (++state_calls == failed_call) throw Signal{2}; return true;
      }, [&](const Context&, const Memory&) -> observer::ScopeBinding {
        if (++binding_calls == failed_call) throw Signal{3}; return {7,9,31};
      }};
    core.invoke_with_ordinal(kIdentity,context,memory,ops,0,0,
        [&](Context&, Memory&, uint64_t ordinal) { forwarded[which] = ordinal; ++originals; });
  }
  auto frozen = Freeze(core);
  Check(originals == 2 && frozen && frozen->records().size() == 2,
      "entry/exit instrumentation failures never skip original");
  if (frozen && frozen->records().size() == 2) {
    const auto& before = frozen->records()[0]; const auto& after = frozen->records()[1];
    Ordinal(forwarded[0],before.entry_ordinal); Ordinal(forwarded[1],after.entry_ordinal);
    Check(before.callback_issues == (observer::kEntryClockCallbackFailed | observer::kBeforeStateCallbackFailed |
        observer::kEntryBindingCallbackFailed) && !before.observation.before_available &&
        before.observation.before.words[0] == 0, "entry diagnostics retain original raw failure policy");
    Check(after.callback_issues == (observer::kExitClockCallbackFailed | observer::kAfterStateCallbackFailed |
        observer::kExitBindingCallbackFailed) && !after.observation.after_available &&
        after.observation.after.words[0] == 0, "exit diagnostics retain original raw failure policy");
  }
}

void UnwindAndPendingDrain() {
  observer::Observer<2> core(kWindow,true); Context context; Memory memory;
  std::atomic<uint64_t> tick{110}; const auto ops = Operations(tick);
  uint64_t forwarded{}; unsigned originals{}; bool propagated{};
  try {
    core.invoke_with_ordinal(kIdentity,context,memory,ops,0,0,
        [&](Context& ctx, Memory&, uint64_t ordinal) -> void {
          forwarded = ordinal; ++originals; ++ctx.value; throw Signal{43};
        });
  } catch (const Signal& signal) { propagated = signal.code == 43; }
  auto thrown = Freeze(core);
  Check(propagated && originals == 1 && context.value == 1 && thrown &&
      thrown->counts().completed_entries == 1 && thrown->records().size() == 1 &&
      (thrown->records()[0].callback_issues & observer::kOriginalUnwound),
      "original exception/side effect unchanged and RAII completion drains");
  if (thrown && !thrown->records().empty()) Ordinal(forwarded,thrown->records()[0].entry_ordinal);

  observer::Observer<2> pending(kWindow,true);
  auto output = std::make_unique<observer::Observer<2>::Snapshot>();
  uint64_t admitted{}, post_stop = 99; bool blocked{}, unchanged{};
  pending.invoke_with_ordinal(kIdentity,context,memory,ops,0,0,
      [&](Context& ctx, Memory& mem, uint64_t ordinal) {
        admitted = ordinal;
        blocked = !pending.try_freeze_into(*output);
        unchanged = output->records().empty() && output->counts().observed_entries == 0;
        pending.invoke_with_ordinal(kIdentity,ctx,mem,ops,0,0,
            [&](Context&, Memory&, uint64_t value) { post_stop = value; });
      });
  Check(blocked && unchanged && post_stop == 0, "pending freeze disarms admissions and leaves destination unchanged");
  Check(pending.try_freeze_into(*output) && output->counts().observed_entries == 1 &&
      output->counts().completed_entries == 1 && output->records().size() == 1,
      "stop/drain waits for admitted original completion only");
  if (!output->records().empty()) Ordinal(admitted,output->records()[0].entry_ordinal);
  const auto retained = output->records().size();
  pending.invoke_with_ordinal(kIdentity,context,memory,ops,0,0,
      [&](Context&, Memory&, uint64_t ordinal) { post_stop = ordinal; });
  Check(post_stop == 0 && pending.try_freeze_into(*output) && output->records().size() == retained &&
      output->counts().observed_entries == 1, "post-drain unadmitted original does not mutate raw evidence");
}
}  // namespace

int main() {
  try {
    ReturnTypesAndLegacy(); DisabledOverflowAndStop(); NestedAndReordered();
    CallbackDiagnostics(); UnwindAndPendingDrain();
  } catch (...) { Check(false, "unexpected synthetic ordinal test exception"); }
  Check(observer::NormalizeInvocationOrdinal(0) == 0 && observer::NormalizeInvocationOrdinal(kMax) == 0 &&
      observer::NormalizeInvocationOrdinal(kMax-1) == kMax-1, "zero and saturated entry ordinals never fabricate unique identity");
  Check(!journal::kNativeAdmitted && !journal::kSimulationCorrectnessProven && !journal::kFpsProven,
      "pure ordinal seam only, no native integration simulation or FPS proof");
  std::cout << "checks=" << checks << " failures=" << failures
      << " synthetic_timer_ordinal_only reservation_exception_not_forced counterMAX_constexpr_only\n";
  return failures ? 1 : 0;
}
