// KI-060 l87: pure parts of the engine-thread filter probe and of the native
// sub_82146BE0 (src/mcla_audio_filter.h). The kernel is checked bit for bit
// against the generated loop body of sub_82146BE0 run by a small interpreter
// of its recompiled statements (and against a hand transliteration of one
// block), on random, silent, denormal, impulse and large inputs for the three
// bank combinations; the offsets are checked against the generated code text
// (MCLA_AFILT_GENERATED_DIR).
// l89: the kernels use MulAdd instead of std::fma (the interpreter keeps
// std::fma, the recompiled form); random and adversarial float operands
// check that the product is exact and MulAdd equals fma; every FP register
// write in both generated functions is float-exact; sub_82147738's two loops
// run by the same kind of interpreter against ProcessChannel; its offsets,
// the race classification and the l89 line fields.

#include "mcla_audio_filter.h"

#include <array>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <regex>
#include <sstream>
#include <string>
#include <vector>

#ifndef MCLA_AFILT_GENERATED_DIR
#error "MCLA_AFILT_GENERATED_DIR (generated/default) is set by CMake"
#endif

namespace {
int checks = 0;
void Check(bool value, const char *message) {
  if (!value) {
    std::fprintf(stderr, "FAIL: %s\n", message);
    std::exit(1);
  }
  ++checks;
}

namespace af = mcla::audio_filter;

std::string ReadFile(const std::string &path) {
  std::ifstream in(path, std::ios::binary);
  std::ostringstream text;
  text << in.rdbuf();
  return text.str();
}

std::string Body(const std::string &text, const std::string &name) {
  const std::string head = "DEFINE_REX_FUNC(" + name + ") {";
  const size_t begin = text.find(head);
  if (begin == std::string::npos) {
    return {};
  }
  const size_t end = text.find("DEFINE_REX_FUNC(", begin + head.size());
  return text.substr(begin, end == std::string::npos ? std::string::npos : end - begin);
}

bool Has(const std::string &text, const std::string &part) {
  return text.find(part) != std::string::npos;
}

void InOrder(const std::string &body, const std::vector<std::string> &lines, const char *what) {
  size_t from = 0;
  for (const auto &line : lines) {
    const size_t at = body.find(line, from);
    if (at == std::string::npos) {
      std::fprintf(stderr, "missing in %s: %s\n", what, line.c_str());
      Check(false, what);
    }
    from = at + line.size();
  }
  Check(true, what);
}

std::string Between(const std::string &body, const std::string &from, const std::string &to) {
  const size_t a = body.find(from);
  if (a == std::string::npos) {
    return {};
  }
  const size_t b = body.find(to, a + from.size());
  return b == std::string::npos ? std::string() : body.substr(a, b - a);
}

std::string Off(const char *op, uint32_t offset, const char *reg) {
  return std::string("// ") + op + std::to_string(offset) + "(" + reg + ")";
}

// --- Interpreter of the recompiled loop body (loc_82146DA0 .. addic.) ---

enum class OpKind {
  kLoadStack,
  kLoadSample,
  kTempToF,
  kFma,
  kMul,
  kAdd,
  kMove,
  kFToTemp,
  kStoreStack,
  kStoreSample,
  kCompare,
  kBranchIfZero,
  kLabel,
};

struct Op {
  OpKind kind;
  int d = 0, a = 0, b = 0, c = 0;
  bool negate = false;
  int offset = 0;
  std::string label;
};

std::vector<Op> ParseLoop(const std::string &loop, size_t &unknown) {
  static const std::regex load_stack(R"(^temp\.u32 = REX_LOAD_U32\(ctx\.r1\.u32 \+ (\d+)\);$)");
  static const std::regex load_sample(
      R"(^temp\.u32 = REX_LOAD_U32\(ctx\.r11\.u32 \+ (-?\d+)\);$)");
  static const std::regex temp_to_f(R"(^ctx\.f(\d+)\.f64 = double\(temp\.f32\);$)");
  static const std::regex fma(
      R"(^ctx\.f(\d+)\.f64 = double\(float\(std::fma\(ctx\.f(\d+)\.f64, ctx\.f(\d+)\.f64, )"
      R"((-?)ctx\.f(\d+)\.f64\)\)\);$)");
  static const std::regex bin(
      R"(^ctx\.f(\d+)\.f64 = double\(float\(ctx\.f(\d+)\.f64 ([*+]) ctx\.f(\d+)\.f64\)\);$)");
  static const std::regex move(R"(^ctx\.f(\d+)\.f64 = ctx\.f(\d+)\.f64;$)");
  static const std::regex f_to_temp(R"(^temp\.f32 = float\(ctx\.f(\d+)\.f64\);$)");
  static const std::regex store_stack(
      R"(^REX_STORE_U32\(ctx\.r1\.u32 \+ (\d+), temp\.u32\);$)");
  static const std::regex store_sample(
      R"(^REX_STORE_U32\(ctx\.r11\.u32 \+ (-?\d+), temp\.u32\);$)");
  static const std::regex compare(
      R"(^ctx\.cr6\.compare<uint32_t>\(ctx\.r(30|27)\.u32, 0, ctx\.xer\);$)");
  static const std::regex branch(R"(^if \(ctx\.cr6\.eq\) goto (loc_[0-9A-F]+);$)");
  static const std::regex label(R"(^(loc_[0-9A-F]+):$)");
  std::vector<Op> ops;
  std::istringstream in(loop);
  std::string raw;
  unknown = 0;
  while (std::getline(in, raw)) {
    std::string l = raw;
    while (!l.empty() && (l.back() == '\r' || l.back() == ' ' || l.back() == '\t')) {
      l.pop_back();
    }
    const size_t first = l.find_first_not_of(" \t");
    l = first == std::string::npos ? std::string() : l.substr(first);
    if (l.empty() || l.rfind("//", 0) == 0 || l.rfind("rex::ppc::SetGuestProgramCounter", 0) == 0 ||
        l == "ctx.fpscr.disableFlushMode();") {
      continue;
    }
    std::smatch m;
    Op op{};
    if (std::regex_match(l, m, load_stack)) {
      op.kind = OpKind::kLoadStack;
      op.offset = std::stoi(m[1].str());
    } else if (std::regex_match(l, m, load_sample)) {
      op.kind = OpKind::kLoadSample;
      op.offset = std::stoi(m[1].str());
    } else if (std::regex_match(l, m, temp_to_f)) {
      op.kind = OpKind::kTempToF;
      op.d = std::stoi(m[1].str());
    } else if (std::regex_match(l, m, fma)) {
      op.kind = OpKind::kFma;
      op.d = std::stoi(m[1].str());
      op.a = std::stoi(m[2].str());
      op.b = std::stoi(m[3].str());
      op.negate = m[4].str() == "-";
      op.c = std::stoi(m[5].str());
    } else if (std::regex_match(l, m, bin)) {
      op.kind = m[3].str() == "*" ? OpKind::kMul : OpKind::kAdd;
      op.d = std::stoi(m[1].str());
      op.a = std::stoi(m[2].str());
      op.b = std::stoi(m[4].str());
    } else if (std::regex_match(l, m, move)) {
      op.kind = OpKind::kMove;
      op.d = std::stoi(m[1].str());
      op.a = std::stoi(m[2].str());
    } else if (std::regex_match(l, m, f_to_temp)) {
      op.kind = OpKind::kFToTemp;
      op.a = std::stoi(m[1].str());
    } else if (std::regex_match(l, m, store_stack)) {
      op.kind = OpKind::kStoreStack;
      op.offset = std::stoi(m[1].str());
    } else if (std::regex_match(l, m, store_sample)) {
      op.kind = OpKind::kStoreSample;
      op.offset = std::stoi(m[1].str());
    } else if (std::regex_match(l, m, compare)) {
      op.kind = OpKind::kCompare;
      op.a = std::stoi(m[1].str());
    } else if (std::regex_match(l, m, branch)) {
      op.kind = OpKind::kBranchIfZero;
      op.label = m[1].str();
    } else if (std::regex_match(l, m, label)) {
      op.kind = OpKind::kLabel;
      op.label = m[1].str();
    } else {
      std::fprintf(stderr, "unknown loop statement: %s\n", l.c_str());
      ++unknown;
      continue;
    }
    ops.push_back(op);
  }
  return ops;
}

// The r1-relative stack words (byte offset / 4; the loop uses 96..176).
using Stack = std::array<uint32_t, 48>;

// Runs the 32 iterations of the parsed loop on `stack` and `samples` (raw
// words, in place).
void RunLoop(const std::vector<Op> &ops, bool a_on, bool b_on, Stack &stack,
             std::array<uint32_t, af::kSamples> &samples) {
  double f[32] = {};
  uint32_t temp = 0;
  int compared = 0;
  for (uint32_t iteration = 0; iteration < af::kIterations; ++iteration) {
    // r11 = data + 8 + 32 * iteration.
    const int base = static_cast<int>(iteration * af::kSamplesPerIteration);
    for (size_t i = 0; i < ops.size(); ++i) {
      const Op &op = ops[i];
      switch (op.kind) {
        case OpKind::kLoadStack:
          temp = stack.at(op.offset / 4);
          break;
        case OpKind::kLoadSample:
          temp = samples.at(base + (op.offset + 8) / 4);
          break;
        case OpKind::kTempToF:
          f[op.d] = double(af::BitsToFloat(temp));
          break;
        case OpKind::kFma:
          f[op.d] = double(float(std::fma(f[op.a], f[op.b], op.negate ? -f[op.c] : f[op.c])));
          break;
        case OpKind::kMul:
          f[op.d] = double(float(f[op.a] * f[op.b]));
          break;
        case OpKind::kAdd:
          f[op.d] = double(float(f[op.a] + f[op.b]));
          break;
        case OpKind::kMove:
          f[op.d] = f[op.a];
          break;
        case OpKind::kFToTemp:
          temp = af::FloatToBits(float(f[op.a]));
          break;
        case OpKind::kStoreStack:
          stack.at(op.offset / 4) = temp;
          break;
        case OpKind::kStoreSample:
          samples.at(base + (op.offset + 8) / 4) = temp;
          break;
        case OpKind::kCompare:
          compared = op.a;
          break;
        case OpKind::kBranchIfZero: {
          const bool on = compared == 30 ? a_on : b_on;
          if (!on) {
            size_t j = i + 1;
            while (j < ops.size() &&
                   !(ops[j].kind == OpKind::kLabel && ops[j].label == op.label)) {
              ++j;
            }
            Check(j < ops.size(), "branch target inside the loop body");
            i = j;
          }
          break;
        }
        case OpKind::kLabel:
          break;
      }
    }
  }
}

// --- Inputs ---

uint32_t lcg = 12345;
uint32_t Next() {
  lcg = lcg * 1664525u + 1013904223u;
  return lcg;
}
float Uniform(float lo, float hi) {
  return lo + (hi - lo) * float(Next() >> 8) / float(1u << 24);
}

// RBJ biquad designs mapped to c0..c4 (y = c0 x + h; h' = c1 x - c3 y + ...).
af::Bank Design(double cutoff, bool high, uint32_t h_fill) {
  const double pi = 3.14159265358979323846;
  const double w = 2.0 * pi * cutoff / 48000.0;
  const double alpha = std::sin(w) / (2.0 * 0.7071);
  const double cw = std::cos(w);
  const double a0 = 1.0 + alpha;
  double b0, b1, b2;
  if (high) {
    b0 = (1.0 + cw) / 2.0;
    b1 = -(1.0 + cw);
    b2 = b0;
  } else {
    b0 = (1.0 - cw) / 2.0;
    b1 = 1.0 - cw;
    b2 = b0;
  }
  af::Bank bank;
  bank.c = {af::FloatToBits(float(b0 / a0)), af::FloatToBits(float(b1 / a0)),
            af::FloatToBits(float(b2 / a0)), af::FloatToBits(float(-2.0 * cw / a0)),
            af::FloatToBits(float((1.0 - alpha) / a0))};
  bank.h.fill(h_fill);
  return bank;
}

enum class Input { kRandom, kSilent, kDenormal, kImpulse, kLarge, kTiny };

void Fill(Input input, std::array<uint32_t, af::kSamples> &samples) {
  for (uint32_t i = 0; i < af::kSamples; ++i) {
    float v = 0.0f;
    switch (input) {
      case Input::kRandom:
        v = Uniform(-1.0f, 1.0f);
        break;
      case Input::kSilent:
        v = (i & 1) ? -0.0f : 0.0f;
        break;
      case Input::kDenormal:
        v = af::BitsToFloat((Next() & 0x007FFFFFu) | (Next() & 0x80000000u));
        break;
      case Input::kImpulse:
        v = i == 3 ? 1.0f : 0.0f;
        break;
      case Input::kLarge:
        v = Uniform(-30000.0f, 30000.0f);
        break;
      case Input::kTiny:
        v = Uniform(-1e-36f, 1e-36f);
        break;
    }
    samples[i] = af::FloatToBits(v);
  }
}

// One kernel-vs-generated comparison; true when bit-identical.
bool CompareCase(const std::vector<Op> &ops, const af::Bank &bank_a, const af::Bank &bank_b,
                 bool a_on, bool b_on, Input input, uint32_t zero_bits) {
  std::array<uint32_t, af::kSamples> samples{};
  Fill(input, samples);
  Stack stack{};
  for (uint32_t k = 0; k < 9; ++k) {
    const uint32_t a_word = k < 5 ? bank_a.c[k] : bank_a.h[k - 5];
    const uint32_t b_word = k < 5 ? bank_b.c[k] : bank_b.h[k - 5];
    stack.at((af::kStackCoefficientsA + 4 * k) / 4) = a_word;
    stack.at((af::kStackCoefficientsB + 4 * k) / 4) = b_word;
  }
  std::array<uint32_t, af::kSamples> generated = samples;
  RunLoop(ops, a_on, b_on, stack, generated);

  af::Object o;
  o.a = bank_a;
  o.b = bank_b;
  std::array<uint32_t, af::kSamples> native = samples;
  af::Process(o, a_on, b_on, 1000, 1, native.data(), zero_bits);
  // The l87 library-fma kernel gives the same bits (MulAdd == fma here).
  af::Object l87;
  l87.a = bank_a;
  l87.b = bank_b;
  std::array<uint32_t, af::kSamples> library = samples;
  af::Process<af::LibraryFma>(l87, a_on, b_on, 1000, 1, library.data(), zero_bits);
  bool same = generated == native && library == native && l87.a.h == o.a.h && l87.b.h == o.b.h;
  for (uint32_t k = 0; k < af::kHistoryWords; ++k) {
    const uint32_t ga = af::FlushHistoryWord(
        stack.at((af::kStackCoefficientsA + af::kHistoryOffset + 4 * k) / 4), zero_bits);
    const uint32_t gb = af::FlushHistoryWord(
        stack.at((af::kStackCoefficientsB + af::kHistoryOffset + 4 * k) / 4), zero_bits);
    same = same && ga == o.a.h[k] && gb == o.b.h[k];
  }
  for (uint32_t k = 0; k < af::kCoefficients; ++k) {
    same = same && stack.at((af::kStackCoefficientsA + 4 * k) / 4) == bank_a.c[k] &&
           stack.at((af::kStackCoefficientsB + 4 * k) / 4) == bank_b.c[k] &&
           o.a.c[k] == bank_a.c[k] && o.b.c[k] == bank_b.c[k];
  }
  return same;
}

void TestKernelAgainstGenerated() {
  const std::string dir = MCLA_AFILT_GENERATED_DIR;
  const std::string g0 = ReadFile(dir + "/mcla_recomp.0.cpp");
  const std::string body = Body(g0, "sub_82146BE0");
  Check(!body.empty(), "sub_82146BE0 found");
  const std::string loop = Between(body, "loc_82146DA0:", "// addic. r10,r10,-1");
  Check(!loop.empty(), "loop body found");
  size_t unknown = 0;
  const std::vector<Op> ops = ParseLoop(loop, unknown);
  Check(unknown == 0, "every loop statement understood by the interpreter");
  size_t fmas = 0, stores = 0, sample_stores = 0;
  for (const Op &op : ops) {
    fmas += op.kind == OpKind::kFma ? 1 : 0;
    stores += op.kind == OpKind::kStoreStack ? 1 : 0;
    sample_stores += op.kind == OpKind::kStoreSample ? 1 : 0;
  }
  // 8 samples x 2 banks x 6 fma-class ops; 4 history stores each; 8 samples.
  Check(fmas == 96 && stores == 64 && sample_stores == 8, "loop shape: 96 fma, 64 + 8 stores");

  const uint32_t zeros[] = {0x00000000u, 0x3F800000u};
  const bool combos[3][2] = {{true, true}, {true, false}, {false, true}};
  const Input inputs[] = {Input::kRandom, Input::kSilent, Input::kDenormal,
                          Input::kImpulse, Input::kLarge, Input::kTiny};
  const double cutoffs[] = {80.0, 400.0, 2500.0, 9000.0, 20000.0};
  size_t cases = 0;
  for (const double cutoff : cutoffs) {
    for (const Input input : inputs) {
      for (const auto &combo : combos) {
        for (const uint32_t zero : zeros) {
          // History: zero, a running state, or denormal words.
          const uint32_t fills[] = {0u, af::FloatToBits(Uniform(-0.5f, 0.5f)), 0x00012345u};
          for (const uint32_t fill : fills) {
            const af::Bank a = Design(cutoff, false, fill);
            const af::Bank b = Design(cutoff / 4.0, true, fill);
            Check(CompareCase(ops, a, b, combo[0], combo[1], input, zero),
                  "native kernel bit-identical to the generated loop");
            ++cases;
          }
        }
      }
    }
  }
  // Random coefficient words of small magnitude (stable enough over 256).
  for (int n = 0; n < 40; ++n) {
    af::Bank a, b;
    for (uint32_t k = 0; k < af::kCoefficients; ++k) {
      a.c[k] = af::FloatToBits(Uniform(-0.45f, 0.45f));
      b.c[k] = af::FloatToBits(Uniform(-0.45f, 0.45f));
    }
    for (uint32_t k = 0; k < af::kHistoryWords; ++k) {
      a.h[k] = af::FloatToBits(Uniform(-1.0f, 1.0f));
      b.h[k] = af::FloatToBits(Uniform(-1.0f, 1.0f));
    }
    for (const auto &combo : combos) {
      Check(CompareCase(ops, a, b, combo[0], combo[1], Input::kRandom, 0),
            "random coefficients: bit-identical");
      ++cases;
    }
  }
  Check(cases == 5 * 6 * 3 * 2 * 3 + 40 * 3, "case count");
}

// Hand transliteration of bank A, sample 0 (mcla_recomp.0.cpp:55503-55566),
// register by register, against one StepBank.
void TestHandTransliteration() {
  for (int n = 0; n < 2000; ++n) {
    float stack[9] = {};  // r1+144..r1+176: c0..c4, h0..h3
    for (int k = 0; k < 9; ++k) {
      stack[k] = k < 5 ? Uniform(-0.9f, 0.9f) : Uniform(-2.0f, 2.0f);
    }
    if (n % 7 == 0) {
      stack[5] = af::BitsToFloat(0x00000101u);
    }
    const float sample = n % 5 == 0 ? af::BitsToFloat(0x00400000u) : Uniform(-1.0f, 1.0f);
    auto S = [&stack](int offset) -> double { return double(stack[(offset - 144) / 4]); };
    auto W = [&stack](int offset, double v) { stack[(offset - 144) / 4] = float(v); };
    double f0 = double(sample), f1, f2, f3, f4, f5, f6, f7, f13, f29, f30;
    af::Bank bank;
    for (int k = 0; k < 5; ++k) {
      bank.c[k] = af::FloatToBits(stack[k]);
    }
    for (int k = 0; k < 4; ++k) {
      bank.h[k] = af::FloatToBits(stack[5 + k]);
    }
    f7 = S(164);
    f13 = S(144);
    f5 = double(float(std::fma(f0, f13, f7)));
    f4 = S(172);
    f6 = S(156);
    f7 = S(160);
    f3 = S(168);
    f2 = S(176);
    f13 = double(float(std::fma(f5, f13, f4)));
    f1 = double(float(f5 * f6));
    f4 = double(float(f5 * f7));
    f30 = double(float(f13 * f6));
    f6 = S(148);
    f29 = double(float(f13 * f7));
    f7 = S(152);
    f1 = double(float(std::fma(f0, f6, -f1)));
    f0 = double(float(std::fma(f0, f7, -f4)));
    W(168, f0);
    f0 = f13;
    f13 = double(float(std::fma(f5, f6, -f30)));
    f7 = double(float(std::fma(f5, f7, -f29)));
    W(176, f7);
    f6 = double(float(f1 + f3));
    W(164, f6);
    f5 = double(float(f13 + f2));
    W(172, f5);
    af::BankState s = af::LoadBank(bank);
    const double y = af::StepBank(s, double(sample));
    af::Bank after;
    af::StoreHistory(s, after);
    Check(af::FloatToBits(float(y)) == af::FloatToBits(float(f0)) &&
              after.h[0] == af::FloatToBits(stack[5]) &&
              after.h[1] == af::FloatToBits(stack[6]) &&
              after.h[2] == af::FloatToBits(stack[7]) && after.h[3] == af::FloatToBits(stack[8]),
          "StepBank equals the hand transliteration of one block");
  }
}

void TestPure() {
  static_assert(af::kBankA + af::kHistoryOffset == 24 && af::kBankB + af::kHistoryOffset == 60);
  static_assert(af::kZeroConstant == 0x82000ED4u);
  static_assert(af::kFramesPerSecondNeeded == 187.5);
  Check(af::BankAOn(23899) && !af::BankAOn(23900) && !af::BankAOn(0xFFFFFFFFu) &&
            af::BankAOn(0),
        "bank A on below 23900 (unsigned)");
  Check(af::BankBOn(1) && !af::BankBOn(0) && af::BankBOn(0x80000000u), "bank B on when != 0");
  Check(af::FlushHistoryWord(0x00000000u, 7) == 7 && af::FlushHistoryWord(0x80000000u, 7) == 7 &&
            af::FlushHistoryWord(0x00012345u, 7) == 7 &&
            af::FlushHistoryWord(0x807FFFFFu, 7) == 7 &&
            af::FlushHistoryWord(0x00800000u, 7) == 0x00800000u &&
            af::FlushHistoryWord(0x3F800000u, 7) == 0x3F800000u &&
            af::FlushHistoryWord(0x7F800000u, 7) == 0x7F800000u,
        "flush: exponent bits 0 (zero, -0, denormal) take f31; others kept");
  Check(af::Denormal(1) && af::Denormal(0x807FFFFFu) && !af::Denormal(0) &&
            !af::Denormal(0x80000000u) && !af::Denormal(0x00800000u),
        "denormal: exponent 0 and mantissa != 0");

  af::Object o;
  o.a.h = {1, 2, 3, 4};
  o.b.h = {5, 6, 7, 8};
  o.bypassed_a = 1;
  o.bypassed_b = 1;
  af::ResetBypassedHistory(o, true, false, 9);
  Check(o.a.h == std::array<uint32_t, 4>{9, 9, 9, 9} &&
            o.b.h == std::array<uint32_t, 4>{5, 6, 7, 8},
        "reset only an enabled bank that was bypassed");
  o.bypassed_a = 0;
  o.a.h = {1, 2, 3, 4};
  af::ResetBypassedHistory(o, true, true, 9);
  Check(o.a.h == std::array<uint32_t, 4>{1, 2, 3, 4} &&
            o.b.h == std::array<uint32_t, 4>{9, 9, 9, 9},
        "no reset without the bypass byte");

  // Flags and caches; a disabled bank's history is only flushed.
  af::Object p;
  p.a = Design(1000.0, false, 0x00000001u);
  p.b.h = {0x00000002u, 0x3F000000u, 0x80000000u, 0x40000000u};
  p.bypassed_a = 1;
  std::array<uint32_t, af::kSamples> samples{};
  Fill(Input::kRandom, samples);
  af::Process(p, true, false, 1234, 0, samples.data(), 0x11111111u);
  Check(p.bypassed_a == 0 && p.bypassed_b == 1 && p.cache_a == 1234 && p.cache_b == 0,
        "flags and caches as the original writes them");
  Check(p.b.h == std::array<uint32_t, 4>{0x11111111u, 0x3F000000u, 0x11111111u, 0x40000000u},
        "disabled bank: history flushed, otherwise unchanged");
  p.cutoff_a = 1234;
  p.param_b = 0;
  Check(!af::NeedsRefresh(p), "no refresh when both caches match");
  p.cache_a = 5;
  Check(af::NeedsRefresh(p), "refresh when a cache differs from its parameter");

  // Classification.
  std::array<uint32_t, af::kSamples> silent{};
  af::Object q;
  Check(af::Classify(silent.data(), af::kSamples, q).zero &&
            !af::Classify(silent.data(), af::kSamples, q).denormal,
        "silence: zero, no denormal");
  silent[17] = 0x80000000u;
  Check(af::Classify(silent.data(), af::kSamples, q).zero, "-0 is still silence");
  q.b.h[2] = 0x00000003u;
  Check(af::Classify(silent.data(), af::kSamples, q).denormal, "denormal history counts");
  silent[3] = 0x3F800000u;
  q.b.h[2] = 0;
  const af::BlockClass c = af::Classify(silent.data(), af::kSamples, q);
  Check(!c.zero && !c.denormal, "a signal block");

  // Comparison.
  af::Object w;
  std::array<uint32_t, af::kSamples> ws{}, gs{};
  Check(af::Compare(w, ws.data(), w, gs.data()).count == 0, "equal states compare equal");
  af::Object g = w;
  g.b.h[1] = 5;
  gs[200] = 1;
  const af::Mismatch m = af::Compare(w, ws.data(), g, gs.data());
  Check(m.count == 2 && m.field == "b.h1" && m.want == 0 && m.got == 5,
        "mismatches counted; first field reported");

  // MXCSR helpers.
  Check(af::MxcsrBefore(0x1FA2u) == 0x1FA0u && af::MxcsrBefore(0x9FC0u) == 0x9FC0u,
        "before: DE and UE cleared, the rest kept");
  Check(af::MxcsrAfter(0x1F92u, 0x1F80u) == 0x1F92u && af::MxcsrAfter(0x1F80u, 0x1FA2u) == 0x1FA2u,
        "after: the callee's state plus the caller's DE/UE");
}

void TestStats() {
  af::Stats s;
  s.AddMix(true, true);
  s.AddMix(true, false);
  s.AddMix(false, true);
  s.AddMix(false, false);
  Check(s.both == 1 && s.a_only == 1 && s.b_only == 1 && s.none == 1, "bank mix");
  s.filter.Add(5000, true, false);
  s.filter.Add(7000, false, true);
  Check(s.filter.calls == 2 && s.filter.ns == 12000 && s.filter.max_ns == 7000 &&
            s.filter.de == 1 && s.filter.ue == 1,
        "function timing");
  s.AddFrame(4000000, true, 2, 0);
  s.AddFrame(6000000, false, 9, 3);
  Check(s.frames == 2 && s.other_frames == 1 && s.frame_ns == 10000000 &&
            s.frame_max_ns == 6000000 && s.filter_per_frame_max == 9 &&
            s.channel_per_frame_max == 3,
        "frames");
  af::Stats t;
  t.frames = 3;
  t.filter_per_frame_max = 4;
  t.mismatches = 1;
  t.Merge(s);
  Check(t.frames == 5 && t.filter_per_frame_max == 9 && t.filter.calls == 2 &&
            t.mismatches == 1 && t.both == 1,
        "merge");
  const std::string line = af::Line(s, 1000000000ull, true, false);
  Check(line.rfind("MCLA_AFILT v=1 ms=1000 native=1 verify=0 frames=2 other=1 fps=2.00 "
                   "need=187.5 fps_ratio=0.011 frame_ns_avg=5000000 frame_ns_max=6000000 "
                   "busy=0.010 bq_calls=2 bq_ns=12000 bq_ns_avg=6000 bq_ns_max=7000 "
                   "bq_per_frame=1.00 bq_per_frame_max=9 bq_a=1 bq_b=1 bq_ab=1 bq_none=1 ",
                   0) == 0,
        "line head");
  Check(line.find(" nat=0 refresh=0 fallback=0 verified=0 mismatch=0 vskip=0") !=
            std::string::npos,
        "line tail");
  af::Mismatch m;
  m.count = 3;
  m.field = "sample7";
  m.want = 0x3F800000u;
  m.got = 0x3F800001u;
  Check(af::VerifyLine(1, 64, m, true, false) ==
            "MCLA_AFILT_VERIFY v=1 n=1 call=64 a=1 b=0 diffs=3 field=sample7 "
            "want=0x3F800000 got=0x3F800001",
        "verify line");
  const std::string json = af::StateJson(s, 2000000000ull, true, true, true);
  Check(json.rfind("{\"version\": 1, \"probe\": true, \"native\": true, \"verify\": true, "
                   "\"seconds\": 2.0, \"frames\": 2, ",
                   0) == 0 &&
            json.back() == '}' && json.find("\"mismatches\": 0") != std::string::npos,
        "F10 object");
}

void TestGenerated() {
  const std::string dir = MCLA_AFILT_GENERATED_DIR;
  const std::string g0 = ReadFile(dir + "/mcla_recomp.0.cpp");
  const std::string g25 = ReadFile(dir + "/mcla_recomp.25.cpp");
  Check(!g0.empty() && !g25.empty(), "generated files readable");
  const std::string f = Body(g0, "sub_82146BE0");
  Check(Has(f, "// stwu r1,-" + std::to_string(af::kFrameSize) + "(r1)"), "frame size 256");
  InOrder(f,
          {"// mr r31,r3", "// li r10," + std::to_string(af::kBypassCutoff), "// mr r3,r4",
           Off("lwz r11,", af::kCutoffA, "r31"), Off("lwz r9,", af::kParamB, "r31"),
           "// subfc r8,r10,r11", "// subfe r7,r8,r8", "// subfic r6,r9,0",
           "// clrlwi r11,r7,31", "// subfe r5,r6,r6", "// mr r30,r11", "// clrlwi r27,r5,31",
           "if (!ctx.cr6.eq) goto loc_82146C3C;", "if (ctx.cr6.eq) goto loc_821475EC;"},
          "bank switches: A = +80 < 23900, B = +76 != 0; neither: return");
  InOrder(f,
          {"// addi r4,r1,80", "// bl 0x823eaf00", "// addi r29,r31," + std::to_string(af::kBankA),
           "// li r5," + std::to_string(af::kBankBytes), "// mr r4,r29",
           "// addi r3,r1," + std::to_string(af::kStackCoefficientsA), "// bl 0x8244d5c0",
           "// addi r28,r31," + std::to_string(af::kBankB),
           "// li r5," + std::to_string(af::kBankBytes), "// mr r4,r28",
           "// addi r3,r1," + std::to_string(af::kStackCoefficientsB), "// bl 0x8244d5c0",
           "// lis r11,-32256", "// cmplwi cr6,r30,0", "// lfs f31,3796(r11)"},
          "descriptor, the two bank copies and the f31 constant");
  InOrder(f,
          {Off("lbz r11,", af::kBypassedA, "r31"), "// stfs f31,176(r1)", "// stfs f31,168(r1)",
           "// stfs f31,172(r1)", "// stfs f31,164(r1)", "// clrlwi r27,r27,24",
           Off("lbz r11,", af::kBypassedB, "r31"), "// stfs f31,128(r1)", "// stfs f31,120(r1)",
           "// stfs f31,124(r1)", "// stfs f31,116(r1)"},
          "history reset of a bypassed bank");
  InOrder(f,
          {Off("lwz r5,", af::kCutoffA, "r31"), Off("lwz r11,", af::kCacheA, "r31"),
           "// addi r6,r1,144", "// li r4,1", "// mr r3,r31", "// bl 0x82146ad0",
           "ctx.lr = 0x82146CE0;", "// lfs f0,144(r1)", "// lfs f13,148(r1)",
           "// lfs f12,152(r1)", "// lfs f11,156(r1)", "// lfs f10,160(r1)",
           "// stfs f0,0(r29)", "// stfs f13,8(r31)", "// stfs f12,12(r31)",
           "// stfs f11,16(r31)", "// stfs f10,20(r31)", Off("lwz r5,", af::kParamB, "r31"),
           Off("lwz r11,", af::kCacheB, "r31"), "// addi r6,r1,96", "// li r4,0",
           "// mr r3,r31", "// bl 0x82146ad0", "ctx.lr = 0x82146D28;", "// lfs f8,96(r1)",
           "// lfs f9,100(r1)", "// lfs f10,104(r1)", "// lfs f11,108(r1)",
           "// lfs f12,112(r1)", "// stfs f8,0(r28)", "// stfs f9,44(r31)",
           "// stfs f10,48(r31)", "// stfs f11,52(r31)", "// stfs f12,56(r31)"},
          "coefficient refresh: arguments, return addresses and copies");
  static_assert(af::kCoefficientReturnA == 0x82146CE0u && af::kCoefficientReturnB == 0x82146D28u);
  InOrder(f,
          {"loc_82146D68:", Off("lwz r10,", af::kCutoffA, "r31"), "// cntlzw r9,r27",
           Off("lwz r8,", af::kParamB, "r31"), "// cntlzw r7,r30", "// lwz r6,88(r1)",
           "// rlwinm r5,r9,27,31,31", "// rlwinm r4,r7,27,31,31",
           Off("stb r5,", af::kBypassedB, "r31"), "// addi r11,r6,8",
           Off("stw r10,", af::kCacheA, "r31"),
           "// li r10," + std::to_string(af::kIterations), Off("stw r8,", af::kCacheB, "r31"),
           Off("stb r4,", af::kBypassedA, "r31"), "loc_82146DA0:"},
          "flags, caches and the loop setup");
  // B's coefficients in f8..f12 on loop entry come from the same slots.
  InOrder(f,
          {"loc_82146D54:", "// lfs f12,112(r1)", "// lfs f11,108(r1)", "// lfs f10,104(r1)",
           "// lfs f9,100(r1)", "// lfs f8,96(r1)", "loc_82146D68:"},
          "unchanged bank B coefficients loaded from the scratch");
  InOrder(f,
          {"// stfs f0,20(r11)", "// addic. r10,r10,-1", "// addi r11,r11," +
           std::to_string(4 * af::kSamplesPerIteration), "// bne 0x82146da0"},
          "32 iterations of 8 samples");
  // Each word: load, (the previous word's store), exponent test, f31 or the
  // word itself; its store follows the next word's load.
  std::vector<std::string> tail;
  std::string pending;
  for (const auto &[stack, object] : {std::pair{af::kStackCoefficientsA, af::kBankA},
                                      std::pair{af::kStackCoefficientsB, af::kBankB}}) {
    for (const uint32_t k : af::kFlushOrder) {
      tail.push_back(Off("lwz r11,", stack + af::kHistoryOffset + 4 * k, "r1"));
      if (!pending.empty()) {
        tail.push_back(pending);
      }
      tail.push_back("// rlwinm r10,r11,0,1,8");
      tail.push_back("// fmr f0,f31");
      tail.push_back(Off("lfs f0,", stack + af::kHistoryOffset + 4 * k, "r1"));
      pending = Off("stfs f0,", object + af::kHistoryOffset + 4 * k, "r31");
    }
  }
  tail.push_back(pending);
  tail.push_back("loc_821475EC:");
  tail.push_back("// li r3,0");
  tail.push_back("// addi r1,r1,256");
  tail.push_back("__restgprlr_27(ctx, base);");
  InOrder(f, tail, "history flush order, the f31 replacement and return 0");
  Check(Has(f, "& 0x7F800000;"), "flush tests the exponent bits");

  // Coefficient routine: uses r4 (low byte), r5 and r6, never r3.
  const std::string coef = Body(g0, "sub_82146AD0");
  InOrder(coef,
          {"// clrldi r11,r5,32", "// mr r30,r4", "// mr r31,r6", "// clrlwi r8,r30,24",
           "// stfs f6,0(r31)", "// stfs f5,4(r31)", "// stfs f4,8(r31)", "// stfs f3,12(r31)",
           "// stfs f2,16(r31)", "// blr"},
          "sub_82146AD0 writes five words at r6");
  Check(!Has(coef, "ctx.r3."), "sub_82146AD0 does not use r3");
  // Descriptor getter: the sample pointer is [arg - 8 + 28].
  InOrder(Body(g25, "sub_823EAF00"),
          {"// addi r11,r3,-8", "// lwz r8,28(r11)", "// stw r8,8(r4)", "// blr"},
          "sub_823EAF00: +88 of the frame is [arg + 20]");
  static_assert(af::kDataPointerOffset == 28 - 8);
  // Engine thread loop: the frame call returns to 0x823EC96C.
  InOrder(Body(g25, "sub_823EC8C8"),
          {"// bl 0x823ec160", "// bl 0x823ec248", "ctx.lr = 0x823EC96C;",
           "sub_823EC248(ctx, base);", "__imp__KeSetEvent(ctx, base);"},
          "engine thread: one frame, then the done event");
  Check(!Body(g0, "sub_82147738").empty(), "sub_82147738 found");
}

// --- l89: MulAdd exactness ---

uint32_t RandomFloatBits() {
  // Any finite float: random sign, exponent 0..254 (0 = zero/denormal),
  // random mantissa.
  const uint32_t r = Next();
  const uint32_t exponent = (r >> 8) % 255u;
  return (r & 0x80000000u) | (exponent << 23) | (Next() & 0x007FFFFFu);
}

bool SameDouble(double a, double b) {
  uint64_t x, y;
  std::memcpy(&x, &a, sizeof(x));
  std::memcpy(&y, &b, sizeof(y));
  return x == y;
}

// One triple: the double product is exact (its fma residual is +-0) and
// MulAdd equals std::fma bit for bit, before and after the round to float.
bool ExactTriple(uint32_t a_bits, uint32_t b_bits, uint32_t c_bits, bool negate) {
  const double a = af::Load(a_bits);
  const double b = af::Load(b_bits);
  const double c = negate ? -af::Load(c_bits) : af::Load(c_bits);
  const double product = a * b;
  const double residual = std::fma(a, b, -product);
  const double fused = std::fma(a, b, c);
  const double split = af::MulAdd(a, b, c);
  return residual == 0.0 && SameDouble(fused, split) &&
         af::FloatToBits(float(fused)) == af::FloatToBits(float(split));
}

void TestMulAddExact() {
  // Adversarial operands: full 24-bit significands, the extremes of the
  // float range (2^-149 * 2^-149 = 2^-298, max * max ~ 2^256), denormals
  // with long mantissas, signed zeros, one ulp around 1, and addends that
  // cancel the product exactly or nearly.
  const uint32_t edges[] = {0x00000000u, 0x80000000u, 0x00000001u, 0x80000001u, 0x007FFFFFu,
                            0x00400001u, 0x00800000u, 0x00FFFFFFu, 0x3F7FFFFFu, 0x3F800000u,
                            0x3F800001u, 0xBFFFFFFFu, 0x3FFFFFFFu, 0x4B7FFFFFu, 0x7F7FFFFFu,
                            0xFF7FFFFFu, 0x5F7FFFFFu, 0x1F800001u, 0x33800000u, 0x0CAAAAABu};
  size_t cases = 0;
  for (const uint32_t a : edges) {
    for (const uint32_t b : edges) {
      for (const uint32_t c : edges) {
        Check(ExactTriple(a, b, c, false) && ExactTriple(a, b, c, true),
              "edge operands: exact product, MulAdd == fma");
        cases += 2;
      }
      // Addends that cancel the product: the float nearest to it, +- one ulp.
      const float p = float(af::Load(a) * af::Load(b));
      if (std::isfinite(p)) {
        const uint32_t pb = af::FloatToBits(p);
        for (const uint32_t c : {pb, pb + 1u, pb - 1u}) {
          if ((c & 0x7F800000u) == 0x7F800000u) {
            continue;
          }
          Check(ExactTriple(a, b, c, true), "cancelling addend: MulAdd == fma");
          ++cases;
        }
      }
    }
  }
  // Random finite float triples, and pairs in the audio range.
  for (int n = 0; n < 1000000; ++n) {
    const uint32_t a = RandomFloatBits();
    const uint32_t b = RandomFloatBits();
    const uint32_t c = RandomFloatBits();
    Check(ExactTriple(a, b, c, (n & 1) != 0), "random floats: exact product, MulAdd == fma");
    const uint32_t x = af::FloatToBits(Uniform(-1.0f, 1.0f));
    const uint32_t k = af::FloatToBits(Uniform(-2.0f, 2.0f));
    const uint32_t h = af::FloatToBits(Uniform(-1.0f, 1.0f));
    Check(ExactTriple(x, k, h, (n & 2) != 0), "audio-range floats: MulAdd == fma");
    cases += 2;
  }
  Check(cases > 2000000, "MulAdd case count");
}

// Every FP register write in a generated function keeps a float-exact double:
// `double(temp.f32)` (lfs/lfsx), `double(float(...))` (single-precision
// arithmetic) or a copy of another register (fmr); and each register is
// written before its first read in the text.
void CheckFloatExact(const std::string &body, const char *what) {
  static const std::regex write(R"(ctx\.f(\d+)\.f64 = (.*);)");
  static const std::regex exact(
      R"(^(double\(temp\.f32\)|double\(float\(.*\)\)|ctx\.f\d+\.f64)$)");
  size_t writes = 0;
  for (std::sregex_iterator it(body.begin(), body.end(), write), end; it != end; ++it) {
    const std::string rhs = (*it)[2].str();
    if (!std::regex_match(rhs, exact)) {
      std::fprintf(stderr, "%s: not float-exact: %s\n", what, it->str().c_str());
      Check(false, what);
    }
    ++writes;
  }
  for (int r = 0; r < 32; ++r) {
    const std::string name = "ctx.f" + std::to_string(r) + ".f64";
    const size_t first = body.find(name);
    if (first == std::string::npos) {
      continue;
    }
    if (body.compare(first, name.size() + 3, name + " = ") != 0) {
      std::fprintf(stderr, "%s: f%d read before written\n", what, r);
      Check(false, what);
    }
  }
  Check(writes > 0, what);
}

// --- l89: interpreter of sub_82147738's two loops ---

enum class ChKind { kLoad, kTempToF, kFma, kMul, kAdd, kFToTemp, kStore, kAddi };

struct ChOp {
  ChKind kind;
  int d = 0, a = 0, b = 0, c = 0;
  bool negate = false;
  int reg = 0;  // address register (kLoad/kStore/kAddi destination)
  int offset = 0;
};

std::vector<ChOp> ParseChannelLoop(const std::string &loop, size_t &unknown) {
  static const std::regex load(R"(^temp\.u32 = REX_LOAD_U32\(ctx\.r(\d+)\.u32 \+ (-?\d+)\);$)");
  static const std::regex temp_to_f(R"(^ctx\.f(\d+)\.f64 = double\(temp\.f32\);$)");
  static const std::regex fma(
      R"(^ctx\.f(\d+)\.f64 = double\(float\(std::fma\(ctx\.f(\d+)\.f64, ctx\.f(\d+)\.f64, )"
      R"((-?)ctx\.f(\d+)\.f64\)\)\);$)");
  static const std::regex bin(
      R"(^ctx\.f(\d+)\.f64 = double\(float\(ctx\.f(\d+)\.f64 ([*+]) ctx\.f(\d+)\.f64\)\);$)");
  static const std::regex f_to_temp(R"(^temp\.f32 = float\(ctx\.f(\d+)\.f64\);$)");
  static const std::regex store(
      R"(^REX_STORE_U32\(ctx\.r(\d+)\.u32 \+ (-?\d+), temp\.u32\);$)");
  static const std::regex addi(R"(^ctx\.r(\d+)\.s64 = ctx\.r11\.s64 \+ (-?\d+);$)");
  // The loop counter (addic. r10,r10,-1): not part of the arithmetic.
  static const std::regex counter(
      R"(^(ctx\.xer\.ca = ctx\.r10\.u32 > 0;|ctx\.r10\.s64 = ctx\.r10\.s64 \+ -1;|)"
      R"(ctx\.cr0\.compare<int32_t>\(ctx\.r10\.s32, 0, ctx\.xer\);)$)");
  std::vector<ChOp> ops;
  std::istringstream in(loop);
  std::string raw;
  unknown = 0;
  while (std::getline(in, raw)) {
    std::string l = raw;
    while (!l.empty() && (l.back() == '\r' || l.back() == ' ' || l.back() == '\t')) {
      l.pop_back();
    }
    const size_t first = l.find_first_not_of(" \t");
    l = first == std::string::npos ? std::string() : l.substr(first);
    if (l.empty() || l.rfind("//", 0) == 0 || l.rfind("rex::ppc::SetGuestProgramCounter", 0) == 0 ||
        l == "ctx.fpscr.disableFlushMode();" || std::regex_match(l, counter) ||
        std::regex_match(l, std::regex(R"(^loc_[0-9A-F]+:$)"))) {
      continue;
    }
    std::smatch m;
    ChOp op{};
    if (std::regex_match(l, m, load)) {
      op.kind = ChKind::kLoad;
      op.reg = std::stoi(m[1].str());
      op.offset = std::stoi(m[2].str());
    } else if (std::regex_match(l, m, temp_to_f)) {
      op.kind = ChKind::kTempToF;
      op.d = std::stoi(m[1].str());
    } else if (std::regex_match(l, m, fma)) {
      op.kind = ChKind::kFma;
      op.d = std::stoi(m[1].str());
      op.a = std::stoi(m[2].str());
      op.b = std::stoi(m[3].str());
      op.negate = m[4].str() == "-";
      op.c = std::stoi(m[5].str());
    } else if (std::regex_match(l, m, bin)) {
      op.kind = m[3].str() == "*" ? ChKind::kMul : ChKind::kAdd;
      op.d = std::stoi(m[1].str());
      op.a = std::stoi(m[2].str());
      op.b = std::stoi(m[4].str());
    } else if (std::regex_match(l, m, f_to_temp)) {
      op.kind = ChKind::kFToTemp;
      op.a = std::stoi(m[1].str());
    } else if (std::regex_match(l, m, store)) {
      op.kind = ChKind::kStore;
      op.reg = std::stoi(m[1].str());
      op.offset = std::stoi(m[2].str());
    } else if (std::regex_match(l, m, addi)) {
      op.kind = ChKind::kAddi;
      op.reg = std::stoi(m[1].str());
      op.offset = std::stoi(m[2].str());
    } else {
      std::fprintf(stderr, "unknown channel loop statement: %s\n", l.c_str());
      ++unknown;
      continue;
    }
    ops.push_back(op);
  }
  return ops;
}

// The 32 iterations of one parsed loop on one channel block: f0, f13, f12,
// f11, f10 = c0..c4 (+12..+28), f9, f8, f7, f6 = h0..h3 on entry and exit,
// r11 = the block, r31+60 = the gain.
void RunChannelLoop(const std::vector<ChOp> &ops, const af::Bank &bank, uint32_t gain,
                    std::array<uint32_t, af::kSamples> &samples,
                    std::array<uint32_t, af::kHistoryWords> &history) {
  double f[32] = {};
  f[0] = af::Load(bank.c[0]);
  f[13] = af::Load(bank.c[1]);
  f[12] = af::Load(bank.c[2]);
  f[11] = af::Load(bank.c[3]);
  f[10] = af::Load(bank.c[4]);
  f[9] = af::Load(bank.h[0]);
  f[8] = af::Load(bank.h[1]);
  f[7] = af::Load(bank.h[2]);
  f[6] = af::Load(bank.h[3]);
  int64_t r[32] = {};
  uint32_t temp = 0;
  for (uint32_t iteration = 0; iteration < af::kIterations; ++iteration) {
    for (const ChOp &op : ops) {
      switch (op.kind) {
        case ChKind::kLoad:
          if (op.reg == 31) {
            Check(op.offset == int(af::kChannelGain), "the loop reads only the gain field");
            temp = gain;
          } else {
            temp = samples.at(size_t((r[op.reg] + op.offset) / 4));
          }
          break;
        case ChKind::kTempToF:
          f[op.d] = double(af::BitsToFloat(temp));
          break;
        case ChKind::kFma:
          f[op.d] = double(float(std::fma(f[op.a], f[op.b], op.negate ? -f[op.c] : f[op.c])));
          break;
        case ChKind::kMul:
          f[op.d] = double(float(f[op.a] * f[op.b]));
          break;
        case ChKind::kAdd:
          f[op.d] = double(float(f[op.a] + f[op.b]));
          break;
        case ChKind::kFToTemp:
          temp = af::FloatToBits(float(f[op.a]));
          break;
        case ChKind::kStore:
          Check(op.reg != 31, "the loop writes no object field");
          samples.at(size_t((r[op.reg] + op.offset) / 4)) = temp;
          break;
        case ChKind::kAddi:
          r[op.reg] = r[11] + op.offset;
          break;
      }
    }
  }
  Check(r[11] == int64_t(af::kSamples * 4), "the loop walks the 256 samples");
  history = {af::FloatToBits(float(f[9])), af::FloatToBits(float(f[8])),
             af::FloatToBits(float(f[7])), af::FloatToBits(float(f[6]))};
}

bool CompareChannelCase(const std::vector<ChOp> &ops, const af::Bank &bank, bool cascade,
                        uint32_t gain, Input input, uint32_t zero_bits) {
  std::array<uint32_t, af::kSamples> samples{};
  Fill(input, samples);
  std::array<uint32_t, af::kSamples> generated = samples;
  std::array<uint32_t, af::kHistoryWords> history{};
  RunChannelLoop(ops, bank, gain, generated, history);
  for (uint32_t &word : history) {
    word = af::FlushHistoryWord(word, zero_bits);
  }
  af::BankState s = af::LoadBank(bank);
  std::array<uint32_t, af::kSamples> native = samples;
  int gain_reads = 0;
  af::ProcessChannel(s, cascade, native.data(), [&] {
    ++gain_reads;
    return gain;
  });
  const auto out = af::ChannelHistoryOut(s, zero_bits);
  af::BankState l = af::LoadBank(bank);
  std::array<uint32_t, af::kSamples> library = samples;
  af::ProcessChannel<af::LibraryFma>(l, cascade, library.data(), [&] { return gain; });
  return generated == native && library == native && history == out &&
         af::ChannelHistoryOut(l, zero_bits) == out && gain_reads == int(af::kSamples);
}

void TestChannelAgainstGenerated() {
  const std::string dir = MCLA_AFILT_GENERATED_DIR;
  const std::string g0 = ReadFile(dir + "/mcla_recomp.0.cpp");
  const std::string body = Body(g0, "sub_82147738");
  Check(!body.empty(), "sub_82147738 found");
  CheckFloatExact(body, "sub_82147738: every FP register write is float-exact");
  CheckFloatExact(Body(g0, "sub_82146BE0"), "sub_82146BE0: every FP register write is float-exact");
  const std::string cascade_loop = Between(body, "loc_821477FC:", "// bne 0x821477fc");
  const std::string single_loop = Between(body, "loc_82147A20:", "// bne 0x82147a20");
  Check(!cascade_loop.empty() && !single_loop.empty(), "both channel loops found");
  size_t unknown = 0;
  const std::vector<ChOp> cascade_ops = ParseChannelLoop(cascade_loop, unknown);
  Check(unknown == 0, "every cascade loop statement understood");
  const std::vector<ChOp> single_ops = ParseChannelLoop(single_loop, unknown);
  Check(unknown == 0, "every single loop statement understood");
  auto shape = [](const std::vector<ChOp> &ops, ChKind kind, bool gain_load) {
    size_t n = 0;
    for (const ChOp &op : ops) {
      if (op.kind == kind && (kind != ChKind::kLoad || (op.reg == 31) == gain_load)) {
        ++n;
      }
    }
    return n;
  };
  // Cascade: 8 x (6 fma, 4 + 1 mul, 2 add); single: 8 x (3 fma, 2 + 1 mul, 1 add);
  // 8 samples in, 8 gain loads, 8 samples out each.
  Check(shape(cascade_ops, ChKind::kFma, false) == 48 &&
            shape(cascade_ops, ChKind::kMul, false) == 40 &&
            shape(cascade_ops, ChKind::kAdd, false) == 16 &&
            shape(cascade_ops, ChKind::kLoad, false) == 8 &&
            shape(cascade_ops, ChKind::kLoad, true) == 8 &&
            shape(cascade_ops, ChKind::kStore, false) == 8,
        "cascade loop shape");
  Check(shape(single_ops, ChKind::kFma, false) == 24 &&
            shape(single_ops, ChKind::kMul, false) == 24 &&
            shape(single_ops, ChKind::kAdd, false) == 8 &&
            shape(single_ops, ChKind::kLoad, false) == 8 &&
            shape(single_ops, ChKind::kLoad, true) == 8 &&
            shape(single_ops, ChKind::kStore, false) == 8,
        "single loop shape");
  Check(Has(body, "// fmadds") && std::string::npos == body.find("std::fma", body.find(
                                                                     "loc_82147B6C:")),
        "no fma after the loops");

  const uint32_t zeros[] = {0x00000000u, 0x3F800000u};
  const uint32_t gains[] = {0x3F800000u, 0x3F000000u, 0x00000000u, 0x80000000u, 0x00000123u,
                            af::FloatToBits(0.7071f)};
  const Input inputs[] = {Input::kRandom, Input::kSilent, Input::kDenormal,
                          Input::kImpulse, Input::kLarge, Input::kTiny};
  const double cutoffs[] = {80.0, 400.0, 2500.0, 9000.0, 20000.0};
  size_t cases = 0;
  for (const double cutoff : cutoffs) {
    for (const Input input : inputs) {
      for (const bool cascade : {true, false}) {
        for (const uint32_t zero : zeros) {
          const uint32_t fills[] = {0u, af::FloatToBits(Uniform(-0.5f, 0.5f)), 0x00012345u};
          for (const uint32_t fill : fills) {
            const af::Bank bank = Design(cutoff, (cases & 1) != 0, fill);
            const uint32_t gain = gains[cases % 6];
            Check(CompareChannelCase(cascade ? cascade_ops : single_ops, bank, cascade, gain,
                                     input, zero),
                  "native channel kernel bit-identical to the generated loop");
            ++cases;
          }
        }
      }
    }
  }
  for (int n = 0; n < 40; ++n) {
    af::Bank bank;
    for (uint32_t k = 0; k < af::kCoefficients; ++k) {
      bank.c[k] = af::FloatToBits(Uniform(-0.45f, 0.45f));
    }
    for (uint32_t k = 0; k < af::kHistoryWords; ++k) {
      bank.h[k] = af::FloatToBits(Uniform(-1.0f, 1.0f));
    }
    const uint32_t gain = af::FloatToBits(Uniform(-2.0f, 2.0f));
    for (const bool cascade : {true, false}) {
      Check(CompareChannelCase(cascade ? cascade_ops : single_ops, bank, cascade, gain,
                               Input::kRandom, 0),
            "random channel coefficients: bit-identical");
      ++cases;
    }
  }
  Check(cases == 5 * 6 * 2 * 2 * 3 + 40 * 2, "channel case count");
}

void TestChannelGenerated() {
  const std::string dir = MCLA_AFILT_GENERATED_DIR;
  const std::string g0 = ReadFile(dir + "/mcla_recomp.0.cpp");
  const std::string g25 = ReadFile(dir + "/mcla_recomp.25.cpp");
  const std::string f = Body(g0, "sub_82147738");
  Check(Has(f, "// stwu r1,-" + std::to_string(af::kChannelFrameSize) + "(r1)"), "frame 192");
  InOrder(f,
          {"// mr r31,r3", "// mr r3,r4", Off("lbz r11,", af::kChannelBypassed, "r31"),
           "if (!ctx.cr6.eq) goto loc_82147BFC;", "// addi r4,r1,96", "// bl 0x823eaf00",
           "sub_823EAF00(ctx, base);", "// lbz r10,97(r1)",
           Off("lfs f0,", af::kChannelCoefficients, "r31"),
           Off("lfs f13,", af::kChannelCoefficients + 4, "r31"),
           Off("lfs f12,", af::kChannelCoefficients + 8, "r31"),
           Off("lfs f11,", af::kChannelCoefficients + 12, "r31"),
           Off("lfs f10,", af::kChannelCoefficients + 16, "r31"),
           "if (ctx.cr6.eq) goto loc_82147BFC;", "// lis r11,-32256", "// li r6,0", "// li r5,0",
           "// li r3,1", "// lfs f5,3796(r11)"},
          "channel entry: bypass byte, descriptor, count, coefficients, f5");
  InOrder(f,
          {"loc_821477A8:", Off("lbz r11,", af::kChannelMask, "r31"), "// slw r10,r3,r4",
           "ctx.r10.u64 = ctx.r4.u8 & 0x20 ? 0 : (ctx.r3.u32 << (ctx.r4.u8 & 0x3F));",
           "// and r9,r10,r11", "if (ctx.cr6.eq) goto loc_82147BE8;",
           Off("lwz r10,", af::kChannelHistory[2], "r31"),
           Off("lwz r9,", af::kChannelHistory[3], "r31"),
           Off("lwz r8,", af::kChannelHistory[0], "r31"),
           Off("lwz r7,", af::kChannelHistory[1], "r31"),
           Off("lbz r30,", af::kChannelCascade, "r31"), "// lwz r11,104(r1)",
           "// lfsx f7,r6,r10", "// lfsx f6,r9,r6", "// lfsx f9,r8,r6", "// add r11,r5,r11",
           "// lfsx f8,r7,r6", "// li r10," + std::to_string(af::kIterations),
           "// stfs f7,88(r1)", "// stfs f6,92(r1)", "if (ctx.cr6.eq) goto loc_82147A20;",
           "loc_821477FC:"},
          "channel mask, history pointers, cascade byte and loop setup");
  InOrder(f,
          {"// bne 0x821477fc", "// stfs f7,88(r1)", "// stfs f6,92(r1)", "// b 0x82147b6c",
           "loc_82147A20:", "// bne 0x82147a20", "loc_82147B6C:", "// stfs f9,80(r1)",
           "// rlwinm r10,r11,0,1,8", "// stfs f8,84(r1)", "// fmr f9,f5",
           Off("lwz r11,", af::kChannelHistory[0], "r31"), "// stfsx f9,r11,r6",
           "// fmr f8,f5", Off("lwz r11,", af::kChannelHistory[1], "r31"),
           "// stfsx f8,r11,r6", "// fmr f7,f5", Off("lwz r11,", af::kChannelHistory[2], "r31"),
           "// stfsx f7,r6,r11", "// fmr f6,f5", Off("lwz r11,", af::kChannelHistory[3], "r31"),
           "// stfsx f6,r11,r6", "// addi r6,r6,4", "loc_82147BE8:", "// lbz r10,97(r1)",
           "// addi r4,r4,1", "// addi r5,r5," + std::to_string(af::kChannelStride),
           "// cmplw cr6,r4,r10", "if (ctx.cr6.lt) goto loc_821477A8;", "loc_82147BFC:",
           "// li r3,0", "// addi r1,r1,192", "__restfpr_25(ctx, base);", "// blr"},
          "history flush h0, h1, h2, h3 with f5; channel stride 1024; return 0");
  Check(!Has(f, "REX_STORE_U32(ctx.r31.u32") && !Has(f, "REX_STORE_U8(ctx.r31.u32"),
        "sub_82147738 writes no object field");
  // Descriptor: r1+96 = [arg - 8 + 20], so byte 97 is the byte at arg + 13.
  InOrder(Body(g25, "sub_823EAF00"), {"// lwz r10,20(r11)", "// stw r10,0(r4)"},
          "sub_823EAF00: +96 of the frame is [arg + 12]");
  static_assert(af::kChannelCountOffset == 20 - 8 + 1);
  // The probed leaves: no calls of their own, reached from the engine code.
  for (const char *leaf : {"sub_823F4450", "sub_823EFD30", "sub_823EE050"}) {
    const std::string b = Body(g25, leaf);
    Check(!b.empty() && !Has(b, "(ctx, base);"), "probed leaf found, makes no call");
  }
  Check(Has(Body(g25, "sub_823F4570"), "sub_823F4450(ctx, base);") &&
            Has(Body(g25, "sub_823EBEC0"), "sub_823EFD30(ctx, base);") &&
            Has(Body(g25, "sub_823EC248"), "sub_823EBEC0(ctx, base);"),
        "callers of the probed leaves");
}

void TestL89Pure() {
  Check(af::ChannelOn(0x01, 0) && !af::ChannelOn(0x01, 1) && af::ChannelOn(0x80, 7) &&
            !af::ChannelOn(0xFF, 8) && !af::ChannelOn(0xFF, 32) && !af::ChannelOn(0xFF, 63) &&
            af::ChannelOn(0x01, 64) && af::ChannelOn(0x02, 65) && !af::ChannelOn(0xFF, 96),
        "channel bit: slw semantics (shift amount & 0x3F, 32..63 give 0)");
  af::Object before;
  before.cutoff_a = 1000;
  before.param_b = 3;
  af::Object after = before;
  after.cache_a = 1000;
  after.cache_b = 3;
  Check(!af::FilterRaced(before, after), "no race when the caches match the copied parameters");
  after.cache_a = 1001;
  Check(af::FilterRaced(before, after), "race: the original saw another cutoff");
  after.cache_a = 1000;
  after.param_b = 4;
  Check(af::FilterRaced(before, after), "race: the parameter changed meanwhile");
  af::ChannelInputs x, y;
  Check(x == y, "equal channel inputs");
  y.gain = 1;
  Check(!(x == y), "a changed gain is a changed input");
  y = x;
  y.history[3] = 4;
  Check(!(x == y), "a changed history pointer is a changed input");
  af::PassStats p;
  p.calls = 100;
  p.timed = 50;
  p.ns = 5000;
  Check(p.NsPerFrame(10) == 1000 && af::PassStats{}.NsPerFrame(10) == 0 && p.NsPerFrame(0) == 0,
        "leaf ns per frame scaled to all calls");
  af::Stats s;
  s.frames = 10;
  s.raced = 2;
  s.ch_native = 7;
  s.mix = p;
  af::Stats t;
  t.Merge(s);
  t.Merge(s);
  Check(t.raced == 4 && t.ch_native == 14 && t.mix.calls == 200 && t.mix.ns == 10000,
        "l89 counters merge");
  const std::string line = af::Line(s, 1000000000ull, true, true);
  Check(line.find(" vskip=0 raced=2 ch_nat=7 ch_fallback=0 ch_verified=0 ch_mismatch=0 "
                  "ch_raced=0 ch_vskip=0 mix_calls=100 mix_out=0 mix_timed=50 mix_ns=5000 "
                  "mix_per_frame=10.00 mix_ns_per_frame=1000 add_calls=0") != std::string::npos,
        "line: l89 fields after vskip");
  Check(line.size() < 2048 && line.find("kick_ns_per_frame=0") != std::string::npos,
        "line: kick fields last");
  const std::string json = af::StateJson(s, 1000000000ull, true, true, true);
  Check(json.find("\"verify_skipped\": 0, \"raced\": 2, \"ch_native_calls\": 7, ") !=
                std::string::npos &&
            json.find("\"mix\": {\"calls\": 100, \"outside\": 0, \"timed\": 50, \"ns\": 5000, "
                      "\"ns_per_frame\": 1000}") != std::string::npos &&
            json.rfind("}}") == json.size() - 2,
        "F10 object: l89 fields");
  af::Mismatch m;
  m.Check("ch2.sample", 17, 1, 2);
  m.Check("ch2.h", 0, 3, 4);
  Check(m.count == 2 && m.field == "ch2.sample17" && m.want == 1 && m.got == 2,
        "mismatch keeps the first field");
  Check(af::ChannelVerifyLine(1, 64, m, 2, 0x03, true) ==
            "MCLA_AFILT_CH_VERIFY v=1 n=1 call=64 channels=2 mask=0x03 cascade=1 diffs=2 "
            "field=ch2.sample17 want=0x00000001 got=0x00000002",
        "channel verify line");
}

}  // namespace

