// Standalone C++23 stdlib-only synthetic checks. No clocks, game, files, threads,
// SDK, native runtime, callbacks or process operations are invoked.
#include "../src/mcla_timer_journal.h"

#include <iostream>
#include <limits>

namespace {
namespace timer = mcla::timer_journal;
unsigned checks = 0, failures = 0;
void Check(bool value, const char* label) {
  ++checks;
  if (!value) { ++failures; std::cerr << "FAIL " << label << '\n'; }
}
bool Has(uint32_t flags, timer::Issue issue) { return (flags & issue) != 0; }

timer::Observation Observe(uint64_t entry = 110, uint64_t exit = 120,
                           uint32_t address = 0x1000, uint64_t thread = 1) {
  timer::Observation result;
  result.identity = {address, 0x82130004, thread};
  result.entry_qpc = entry; result.exit_qpc = exit;
  result.incoming_r5_low_byte = 0xA5;
  result.incoming_f1_bits = 0x7FF8000000000042ull;
  for (size_t i = 0; i < timer::kWordOffsets.size(); ++i) {
    result.before.words[i] = uint32_t(0xA0000000u + i);
    result.after.words[i] = uint32_t(0xB0000000u + i);
  }
  result.before.bytes = {0xFF, 0x12, 0x80};
  result.after.bytes = {0xFE, 0x13, 0x81};
  result.before.qwords = {0xFEDCBA9876543210ull};
  result.after.qwords = {0x0123456789ABCDEFull};
  result.before_available = result.after_available = true;
  return result;
}

void WindowTests() {
  constexpr timer::Window window{100, 200, 1000};
  static_assert(window.valid());
  static_assert(!timer::Window{}.valid());
  static_assert(!timer::Window{100, 100, 1000}.valid());
  static_assert(!timer::Window{100, 200, 0}.valid());
  static_assert(!timer::Window{1, 14, 1}.valid());
  static_assert(timer::Window{1, 13, 1}.valid());
  Check(!timer::Window{timer::kMaxQpc, timer::kMaxQpc + 1, 1000}.valid(), "QPC range bounded");
  Check(!timer::Window{100, 200, timer::kMaxFrequency + 1}.valid(), "frequency range bounded");
  timer::Journal<8> journal(window);
  const timer::Observation inputs[] = {
      Observe(110, 120), Observe(90, 100), Observe(190, 200),
      Observe(90, 210), Observe(80, 90), Observe(200, 210), Observe(100, 100)};
  for (const auto& input : inputs) Check(journal.observe(input), "store window witness");
  const auto records = journal.records();
  Check(records[0].wholly_inside_window, "interior witness");
  Check(Has(records[1].issues, timer::kStraddlesBegin), "begin touching retained");
  Check(!records[1].wholly_inside_window, "begin straddle not interior");
  Check(Has(records[2].issues, timer::kStraddlesEnd), "end touching retained");
  Check(Has(records[3].issues, timer::kStraddlesBegin) &&
        Has(records[3].issues, timer::kStraddlesEnd), "both boundaries retained");
  Check(Has(records[4].issues, timer::kOutsideBefore), "before window retained");
  Check(Has(records[5].issues, timer::kOutsideAfter), "exclusive end outside");
  Check(records[6].wholly_inside_window && !Has(records[6].issues, timer::kInvalidClock),
        "equal ticks valid but not fabricated duration");
  timer::Journal<1> invalid(timer::Window{});
  Check(invalid.observe(Observe()), "invalid window still stores raw observations");
  Check(Has(invalid.issues(), timer::kInvalidWindow), "invalid window sticky");
  Check(!invalid.records()[0].wholly_inside_window, "invalid window cannot classify interior");
}

void ExactStateTests() {
  timer::Journal<2> journal({100, 200, 1000});
  auto input = Observe(); const auto before = input.before, after = input.after;
  Check(journal.observe(input), "opaque state stored");
  input.before.words.fill(0); input.after.bytes.fill(0); input.identity.timer_address = 0;
  const auto& output = journal.records()[0].observation;
  Check(output.before == before && output.after == after, "all opaque fields detached");
  Check(output.incoming_r5_low_byte == 0xA5 && output.incoming_f1_bits == 0x7FF8000000000042ull,
        "incoming bits preserved without interpreting NaN");
  Check(output.identity.timer_address == 0x1000 && output.identity.caller_address == 0x82130004 &&
        output.identity.thread_identity == 1, "timer caller thread preserved");
  Check(timer::kWordOffsets == std::array<uint32_t, 18>{
      8,16,20,24,28,32,36,40,44,48,52,60,84,88,92,96,100,104}, "exact audited word offsets");
  Check(timer::kByteOffsets == std::array<uint32_t, 3>{56,58,72}, "exact audited byte offsets");
  Check(timer::kQwordOffsets == std::array<uint32_t, 1>{64}, "guest timebase field preserved opaque");
  Check(!timer::kSimulationCorrectnessProven && !timer::kFpsProven && !timer::kNativeAdmitted,
        "no automatic acceptance");
}

void InvalidTests() {
  timer::Journal<8> journal({100, 200, 1000});
  const timer::Observation bad_clocks[] = {Observe(0, 120), Observe(120, 110),
      Observe(timer::kMaxQpc, timer::kMaxQpc + 1)};
  for (const auto& input : bad_clocks) {
    Check(journal.observe(input), "invalid clock retained");
    Check(Has(journal.records().back().issues, timer::kInvalidClock), "invalid clock explicit");
    Check(!journal.records().back().wholly_inside_window, "invalid clock not interior");
  }
  auto missing = Observe(); missing.identity.timer_address = 0;
  Check(journal.observe(missing) && Has(journal.records().back().issues, timer::kMissingIdentity),
        "missing address explicit");
  missing = Observe(); missing.identity.caller_address = 0;
  Check(journal.observe(missing) && Has(journal.records().back().issues, timer::kMissingIdentity),
        "missing caller explicit");
  missing = Observe(); missing.identity.thread_identity = 0;
  Check(journal.observe(missing) && Has(journal.records().back().issues, timer::kMissingIdentity),
        "missing thread explicit");
  missing = Observe(); missing.before_available = false;
  Check(journal.observe(missing) && Has(journal.records().back().issues, timer::kMissingState),
        "missing before state explicit");
  missing = Observe(); missing.after_available = false;
  Check(journal.observe(missing) && Has(journal.records().back().issues, timer::kMissingState),
        "missing after state explicit");
}

void OverlapTests() {
  timer::Journal<5> journal({100, 200, 1000});
  Check(journal.observe(Observe(110, 150, 0x1000, 1)), "first call stored");
  Check(journal.observe(Observe(120, 140, 0x2000, 2)), "nested different instance stored");
  Check(Has(journal.records()[0].issues, timer::kWitnessIntervalsOverlapOrTouch) &&
        Has(journal.records()[1].issues, timer::kWitnessIntervalsOverlapOrTouch), "overlap marks both");
  Check(!Has(journal.records()[1].issues, timer::kSharedAddressOverlapOrTouch), "instances not merged");
  Check(Has(journal.records()[1].issues, timer::kExitOrderRegressed), "completion order raw not sorted");
  Check(journal.observe(Observe(105, 130, 0x1000, 3)), "same-address other-thread call stored");
  Check(Has(journal.records()[2].issues, timer::kSharedAddressOverlapOrTouch) &&
        Has(journal.records()[0].issues, timer::kSharedAddressOverlapOrTouch), "possible shared-address overlap explicit");
  Check(Has(journal.records()[2].issues, timer::kEntryOrderRegressed), "entry order preserved");
  Check(journal.observe(Observe(150, 160, 0x3000, 4)), "touching clocks stored");
  Check(Has(journal.records()[3].issues, timer::kWitnessIntervalsOverlapOrTouch), "equal boundary uncertainty retained");
  Check(journal.observe(Observe(170, 180, 0x1000, 1)), "later reused address stored");
  Check(!Has(journal.records()[4].issues, timer::kWitnessIntervalsOverlapOrTouch),
        "same address alone not overlap or lifetime proof");
  Check(journal.observed_calls() == 5 && journal.records().size() == 5,
        "all identities retained individually; no simulated-time sum");
}

void OverflowTests() {
  timer::Journal<2> journal({100, 200, 1000});
  Check(journal.observe(Observe()), "capacity first");
  Check(journal.observe(Observe(130, 140)), "capacity second");
  Check(!journal.observe(Observe(150, 160)), "overflow rejects append");
  Check(!journal.observe(Observe(170, 180)), "overflow remains sticky");
  Check(journal.overflowed() && Has(journal.issues(), timer::kStorageOverflow), "overflow explicit");
  Check(journal.observed_calls() == 4 && journal.records().size() == 2,
        "observed versus retained counts separate");
  Check(journal.records()[0].observed_ordinal == 1 && journal.records()[1].observed_ordinal == 2 &&
        journal.records()[0].observation.entry_qpc == 110, "overflow never overwrites prior data");
}

void DifferentCallerTests() {
  timer::Journal<2> journal({100, 200, 1000});
  auto first = Observe(110, 120);
  auto second = Observe(115, 125);
  second.identity.caller_address = 0x82410008;
  Check(journal.observe(first) && journal.observe(second), "different callers retained");
  Check(journal.records()[0].observation.identity.caller_address == 0x82130004 &&
        journal.records()[1].observation.identity.caller_address == 0x82410008,
        "same address never merges caller identities");
  Check(Has(journal.records()[0].issues, timer::kSharedAddressOverlapOrTouch) &&
        Has(journal.records()[1].issues, timer::kSharedAddressOverlapOrTouch),
        "same-address overlap independent of caller identity");
}
}  // namespace

int main() {
  WindowTests(); ExactStateTests(); InvalidTests(); OverlapTests(); OverflowTests();
  DifferentCallerTests();
  std::cout << "checks=" << checks << " failures=" << failures << '\n';
  return failures ? 1 : 0;
}
