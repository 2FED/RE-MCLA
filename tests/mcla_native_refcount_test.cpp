// M10 CPU task 1: the native AddRef/Release and XAPI interlocked helpers
// (src/mcla_native_refcount.h) against a line-by-line transliteration of the
// generated bodies (generated/default/mcla_recomp.27.cpp sub_82422160 and
// sub_824221D8, mcla_recomp.5.cpp sub_821CE080-sub_821CE128) on a fake guest
// context and a fake big-endian guest memory buffer. The global lock of the
// generated code is single-threaded here, so mfmsr returns 0x8000 as
// REX_CHECK_GLOBAL_LOCK() does outside a locked region.

#include "mcla_native_refcount.h"

#include <array>
#include <atomic>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <thread>
#include <vector>

namespace {

int checks = 0;
int failures = 0;
void Check(bool ok, const char *label) {
  ++checks;
  if (!ok) {
    ++failures;
    std::fprintf(stderr, "FAIL: %s\n", label);
  }
}

// Same layout and semantics as rex::ppc::Register / CRRegister / XERRegister.
union Register {
  int8_t s8;
  uint8_t u8;
  int16_t s16;
  uint16_t u16;
  int32_t s32;
  uint32_t u32;
  int64_t s64;
  uint64_t u64;
};
struct XER {
  uint8_t so, ov, ca;
};
struct CR {
  uint8_t lt, gt, eq, so;
  template <typename T>
  void compare(T left, T right, const XER &xer) {
    lt = left < right;
    gt = left > right;
    eq = left == right;
    so = xer.so;
  }
  bool operator==(const CR &) const = default;
};

struct Context {
  std::array<Register, 32> gpr{};
  Register &r1 = gpr[1];
  Register &r3 = gpr[3];
  Register &r4 = gpr[4];
  Register &r5 = gpr[5];
  Register &r9 = gpr[9];
  Register &r10 = gpr[10];
  Register &r11 = gpr[11];
  Register &r12 = gpr[12];
  Register &r13 = gpr[13];
  Register &r31 = gpr[31];
  uint64_t lr = 0;
  Register ctr{};
  XER xer{};
  Register reserved{};
  uint32_t msr = 0x200A000;
  CR cr0{}, cr1{}, cr2{}, cr3{}, cr4{}, cr5{}, cr6{}, cr7{};

