// Generic adapter prerequisite only. Real LastError/MXCSR, but predetermined
// integer samples, fake QPC and fake ABI Query: no SDK/DLL/game/native hook.
#include "../src/mcla_clock_host_invoke.h"
#include "../src/mcla_timer_observer_core.h"
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>
#include <xmmintrin.h>
#include <array>
#include <cstring>
#include <initializer_list>
#include <iostream>
#include <limits>
#include <memory>

namespace {
namespace invoke = mcla::clock_read_witness::host_invoke;
namespace host = mcla::clock_read_witness::host;
namespace journal = mcla::clock_read_witness::host_journal;
namespace timer = mcla::timer_observer;
namespace timer_journal = mcla::timer_journal;
constexpr journal::RunToken kRun{0x123456789ABCDEF0ull,0xFEDCBA9876543210ull};
struct Context { uint32_t value{}; };
struct Native {
  uint32_t error, mxcsr;
  static Native Capture() noexcept { const auto error = GetLastError(); return {error,_mm_getcsr()}; }
  void Restore() const noexcept { _mm_setcsr(mxcsr); SetLastError(error); }
  bool operator==(const Native&) const = default;
};
struct Signal { unsigned code; };
struct Schedule {
  uint64_t samples[32];
  MclaClockWitnessBindingV1 requests[32];
  Native incoming[32];
  uint32_t count, next, queries, fallbacks, errors;
  bool bad_status;
};
constinit thread_local Schedule schedule{};
unsigned checks{}, failures{};
void Check(bool ok, const char* label) {
  ++checks; if (!ok) { ++failures; std::cerr << "FAIL " << label << '\n'; }
}
void Reset(std::initializer_list<uint64_t> values) {
  schedule = {}; schedule.count = uint32_t(values.size());
  size_t i = 0; for (auto value : values) { if (i < 32) schedule.samples[i++] = value; else ++schedule.errors; }
}
host::Identity Identity(uint64_t ordinal) noexcept { return {kRun.high,kRun.low,ordinal,0,0,0,0}; }
journal::Invocation Invocation(uint64_t ordinal) noexcept {
  return {ordinal,0,0,31,0x827D7500,0x822C21B4,0,0};
}
uint64_t Clock() {
  const unsigned index = schedule.next++;
  if (index >= schedule.count || index >= 32) { ++schedule.errors; throw Signal{900}; }
  schedule.incoming[index] = Native::Capture();
  Native{0xABCD1000u+index,index % 2 ? 0x5F80u : 0x3F80u}.Restore();
  return schedule.samples[index];
}
uint64_t Fallback() { ++schedule.fallbacks; return Clock(); }
uint64_t MCLA_CW_CALL Query(const MclaClockWitnessBindingV1* binding, MclaClockWitnessStatusV1* status) {
  const unsigned index = schedule.queries++;
  if (!binding || !status || index >= 32) { ++schedule.errors; throw Signal{901}; }
  schedule.requests[index] = *binding;
  const uint64_t result = Clock();
  const bool bound = binding->timer_invocation != 0;
  const bool bad_phase = binding->phase_present
      ? binding->phase_present != 1 || !binding->phase_call_id || !binding->phase_thread_epoch || !binding->phase_native_thread_id
      : binding->phase_call_id || binding->phase_thread_epoch || binding->phase_native_thread_id;
  *status = {schedule.bad_status ? 99u : 1u,sizeof(*status),bound ? MCLA_CW_ACCEPTED : MCLA_CW_UNBOUND,
      bound ? MCLA_CW_PUBLISHED : MCLA_CW_NO_EVENT,bad_phase ? 1u : 0u,0};
  return result;
}
uint64_t Read(Context& context, uint8_t* base) { return host::Read<Native>(&context,base,Fallback); }
template<size_t Capacity> auto Freeze(journal::Journal<Capacity>& core) {
  auto output = std::make_unique<typename journal::Journal<Capacity>::Snapshot>();
  core.Stop(); Check(core.TryFreezeInto(*output),"adapter journal drains after completed original"); return output;
}

void OriginalAndRetries() {
  auto core = std::make_unique<journal::Journal<2>>(); core->Arm(kRun);
  Context context; uint8_t bytes[4]{}; unsigned originals{}, retries{};
  Reset({0,0x100000000ull,0x100000007ull});
  const Native incoming{0x24681357u,0x1F80u}; incoming.Restore();
  Native captured_inside{}; uint64_t result{};
  invoke::Invoke<Native>(core.get(),Identity(7),Invocation(7),context,bytes,Query,true,
      [&](Context& same, uint8_t* base) {
        ++originals; captured_inside = Native::Capture();
        Check(&same == &context && base == bytes,"adapter calls exact original context/base once");
        do { result = Read(same,base); ++retries; } while (uint32_t(result) == 0);
        ++same.value; ++base[0];
      });
  const auto produced = Native::Capture();
  auto snapshot = Freeze(*core);
  Check(originals == 1 && captured_inside == incoming && retries == 3 && result == 0x100000007ull &&
      context.value == 1 && bytes[0] == 1 && schedule.queries == 3 && schedule.next == 3 && !schedule.fallbacks &&
      produced == Native{0xABCD1002u,0x3F80u},"adapter preserves incoming/produced state and original retry control");
  Check(snapshot->size == 1 && snapshot->rows[0].invocation.timer_ordinal == 7 &&
      snapshot->rows[0].outcome == journal::Outcome::Normal && snapshot->rows[0].trace.size == 3,
      "one original scope publishes actual three-read trace");
  for (unsigned i = 0; i < 3; ++i)
    Check(snapshot->rows[0].trace.rows[i].request.read_ordinal == i &&
        snapshot->rows[0].trace.rows[i].request.timer_invocation == 7 &&
        snapshot->rows[0].trace.rows[i].result == schedule.samples[i],"each real retry keeps exact caller ordinal/result");
}

// R2 observation repair: do not rely on optimized captured automatic scalars
// surviving nested Windows EH. Keep the exact same body in direct/adapter paths,
// a volatile runtime operand and externally observable heap-backed results.
struct ThrowObservation {
  Native before_fp{}, after_fp{}, caught{};
  const void* context{};
  const void* base{};
  volatile float operand = 1.75f;
  int fp{};
  unsigned originals{}, signal{};
};
__declspec(noinline) void ThrowBody(Context& same, uint8_t* base, ThrowObservation& result) {
  ++result.originals; result.context = &same; result.base = base;
  Read(same,base); result.before_fp = Native::Capture();
  const float operand = result.operand;
  result.fp = _mm_cvtss_si32(_mm_set_ss(operand));
  result.after_fp = Native::Capture();
  ++same.value; ++base[0]; throw Signal{43};
}
void ThrowAndFloatingPoint() {
  auto core = std::make_unique<journal::Journal<1>>(); core->Arm(kRun);
  auto direct = std::make_unique<ThrowObservation>();
  auto adapted = std::make_unique<ThrowObservation>();
  auto direct_trace = std::make_unique<host::Trace<16>>();
  Context reference, context; uint8_t reference_bytes[4]{}, bytes[4]{};
  // EH first touch is preflight, not a claimed original-query effect.
  try { throw Signal{0}; } catch (const Signal&) {}
  Reset({77}); Native{0x13572468u,0x1F80u}.Restore();
  try {
    host::Scope<Native> scope(Identity(8),&reference,reference_bytes,Query,*direct_trace,true);
    ThrowBody(reference,reference_bytes,*direct);
  } catch (const Signal& signal) { direct->caught = Native::Capture(); direct->signal = signal.code; }
  const auto direct_queries = schedule.queries, direct_fallbacks = schedule.fallbacks;
  Reset({77}); Native{0x13572468u,0x1F80u}.Restore();
  try {
    invoke::Invoke<Native>(core.get(),Identity(8),Invocation(8),context,bytes,Query,true,
        [&](Context& same, uint8_t* base) { ThrowBody(same,base,*adapted); });
  } catch (const Signal& signal) { adapted->caught = Native::Capture(); adapted->signal = signal.code; }
  auto snapshot = Freeze(*core);
  Check(direct->signal == 43 && adapted->signal == 43 && direct->originals == 1 && adapted->originals == 1 &&
      reference.value == 1 && context.value == 1 && reference_bytes[0] == 1 && bytes[0] == 1 &&
      direct->fp == 1 && adapted->fp == 1 && direct->before_fp == Native{0xABCD1000u,0x3F80u} &&
      adapted->before_fp == direct->before_fp && adapted->after_fp == direct->after_fp &&
      adapted->caught == direct->caught && direct_queries == 1 && schedule.queries == 1 &&
      !direct_fallbacks && !schedule.fallbacks,
      "original throw and produced native state propagate after caller floating-point operation");
  Check(direct->context == &reference && direct->base == reference_bytes && adapted->context == &context &&
      adapted->base == bytes,"shared noinline throw body retains exact caller context and base");
  Check(snapshot->rows[0].trace.size == 1 && direct_trace->size == 1 &&
      std::memcmp(&snapshot->rows[0].trace,direct_trace.get(),sizeof(*direct_trace)) == 0,
      "direct and adapted throw controls retain identical pre-throw clock receipts");
  Check(snapshot->rows[0].outcome == journal::Outcome::Unwound &&
      snapshot->rows[0].disposition == journal::Disposition::Completed &&
      (snapshot->rows[0].flags & journal::kOriginalUnwound) && snapshot->rows[0].trace.rows[0].result == 77,
      "successful clock receipt retained even when subsequent original timer unwinds");
}
void NestedInertPaths() {
  auto core = std::make_unique<journal::Journal<1>>(); core->Arm(kRun);
  auto disabled = std::make_unique<journal::Journal<1>>();
  Context context; uint8_t bytes[4]{}; Reset({1,2,3,4,5}); unsigned originals{};
  invoke::Invoke<Native>(core.get(),Identity(10),Invocation(10),context,bytes,Query,true,
      [&](Context& same, uint8_t* base) {
        ++originals; Read(same,base);
        auto child = [&](Context& child_context, uint8_t* child_base) { ++originals; Read(child_context,child_base); };
        invoke::Invoke<Native>(static_cast<journal::Journal<1>*>(nullptr),Identity(11),Invocation(11),same,base,Query,true,child);
        invoke::Invoke<Native>(disabled.get(),Identity(12),Invocation(12),same,base,Query,true,child);
        invoke::Invoke<Native>(core.get(),Identity(13),Invocation(13),same,base,Query,true,child);
        Read(same,base);
      });
  auto snapshot = Freeze(*core); auto absent = Freeze(*disabled);
  Check(originals == 4 && schedule.queries == 5 && !schedule.fallbacks && snapshot->size == 1 &&
      snapshot->counts.observed_scopes == 2 && snapshot->counts.lost_scopes == 1 && snapshot->counts.inconclusive &&
      !absent->size && !absent->counts.observed_scopes,"null disabled and overflow adapters preserve original calls and explicit inventory loss");
  Check(schedule.requests[0].timer_invocation == 10 && schedule.requests[0].read_ordinal == 0 &&
      schedule.requests[1].timer_invocation == 0 && schedule.requests[2].timer_invocation == 0 &&
      schedule.requests[3].timer_invocation == 0 && schedule.requests[4].timer_invocation == 10 &&
      schedule.requests[4].read_ordinal == 1 && snapshot->rows[0].trace.size == 2,
      "EVERY inert adapter shadows parent and then restores its distinct read counter");
  const auto preserved = snapshot->counts.observed_scopes;
  Reset({9});
  invoke::Invoke<Native>(core.get(),Identity(14),Invocation(14),context,bytes,Query,true,
      [&](Context& same, uint8_t* base) { Read(same,base); });
  auto repeated = Freeze(*core);
  Check(schedule.requests[0].timer_invocation == 0 && repeated->counts.observed_scopes == preserved &&
      std::memcmp(snapshot.get(),repeated.get(),sizeof(*snapshot)) == 0,"post-seal original uses inert local trace without changing frozen receipt bytes");
}

void RawDiagnosticsAndTraceLimit() {
  auto core = std::make_unique<journal::Journal<4>>(); core->Arm(kRun);
  Context context; uint8_t bytes[4]{}; Reset({11,12,13});
  auto partial = Identity(20); partial.phase_call_id = 90; partial.phase_present = 1; // Other raw fields stay absent.
  invoke::Invoke<Native>(core.get(),partial,Invocation(999),context,bytes,Query,true,
      [&](Context& same, uint8_t* base) { Read(same,base); });
  schedule.bad_status = true;
  invoke::Invoke<Native>(core.get(),Identity(21),Invocation(21),context,bytes,Query,true,
      [&](Context& same, uint8_t* base) { Read(same,base); });
  schedule.bad_status = false;
  invoke::Invoke<Native>(core.get(),Identity(22),Invocation(22),context,bytes,nullptr,true,
      [&](Context& same, uint8_t* base) { Read(same,base); });
  Reset({0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19});
  unsigned originals{}, reads{};
  invoke::Invoke<Native>(core.get(),Identity(23),Invocation(23),context,bytes,Query,true,
      [&](Context& same, uint8_t* base) {
        ++originals; for (unsigned i = 0; i < 20; ++i) { Check(Read(same,base) == i,"trace overflow never changes original query value"); ++reads; }
      });
  auto snapshot = Freeze(*core);
  Check(snapshot->size == 4 && snapshot->rows[0].invocation.timer_ordinal == 999 &&
      snapshot->rows[0].trace.rows[0].scope_binding.timer_invocation == 20 &&
      snapshot->rows[0].trace.rows[0].request.phase_call_id == 90 &&
      !snapshot->rows[0].trace.rows[0].request.phase_thread_epoch &&
      (snapshot->rows[0].trace.rows[0].decisions & host::kInvalidPhase),
      "raw invocation/Scope contradiction and partial phase survive without repair");
  Check(snapshot->rows[1].trace.rows[0].status.abi_version == 99 &&
      (snapshot->rows[1].trace.rows[0].decisions & host::kInvalidStatus) &&
      (snapshot->rows[2].trace.rows[0].decisions & host::kQueryUnavailable) &&
      !snapshot->rows[2].trace.rows[0].status_available && !snapshot->rows[2].trace.rows[0].used_query,
      "malformed returned ABI and missing query remain explicit raw diagnostics");
  Check(originals == 1 && reads == 20 && schedule.queries == 20 && !schedule.fallbacks &&
      snapshot->rows[3].trace.size == 16 && snapshot->rows[3].trace.observed_reads == 20 &&
      snapshot->rows[3].trace.lost_rows == 4 && snapshot->rows[3].trace.overflow && snapshot->counts.inconclusive,
      "accepted adapter keeps real Trace16 while all twenty evaluations still execute");
  for (unsigned i = 0; i < 16; ++i) Check(snapshot->rows[3].trace.rows[i].request.read_ordinal == i,
      "Trace16 retains first sixteen actual read ordinals without reuse");
}

void ZeroMaxAndExplicitDisabled() {
  auto core = std::make_unique<journal::Journal<3>>(); core->Arm(kRun);
  Context context; uint8_t bytes[4]{}; Reset({1,2,3}); unsigned originals{};
  const uint64_t timers[] = {0,std::numeric_limits<uint64_t>::max(),33};
  for (unsigned i = 0; i < 3; ++i)
    invoke::Invoke<Native>(core.get(),Identity(timers[i]),Invocation(timers[i]),context,bytes,Query,i != 2,
        [&](Context& same, uint8_t* base) { ++originals; Read(same,base); });
  auto snapshot = Freeze(*core);
  Check(originals == 3 && snapshot->size == 3 && schedule.queries == 3 && !schedule.fallbacks,
      "accepted zero MAX and explicitly disabled invocation each retain one ordinary original call");
  for (unsigned i = 0; i < 3; ++i) {
    const auto& row = snapshot->rows[i];
    Check(row.invocation.timer_ordinal == timers[i] && row.trace.rows[0].scope_binding.timer_invocation == timers[i] &&
        row.trace.rows[0].request.timer_invocation == 0 &&
        (row.trace.rows[0].decisions & (i == 2 ? host::kDisabled : host::kUnboundTimer)),
        "zero MAX and disabled adapter scopes preserve raw identity but send no fabricated binding");
  }
}

void ObserverPublicationSeam(bool unwind) {
  auto receipts = std::make_unique<journal::Journal<2>>(); receipts->Arm(kRun);
  auto at_callback = std::make_unique<journal::Journal<2>::Snapshot>();
  timer::Observer<2> observer({100,200,1000},true);
  Context context; uint8_t bytes[4]{}; uint8_t* memory = bytes;
  Reset({88}); unsigned originals{}, states{}; uint64_t forwarded{}, qpc = 110;
  bool frozen_inside{}, no_scope_inside{}; bool propagated{};
  const Native caller_incoming{0x35792468u,0x1F80u};
  // Give the real timer observer an earlier admission WITHOUT a host journal
  // slot. The tested ordinal2 must stay distinct from host storage ordinal1.
  Context primer_context; uint8_t* primer_memory = bytes;
  const timer::Operations<Context,uint8_t*> primer_operations{
      [&] { return qpc++; },
      [](const Context& same, uint8_t* const&, timer_journal::State& output) {
        output.words[0] = same.value; return true;
      }};
  observer.invoke({0x827D7500,0x822C21B0,31},primer_context,primer_memory,primer_operations,0,0,
      [](Context&, uint8_t*&) {});
  const timer::Operations<Context,uint8_t*> operations{
      [&] { return qpc++; },
      [&](const Context& same, uint8_t* const&, timer_journal::State& output) {
        output.words[0] = same.value;
        if (++states == 2) {
          receipts->Stop(); frozen_inside = receipts->TryFreezeInto(*at_callback);
          no_scope_inside = host::detail::current == nullptr; // White-box lifetime assertion only.
        }
        // Model existing timer instrumentation disturbing native state. The
        // caller restores its external incoming before adapter entry below.
        Native{0xEEEE0000u+states,0x7F80u}.Restore(); return true;
      }};
  caller_incoming.Restore(); Native produced{};
  try {
    observer.invoke_with_ordinal({0x827D7500,0x822C21B4,31},context,memory,operations,0xA5,0xFEDCBA9876543210ull,
        [&](Context& same, uint8_t*& same_memory, uint64_t ordinal) {
          forwarded = ordinal; caller_incoming.Restore();
          invoke::Invoke<Native>(receipts.get(),Identity(ordinal),Invocation(ordinal),same,same_memory,Query,true,
              [&](Context& actual, uint8_t* base) {
                ++originals;
                Check(&actual == &context && base == bytes && Native::Capture() == caller_incoming,
                    "real ordinal seam passes exact refs and restored caller incoming");
                Read(actual,base); ++actual.value;
                if (unwind) throw Signal{43};
              });
        });
  } catch (const Signal& signal) { propagated = signal.code == 43; }
  produced = Native::Capture(); // Outer hook owns restoration after Completion.
  (void)produced;
  auto timer_snapshot = std::make_unique<timer::Observer<2>::Snapshot>();
  Check(observer.try_freeze_into(*timer_snapshot),"actual timer Completion fully drains after adapter");
  Check(originals == 1 && states == 2 && frozen_inside && no_scope_inside && at_callback->size == 1 &&
      at_callback->counts.completed_scopes == 1 && at_callback->rows[0].trace.size == 1 &&
      at_callback->rows[0].trace.rows[0].result == 88 &&
      at_callback->rows[0].outcome == (unwind ? journal::Outcome::Unwound : journal::Outcome::Normal),
      "host Scope and Lease publication finished BEFORE timer Completion after-state callback");
  if (timer_snapshot->records().size() == 2) {
    Check(forwarded == 2 && forwarded == timer_snapshot->records()[1].entry_ordinal &&
        at_callback->rows[0].storage_ordinal == 1 && at_callback->rows[0].invocation.timer_ordinal == forwarded &&
        at_callback->rows[0].trace.rows[0].request.timer_invocation == forwarded,
        "existing reserve ordinal is forwarded unchanged, not journal storage ordinal or surrogate");
    Check(propagated == unwind && bool(timer_snapshot->records()[1].callback_issues & timer::kOriginalUnwound) == unwind &&
        schedule.queries == 1 && !schedule.fallbacks,"real timer seam preserves normal/throw behavior and exact one original read");
  } else Check(false,"primer and tested timer invocation both retained");
}
}  // namespace

int main() {
  const auto native = Native::Capture();
  try {
    OriginalAndRetries(); ThrowAndFloatingPoint(); NestedInertPaths(); RawDiagnosticsAndTraceLimit(); ZeroMaxAndExplicitDisabled();
    ObserverPublicationSeam(false); ObserverPublicationSeam(true);
  } catch (...) { Check(false,"unexpected synthetic host Invoke exception"); }
  Check(!invoke::kNativeAdmitted && !invoke::kCompleteClockHistoryProven && !invoke::kFrameProven &&
      !invoke::kSimulationProven && !invoke::kFpsProven,"generic adapter has no native SDK frame simulation or FPS admission");
  native.Restore();
  std::cout << "checks=" << checks << " failures=" << failures
      << " synthetic_host_invoke_only no_SDK_no_real_clock_no_game\n";
  return failures ? 1 : 0;
}
