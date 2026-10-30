#pragma once

// KI-022: host requests for the SDK's bounded unconditional resync of the
// wheel's infinite force-feedback effects (centering spring, damper,
// continuous engine periodic). See
// docs/evidence/M9-006-ki-022-fix-build-20261030.md.
//
// The API exists only in the KI-022 dev SDK (branch mcla/ki-022-ffb-rearm,
// header rex/input/sdl/wheel_force_feedback_resync.h). Against an SDK without
// it (the pinned v0.20.0.3) the request compiles to a no-op that returns
// false, so the build cannot break on a missing symbol; the fix is then
// simply absent.

#if __has_include(<rex/input/sdl/wheel_force_feedback_resync.h>)
#define MCLA_HAS_WHEEL_EFFECT_RESYNC 1
namespace rex::input::sdl {
void RequestWheelEffectResync(const char *reason);
}  // namespace rex::input::sdl
#else
#define MCLA_HAS_WHEEL_EFFECT_RESYNC 0
#endif

namespace mcla::wheel_ffb {

// Thread-safe and cheap; the SDK coalesces requests and executes them on the
// input poll path of the active focused wheel (no-op without a wheel).
// Returns false when the SDK has no resync API.
inline bool RequestEffectResync(const char *reason) {
#if MCLA_HAS_WHEEL_EFFECT_RESYNC
  rex::input::sdl::RequestWheelEffectResync(reason);
  return true;
#else
  (void)reason;
  return false;
#endif
}

}  // namespace mcla::wheel_ffb
