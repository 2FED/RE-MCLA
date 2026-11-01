// Native XDK D3D resource AddRef/Release and XAPI interlocked helpers
// (M10 CPU task 1). Strong overrides of the generated weak bodies; the guest
// semantics live in mcla_native_refcount.h (see the equivalence note in
// docs/evidence/M10-host-cpu-and-ki-fixes-20261101.md).
//
// --mcla_native_d3d_refcount=false keeps the generated AddRef/Release and
// --mcla_native_xapi_interlocked=false the generated XAPI helpers, for A/B
// comparison. Both are read once (init-only).

#include "mcla_native_refcount.h"

#include "mcla_init.h"

#include <rex/cvar.h>

REXCVAR_DEFINE_BOOL(mcla_native_d3d_refcount, true, "MCLA",
    "Native D3D resource AddRef/Release (sub_82422160/sub_824221D8) without the "
    "emulated interrupt lock")
    .lifecycle(rex::cvar::Lifecycle::kInitOnly);

REXCVAR_DEFINE_BOOL(mcla_native_xapi_interlocked, true, "MCLA",
    "Native XAPI Interlocked* helpers (sub_821CE080-sub_821CE128) without the "
    "emulated interrupt lock")
    .lifecycle(rex::cvar::Lifecycle::kInitOnly);

namespace {

namespace native = mcla::native_refcount;

bool NativeRefcountWanted() {
  static const bool wanted = REXCVAR_GET(mcla_native_d3d_refcount);
  return wanted;
}

bool NativeInterlockedWanted() {
  static const bool wanted = REXCVAR_GET(mcla_native_xapi_interlocked);
  return wanted;
}

// Guest memory with the generated code's address translation and byte order.
struct GuestMemory {
  uint8_t *base;
  uint32_t *Word(uint32_t ea) const { return reinterpret_cast<uint32_t *>(REX_RAW_ADDR(ea)); }
  uint32_t Load32(uint32_t ea) const { return REX_LOAD_U32(ea); }
  uint64_t Load64(uint32_t ea) const { return REX_LOAD_U64(ea); }
  void Store32(uint32_t ea, uint32_t value) const { REX_STORE_U32(ea, value); }
  void Store64(uint32_t ea, uint64_t value) const { REX_STORE_U64(ea, value); }
};

}  // namespace

REX_EXTERN(sub_82422160) {
  if (NativeRefcountWanted()) {
    GuestMemory memory{base};
    if (native::AddRef(ctx, memory, [base](PPCContext &c) { sub_82422160(c, base); })) {
      return;
    }
  }
  __imp__sub_82422160(ctx, base);
}

REX_EXTERN(sub_824221D8) {
  if (NativeRefcountWanted()) {
    GuestMemory memory{base};
    native::ReleaseEntry entry;
    switch (native::ReleaseFast(ctx, memory, entry)) {
      case native::ReleaseStep::kDone:
        return;
      case native::ReleaseStep::kDestroy: {
        // The destroy path is guest code; keep the original's crash breadcrumb.
        rex::ppc::GuestFunctionScope scope(ctx, native::kReleaseAddress);
        native::ReleaseDestroy(
            ctx, memory, entry, [base](PPCContext &c) { sub_824221D8(c, base); },
            [base](PPCContext &c) { sub_82421AA8(c, base); });
        return;
      }
      case native::ReleaseStep::kNotHandled:
        break;
    }
  }
  __imp__sub_824221D8(ctx, base);
}

REX_EXTERN(sub_821CE080) {
  GuestMemory memory{base};
  if (!NativeInterlockedWanted() || !native::InterlockedExchange(ctx, memory)) {
    __imp__sub_821CE080(ctx, base);
  }
}

REX_EXTERN(sub_821CE0A0) {
  GuestMemory memory{base};
  if (!NativeInterlockedWanted() || !native::InterlockedCompareExchange(ctx, memory)) {
    __imp__sub_821CE0A0(ctx, base);
  }
}

REX_EXTERN(sub_821CE0D8) {
  GuestMemory memory{base};
  if (!NativeInterlockedWanted() || !native::InterlockedAdd(ctx, memory, 1)) {
    __imp__sub_821CE0D8(ctx, base);
  }
}

REX_EXTERN(sub_821CE100) {
  GuestMemory memory{base};
  if (!NativeInterlockedWanted() || !native::InterlockedAdd(ctx, memory, -1)) {
    __imp__sub_821CE100(ctx, base);
  }
}

REX_EXTERN(sub_821CE128) {
  GuestMemory memory{base};
  if (!NativeInterlockedWanted() || !native::InterlockedExchangeAdd(ctx, memory)) {
    __imp__sub_821CE128(ctx, base);
  }
}
