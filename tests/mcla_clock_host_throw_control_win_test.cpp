// Narrow synthetic experimental control, NOT an adapter fix or SDK/game gate.
// One shared noinline original, real LastError/MXCSR and volatile runtime FP.
// All observations live in caller-owned heap storage across exception unwind.
// No clock source, DLL, guest, input, filesystem or production hook is used.
#include "../src/mcla_clock_host_invoke.h"
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>
#include <xmmintrin.h>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <memory>

namespace {
namespace host = mcla::clock_read_witness::host;
namespace journal = mcla::clock_read_witness::host_journal;
namespace invoke = mcla::clock_read_witness::host_invoke;
constexpr bool kRuntimeAdmitted = false, kSdkProven = false, kGameProven = false;
constexpr bool kClockHistoryProven = false, kSimulationProven = false, kFpsProven = false;
constexpr journal::RunToken kRun{0x123456789ABCDEF0ull,0xFEDCBA9876543210ull};
struct Native {
  uint32_t error{}, mxcsr{};
  static Native Capture() noexcept { const auto e = GetLastError(); return {e,_mm_getcsr()}; }
  void Restore() const noexcept { _mm_setcsr(mxcsr); SetLastError(error); }
  bool operator==(const Native&) const = default;
};
struct Signal { unsigned code; };
struct Result {
  Native before_read{}, query_incoming{}, before_fp{}, after_fp{}, before_throw{}, caught_native{};
  MclaClockWitnessBindingV1 request{};
  const void *expected_context{}, *expected_base{}, *actual_context{}, *actual_base{};
  volatile float operand = 1.75f;
  uint64_t clock_result{};
  unsigned originals{}, queries{}, fallbacks{}, query_errors{}, progress{};
  unsigned propagated{}, unexpected_exception{}, signal_code{};
  int fp_result{};
};
struct Context { Result* result{}; uint32_t value{}; };
struct Run {
  Result result{};
  Context context{};
  uint8_t bytes[4]{};
  host::Trace<16> trace{};
};
constinit thread_local Result* active_result = nullptr;
unsigned checks{}, failures{};
void Check(bool condition, const char* label) {
  ++checks;
  if (!condition) { ++failures; std::cerr << "FAIL " << label << '\n'; }
}
host::Identity Identity() noexcept { return {kRun.high,kRun.low,8,0,0,0,0}; }
journal::Invocation Invocation() noexcept { return {8,0,0,31,0x827D7500,0x822C21B4,0,0}; }
uint64_t Fallback() {
  ++active_result->fallbacks;
  Native{0xABCD1000u,0x3F80u}.Restore();
  return 77;
}
uint64_t MCLA_CW_CALL Query(const MclaClockWitnessBindingV1* binding,
                          MclaClockWitnessStatusV1* status) {
  auto& r = *active_result;
  r.query_incoming = Native::Capture(); ++r.queries;
  if (!binding || !status) { ++r.query_errors; throw Signal{901}; }
  r.request = *binding;
  *status = {MCLA_CW_ABI_V1,sizeof(*status),MCLA_CW_ACCEPTED,MCLA_CW_PUBLISHED,0,0};
  Native{0xABCD1000u,0x3F80u}.Restore();
  return 77;
}

// Both paths call THIS exact body. Heap stores before throw are observable to
// the external catcher; no assignments to captured automatic scalar locals.
// The operand is a volatile runtime load, not an intrinsic constant-fold test.
__declspec(noinline) void OriginalBody(Context& context, uint8_t* base) {
  auto& r = *context.result;
  ++r.originals; r.actual_context = &context; r.actual_base = base;
  r.before_read = Native::Capture(); r.progress = 1;
  r.clock_result = host::Read<Native>(&context,base,Fallback);
  r.before_fp = Native::Capture(); r.progress = 2;
  const float operand = r.operand;
  r.fp_result = _mm_cvtss_si32(_mm_set_ss(operand));
  r.after_fp = Native::Capture(); r.progress = 3;
  ++context.value; ++base[0];
  r.before_throw = Native::Capture(); r.progress = 4;
  throw Signal{43};
}

void Prepare(Run& run) noexcept {
  run.context.result = &run.result;
  run.result.expected_context = &run.context; run.result.expected_base = run.bytes;
}
void Direct(Run& run) {
  active_result = &run.result;
  Native{0x13572468u,0x1F80u}.Restore();
  try {
    host::Scope<Native> scope(Identity(),&run.context,run.bytes,Query,run.trace,true);
    OriginalBody(run.context,run.bytes);
  } catch (const Signal& signal) {
    // Capture FIRST: comparisons, printing and snapshot work happen later.
    run.result.caught_native = Native::Capture();
    run.result.signal_code = signal.code; run.result.propagated = 1;
  } catch (...) {
    run.result.caught_native = Native::Capture(); run.result.unexpected_exception = 1;
  }
  active_result = nullptr;
}
void Adapted(Run& run, journal::Journal<1>& receipts) {
  active_result = &run.result;
  Native{0x13572468u,0x1F80u}.Restore();
  try {
    invoke::Invoke<Native>(&receipts,Identity(),Invocation(),run.context,run.bytes,Query,true,
        [](Context& context, uint8_t* base) { OriginalBody(context,base); });
  } catch (const Signal& signal) {
    run.result.caught_native = Native::Capture();
    run.result.signal_code = signal.code; run.result.propagated = 1;
  } catch (...) {
    run.result.caught_native = Native::Capture(); run.result.unexpected_exception = 1;
  }
  active_result = nullptr;
}
void PrintNative(const char* name, Native value) {
  std::cout << ' ' << name << "=0x" << std::hex << value.error << "/0x" << value.mxcsr << std::dec;
}
void Print(const char* label, const Run& run) {
  const auto& r = run.result;
  std::cout << label << " originals=" << r.originals << " queries=" << r.queries
      << " fallbacks=" << r.fallbacks << " query_errors=" << r.query_errors
      << " progress=" << r.progress << " clock=" << r.clock_result << " fp=" << r.fp_result
      << " signal=" << r.signal_code << " caught=" << r.propagated
      << " unexpected=" << r.unexpected_exception << " ctx=" << run.context.value
      << " byte=" << unsigned(run.bytes[0]) << " exact_context="
      << (r.actual_context == r.expected_context) << " exact_base=" << (r.actual_base == r.expected_base);
  PrintNative("before_read",r.before_read); PrintNative("query_incoming",r.query_incoming);
  PrintNative("before_fp",r.before_fp); PrintNative("after_fp",r.after_fp);
  PrintNative("before_throw",r.before_throw); PrintNative("catch",r.caught_native);
  std::cout << '\n';
}
void CheckRun(const Run& run) {
  const auto& r = run.result;
  Check(r.originals == 1 && r.queries == 1 && !r.fallbacks && !r.query_errors,
      "one original and one bound fake query execute before throw");
  Check(r.actual_context == r.expected_context && r.actual_base == r.expected_base,
      "shared original receives exact caller-owned context and base");
  Check(r.progress == 4 && run.context.value == 1 && run.bytes[0] == 1 && r.clock_result == 77,
      "heap observations and original mutations remain visible after throw");
  Check(r.propagated == 1 && r.signal_code == 43 && !r.unexpected_exception,
      "same typed exception propagates after shared original body");
  Check(r.before_read == Native{0x13572468u,0x1F80u} && r.query_incoming == r.before_read,
      "original and fake query receive caller native incoming state");
  Check(r.before_fp == Native{0xABCD1000u,0x3F80u},
      "actual clock-produced round-down state reaches caller FP instruction");
  Check(r.fp_result == 1 && (r.after_fp.mxcsr & 0x6000u) == 0x2000u,
      "runtime volatile1.75 conversion obeys round-down MXCSR");
  Check(r.before_throw == r.after_fp,
      "integer original mutations do not replace captured post-FP state");
  Check(r.request.timer_invocation == 8 && r.request.read_ordinal == 0,
      "control uses exact matching timer/read identity");
}
}  // namespace

