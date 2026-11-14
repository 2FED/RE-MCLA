// Pure exact-entry prerequisite. Fake clocks/context/memory only: no SDK,
// platform/native API or game. Primary alone compiles/runs. Actual counter MAX
// and a reserve/mutex exception are NOT forced; the MAX normalizer is constexpr.
#include "../src/mcla_timer_observer_core.h"
#include <array>
#include <iostream>
#include <limits>
#include <memory>
#include <utility>

namespace {
namespace observer = mcla::timer_observer;
namespace journal = mcla::timer_journal;
struct Context { unsigned value{}; };
struct Memory { unsigned value{}; };
struct Signal { unsigned code; };
using Ops = observer::Operations<Context, Memory>;
constexpr journal::Window kWindow{100, 200, 1000};
constexpr journal::Identity kIdentity{0x1000, 0x822C21B4, 31};
constexpr uint64_t kMax = std::numeric_limits<uint64_t>::max();
static_assert(observer::NormalizeInvocationOrdinal(kMax) == 0);
static_assert(observer::NormalizeInvocationOrdinal(kMax - 1) == kMax - 1);
unsigned checks{}, failures{};
void Check(bool ok, const char* label) {
  ++checks; if (!ok) { ++failures; std::cerr << "FAIL " << label << '\n'; }
}
template<size_t Capacity> auto Freeze(observer::Observer<Capacity>& core) {
  auto output = std::make_unique<typename observer::Observer<Capacity>::Snapshot>();
  Check(core.try_freeze_into(*output), "all completed exact-entry invocations drain");
  return output;
}
Ops Operations(uint64_t& tick, unsigned& clocks, unsigned& states) {
  return {[&] { ++clocks; return tick++; },
      [&](const Context& context, const Memory& memory, journal::State& output) {
        ++states; output.words[0] = context.value; output.words[1] = memory.value; return true;
      }, {}};
}

void CapturedEntryAndAvailability() {
  const std::array<observer::ScopeBinding, 3> bindings{{{7, 9, 31}, {7, 0, 31}, {}}};
  for (const auto binding : bindings) {
    observer::Observer<2> core(kWindow, true); Context context; Memory memory;
    uint64_t tick = 110; unsigned clocks{}, states{}, reads{}, originals{};
    auto ops = Operations(tick, clocks, states);
    ops.read_binding = [&](const Context&, const Memory&) {
      ++reads; return reads == 1 ? binding : observer::ScopeBinding{99, 101, 37};
    };
    observer::EntryEnvelope forwarded{}; uint64_t ordinal{};
    Context& returned = core.invoke_with_entry(kIdentity, context, memory, ops, 0xA5, 0xFEDCBA9876543210ull,
        [&](Context& actual, Memory& bytes, uint64_t value, const observer::EntryEnvelope& entry) -> Context& {
          Check(&actual == &context && &bytes == &memory, "exact entry original keeps same reference objects");
          Check(clocks == 1 && states == 1 && reads == 1,
              "original receives captured entry without an extra callback or clock");
          forwarded = entry; ordinal = value; ++originals; ++actual.value; ++bytes.value; return actual;
        });
    auto snapshot = Freeze(core);
    Check(&returned == &context && originals == 1 && context.value == 1 && memory.value == 1,
        "reference result and original-once mutations survive additive entry seam");
    Check(snapshot->records().size() == 1 && clocks == 2 && states == 2 && reads == 2,
        "entry/exit callbacks each execute exactly once as before");
    if (snapshot->records().size() != 1) continue;
    const auto& row = snapshot->records()[0];
    Check(ordinal == row.entry_ordinal && ordinal == 1 && forwarded.binding == row.entry_binding &&
        forwarded.binding == binding && forwarded.binding_available && !forwarded.callback_issues,
        "forwarded entry envelope equals the same raw Completion entry, including partial and zero tuples");
    Check(row.exit_binding == observer::ScopeBinding{99, 101, 37} && forwarded.binding != row.exit_binding,
        "later different exit binding cannot overwrite forwarded entry");
    Check(row.observation.incoming_r5_low_byte == 0xA5 &&
        row.observation.incoming_f1_bits == 0xFEDCBA9876543210ull,
        "existing incoming argument capture unchanged");
  }
  observer::Observer<1> absent(kWindow, true); Context context; Memory memory;
  uint64_t tick = 110; unsigned clocks{}, states{}; const auto ops = Operations(tick, clocks, states);
  observer::EntryEnvelope envelope{};
  absent.invoke_with_entry(kIdentity, context, memory, ops, 0, 0,
      [&](Context&, Memory&, uint64_t ordinal, const observer::EntryEnvelope& entry) {
        envelope = entry; Check(ordinal == 1, "absent optional binding does not change reservation ordinal");
      });
  auto snapshot = Freeze(absent);
  Check(!envelope.binding_available && envelope.binding == observer::ScopeBinding{} &&
      !envelope.callback_issues && snapshot->records().size() == 1,
      "absent binding callback remains unavailable, not a fabricated healthy tuple");
}

void EntryAndExitFailures() {
  for (unsigned which = 0; which < 3; ++which) {
    observer::Observer<1> core(kWindow, true); Context context; Memory memory;
    unsigned clocks{}, states{}, bindings{}, originals{};
    const unsigned failed = which == 1 ? 2 : 1;
    Ops ops{[&]() -> uint64_t {
        if (++clocks == failed) throw Signal{1}; return 110 + clocks;
      }, [&](const Context&, const Memory&, journal::State& output) {
        output.words[0] = 99; if (++states == failed) throw Signal{2}; return true;
      }, [&](const Context&, const Memory&) -> observer::ScopeBinding {
        if (++bindings == failed && which != 2) throw Signal{3}; return {7, 0, 31};
      }};
    observer::EntryEnvelope entry{};
    core.invoke_with_entry(kIdentity, context, memory, ops, 0, 0,
        [&](Context&, Memory&, uint64_t ordinal, const observer::EntryEnvelope& envelope) {
          ++originals; entry = envelope; Check(ordinal == 1, "callback failures retain reserved ordinal");
        });
    auto snapshot = Freeze(core);
    Check(originals == 1 && clocks == 2 && states == 2 && bindings == 2 && snapshot->records().size() == 1,
        "throwing instrumentation never skips original or adds callback evaluations");
    if (snapshot->records().size() != 1) continue;
    const auto& row = snapshot->records()[0];
    const uint32_t entry_failures = observer::kEntryClockCallbackFailed | observer::kBeforeStateCallbackFailed;
    if (which == 0) {
      Check(!entry.binding_available && entry.binding == row.entry_binding && entry.binding == observer::ScopeBinding{} &&
          entry.callback_issues == (entry_failures | observer::kEntryBindingCallbackFailed) &&
          entry.callback_issues == row.callback_issues && !row.observation.before_available,
          "entry callback failures and unavailable zero binding forwarded verbatim");
    } else if (which == 1) {
      Check(entry.binding_available && entry.binding == observer::ScopeBinding{7, 0, 31} && !entry.callback_issues &&
          row.callback_issues == (observer::kExitClockCallbackFailed | observer::kAfterStateCallbackFailed |
                                  observer::kExitBindingCallbackFailed),
          "exit callback failures do not backfill or mutate the captured entry envelope");
    } else {
      Check(entry.binding_available && entry.binding == observer::ScopeBinding{7, 0, 31} &&
          entry.callback_issues == entry_failures && row.callback_issues == entry_failures,
          "clock and state callback failures do not erase successfully available partial binding");
    }
  }
}

void NoAdmissionAndPending() {
  Context context; Memory memory; uint64_t tick = 110; unsigned clocks{}, states{}, bindings{}, calls{};
  auto ops = Operations(tick, clocks, states);
  ops.read_binding = [&](const Context&, const Memory&) { ++bindings; return observer::ScopeBinding{7, 9, 31}; };
  auto unavailable = [&](Context&, Memory&, uint64_t ordinal, const observer::EntryEnvelope& entry) {
    ++calls;
    Check(ordinal == 0 && !entry.binding_available && entry.binding == observer::ScopeBinding{} && !entry.callback_issues,
        "every unadmitted original receives zero ordinal and unavailable empty entry envelope");
  };
  observer::Observer<1> disabled(kWindow);
  disabled.invoke_with_entry(kIdentity, context, memory, ops, 0, 0, unavailable);
  Check(clocks == 0 && states == 0 && bindings == 0, "disabled entry API calls no instrumentation");
  observer::Observer<1> core(kWindow, true);
  auto pending = std::make_unique<observer::Observer<1>::Snapshot>();
  core.invoke_with_entry(kIdentity, context, memory, ops, 0, 0,
      [&](Context& actual, Memory& bytes, uint64_t ordinal, const observer::EntryEnvelope& entry) {
        ++calls;
        Check(ordinal == 1 && entry.binding_available, "outer admitted entry remains bound");
        core.invoke_with_entry(kIdentity, actual, bytes, ops, 0, 0, unavailable);  // In-flight overflow.
        Check(!core.try_freeze_into(*pending) && pending->records().empty(),
            "pending freeze rejects active original and leaves destination unchanged");
        core.invoke_with_entry(kIdentity, actual, bytes, ops, 0, 0, unavailable);  // Stopped.
      });
  core.invoke_with_entry(kIdentity, context, memory, ops, 0, 0, unavailable);
  auto snapshot = Freeze(core);
  Check(calls == 5 && snapshot->records().size() == 1 && snapshot->counts().observed_entries == 2 &&
      snapshot->counts().overflow_entries == 1 && snapshot->counts().completed_entries == 1 &&
      clocks == 2 && states == 2 && bindings == 2,
      "overflow and post-stop paths do not fabricate callbacks or alter reservation policy");
  observer::Observer<1> full(kWindow, true);
  full.invoke_with_entry(kIdentity, context, memory, ops, 0, 0,
      [&](Context&, Memory&, uint64_t ordinal, const observer::EntryEnvelope& entry) {
        Check(ordinal == 1 && entry.binding_available, "first retained slot exposes admitted entry");
      });
  full.invoke_with_entry(kIdentity, context, memory, ops, 0, 0, unavailable);
  auto full_snapshot = Freeze(full);
  Check(full_snapshot->records().size() == 1 && full_snapshot->counts().observed_entries == 2 &&
      full_snapshot->counts().overflow_entries == 1 && clocks == 4 && states == 4 && bindings == 4,
      "already retained storage overflow also passes zero unavailable entry without callbacks");
}

void OriginalThrowAndRetainedOrder() {
  observer::Observer<2> core(kWindow, true); Context context; Memory memory;
  uint64_t tick = 90; unsigned clocks{}, states{}, bindings{}, calls{};
  auto ops = Operations(tick, clocks, states);
  ops.read_binding = [&](const Context&, const Memory&) { ++bindings; return observer::ScopeBinding{7, 9, 31}; };
  // Healthy pre-window primer is discarded, but its entry ordinal is consumed.
  core.invoke_with_entry(kIdentity, context, memory, ops, 0, 0,
      [&](Context&, Memory&, uint64_t ordinal, const observer::EntryEnvelope&) {
        ++calls; Check(ordinal == 1, "discarded primer consumes first reservation ordinal");
      });
  tick = 110; uint64_t actual{}; observer::EntryEnvelope captured{}; bool propagated{};
  try {
    core.invoke_with_entry(kIdentity, context, memory, ops, 0, 0,
        [&](Context& same, Memory& bytes, uint64_t ordinal, const observer::EntryEnvelope& entry) -> void {
          ++calls; actual = ordinal; captured = entry; ++same.value; ++bytes.value; throw Signal{43};
        });
  } catch (const Signal& signal) { propagated = signal.code == 43; }
  auto snapshot = Freeze(core);
  Check(propagated && calls == 2 && context.value == 1 && memory.value == 1 && snapshot->records().size() == 1 &&
      snapshot->counts().outside_before_entries == 1 && snapshot->counts().completed_entries == 2,
      "original exception and mutations survive, with old outside-discard policy unchanged");
  if (snapshot->records().size() == 1) {
    const auto& raw = snapshot->records()[0];
    Check(actual == 2 && raw.entry_ordinal == 2 && captured.binding == raw.entry_binding &&
        captured.binding_available && !captured.callback_issues && (raw.callback_issues & observer::kOriginalUnwound),
        "raw timer ordinal two is forwarded instead of retained storage position one");
  }
}

void LegacyApisAndMoveOnly() {
  observer::Observer<3> core(kWindow, true); Context context; Memory memory;
  uint64_t tick = 110; unsigned clocks{}, states{}; const auto ops = Operations(tick, clocks, states);
  const int old = core.invoke(kIdentity, context, memory, ops, 0, 0, [](Context&, Memory&) { return 7; });
  uint64_t forwarded{};
  const int ordinal = core.invoke_with_ordinal(kIdentity, context, memory, ops, 0, 0,
      [&](Context&, Memory&, uint64_t value) { forwarded = value; return 9; });
  auto moved = core.invoke_with_entry(kIdentity, context, memory, ops, 0, 0,
      [value = std::make_unique<unsigned>(77)](Context&, Memory&, uint64_t id,
                                             const observer::EntryEnvelope& entry) mutable {
        Check(id == 3 && !entry.binding_available, "move-only entry callable retains exact third ordinal");
        return std::move(value);
      });
  auto snapshot = Freeze(core);
  Check(old == 7 && ordinal == 9 && forwarded == 2 && moved && *moved == 77 && snapshot->records().size() == 3 &&
      clocks == 6 && states == 6, "legacy APIs and move-only entry result preserve original return semantics");
}
}  // namespace

int main() {
  try {
    CapturedEntryAndAvailability(); EntryAndExitFailures(); NoAdmissionAndPending();
    OriginalThrowAndRetainedOrder(); LegacyApisAndMoveOnly();
  } catch (...) { Check(false, "unexpected synthetic exact-entry test exception"); }
  Check(!journal::kNativeAdmitted && !journal::kSimulationCorrectnessProven && !journal::kFpsProven,
      "entry forwarding is not native integration history simulation or FPS proof");
  std::cout << "checks=" << checks << " failures=" << failures
      << " synthetic_timer_entry_only reservation_exception_not_forced counterMAX_constexpr_only\n";
  return failures ? 1 : 0;
}
