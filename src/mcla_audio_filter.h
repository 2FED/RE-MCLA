#pragma once

// KI-060 Deck audio crackle, l87: pure parts of the XAudio engine-thread
// filter probe (mcla_audio_filter_probe, MCLA_AFILT v=1) and of the native
// replacement of the two-bank biquad effect sub_82146BE0
// (mcla_audio_filter_native, with the bit-for-bit self-check
// mcla_audio_filter_native_verify, MCLA_AFILT_VERIFY v=1). The probe and the
// self-check are default off; the native filters are default on since l98
// (kNativeDefault). The hooks are in mcla_audio_filter.cpp; everything here is
// unit-tested (tests/mcla_audio_filter_test.cpp, which also runs the
// generated loop body of sub_82146BE0 as an interpreter and checks the
// offsets below against the generated code). Analysis:
// docs/evidence/M11-ki060-audio-filter-analysis-20261110.md; implementation:
// docs/evidence/M11-ki060-l87-implementation-20261110.md.
//
// l89 (docs/evidence/M11-ki060-l89-implementation-20261110.md): both native
// kernels use MulAdd instead of the library fma (bit-identical, proof at
// MulAdd); mcla_audio_filter_native also runs the per-channel biquad
// sub_82147738 natively (layout below StepSection), unless
// mcla_audio_filter_native_ch_off; the self-check covers both functions and
// counts a difference explained by a concurrent input change as `raced`;
// the probe also times the three remaining engine-thread leaves
// sub_823F4450, sub_823EFD30 and sub_823EE050.
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
#include <utility>

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
// l89 probe: the remaining engine-thread leaves (no calls of their own).
inline constexpr uint32_t kMixFunction = 0x823F4450u;        // gain-ramp mixer
inline constexpr uint32_t kAtomicAddFunction = 0x823EFD30u;  // lwarx/stwcx. add
inline constexpr uint32_t kKickFunction = 0x823EE050u;       // XMA unlock and kick

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

// Static layout of sub_82147738 (mcla_recomp.0.cpp:57113-57851, l89):
// - r3 = effect object (`mr r31,r3`, :57135), r4 = buffer argument
//   (`mr r3,r4`, :57137). Byte +68 != 0: return r3 = 0 at once (:57139-57143,
//   loc_82147BFC :57830).
// - sub_823EAF00 with r4 = r1+96 (:57144-57148) copies [arg+12], [arg+16],
//   [arg+20] to r1+96/100/104: the channel count is `lbz r10,97(r1)`
//   (:57150), i.e. the byte at arg+13; the sample pointer is
//   `lwz r11,104(r1)` (:57207) = [arg+20], as for sub_82146BE0.
// - Coefficients c0..c4 = floats +12..+28 in f0, f13, f12, f11, f10, read
//   once (:57151-57170); count 0: return (:57171). f5 = the float at
//   0x82000ED4 (:57173-57183).
// - Channel loop r4 = 0.. count-1 (`addi r4,r4,1`, `cmplw`, `blt`,
//   :57822-57829), sample block data + 1024 * channel (`addi r5,r5,1024`,
//   `add r11,r5,r11`); a channel runs when bit (1 << r4, slw) & byte +74
//   (re-read per channel, :57187-57195). Its history words are
//   [[+32] + r6] (h0), [[+36] + r6] (h1), [[+40] + r6] (h2), [[+44] + r6]
//   (h3), r6 = 4 * the number of channels run before (`addi r6,r6,4` only on
//   that path, :57817); loads :57196-57224.
// - Byte +75 (per channel, :57204) != 0: two cascaded sections, loop
//   loc_821477FC (:57235-57531), the arithmetic of StepBank; == 0: one
//   section, loop loc_82147A20 (:57540-57734), StepSection. Both 32 x 8
//   samples in place, each output `fmuls` by the gain float +60, re-read
//   before every sample's output (`lfs fN,60(r31)`).
// - Then (loc_82147B6C, :57735-57817) h0, h1, h2, h3 are stored in that
//   order to the four arrays (pointers re-read), each replaced by f5 when its
//   exponent bits are 0. Return r3 = 0 (:57832), f25-f31, r30, r31, lr
//   restored. The object is only read.
inline constexpr uint32_t kChannelBypassed = 68;      // byte
inline constexpr uint32_t kChannelCoefficients = 12;  // c0..c4 at +12..+28
inline constexpr std::array<uint32_t, 4> kChannelHistory = {32, 36, 40, 44};  // h0..h3
inline constexpr uint32_t kChannelGain = 60;
inline constexpr uint32_t kChannelMask = 74;     // byte
inline constexpr uint32_t kChannelCascade = 75;  // byte
inline constexpr uint32_t kChannelCountOffset = 13;  // byte [arg + 13]
inline constexpr uint32_t kChannelStride = 1024;
inline constexpr uint32_t kChannelFrameSize = 192;  // `stwu r1,-192(r1)`

