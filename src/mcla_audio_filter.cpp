// KI-060 Deck audio crackle, l87: XAudio engine-thread filter probe
// (MCLA_AFILT v=1) and the native replacement of sub_82146BE0 with its
// bit-for-bit self-check (MCLA_AFILT_VERIFY v=1). See mcla_audio_filter.h for
// the guest layout and its citations, and
// docs/evidence/M11-ki060-l87-implementation-20261110.md.
//
// Three strong overrides; with the three init-only switches off (the
// default) each is one static bool test and the original call:
//   sub_823EC248 (one engine frame): times the frame, counts the filter calls
//     made during it and, with the probe on, writes one MCLA_AFILT line per
//     second of frames;
//   sub_82147738 (per-channel biquad): pass-through, timed (probe);
//   sub_82146BE0 (two-bank biquad): timed and classified (probe); with
//     mcla_audio_filter_native the native replacement runs instead of the
//     original; with mcla_audio_filter_native_verify one call in 64 runs the
//     ORIGINAL and is compared bit for bit with the native prediction made
//     from a host copy of its inputs.
// The probe reads only guest words the original itself reads (the object
// fields, the history and the 256 samples) and writes none; it clears the
// MXCSR DE/UE sticky bits before a call and puts the caller's bits back
// after reading them. The native path writes only what the original writes
// (object fields, samples) plus, when coefficients are refreshed, the
// original's own stack frame region (back chain and the coefficient scratch
// that the guest sub_82146AD0 fills).

#include "mcla_audio_filter.h"

#include "mcla_init.h"
#include "mcla_logging.h"

#include <rex/cvar.h>

#include <array>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <mutex>
#include <string>

#if defined(__x86_64__) || defined(_M_X64)
#include <xmmintrin.h>
#define MCLA_AFILT_HAVE_MXCSR 1
#else
#define MCLA_AFILT_HAVE_MXCSR 0
#endif

REXCVAR_DEFINE_BOOL(mcla_audio_filter_probe, false, "MCLA",
    "Log the XAudio engine-thread filter cost once per second (KI-060; "
    "MCLA_AFILT lines and the F10 audio_filter block; diagnostic, read-only)")
    .lifecycle(rex::cvar::Lifecycle::kInitOnly);

REXCVAR_DEFINE_BOOL(mcla_audio_filter_native, false, "MCLA",
    "Run the two-bank voice filter sub_82146BE0 natively (KI-060; same "
    "arithmetic and rounding as the recompiled code)")
    .lifecycle(rex::cvar::Lifecycle::kInitOnly);

REXCVAR_DEFINE_BOOL(mcla_audio_filter_native_verify, false, "MCLA",
    "Compare the native voice filter with the original bit for bit on one "
    "call in 64 (KI-060; MCLA_AFILT_VERIFY lines on a mismatch)")
    .lifecycle(rex::cvar::Lifecycle::kInitOnly);

namespace {

namespace af = mcla::audio_filter;

bool ProbeOn() {
  static const bool on = REXCVAR_GET(mcla_audio_filter_probe);
  return on;
}

bool NativeOn() {
  static const bool on = REXCVAR_GET(mcla_audio_filter_native);
  return on;
}

bool VerifyOn() {
  static const bool on = REXCVAR_GET(mcla_audio_filter_native_verify);
  return on;
}

bool AnyOn() {
  static const bool on = ProbeOn() || NativeOn() || VerifyOn();
  return on;
}

// steady_clock is QueryPerformanceCounter with the MSVC STL (the clock the
// host's other timing probes use).
uint64_t NowNs() {
  return static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(
                                   std::chrono::steady_clock::now().time_since_epoch())
                                   .count());
}

struct Shared {
  std::mutex mutex;
  af::Stats interval;
  af::Stats total;
  uint64_t start_ns = 0;     // session (first frame)
  uint64_t interval_ns = 0;  // start of the interval
  bool start_logged = false;
  std::atomic<uint64_t> verify_counter{0};
  std::atomic<uint64_t> verify_lines{0};
};

