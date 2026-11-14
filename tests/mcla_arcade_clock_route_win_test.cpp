// Guest-free ONE-TU prerequisite: actual phase/strong timer/Arcade/controller
// sources and the already loaded, paired SDK DLL. Compile ONLY this TU, with
// MCLA_PHASE_CENSUS_PROBE, MCLA_UPDATE_TIMING_PROBE, MCLA_ARCADE_TIMER_PROBE,
// MCLA_ARCADE_UPDATE_BINDING_PROBE and MCLA_CLOCK_READ_WITNESS_PROBE.
// No FRAME_HANDOFF, UPDATE_FAULT_CONTEXT or TIMER_OBSERVER macros are allowed.
// Each argv mode needs a FRESH process. InstallState/cache publication below
// is an explicit white-box seam: no Runtime/dispatcher installation is tested.
// Generated originals are synthetic; SDK clock values are real and are NEVER
// compared across baseline/routed calls as though real time were repeatable.
// No game, input, GPU, files, sleeps, full clock history, frame, simulation,
// performance or production separate-TU/runtime-admission claim is made.
#include "../src/mcla_phase_census_win.cpp"
#include "../src/mcla_update_timing_win.cpp"
#include "../src/mcla_arcade_timer_win.cpp"
#include "../src/mcla_clock_game_bridge_win.cpp"
#include <array>
#include <chrono>
#include <cstring>
#include <iostream>
#include <memory>
#include <string_view>

#if !defined(MCLA_CLOCK_READ_WITNESS_PROBE) || !defined(MCLA_ARCADE_UPDATE_BINDING_PROBE) || \
    defined(MCLA_FRAME_HANDOFF_PROBE) || defined(MCLA_UPDATE_FAULT_CONTEXT_PROBE) || \
    defined(MCLA_TIMER_OBSERVER_PROBE)
#error Compile only the explicit private Arcade clock route fixture configuration.
#endif

