#pragma once

// KI-060 Deck audio crackle, l87: pure parts of the XAudio engine-thread
// filter probe (mcla_audio_filter_probe, MCLA_AFILT v=1) and of the native
// replacement of the two-bank biquad effect sub_82146BE0
// (mcla_audio_filter_native, with the bit-for-bit self-check
// mcla_audio_filter_native_verify, MCLA_AFILT_VERIFY v=1). All three are
// default off. The hooks are in mcla_audio_filter.cpp; everything here is
// unit-tested (tests/mcla_audio_filter_test.cpp, which also runs the
// generated loop body of sub_82146BE0 as an interpreter and checks the
// offsets below against the generated code). Analysis:
// docs/evidence/M11-ki060-audio-filter-analysis-20261110.md; implementation:
// docs/evidence/M11-ki060-l87-implementation-20261110.md.
//
// Static layout of sub_82146BE0 (generated/default/mcla_recomp.0.cpp at the
// M11 codegen; line numbers are of that file):
// - r3 = effect object (`mr r31,r3`, :55202), r4 = buffer argument
//   (`mr r3,r4`, :55206; passed to sub_823EAF00 with r4 = r1+80, :55244).
//   sub_823EAF00 (mcla_recomp.25.cpp:34229) reads [arg-8+20], [arg-8+24] and
//   [arg-8+28] into r1+80/84/88; the loop pointer is r1+88 (`lwz r6,88(r1)`,
//   `addi r11,r6,8`, :55456/:55464), so the sample pointer is [arg+20].
// - Bank A ("low-pass") on when +80 < 23900 unsigned (`lwz r11,80(r31)`,
//   `li r10,23900`, subfc/subfe/clrlwi, :55204-55222); bank B on when
//   +76 != 0 (`lwz r9,76(r31)`, subfic/subfe/clrlwi, :55210-55230); neither
//   on: `li r3,0` and return without touching the object (:55234-55240).
// - Bank A words +4..+36 copied to the stack r1+144 (`addi r29,r31,4`,
//   `li r5,36`, `addi r3,r1,144`, memcpy sub_8244D5C0, :55248-55258); bank B
//   +40..+76 to r1+96 (`addi r28,r31,40`, :55259-55269). Per bank: five
//   coefficients c0..c4 (+0..+16) then four history words h0..h3
//   (+20..+32).
// - History reset: bank on and byte +100 (A) / +101 (B) != 0 stores f31
//   into the four stack history words (:55280-55324). f31 is the float at
//   0x82000ED4 (`lis r11,-32256`, `lfs f31,3796(r11)`, :55270-55277).
// - Coefficient refresh: +88 != +80 (re-read) calls sub_82146AD0 with
//   r3 = object, r4 = 1, r5 = +80, r6 = r1+144 (return 0x82146CE0), then
//   copies the five words to +4..+20 (:55327-55374); +84 != +76 calls it
//   with r4 = 0, r5 = +76, r6 = r1+96 (return 0x82146D28) and copies to
//   +40..+56 (:55377-55424).
// - Then (:55447-55472): +101 = (B off), +88 = +80, +84 = +76 (both
//   re-read), +100 = (A off); r10 = 32 iterations of 8 samples in place
//   (`addi r11,r11,32`, `bne 0x82146da0`, :56681-56688): 256 floats.
// - Per sample and bank (both banks the same arithmetic, A first): see
//   StepBank; every result is rounded to float (double(float(...))), fma
//   in double then rounded to float as the recompiled fmadds/fmsubs do.
// - After the loop, each of the eight stack history words is written back
//   (A: h0->+24, h2->+32, h1->+28, h3->+36; B: h0->+60, h2->+68,
//   h1->+64, h3->+72), replaced by f31 when its exponent bits are 0
//   (`rlwinm r10,r11,0,1,8` = & 0x7F800000, :56689-56880). Return r3 = 0
//   (:56883), r27-r31, f29-f31 and lr restored.

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>

