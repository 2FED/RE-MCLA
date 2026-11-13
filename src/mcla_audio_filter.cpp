// KI-060 Deck audio crackle, l87: XAudio engine-thread filter probe
// (MCLA_AFILT v=1) and the native replacement of sub_82146BE0 with its
// bit-for-bit self-check (MCLA_AFILT_VERIFY v=1). See mcla_audio_filter.h for
// the guest layout and its citations, and
// docs/evidence/M11-ki060-l87-implementation-20261110.md.
// l89 (docs/evidence/M11-ki060-l89-implementation-20261110.md): the native
// kernels without the library fma, the native sub_82147738 with its
// self-check (MCLA_AFILT_CH_VERIFY v=1), the `raced` classification and the
// remaining-leaf probe.
//
// l98 (docs/evidence/M11-l98-owner-build-20261113.md): the native filters are
// default on (audio_filter::kNativeDefault). With the probe and the
// self-check off (the default, af::Lean) the two filter hooks run the native
// kernels with no timing and no per-call statistics, the frame hook calls the
// original directly, and only fallbacks to the original are counted; one
// MCLA_AFILT kind=config line at start-up shows the switches.
//
// Strong overrides; with the init-only switches off (native off by the
// control key audio_filter_native_off) each is one static bool test and the
// original call:
//   sub_823EC248 (one engine frame): times the frame, counts the filter calls
//     made during it and, with the probe on, writes one MCLA_AFILT line per
//     second of frames;
//   sub_82147738 (per-channel biquad): timed (probe); with
//     mcla_audio_filter_native the native replacement runs instead of the
//     original (not with mcla_audio_filter_native_ch_off, the control); with
//     mcla_audio_filter_native_verify one call in 64 runs the ORIGINAL and is
//     compared bit for bit with a host prediction;
//   sub_82146BE0 (two-bank biquad): timed and classified (probe); with
//     mcla_audio_filter_native the native replacement runs instead of the
//     original; with mcla_audio_filter_native_verify one call in 64 runs the
//     ORIGINAL and is compared bit for bit with the native prediction made
//     from a host copy of its inputs;
//   sub_823F4450, sub_823EFD30, sub_823EE050 (probe only): pass-through,
//     counted, and timed inside an engine frame while their rate stays at or
//     below 50,000 calls per second.
// The probe reads only guest words the original itself reads (the object
// fields, the history and the 256 samples) and writes none; it clears the
// MXCSR DE/UE sticky bits before a call and puts the caller's bits back
// after reading them. The native paths write only what the originals write
// (object fields, samples, history words) plus, when sub_82146BE0 refreshes
// coefficients, the original's own stack frame region (back chain and the
// coefficient scratch that the guest sub_82146AD0 fills).

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
#include <vector>

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

REXCVAR_DEFINE_BOOL(mcla_audio_filter_native, mcla::audio_filter::kNativeDefault, "MCLA",
    "Run the voice filters sub_82146BE0 and sub_82147738 natively (KI-060; "
    "same arithmetic and rounding as the recompiled code; default on since l98)")
    .lifecycle(rex::cvar::Lifecycle::kInitOnly);

REXCVAR_DEFINE_BOOL(mcla_audio_filter_native_verify, false, "MCLA",
    "Compare the native voice filters with the originals bit for bit on one "
    "call in 64 each (KI-060; MCLA_AFILT_VERIFY / MCLA_AFILT_CH_VERIFY lines "
    "on a mismatch)")
    .lifecycle(rex::cvar::Lifecycle::kInitOnly);

REXCVAR_DEFINE_BOOL(mcla_audio_filter_native_ch_off, false, "MCLA",
    "Keep the per-channel filter sub_82147738 on the recompiled code while "
    "the native switches are on (KI-060 l89 control)")
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

bool ChannelOff() {
  static const bool on = REXCVAR_GET(mcla_audio_filter_native_ch_off);
  return on;
}

bool AnyOn() {
  static const bool on = ProbeOn() || NativeOn() || VerifyOn();
  return on;
}

bool ChannelNativeOn() {
  static const bool on = NativeOn() && !ChannelOff();
  return on;
}

bool ChannelVerifyOn() {
  static const bool on = VerifyOn() && !ChannelOff();
  return on;
}