// l98: the native default, the lean switch, the config line and the lean F10
// object.
void TestL98Pure() {
  Check(af::kNativeDefault, "l98: the native voice filters are default on");
  Check(af::Lean(true, false, false) && !af::Lean(false, false, false) &&
            !af::Lean(true, true, false) && !af::Lean(true, false, true) &&
            !af::Lean(true, true, true) && !af::Lean(false, true, true),
        "lean only with native on and the self-check and the probe off");
  Check(af::ConfigLine(true, false, false, false) ==
            "MCLA_AFILT v=1 kind=config native=1 verify=0 probe=0 ch_off=0 lean=1",
        "config line of the l98 default");
  Check(af::ConfigLine(false, false, false, false) ==
            "MCLA_AFILT v=1 kind=config native=0 verify=0 probe=0 ch_off=0 lean=0",
        "config line of the audio-off control");
  Check(af::ConfigLine(true, true, true, true) ==
            "MCLA_AFILT v=1 kind=config native=1 verify=1 probe=1 ch_off=1 lean=0",
        "config line with every switch on");
  Check(af::LeanStateJson(0, 0, false) ==
            "{\"version\": 1, \"probe\": false, \"native\": true, \"verify\": false, "
            "\"lean\": true, \"ch_off\": false, \"fallback\": 0, \"ch_fallback\": 0}",
        "lean F10 object");
  Check(af::LeanStateJson(3, 18446744073709551615ull, true) ==
            "{\"version\": 1, \"probe\": false, \"native\": true, \"verify\": false, "
            "\"lean\": true, \"ch_off\": true, \"fallback\": 3, "
            "\"ch_fallback\": 18446744073709551615}",
        "lean F10 object: counts and the channel control");
}

int main() {
  TestPure();
  TestL98Pure();
  TestStats();
  TestL89Pure();
  TestHandTransliteration();
  TestKernelAgainstGenerated();
  TestGenerated();
  TestMulAddExact();
  TestChannelAgainstGenerated();
  TestChannelGenerated();
  std::printf("mcla_audio_filter_test: %d checks passed\n", checks);
  return 0;
}
