// M11 l106 guest MSAA off (MCLA_GMSAA v=1). See mcla_guest_msaa.h for the
// guest layout and its generated-code citations, and
// docs/evidence/M11-l106-guest-msaa-off-20261115.md (RE, mechanism, risks).
//
// Two strong overrides; both call the ORIGINAL:
//   sub_822E4AB0 (renderer creation, once from the graphics start-up):
//     after the original, reads the renderer pointer and its mode +384 and
//     writes one MCLA_GMSAA line (always). With mcla_guest_msaa_off (default
//     false, off) and a plausible renderer whose mode is 2 or 4 it stores 1
//     at +384 first; l107: also with mcla_guest_msaa_scene=1
//     (mcla_guest_tiling.cpp), so the scene 1x control starts from the
//     game's own 1x mode.
//   sub_822E4CC8 (render-target set-up, r3 = the renderer): read-only; at the
//     first call logs the mode the scene targets are about to be created
//     with, then runs the original.
// With the cvar at its default no guest memory or register is written.

#include "mcla_guest_msaa.h"

#include "mcla_guest_tiling.h"
#include "mcla_init.h"
#include "mcla_logging.h"

#include <rex/cvar.h>

#include <atomic>
#include <mutex>
#include <string>

REXCVAR_DEFINE_BOOL(mcla_guest_msaa_off, false, "MCLA",
    "Create and draw the game's main scene without MSAA: after the renderer "
    "is created, its multisample mode (+384, 2 = 2x on an HD output) is set "
    "to 1 before any render target exists (dev control; MCLA_GMSAA)")
    .lifecycle(rex::cvar::Lifecycle::kInitOnly);

namespace {

namespace gm = mcla::guest_msaa;

bool Requested() {
  static const bool requested =
      REXCVAR_GET(mcla_guest_msaa_off) || MclaGuestTilingRequestsCreationOne();
  return requested;
}

struct Shared {
  std::mutex mutex;
  gm::State state;  // under the mutex
  std::atomic<uint64_t> target_setups{0};
  std::atomic<uint8_t *> base{nullptr};
};

Shared &SharedState() {
  static Shared shared;
  return shared;
}

}  // namespace

// Renderer creation (no arguments; the renderer pointer is the word at
// 0x8287E064 afterwards).
REX_EXTERN(sub_822E4AB0) {
  __imp__sub_822E4AB0(ctx, base);

  const bool requested = Requested();
  const uint32_t renderer = REX_LOAD_U32(gm::kRendererPtr);
  const bool plausible = gm::PointerPlausible(renderer);
  int64_t before = gm::kModeUnknown;
  uint32_t width = 0;
  uint32_t height = 0;
  if (plausible) {
    before = static_cast<int32_t>(REX_LOAD_U32(renderer + gm::kModeOffset));
    width = REX_LOAD_U32(renderer + gm::kWidthOffset);
    height = REX_LOAD_U32(renderer + gm::kHeightOffset);
  }
  const gm::Outcome outcome = gm::Decide(requested, plausible, before);
  if (gm::Writes(outcome)) {
    // The value the game's own branch at loc_822E4BA0 stores.
    REX_STORE_U32(renderer + gm::kModeOffset, static_cast<uint32_t>(gm::kModeOne));
  }
  const int64_t after =
      plausible ? static_cast<int64_t>(static_cast<int32_t>(REX_LOAD_U32(renderer + gm::kModeOffset)))
                : gm::kModeUnknown;
  auto &s = SharedState();
  s.base.store(base, std::memory_order_relaxed);
  {
    std::lock_guard guard(s.mutex);
    s.state.requested = requested;
    s.state.created = true;
    s.state.outcome = outcome;
    s.state.renderer = renderer;
    s.state.mode_before = before;
    s.state.mode_after = after;
    s.state.width = width;
    s.state.height = height;
  }
  MCLA_APP_INFO("{} outcome={} renderer=0x{:08X} size={}x{}",
                gm::Line(requested, gm::Writes(outcome), before, after, gm::kWhereRendererCreate),
                gm::OutcomeName(outcome), renderer, width, height);
}

// Render-target set-up: r3 = the renderer, r4 = 1 (from sub_82305230).
REX_EXTERN(sub_822E4CC8) {
  auto &s = SharedState();
  const uint64_t call = s.target_setups.fetch_add(1, std::memory_order_relaxed);
  if (call == 0) {
    const uint32_t renderer = ctx.r3.u32;
    int64_t mode = gm::kModeUnknown;
    if (gm::PointerPlausible(renderer)) {
      mode = static_cast<int32_t>(REX_LOAD_U32(renderer + gm::kModeOffset));
    }
    bool requested = false;
    bool applied = false;
    {
      std::lock_guard guard(s.mutex);
      s.state.target_setup_mode = mode;
      requested = s.state.requested;
      applied = gm::Writes(s.state.outcome) && renderer == s.state.renderer &&
                mode == gm::kModeOne;
    }
    MCLA_APP_INFO("{} renderer=0x{:08X}",
                  gm::Line(requested, applied, mode, mode, gm::kWhereTargetSetup), renderer);
  }
  __imp__sub_822E4CC8(ctx, base);
}

std::string MclaGuestMsaaStateJson() {
  auto &s = SharedState();
  gm::State state;
  {
    std::lock_guard guard(s.mutex);
    state = s.state;
  }
  if (!state.created) {
    state.requested = Requested();
  }
  state.target_setups = s.target_setups.load(std::memory_order_relaxed);
  // A fresh read of the mode (the renderer lives for the whole session).
  uint8_t *base = s.base.load(std::memory_order_relaxed);
  if (base != nullptr && gm::PointerPlausible(state.renderer) &&
      REX_LOAD_U32(gm::kRendererPtr) == state.renderer) {
    state.mode_now = static_cast<int32_t>(REX_LOAD_U32(state.renderer + gm::kModeOffset));
  }
  return gm::StateJson(state);
}
