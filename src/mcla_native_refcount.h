// Native replacements for small guest atomic helpers (M10 CPU task 1,
// docs/evidence/M10-host-cpu-and-ki-fixes-20261101.md).
//
// The generated code wraps every guest "mfmsr; mtmsrd r13,1; lwarx ... stwcx.;
// mtmsrd" interrupt-disable region in two lock/unlock pairs of the global
// recursive mutex and three seq_cst fences. These helpers do the same update
// with one host atomic on the big-endian guest word (lock cmpxchg or xchg, a
// full barrier on x86) and leave the guest registers exactly as the generated
// body does on return:
//
//   sub_82422160  XDK D3D resource AddRef (+1 at this+4, parent rule)
//   sub_824221D8  XDK D3D resource Release (-1 at this+4, parent rule, then the
//                 guest destroy sub_82421AA8 when the count reaches 0)
//   sub_821CE080  XAPI InterlockedExchange
//   sub_821CE0A0  XAPI InterlockedCompareExchange
//   sub_821CE0D8  XAPI InterlockedIncrement
//   sub_821CE100  XAPI InterlockedDecrement
//   sub_821CE128  XAPI InterlockedExchangeAdd
//
// Header-only and templated on the context and guest-memory types so the unit
// test runs the same code on a fake context and buffer. Mem must provide
// Word(ea) -> uint32_t* (host pointer of the big-endian word), Load32(ea),
// Load64(ea), Store32(ea, v) and Store64(ea, v) with the generated code's
// REX_LOAD/REX_STORE byte order.
//
// Not mirrored (not observable by correct guest code): the original's stores
// below the stack pointer in the fast paths (saved LR, saved r31 and the back
// chain of a frame that is popped again before returning), and the global
// lock itself. The interrupt mask the lock emulates is per hardware thread on
// the real console, so no other thread relied on it.

#pragma once

#include <atomic>
#include <cstdint>