namespace mcla::audio_filter {

// Guest functions.
inline constexpr uint32_t kFilterFunction = 0x82146BE0u;         // two-bank biquad
inline constexpr uint32_t kCoefficientFunction = 0x82146AD0u;    // coefficients
inline constexpr uint32_t kChannelFilterFunction = 0x82147738u;  // per-channel biquad
inline constexpr uint32_t kFrameFunction = 0x823EC248u;          // one engine frame
inline constexpr uint32_t kDescriptorFunction = 0x823EAF00u;
inline constexpr uint32_t kEngineLoopReturn = 0x823EC96Cu;  // engine thread call
inline constexpr uint32_t kCoefficientReturnA = 0x82146CE0u;
inline constexpr uint32_t kCoefficientReturnB = 0x82146D28u;

// Effect object layout (byte offsets from r3).
inline constexpr uint32_t kBankA = 4;
inline constexpr uint32_t kBankB = 40;
inline constexpr uint32_t kBankBytes = 36;  // memcpy length
inline constexpr uint32_t kCoefficients = 5;
inline constexpr uint32_t kHistoryWords = 4;
inline constexpr uint32_t kHistoryOffset = 20;  // within a bank
inline constexpr uint32_t kParamB = 76;
inline constexpr uint32_t kCutoffA = 80;
inline constexpr uint32_t kCacheB = 84;
inline constexpr uint32_t kCacheA = 88;
inline constexpr uint32_t kBypassedA = 100;
inline constexpr uint32_t kBypassedB = 101;
inline constexpr uint32_t kBypassCutoff = 23900;
// Sample pointer: [arg + 20] (sub_823EAF00: [arg - 8 + 28]).
inline constexpr uint32_t kDataPointerOffset = 20;
inline constexpr uint32_t kSamples = 256;
inline constexpr uint32_t kIterations = 32;
inline constexpr uint32_t kSamplesPerIteration = 8;
inline constexpr uint32_t kZeroConstant = 0x82000000u + 3796u;  // f31
// The original's frame (`stwu r1,-256(r1)`) and coefficient scratch.
inline constexpr uint32_t kFrameSize = 256;
inline constexpr uint32_t kStackCoefficientsA = 144;
inline constexpr uint32_t kStackCoefficientsB = 96;
inline constexpr uint32_t kExponentMask = 0x7F800000u;
// History write-back order (indices into h0..h3).
inline constexpr std::array<uint32_t, 4> kFlushOrder = {0, 2, 1, 3};

// 48 kHz / 256 samples per engine frame.
inline constexpr double kFramesPerSecondNeeded = 48000.0 / kSamples;

static_assert(kBankA + kBankBytes == kBankB && kBankB + kBankBytes == kParamB);
static_assert(kIterations * kSamplesPerIteration == kSamples);

constexpr bool BankAOn(uint32_t cutoff) { return cutoff < kBypassCutoff; }
constexpr bool BankBOn(uint32_t param) { return param != 0; }
constexpr bool ExponentZero(uint32_t bits) { return (bits & kExponentMask) == 0; }
constexpr bool Denormal(uint32_t bits) {
  return ExponentZero(bits) && (bits & 0x007FFFFFu) != 0;
}
constexpr uint32_t FlushHistoryWord(uint32_t bits, uint32_t zero_bits) {
  return ExponentZero(bits) ? zero_bits : bits;
}

inline float BitsToFloat(uint32_t bits) {
  float value;
  std::memcpy(&value, &bits, sizeof(value));
  return value;
}

inline uint32_t FloatToBits(float value) {
  uint32_t bits;
  std::memcpy(&bits, &value, sizeof(bits));
  return bits;
}

// One bank (raw guest words, host order).
struct Bank {
  std::array<uint32_t, kCoefficients> c{};
  std::array<uint32_t, kHistoryWords> h{};
};

// The fields of the effect object the filter reads or writes.
struct Object {
  Bank a;
  Bank b;
  uint32_t param_b = 0;   // +76
  uint32_t cutoff_a = 0;  // +80
  uint32_t cache_b = 0;   // +84
  uint32_t cache_a = 0;   // +88
  uint8_t bypassed_a = 0;  // +100
  uint8_t bypassed_b = 0;  // +101
};

// A bank in registers: the recompiled code holds every value as a double that
// is exactly a float (each result is double(float(...))), so keeping the
// history in doubles between samples is the same as its float stack slots.
struct BankState {
  double c0 = 0, c1 = 0, c2 = 0, c3 = 0, c4 = 0;
  double h0 = 0, h1 = 0, h2 = 0, h3 = 0;
};

// `double(float(v))`, the recompiled rounding of every fmuls/fadds/fmadds.
inline double Round(double v) { return static_cast<double>(static_cast<float>(v)); }

inline double Load(uint32_t bits) { return static_cast<double>(BitsToFloat(bits)); }

inline BankState LoadBank(const Bank &bank) {
  BankState s;
  s.c0 = Load(bank.c[0]);
  s.c1 = Load(bank.c[1]);
  s.c2 = Load(bank.c[2]);
  s.c3 = Load(bank.c[3]);
  s.c4 = Load(bank.c[4]);
  s.h0 = Load(bank.h[0]);
  s.h1 = Load(bank.h[1]);
  s.h2 = Load(bank.h[2]);
  s.h3 = Load(bank.h[3]);
  return s;
}

inline void StoreHistory(const BankState &s, Bank &bank) {
  bank.h[0] = FloatToBits(static_cast<float>(s.h0));
  bank.h[1] = FloatToBits(static_cast<float>(s.h1));
  bank.h[2] = FloatToBits(static_cast<float>(s.h2));
  bank.h[3] = FloatToBits(static_cast<float>(s.h3));
}

// One sample through one bank: two cascaded transposed direct-form II
// sections sharing c0..c4, in the recompiled operand order (bank A, sample 0
// at :55509-55566; every other sample and bank B the same, checked by the
// test's interpreter). Each line is one PPC instruction.
inline double StepBank(BankState &s, double x) {
  const double y1 = Round(std::fma(x, s.c0, s.h0));    // fmadds f5,f0,f13,f7
  const double y2 = Round(std::fma(y1, s.c0, s.h2));   // fmadds f13,f5,f13,f4
  const double p1 = Round(y1 * s.c3);                  // fmuls f1,f5,f6
  const double p2 = Round(y1 * s.c4);                  // fmuls f4,f5,f7
  const double p3 = Round(y2 * s.c3);                  // fmuls f30,f13,f6
  const double p4 = Round(y2 * s.c4);                  // fmuls f29,f13,f7
  const double u1 = Round(std::fma(x, s.c1, -p1));     // fmsubs f1,f0,f6,f1
  const double n1 = Round(std::fma(x, s.c2, -p2));     // fmsubs f0,f0,f7,f4
  const double u3 = Round(std::fma(y1, s.c1, -p3));    // fmsubs f13,f5,f6,f30
  const double n3 = Round(std::fma(y1, s.c2, -p4));    // fmsubs f7,f5,f7,f29
  s.h0 = Round(u1 + s.h1);                             // fadds f6,f1,f3
  s.h1 = n1;                                           // stfs f0,168(r1)
  s.h2 = Round(u3 + s.h3);                             // fadds f5,f13,f2
  s.h3 = n3;                                           // stfs f7,176(r1)
  return y2;
}

// Entry: a bank that is on and was bypassed last block starts from f31.
inline void ResetBypassedHistory(Object &o, bool a_on, bool b_on, uint32_t zero_bits) {
  if (a_on && o.bypassed_a != 0) {
    o.a.h.fill(zero_bits);
  }
  if (b_on && o.bypassed_b != 0) {
    o.b.h.fill(zero_bits);
  }
}

// Everything after the coefficient refresh: the flags and caches (with the
// values re-read at :55447/:55451), the 256 samples in place (raw float
// bits, host order) and the end-of-block history flush.
inline void Process(Object &o, bool a_on, bool b_on, uint32_t cutoff_now, uint32_t param_now,
                    uint32_t *samples, uint32_t zero_bits) {
  o.bypassed_b = b_on ? 0 : 1;
  o.cache_a = cutoff_now;
  o.cache_b = param_now;
  o.bypassed_a = a_on ? 0 : 1;
  BankState a = LoadBank(o.a);
  BankState b = LoadBank(o.b);
  if (a_on && b_on) {
    for (uint32_t i = 0; i < kSamples; ++i) {
      const double x = StepBank(b, StepBank(a, Load(samples[i])));
      samples[i] = FloatToBits(static_cast<float>(x));
    }
  } else if (a_on) {
    for (uint32_t i = 0; i < kSamples; ++i) {
      samples[i] = FloatToBits(static_cast<float>(StepBank(a, Load(samples[i]))));
    }
  } else if (b_on) {
    for (uint32_t i = 0; i < kSamples; ++i) {
      samples[i] = FloatToBits(static_cast<float>(StepBank(b, Load(samples[i]))));
    }
  }
  if (a_on) {
    StoreHistory(a, o.a);
  }
  if (b_on) {
    StoreHistory(b, o.b);
  }
  for (uint32_t k = 0; k < kHistoryWords; ++k) {
    o.a.h[k] = FlushHistoryWord(o.a.h[k], zero_bits);
    o.b.h[k] = FlushHistoryWord(o.b.h[k], zero_bits);
  }
}

// Whether a call refreshes coefficients (the native path then calls the
// guest sub_82146AD0; the self-check skips such calls).
constexpr bool NeedsRefresh(const Object &o) {
  return o.cache_a != o.cutoff_a || o.cache_b != o.param_b;
}

// The input block as the probe classifies it.
struct BlockClass {
  bool zero = true;       // every sample +-0
  bool denormal = false;  // any denormal sample or history word
};

inline BlockClass Classify(const uint32_t *samples, uint32_t count, const Object &o) {
  BlockClass c;
  for (uint32_t i = 0; i < count; ++i) {
    c.zero = c.zero && (samples[i] & 0x7FFFFFFFu) == 0;
    c.denormal = c.denormal || Denormal(samples[i]);
  }
  for (uint32_t k = 0; k < kHistoryWords; ++k) {
    c.denormal = c.denormal || Denormal(o.a.h[k]) || Denormal(o.b.h[k]);
  }
  return c;
}

// Bit-for-bit comparison of the native prediction with what the original
// wrote (self-check). Compared: both banks (nine words each), the two caches,
// the two bypass bytes and the 256 samples; +76/+80 are only read.
struct Mismatch {
  uint32_t count = 0;
  std::string field;  // first differing field
  uint32_t want = 0;
  uint32_t got = 0;
};

inline Mismatch Compare(const Object &want, const uint32_t *want_samples, const Object &got,
                        const uint32_t *got_samples) {
  Mismatch m;
  // The field name is built only for the first difference (index < 0: none).
  auto check = [&m](const char *field, int index, uint32_t a, uint32_t b) {
    if (a != b && m.count++ == 0) {
      m.field = index >= 0 ? field + std::to_string(index) : std::string(field);
      m.want = a;
      m.got = b;
    }
  };
  for (uint32_t k = 0; k < kCoefficients; ++k) {
    check("a.c", int(k), want.a.c[k], got.a.c[k]);
    check("b.c", int(k), want.b.c[k], got.b.c[k]);
  }
  for (uint32_t k = 0; k < kHistoryWords; ++k) {
    check("a.h", int(k), want.a.h[k], got.a.h[k]);
    check("b.h", int(k), want.b.h[k], got.b.h[k]);
  }
  check("cache_a", -1, want.cache_a, got.cache_a);
  check("cache_b", -1, want.cache_b, got.cache_b);
  check("bypassed_a", -1, want.bypassed_a, got.bypassed_a);
  check("bypassed_b", -1, want.bypassed_b, got.bypassed_b);
  for (uint32_t i = 0; i < kSamples; ++i) {
    check("sample", int(i), want_samples[i], got_samples[i]);
  }
  return m;
}

// MXCSR sticky bits the probe watches (denormal operand, underflow).
inline constexpr uint32_t kMxcsrDenormal = 0x0002u;
inline constexpr uint32_t kMxcsrUnderflow = 0x0010u;
inline constexpr uint32_t kMxcsrWatched = kMxcsrDenormal | kMxcsrUnderflow;
// Before the call: the watched bits cleared, everything else kept.
constexpr uint32_t MxcsrBefore(uint32_t saved) { return saved & ~kMxcsrWatched; }
// After the call: the callee's state with the caller's watched bits back.
constexpr uint32_t MxcsrAfter(uint32_t saved, uint32_t after) {
  return after | (saved & kMxcsrWatched);
}

// Per-function timing.
struct FunctionStats {
  uint64_t calls = 0;
  uint64_t ns = 0;
  uint64_t max_ns = 0;
  uint64_t de = 0;  // calls after which MXCSR DE was set
  uint64_t ue = 0;  // calls after which MXCSR UE was set
  void Add(uint64_t call_ns, bool denormal, bool underflow) {
    ++calls;
    ns += call_ns;
    max_ns = std::max(max_ns, call_ns);
    de += denormal ? 1 : 0;
    ue += underflow ? 1 : 0;
  }
  void Merge(const FunctionStats &o) {
    calls += o.calls;
    ns += o.ns;
    max_ns = std::max(max_ns, o.max_ns);
    de += o.de;
    ue += o.ue;
  }
};

struct Stats {
  uint64_t frames = 0;
  uint64_t other_frames = 0;  // frame calls not from the engine thread loop
  uint64_t frame_ns = 0;
  uint64_t frame_max_ns = 0;
  uint64_t filter_per_frame_max = 0;
  uint64_t channel_per_frame_max = 0;
  uint64_t outside = 0;  // filter calls outside any frame
  FunctionStats filter;   // sub_82146BE0
  FunctionStats channel;  // sub_82147738
  uint64_t a_only = 0;
  uint64_t b_only = 0;
  uint64_t both = 0;
  uint64_t none = 0;
  uint64_t zero = 0;
  uint64_t denormal = 0;
  // Native path and self-check.
  uint64_t native = 0;
  uint64_t refresh = 0;   // native calls that refreshed coefficients
  uint64_t fallback = 0;  // native on, original called (no buffer pointer)
  uint64_t verified = 0;
  uint64_t mismatches = 0;
  uint64_t verify_skipped = 0;  // sampled but refreshing: not compared