// 48 kHz / 256 samples per engine frame.
inline constexpr double kFramesPerSecondNeeded = 48000.0 / kSamples;

static_assert(kBankA + kBankBytes == kBankB && kBankB + kBankBytes == kParamB);
static_assert(kIterations * kSamplesPerIteration == kSamples);
static_assert(kSamples * 4 <= kChannelStride);

constexpr bool BankAOn(uint32_t cutoff) { return cutoff < kBypassCutoff; }
constexpr bool BankBOn(uint32_t param) { return param != 0; }
constexpr bool ExponentZero(uint32_t bits) { return (bits & kExponentMask) == 0; }
constexpr bool Denormal(uint32_t bits) {
  return ExponentZero(bits) && (bits & 0x007FFFFFu) != 0;
}
constexpr uint32_t FlushHistoryWord(uint32_t bits, uint32_t zero_bits) {
  return ExponentZero(bits) ? zero_bits : bits;
}
// `slw r10,r3,r4` with r3 = 1 (shift amount r4 & 0x3F; 32..63 give 0), and
// `and r9,r10,r11` with the mask byte (:57189-57195).
constexpr bool ChannelOn(uint8_t mask, uint32_t channel) {
  const uint32_t amount = channel & 0x3Fu;
  const uint32_t bit = (amount & 0x20u) != 0 ? 0u : (1u << amount);
  return (bit & mask) != 0;
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

// l89: the multiply-add of the native kernels, without the library fma.
//
// The recompiled fmadds/fmsubs are `double(float(std::fma(a, b, +-c)))`.
// mcla.exe is built -O3 -msse4.1 (no -mfma), so std::fma is a call to the CRT
// (the Wine builtin under Proton). In both kernels every operand a, b, c is
// a double that holds a float exactly (each register is written only by
// lfs/lfsx `double(temp.f32)`, by `double(float(...))` or by fmr of such a
// register; the unit test checks this for every statement of both generated
// loops). Then:
// 1. a * b is exact in double: the significands have at most 24 bits each,
//    so the product has at most 48 <= 53; its magnitude is 0 or within
//    [2^-298, 2^256), inside the normal double range, so it neither rounds,
//    overflows nor underflows.
// 2. fma(a, b, c) = round64(a*b + c) (one rounding of the exact sum).
//    a * b + c in double = round64(round64(a*b) + c) = round64(a*b + c) by 1.
//    This holds in every rounding mode, and the sign of an exact zero sum is
//    decided by the same operands (the exact product is the rounded one).
// 3. The result then goes through the same round32 (`double(float(...))`),
//    so the float bits are equal. NaN payload selection may differ (the
//    known residual of the l87 kernel, unreachable for finite audio).
// The product and the sum are separate statements and contraction is off
// here, so no fused instruction is formed even if a build adds -mfma (a fused
// result would be identical anyway, by 1). The M7-016 Proton fma
// differences need |result| = 2^-1075, which float operands cannot reach.
inline double MulAdd(double a, double b, double c) {
#if defined(__clang__)
#pragma clang fp contract(off)
#endif
  const double product = a * b;
  return product + c;
}

// The l87 kernels' library form (the recompiled code's call), kept for the
// unit test and the benchmark; the hooks never use it.
inline double LibraryFma(double a, double b, double c) { return std::fma(a, b, c); }

using MulAddFn = double (*)(double, double, double);

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
// test's interpreter). Each line is one PPC instruction. `Fma` is MulAdd in
// the hooks (l89) and LibraryFma in the l87 reference (test, benchmark).
template <MulAddFn Fma = MulAdd>
inline double StepBank(BankState &s, double x) {
  const double y1 = Round(Fma(x, s.c0, s.h0));    // fmadds f5,f0,f13,f7
  const double y2 = Round(Fma(y1, s.c0, s.h2));   // fmadds f13,f5,f13,f4
  const double p1 = Round(y1 * s.c3);             // fmuls f1,f5,f6
  const double p2 = Round(y1 * s.c4);             // fmuls f4,f5,f7
  const double p3 = Round(y2 * s.c3);             // fmuls f30,f13,f6
  const double p4 = Round(y2 * s.c4);             // fmuls f29,f13,f7
  const double u1 = Round(Fma(x, s.c1, -p1));     // fmsubs f1,f0,f6,f1
  const double n1 = Round(Fma(x, s.c2, -p2));     // fmsubs f0,f0,f7,f4
  const double u3 = Round(Fma(y1, s.c1, -p3));    // fmsubs f13,f5,f6,f30
  const double n3 = Round(Fma(y1, s.c2, -p4));    // fmsubs f7,f5,f7,f29
  s.h0 = Round(u1 + s.h1);                        // fadds f6,f1,f3
  s.h1 = n1;                                      // stfs f0,168(r1)
  s.h2 = Round(u3 + s.h3);                        // fadds f5,f13,f2
  s.h3 = n3;                                      // stfs f7,176(r1)
  return y2;
}

// One sample through the single section of sub_82147738 (byte +75 == 0, loop
// loc_82147A20, sample 0 at :57550-57593): the first half of StepBank; h2
// and h3 are not touched.
template <MulAddFn Fma = MulAdd>
inline double StepSection(BankState &s, double x) {
  const double y = Round(Fma(x, s.c0, s.h0));   // fmadds f3,f4,f0,f9
  const double p1 = Round(y * s.c3);            // fmuls f26,f3,f11
  const double p2 = Round(y * s.c4);            // fmuls f25,f3,f10
  const double u = Round(Fma(x, s.c1, -p1));    // fmsubs f28,f4,f13,f26
  const double n = Round(Fma(x, s.c2, -p2));    // fmsubs f4,f4,f12,f25
  s.h0 = Round(u + s.h1);                       // fadds f8,f28,f8
  s.h1 = n;
  return y;
}

// The 256 samples of one sub_82147738 channel in place (raw float bits, host
// order): the section(s), then `fmuls` by the gain that `gain()` returns
// (the original re-reads +60 for every output, so the hook passes a guest
// load). The history is in `s` (c0..c4 the object's +12..+28).
template <MulAddFn Fma = MulAdd, typename Gain>
inline void ProcessChannel(BankState &s, bool cascade, uint32_t *samples, Gain &&gain) {
  if (cascade) {
    for (uint32_t i = 0; i < kSamples; ++i) {
      const double y = StepBank<Fma>(s, Load(samples[i]));
      samples[i] = FloatToBits(static_cast<float>(y * Load(gain())));
    }
  } else {
    for (uint32_t i = 0; i < kSamples; ++i) {
      const double y = StepSection<Fma>(s, Load(samples[i]));
      samples[i] = FloatToBits(static_cast<float>(y * Load(gain())));
    }
  }
}

// The channel's four history words after the block (h0..h3 order), each
// replaced by f5 when its exponent bits are 0 (loc_82147B6C).
inline std::array<uint32_t, kHistoryWords> ChannelHistoryOut(const BankState &s,
                                                             uint32_t zero_bits) {
  Bank bank;
  StoreHistory(s, bank);
  for (uint32_t k = 0; k < kHistoryWords; ++k) {
    bank.h[k] = FlushHistoryWord(bank.h[k], zero_bits);
  }
  return bank.h;
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
template <MulAddFn Fma = MulAdd>
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
      const double x = StepBank<Fma>(b, StepBank<Fma>(a, Load(samples[i])));
      samples[i] = FloatToBits(static_cast<float>(x));
    }
  } else if (a_on) {
    for (uint32_t i = 0; i < kSamples; ++i) {
      samples[i] = FloatToBits(static_cast<float>(StepBank<Fma>(a, Load(samples[i]))));
    }
  } else if (b_on) {
    for (uint32_t i = 0; i < kSamples; ++i) {
      samples[i] = FloatToBits(static_cast<float>(StepBank<Fma>(b, Load(samples[i]))));
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

// l89 self-check race: `before` is the host copy the prediction was made
// from, `after` the object once the original has run. The original stores
// +88 = +80 and +84 = +76 as it read them last; a cache that differs from the
// copied parameter, or a parameter that differs now, means another thread
// changed +80/+76 between the copy and the original (l87: a coefficient
// refresh the copy did not predict).
constexpr bool FilterRaced(const Object &before, const Object &after) {
  return after.cache_a != before.cutoff_a || after.cache_b != before.param_b ||
         after.cutoff_a != before.cutoff_a || after.param_b != before.param_b;
}

// The sub_82147738 object fields (only read by the original). A self-check
// difference with these changed across the original's run is `raced`.
struct ChannelInputs {
  uint8_t bypassed = 0;  // +68
  uint8_t mask = 0;      // +74
  uint8_t cascade = 0;   // +75
  std::array<uint32_t, kCoefficients> c{};             // +12..+28
  uint32_t gain = 0;                                   // +60
  std::array<uint32_t, kHistoryWords> history{};       // +32..+44 (pointers)
  bool operator==(const ChannelInputs &) const = default;
};

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
  // The field name is built only for the first difference (index < 0: none).
  void Check(const char *name, int index, uint32_t a, uint32_t b) {
    if (a != b && count++ == 0) {
      field = index >= 0 ? name + std::to_string(index) : std::string(name);
      want = a;
      got = b;
    }
  }
};

inline Mismatch Compare(const Object &want, const uint32_t *want_samples, const Object &got,
                        const uint32_t *got_samples) {
  Mismatch m;
  for (uint32_t k = 0; k < kCoefficients; ++k) {
    m.Check("a.c", int(k), want.a.c[k], got.a.c[k]);
    m.Check("b.c", int(k), want.b.c[k], got.b.c[k]);
  }
  for (uint32_t k = 0; k < kHistoryWords; ++k) {
    m.Check("a.h", int(k), want.a.h[k], got.a.h[k]);
    m.Check("b.h", int(k), want.b.h[k], got.b.h[k]);
  }
  m.Check("cache_a", -1, want.cache_a, got.cache_a);
  m.Check("cache_b", -1, want.cache_b, got.cache_b);
  m.Check("bypassed_a", -1, want.bypassed_a, got.bypassed_a);
  m.Check("bypassed_b", -1, want.bypassed_b, got.bypassed_b);
  for (uint32_t i = 0; i < kSamples; ++i) {
    m.Check("sample", int(i), want_samples[i], got_samples[i]);
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

// l89 probe of a pass-through leaf: calls inside a frame (timed while its
// rate stays at or below kPassTimedMaxPerSecond), calls outside any frame
// (counted only).
struct PassStats {
  uint64_t calls = 0;
  uint64_t timed = 0;
  uint64_t ns = 0;  // over the timed calls
  uint64_t outside = 0;
  void Merge(const PassStats &o) {
    calls += o.calls;
    timed += o.timed;
    ns += o.ns;
    outside += o.outside;
  }
  // ns per frame, scaled to all in-frame calls when only some were timed.
  uint64_t NsPerFrame(uint64_t frames) const {
    if (frames == 0 || timed == 0) {
      return 0;
    }
    return static_cast<uint64_t>(double(ns) * double(calls) / double(timed) / double(frames));
  }
};
inline constexpr uint64_t kPassTimedMaxPerSecond = 50000;

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
  // Native path and self-check of sub_82146BE0.
  uint64_t native = 0;
  uint64_t refresh = 0;   // native calls that refreshed coefficients
  uint64_t fallback = 0;  // native on, original called (no buffer pointer)
  uint64_t verified = 0;
  uint64_t mismatches = 0;
  uint64_t verify_skipped = 0;  // sampled but refreshing: not compared
  uint64_t raced = 0;  // l89: differences explained by FilterRaced (not mismatches)
  // l89: native path and self-check of sub_82147738.
  uint64_t ch_native = 0;
  uint64_t ch_fallback = 0;  // no buffer argument or sample pointer
  uint64_t ch_verified = 0;
  uint64_t ch_mismatches = 0;
  uint64_t ch_raced = 0;  // ChannelInputs changed across the original
  uint64_t ch_verify_skipped = 0;  // sampled but bypassed / no channel / no buffer
  // l89 probe: the remaining engine-thread leaves.
  PassStats mix;   // sub_823F4450
  PassStats add;   // sub_823EFD30
  PassStats kick;  // sub_823EE050

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
    raced += o.raced;
    ch_native += o.ch_native;
    ch_fallback += o.ch_fallback;
    ch_verified += o.ch_verified;
    ch_mismatches += o.ch_mismatches;
    ch_raced += o.ch_raced;
    ch_verify_skipped += o.ch_verify_skipped;
    mix.Merge(o.mix);
    add.Merge(o.add);
    kick.Merge(o.kick);
  }
};

inline double Ratio(uint64_t a, uint64_t b) { return b ? double(a) / double(b) : 0.0; }

// The l89 fields appended to the MCLA_AFILT line and the F10 object.
inline std::string L89Fields(const Stats &s, bool json) {
  using ull = unsigned long long;
  char text[1024];
  if (!json) {
    std::snprintf(text, sizeof(text),
                  " raced=%llu ch_nat=%llu ch_fallback=%llu ch_verified=%llu ch_mismatch=%llu "
                  "ch_raced=%llu ch_vskip=%llu",
                  ull(s.raced), ull(s.ch_native), ull(s.ch_fallback), ull(s.ch_verified),
                  ull(s.ch_mismatches), ull(s.ch_raced), ull(s.ch_verify_skipped));
  } else {
    std::snprintf(text, sizeof(text),
                  ", \"raced\": %llu, \"ch_native_calls\": %llu, \"ch_fallback\": %llu, "
                  "\"ch_verified\": %llu, \"ch_mismatches\": %llu, \"ch_raced\": %llu, "
                  "\"ch_verify_skipped\": %llu",
                  ull(s.raced), ull(s.ch_native), ull(s.ch_fallback), ull(s.ch_verified),
                  ull(s.ch_mismatches), ull(s.ch_raced), ull(s.ch_verify_skipped));
  }
  std::string out = text;
  const std::pair<const char *, const PassStats *> leaves[] = {
      {"mix", &s.mix}, {"add", &s.add}, {"kick", &s.kick}};
  for (const auto &[name, p] : leaves) {
    if (!json) {
      std::snprintf(text, sizeof(text),
                    " %s_calls=%llu %s_out=%llu %s_timed=%llu %s_ns=%llu %s_per_frame=%.2f "
                    "%s_ns_per_frame=%llu",
                    name, ull(p->calls), name, ull(p->outside), name, ull(p->timed), name,
                    ull(p->ns), name, Ratio(p->calls, s.frames), name,
                    ull(p->NsPerFrame(s.frames)));
    } else {
      std::snprintf(text, sizeof(text),
                    ", \"%s\": {\"calls\": %llu, \"outside\": %llu, \"timed\": %llu, "
                    "\"ns\": %llu, \"ns_per_frame\": %llu}",
                    name, ull(p->calls), ull(p->outside), ull(p->timed), ull(p->ns),
                    ull(p->NsPerFrame(s.frames)));
    }
    out += text;
  }
  return out;
}

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
  return text + L89Fields(s, false);
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

// One MCLA_AFILT_CH_VERIFY line (l89; the first few sub_82147738 mismatches).
// Fields: "ch<k>.sample<i>" or "ch<k>.h<j>" (k = channel number).
inline std::string ChannelVerifyLine(uint64_t index, uint64_t verified, const Mismatch &m,
                                     uint32_t channels, uint8_t mask, bool cascade) {
  char text[256];
  std::snprintf(text, sizeof(text),
                "MCLA_AFILT_CH_VERIFY v=1 n=%llu call=%llu channels=%u mask=0x%02X cascade=%d "
                "diffs=%u field=%s want=0x%08X got=0x%08X",
                static_cast<unsigned long long>(index),
                static_cast<unsigned long long>(verified), channels, unsigned(mask),
                cascade ? 1 : 0, m.count, m.field.c_str(), m.want, m.got);
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
      "\"mismatches\": %llu, \"verify_skipped\": %llu",
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
  return text + L89Fields(s, true) + "}";
}

// l98 (docs/evidence/M11-l98-owner-build-20261113.md): the default of the
// host init-only cvar mcla_audio_filter_native. Off until l97, on since l98
// on both platforms (owner verdict on the l96 Deck runs: engine 69 % -> 99.6 %
// of real time, music perfect; PC by ear unchanged; native 2.36 M calls,
// verified 28 k, mismatch 0, fallback 0;
// docs/evidence/M11-l96-deck-analysis-20261113.md). The owner control key
// audio_filter_native_off (mcla_dev_overrides.h) turns it off for one arm.
inline constexpr bool kNativeDefault = true;

// Native on with the probe and the self-check off (the l98 default): the
// filter hooks run the native kernels without timing, per-call statistics or
// engine-frame accounting; only fallbacks to the original are counted.
constexpr bool Lean(bool native, bool verify, bool probe) {
  return native && !verify && !probe;
}

// The one MCLA_AFILT config line at start-up (l98), in every log.
inline std::string ConfigLine(bool native, bool verify, bool probe, bool ch_off) {
  char text[128];
  std::snprintf(text, sizeof(text),
                "MCLA_AFILT v=1 kind=config native=%d verify=%d probe=%d ch_off=%d lean=%d",
                native ? 1 : 0, verify ? 1 : 0, probe ? 1 : 0, ch_off ? 1 : 0,
                Lean(native, verify, probe) ? 1 : 0);
  return text;
}

// The F10 "audio_filter" object in the lean mode (l98): the switches and the
// session's fallbacks to the original (no buffer argument or sample pointer).
inline std::string LeanStateJson(uint64_t fallback, uint64_t ch_fallback, bool ch_off) {
  char text[256];
  std::snprintf(text, sizeof(text),
                "{\"version\": 1, \"probe\": false, \"native\": true, \"verify\": false, "
                "\"lean\": true, \"ch_off\": %s, \"fallback\": %llu, \"ch_fallback\": %llu}",
                ch_off ? "true" : "false", static_cast<unsigned long long>(fallback),
                static_cast<unsigned long long>(ch_fallback));
  return text;
}

// Self-check sampling: one call in kVerifyEvery; at most kVerifyLines lines.
inline constexpr uint64_t kVerifyEvery = 64;
inline constexpr uint64_t kVerifyLines = 8;
inline constexpr uint64_t kLineIntervalNs = 1000000000ull;

}  // namespace mcla::audio_filter

// The F10 state block "audio_filter"; empty when all switches are off.
std::string MclaAudioFilterStateJson();

// l98: logs the MCLA_AFILT config line once. Call at start-up after the
// owner override file has been applied (the switches are init-only).
void MclaAudioFilterLogConfig();
