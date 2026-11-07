// M11-007d / KI-047: guest texture lock probe and read-back completion at the
// lock (MCLA_LOCK v=1). See mcla_lock_probe.h for the lock core's layout, its
// citations and the placement of the completion.
//
// One pass-through strong override of sub_82421CA0 (the XDK lock core),
// active only with the init-only switches mcla_lock_probe and/or
// mcla_lock_readback_ensure (both default off; with both off the hook is one
// static bool test and the original call). It queries the locked physical
// range at entry (rex::gpu::ReadbackSyncQueryRange; short provider lock, no
// wait), calls the ORIGINAL UNCHANGED, queries again and:
//   - mcla_lock_probe: logs a paced MCLA_LOCK fn=lock line (32 lines, then
//     2/s) for every lock overlapping a read-back key and counts it per key;
//   - mcla_lock_readback_ensure: for an overlapping key whose copy is still
//     pending after the guest's own fence wait, calls
//     rex::gpu::ReadbackSyncEnsureRange(start, len, 20 ms) before returning
//     to the guest (the result goes on the same line).
// A MCLA_LOCK kind=minute line summarises each minute (written by the first
// lock after the minute). The hook reads only the lock flags (guest stack,
// checked reader) and never writes guest memory; no lock of this file is held
// across the original call or the SDK calls, so a nested or concurrent lock
// on another guest thread is safe. Without rex/graphics/readback_range.h (an
// older SDK) the hook only passes through and logs one kind=unavailable line.

#include "mcla_lock_probe.h"

#include "mcla_guest_reader.h"
#include "mcla_init.h"
#include "mcla_logging.h"

#include <rex/cvar.h>

#include <chrono>
#include <mutex>

#if __has_include(<rex/graphics/readback_range.h>)
#include <rex/graphics/readback_range.h>
#define MCLA_HAVE_READBACK_RANGE 1
#else
#define MCLA_HAVE_READBACK_RANGE 0
#endif

#if defined(_WIN32)
#include <windows.h>
#endif

REXCVAR_DEFINE_BOOL(mcla_lock_probe, false, "MCLA",
    "Log guest texture locks that overlap a GPU read-back key (M11-007d / "
    "KI-047; MCLA_LOCK lines; diagnostic, read-only)")
    .lifecycle(rex::cvar::Lifecycle::kInitOnly);

REXCVAR_DEFINE_BOOL(mcla_lock_readback_ensure, false, "MCLA",
    "Complete a pending GPU read-back copy of a locked range before the guest "
    "lock returns (M11-007d / KI-047; ReadbackSyncEnsureRange, 20 ms bound)")
    .lifecycle(rex::cvar::Lifecycle::kInitOnly);

namespace {

namespace lock = mcla::lock_probe;
using Reader = mcla::guest::Reader;

bool ProbeOn() {
  static const bool on = REXCVAR_GET(mcla_lock_probe);
  return on;
}

bool EnsureOn() {
  static const bool on = REXCVAR_GET(mcla_lock_readback_ensure);
  return on;
}

bool AnyOn() {
  static const bool on = ProbeOn() || EnsureOn();
  return on;
}

[[maybe_unused]] uint64_t NowMs() {
  return static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(
                                   std::chrono::steady_clock::now().time_since_epoch())
                                   .count());
}

[[maybe_unused]] uint32_t HostThreadId() {
#if defined(_WIN32)
  return static_cast<uint32_t>(GetCurrentThreadId());
#else
  return 0;
#endif
}

struct ProbeState {
  std::mutex mutex;
  lock::Minute minute;
  lock::MinuteClock clock;
  lock::LinePacer pacer;
  bool unavailable_logged = false;
};

ProbeState &State() {
  static ProbeState state;
  return state;
}

