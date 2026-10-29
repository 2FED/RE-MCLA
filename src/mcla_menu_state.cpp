// Unlocked-mode menu cap from the game's own pause state (M9-007).
// See docs/evidence/M9-007-menu-state-analysis-20261029.md.
//
// The unlocked mode (mcla_guest_vblank_hz > 0) keeps menus at the stock rate
// with a draw-count heuristic in the GPU plugin (mcla_ui_cap_draws_low). Menus
// drawn over the world (pause, map/Arcade, race setup) keep a high draw count
// and stay uncapped. The game itself pauses its simulation for those screens:
//
//   game-state object  G = *(be32*)0x828CCEA0   (allocated by sub_822D29A0,
//                        constructed by sub_822D2818, base sub_8238A988)
//   G+4  simulation paused  (requests 1 PauseSimulation / 7 PauseOnly set it,
//                            2 ResumeSimulation / 8 ResumeOnly clear it)
//   G+6  paused locally     (requests 3 / 4; online sessions)
//   G+10 one-frame step     (request 6 FrameAdvance; cleared every update)
//
// Requests are queued by vtable slot 2 (sub_822D26D0) and applied by slot 3,
// sub_822D29E8, which the per-frame game update sub_822C1FA8 calls first.
// UILogic_PauseGame (sub_82215A60) requests 1 (or 3 when online) and is called
// by the pause, map/nav, Arcade, prompt and tutorial screens. The script
// binding Game_IsPaused (sub_822BF228) returns G+4 && !G+10.
//
// After each applied update this hook forces the existing UI cap while the
// game is paused, by raising mcla_ui_cap_draws_low (hot-reload) so the next
// presented frame counts as a UI frame. On resume, and on host shutdown, the
// saved threshold is restored and the unchanged draw-count hysteresis (8 heavy
// frames) releases the cap, so the heuristic stays the fallback for everything
// else. The transition logic is in mcla_menu_state.h.

#include "mcla_menu_state.h"

#include "mcla_init.h"
#include "mcla_logging.h"

#include <rex/cvar.h>

#include <mutex>
#include <string>

REXCVAR_DEFINE_BOOL(mcla_ui_cap_guest_pause, true, "MCLA",
    "Unlocked mode: keep the stock vblank rate while the game's own "
    "simulation pause is set (pause, map, Arcade and race setup menus)")
    .lifecycle(rex::cvar::Lifecycle::kInitOnly);

namespace {

constexpr uint32_t kGameStatePointer = 0x828CCEA0u;
constexpr const char *kLowCvar = "mcla_ui_cap_draws_low";

// The hook runs on the guest thread that updates the game; shutdown runs on
// the UI thread. Steady frames take the lock once and compare one bool.
std::mutex controller_mutex;
mcla::menu_state::Controller controller;

bool GuestPauseWanted() {
  static const bool wanted = REXCVAR_GET(mcla_ui_cap_guest_pause);
  return wanted;
}

void WriteLow(int32_t value) {
  // The GPU cvars live in the GPU plugin, so they are written through the
  // registry. Failure (unknown name, range) leaves the cvar unchanged.
  if (!rex::cvar::SetFlagByName(kLowCvar, std::to_string(value))) {
    MCLA_APP_ERROR("MCLA menu state: could not set {}={}", kLowCvar, value);
  }
}

}  // namespace

namespace mcla::menu_state {

void ShutdownMenuStateCap() {
  std::lock_guard lock(controller_mutex);
  if (controller.stopped()) {
    return;
  }
  const int32_t current =
      controller.forcing() ? rex::cvar::Query<int32_t>(kLowCvar) : 0;
  if (const auto restore = controller.Shutdown(current)) {
    WriteLow(*restore);
    MCLA_APP_INFO("MCLA menu state: shutdown, {} restored to {}", kLowCvar,
                  *restore);
  }
}

}  // namespace mcla::menu_state

// Strong override of the generated weak game-state update (vtable slot 3).
REX_EXTERN(sub_822D29E8) {
  const uint32_t state = ctx.r3.u32;
  __imp__sub_822D29E8(ctx, base);
  if (!GuestPauseWanted() || !state || state != REX_LOAD_U32(kGameStatePointer)) {
    return;
  }
  const uint8_t simulation = REX_LOAD_U8(state + 4);
  const uint8_t local = REX_LOAD_U8(state + 6);
  const bool paused = simulation != 0 || local != 0;
  std::lock_guard lock(controller_mutex);
  if (!controller.NeedsInputs(paused)) {
    return;
  }
  mcla::menu_state::Inputs inputs;
  inputs.guest_vblank_hz = rex::cvar::Query<int32_t>("mcla_guest_vblank_hz");
  inputs.current_low = rex::cvar::Query<int32_t>(kLowCvar);
  const auto decision = controller.Observe(paused, inputs);
  if (decision.write_low) {
    WriteLow(*decision.write_low);
  }
  if (decision.log) {
    MCLA_APP_INFO("MCLA menu state: game {} (sim {} local {}), UI cap {}, {} {}",
                  paused ? "paused" : "running", simulation, local,
                  controller.forcing() ? "forced" : "draw-count", kLowCvar,
                  decision.write_low ? *decision.write_low : inputs.current_low);
  }
}