Shared &State() {
  static Shared state;
  return state;
}

// Per thread: counters of the frame being run (merged at its end) and the
// filter calls made in it.
struct Local {
  int depth = 0;
  uint64_t filter_calls = 0;
  uint64_t channel_calls = 0;
  af::Stats stats;
};
thread_local Local local;

// Runs `fn` and adds the call to `stats` (probe).
template <typename Fn>
void Timed(PPCContext &ctx, af::FunctionStats &stats, Fn fn) {
  // Both functions start with ctx.fpscr.disableFlushMode() before any FP
  // instruction; doing it first keeps the SDK's cached MXCSR from bringing
  // back old sticky bits inside the call.
  ctx.fpscr.disableFlushMode();
#if MCLA_AFILT_HAVE_MXCSR
  const uint32_t saved = _mm_getcsr();
  _mm_setcsr(af::MxcsrBefore(saved));
#endif
  const uint64_t begin = NowNs();
  fn();
  const uint64_t ns = NowNs() - begin;
#if MCLA_AFILT_HAVE_MXCSR
  const uint32_t after = _mm_getcsr();
  _mm_setcsr(af::MxcsrAfter(saved, after));
  stats.Add(ns, (after & af::kMxcsrDenormal) != 0, (after & af::kMxcsrUnderflow) != 0);
#else
  stats.Add(ns, false, false);
#endif
}

// Applies `fn` to the per-thread counters inside a frame, or to the shared
// interval otherwise (calls outside a frame are rare).
template <typename Fn>
void Count(bool outside_counts, Fn fn) {
  if (local.depth > 0) {
    fn(local.stats);
    return;
  }
  auto &s = State();
  std::lock_guard guard(s.mutex);
  if (outside_counts) {
    ++s.interval.outside;
  }
  fn(s.interval);
}

af::Object ReadObject(uint8_t *base, uint32_t obj) {
  af::Object o;
  for (uint32_t k = 0; k < af::kCoefficients; ++k) {
    o.a.c[k] = REX_LOAD_U32(obj + af::kBankA + 4 * k);
    o.b.c[k] = REX_LOAD_U32(obj + af::kBankB + 4 * k);
  }
  for (uint32_t k = 0; k < af::kHistoryWords; ++k) {
    o.a.h[k] = REX_LOAD_U32(obj + af::kBankA + af::kHistoryOffset + 4 * k);
    o.b.h[k] = REX_LOAD_U32(obj + af::kBankB + af::kHistoryOffset + 4 * k);
  }
  o.param_b = REX_LOAD_U32(obj + af::kParamB);
  o.cutoff_a = REX_LOAD_U32(obj + af::kCutoffA);
  o.cache_b = REX_LOAD_U32(obj + af::kCacheB);
  o.cache_a = REX_LOAD_U32(obj + af::kCacheA);
  o.bypassed_a = REX_LOAD_U8(obj + af::kBypassedA);
  o.bypassed_b = REX_LOAD_U8(obj + af::kBypassedB);
  return o;
}

void ReadSamples(uint8_t *base, uint32_t data, uint32_t *samples) {
  for (uint32_t i = 0; i < af::kSamples; ++i) {
    samples[i] = REX_LOAD_U32(data + 4 * i);
  }
}

