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
//
// Hangout / meet spot (owner l41, 2026-10-29): the wager / pink-slip menu at
// a hangout does not pause the simulation (G+4 = G+6 = 0; the crowd keeps
// animating). It deactivates the local racer instead: Racer_Deactivate
// (sub_822AB258, script native sub_822AD200) sets byte racer+908 = 1 and
// turns the racer's input off; the reset/teleport and activate paths clear
// it. racer = *(*(be32*)0x82874374 + 8), +656 == 3 for an active racer. Both
// owner hangout packages showed +908 = 1 with the simulation running; a
// free-roam driving package showed 0. The camera input (sub_822B35E8) runs
// only while +908 == 0, so it cannot be set while the player drives. Other
// setters (nav-map BeginWarp sub_8221A440, race-over UI, network player
// handlers) are non-interactive screens where stock also ran at 30.
// The racer pointers are followed only through checked reads (guest heap
// bounds and page protection) and not at all after ShutdownMenuStateCap;
// --mcla_ui_cap_racer_inactive=false turns this signal off.

#include "mcla_menu_state.h"

#include "mcla_init.h"
#include "mcla_logging.h"

#include <rex/cvar.h>
#include <rex/system/kernel_state.h>
#include <rex/system/xmemory.h>

#include <atomic>
#include <mutex>
#include <string>

REXCVAR_DEFINE_BOOL(mcla_ui_cap_guest_pause, true, "MCLA",
    "Unlocked mode: keep the stock vblank rate while the game's own "
    "simulation pause is set (pause, map, Arcade and race setup menus)")
    .lifecycle(rex::cvar::Lifecycle::kInitOnly);

REXCVAR_DEFINE_BOOL(mcla_ui_cap_racer_inactive, true, "MCLA",
    "Unlocked mode: also keep the stock vblank rate while the local racer is "
    "deactivated by the game (hangout / meet-spot race menus, warps)")
    .lifecycle(rex::cvar::Lifecycle::kInitOnly);

namespace {

constexpr uint32_t kGameStatePointer = 0x828CCEA0u;
constexpr uint32_t kPlayerManagerPointer = 0x82874374u;  // +8 -> local racer
constexpr uint32_t kRacerState = 656;                   // 3 = active
constexpr uint32_t kRacerInputOff = 908;                // Racer_Deactivate
constexpr const char *kLowCvar = "mcla_ui_cap_draws_low";

// The hook runs on the guest thread that updates the game; shutdown runs on
// the UI thread. Steady frames take the lock once and compare one bool.
std::mutex controller_mutex;
mcla::menu_state::Controller controller;

bool GuestPauseWanted() {
  static const bool wanted = REXCVAR_GET(mcla_ui_cap_guest_pause);
  return wanted;
}

bool RacerInactiveWanted() {
  static const bool wanted = REXCVAR_GET(mcla_ui_cap_racer_inactive);
  return wanted;
}

// Set by ShutdownMenuStateCap: the racer is no longer read, so a player
// manager torn down with the host cannot be followed.
std::atomic<bool> racer_reads_stopped{false};

// The racer pointers are followed only when the address lies inside a guest
// heap and the page is readable (the KI-026 probe's check: heap bounds before
// QueryProtect, both ends of the span). Spans are at most 4 bytes, so both
// ends cover every page they touch. Check and read are not atomic; a page
// decommitted in between remains a residual risk.
bool GuestReadable(rex::memory::Memory *memory, uint32_t address,
                   uint32_t size) {
  if (!memory || !address || address > UINT32_MAX - (size - 1)) {
    return false;
  }
  for (const uint32_t probe : {address, address + size - 1}) {
    auto *heap = memory->LookupHeap(probe);
    uint32_t protect = 0;
    if (!heap || probe < heap->heap_base() ||
        probe - heap->heap_base() >= heap->heap_size() ||
        !heap->QueryProtect(probe, &protect) ||
        !(protect & rex::memory::kMemoryProtectRead)) {
      return false;
    }
  }
  return true;
}

// Byte racer+908 of the local racer, or 0 when there is no active racer or a
// read is refused.
uint8_t ReadRacerInputOff(uint8_t *base) {
  if (!RacerInactiveWanted() ||
      racer_reads_stopped.load(std::memory_order_acquire)) {
    return 0;
  }
  auto *kernel = rex::system::kernel_state();
  rex::memory::Memory *memory = kernel ? kernel->memory() : nullptr;
  if (!GuestReadable(memory, kPlayerManagerPointer, 4)) {
    return 0;
  }
  const uint32_t players = REX_LOAD_U32(kPlayerManagerPointer);
  if (!players || !GuestReadable(memory, players + 8, 4)) {
    return 0;
  }
  const uint32_t racer = REX_LOAD_U32(players + 8);
  const bool racer_read = racer != 0 &&
                          GuestReadable(memory, racer + kRacerState, 4) &&
                          GuestReadable(memory, racer + kRacerInputOff, 1);
  return mcla::menu_state::RacerInputOff(
      racer_read, racer_read ? REX_LOAD_U32(racer + kRacerState) : 0,
      racer_read ? REX_LOAD_U8(racer + kRacerInputOff) : 0);
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
  racer_reads_stopped.store(true, std::memory_order_release);
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
  const uint8_t racer_off = ReadRacerInputOff(base);
  const bool paused = mcla::menu_state::CapPaused(simulation, local, racer_off);
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
    MCLA_APP_INFO("MCLA menu state: game {} (sim {} local {} racer-off {}), "
                  "UI cap {}, {} {}",
                  paused ? "paused" : "running", simulation, local, racer_off,
                  controller.forcing() ? "forced" : "draw-count", kLowCvar,
                  decision.write_low ? *decision.write_low : inputs.current_low);
  }
}