  void AddMix(bool a_on, bool b_on) {
    a_only += a_on && !b_on ? 1 : 0;
    b_only += !a_on && b_on ? 1 : 0;
    both += a_on && b_on ? 1 : 0;
    none += !a_on && !b_on ? 1 : 0;
  }
  void AddFrame(uint64_t ns, bool engine, uint64_t filter_calls, uint64_t channel_calls) {
    ++frames;
    other_frames += engine ? 0 : 1;
    frame_ns += ns;
    frame_max_ns = std::max(frame_max_ns, ns);
    filter_per_frame_max = std::max(filter_per_frame_max, filter_calls);
    channel_per_frame_max = std::max(channel_per_frame_max, channel_calls);
  }
  void Merge(const Stats &o) {
    frames += o.frames;
    other_frames += o.other_frames;
    frame_ns += o.frame_ns;
    frame_max_ns = std::max(frame_max_ns, o.frame_max_ns);
    filter_per_frame_max = std::max(filter_per_frame_max, o.filter_per_frame_max);
    channel_per_frame_max = std::max(channel_per_frame_max, o.channel_per_frame_max);
    outside += o.outside;
    filter.Merge(o.filter);
    channel.Merge(o.channel);
    a_only += o.a_only;
    b_only += o.b_only;
    both += o.both;
    none += o.none;
    zero += o.zero;
    denormal += o.denormal;
    native += o.native;
    refresh += o.refresh;
    fallback += o.fallback;
    verified += o.verified;
    mismatches += o.mismatches;
    verify_skipped += o.verify_skipped;
  }
};

inline double Ratio(uint64_t a, uint64_t b) { return b ? double(a) / double(b) : 0.0; }

// One MCLA_AFILT line for an interval of `interval_ns`.
inline std::string Line(const Stats &s, uint64_t interval_ns, bool native, bool verify) {
  const double seconds = double(interval_ns) / 1e9;
  const double fps = seconds > 0 ? double(s.frames) / seconds : 0.0;
  char text[2048];
  std::snprintf(
      text, sizeof(text),
      "MCLA_AFILT v=1 ms=%llu native=%d verify=%d frames=%llu other=%llu fps=%.2f "
      "need=%.1f fps_ratio=%.3f frame_ns_avg=%llu frame_ns_max=%llu busy=%.3f "
      "bq_calls=%llu bq_ns=%llu bq_ns_avg=%llu bq_ns_max=%llu bq_per_frame=%.2f "
      "bq_per_frame_max=%llu bq_a=%llu bq_b=%llu bq_ab=%llu bq_none=%llu bq_zero=%llu "
      "bq_denorm=%llu bq_de=%llu bq_ue=%llu outside=%llu ch_calls=%llu ch_ns=%llu "
      "ch_ns_max=%llu ch_per_frame=%.2f ch_per_frame_max=%llu ch_de=%llu ch_ue=%llu "
      "nat=%llu refresh=%llu fallback=%llu verified=%llu mismatch=%llu vskip=%llu",
      static_cast<unsigned long long>(interval_ns / 1000000), native ? 1 : 0, verify ? 1 : 0,
      static_cast<unsigned long long>(s.frames), static_cast<unsigned long long>(s.other_frames),
      fps, kFramesPerSecondNeeded, fps / kFramesPerSecondNeeded,
      static_cast<unsigned long long>(s.frames ? s.frame_ns / s.frames : 0),
      static_cast<unsigned long long>(s.frame_max_ns), Ratio(s.frame_ns, interval_ns),
      static_cast<unsigned long long>(s.filter.calls),
      static_cast<unsigned long long>(s.filter.ns),
      static_cast<unsigned long long>(s.filter.calls ? s.filter.ns / s.filter.calls : 0),
      static_cast<unsigned long long>(s.filter.max_ns),
      Ratio(s.filter.calls - std::min(s.filter.calls, s.outside), s.frames),
      static_cast<unsigned long long>(s.filter_per_frame_max),
      static_cast<unsigned long long>(s.a_only), static_cast<unsigned long long>(s.b_only),
      static_cast<unsigned long long>(s.both), static_cast<unsigned long long>(s.none),
      static_cast<unsigned long long>(s.zero), static_cast<unsigned long long>(s.denormal),
      static_cast<unsigned long long>(s.filter.de), static_cast<unsigned long long>(s.filter.ue),
      static_cast<unsigned long long>(s.outside),
      static_cast<unsigned long long>(s.channel.calls),
      static_cast<unsigned long long>(s.channel.ns),
      static_cast<unsigned long long>(s.channel.max_ns), Ratio(s.channel.calls, s.frames),
      static_cast<unsigned long long>(s.channel_per_frame_max),
      static_cast<unsigned long long>(s.channel.de),
      static_cast<unsigned long long>(s.channel.ue), static_cast<unsigned long long>(s.native),
      static_cast<unsigned long long>(s.refresh), static_cast<unsigned long long>(s.fallback),
      static_cast<unsigned long long>(s.verified), static_cast<unsigned long long>(s.mismatches),
      static_cast<unsigned long long>(s.verify_skipped));
  return text;
}

// One MCLA_AFILT_VERIFY line (the first few mismatches of a session).
inline std::string VerifyLine(uint64_t index, uint64_t verified, const Mismatch &m, bool a_on,
                              bool b_on) {
  char text[256];
  std::snprintf(text, sizeof(text),
                "MCLA_AFILT_VERIFY v=1 n=%llu call=%llu a=%d b=%d diffs=%u field=%s "
                "want=0x%08X got=0x%08X",
                static_cast<unsigned long long>(index),
                static_cast<unsigned long long>(verified), a_on ? 1 : 0, b_on ? 1 : 0, m.count,
                m.field.c_str(), m.want, m.got);
  return text;
}

// The F10 "audio_filter" object (session totals).
inline std::string StateJson(const Stats &s, uint64_t session_ns, bool probe, bool native,
                             bool verify) {
  const double seconds = double(session_ns) / 1e9;
  const double fps = seconds > 0 ? double(s.frames) / seconds : 0.0;
  char text[2048];
  std::snprintf(
      text, sizeof(text),
      "{\"version\": 1, \"probe\": %s, \"native\": %s, \"verify\": %s, \"seconds\": %.1f, "
      "\"frames\": %llu, \"other_frames\": %llu, \"fps\": %.2f, \"fps_needed\": %.1f, "
      "\"frame_ns_avg\": %llu, \"frame_ns_max\": %llu, \"busy\": %.3f, "
      "\"filter\": {\"calls\": %llu, \"ns\": %llu, \"ns_max\": %llu, \"per_frame_max\": %llu, "
      "\"a_only\": %llu, \"b_only\": %llu, \"both\": %llu, \"none\": %llu, \"zero\": %llu, "
      "\"denormal\": %llu, \"mxcsr_de\": %llu, \"mxcsr_ue\": %llu, \"outside\": %llu}, "
      "\"channel\": {\"calls\": %llu, \"ns\": %llu, \"ns_max\": %llu, \"per_frame_max\": %llu, "
      "\"mxcsr_de\": %llu, \"mxcsr_ue\": %llu}, "
      "\"native_calls\": %llu, \"refresh\": %llu, \"fallback\": %llu, \"verified\": %llu, "
      "\"mismatches\": %llu, \"verify_skipped\": %llu}",
      probe ? "true" : "false", native ? "true" : "false", verify ? "true" : "false", seconds,
      static_cast<unsigned long long>(s.frames), static_cast<unsigned long long>(s.other_frames),
      fps, kFramesPerSecondNeeded,
      static_cast<unsigned long long>(s.frames ? s.frame_ns / s.frames : 0),
      static_cast<unsigned long long>(s.frame_max_ns), Ratio(s.frame_ns, session_ns),
      static_cast<unsigned long long>(s.filter.calls),
      static_cast<unsigned long long>(s.filter.ns),
      static_cast<unsigned long long>(s.filter.max_ns),
      static_cast<unsigned long long>(s.filter_per_frame_max),
      static_cast<unsigned long long>(s.a_only), static_cast<unsigned long long>(s.b_only),
      static_cast<unsigned long long>(s.both), static_cast<unsigned long long>(s.none),
      static_cast<unsigned long long>(s.zero), static_cast<unsigned long long>(s.denormal),
      static_cast<unsigned long long>(s.filter.de), static_cast<unsigned long long>(s.filter.ue),
      static_cast<unsigned long long>(s.outside),
      static_cast<unsigned long long>(s.channel.calls),
      static_cast<unsigned long long>(s.channel.ns),
      static_cast<unsigned long long>(s.channel.max_ns),
      static_cast<unsigned long long>(s.channel_per_frame_max),
      static_cast<unsigned long long>(s.channel.de),
      static_cast<unsigned long long>(s.channel.ue), static_cast<unsigned long long>(s.native),
      static_cast<unsigned long long>(s.refresh), static_cast<unsigned long long>(s.fallback),
      static_cast<unsigned long long>(s.verified), static_cast<unsigned long long>(s.mismatches),
      static_cast<unsigned long long>(s.verify_skipped));
  return text;
}

// Self-check sampling: one call in kVerifyEvery; at most kVerifyLines lines.
inline constexpr uint64_t kVerifyEvery = 64;
inline constexpr uint64_t kVerifyLines = 8;
inline constexpr uint64_t kLineIntervalNs = 1000000000ull;

}  // namespace mcla::audio_filter

// The F10 state block "audio_filter"; empty when all three switches are off.
std::string MclaAudioFilterStateJson();
