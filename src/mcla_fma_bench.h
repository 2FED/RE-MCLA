#pragma once

// l95 (M11) FMA stage 0 (docs/evidence/M11-fma-dispatch-design-20261111.md):
// with the host bool mcla_fma_bench (dev key fma_bench, default off) the host
// times, once at start-up, 1e5 double fma calls through the CRT (the UCRT
// function the generated code imports) and through a target("fma") hardware
// path on fixed operands, only when CPUID reports FMA3 and the OS has enabled
// the YMM state; it counts results that differ bitwise and logs one line
//   MCLA_FMA_BENCH v=1 crt_ns= hw_ns= cpu_fma3= os_ymm= mismatch= ...
// Nothing is interposed: the generated code keeps calling the CRT.

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>

namespace mcla::fma_bench {

inline constexpr uint32_t kLineVersion = 1;
inline constexpr uint32_t kCalls = 100000;
// Distinct operand triples, reused cyclically.
inline constexpr uint32_t kOperandSets = 4096;

// CPUID leaf 1: ECX bit 12 = FMA3, bit 27 = OSXSAVE, bit 28 = AVX.
constexpr bool CpuHasFma3(uint32_t leaf1_ecx) {
  return (leaf1_ecx & (1u << 12)) != 0;
}
constexpr bool CpuHasOsXsave(uint32_t leaf1_ecx) {
  return (leaf1_ecx & (1u << 27)) != 0;
}
// The OS saves the XMM and YMM state: XCR0 bits 1 and 2 (read only with
// OSXSAVE).
constexpr bool OsYmm(uint32_t leaf1_ecx, uint64_t xcr0) {
  return CpuHasOsXsave(leaf1_ecx) && (leaf1_ecx & (1u << 28)) != 0 && (xcr0 & 6u) == 6u;
}

// Deterministic operands: finite values of mixed sign and magnitude, with a
// few zeros, subnormals and exact cancellations mixed in.
struct Operands {
  double a, b, c;
};

inline uint64_t NextRandom(uint64_t &state) {
  state = state * 6364136223846793005ull + 1442695040888963407ull;
  return state;
}

inline double FromBits(uint64_t bits) {
  double value = 0;
  std::memcpy(&value, &bits, sizeof(value));
  return value;
}

inline uint64_t ToBits(double value) {
  uint64_t bits = 0;
  std::memcpy(&bits, &value, sizeof(bits));
  return bits;
}

inline Operands MakeOperands(uint32_t index) {
  uint64_t state = 0x9E3779B97F4A7C15ull ^ (uint64_t(index) * 0xD1B54A32D192ED03ull);
  auto finite = [&state](int max_exponent) {
    const uint64_t r = NextRandom(state);
    const uint64_t sign = (r >> 63) << 63;
    const uint64_t exponent = uint64_t(1023 - max_exponent + int((r >> 52) % (2 * max_exponent)));
    const uint64_t mantissa = NextRandom(state) & ((1ull << 52) - 1);
    return FromBits(sign | (exponent << 52) | mantissa);
  };
  Operands o{finite(40), finite(40), finite(80)};
  switch (index % 64) {
    case 0:
      o.c = -(o.a * o.b);  // near-exact cancellation: the fused result differs
      break;
    case 1:
      o.a = 0.0;
      break;
    case 2:
      o.b = FromBits(1);  // smallest subnormal
      break;
    case 3:
      o.c = -0.0;
      o.a = -0.0;
      break;
    default:
      break;
  }
  return o;
}

struct Mismatch {
  uint64_t bitwise = 0;  // results that differ in any bit
  uint64_t nan = 0;      // ... both NaN (payload / sign pattern only)
};

inline void Compare(double crt, double hw, Mismatch &m) {
  if (ToBits(crt) == ToBits(hw)) {
    return;
  }
  ++m.bitwise;
  if (crt != crt && hw != hw) {
    ++m.nan;
  }
}

inline std::string Line(double crt_ns, double hw_ns, bool cpu_fma3, bool os_ymm,
                        bool ran_hw, const Mismatch &m, uint32_t calls) {
  char text[256];
  std::snprintf(text, sizeof(text),
                "MCLA_FMA_BENCH v=%u crt_ns=%.2f hw_ns=%.2f cpu_fma3=%d os_ymm=%d mismatch=%llu "
                "nan_mismatch=%llu calls=%u operand_sets=%u hw=%s",
                kLineVersion, crt_ns, hw_ns, cpu_fma3 ? 1 : 0, os_ymm ? 1 : 0,
                static_cast<unsigned long long>(m.bitwise),
                static_cast<unsigned long long>(m.nan), calls, kOperandSets,
                ran_hw ? "ran" : "skipped");
  return text;
}

}  // namespace mcla::fma_bench

// Runs the benchmark once when mcla_fma_bench is set (start-up, before the
// guest runs).
void MclaFmaBenchAtStartup();
