// l95 (M11) FMA stage 0 start-up benchmark. See mcla_fma_bench.h.

#include "mcla_fma_bench.h"

#include "mcla_logging.h"

#include <rex/cvar.h>

#include <chrono>
#include <cmath>
#include <vector>

#if defined(_MSC_VER) || defined(_WIN32)
#include <intrin.h>
#endif

// Dev key fma_bench (l95) writes true.
REXCVAR_DEFINE_BOOL(mcla_fma_bench, false, "MCLA",
    "Time 1e5 fma calls through the CRT and the FMA3 hardware path once at start-up "
    "and log MCLA_FMA_BENCH (diagnostic; nothing is interposed)")
    .lifecycle(rex::cvar::Lifecycle::kInitOnly);

namespace {

namespace fb = mcla::fma_bench;

using FmaFn = double (*)(double, double, double);

// The UCRT function, called through a pointer so the compiler can neither
// inline nor constant-fold it.
FmaFn volatile g_crt_fma = &::fma;

__attribute__((target("fma"), noinline)) double HardwareFma(double a, double b, double c) {
  return __builtin_fma(a, b, c);
}

FmaFn volatile g_hw_fma = &HardwareFma;

void Cpuid(int leaf, int regs[4]) {
#if defined(_WIN32)
  __cpuid(regs, leaf);
#else
  regs[0] = regs[1] = regs[2] = regs[3] = 0;
  (void)leaf;
#endif
}

__attribute__((target("xsave"))) uint64_t ReadXcr0() {
  return _xgetbv(0);
}

template <typename F>
double TimeNs(const std::vector<fb::Operands> &ops, std::vector<double> &out, F &&fn) {
  const auto start = std::chrono::steady_clock::now();
  for (uint32_t i = 0; i < fb::kCalls; ++i) {
    const fb::Operands &o = ops[i % fb::kOperandSets];
    out[i % fb::kOperandSets] = fn(o.a, o.b, o.c);
  }
  const auto end = std::chrono::steady_clock::now();
  return double(std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count()) /
         double(fb::kCalls);
}

}  // namespace

void MclaFmaBenchAtStartup() {
  if (!REXCVAR_GET(mcla_fma_bench)) {
    return;
  }
  int regs[4] = {};
  Cpuid(1, regs);
  const uint32_t ecx = uint32_t(regs[2]);
  const bool cpu_fma3 = fb::CpuHasFma3(ecx);
  const uint64_t xcr0 = fb::CpuHasOsXsave(ecx) ? ReadXcr0() : 0;
  const bool os_ymm = fb::OsYmm(ecx, xcr0);
  std::vector<fb::Operands> ops(fb::kOperandSets);
  for (uint32_t i = 0; i < fb::kOperandSets; ++i) {
    ops[i] = fb::MakeOperands(i);
  }
  std::vector<double> crt(fb::kOperandSets), hw(fb::kOperandSets);
  const FmaFn crt_fn = g_crt_fma;
  const double crt_ns = TimeNs(ops, crt, crt_fn);
  double hw_ns = 0;
  fb::Mismatch mismatch;
  const bool run_hw = cpu_fma3 && os_ymm;
  if (run_hw) {
    const FmaFn hw_fn = g_hw_fma;
    hw_ns = TimeNs(ops, hw, hw_fn);
    for (uint32_t i = 0; i < fb::kOperandSets; ++i) {
      fb::Compare(crt[i], hw[i], mismatch);
    }
  }
  MCLA_APP_INFO("{}", fb::Line(crt_ns, hw_ns, cpu_fma3, os_ymm, run_hw, mismatch, fb::kCalls));
}