namespace {
namespace phase = mcla::phase_census;
namespace pd = phase::detail;
namespace arcade = mcla::arcade_timer;
namespace ad = arcade::detail;
namespace cg = mcla::clock_game;
namespace core = mcla::timer_observer;
namespace host = mcla::clock_read_witness::host;
namespace journal = mcla::clock_read_witness::host_journal;
using HostSnapshot = journal::Journal<1024>::Snapshot;
constexpr char token[] = "123456789abcdef0fedcba9876543210";
constexpr uint64_t run_high = 0x123456789ABCDEF0ull, run_low = 0xFEDCBA9876543210ull;
constexpr uint32_t timer_address = 128, caller_address = 0x822C21B4;
struct Native {
  DWORD error; unsigned mxcsr;
  static Native Capture() noexcept { const auto e = GetLastError(); return {e, _mm_getcsr()}; }
  void Restore() const noexcept { _mm_setcsr(mxcsr); SetLastError(error); }
  bool operator==(const Native&) const = default;
};
constexpr Native incoming{0x24681357u, 0x3F80u};
enum class Mode { Healthy, Unwind, NullObserver, Outside, Render, Context, Base, NestedNull, Hostile };
Mode mode{};
bool routed{};
unsigned checks{}, failures{}, phase_clock_calls{}, hostile_clock_calls{}, hostile_state_calls{}, binding_calls{};
core::ScopeBinding callback_entry{}, callback_exit{};
struct Signal { unsigned code; };
struct Body {
  PPCContext context{}, alternate{};
  std::array<uint8_t, 512> bytes{};
  const void *seen_context{}, *seen_base{};
  phase::ActiveUpdate active{};
  Native incoming{}, before_read[2]{}, after_read[2]{}, before_fp{}, after_fp{}, produced{}, external{};
  uint64_t values[2]{};
  volatile float operand = 1.75f;
  int fp{};
  unsigned originals{}, reads{}, caught{}, unexpected{};
  bool unwind{};
  Body* child{}; // Preflight-owned heap child; no allocation inside the original.
};
constinit thread_local Body* body = nullptr;

void Check(bool ok, const char* label) {
  const auto native = Native::Capture(); Native{0, 0x1F80}.Restore();
  ++checks; if (!ok) { ++failures; std::cerr << "FAIL " << label << '\n'; }
  native.Restore();
}
void Flag(const char* name, bool value) {
  Check(rex::cvar::SetFlagByName(name, value ? "true" : "false"), "fixture pre-init CVar selection");
}
void Disturb() noexcept { Native{0x99887766u, 0x5F80u}.Restore(); }
uint64_t HostilePhaseClock() {
  ++phase_clock_calls; const auto tick = pd::Qpc(); Disturb(); return tick;
}
core::ScopeBinding ScopeTuple(phase::ActiveUpdate value) noexcept {
  return {value.phase_call_id, value.thread_epoch, value.native_thread_id};
}
const ad::Operations hostile_operations{
    [] { ++hostile_clock_calls; const auto tick = ad::Qpc(); Disturb(); return tick; },
    [](const PPCContext&, const ad::MemoryArg& memory, mcla::timer_journal::State& output) {
      ++hostile_state_calls;
      const bool ok = mcla::timer_memory::Read(reinterpret_cast<uintptr_t>(memory.base), memory.initial_timer, output);
      Disturb(); return ok;
    },
    [](const PPCContext& context, const ad::MemoryArg& memory) {
      auto value = ScopeTuple(phase::ReadActiveUpdate(context, memory.base));
      ++binding_calls;
      if (binding_calls == 1) callback_entry = value;
      else {
        // Deliberately DIFFERENT exit tuple. The host must receive the captured
        // entry, not a second callback, synthesized tuple or later exit value.
        value.phase_call_id += 1000; value.thread_epoch += 2000; callback_exit = value;
      }
      Disturb(); return value;
    }};

__declspec(noinline) void TimerOriginal(PPCContext& context, uint8_t* base);
void NullTimer(PPCContext& context, uint8_t* base) {
  const auto saved = ad::HostState::Capture();
  ad::InvokeObserved(__imp__sub_821BDA90, nullptr, context, base, ad::binding_operations, saved, true);
}
void Child(Body& child) {
  Body* previous = body; body = &child;
  try {
    if (routed) NullTimer(child.context, child.bytes.data());
    else TimerOriginal(child.context, child.bytes.data());
  } catch (...) { body = previous; throw; }
  child.external = Native::Capture(); body = previous;
}
// Shared direct/routed original. Heap stores and volatile FP operand remain
// observable on unwind; no captured automatic observation variables are used.
__declspec(noinline) void TimerOriginal(PPCContext& context, uint8_t* base) {
  auto& b = *body; ++b.originals; b.seen_context = &context; b.seen_base = base;
  b.incoming = Native::Capture(); b.active = phase::ReadActiveUpdate(context, base);
  for (unsigned i = 0; i < 2; ++i) {
    b.before_read[i] = Native::Capture(); b.values[i] = cg::Read(context, base); ++b.reads;
    b.after_read[i] = Native::Capture();
    if (!i && b.child) Child(*b.child);
  }
  b.before_fp = Native::Capture(); const float operand = b.operand;
  b.fp = _mm_cvtss_si32(_mm_set_ss(operand)); b.after_fp = Native::Capture();
  base[timer_address + 8] ^= 0x5A; base[7] ^= 0x39;
  context.r3.u64 = 0xFEEDFACE12345678ull; context.r31.u64 ^= 0x8877665544332211ull;
  context.r5.u64 ^= 0xDEADBEEFull; context.f1.u64 ^= 0x1122334455667788ull;
  context.f28.u64 ^= 0x8000000000000000ull; context.lr = 0x82221100;
  SetLastError(0xABCD1234u); b.produced = Native::Capture();
  if (b.unwind) throw Signal{991};
}
void CallTimer(PPCContext& context, uint8_t* base) {
  if (!routed) { __imp__sub_821BDA90(context, base); return; }
  if (mode == Mode::Hostile) {
    const auto saved = ad::HostState::Capture();
    ad::InvokeObserved(__imp__sub_821BDA90, ad::configured->observer.load(), context, base,
                       hostile_operations, saved, true);
  } else sub_821BDA90(context, base); // The actual strong UpdateTiming route.
}
void UpdateOriginal(PPCContext& context, uint8_t* base) {
  if (mode == Mode::Render) {
    if (routed) sub_82131008(context, base); else __imp__sub_82131008(context, base);
  } else if (mode == Mode::Context) {
    std::memcpy(&body->alternate, &context, sizeof(context));
    CallTimer(body->alternate, base); std::memcpy(&context, &body->alternate, sizeof(context));
  } else CallTimer(context, mode == Mode::Base ? base + 1 : base);
}
void Initialize(Body& b) {
  for (size_t i = 0; i < b.bytes.size(); ++i) b.bytes[i] = uint8_t(i);
  b.context.r3.u32 = timer_address; b.context.lr = caller_address;
  b.context.r5.u64 = 0xAABBCCDD; b.context.r31.u64 = 0x123456789;
  b.context.f1.u64 = 0x0123456789ABCDEFull; b.context.f28.u64 = 0x8000000000000000ull;
}
void Run(Body& b, bool instrumented) {
  routed = instrumented; body = &b; incoming.Restore();
  // Null observer is not a stopped/drained substitute: retain the actual
  // observer pointer and restore it before the real final Completion drain.
  auto* retained = ad::configured->observer.load();
  if (instrumented && mode == Mode::NullObserver) ad::configured->observer.store(nullptr);
  try {
    if (mode == Mode::Outside) CallTimer(b.context, b.bytes.data());
    else if (instrumented) sub_822C1FA8(b.context, b.bytes.data());
    else __imp__sub_822C1FA8(b.context, b.bytes.data());
  } catch (const Signal& signal) { b.caught = signal.code; }
    catch (...) { b.unexpected = 1; }
  b.external = Native::Capture();
  if (instrumented && mode == Mode::NullObserver) ad::configured->observer.store(retained);
  body = nullptr;
}
bool Arithmetic(const MclaClockWitnessRecordV1& row) {
  if (row.presence != 15 || row.ratio_numerator != 1 || row.ratio_denominator != 1 ||
      row.returned_guest != row.post_last_guest) return false;
  if (row.branch == 1) {
    const uint64_t delta = row.host_sample > row.pre_last_host ? row.host_sample - row.pre_last_host : 0;
    return row.post_last_host == row.host_sample && row.post_last_guest == row.pre_last_guest + delta;
  }
  return row.branch == 2 && row.pre_last_host == row.post_last_host && row.pre_last_guest == row.post_last_guest;
}
void CheckBody(const Body& direct, const Body& observed, bool child = false) {
  const void* context = !child && mode == Mode::Context ? &observed.alternate : &observed.context;
  const void* base = observed.bytes.data() + (!child && mode == Mode::Base ? 1 : 0);
  Check(observed.originals == 1 && observed.reads == 2 && observed.seen_context == context && observed.seen_base == base,
        "same exact context/base and one original with two evaluations");
  Check(std::memcmp(&direct.context, &observed.context, sizeof(PPCContext)) == 0 && direct.bytes == observed.bytes,
        "full context and memory differential excludes nondeterministic clock values");
  Check(observed.incoming == direct.incoming && observed.before_read[0] == direct.before_read[0] &&
        observed.before_fp == direct.before_fp && observed.after_fp == direct.after_fp,
        "original native state and volatile FP agree with shared direct control");
  for (unsigned i = 0; i < 2; ++i)
    Check(observed.before_read[i] == direct.before_read[i] && observed.after_read[i] == direct.after_read[i],
          "each actual SDK read preserves direct-control native state before subsequent original work");
  Check(observed.fp == 1 && (observed.before_fp.mxcsr & 0x6000u) == 0x2000u &&
        (observed.after_fp.mxcsr & 0x6000u) == 0x2000u,
        "actual SDK reads preserve round-down for subsequent original FP arithmetic");
  // Compare the separately caught direct control, not an assumed equality
  // between before-throw state and the C++ EH runtime's produced catch state.
  Check(observed.external == direct.external && observed.produced == direct.produced &&
        observed.caught == direct.caught && observed.caught == (observed.unwind ? 991u : 0u) &&
        !observed.unexpected && !direct.unexpected, "normal/unwind native and exception outcome matches direct control");
}
void EmitBody(const char* name, const Body& b) {
  const auto saved = Native::Capture(); Native{0, 0x1F80}.Restore();
  std::cout << "BODY role=" << name << " originals=" << b.originals << " reads=" << b.reads
      << " value0=" << b.values[0] << " value1=" << b.values[1] << " fp=" << b.fp << " caught=" << b.caught
      << " incomingError=" << b.incoming.error << " beforeFPError=" << b.before_fp.error
      << " producedError=" << b.produced.error << " externalError=" << b.external.error
      << " incomingMXCSR=" << b.incoming.mxcsr << " beforeFPMXCSR=" << b.before_fp.mxcsr
      << " afterFPMXCSR=" << b.after_fp.mxcsr << " producedMXCSR=" << b.produced.mxcsr
      << " externalMXCSR=" << b.external.mxcsr << '\n'; saved.Restore();
}
void ValidateRaw(const ad::Observer::Snapshot& timers, const HostSnapshot& h,
                 const MclaClockWitnessSnapshotV2& s, const Body& observed) {
  const bool inert = mode == Mode::NullObserver;
  const bool nested = mode == Mode::NestedNull;
  const bool phase_unknown = mode == Mode::Outside || mode == Mode::Render || mode == Mode::Context || mode == Mode::Base;
  const auto timer_size = timers.records().size();
  Check(timer_size == (inert ? 1u : 2u) && timers.counts().admitted_entries == timer_size &&
        timers.counts().completed_entries == timer_size && !timers.counts().observer_failed &&
        !timers.counts().overflow_entries, "actual timer Completions drained including primer");
  Check(h.size == (nested ? 2u : 1u) && h.counts.observed_scopes == h.size && h.counts.completed_scopes == h.size &&
        h.counts.stored_scopes == h.size && h.counts.normal_scopes == h.size - unsigned(observed.unwind) &&
        h.counts.unwound_scopes == unsigned(observed.unwind) && !h.counts.zero_read_scopes &&
        !h.counts.lost_scopes && !h.counts.aborted_scopes && !h.counts.trace_loss_scopes &&
        !h.counts.ordinal_saturated_scopes && !h.counts.trace_counter_saturated_scopes && !h.counts.counter_saturated,
        "every raw host scope retained without discarded diagnostics");
  Check(s.abi_version == MCLA_CW_ABI_V2 && s.struct_size == sizeof(s) && s.capacity == MCLA_CW_CAPACITY_V2 &&
        s.row_count == (inert ? 0u : 2u) && s.observed_bound_reads == s.row_count &&
        s.stored_entries == s.row_count && s.published_entries == s.row_count && !s.aborted_entries &&
        !s.overflow_entries && !s.invalid_bindings && !s.saturated_rejections && !s.missing_witnesses &&
        !s.duplicate_witnesses && !s.counter_saturated && !s.proof_flags,
        "actual SDK retains exact admitted inventory without invented inert events");
  Check(h.run.high == run_high && h.run.low == run_low && s.run_high == run_high && s.run_low == run_low,
        "complete run128 retained in both inventories");
  Check(s.diagnostic_entries == (phase_unknown ? 2u : 0u), "SDK invalid phase diagnostics retained rather than filtered");
  Check(h.counts.diagnostic_scopes == ((inert || nested || phase_unknown || observed.unwind) ? 1u : 0u) &&
        h.counts.inconclusive == ((inert || nested || phase_unknown || observed.unwind) ? 1u : 0u),
        "host diagnostic and unwind inventory never promoted to healthy");
  if (h.size > 1024 || s.row_count > 4096 || timer_size < (inert ? 1u : 2u)) return;
  const core::RawRecord* row = inert ? nullptr : &timers.records()[1];
  if (row) {
    Check(row->entry_ordinal == 2 && h.rows[0].storage_ordinal == 1 && h.rows[0].invocation.timer_ordinal == 2,
          "real observer ordinal differs from host storage after primer");
    Check(row->callback_issues == (observed.unwind ? core::kOriginalUnwound : 0u), "raw timer callback issues exact");
    Check(row->observation.identity.timer_address == timer_address && row->observation.identity.caller_address == caller_address &&
          row->observation.before_available && row->observation.after_available &&
          (row->observation.before.words[0] ^ row->observation.after.words[0]) == 0x5A000000u,
          "real timer memory reader uses original r3 despite guest mutation");
    Check(h.rows[0].invocation.phase_call_id == row->entry_binding.phase_call_id &&
          h.rows[0].invocation.phase_thread_epoch == row->entry_binding.thread_epoch &&
          h.rows[0].invocation.phase_native_thread_id == row->entry_binding.native_thread_id &&
          h.rows[0].invocation.phase_present == 1 &&
          h.rows[0].invocation.timer_address == row->observation.identity.timer_address &&
          h.rows[0].invocation.caller_address == row->observation.identity.caller_address &&
          h.rows[0].invocation.native_thread_identity == row->observation.identity.thread_identity,
          "clock host invocation did not use the same timer entry binding");
    if (mode == Mode::Hostile) {
      Check(binding_calls == 2 && hostile_clock_calls == 2 && hostile_state_calls == 2,
            "no additional clock state or binding callbacks for clock forwarding");
      Check(row->entry_binding == callback_entry && row->exit_binding == callback_exit &&
            row->entry_binding != row->exit_binding && row->entry_binding == ScopeTuple(observed.active),
            "hostile different exit tuple retained while exact entry forwarded");
    } else Check(row->entry_binding == ScopeTuple(observed.active) && row->exit_binding == row->entry_binding,
                 "timer binding is exact current lexical phase tuple, not nearest or ancestor");
  }
  auto seen = std::make_unique<bool[]>(s.row_count);
  for (uint64_t i = 0; i < h.size && i < 1024; ++i) {
    const auto& scope = h.rows[i]; const bool zero = inert || i == 1;
    if (i == 1 && !observed.child) {
      Check(false, "unexpected host scope has no caller-owned body");
      continue;
    }
    const auto& b = i == 1 ? *observed.child : observed;
    Check(scope.storage_ordinal == i + 1 && scope.invocation.timer_ordinal == (zero ? 0u : 2u) &&
          scope.disposition == journal::Disposition::Completed &&
          scope.outcome == (b.unwind ? journal::Outcome::Unwound : journal::Outcome::Normal) && !scope.reserved &&
          scope.flags == ((b.unwind ? uint32_t(journal::kOriginalUnwound) : 0u) |
                          ((zero || phase_unknown) ? uint32_t(journal::kReceiptDiagnostic) : 0u)),
          "raw zero/admitted ordinal, storage and actual outcome all retained");
    Check(scope.trace.size == 2 && scope.trace.observed_reads == 2 && !scope.trace.lost_rows &&
          !scope.trace.overflow && !scope.trace.ordinal_saturated && !scope.trace.counter_saturated,
          "two actual read evaluations retain distinct ordinals without trace loss");
    if (zero) Check(!scope.invocation.phase_call_id && !scope.invocation.phase_thread_epoch &&
                     !scope.invocation.phase_native_thread_id && !scope.invocation.phase_present,
                     "NULL observer masks even a live parent Update identity");
    for (uint32_t j = 0; j < scope.trace.size && j < 16; ++j) {
      const auto& receipt = scope.trace.rows[j];
      const MclaClockWitnessBindingV1 expected_binding{MCLA_CW_ABI_V1, sizeof(MclaClockWitnessBindingV1),
          run_high, run_low, scope.invocation.timer_ordinal, j, scope.invocation.phase_call_id,
          scope.invocation.phase_thread_epoch, scope.invocation.phase_native_thread_id,
          scope.invocation.phase_present, 0, 0};
      Check(std::memcmp(&receipt.scope_binding, &expected_binding, sizeof(expected_binding)) == 0 &&
            std::memcmp(&receipt.request, &expected_binding, sizeof(expected_binding)) == 0,
            "complete request tuple matches raw host invocation without phase or run repair");
      Check(receipt.scope_binding.timer_invocation == (zero ? 0u : 2u) &&
            receipt.request.timer_invocation == (zero ? 0u : 2u) && receipt.request.read_ordinal == j &&
            receipt.trace_ordinal == j + 1 && j < 2 && receipt.result == b.values[j] && receipt.result_available &&
            receipt.used_query && receipt.status_available && receipt.read_ordinal_available && !receipt.threw,
            "actual return and each per-read identity preserved including inert calls");
      const uint32_t decisions = zero ? host::kUnboundTimer : phase_unknown ? host::kInvalidPhase : 0u;
      Check(receipt.status.abi_version == MCLA_CW_ABI_V1 && receipt.status.struct_size == sizeof(receipt.status) &&
            !receipt.status.reserved && receipt.decisions == decisions &&
            receipt.status.admission == (zero ? MCLA_CW_UNBOUND : MCLA_CW_ACCEPTED) &&
            receipt.status.completion == (zero ? MCLA_CW_NO_EVENT : MCLA_CW_PUBLISHED) &&
            receipt.status.issues == (phase_unknown && !zero ? 1u : 0u), "all raw guard and SDK diagnostic statuses retained");
      unsigned matches = 0, found = 0;
      for (uint32_t k = 0; k < s.row_count && k < 4096; ++k)
        if (std::memcmp(&receipt.request, &s.rows[k].binding, sizeof(receipt.request)) == 0) { ++matches; found = k; }
      Check(matches == (zero ? 0u : 1u), "SDK identity join is exact, never inherited from parent or selected by time");
      if (!zero && matches == 1) {
        Check(!seen[found] && s.rows[found].returned_guest == receipt.result &&
              s.rows[found].issues == receipt.status.issues && s.rows[found].disposition == MCLA_CW_PUBLISHED,
              "unique full binding joins actual SDK result and issues");
        seen[found] = true;
      }
    }
  }
  for (uint32_t i = 0; i < s.row_count && i < 4096; ++i)
    Check(seen[i] && s.rows[i].admission_ordinal == i + 1 && Arithmetic(s.rows[i]),
          "all SDK rows retain default-config unsigned branch arithmetic without filtering");
}
} // namespace

