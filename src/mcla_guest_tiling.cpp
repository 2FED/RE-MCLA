// M11 l107 guest tiling and scene MSAA (MCLA_GTILE v=1). See
// mcla_guest_tiling.h for the guest layout and its generated-code citations,
// and docs/evidence/M11-l107-guest-tiling-and-msaa-20261117.md (RE,
// mechanism, risks).
//
// One strong override; it calls the ORIGINAL:
//   sub_8217A470 (BeginTiledRendering; from the scene begin, the SD pass and
//     the shadow / depth collector pass): before the original, with
//     mcla_guest_msaa_scene 1 or 2 (default 0, off) and a call from one of
//     the two renderer passes whose sample count r6 is higher, lowers r6 to
//     the request. After the original (always) it reads the tile count and
//     rectangles the game stored and the renderer bytes that decide the
//     count, and writes one MCLA_GTILE line per caller at the first call and
//     whenever the observation changes (at most 32 lines per caller).
// With the cvar at its default no guest memory or register is written; the
// only effect is the reads and the lines.

#include "mcla_guest_tiling.h"

#include "mcla_guest_msaa.h"
#include "mcla_init.h"
#include "mcla_logging.h"

#include <rex/cvar.h>

#include <mutex>
#include <string>

REXCVAR_DEFINE_INT32(mcla_guest_msaa_scene, 0, "MCLA",
    "Draw the game's main scene (and its SD-output pass) with at most this "
    "many samples: 1 or 2 (0 = the game's own count, 4x on the 960x540 SD "
    "guest mode); 1 also sets the renderer mode +384 to 1 at creation "
    "(dev control; MCLA_GTILE)")
    .lifecycle(rex::cvar::Lifecycle::kInitOnly);

namespace {

namespace gt = mcla::guest_tiling;

int32_t Cvar() {
  static const int32_t value = REXCVAR_GET(mcla_guest_msaa_scene);
  return value;
}

struct Shared {
  std::mutex mutex;
  gt::State state;  // under the mutex
};

Shared &SharedState() {
  static Shared shared;
  return shared;
}

int32_t S32(uint8_t *base, uint32_t address) {
  return static_cast<int32_t>(REX_LOAD_U32(address));
}

}  // namespace

// BeginTiledRendering: r6 = the sample count; the return address tells the
// caller.
REX_EXTERN(sub_8217A470) {
  const gt::Where where = gt::FromReturn(static_cast<uint32_t>(ctx.lr));
  const int32_t cvar = Cvar();
  const int requested = gt::RequestedCount(cvar);
  const int game_count = static_cast<int32_t>(ctx.r6.u32);
  const int count = gt::AppliedCount(game_count, requested, where);
  const bool rewrote = count != game_count;
  if (rewrote) {
    ctx.r6.u64 = static_cast<uint64_t>(static_cast<int64_t>(count));
  }

  // The renderer bytes as the callers read them (before the original runs).
  gt::Observation o;
  const uint32_t renderer = REX_LOAD_U32(gt::kRendererPtr);
  int32_t sd_pass_mode = -1;
  if (mcla::guest_msaa::PointerPlausible(renderer)) {
    o.mode = S32(base, renderer + gt::kModeOffset);
    sd_pass_mode = S32(base, renderer + gt::kSdPassModeOffset);
    o.force4 = REX_LOAD_U8(renderer + gt::kForce4Offset);
    uint8_t tls_byte = 1;
    const uint32_t tls = REX_LOAD_U32(ctx.r13.u32);
    if (tls != 0) {
      tls_byte = REX_LOAD_U8(tls + gt::kTlsByteOffset);
    }
    o.sd = gt::EffectiveSdFlag(REX_LOAD_U8(renderer + gt::kSdFlagOffset),
                               REX_LOAD_U8(renderer + gt::kPendingOffset),
                               REX_LOAD_U8(renderer + gt::kPendingValueOffset), tls_byte);
  }
  const bool columns = (ctx.r7.u32 & 1u) != 0;
  const int overlap = static_cast<int32_t>(ctx.r8.u32);

  __imp__sub_8217A470(ctx, base);

  o.tiles = S32(base, gt::kTileCount);
  o.msaa = count;
  o.game_msaa = game_count;
  o.applied = rewrote;
  o.forced4x = gt::Forced4x(where, game_count, gt::BaseCount(where, o.mode, sd_pass_mode));
  gt::Rect rects[gt::kMaxTileRects];
  const int n = std::clamp(o.tiles, 0, static_cast<int>(gt::kMaxTileRects));
  for (int i = 0; i < n; ++i) {
    const uint32_t at = gt::kTileRects + static_cast<uint32_t>(i) * gt::kTileRectBytes;
    rects[i] = {S32(base, at), S32(base, at + 4), S32(base, at + 8), S32(base, at + 12)};
  }
  const gt::Cover cover = gt::CoverOf(rects, n);
  o.width = cover.width;
  o.height = cover.height;
  o.first_span = columns && n > 0 ? rects[0].x2 - rects[0].x1 : cover.first_span;
  o.rule_tiles = gt::GameTileRule(cover.width, cover.height, count, columns, overlap).count;

  auto &s = SharedState();
  bool write = false;
  uint64_t calls = 0;
  {
    std::lock_guard guard(s.mutex);
    s.state.cvar = cvar;
    auto &w = s.state.where[static_cast<size_t>(where)];
    write = gt::Record(w, o, rewrote);
    calls = w.calls;
  }
  if (write) {
    MCLA_APP_INFO("{}", gt::Line(o, requested, where, calls));
  }
}

std::string MclaGuestTilingStateJson() {
  auto &s = SharedState();
  gt::State state;
  {
    std::lock_guard guard(s.mutex);
    state = s.state;
  }
  state.cvar = Cvar();
  return gt::StateJson(state);
}

// mcla_guest_msaa.cpp: the scene 1x request also makes the creation write.
bool MclaGuestTilingRequestsCreationOne() {
  return gt::RequestsCreationOne(Cvar());
}