#if MCLA_HAVE_READBACK_RANGE
lock::KeyState Query(uint32_t phys, uint32_t length) {
  rex::gpu::ReadbackRangeInfo info;
  lock::KeyState k;
  k.overlap = rex::gpu::ReadbackSyncQueryRange(phys, length, &info);
  if (k.overlap) {
    k.key_base = info.key_base;
    k.key_length = info.key_length;
    k.slot_size = info.slot_size;
    k.width = info.width;
    k.height = info.height;
    k.format = info.format;
    k.exempt = info.exempt;
    k.pending = info.pending;
    k.unsubmitted = info.unsubmitted;
    k.copy_age_frames = info.copy_age_frames;
    k.resolve_age_frames = info.resolve_age_frames;
  }
  return k;
}

lock::EnsureState Ensure(uint32_t phys, uint32_t length) {
  const rex::gpu::ReadbackSyncEnsureResult r =
      rex::gpu::ReadbackSyncEnsureRange(phys, length, lock::kEnsureTimeoutMs);
  lock::EnsureState e;
  e.called = true;
  e.waited_ms = r.waited_ms;
  e.waited_us = r.waited_us;
  e.pending = r.pending;
  e.delivered = r.delivered;
  e.timed_out = r.timed_out;
  return e;
}
#endif

}  // namespace

// Lock core: r3 = resource, r4 = 64-bit selector, r7 = texture base,
// r8 = mip base, r9 = locked start (guest virtual), r10 = locked length,
// flags at r1 + 84; returns r3 = the locked pointer.
REX_EXTERN(sub_82421CA0) {
  if (!AnyOn()) {
    __imp__sub_82421CA0(ctx, base);
    return;
  }
#if MCLA_HAVE_READBACK_RANGE
  lock::Lock l;
  l.resource = ctx.r3.u32;
  l.r4 = ctx.r4.u64;
  l.texture_base = ctx.r7.u32;
  l.start = ctx.r9.u32;
  l.length = ctx.r10.u32;
  l.lr = static_cast<uint32_t>(ctx.lr);
  const uint32_t stack = ctx.r1.u32;
  const uint32_t phys = lock::GuestToPhysical(l.start);
  const bool query = lock::ShouldQuery(ProbeOn(), EnsureOn(), l.length);
  const lock::KeyState entry = query ? Query(phys, l.length) : lock::KeyState{};
  if (ProbeOn() && entry.overlap) {
    // Read before the original runs (its frame is below r1; the caller's
    // argument area at r1 + 84 is not touched by the core).
    const Reader read = mcla::guest::HookReader(base);
    bool ok = true;
    l.flags = read.U32(stack + lock::kFlagsStackOffset, ok);
    l.flags_read = ok;
  }
  __imp__sub_82421CA0(ctx, base);
  const lock::KeyState after = query ? Query(phys, l.length) : lock::KeyState{};
  const lock::Plan plan = lock::Decide(ProbeOn(), EnsureOn(), l.length, entry.overlap,
                                       after.overlap, after.pending);
  const lock::EnsureState ensure = plan.ensure ? Ensure(phys, l.length) : lock::EnsureState{};
  const uint64_t now = NowMs();
  auto &state = State();
  std::lock_guard guard(state.mutex);
  state.minute.Add(entry, after, ensure);
  if (plan.log) {
    uint32_t suppressed = 0;
    if (state.pacer.Allow(now, suppressed)) {
      l.tid = HostThreadId();
      MCLA_APP_INFO("{}", lock::LockLine(l, entry, after, ensure, suppressed));
    }
  }
  if (state.clock.Due(now)) {
    MCLA_APP_INFO("{}", lock::MinuteLine(state.minute, ProbeOn(), EnsureOn()));
    state.minute = lock::Minute{};
  }
#else
  __imp__sub_82421CA0(ctx, base);
  auto &state = State();
  std::lock_guard guard(state.mutex);
  if (!state.unavailable_logged) {
    state.unavailable_logged = true;
    MCLA_APP_INFO("MCLA_LOCK v=1 kind=unavailable reason=\"SDK without "
                  "rex/graphics/readback_range.h\"");
  }
#endif
}