// The guest sub_82146AD0 for one bank, as the original calls it (r3 =
// object, r4 = 1 for bank A / 0 for bank B, r5 = the cutoff word, r6 = the
// coefficient scratch in the caller's frame); returns the five words it wrote.
std::array<uint32_t, af::kCoefficients> Refresh(PPCContext &ctx, uint8_t *base, uint32_t obj,
                                                bool bank_a, uint32_t value, uint32_t frame) {
  const uint32_t scratch = frame + (bank_a ? af::kStackCoefficientsA : af::kStackCoefficientsB);
  ctx.r6.s64 = static_cast<int64_t>(scratch);
  ctx.r4.s64 = bank_a ? 1 : 0;
  ctx.r3.u64 = obj;
  ctx.r5.u64 = value;
  ctx.lr = bank_a ? af::kCoefficientReturnA : af::kCoefficientReturnB;
  sub_82146AD0(ctx, base);
  std::array<uint32_t, af::kCoefficients> words{};
  for (uint32_t k = 0; k < af::kCoefficients; ++k) {
    words[k] = REX_LOAD_U32(scratch + 4 * k);
  }
  return words;
}

// The native sub_82146BE0. False (nothing done) when the buffer pointer is
// missing; the caller then runs the original.
bool Native(PPCContext &ctx, uint8_t *base, af::Stats &stats) {
  ctx.fpscr.disableFlushMode();
  const uint32_t obj = ctx.r3.u32;
  const uint32_t arg = ctx.r4.u32;
  const bool a_on = af::BankAOn(REX_LOAD_U32(obj + af::kCutoffA));
  const bool b_on = af::BankBOn(REX_LOAD_U32(obj + af::kParamB));
  if (!a_on && !b_on) {
    ctx.r3.s64 = 0;
    ++stats.native;
    return true;
  }
  const uint32_t data = arg != 0 ? REX_LOAD_U32(arg + af::kDataPointerOffset) : 0;
  if (data == 0) {
    return false;
  }
  const uint32_t zero_bits = REX_LOAD_U32(af::kZeroConstant);
  af::Object o = ReadObject(base, obj);
  af::ResetBypassedHistory(o, a_on, b_on, zero_bits);
  // Coefficient refresh, in the original's frame (`stwu r1,-256(r1)`).
  const uint32_t cutoff = REX_LOAD_U32(obj + af::kCutoffA);
  const uint32_t cache_a = REX_LOAD_U32(obj + af::kCacheA);
  const uint32_t param = REX_LOAD_U32(obj + af::kParamB);
  const uint32_t cache_b = REX_LOAD_U32(obj + af::kCacheB);
  if (cache_a != cutoff || cache_b != param) {
    const uint64_t saved_r1 = ctx.r1.u64;
    const uint64_t saved_lr = ctx.lr;
    const uint32_t frame = ctx.r1.u32 - af::kFrameSize;
    REX_STORE_U32(frame, ctx.r1.u32);
    ctx.r1.u32 = frame;
    if (cache_a != cutoff) {
      o.a.c = Refresh(ctx, base, obj, true, cutoff, frame);
      for (uint32_t k = 0; k < af::kCoefficients; ++k) {
        REX_STORE_U32(obj + af::kBankA + 4 * k, o.a.c[k]);
      }
    }
    // The original re-reads +76 after the bank A refresh (:55377).
    const uint32_t param_now = REX_LOAD_U32(obj + af::kParamB);
    if (REX_LOAD_U32(obj + af::kCacheB) != param_now) {
      o.b.c = Refresh(ctx, base, obj, false, param_now, frame);
      for (uint32_t k = 0; k < af::kCoefficients; ++k) {
        REX_STORE_U32(obj + af::kBankB + 4 * k, o.b.c[k]);
      }
    }
    ctx.r1.u64 = saved_r1;
    ctx.lr = saved_lr;
    ++stats.refresh;
  }
  const uint32_t cutoff_now = REX_LOAD_U32(obj + af::kCutoffA);
  const uint32_t param_now = REX_LOAD_U32(obj + af::kParamB);
  std::array<uint32_t, af::kSamples> samples;
  ReadSamples(base, data, samples.data());
  af::Process(o, a_on, b_on, cutoff_now, param_now, samples.data(), zero_bits);
  // Stores in the original's order: flags and caches, samples, history.
  REX_STORE_U8(obj + af::kBypassedB, o.bypassed_b);
  REX_STORE_U32(obj + af::kCacheA, o.cache_a);
  REX_STORE_U32(obj + af::kCacheB, o.cache_b);
  REX_STORE_U8(obj + af::kBypassedA, o.bypassed_a);
  for (uint32_t i = 0; i < af::kSamples; ++i) {
    REX_STORE_U32(data + 4 * i, samples[i]);
  }
  for (const uint32_t k : af::kFlushOrder) {
    REX_STORE_U32(obj + af::kBankA + af::kHistoryOffset + 4 * k, o.a.h[k]);
  }
  for (const uint32_t k : af::kFlushOrder) {
    REX_STORE_U32(obj + af::kBankB + af::kHistoryOffset + 4 * k, o.b.h[k]);
  }
  ctx.r3.s64 = 0;
  ++stats.native;
  return true;
}