namespace mcla::guest_source_scout { bool Enabled() { return false; } }
REX_EXTERN(__imp__sub_821BDA90) { TimerOriginal(ctx, base); }
REX_EXTERN(__imp__sub_822C1FA8) { UpdateOriginal(ctx, base); }
REX_EXTERN(__imp__sub_82131008) { CallTimer(ctx, base); }
REX_EXTERN(__imp__sub_82305D00) { CallTimer(ctx, base); }
REX_EXTERN(__imp__sub_821BD910) { ctx.r0.u64 = 1; }
REX_EXTERN(__imp__sub_822C52C8) { ctx.r0.u64 = 2; }

int main(int argc, char** argv) {
  if (argc != 2) return 2;
  const std::string_view argument(argv[1]);
  if (argument == "healthy-update") mode = Mode::Healthy;
  else if (argument == "unwind-update") mode = Mode::Unwind;
  else if (argument == "null-observer") mode = Mode::NullObserver;
  else if (argument == "outside") mode = Mode::Outside;
  else if (argument == "render-child") mode = Mode::Render;
  else if (argument == "context-mismatch") mode = Mode::Context;
  else if (argument == "base-mismatch") mode = Mode::Base;
  else if (argument == "nested-null") mode = Mode::NestedNull;
  else if (argument == "hostile-update") mode = Mode::Hostile;
  else return 2;
  const auto saved = Native::Capture(); std::cout << std::unitbuf; std::cerr << std::unitbuf;
  try {
    const auto* flag = rex::cvar::GetFlagInfo("mcla_clock_read_witness_probe");
    Check(flag && flag->default_value == "false" && flag->getter && flag->getter() == "false" &&
          flag->lifecycle == rex::cvar::Lifecycle::kInitOnly, "actual new clock flag default OFF/init-only");
    Check(!arcade::Enabled() && !phase::Enabled() && !mcla::update_timing::Enabled(), "actual observers initially OFF");
    auto direct = std::make_unique<Body>(), observed = std::make_unique<Body>();
    auto direct_child = std::make_unique<Body>(), observed_child = std::make_unique<Body>();
    Initialize(*direct); Initialize(*observed); Initialize(*direct_child); Initialize(*observed_child);
    // Include object-representation padding in the differential deterministically.
    std::memcpy(&observed->context, &direct->context, sizeof(PPCContext));
    std::memcpy(&observed_child->context, &direct_child->context, sizeof(PPCContext));
    direct->unwind = observed->unwind = mode == Mode::Unwind;
    if (mode == Mode::NestedNull) { direct->child = direct_child.get(); observed->child = observed_child.get(); }
    // Original-only baseline is BEFORE Arm: its actual clock calls are outside
    // the bounded inventory, not retroactively fabricated as complete history.
    Run(*direct, false);
    Flag("mcla_arcade_timer_probe", true); Flag("mcla_phase_census_probe", true);
    Flag("mcla_arcade_update_binding_probe", true); Flag("mcla_clock_read_witness_probe", true);
    Check(sub_821BDA90 != __imp__sub_821BDA90 && sub_822C1FA8 != __imp__sub_822C1FA8 &&
          sub_82131008 != __imp__sub_82131008 && sub_82305D00 != __imp__sub_82305D00,
          "strong routes distinct from synthetic originals, not dispatcher attestation");
    LARGE_INTEGER frequency{};
    if (!QueryPerformanceFrequency(&frequency) || frequency.QuadPart <= 0 || frequency.QuadPart > 10000000000LL)
      throw std::runtime_error("fixture QPF unavailable");
    const auto begin = ad::Qpc(), qpf = uint64_t(frequency.QuadPart);
    if (!begin || begin > uint64_t(INT64_MAX) - qpf * 12) throw std::runtime_error("fixture window overflow");
    const auto end = begin + qpf * 12;
    // Explicit white-box init seam; the production Install(Runtime&) path and
    // dispatcher map are deliberately NOT claimed by this prerequisite.
    ad::InstallState(*ad::configured, true, sub_821BDA90);
    ad::configured->clock_enabled.store(true, std::memory_order_release); cg::Install();
    phase::Begin(begin, end, qpf, token); arcade::Begin(begin, end, qpf, token);
    auto* phase_state = pd::configured.load(); phase_state->clock = HostilePhaseClock;
    auto* timer_observer = ad::configured->observer.load();
    auto primer = std::make_unique<Body>(); Initialize(*primer);
    ad::MemoryArg memory{primer->bytes.data(), timer_address};
    timer_observer->invoke({timer_address, caller_address, GetCurrentThreadId()}, primer->context, memory,
        ad::binding_operations, 0, 0, [](PPCContext&, ad::MemoryArg&) {});
    Run(*observed, true); CheckBody(*direct, *observed);
    if (mode == Mode::NestedNull) CheckBody(*direct_child, *observed_child, true);
    Check(ad::Qpc() < end, "all bounded work inside window, not a temporal coverage proof");
    Check(phase_clock_calls == (mode == Mode::Outside ? 0u : mode == Mode::Render ? 4u : 2u) &&
          phase_state->active == 0 && !phase_state->failed.load() && !phase_state->invalid_qpc,
          "exact phase entry/exit clock inventory, no forwarding-generated phase calls");
    Check(phase_state->used == (mode == Mode::Outside ? 0u : mode == Mode::Render ? 2u : 1u) &&
          phase_state->original_exception == observed->unwind,
          "all actual lexical phase rows and deliberate unwind retained");
    const bool lexical_update = mode != Mode::Outside && mode != Mode::Render &&
                                mode != Mode::Context && mode != Mode::Base;
    Check(observed->active.bound() == lexical_update && !direct->active.bound(),
          "only identical-context/base current Update binds, no parent or baseline inference");
    if (lexical_update) {
      unsigned matches = 0;
      for (size_t i = 0; i < phase_state->used; ++i) {
        const auto& row = phase_state->scopes[i];
        if (row.call != observed->active.phase_call_id) continue;
        ++matches;
        Check(row.kind == pd::Kind::Update && row.completed && row.exception == observed->unwind &&
              row.thread_epoch == observed->active.thread_epoch && row.tid == observed->active.native_thread_id,
              "literal timer entry tuple names one actual completed lexical Update");
      }
      Check(matches == 1, "exact unique phase ID lookup, never nearest timestamp");
    }
    Check(pd::active_invocation == nullptr && pd::local.depth == 0 && host::detail::current == nullptr,
          "all actual phase and clock TLS scopes unwound");
    Check(mcla::update_timing::detail::configured.load() == nullptr,
          "strong Arcade route does not enable unrelated update-timing census");
    // White-box early cut, NOT production arcade::Finish's end-QPC/deadline
    // admission. Actual stop/freeze proves admitted Completion quiescence here;
    // this fixture does not wait12seconds or assert full-window coverage.
    timer_observer->stop_admissions(); auto timer_snapshot = std::make_unique<ad::Observer::Snapshot>();
    const bool drained = timer_observer->try_freeze_into(*timer_snapshot);
    Check(drained, "actual admitted timer Completion drain precedes clock Finish");
    if (!drained) throw std::runtime_error("fixture timer drain failed");
    Check(!phase_state->stopped.load(), "phase remains live through admitted timer drain");
    const auto timer_text = ad::Serialize(*timer_snapshot, token);
    const auto clock_text = cg::FinishAfterTimerDrain(token);
    auto* owned = cg::state.load(std::memory_order_acquire);
    if (!owned || !owned->host_snapshot || !owned->sdk_snapshot) throw std::runtime_error("fixture snapshots unavailable");
    ValidateRaw(*timer_snapshot, *owned->host_snapshot, *owned->sdk_snapshot, *observed);
    unsigned fake_clock = 0, waits = 0; const auto epoch = std::chrono::steady_clock::time_point{};
    const auto phase_text = pd::Drain(*phase_state, {}, [&] { return epoch + std::chrono::milliseconds(fake_clock++); },
                                      [&] { ++waits; });
    Check(!waits && phase_state->stopped.load(), "independent phase drain follows real timer and SDK inventories");
    Native{0, 0x1F80}.Restore(); EmitBody("direct", *direct); EmitBody("observed", *observed);
    if (mode == Mode::NestedNull) { EmitBody("direct-child", *direct_child); EmitBody("observed-child", *observed_child); }
    std::cout << "TIMER_JSON " << timer_text << '\n' << "PHASE_JSON " << phase_text << '\n'
              << "RAW_JSON " << clock_text << '\n';
    arcade::Disarm(); phase::Disarm(); saved.Restore();
    std::cout << "ARCADE_CLOCK_ROUTE mode=" << argument << " checks=" << checks << " failures=" << failures
              << " synthetic_generated_originals_actual_paired_SDK_no_game_no_history_no_frame_no_simulation_no_FPS\n";
    std::cout.flush(); std::cerr.flush(); return !std::cout || !std::cerr ? 2 : failures ? 1 : 0;
  } catch (const std::exception& error) {
    arcade::Disarm(); phase::Disarm(); saved.Restore(); std::cerr << "FAIL " << error.what() << '\n'; return 1;
  } catch (...) {
    arcade::Disarm(); phase::Disarm(); saved.Restore(); std::cerr << "FAIL unexpected fixture exception\n"; return 1;
  }
}
