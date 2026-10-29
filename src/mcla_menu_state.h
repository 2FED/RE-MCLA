#pragma once

// M9-007 menu-state cap: pure transition logic, no SDK dependencies, so it
// can be unit-tested (tests/mcla_menu_state_test.cpp). The guest hook and the
// cvar access live in mcla_menu_state.cpp.
//
// The controller acts only on edges of the game's pause state. On a pause
// edge it decides whether to force the UI cap by raising the GPU cvar
// mcla_ui_cap_draws_low to kForcedLow; on the resume edge (and on host
// shutdown) it restores the value it saved. It never writes the forced value
// when the cvar already holds it, and it never restores over a value the user
// changed while the game was paused.

#include <cstdint>
#include <optional>

namespace mcla::menu_state {

// Upper bound of the mcla_ui_cap_draws_low range: every frame counts as a UI
// frame, so the GPU plugin's existing cap turns on at the next frame.
inline constexpr int32_t kForcedLow = 100000;

// At most this many transitions are logged per process.
inline constexpr int kMaxLoggedTransitions = 200;

struct Inputs {
  // mcla_guest_vblank_hz; > 0 means the unlocked mode is configured.
  int32_t guest_vblank_hz = 0;
  // Current mcla_ui_cap_draws_low; <= 0 means the user disabled the UI cap.
  int32_t current_low = 0;
};

struct Decision {
  // True when the observed pause state changed (log candidates).
  bool transition = false;
  // True when this transition should be logged (bounded count).
  bool log = false;
  // Value to write to mcla_ui_cap_draws_low, if any.
  std::optional<int32_t> write_low;
};

class Controller {
 public:
  // True when the controller needs the cvar inputs for this observation: only
  // on a pause edge while it is still active. Callers query the cvars only
  // when this returns true, so steady frames cost one comparison.
  bool NeedsInputs(bool paused) const { return !stopped_ && paused != paused_; }

  // Records one observation of the pause state. `inputs` is read only when
  // NeedsInputs(paused) was true.
  Decision Observe(bool paused, const Inputs &inputs) {
    Decision decision;
    if (!NeedsInputs(paused)) {
      return decision;
    }
    paused_ = paused;
    decision.transition = true;
    if (logged_ < kMaxLoggedTransitions) {
      ++logged_;
      decision.log = true;
    }
    if (paused) {
      // Stock mode, or the UI cap disabled by the user: leave everything.
      if (inputs.guest_vblank_hz <= 0 || inputs.current_low <= 0) {
        return decision;
      }
      forcing_ = true;
      saved_low_ = inputs.current_low;
      wrote_ = inputs.current_low != kForcedLow;
      if (wrote_) {
        decision.write_low = kForcedLow;
      }
      return decision;
    }
    decision.write_low = Release(inputs.current_low);
    return decision;
  }

  // Host shutdown: restore the saved threshold if it is still forced, and
  // ignore every later observation.
  std::optional<int32_t> Shutdown(int32_t current_low) {
    if (stopped_) {
      return std::nullopt;
    }
    stopped_ = true;
    return Release(current_low);
  }

  bool paused() const { return paused_; }
  bool forcing() const { return forcing_; }
  bool stopped() const { return stopped_; }
  int32_t saved_low() const { return saved_low_; }
  int logged() const { return logged_; }

 private:
  std::optional<int32_t> Release(int32_t current_low) {
    const bool restore = forcing_ && wrote_ && current_low == kForcedLow;
    forcing_ = false;
    wrote_ = false;
    if (!restore) {
      return std::nullopt;
    }
    return saved_low_;
  }

  bool paused_ = false;
  bool forcing_ = false;
  bool wrote_ = false;
  bool stopped_ = false;
  int32_t saved_low_ = 0;
  int logged_ = 0;
};

// Host shutdown hook (defined in mcla_menu_state.cpp): restores a forced
// mcla_ui_cap_draws_low so a later settings save cannot persist it.
void ShutdownMenuStateCap();

}  // namespace mcla::menu_state