// Self-check: the original runs on the real state; the native prediction is
// made from a host copy of its inputs first. Calls that refresh
// coefficients (a guest call with its own side effects) are not compared.
void VerifiedOriginal(PPCContext &ctx, uint8_t *base, af::Stats &stats) {
  const uint32_t obj = ctx.r3.u32;
  const uint32_t arg = ctx.r4.u32;
  const bool a_on = af::BankAOn(REX_LOAD_U32(obj + af::kCutoffA));
  const bool b_on = af::BankBOn(REX_LOAD_U32(obj + af::kParamB));
  const uint32_t data = arg != 0 ? REX_LOAD_U32(arg + af::kDataPointerOffset) : 0;
  af::Object want = ReadObject(base, obj);
  if ((!a_on && !b_on) || data == 0 || af::NeedsRefresh(want)) {
    __imp__sub_82146BE0(ctx, base);
    ++stats.verify_skipped;
    return;
  }
  const uint32_t zero_bits = REX_LOAD_U32(af::kZeroConstant);
  std::array<uint32_t, af::kSamples> want_samples;
  ReadSamples(base, data, want_samples.data());
  af::ResetBypassedHistory(want, a_on, b_on, zero_bits);
  af::Process(want, a_on, b_on, want.cutoff_a, want.param_b, want_samples.data(), zero_bits);
  __imp__sub_82146BE0(ctx, base);
  const af::Object got = ReadObject(base, obj);
  std::array<uint32_t, af::kSamples> got_samples;
  ReadSamples(base, data, got_samples.data());
  const af::Mismatch m = af::Compare(want, want_samples.data(), got, got_samples.data());
  ++stats.verified;
  if (m.count == 0) {
    return;
  }
  ++stats.mismatches;
  auto &s = State();
  const uint64_t index = s.verify_lines.fetch_add(1, std::memory_order_relaxed);
  if (index < af::kVerifyLines) {
    MCLA_APP_INFO("{}", af::VerifyLine(index + 1, stats.verified, m, a_on, b_on));
  }
}

void RunFilter(PPCContext &ctx, uint8_t *base, af::Stats &stats) {
  if (VerifyOn() &&
      State().verify_counter.fetch_add(1, std::memory_order_relaxed) % af::kVerifyEvery == 0) {
    VerifiedOriginal(ctx, base, stats);
    return;
  }
  if (NativeOn()) {
    if (Native(ctx, base, stats)) {
      return;
    }
    ++stats.fallback;
  }
  __imp__sub_82146BE0(ctx, base);
}

// Probe classification of a call (before it runs).
void Classify(PPCContext &ctx, uint8_t *base, af::Stats &stats) {
  const uint32_t obj = ctx.r3.u32;
  const uint32_t arg = ctx.r4.u32;
  const bool a_on = af::BankAOn(REX_LOAD_U32(obj + af::kCutoffA));
  const bool b_on = af::BankBOn(REX_LOAD_U32(obj + af::kParamB));
  stats.AddMix(a_on, b_on);
  if ((!a_on && !b_on) || arg == 0) {
    return;
  }
  const uint32_t data = REX_LOAD_U32(arg + af::kDataPointerOffset);
  if (data == 0) {
    return;
  }
  std::array<uint32_t, af::kSamples> samples;
  ReadSamples(base, data, samples.data());
  const af::BlockClass c = af::Classify(samples.data(), af::kSamples, ReadObject(base, obj));
  stats.zero += c.zero ? 1 : 0;
  stats.denormal += c.denormal ? 1 : 0;
}

}  // namespace

