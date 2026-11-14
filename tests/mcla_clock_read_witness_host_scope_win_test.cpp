// Synthetic host Read/scope only. No real clock, SDK, DLL load, game, input,
// file writes or observer mutex. Primary owns compilation/native execution.
#include "../src/mcla_clock_read_witness_host_scope.h"
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>
#include <xmmintrin.h>
#include <array>
#include <cstring>
#include <initializer_list>
#include <iostream>
#include <thread>
#include <type_traits>

namespace {
namespace host = mcla::clock_read_witness::host;
struct Native {
  uint32_t error, mxcsr;
  static Native Capture() noexcept { const uint32_t error = GetLastError(); return {error,_mm_getcsr()}; }
  void Restore() const noexcept { SetLastError(error); _mm_setcsr(mxcsr); }
  bool operator==(const Native&) const = default;
};
struct AlternateNativePolicy {
  static Native Capture() noexcept { return Native::Capture(); }
};
static_assert(std::is_trivial_v<host::Receipt> && std::is_standard_layout_v<host::Receipt>);
static_assert(std::is_trivial_v<host::Trace<8>> && std::is_standard_layout_v<host::Trace<8>>);
struct Signal { uint32_t code; };
struct Schedule {
  uint64_t samples[32];
  MclaClockWitnessBindingV1 requests[32];
  Native incoming[32];
  const void *reentry_context, *reentry_base;
  uint64_t reentry_result;
  uint32_t count, next, queries, fallbacks, errors;
  bool throw_once, bad_status, reenter_once;
};
constinit thread_local Schedule schedule{};
unsigned checks{}, failures{};
constexpr uint64_t kHigh = 0x123456789ABCDEF0ull, kLow = 0xFEDCBA9876543210ull;
void Check(bool ok, const char* label) {
  ++checks; if (!ok) { ++failures; std::cerr << "FAIL " << label << '\n'; }
}
void Reset(std::initializer_list<uint64_t> samples) {
  schedule = {};
  schedule.count = uint32_t(samples.size());
  size_t i = 0; for (auto value : samples) { if (i < 32) schedule.samples[i++] = value; else ++schedule.errors; }
}
host::Identity Identity(uint64_t timer) noexcept { return {kHigh,kLow,timer,100+timer,7,31,1}; }
uint64_t OriginalClock() {
  const uint32_t index = schedule.next++;
  if (index >= schedule.count || index >= 32) { ++schedule.errors; throw Signal{900}; }
  schedule.incoming[index] = Native::Capture();
  Native{uint32_t(0xABCD1000u+index),index % 2 ? 0x5F80u : 0x3F80u}.Restore();
  if (schedule.throw_once) { schedule.throw_once = false; throw Signal{43}; }
  return schedule.samples[index];
}
uint64_t Fallback() { ++schedule.fallbacks; return OriginalClock(); }
uint64_t MCLA_CW_CALL Query(const MclaClockWitnessBindingV1* request, MclaClockWitnessStatusV1* status) {
  const uint32_t index = schedule.queries++;
  if (!request || !status || index >= 32) { ++schedule.errors; throw Signal{901}; }
  schedule.requests[index] = *request;
  const bool bound = (request->run_high || request->run_low) && request->timer_invocation;
  const bool invalid_phase = request->phase_present
      ? request->phase_present != 1 || !request->phase_call_id || !request->phase_thread_epoch || !request->phase_native_thread_id
      : request->phase_call_id || request->phase_thread_epoch || request->phase_native_thread_id;
  *status = {schedule.bad_status ? 99u : MCLA_CW_ABI_V1,sizeof(*status),
      bound ? MCLA_CW_ACCEPTED : MCLA_CW_UNBOUND,bound ? MCLA_CW_IN_FLIGHT : MCLA_CW_NO_EVENT,
      invalid_phase ? 1u : 0u,0};
  try {
    if (schedule.reenter_once) {
      schedule.reenter_once = false;
      schedule.reentry_result = host::Read<Native>(schedule.reentry_context,schedule.reentry_base,Fallback);
    }
    const uint64_t value = OriginalClock();
    if (bound) status->completion = MCLA_CW_PUBLISHED;
    return value;
  } catch (...) { if (bound) status->completion = MCLA_CW_ABORTED; throw; }
}
void Ordinals(const host::Trace<8>& trace) {
  for (uint32_t i = 0; i < trace.size; ++i)
    if (trace.rows[i].read_ordinal_available)
      Check(trace.rows[i].scope_binding.read_ordinal == i, "host timebase read reused a caller ordinal");
}

void NoScopeAndDefaultOff() {
  int context{}, base{}; host::Receipt receipt{};
  Reset({500}); Native{0x24681357u,0x1F80u}.Restore();
  const uint64_t plain = Fallback(); const auto expected = Native::Capture();
  Reset({500}); Native{0x24681357u,0x1F80u}.Restore();
  const uint64_t result = host::Read<Native>(&context,&base,Fallback,&receipt);
  const auto produced = Native::Capture();
  Check(result == plain && schedule.fallbacks == 1 && schedule.queries == 0 &&
      receipt.result_available && receipt.result == 500 && !receipt.used_query && !receipt.status_available &&
      (receipt.decisions & host::kNoScope) && (receipt.decisions & host::kQueryUnavailable),
      "no-scope fallback executes once and explicitly lacks witness status");
  Check(produced == expected && schedule.incoming[0] == Native{0x24681357u,0x1F80u},
      "first host TLS read preserves incoming and produced LastError/MXCSR");
  Reset({700}); host::Trace<8> trace{};
  Native{0x87654321u,0x1F80u}.Restore();
  {
    host::Scope<Native> scope(Identity(1),&context,&base,Query,trace); // Default OFF.
    Check(host::Read<Native>(&context,&base,Fallback) == 700, "default-off scope retains ordinary clock result");
  }
  const auto after_scope = Native::Capture();
  Check(schedule.queries == 1 && schedule.fallbacks == 0 && trace.size == 1 &&
      (trace.rows[0].decisions & host::kDisabled) && trace.rows[0].scope_binding.timer_invocation == 1 &&
      trace.rows[0].request.timer_invocation == 0 && trace.rows[0].status.admission == MCLA_CW_UNBOUND,
      "default-off scope sends one explicit unbound request without borrowing identity");
  Check(after_scope == Native{0xABCD1000u,0x3F80u}, "scope destructor preserves produced not stale incoming state");
}

void RetriesAndNativeState() {
  int context{}, base{}; host::Trace<8> trace{};
  Reset({0,0x100000000ull,0x100000007ull});
  uint64_t value{}; unsigned loops{};
  {
    host::Scope<Native> scope(Identity(2),&context,&base,Query,trace,true);
    do { value = host::Read<Native>(&context,&base,Fallback); ++loops; } while (uint32_t(value) == 0);
  }
  Check(value == 0x100000007ull && loops == 3 && schedule.next == 3 && schedule.queries == 3 &&
      schedule.fallbacks == 0 && trace.size == 3 && trace.observed_reads == 3 &&
      trace.rows[0].result_available && trace.rows[0].result == 0 && trace.rows[1].result == 0x100000000ull,
      "low32-zero retry loop is unchanged and every actual query including zero is retained");
  Ordinals(trace);
  for (uint32_t i = 0; i < 3; ++i) Check(trace.rows[i].request.timer_invocation == 2 &&
      trace.rows[i].request.run_high == kHigh && trace.rows[i].request.run_low == kLow &&
      trace.rows[i].request.read_ordinal == i && trace.rows[i].used_query && trace.rows[i].status_available,
      "exact run timer and per-evaluation read ordinal sent to ABI query");

  Reset({123}); Native{0x13572468u,0x1F80u}.Restore();
  const auto direct = Query(&trace.rows[0].request,&trace.rows[0].status);
  const auto direct_state = Native::Capture();
  const int direct_fp = _mm_cvtss_si32(_mm_set_ss(1.75f));
  Reset({123}); host::Trace<8> separate{}; Native{0x13572468u,0x1F80u}.Restore();
  uint64_t recorded{}; Native recorded_state{}; int recorded_fp{};
  {
    host::Scope<Native> scope(Identity(3),&context,&base,Query,separate,true);
    recorded = host::Read<Native>(&context,&base,Fallback);
    recorded_state = Native::Capture();
    recorded_fp = _mm_cvtss_si32(_mm_set_ss(1.75f));
  }
  Check(recorded == direct && direct_state == recorded_state && direct_fp == recorded_fp && recorded_fp == 1 &&
      schedule.incoming[0] == Native{0x13572468u,0x1F80u},
      "original-produced native state is visible before subsequent caller floating-point operation");
}

void NestedScopesAndGuards() {
  int context{}, other_context{}, base{}, other_base{};
  host::Trace<8> outer{}, inner{}, disabled{}, mismatch{};
  Reset({10,11,12,13,14,15,16,17,18,19,20,21});
  {
    host::Scope<Native> scope(Identity(10),&context,&base,Query,outer,true);
    host::Read<Native>(&context,&base,Fallback);
    {
      host::Scope<Native,2> child(Identity(11),&context,&base,Query,inner,true);
      host::Read<Native>(&context,&base,Fallback);
    }
    {
      host::Scope<Native> child(Identity(12),&context,&base,Query,disabled,false);
      host::Read<Native>(&context,&base,Fallback);
    }
    {
      host::Scope<Native> child(Identity(13),&other_context,&base,Query,mismatch,true);
      host::Read<Native>(&context,&base,Fallback);
      host::Read<Native>(&other_context,&other_base,Fallback);
      host::Read<Native>(&other_context,&base,Fallback);
    }
    host::Read<Native>(&context,&base,Fallback);
  }
  Check(outer.size == 2 && outer.rows[0].request.timer_invocation == 10 && outer.rows[1].request.timer_invocation == 10 &&
      inner.size == 1 && inner.rows[0].request.timer_invocation == 11 &&
      disabled.size == 1 && disabled.rows[0].request.timer_invocation == 0,
      "all nested scope types restore and inert scopes shadow admitted parents");
  Check(mismatch.size == 3 && (mismatch.rows[0].decisions & host::kContextMismatch) &&
      (mismatch.rows[1].decisions & host::kBaseMismatch) && mismatch.rows[0].request.timer_invocation == 0 &&
      mismatch.rows[1].request.timer_invocation == 0 && mismatch.rows[2].request.timer_invocation == 13,
      "exact pointer guard has no ancestor search or parent fallback");
  Ordinals(outer); Ordinals(inner); Ordinals(disabled); Ordinals(mismatch);

  const uint64_t timers[] = {0,std::numeric_limits<uint64_t>::max(),21};
  for (unsigned i = 0; i < 3; ++i) {
    host::Trace<8> parent{}, child_trace{};
    Reset({1});
    auto id = Identity(timers[i]); if (i == 2) { id.run_high = 0; id.run_low = 0; }
    {
      host::Scope<Native> scope(Identity(20),&context,&base,Query,parent,true);
      host::Scope<Native,1> child(id,&context,&base,Query,child_trace,true);
      host::Read<Native>(&context,&base,Fallback);
    }
    Check(parent.size == 0 && child_trace.size == 1 && child_trace.rows[0].request.timer_invocation == 0 &&
        child_trace.rows[0].scope_binding.timer_invocation == timers[i] &&
        (child_trace.rows[0].decisions & (i == 2 ? host::kUnboundRun : host::kUnboundTimer)),
        "zero MAX and unbound-run child preserve raw identity but cannot inherit parent binding");
  }
}

void PhaseFallbackAndStatus() {
  int context{}, base{};
  const host::Identity identities[] = {{kHigh,kLow,30,99,0,31,1}, {kHigh,kLow,31,99,7,31,0},
      {kHigh,kLow,32,99,7,31,2}, {kHigh,kLow,33,0,0,0,0}};
  for (unsigned i = 0; i < 4; ++i) {
    Reset({1}); host::Trace<8> trace{};
    {
      host::Scope<Native> scope(identities[i],&context,&base,Query,trace,true);
      host::Read<Native>(&context,&base,Fallback);
    }
    const auto& row = trace.rows[0];
    Check(row.request.phase_call_id == identities[i].phase_call_id &&
        row.request.phase_thread_epoch == identities[i].phase_thread_epoch &&
        row.request.phase_present == identities[i].phase_present && row.request.timer_invocation == 30+i &&
        bool(row.decisions & host::kInvalidPhase) == (i != 3) && row.status.issues == (i != 3 ? 1u : 0u),
        "raw optional phase contradictions are flagged and never normalized or hidden");
  }
  Reset({55}); host::Trace<8> unavailable{};
  {
    host::Scope<Native> scope(Identity(34),&context,&base,nullptr,unavailable,true);
    Check(host::Read<Native>(&context,&base,Fallback) == 55, "missing query uses original fallback exactly once");
  }
  Check(schedule.fallbacks == 1 && schedule.queries == 0 && unavailable.size == 1 &&
      (unavailable.rows[0].decisions & host::kQueryUnavailable) && !unavailable.rows[0].used_query &&
      !unavailable.rows[0].status_available, "fallback result is not promoted to available witness status");
  Reset({66}); schedule.bad_status = true; host::Trace<8> invalid{};
  {
    host::Scope<Native> scope(Identity(35),&context,&base,Query,invalid,true);
    host::Read<Native>(&context,&base,Fallback);
  }
  Check((invalid.rows[0].decisions & host::kInvalidStatus) && !invalid.rows[0].status_available &&
      invalid.rows[0].status.abi_version == 99 && invalid.rows[0].result == 66,
      "unsupported returned ABI remains raw diagnostic without changing original result");
}

void SameScopeReentryAndPolicyTypes() {
  int context{}, base{}; host::Trace<8> trace{};
  Reset({101,202});
  schedule.reenter_once = true; schedule.reentry_context = &context; schedule.reentry_base = &base;
  Native{0x24681357u,0x1F80u}.Restore();
  uint64_t result{};
  {
    host::Scope<Native> scope(Identity(80),&context,&base,Query,trace,true);
    result = host::Read<Native>(&context,&base,Fallback);
  }
  const auto produced = Native::Capture();
  Check(result == 202 && schedule.reentry_result == 101 && schedule.queries == 2 &&
      schedule.next == 2 && schedule.fallbacks == 0 && !schedule.errors && trace.size == 2 && trace.observed_reads == 2,
      "same-scope query-induced reentry performs exactly two original queries without fallback");
  Check(trace.rows[0].trace_ordinal == 1 && trace.rows[1].trace_ordinal == 2 &&
      trace.rows[0].result == 202 && trace.rows[1].result == 101 &&
      trace.rows[0].result_available && trace.rows[1].result_available &&
      trace.rows[0].request.timer_invocation == 80 && trace.rows[1].request.timer_invocation == 80 &&
      schedule.requests[0].read_ordinal == 0 && schedule.requests[1].read_ordinal == 1,
      "outer reserved row is not overwritten when nested query completes first");
  Check(schedule.incoming[0] == Native{0x24681357u,0x1F80u} &&
      schedule.incoming[1] == Native{0xABCD1000u,0x3F80u} && produced == Native{0xABCD1001u,0x5F80u},
      "inner original-produced native state reaches outer original instead of stale outer incoming state");
  Ordinals(trace);

  Reset({10,20}); host::Trace<8> outer{}, child{};
  {
    host::Scope<Native> scope(Identity(81),&context,&base,Query,outer,true);
    {
      host::Scope<AlternateNativePolicy> nested(Identity(82),&context,&base,Query,child,true);
      host::Read<Native>(&context,&base,Fallback);
    }
    host::Read<AlternateNativePolicy>(&context,&base,Fallback);
  }
  Check(child.size == 1 && outer.size == 1 && child.rows[0].request.timer_invocation == 82 &&
      outer.rows[0].request.timer_invocation == 81 && child.rows[0].result == 10 && outer.rows[0].result == 20 &&
      schedule.queries == 2 && !schedule.fallbacks,
      "common TLS scope lane spans NativePolicy types as well as read-limit types");
  Ordinals(child); Ordinals(outer);
}

void ThrowsAndRestoration() {
  int context{}, base{};
  auto request = MclaClockWitnessBindingV1{MCLA_CW_ABI_V1,sizeof(MclaClockWitnessBindingV1),kHigh,kLow,40,0,0,0,0,0,0,0};
  MclaClockWitnessStatusV1 status{};
  Reset({88}); schedule.throw_once = true; Native{0x24681357u,0x1F80u}.Restore();
  unsigned direct_exception{};
  try { Query(&request,&status); } catch (const Signal& signal) { direct_exception = signal.code; }
  const auto direct_state = Native::Capture();
  Reset({88,99}); schedule.throw_once = true; Native{0x24681357u,0x1F80u}.Restore();
  host::Trace<8> outer{}, child{}; unsigned recorded_exception{}; Native recorded_state{};
  {
    host::Scope<Native> scope(Identity(40),&context,&base,Query,outer,true);
    try {
      host::Scope<Native> nested(Identity(41),&context,&base,Query,child,true);
      host::Read<Native>(&context,&base,Fallback);
    } catch (const Signal& signal) { recorded_exception = signal.code; }
    recorded_state = Native::Capture();
    host::Read<Native>(&context,&base,Fallback);
  }
  Check(direct_exception == 43 && recorded_exception == direct_exception && recorded_state == direct_state &&
      child.size == 1 && child.rows[0].threw && !child.rows[0].result_available &&
      (child.rows[0].decisions & host::kQueryThrew) && child.rows[0].status.completion == MCLA_CW_ABORTED,
      "query throw/native effects unchanged with explicit failed receipt");
  Check(outer.size == 1 && outer.rows[0].request.timer_invocation == 40 && outer.rows[0].request.read_ordinal == 0 &&
      outer.rows[0].result == 99 && schedule.queries == 2 && schedule.fallbacks == 0,
      "throw restores outer scope and never retries or falls back after query exception");
  Reset({77}); schedule.throw_once = true; Native{0x24681357u,0x1F80u}.Restore();
  unsigned direct_fallback_exception{};
  try { Fallback(); } catch (const Signal& signal) { direct_fallback_exception = signal.code; }
  const auto direct_fallback_state = Native::Capture();
  Reset({77}); schedule.throw_once = true; host::Trace<8> fallback{}; host::Receipt receipt{};
  Native{0x24681357u,0x1F80u}.Restore();
  bool propagated{};
  {
    host::Scope<Native> scope(Identity(42),&context,&base,nullptr,fallback,true);
    try { host::Read<Native>(&context,&base,Fallback,&receipt); }
    catch (const Signal& signal) { propagated = signal.code == 43; }
  }
  const auto fallback_state = Native::Capture();
  Check(propagated && direct_fallback_exception == 43 && fallback_state == direct_fallback_state &&
      schedule.incoming[0] == Native{0x24681357u,0x1F80u} &&
      schedule.fallbacks == 1 && !schedule.queries && receipt.threw &&
      (receipt.decisions & host::kQueryUnavailable) && !receipt.status_available,
      "original-only fallback exception propagates once with unavailable witness status");
}

void BoundedTraceAndReadLimit() {
  int context{}, base{}; host::Trace<8> trace{};
  Reset({0,1,2,3});
  {
    host::Scope<Native,2> scope(Identity(50),&context,&base,Query,trace,true);
    for (unsigned i = 0; i < 4; ++i) host::Read<Native>(&context,&base,Fallback);
  }
  Check(trace.size == 4 && trace.ordinal_saturated && trace.rows[0].read_ordinal_available &&
      trace.rows[1].read_ordinal_available && !trace.rows[2].read_ordinal_available &&
      !trace.rows[3].read_ordinal_available && trace.rows[0].request.timer_invocation == 50 &&
      trace.rows[1].request.timer_invocation == 50 && trace.rows[2].request.timer_invocation == 0 &&
      trace.rows[3].request.timer_invocation == 0 && (trace.rows[2].decisions & host::kOrdinalSaturated) &&
      schedule.queries == 4 && schedule.next == 4, "read-limit saturation is nonwrapping unbound metadata, not skipped original work");
  Ordinals(trace);
  Reset({0,1,2,3,4,5,6,7,8,9,10}); host::Trace<8> overflow{}; host::Receipt last{};
  {
    host::Scope<Native> scope(Identity(51),&context,&base,Query,overflow,true);
    for (unsigned i = 0; i < 11; ++i) host::Read<Native>(&context,&base,Fallback,&last);
  }
  Check(overflow.size == 8 && overflow.observed_reads == 11 && overflow.lost_rows == 3 && overflow.overflow &&
      !overflow.ordinal_saturated && schedule.queries == 11 && schedule.next == 11 &&
      last.result == 10 && last.request.read_ordinal == 10 && (last.decisions & host::kTraceOverflow),
      "trace overflow retains first bounded slots and reports loss while original work continues");
  Ordinals(overflow);

  // Explicit synthetic public-POD counter state, NOT a claim that 2^64 reads
  // occurred. A full caller-owned Trace must never wrap counters or overwrite.
  host::Trace<8> saturated{};
  saturated.size = 8;
  saturated.observed_reads = saturated.lost_rows = std::numeric_limits<uint64_t>::max();
  for (uint32_t i = 0; i < 8; ++i) {
    saturated.rows[i].result = 900+i; saturated.rows[i].result_available = 1;
    saturated.rows[i].request.timer_invocation = 0xDEADu+i;
  }
  std::array<unsigned char,sizeof(saturated.rows)> retained{};
  std::memcpy(retained.data(),saturated.rows,sizeof(saturated.rows));
  Reset({71,72}); host::Receipt final{};
  {
    host::Scope<Native> scope(Identity(52),&context,&base,Query,saturated,true);
    host::Read<Native>(&context,&base,Fallback);
    host::Read<Native>(&context,&base,Fallback,&final);
  }
  Check(saturated.observed_reads == std::numeric_limits<uint64_t>::max() &&
      saturated.lost_rows == std::numeric_limits<uint64_t>::max() && saturated.size == 8 &&
      saturated.counter_saturated && saturated.overflow && !saturated.ordinal_saturated &&
      std::memcmp(retained.data(),saturated.rows,sizeof(saturated.rows)) == 0 &&
      schedule.queries == 2 && schedule.next == 2 && !schedule.fallbacks && final.result == 72 &&
      final.request.timer_invocation == 52 && final.request.read_ordinal == 1 &&
      (final.decisions & host::kTraceOverflow),
      "synthetic MAX trace counters saturate without overwrite or stopping original work");
}

void ThreadIsolation() {
  struct Result { host::Trace<8> trace; Native incoming, produced; uint64_t value; uint32_t queries, errors; };
  std::array<Result,4> results{};
  std::array<std::thread,4> threads;
  for (unsigned i = 0; i < 4; ++i) threads[i] = std::thread([&,i] {
    int context{}, base{}; Reset({100+i}); Native{0x24681357u,0x1F80u}.Restore();
    {
      host::Scope<Native> scope(Identity(60+i),&context,&base,Query,results[i].trace,true);
      results[i].value = host::Read<Native>(&context,&base,Fallback);
    }
    results[i].produced = Native::Capture(); results[i].queries = schedule.queries;
    results[i].incoming = schedule.incoming[0];
    results[i].errors = schedule.errors;
  });
  for (auto& thread : threads) thread.join();
  for (unsigned i = 0; i < 4; ++i) {
    Check(results[i].value == 100+i && results[i].queries == 1 && !results[i].errors && results[i].trace.size == 1 &&
        results[i].trace.rows[0].request.timer_invocation == 60+i && results[i].trace.rows[0].request.read_ordinal == 0 &&
        results[i].incoming == Native{0x24681357u,0x1F80u} &&
        results[i].produced == Native{0xABCD1000u,0x3F80u}, "thread-local first-use scope does not leak another caller binding/native state");
    Ordinals(results[i].trace);
  }
}
}  // namespace

int main() {
  const auto saved = Native::Capture();
  try {
    NoScopeAndDefaultOff(); RetriesAndNativeState(); NestedScopesAndGuards();
    PhaseFallbackAndStatus(); SameScopeReentryAndPolicyTypes(); ThrowsAndRestoration();
    BoundedTraceAndReadLimit(); ThreadIsolation();
  } catch (...) { Check(false, "unexpected synthetic host scope exception"); }
  Check(!host::kNativeAdmitted && !host::kCompleteClockHistoryProven && !host::kSimulationProven && !host::kFpsProven,
      "host scope prerequisite is not integration, complete history, simulation or FPS proof");
  saved.Restore();
  std::cout << "checks=" << checks << " failures=" << failures
      << " synthetic_host_scope_only no_real_clock_no_global_macro_no_integration\n";
  return failures ? 1 : 0;
}