  Context() = default;
  Context(const Context &o) { *this = o; }
  Context &operator=(const Context &o) {
    gpr = o.gpr;
    lr = o.lr;
    ctr = o.ctr;
    xer = o.xer;
    reserved = o.reserved;
    msr = o.msr;
    cr0 = o.cr0; cr1 = o.cr1; cr2 = o.cr2; cr3 = o.cr3;
    cr4 = o.cr4; cr5 = o.cr5; cr6 = o.cr6; cr7 = o.cr7;
    return *this;
  }
};

bool SameContext(const Context &a, const Context &b) {
  for (size_t i = 0; i < 32; ++i)
    if (a.gpr[i].u64 != b.gpr[i].u64) {
      std::fprintf(stderr, "  r%zu %016llX != %016llX\n", i,
                   static_cast<unsigned long long>(a.gpr[i].u64),
                   static_cast<unsigned long long>(b.gpr[i].u64));
      return false;
    }
  const bool same = a.lr == b.lr && a.ctr.u64 == b.ctr.u64 && a.xer.so == b.xer.so &&
                    a.xer.ov == b.xer.ov && a.xer.ca == b.xer.ca &&
                    a.reserved.u64 == b.reserved.u64 && a.msr == b.msr && a.cr0 == b.cr0 &&
                    a.cr1 == b.cr1 && a.cr2 == b.cr2 && a.cr3 == b.cr3 && a.cr4 == b.cr4 &&
                    a.cr5 == b.cr5 && a.cr6 == b.cr6 && a.cr7 == b.cr7;
  if (!same)
    std::fprintf(stderr, "  lr %llX/%llX msr %X/%X res %llX/%llX cr0 %d%d%d%d/%d%d%d%d cr6 %d%d%d%d/%d%d%d%d\n",
                 static_cast<unsigned long long>(a.lr), static_cast<unsigned long long>(b.lr), a.msr,
                 b.msr, static_cast<unsigned long long>(a.reserved.u64),
                 static_cast<unsigned long long>(b.reserved.u64), a.cr0.lt, a.cr0.gt, a.cr0.eq,
                 a.cr0.so, b.cr0.lt, b.cr0.gt, b.cr0.eq, b.cr0.so, a.cr6.lt, a.cr6.gt, a.cr6.eq,
                 a.cr6.so, b.cr6.lt, b.cr6.gt, b.cr6.eq, b.cr6.so);
  return same;
}

// Fake guest memory: guest address == offset, big-endian words.
constexpr uint32_t kMemorySize = 0x10000;
constexpr uint32_t kStackTop = 0xF000;
struct Memory {
  alignas(16) std::array<uint8_t, kMemorySize> bytes{};
  uint32_t *Word(uint32_t ea) { return reinterpret_cast<uint32_t *>(bytes.data() + ea); }
  uint32_t Load32(uint32_t ea) const {
    uint32_t v;
    std::memcpy(&v, bytes.data() + ea, 4);
    return __builtin_bswap32(v);
  }
  uint64_t Load64(uint32_t ea) const {
    uint64_t v;
    std::memcpy(&v, bytes.data() + ea, 8);
    return __builtin_bswap64(v);
  }
  void Store32(uint32_t ea, uint32_t value) {
    value = __builtin_bswap32(value);
    std::memcpy(bytes.data() + ea, &value, 4);
  }
  void Store64(uint32_t ea, uint64_t value) {
    value = __builtin_bswap64(value);
    std::memcpy(bytes.data() + ea, &value, 8);
  }
};

// Memory equal except the dead area below the caller's stack pointer, where
// the generated bodies store frames they pop again before returning (the
// native fast paths do not store them).
bool SameMemory(const Memory &a, const Memory &b, uint32_t sp) {
  for (uint32_t i = 0; i < kMemorySize; ++i) {
    if (i >= sp - 0x1000 && i < sp) continue;
    if (a.bytes[i] != b.bytes[i]) {
      std::fprintf(stderr, "  memory differs at %X\n", i);
      return false;
    }
  }
  return true;
}

// ---- Reference: the generated bodies, transliterated ----------------------

constexpr uint64_t kCheckGlobalLock = 0x8000;  // REX_CHECK_GLOBAL_LOCK() unlocked
#define REF_STWCX(m, ea, value)                                                        \
  do {                                                                                 \
    ctx.cr0.lt = 0;                                                                    \
    ctx.cr0.gt = 0;                                                                    \
    ctx.cr0.eq = __sync_bool_compare_and_swap((m).Word(ea), ctx.reserved.u32,          \
                                              __builtin_bswap32(value));               \
    ctx.cr0.so = ctx.xer.so;                                                           \
  } while (0)

struct DestroyLog {
  std::vector<uint32_t> objects;
  std::vector<uint64_t> lrs;
};

// The guest destroy sub_82421AA8 stand-in: records the call and clobbers the
// volatile registers like any callee may (r31 is preserved, as the ABI says).
void FakeDestroy(Context &ctx, Memory &mem, DestroyLog &log) {
  log.objects.push_back(ctx.r3.u32);
  log.lrs.push_back(ctx.lr);
  mem.Store32(ctx.r1.u32 - 32, 0xD5D5D5D5);  // its own frame below SP
  for (int r = 3; r <= 12; ++r) ctx.gpr[r].u64 = 0xDEAD000000000000ull | (r * 0x111u);
  ctx.cr0 = {1, 0, 0, 0};
  ctx.cr6 = {0, 1, 0, 0};
  ctx.ctr.u64 = 0xC7C7;
}

void RefAddRef(Context &ctx, Memory &mem) {
  uint32_t ea{};
  ctx.r12.u64 = ctx.lr;
  mem.Store32(ctx.r1.u32 + -8, ctx.r12.u32);
  mem.Store64(ctx.r1.u32 + -16, ctx.gpr[31].u64);
  ea = -96 + ctx.r1.u32;
  mem.Store32(ea, ctx.r1.u32);
  ctx.r1.u32 = ea;
  ctx.r11.s64 = ctx.r3.s64 + 4;
loc_82422174:
  ctx.r9.u64 = kCheckGlobalLock;
  ctx.msr = (ctx.r13.u32 & 0x8020) | (ctx.msr & ~0x8020);
  ea = ctx.r11.u32;
  ctx.reserved.u32 = *mem.Word(ea);
  ctx.r10.u64 = __builtin_bswap32(ctx.reserved.u32);
  ctx.r10.s64 = ctx.r10.s64 + 1;
  ea = ctx.r11.u32;
  REF_STWCX(mem, ea, ctx.r10.u32);
  ctx.msr = (ctx.r9.u32 & 0x8020) | (ctx.msr & ~0x8020);
  if (!ctx.cr0.eq) goto loc_82422174;
  ctx.r31.u64 = ctx.r10.u64;
  ctx.cr6.compare<uint32_t>(ctx.r31.u32, 1, ctx.xer);
  if (!ctx.cr6.eq) goto loc_824221BC;
  ctx.r11.u64 = mem.Load32(ctx.r3.u32 + 0);
  ctx.r10.u64 = ctx.r11.u32 & 0xF;
  ctx.cr6.compare<uint32_t>(ctx.r10.u32, 4, ctx.xer);
  if (!ctx.cr6.eq) goto loc_824221BC;
  ctx.r11.u64 = __builtin_rotateleft64(ctx.r11.u32 | (ctx.r11.u64 << 32), 0) & 0x40000000;
  ctx.cr0.compare<int32_t>(ctx.r11.s32, 0, ctx.xer);
  if (ctx.cr0.eq) goto loc_824221BC;
  ctx.r3.u64 = mem.Load32(ctx.r3.u32 + 24);
  ctx.lr = 0x824221BC;
  RefAddRef(ctx, mem);
loc_824221BC:
  ctx.r3.u64 = ctx.r31.u64;
  ctx.r1.s64 = ctx.r1.s64 + 96;
  ctx.r12.u64 = mem.Load32(ctx.r1.u32 + -8);
  ctx.lr = ctx.r12.u64;
  ctx.r31.u64 = mem.Load64(ctx.r1.u32 + -16);
}

void RefRelease(Context &ctx, Memory &mem, DestroyLog &log) {
  uint32_t ea{};
  ctx.r12.u64 = ctx.lr;
  mem.Store32(ctx.r1.u32 + -8, ctx.r12.u32);
  mem.Store64(ctx.r1.u32 + -16, ctx.r31.u64);
  ea = -96 + ctx.r1.u32;
  mem.Store32(ea, ctx.r1.u32);
  ctx.r1.u32 = ea;
  ctx.r31.u64 = ctx.r3.u64;
  ctx.r11.s64 = ctx.r31.s64 + 4;
loc_824221F0:
  ctx.r9.u64 = kCheckGlobalLock;
  ctx.msr = (ctx.r13.u32 & 0x8020) | (ctx.msr & ~0x8020);
  ea = ctx.r11.u32;
  ctx.reserved.u32 = *mem.Word(ea);
  ctx.r10.u64 = __builtin_bswap32(ctx.reserved.u32);
  ctx.r10.s64 = ctx.r10.s64 + -1;
  ea = ctx.r11.u32;
  REF_STWCX(mem, ea, ctx.r10.u32);
  ctx.msr = (ctx.r9.u32 & 0x8020) | (ctx.msr & ~0x8020);
  if (!ctx.cr0.eq) goto loc_824221F0;
  ctx.r3.u64 = ctx.r10.u64;
  ctx.cr6.compare<uint32_t>(ctx.r3.u32, 0, ctx.xer);
  if (!ctx.cr6.eq) goto loc_82422244;
  ctx.r11.u64 = mem.Load32(ctx.r31.u32 + 0);
  ctx.r10.u64 = ctx.r11.u32 & 0xF;
  ctx.cr6.compare<uint32_t>(ctx.r10.u32, 4, ctx.xer);
  if (!ctx.cr6.eq) goto loc_82422238;
  ctx.r11.u64 = __builtin_rotateleft64(ctx.r11.u32 | (ctx.r11.u64 << 32), 0) & 0x40000000;
  ctx.cr0.compare<int32_t>(ctx.r11.s32, 0, ctx.xer);
  if (ctx.cr0.eq) goto loc_82422238;
  ctx.r3.u64 = mem.Load32(ctx.r31.u32 + 24);
  ctx.lr = 0x82422238;
  RefRelease(ctx, mem, log);
loc_82422238:
  ctx.r3.u64 = ctx.r31.u64;
  ctx.lr = 0x82422240;
  FakeDestroy(ctx, mem, log);
  ctx.r3.s64 = 0;
loc_82422244:
  ctx.r1.s64 = ctx.r1.s64 + 96;
  ctx.r12.u64 = mem.Load32(ctx.r1.u32 + -8);
  ctx.lr = ctx.r12.u64;
  ctx.r31.u64 = mem.Load64(ctx.r1.u32 + -16);
}

void RefExchange(Context &ctx, Memory &mem) {  // sub_821CE080
  uint32_t ea{};
loc_821CE080:
  ctx.r10.u64 = kCheckGlobalLock;
  ctx.msr = (ctx.r13.u32 & 0x8020) | (ctx.msr & ~0x8020);
  ea = ctx.r3.u32;
  ctx.reserved.u32 = *mem.Word(ea);
  ctx.r11.u64 = __builtin_bswap32(ctx.reserved.u32);
  ea = ctx.r3.u32;
  REF_STWCX(mem, ea, ctx.r4.u32);
  ctx.msr = (ctx.r10.u32 & 0x8020) | (ctx.msr & ~0x8020);
  if (!ctx.cr0.eq) goto loc_821CE080;
  ctx.r3.u64 = ctx.r11.u64;
}

void RefCompareExchange(Context &ctx, Memory &mem) {  // sub_821CE0A0
  uint32_t ea{};
loc_821CE0A0:
  ctx.r10.u64 = kCheckGlobalLock;
  ctx.msr = (ctx.r13.u32 & 0x8020) | (ctx.msr & ~0x8020);
  ea = ctx.r3.u32;
  ctx.reserved.u32 = *mem.Word(ea);
  ctx.r11.u64 = __builtin_bswap32(ctx.reserved.u32);
  ctx.cr6.compare<int32_t>(ctx.r11.s32, ctx.r5.s32, ctx.xer);
  if (!ctx.cr6.eq) goto loc_821CE0C8;
  ea = ctx.r3.u32;
  REF_STWCX(mem, ea, ctx.r4.u32);
  ctx.msr = (ctx.r10.u32 & 0x8020) | (ctx.msr & ~0x8020);
  if (!ctx.cr0.eq) goto loc_821CE0A0;
  ctx.r3.u64 = ctx.r11.u64;
  return;
loc_821CE0C8:
  ea = ctx.r3.u32;
  REF_STWCX(mem, ea, ctx.r11.u32);
  ctx.msr = (ctx.r10.u32 & 0x8020) | (ctx.msr & ~0x8020);
  ctx.r3.u64 = ctx.r11.u64;
}

void RefAdd(Context &ctx, Memory &mem, int delta) {  // sub_821CE0D8 / sub_821CE100
  uint32_t ea{};
loop:
  ctx.r10.u64 = kCheckGlobalLock;
  ctx.msr = (ctx.r13.u32 & 0x8020) | (ctx.msr & ~0x8020);
  ea = ctx.r3.u32;
  ctx.reserved.u32 = *mem.Word(ea);
  ctx.r11.u64 = __builtin_bswap32(ctx.reserved.u32);
  ctx.r11.s64 = ctx.r11.s64 + delta;
  ea = ctx.r3.u32;
  REF_STWCX(mem, ea, ctx.r11.u32);
  ctx.msr = (ctx.r10.u32 & 0x8020) | (ctx.msr & ~0x8020);
  if (!ctx.cr0.eq) goto loop;
  ctx.r3.u64 = ctx.r11.u64;
}

void RefExchangeAdd(Context &ctx, Memory &mem) {  // sub_821CE128
  uint32_t ea{};
loc_821CE128:
  ctx.r9.u64 = kCheckGlobalLock;
  ctx.msr = (ctx.r13.u32 & 0x8020) | (ctx.msr & ~0x8020);
  ea = ctx.r3.u32;
  ctx.reserved.u32 = *mem.Word(ea);
  ctx.r11.u64 = __builtin_bswap32(ctx.reserved.u32);
  ctx.r10.u64 = ctx.r4.u64 + ctx.r11.u64;
  ea = ctx.r3.u32;
  REF_STWCX(mem, ea, ctx.r10.u32);
  ctx.msr = (ctx.r9.u32 & 0x8020) | (ctx.msr & ~0x8020);
  if (!ctx.cr0.eq) goto loc_821CE128;
  ctx.r3.u64 = ctx.r11.u64;
}

// ---- Native under test -----------------------------------------------------

namespace native = mcla::native_refcount;

void NativeAddRef(Context &ctx, Memory &mem) {
  if (!native::AddRef(ctx, mem, [&mem](Context &c) { NativeAddRef(c, mem); })) RefAddRef(ctx, mem);
}

void NativeRelease(Context &ctx, Memory &mem, DestroyLog &log) {
  native::ReleaseEntry entry;
  switch (native::ReleaseFast(ctx, mem, entry)) {
    case native::ReleaseStep::kDone:
      return;
    case native::ReleaseStep::kDestroy:
      native::ReleaseDestroy(
          ctx, mem, entry, [&](Context &c) { NativeRelease(c, mem, log); },
          [&](Context &c) { FakeDestroy(c, mem, log); });
      return;
    case native::ReleaseStep::kNotHandled:
      RefRelease(ctx, mem, log);
      return;
  }
}

// ---- Fixtures ---------------------------------------------------------------

struct Rng {
  uint64_t state;
  uint64_t Next() {
    state = state * 6364136223846793005ull + 1442695040888963407ull;
    return state ^ (state >> 29);
  }
};

void RandomContext(Context &ctx, Rng &rng) {
  for (auto &r : ctx.gpr) r.u64 = rng.Next();
  ctx.r1.u64 = kStackTop;
  ctx.r13.u64 = rng.Next() & ~0x8000ull;  // the kernel PCR pointer: EE clear
  ctx.lr = 0x82000000ull | (rng.Next() & 0xFFFFFCull);
  if (rng.Next() & 1) ctx.lr |= 0xABCD00000000ull;  // a 64-bit LR is reloaded as 32 bits
  ctx.ctr.u64 = rng.Next();
  ctx.xer = {static_cast<uint8_t>(rng.Next() & 1), 0, static_cast<uint8_t>(rng.Next() & 1)};
  ctx.reserved.u64 = rng.Next();
  ctx.msr = static_cast<uint32_t>(rng.Next());
  for (CR *cr : {&ctx.cr0, &ctx.cr1, &ctx.cr2, &ctx.cr3, &ctx.cr4, &ctx.cr5, &ctx.cr6, &ctx.cr7}) {
    const auto bits = rng.Next();
    *cr = {static_cast<uint8_t>(bits & 1), static_cast<uint8_t>((bits >> 1) & 1),
           static_cast<uint8_t>((bits >> 2) & 1), static_cast<uint8_t>((bits >> 3) & 1)};
  }
}

// Object layout: word 0 = type (low 4 bits) and flag 0x40000000, word 1 =
// count, word 6 = parent pointer.
constexpr uint32_t kObject = 0x2000, kParent = 0x2100, kGrandParent = 0x2200;

uint32_t InterestingCount(Rng &rng) {
  static constexpr std::array<uint32_t, 8> values{0, 1, 2, 5, 0x7FFFFFFF, 0x80000000, 0xFFFFFFFF, 0xFFFFFFFE};
  return (rng.Next() & 3) ? values[rng.Next() % values.size()] : static_cast<uint32_t>(rng.Next());
}

uint32_t InterestingHeader(Rng &rng) {
  const uint32_t type = (rng.Next() & 1) ? 4 : static_cast<uint32_t>(rng.Next() & 0xF);
  const uint32_t flag = (rng.Next() & 3) ? 0x40000000u : 0;
  return (static_cast<uint32_t>(rng.Next()) & ~0x4000000Fu) | type | flag;
}

void FillObjects(Memory &mem, Rng &rng, bool release) {
  for (const uint32_t object : {kObject, kParent, kGrandParent}) {
    mem.Store32(object + 0, InterestingHeader(rng));
    // Release chains need 1 to reach zero; AddRef chains need 0 to reach 1.
    const uint32_t chain = release ? 1 : 0;
    mem.Store32(object + 4, (rng.Next() & 1) ? chain : InterestingCount(rng));
  }
  mem.Store32(kObject + 24, kParent);
  mem.Store32(kParent + 24, kGrandParent);
  mem.Store32(kGrandParent + 24, kObject + 0x40);  // a leaf object without parent rule
  mem.Store32(kObject + 0x40, 0);
  mem.Store32(kObject + 0x44, 7);
}

template <class RunRef, class RunNative>
void Compare(const char *label, const Context &start, const Memory &memory, RunRef run_ref,
             RunNative run_native) {
  Context ref_ctx = start, native_ctx = start;
  auto ref_mem = std::make_unique<Memory>(memory);
  auto native_mem = std::make_unique<Memory>(memory);
  run_ref(ref_ctx, *ref_mem);
  run_native(native_ctx, *native_mem);
  const bool same_ctx = SameContext(ref_ctx, native_ctx);
  const bool same_mem = SameMemory(*ref_mem, *native_mem, static_cast<uint32_t>(start.r1.u64));
  Check(same_ctx && same_mem, label);
}

void TestDirectedAddRef() {
  Rng rng{1};
  auto mem = std::make_unique<Memory>();
  Context ctx;
  RandomContext(ctx, rng);
  struct Case {
    uint32_t count, header, parent_count, parent_header;
    const char *label;
  };
  const Case cases[] = {
      {5, 4 | 0x40000000, 0, 4, "addref plain count"},
      {0, 4 | 0x40000000, 0, 4 | 0x40000000, "addref 0->1 with parent chain"},
      {0, 4 | 0x40000000, 3, 4 | 0x40000000, "addref 0->1 parent stays above 1"},
      {0, 3 | 0x40000000, 0, 4, "addref 0->1 type not 4"},
      {0, 4, 0, 4, "addref 0->1 flag clear"},
      {0xFFFFFFFF, 4 | 0x40000000, 0, 4, "addref wraps to 0 with r3 = 0x100000000"},
      {0x7FFFFFFF, 4, 0, 4, "addref to 0x80000000"},
  };
  for (const auto &c : cases) {
    mem->Store32(kObject + 0, c.header);
    mem->Store32(kObject + 4, c.count);
    mem->Store32(kObject + 24, kParent);
    mem->Store32(kParent + 0, c.parent_header);
    mem->Store32(kParent + 4, c.parent_count);
    mem->Store32(kParent + 24, kGrandParent);
    mem->Store32(kGrandParent + 0, 0);
    mem->Store32(kGrandParent + 4, 9);
    ctx.r3.u64 = kObject;
    Compare(c.label, ctx, *mem, RefAddRef, NativeAddRef);
  }
  // The returned value and the stored count, checked directly.
  mem->Store32(kObject + 0, 4 | 0x40000000);
  mem->Store32(kObject + 4, 0);
  mem->Store32(kParent + 0, 4);
  mem->Store32(kParent + 4, 10);
  Context direct = ctx;
  direct.r3.u64 = kObject;
  const uint64_t r31 = direct.r31.u64, r1 = direct.r1.u64;
  NativeAddRef(direct, *mem);
  Check(direct.r3.u64 == 1 && mem->Load32(kObject + 4) == 1 && mem->Load32(kParent + 4) == 11 &&
            direct.r31.u64 == r31 && direct.r1.u64 == r1 && direct.lr == static_cast<uint32_t>(ctx.lr),
        "addref returns this object's count, increments the parent, keeps r31/r1");
  direct.r3.u64 = kObject;
  mem->Store32(kObject + 4, 0xFFFFFFFF);
  NativeAddRef(direct, *mem);
  Check(direct.r3.u64 == 0x100000000ull && mem->Load32(kObject + 4) == 0,
        "addref of 0xFFFFFFFF returns the 64-bit sum like addi on the zero-extended word");
}

void TestDirectedRelease() {
  Rng rng{2};
  auto mem = std::make_unique<Memory>();
  Context ctx;
  RandomContext(ctx, rng);
  struct Case {
    uint32_t count, header, parent_count, parent_header;
    const char *label;
  };
  const Case cases[] = {
      {5, 4 | 0x40000000, 1, 4 | 0x40000000, "release plain count"},
      {1, 4 | 0x40000000, 1, 4 | 0x40000000, "release 1->0 parent also destroyed"},
      {1, 4 | 0x40000000, 3, 4 | 0x40000000, "release 1->0 parent survives"},
      {1, 2 | 0x40000000, 1, 4, "release 1->0 type not 4, no parent"},
      {1, 4, 1, 4, "release 1->0 flag clear, no parent"},
      {0, 4 | 0x40000000, 1, 4, "release of 0 returns 0xFFFFFFFFFFFFFFFF"},
      {0x80000000, 4, 1, 4, "release to 0x7FFFFFFF"},
  };
  for (const auto &c : cases) {
    mem->Store32(kObject + 0, c.header);
    mem->Store32(kObject + 4, c.count);
    mem->Store32(kObject + 24, kParent);
    mem->Store32(kParent + 0, c.parent_header);
    mem->Store32(kParent + 4, c.parent_count);
    mem->Store32(kParent + 24, kGrandParent);
    mem->Store32(kGrandParent + 0, 0);
    mem->Store32(kGrandParent + 4, 9);
    ctx.r3.u64 = kObject;
    DestroyLog ref_log, native_log;
    Compare(
        c.label, ctx, *mem, [&](Context &x, Memory &m) { RefRelease(x, m, ref_log); },
        [&](Context &x, Memory &m) { NativeRelease(x, m, native_log); });
    Check(ref_log.objects == native_log.objects && ref_log.lrs == native_log.lrs,
          "release destroys the same objects in the same order with the same return address");
  }
  // Destroy order: the parent is released (and destroyed) before this object.
  mem->Store32(kObject + 0, 4 | 0x40000000);
  mem->Store32(kObject + 4, 1);
  mem->Store32(kParent + 0, 4);
  mem->Store32(kParent + 4, 1);
  DestroyLog log;
  Context direct = ctx;
  direct.r3.u64 = kObject;
  const uint64_t r31 = direct.r31.u64, r1 = direct.r1.u64;
  NativeRelease(direct, *mem, log);
  Check(log.objects.size() == 2 && log.objects[0] == kParent && log.objects[1] == kObject &&
            log.lrs[0] == native::kReleaseDestroyReturn && direct.r3.u64 == 0 &&
            direct.r31.u64 == r31 && direct.r1.u64 == r1,
        "release destroys parent first, then this object, returns 0, keeps r31/r1");
}

void TestDirectedInterlocked() {
  Rng rng{3};
  auto mem = std::make_unique<Memory>();
  Context ctx;
  RandomContext(ctx, rng);
  constexpr uint32_t kWord = 0x3000;
  for (const uint32_t value : {0u, 1u, 0x7FFFFFFFu, 0x80000000u, 0xFFFFFFFFu, 0x12345678u}) {
    mem->Store32(kWord, value);
    ctx.r3.u64 = kWord;
    ctx.r4.u64 = 0xFFFFFFFF00000000ull | 0xCAFEBABEu;
    Compare("exchange", ctx, *mem, RefExchange,
            [](Context &c, Memory &m) { Check(native::InterlockedExchange(c, m), "exchange handled"); });
    for (const uint64_t comparand : {static_cast<uint64_t>(value), static_cast<uint64_t>(value) ^ 1u,
                                     0xFFFFFFFF00000000ull | value}) {
      ctx.r5.u64 = comparand;
      Compare("compare-exchange", ctx, *mem, RefCompareExchange, [](Context &c, Memory &m) {
        Check(native::InterlockedCompareExchange(c, m), "compare-exchange handled");
      });
    }
    Compare("increment", ctx, *mem, [](Context &c, Memory &m) { RefAdd(c, m, 1); },
            [](Context &c, Memory &m) { Check(native::InterlockedAdd(c, m, 1), "increment handled"); });
    Compare("decrement", ctx, *mem, [](Context &c, Memory &m) { RefAdd(c, m, -1); },
            [](Context &c, Memory &m) { Check(native::InterlockedAdd(c, m, -1), "decrement handled"); });
    for (const uint64_t addend : {1ull, 0xFFFFFFFFull, 0xFFFFFFFFFFFFFFFFull, 0x100000005ull}) {
      ctx.r4.u64 = addend;
      Compare("exchange-add", ctx, *mem, RefExchangeAdd, [](Context &c, Memory &m) {
        Check(native::InterlockedExchangeAdd(c, m), "exchange-add handled");
      });
    }
  }
  // Results checked directly.
  mem->Store32(kWord, 0);
  ctx.r3.u64 = kWord;
  native::InterlockedAdd(ctx, *mem, -1);
  Check(ctx.r3.u64 == 0xFFFFFFFFFFFFFFFFull && mem->Load32(kWord) == 0xFFFFFFFFu,
        "decrement of 0 returns the sign-extended -1 and stores 0xFFFFFFFF");
  ctx.r3.u64 = kWord;
  ctx.r4.u64 = 7;
  ctx.r5.u64 = 0xFFFFFFFFFFFFFFFFull;  // cmpw compares the low words: equal
  native::InterlockedCompareExchange(ctx, *mem);
  Check(ctx.r3.u64 == 0xFFFFFFFFu && mem->Load32(kWord) == 7 && ctx.cr6.eq,
        "compare-exchange compares the low 32 bits and returns the old value");
  ctx.r3.u64 = kWord;
  ctx.r4.u64 = 9;
  ctx.r5.u64 = 8;
  native::InterlockedCompareExchange(ctx, *mem);
  Check(ctx.r3.u64 == 7 && mem->Load32(kWord) == 7 && !ctx.cr6.eq && ctx.cr6.lt,
        "compare-exchange mismatch leaves the word");
}

void TestRandomized() {
  Rng rng{0x4D434C41};
  auto mem = std::make_unique<Memory>();
  for (int i = 0; i < 4000; ++i) {
    Context ctx;
    RandomContext(ctx, rng);
    const int which = static_cast<int>(rng.Next() % 7);
    FillObjects(*mem, rng, which == 1);
    ctx.r3.u64 = kObject;
    if (rng.Next() & 1) ctx.r3.u64 |= 0x0000000100000000ull;  // upper bits are ignored by the address
    DestroyLog ref_log, native_log;
    switch (which) {
      case 0:
        Compare("random addref", ctx, *mem, RefAddRef, NativeAddRef);
        break;
      case 1:
        Compare(
            "random release", ctx, *mem, [&](Context &x, Memory &m) { RefRelease(x, m, ref_log); },
            [&](Context &x, Memory &m) { NativeRelease(x, m, native_log); });
        Check(ref_log.objects == native_log.objects && ref_log.lrs == native_log.lrs,
              "random release destroy sequence");
        break;
      case 2:
        Compare("random exchange", ctx, *mem, RefExchange,
                [](Context &c, Memory &m) { native::InterlockedExchange(c, m); });
        break;
      case 3:
        if (rng.Next() & 1) ctx.r5.u64 = mem->Load32(kObject);
        Compare("random compare-exchange", ctx, *mem, RefCompareExchange,
                [](Context &c, Memory &m) { native::InterlockedCompareExchange(c, m); });
        break;
      case 4:
        Compare("random increment", ctx, *mem, [](Context &c, Memory &m) { RefAdd(c, m, 1); },
                [](Context &c, Memory &m) { native::InterlockedAdd(c, m, 1); });
        break;
      case 5:
        Compare("random decrement", ctx, *mem, [](Context &c, Memory &m) { RefAdd(c, m, -1); },
                [](Context &c, Memory &m) { native::InterlockedAdd(c, m, -1); });
        break;
      default:
        Compare("random exchange-add", ctx, *mem, RefExchangeAdd,
                [](Context &c, Memory &m) { native::InterlockedExchangeAdd(c, m); });
        break;
    }
  }
}

void TestMisalignedNotHandled() {
  Rng rng{4};
  auto mem = std::make_unique<Memory>();
  Context ctx;
  RandomContext(ctx, rng);
  const auto before_mem = std::make_unique<Memory>(*mem);
  for (const uint32_t offset : {1u, 2u, 3u}) {
    ctx.r3.u64 = 0x3000 + offset;
    Context copy = ctx;
    native::ReleaseEntry entry;
    Check(!native::AddRef(copy, *mem, [](Context &) {}) && SameContext(copy, ctx),
          "misaligned addref is left to the generated body");
    Check(native::ReleaseFast(copy, *mem, entry) == native::ReleaseStep::kNotHandled &&
              SameContext(copy, ctx),
          "misaligned release is left to the generated body");
    Check(!native::InterlockedExchange(copy, *mem) && !native::InterlockedCompareExchange(copy, *mem) &&
              !native::InterlockedAdd(copy, *mem, 1) && !native::InterlockedExchangeAdd(copy, *mem) &&
              SameContext(copy, ctx),
          "misaligned interlocked words are left to the generated bodies");
  }
  Check(std::memcmp(before_mem->bytes.data(), mem->bytes.data(), kMemorySize) == 0,
        "misaligned calls do not touch memory");
}

void TestConcurrent() {
  // Many threads hammer one counter with native AddRef/Release (no zero
  // crossing) and the interlocked helpers; no update may be lost.
  auto mem = std::make_unique<Memory>();
  constexpr uint32_t kCounter = 0x4000, kWord = 0x4100;
  mem->Store32(kCounter + 0, 0);  // type 0: no parent rule
  mem->Store32(kCounter + 4, 1000);
  mem->Store32(kWord, 0);
  constexpr int kThreads = 8, kIterations = 20000;
  std::vector<std::thread> threads;
  for (int t = 0; t < kThreads; ++t) {
    threads.emplace_back([&mem, t] {
      Context ctx;
      ctx.r1.u64 = kStackTop - 0x1000 * static_cast<uint32_t>(t);
      DestroyLog log;
      for (int i = 0; i < kIterations; ++i) {
        ctx.r3.u64 = kCounter;
        NativeAddRef(ctx, *mem);
        ctx.r3.u64 = kCounter;
        NativeAddRef(ctx, *mem);
        ctx.r3.u64 = kCounter;
        NativeRelease(ctx, *mem, log);
        ctx.r3.u64 = kWord;
        native::InterlockedAdd(ctx, *mem, 1);
        ctx.r3.u64 = kWord;
        ctx.r4.u64 = 2;
        native::InterlockedExchangeAdd(ctx, *mem);
        ctx.r3.u64 = kWord;
        native::InterlockedAdd(ctx, *mem, -1);
      }
    });
  }
  for (auto &thread : threads) thread.join();
  Check(mem->Load32(kCounter + 4) == 1000u + kThreads * kIterations, "concurrent addref/release lose no update");
  Check(mem->Load32(kWord) == 2u * kThreads * kIterations, "concurrent interlocked helpers lose no update");
}

}  // namespace

int main() {
  TestDirectedAddRef();
  TestDirectedRelease();
  TestDirectedInterlocked();
  TestRandomized();
  TestMisalignedNotHandled();
  TestConcurrent();
  if (failures) {
    std::fprintf(stderr, "native refcount: %d of %d checks failed\n", failures, checks);
    return 1;
  }
  std::printf("PASS: %d native refcount checks\n", checks);
  return 0;
}