int main() {
  const auto process_incoming = Native::Capture();
  // Preflight allocations and optional EH first-touch are outside both paths.
  auto direct = std::make_unique<Run>(); auto adapted = std::make_unique<Run>();
  auto receipts = std::make_unique<journal::Journal<1>>();
  auto snapshot = std::make_unique<journal::Journal<1>::Snapshot>();
  Prepare(*direct); Prepare(*adapted);
  Check(receipts->Arm(kRun),"adapter control journal arms once before observation");
  try { throw Signal{0}; } catch (const Signal&) {}

  Direct(*direct); const bool direct_tls_clear = host::detail::current == nullptr;
  Adapted(*adapted,*receipts); const bool adapted_tls_clear = host::detail::current == nullptr;
  // Both external catch states already reside in heap storage. Stop/freeze and
  // output cannot overwrite them, and catch state is NOT assumed to equal the
  // pre-throw state: any exception-runtime effect is compared directly.
  receipts->Stop(); const bool frozen = receipts->TryFreezeInto(*snapshot);
  Print("direct",*direct); Print("adapted",*adapted);
  CheckRun(*direct); CheckRun(*adapted);
  const auto& a = direct->result; const auto& b = adapted->result;
  Check(a.before_read == b.before_read && a.query_incoming == b.query_incoming &&
      a.before_fp == b.before_fp && a.after_fp == b.after_fp && a.before_throw == b.before_throw,
      "direct and adapted pre-throw native observations match exactly");
  Check(a.caught_native == b.caught_native,
      "direct and adapted external catch native states match exactly");
  Check(a.fp_result == b.fp_result && a.progress == b.progress && a.clock_result == b.clock_result &&
      a.signal_code == b.signal_code && direct->context.value == adapted->context.value &&
      direct->bytes[0] == adapted->bytes[0],"direct and adapted original results and mutations match");
  Check(direct_tls_clear && adapted_tls_clear,"both unwind paths restore host Scope TLS");
  Check(frozen && snapshot->size == 1 && snapshot->rows[0].disposition == journal::Disposition::Completed &&
      snapshot->rows[0].outcome == journal::Outcome::Unwound &&
      (snapshot->rows[0].flags & journal::kOriginalUnwound),
      "adapter journal retains explicit completed-unwind outcome");
  Check(frozen && snapshot->rows[0].trace.size == 1 && direct->trace.size == 1 &&
      std::memcmp(&snapshot->rows[0].trace,&direct->trace,sizeof(direct->trace)) == 0,
      "both controls retain identical successful pre-throw clock receipts");
  Check(!kRuntimeAdmitted && !kSdkProven && !kGameProven && !kClockHistoryProven &&
      !kSimulationProven && !kFpsProven,"synthetic control makes no runtime SDK game or speed claim");
  std::cout << "checks=" << checks << " failures=" << failures << '\n';
  process_incoming.Restore();
  return failures ? 1 : 0;
}