// Two-bank voice filter: r3 = effect object, r4 = buffer argument.
REX_EXTERN(sub_82146BE0) {
  if (!AnyOn()) {
    __imp__sub_82146BE0(ctx, base);
    return;
  }
  ++local.filter_calls;
  af::Stats call;
  if (ProbeOn()) {
    Classify(ctx, base, call);
    Timed(ctx, call.filter, [&] { RunFilter(ctx, base, call); });
  } else {
    RunFilter(ctx, base, call);
  }
  Count(true, [&call](af::Stats &s) { s.Merge(call); });
}

// Per-channel biquad (pass-through).
REX_EXTERN(sub_82147738) {
  if (!ProbeOn()) {
    __imp__sub_82147738(ctx, base);
    return;
  }
  ++local.channel_calls;
  af::FunctionStats call;
  Timed(ctx, call, [&] { __imp__sub_82147738(ctx, base); });
  Count(false, [&call](af::Stats &s) { s.channel.Merge(call); });
}

// One engine frame: r3 = the engine object; the engine thread calls it from
// 0x823EC968 (return 0x823EC96C).
REX_EXTERN(sub_823EC248) {
  if (!AnyOn()) {
    __imp__sub_823EC248(ctx, base);
    return;
  }
  const bool engine = static_cast<uint32_t>(ctx.lr) == af::kEngineLoopReturn;
  if (local.depth++ == 0) {
    local.filter_calls = 0;
    local.channel_calls = 0;
  }
  const uint64_t begin = NowNs();
  __imp__sub_823EC248(ctx, base);
  const uint64_t end = NowNs();
  if (--local.depth > 0) {
    return;
  }
  local.stats.AddFrame(end - begin, engine, local.filter_calls, local.channel_calls);
  auto &s = State();
  std::string line;
  std::string start;
  {
    std::lock_guard guard(s.mutex);
    s.interval.Merge(local.stats);
    if (s.start_ns == 0) {
      s.start_ns = begin;
      s.interval_ns = begin;
    }
    if (!s.start_logged) {
      s.start_logged = true;
      start = "MCLA_AFILT v=1 kind=start probe=" + std::to_string(ProbeOn() ? 1 : 0) +
              " native=" + std::to_string(NativeOn() ? 1 : 0) +
              " verify=" + std::to_string(VerifyOn() ? 1 : 0) +
              " mxcsr=" + std::to_string(MCLA_AFILT_HAVE_MXCSR);
    }
    if (end - s.interval_ns >= af::kLineIntervalNs) {
      if (ProbeOn()) {
        line = af::Line(s.interval, end - s.interval_ns, NativeOn(), VerifyOn());
      }
      s.total.Merge(s.interval);
      s.interval = af::Stats{};
      s.interval_ns = end;
    }
  }
  local.stats = af::Stats{};
  if (!start.empty()) {
    MCLA_APP_INFO("{}", start);
  }
  if (!line.empty()) {
    MCLA_APP_INFO("{}", line);
  }
}

std::string MclaAudioFilterStateJson() {
  if (!AnyOn()) {
    return {};
  }
  auto &s = State();
  std::lock_guard guard(s.mutex);
  af::Stats all = s.total;
  all.Merge(s.interval);
  const uint64_t now = NowNs();
  return af::StateJson(all, s.start_ns ? now - s.start_ns : 0, ProbeOn(), NativeOn(),
                       VerifyOn());
}