bool ChannelAnyOn() {
  static const bool on = ProbeOn() || ChannelNativeOn() || ChannelVerifyOn();
  return on;
}

// l98: native on, probe and self-check off (the default).
bool LeanOn() {
  static const bool on = af::Lean(NativeOn(), VerifyOn(), ProbeOn());
  return on;
}

// steady_clock is QueryPerformanceCounter with the MSVC STL (the clock the
// host's other timing probes use).
uint64_t NowNs() {
  return static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(
                                   std::chrono::steady_clock::now().time_since_epoch())
                                   .count());
}

// The three probed leaves (l89).
enum LeafId { kMix, kAdd, kKick, kLeafCount };
constexpr std::array<af::PassStats af::Stats::*, kLeafCount> kLeafField = {
    &af::Stats::mix, &af::Stats::add, &af::Stats::kick};
constexpr std::array<uint32_t, kLeafCount> kLeafAddress = {
    af::kMixFunction, af::kAtomicAddFunction, af::kKickFunction};

struct Shared {
  std::mutex mutex;
  af::Stats interval;
  af::Stats total;
  uint64_t start_ns = 0;     // session (first frame)
  uint64_t interval_ns = 0;  // start of the interval
  bool start_logged = false;
  std::atomic<uint64_t> verify_counter{0};
  std::atomic<uint64_t> verify_lines{0};
  std::atomic<uint64_t> ch_verify_counter{0};
  std::atomic<uint64_t> ch_verify_lines{0};
  // l98 lean mode: fallbacks to the original (no buffer argument or sample
  // pointer), the only counters it keeps.
  std::atomic<uint64_t> lean_fallback{0};
  std::atomic<uint64_t> lean_ch_fallback{0};
  // Leaf calls outside an engine frame (any thread) and the timing switch.
  std::array<std::atomic<uint64_t>, kLeafCount> leaf_outside{};
  std::array<std::atomic<bool>, kLeafCount> leaf_timed{true, true, true};
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

// l98 lean mode: the native kernels' per-call counters land here and are not
// read (the lean mode keeps no statistics besides the fallbacks).
thread_local af::Stats lean_sink;

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

void WriteSamples(uint8_t *base, uint32_t data, const uint32_t *samples) {
  for (uint32_t i = 0; i < af::kSamples; ++i) {
    REX_STORE_U32(data + 4 * i, samples[i]);
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
  WriteSamples(base, data, samples.data());
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
// A difference that FilterRaced explains (another thread changed +80/+76
// between the copy and the original, l87) is counted `raced`, not a
// mismatch.
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
  const af::Object before = want;
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
  if (af::FilterRaced(before, got)) {
    ++stats.raced;
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

// --- sub_82147738 (l89) ---

af::ChannelInputs ReadChannelInputs(uint8_t *base, uint32_t obj) {
  af::ChannelInputs in;
  in.bypassed = REX_LOAD_U8(obj + af::kChannelBypassed);
  in.mask = REX_LOAD_U8(obj + af::kChannelMask);
  in.cascade = REX_LOAD_U8(obj + af::kChannelCascade);
  for (uint32_t k = 0; k < af::kCoefficients; ++k) {
    in.c[k] = REX_LOAD_U32(obj + af::kChannelCoefficients + 4 * k);
  }
  in.gain = REX_LOAD_U32(obj + af::kChannelGain);
  for (uint32_t k = 0; k < af::kHistoryWords; ++k) {
    in.history[k] = REX_LOAD_U32(obj + af::kChannelHistory[k]);
  }
  return in;
}

af::BankState ChannelCoefficients(const std::array<uint32_t, af::kCoefficients> &c) {
  af::BankState s;
  s.c0 = af::Load(c[0]);
  s.c1 = af::Load(c[1]);
  s.c2 = af::Load(c[2]);
  s.c3 = af::Load(c[3]);
  s.c4 = af::Load(c[4]);
  return s;
}

// The native sub_82147738. False (nothing done) when the buffer argument or
// the sample pointer is missing while the original would read through it;
// the caller then runs the original.
bool NativeChannel(PPCContext &ctx, uint8_t *base, af::Stats &stats) {
  ctx.fpscr.disableFlushMode();
  const uint32_t obj = ctx.r3.u32;
  const uint32_t arg = ctx.r4.u32;
  if (REX_LOAD_U8(obj + af::kChannelBypassed) != 0) {
    ctx.r3.s64 = 0;
    ++stats.ch_native;
    return true;
  }
  if (arg == 0) {
    return false;
  }
  const uint32_t count = REX_LOAD_U8(arg + af::kChannelCountOffset);
  const uint32_t data = REX_LOAD_U32(arg + af::kDataPointerOffset);
  std::array<uint32_t, af::kCoefficients> c;
  for (uint32_t k = 0; k < af::kCoefficients; ++k) {
    c[k] = REX_LOAD_U32(obj + af::kChannelCoefficients + 4 * k);
  }
  if (count == 0) {
    ctx.r3.s64 = 0;
    ++stats.ch_native;
    return true;
  }
  if (data == 0) {
    return false;
  }
  const uint32_t zero_bits = REX_LOAD_U32(af::kZeroConstant);
  const af::BankState coefficients = ChannelCoefficients(c);
  std::array<uint32_t, af::kSamples> samples;
  uint32_t slot = 0;  // r6
  for (uint32_t channel = 0; channel < count; ++channel) {
    if (!af::ChannelOn(REX_LOAD_U8(obj + af::kChannelMask), channel)) {
      continue;
    }
    // The original's load order: h2, h3, h0, h1 (:57196-57224).
    const uint32_t h2 = REX_LOAD_U32(obj + af::kChannelHistory[2]);
    const uint32_t h3 = REX_LOAD_U32(obj + af::kChannelHistory[3]);
    const uint32_t h0 = REX_LOAD_U32(obj + af::kChannelHistory[0]);
    const uint32_t h1 = REX_LOAD_U32(obj + af::kChannelHistory[1]);
    const bool cascade = REX_LOAD_U8(obj + af::kChannelCascade) != 0;
    af::BankState s = coefficients;
    s.h2 = af::Load(REX_LOAD_U32(h2 + slot));
    s.h3 = af::Load(REX_LOAD_U32(h3 + slot));
    s.h0 = af::Load(REX_LOAD_U32(h0 + slot));
    s.h1 = af::Load(REX_LOAD_U32(h1 + slot));
    const uint32_t block = data + af::kChannelStride * channel;
    ReadSamples(base, block, samples.data());
    af::ProcessChannel(s, cascade, samples.data(),
                       [&] { return REX_LOAD_U32(obj + af::kChannelGain); });
    WriteSamples(base, block, samples.data());
    const auto out = af::ChannelHistoryOut(s, zero_bits);
    for (uint32_t k = 0; k < af::kHistoryWords; ++k) {
      REX_STORE_U32(REX_LOAD_U32(obj + af::kChannelHistory[k]) + slot, out[k]);
    }
    slot += 4;
  }
  ctx.r3.s64 = 0;
  ++stats.ch_native;
  return true;
}

// Self-check of sub_82147738: a host prediction of every channel the mask
// selects (samples and the four history words), then the ORIGINAL on the
// real state, then a bit comparison. A difference while the object inputs
// or the call's count / sample pointer changed across the original's run is
// counted `ch_raced`.
void VerifiedChannel(PPCContext &ctx, uint8_t *base, af::Stats &stats) {
  const uint32_t obj = ctx.r3.u32;
  const uint32_t arg = ctx.r4.u32;
  const af::ChannelInputs before = ReadChannelInputs(base, obj);
  const uint32_t count = arg != 0 ? REX_LOAD_U8(arg + af::kChannelCountOffset) : 0;
  const uint32_t data = arg != 0 ? REX_LOAD_U32(arg + af::kDataPointerOffset) : 0;
  if (before.bypassed != 0 || count == 0 || data == 0) {
    __imp__sub_82147738(ctx, base);
    ++stats.ch_verify_skipped;
    return;
  }
  struct Prediction {
    uint32_t channel = 0;
    std::array<uint32_t, af::kHistoryWords> address{};
    std::array<uint32_t, af::kHistoryWords> history{};
    std::array<uint32_t, af::kSamples> samples{};
  };
  const uint32_t zero_bits = REX_LOAD_U32(af::kZeroConstant);
  const af::BankState coefficients = ChannelCoefficients(before.c);
  std::vector<Prediction> predictions;
  uint32_t slot = 0;
  for (uint32_t channel = 0; channel < count; ++channel) {
    if (!af::ChannelOn(before.mask, channel)) {
      continue;
    }
    Prediction &p = predictions.emplace_back();
    p.channel = channel;
    for (uint32_t k = 0; k < af::kHistoryWords; ++k) {
      p.address[k] = before.history[k] + slot;
    }
    af::BankState s = coefficients;
    s.h0 = af::Load(REX_LOAD_U32(p.address[0]));
    s.h1 = af::Load(REX_LOAD_U32(p.address[1]));
    s.h2 = af::Load(REX_LOAD_U32(p.address[2]));
    s.h3 = af::Load(REX_LOAD_U32(p.address[3]));
    ReadSamples(base, data + af::kChannelStride * channel, p.samples.data());
    af::ProcessChannel(s, before.cascade != 0, p.samples.data(), [&] { return before.gain; });
    p.history = af::ChannelHistoryOut(s, zero_bits);
    slot += 4;
  }
  __imp__sub_82147738(ctx, base);
  const af::ChannelInputs after = ReadChannelInputs(base, obj);
  const bool call_changed = REX_LOAD_U8(arg + af::kChannelCountOffset) != count ||
                            REX_LOAD_U32(arg + af::kDataPointerOffset) != data;
  af::Mismatch m;
  std::array<uint32_t, af::kSamples> got;
  for (const Prediction &p : predictions) {
    const std::string prefix = "ch" + std::to_string(p.channel) + ".";
    const std::string sample_name = prefix + "sample";
    const std::string history_name = prefix + "h";
    ReadSamples(base, data + af::kChannelStride * p.channel, got.data());
    for (uint32_t i = 0; i < af::kSamples; ++i) {
      m.Check(sample_name.c_str(), int(i), p.samples[i], got[i]);
    }
    for (uint32_t k = 0; k < af::kHistoryWords; ++k) {
      m.Check(history_name.c_str(), int(k), p.history[k], REX_LOAD_U32(p.address[k]));
    }
  }
  ++stats.ch_verified;
  if (m.count == 0) {
    return;
  }
  if (!(after == before) || call_changed) {
    ++stats.ch_raced;
    return;
  }
  ++stats.ch_mismatches;
  auto &s = State();
  const uint64_t index = s.ch_verify_lines.fetch_add(1, std::memory_order_relaxed);
  if (index < af::kVerifyLines) {
    MCLA_APP_INFO("{}", af::ChannelVerifyLine(index + 1, stats.ch_verified, m, count,
                                              before.mask, before.cascade != 0));
  }
}

void RunChannel(PPCContext &ctx, uint8_t *base, af::Stats &stats) {
  if (ChannelVerifyOn() &&
      State().ch_verify_counter.fetch_add(1, std::memory_order_relaxed) % af::kVerifyEvery ==
          0) {
    VerifiedChannel(ctx, base, stats);
    return;
  }
  if (ChannelNativeOn()) {
    if (NativeChannel(ctx, base, stats)) {
      return;
    }
    ++stats.ch_fallback;
  }
  __imp__sub_82147738(ctx, base);
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

// A probed leaf (l89): inside an engine frame of this thread counted and,
// while its timing is on, timed; outside one only counted.
template <typename Fn>
void RunLeaf(LeafId leaf, Fn fn) {
  auto &s = State();
  if (local.depth == 0) {
    fn();
    s.leaf_outside[leaf].fetch_add(1, std::memory_order_relaxed);
    return;
  }
  af::PassStats &p = local.stats.*kLeafField[leaf];
  ++p.calls;
  if (!s.leaf_timed[leaf].load(std::memory_order_relaxed)) {
    fn();
    return;
  }
  const uint64_t begin = NowNs();
  fn();
  p.ns += NowNs() - begin;
  ++p.timed;
}

}  // namespace

// Two-bank voice filter: r3 = effect object, r4 = buffer argument.
REX_EXTERN(sub_82146BE0) {
  if (LeanOn()) {
    if (Native(ctx, base, lean_sink)) {
      return;
    }
    State().lean_fallback.fetch_add(1, std::memory_order_relaxed);
    __imp__sub_82146BE0(ctx, base);
    return;
  }
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

// Per-channel biquad with gain: r3 = effect object, r4 = buffer argument.
REX_EXTERN(sub_82147738) {
  if (LeanOn()) {
    if (ChannelNativeOn()) {
      if (NativeChannel(ctx, base, lean_sink)) {
        return;
      }
      State().lean_ch_fallback.fetch_add(1, std::memory_order_relaxed);
    }
    __imp__sub_82147738(ctx, base);
    return;
  }
  if (!ChannelAnyOn()) {
    __imp__sub_82147738(ctx, base);
    return;
  }
  ++local.channel_calls;
  af::Stats call;
  if (ProbeOn()) {
    Timed(ctx, call.channel, [&] { RunChannel(ctx, base, call); });
  } else {
    ++call.channel.calls;
    RunChannel(ctx, base, call);
  }
  Count(false, [&call](af::Stats &s) { s.Merge(call); });
}

// The three remaining engine-thread leaves (probe only, pass-through).
REX_EXTERN(sub_823F4450) {
  if (!ProbeOn()) {
    __imp__sub_823F4450(ctx, base);
    return;
  }
  RunLeaf(kMix, [&] { __imp__sub_823F4450(ctx, base); });
}

REX_EXTERN(sub_823EFD30) {
  if (!ProbeOn()) {
    __imp__sub_823EFD30(ctx, base);
    return;
  }
  RunLeaf(kAdd, [&] { __imp__sub_823EFD30(ctx, base); });
}

REX_EXTERN(sub_823EE050) {
  if (!ProbeOn()) {
    __imp__sub_823EE050(ctx, base);
    return;
  }
  RunLeaf(kKick, [&] { __imp__sub_823EE050(ctx, base); });
}

// One engine frame: r3 = the engine object; the engine thread calls it from
// 0x823EC968 (return 0x823EC96C).
REX_EXTERN(sub_823EC248) {
  if (!AnyOn() || LeanOn()) {
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
  std::string untimed;
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
              " mxcsr=" + std::to_string(MCLA_AFILT_HAVE_MXCSR) +
              " ch_off=" + std::to_string(ChannelOff() ? 1 : 0) + " kernel=muladd";
    }
    if (end - s.interval_ns >= af::kLineIntervalNs) {
      const uint64_t interval = end - s.interval_ns;
      for (int leaf = 0; leaf < kLeafCount; ++leaf) {
        af::PassStats &p = s.interval.*kLeafField[leaf];
        p.outside += s.leaf_outside[leaf].exchange(0, std::memory_order_relaxed);
        // Too frequent to time without distorting it: count only from now on.
        const double rate = double(p.calls) * 1e9 / double(interval);
        if (ProbeOn() && rate > double(af::kPassTimedMaxPerSecond) &&
            s.leaf_timed[leaf].exchange(false, std::memory_order_relaxed)) {
          char text[128];
          std::snprintf(text, sizeof(text),
                        "MCLA_AFILT v=1 kind=untimed fn=%08X calls_per_s=%.0f limit=%llu",
                        kLeafAddress[leaf], rate,
                        static_cast<unsigned long long>(af::kPassTimedMaxPerSecond));
          untimed += untimed.empty() ? std::string(text) : std::string("\n") + text;
        }
      }
      if (ProbeOn()) {
        line = af::Line(s.interval, interval, NativeOn(), VerifyOn());
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
  if (!untimed.empty()) {
    MCLA_APP_INFO("{}", untimed);
  }
  if (!line.empty()) {
    MCLA_APP_INFO("{}", line);
  }
}

std::string MclaAudioFilterStateJson() {
  if (LeanOn()) {
    auto &s = State();
    return af::LeanStateJson(s.lean_fallback.load(std::memory_order_relaxed),
                             s.lean_ch_fallback.load(std::memory_order_relaxed), ChannelOff());
  }
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

void MclaAudioFilterLogConfig() {
  MCLA_APP_INFO("{}", af::ConfigLine(NativeOn(), VerifyOn(), ProbeOn(), ChannelOff()));
}