namespace mcla::native_refcount {

// Value mfmsr returns in these leaf regions: REX_CHECK_GLOBAL_LOCK() is
// 0x8000 whenever the calling thread is not itself inside a locked region,
// which a leaf helper never is.
inline constexpr uint64_t kMsrInterruptsEnabled = 0x8000;
inline constexpr uint32_t kMsrMask = 0x8020;

inline constexpr uint32_t kAddRefAddress = 0x82422160;
inline constexpr uint32_t kReleaseAddress = 0x824221D8;
inline constexpr uint32_t kAddRefParentReturn = 0x824221BC;
inline constexpr uint32_t kReleaseParentReturn = 0x82422238;
inline constexpr uint32_t kReleaseDestroyReturn = 0x82422240;

inline constexpr uint32_t ByteSwap32(uint32_t value) { return __builtin_bswap32(value); }

// Atomically replaces the big-endian word with update(old) (host order) and
// returns the old host-order value; raw_old receives the old raw word.
template <class Update>
inline uint32_t UpdateBe32(uint32_t *word, Update update, uint32_t &raw_old) {
  std::atomic_ref<uint32_t> ref(*word);
  uint32_t raw = ref.load(std::memory_order_relaxed);
  while (!ref.compare_exchange_weak(raw, ByteSwap32(update(ByteSwap32(raw))),
                                    std::memory_order_seq_cst, std::memory_order_relaxed)) {
  }
  raw_old = raw;
  return ByteSwap32(raw);
}

inline bool Aligned32(uint32_t ea) { return (ea & 3u) == 0; }

// Register effects shared by every region: mfmsr into msr_reg, the mtmsrd
// pair on ctx.msr, the reservation, and a successful stwcx. into CR0.
template <class Ctx>
inline void ReservationEffects(Ctx &ctx, uint32_t raw_old) {
  ctx.msr = static_cast<uint32_t>((kMsrInterruptsEnabled & kMsrMask) | (ctx.msr & ~kMsrMask));
  ctx.reserved.u32 = raw_old;
  ctx.cr0.lt = 0;
  ctx.cr0.gt = 0;
  ctx.cr0.eq = 1;
  ctx.cr0.so = ctx.xer.so;
}

// The original prologue/epilogue around a call (mflr r12; stw r12,-8(r1);
// std r31,-16(r1); stwu r1,-96(r1) ... addi r1,r1,96; lwz r12,-8(r1); mtlr
// r12; ld r31,-16(r1)). Used only on the paths that call further guest code.
template <class Ctx, class Mem>
inline void PushFrame(Ctx &ctx, Mem &mem) {
  ctx.r12.u64 = ctx.lr;
  mem.Store32(ctx.r1.u32 + -8, ctx.r12.u32);
  mem.Store64(ctx.r1.u32 + -16, ctx.r31.u64);
  const uint32_t ea = -96 + ctx.r1.u32;
  mem.Store32(ea, ctx.r1.u32);
  ctx.r1.u32 = ea;
}

template <class Ctx, class Mem>
inline void PopFrame(Ctx &ctx, Mem &mem) {
  ctx.r1.s64 = ctx.r1.s64 + 96;
  ctx.r12.u64 = mem.Load32(ctx.r1.u32 + -8);
  ctx.lr = ctx.r12.u64;
  ctx.r31.u64 = mem.Load64(ctx.r1.u32 + -16);
}

// Fast-path return without a frame: lr and r12 end as the 32-bit reload of
// the saved LR.
template <class Ctx>
inline void LeafReturn(Ctx &ctx) {
  ctx.r12.u64 = static_cast<uint32_t>(ctx.lr);
  ctx.lr = ctx.r12.u64;
}

// sub_82422160. Returns false (nothing changed) when the counter word is not
// 4-byte aligned; the caller then runs the generated body. add_ref_parent(ctx)
// performs the guest call "bl 0x82422160" for the parent.
template <class Ctx, class Mem, class Call>
inline bool AddRef(Ctx &ctx, Mem &mem, Call &&add_ref_parent) {
  const uint32_t counter = ctx.r3.u32 + 4;
  if (!Aligned32(counter)) return false;
  // addi r11,r3,4; loop: mfmsr r9; mtmsrd r13,1; lwarx r10,0,r11;
  // addi r10,r10,1; stwcx. r10,0,r11; mtmsrd r9,1; bne loop
  ctx.r11.s64 = ctx.r3.s64 + 4;
  uint32_t raw_old = 0;
  const uint32_t old = UpdateBe32(mem.Word(counter), [](uint32_t v) { return v + 1; }, raw_old);
  ctx.r9.u64 = kMsrInterruptsEnabled;
  ReservationEffects(ctx, raw_old);
  ctx.r10.u64 = old;
  ctx.r10.s64 = ctx.r10.s64 + 1;
  // mr r31,r10; cmplwi cr6,r31,1; bne cr6,done
  const uint64_t count = ctx.r10.u64;
  ctx.cr6.template compare<uint32_t>(static_cast<uint32_t>(count), 1, ctx.xer);
  if (static_cast<uint32_t>(count) == 1) {
    // lwz r11,0(r3); clrlwi r10,r11,28; cmplwi cr6,r10,4; bne cr6,done
    ctx.r11.u64 = mem.Load32(ctx.r3.u32 + 0);
    ctx.r10.u64 = ctx.r11.u32 & 0xF;
    ctx.cr6.template compare<uint32_t>(ctx.r10.u32, 4, ctx.xer);
    if (ctx.r10.u32 == 4) {
      // rlwinm. r11,r11,0,1,1; beq done
      ctx.r11.u64 = ctx.r11.u32 & 0x40000000u;
      ctx.cr0.template compare<int32_t>(ctx.r11.s32, 0, ctx.xer);
      if (ctx.r11.u32 != 0) {
        // lwz r3,24(r3); bl 0x82422160, inside the original frame with r31
        // holding this object's new count.
        PushFrame(ctx, mem);
        ctx.r31.u64 = count;
        ctx.r3.u64 = mem.Load32(ctx.r3.u32 + 24);
        ctx.lr = kAddRefParentReturn;
        add_ref_parent(ctx);
        ctx.r3.u64 = ctx.r31.u64;
        PopFrame(ctx, mem);
        return true;
      }
    }
  }
  // mr r3,r31
  ctx.r3.u64 = count;
  LeafReturn(ctx);
  return true;
}

// sub_824221D8 without the zero path: returns kDone when the count stayed
// non-zero (registers final), kDestroy when it reached zero (the caller then
// runs ReleaseDestroy, under the original function's guest scope), and
// kNotHandled (nothing changed) for a misaligned counter word.
enum class ReleaseStep { kNotHandled, kDone, kDestroy };

struct ReleaseEntry {
  uint64_t self = 0;  // entry r3
};

template <class Ctx, class Mem>
inline ReleaseStep ReleaseFast(Ctx &ctx, Mem &mem, ReleaseEntry &entry) {
  const uint32_t counter = ctx.r3.u32 + 4;
  if (!Aligned32(counter)) return ReleaseStep::kNotHandled;
  entry.self = ctx.r3.u64;
  // mr r31,r3; addi r11,r31,4; loop: ... lwarx r10; addi r10,r10,-1; stwcx.
  ctx.r11.s64 = ctx.r3.s64 + 4;
  uint32_t raw_old = 0;
  const uint32_t old = UpdateBe32(mem.Word(counter), [](uint32_t v) { return v - 1; }, raw_old);
  ctx.r9.u64 = kMsrInterruptsEnabled;
  ReservationEffects(ctx, raw_old);
  ctx.r10.u64 = old;
  ctx.r10.s64 = ctx.r10.s64 + -1;
  // mr r3,r10; cmplwi cr6,r3,0; bne cr6,done
  ctx.r3.u64 = ctx.r10.u64;
  ctx.cr6.template compare<uint32_t>(ctx.r3.u32, 0, ctx.xer);
  if (ctx.r3.u32 != 0) {
    LeafReturn(ctx);
    return ReleaseStep::kDone;
  }
  return ReleaseStep::kDestroy;
}

// The zero path of sub_824221D8, exactly as generated, inside the original
// frame: parent rule with "bl 0x824221D8", then "mr r3,r31; bl 0x82421aa8;
// li r3,0". release_parent and destroy perform those two guest calls.
template <class Ctx, class Mem, class CallParent, class CallDestroy>
inline void ReleaseDestroy(Ctx &ctx, Mem &mem, const ReleaseEntry &entry,
                           CallParent &&release_parent, CallDestroy &&destroy) {
  PushFrame(ctx, mem);
  ctx.r31.u64 = entry.self;
  // lwz r11,0(r31); clrlwi r10,r11,28; cmplwi cr6,r10,4; bne cr6,destroy
  ctx.r11.u64 = mem.Load32(ctx.r31.u32 + 0);
  ctx.r10.u64 = ctx.r11.u32 & 0xF;
  ctx.cr6.template compare<uint32_t>(ctx.r10.u32, 4, ctx.xer);
  if (ctx.r10.u32 == 4) {
    // rlwinm. r11,r11,0,1,1; beq destroy; lwz r3,24(r31); bl 0x824221d8
    ctx.r11.u64 = ctx.r11.u32 & 0x40000000u;
    ctx.cr0.template compare<int32_t>(ctx.r11.s32, 0, ctx.xer);
    if (ctx.r11.u32 != 0) {
      ctx.r3.u64 = mem.Load32(ctx.r31.u32 + 24);
      ctx.lr = kReleaseParentReturn;
      release_parent(ctx);
    }
  }
  ctx.r3.u64 = ctx.r31.u64;
  ctx.lr = kReleaseDestroyReturn;
  destroy(ctx);
  ctx.r3.s64 = 0;
  PopFrame(ctx, mem);
}

// XAPI interlocked helpers (leaf functions without a frame; LR untouched).
// Each returns false (nothing changed) for a misaligned word.

// sub_821CE080: mfmsr r10; lwarx r11,0,r3; stwcx. r4,0,r3; mr r3,r11
template <class Ctx, class Mem>
inline bool InterlockedExchange(Ctx &ctx, Mem &mem) {
  const uint32_t ea = ctx.r3.u32;
  if (!Aligned32(ea)) return false;
  std::atomic_ref<uint32_t> ref(*mem.Word(ea));
  const uint32_t raw_old = ref.exchange(ByteSwap32(ctx.r4.u32), std::memory_order_seq_cst);
  ctx.r10.u64 = kMsrInterruptsEnabled;
  ReservationEffects(ctx, raw_old);
  ctx.r11.u64 = ByteSwap32(raw_old);
  ctx.r3.u64 = ctx.r11.u64;
  return true;
}

// sub_821CE0A0: lwarx r11,0,r3; cmpw cr6,r11,r5; bne cr6,keep;
// stwcx. r4,0,r3 (retry on failure) / keep: stwcx. r11,0,r3; mr r3,r11
template <class Ctx, class Mem>
inline bool InterlockedCompareExchange(Ctx &ctx, Mem &mem) {
  const uint32_t ea = ctx.r3.u32;
  if (!Aligned32(ea)) return false;
  std::atomic_ref<uint32_t> ref(*mem.Word(ea));
  uint32_t raw = ByteSwap32(ctx.r5.u32);
  // lock cmpxchg writes in both outcomes, as the original's stwcx. does.
  ref.compare_exchange_strong(raw, ByteSwap32(ctx.r4.u32), std::memory_order_seq_cst,
                              std::memory_order_seq_cst);
  ctx.r10.u64 = kMsrInterruptsEnabled;
  ReservationEffects(ctx, raw);
  ctx.r11.u64 = ByteSwap32(raw);
  ctx.cr6.template compare<int32_t>(ctx.r11.s32, ctx.r5.s32, ctx.xer);
  ctx.r3.u64 = ctx.r11.u64;
  return true;
}

// sub_821CE0D8 (+1) and sub_821CE100 (-1): lwarx r11,0,r3; addi r11,r11,d;
// stwcx. r11,0,r3; mr r3,r11 (the 64-bit sum of the zero-extended old value)
template <class Ctx, class Mem>
inline bool InterlockedAdd(Ctx &ctx, Mem &mem, int32_t delta) {
  const uint32_t ea = ctx.r3.u32;
  if (!Aligned32(ea)) return false;
  uint32_t raw_old = 0;
  const uint32_t old = UpdateBe32(
      mem.Word(ea), [delta](uint32_t v) { return v + static_cast<uint32_t>(delta); }, raw_old);
  ctx.r10.u64 = kMsrInterruptsEnabled;
  ReservationEffects(ctx, raw_old);
  ctx.r11.u64 = old;
  ctx.r11.s64 = ctx.r11.s64 + delta;
  ctx.r3.u64 = ctx.r11.u64;
  return true;
}

// sub_821CE128: mfmsr r9; lwarx r11,0,r3; add r10,r4,r11; stwcx. r10,0,r3;
// mr r3,r11
template <class Ctx, class Mem>
inline bool InterlockedExchangeAdd(Ctx &ctx, Mem &mem) {
  const uint32_t ea = ctx.r3.u32;
  if (!Aligned32(ea)) return false;
  const uint32_t addend = ctx.r4.u32;
  uint32_t raw_old = 0;
  const uint32_t old =
      UpdateBe32(mem.Word(ea), [addend](uint32_t v) { return v + addend; }, raw_old);
  ctx.r9.u64 = kMsrInterruptsEnabled;
  ReservationEffects(ctx, raw_old);
  ctx.r11.u64 = old;
  ctx.r10.u64 = ctx.r4.u64 + ctx.r11.u64;
  ctx.r3.u64 = ctx.r11.u64;
  return true;
}

}  // namespace mcla::native_refcount
