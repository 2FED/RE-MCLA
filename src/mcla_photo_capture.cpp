// KI-037: complete first Photo Mode capture (M9-006).
// See docs/evidence/M9-006-ki-037-photo-analysis-20261029.md and
// docs/evidence/M9-006-ki-037-fix-build-20261029.md.
//
// The photo album object (mcUIPhotoAlbum) captures the frame into a 1280x720
// LIN_A8R8G8B8 texture and JPEG-encodes it on the CPU a few frames later. The
// capture window is bracketed by two guest functions, both called only from
// the per-frame sub_8263EAA8:
//   sub_8263D120  freeze for capture: sets the capture-active byte 0x8290622E
//                 (only if clear), saves and changes the renderer masks and
//                 stores the world/render pause flags
//   sub_8263D1E0  unfreeze: restores them and clears 0x8290622E (only if set)
// With asynchronous pipeline creation the GPU plugin drops draws whose
// pipelines are still being created. On a fresh build (empty per-build
// pipeline cache) that removes geometry from exactly the frames that become
// the first photo (owner Test A, 2026-10-29). While the window is open this
// host turns on the SDK's hot-reload cvar d3d12_async_pipeline_wait, so the
// plugin waits (bounded) for such pipelines instead. The world is frozen then,
// so the one-off creation hitch is not visible in gameplay. The window logic
// is in mcla_photo_capture.h.
//
// KI-022: the capture freeze also changes the game's time scale, so after a
// window that opened, the unfreeze asks the SDK to resync the wheel's
// infinite force-feedback effects (reason host:photo-close). Follows the
// init-only mcla_photo_pipeline_wait switch (the window must be enabled).

#include "mcla_photo_capture.h"

#include "mcla_init.h"
#include "mcla_logging.h"
#include "mcla_wheel_ffb_resync.h"

#include <rex/cvar.h>

#include <chrono>
#include <mutex>

REXCVAR_DEFINE_BOOL(mcla_photo_pipeline_wait, true, "MCLA",
    "Photo Mode: wait for GPU pipelines still being created during the "
    "capture window instead of dropping their draws (KI-037)")
    .lifecycle(rex::cvar::Lifecycle::kInitOnly);

namespace {

constexpr uint32_t kCaptureActiveByte = 0x8290622Eu;
constexpr const char *kWaitCvar = "d3d12_async_pipeline_wait";

uint64_t NowMs() {
  return static_cast<uint64_t>(
      std::chrono::duration_cast<std::chrono::milliseconds>(
          std::chrono::steady_clock::now().time_since_epoch())
          .count());
}

// The hooks run on the guest thread that runs the game frame; shutdown runs
// on the UI thread. Frames outside the window take the lock and compare one
// bool.
std::mutex window_mutex;
// KI-022: set when a capture window opened; consumed by the unfreeze, also
// after a window that already ended by timeout. Guarded by window_mutex.
bool wheel_resync_on_unfreeze = false;
mcla::photo_capture::Window &GetWindow() {
  // Created at the first call, after the cvars are parsed. Disabled when the
  // option is off or the GPU plugin has no wait cvar (older SDK).
  static mcla::photo_capture::Window window([] {
    if (!REXCVAR_GET(mcla_photo_pipeline_wait)) {
      MCLA_APP_INFO("MCLA photo capture: pipeline wait disabled by option");
      return false;
    }
    if (!rex::cvar::GetFlagInfo(kWaitCvar)) {
      MCLA_APP_ERROR("MCLA photo capture: GPU plugin has no {}, pipeline wait "
                     "unavailable", kWaitCvar);
      return false;
    }
    return true;
  }());
  return window;
}

void Apply(const mcla::photo_capture::Decision &decision) {
  if (decision.write_wait) {
    // The GPU cvars live in the GPU plugin, so they are written through the
    // registry. Failure leaves the cvar unchanged.
    if (!rex::cvar::SetFlagByName(kWaitCvar,
                                  *decision.write_wait ? "true" : "false")) {
      MCLA_APP_ERROR("MCLA photo capture: could not set {}={}", kWaitCvar,
                     *decision.write_wait);
    }
  }
  if (!decision.log) {
    return;
  }
  const char *wait = decision.write_wait
                         ? (*decision.write_wait ? "on" : "off")
                         : "unchanged";
  if (decision.event == mcla::photo_capture::Event::kOpen) {
    MCLA_APP_INFO("MCLA photo capture: {}, pipeline wait {}",
                  mcla::photo_capture::EventName(decision.event), wait);
  } else {
    MCLA_APP_INFO("MCLA photo capture: {} after {} frames ({} ms), pipeline "
                  "wait {}",
                  mcla::photo_capture::EventName(decision.event),
                  decision.frames, decision.elapsed_ms, wait);
  }
}

}  // namespace

namespace mcla::photo_capture {

void ShutdownPhotoCapture() {
  std::lock_guard lock(window_mutex);
  Apply(GetWindow().Shutdown(NowMs()));
}

}  // namespace mcla::photo_capture

// Strong override of the generated weak capture-freeze function.
REX_EXTERN(sub_8263D120) {
  __imp__sub_8263D120(ctx, base);
  if (!REX_LOAD_U8(kCaptureActiveByte)) {
    return;
  }
  std::lock_guard lock(window_mutex);
  auto &window = GetWindow();
  if (!window.enabled() || window.open()) {
    return;
  }
  const auto decision =
      window.Open(NowMs(), rex::cvar::Query<bool>(kWaitCvar));
  if (decision.transition) {
    wheel_resync_on_unfreeze = true;
  }
  Apply(decision);
}

// Strong override of the generated weak capture-unfreeze function.
REX_EXTERN(sub_8263D1E0) {
  __imp__sub_8263D1E0(ctx, base);
  if (REX_LOAD_U8(kCaptureActiveByte)) {
    return;
  }
  std::lock_guard lock(window_mutex);
  Apply(GetWindow().Close(NowMs()));
  if (wheel_resync_on_unfreeze) {
    wheel_resync_on_unfreeze = false;
    mcla::wheel_ffb::RequestEffectResync("host:photo-close");
  }
}

// Strong override of the generated weak per-frame photo-album update that
// calls both functions above; it only drives the safety timeout.
REX_EXTERN(sub_8263EAA8) {
  __imp__sub_8263EAA8(ctx, base);
  std::lock_guard lock(window_mutex);
  auto &window = GetWindow();
  if (window.open()) {
    Apply(window.Tick(NowMs()));
  }
}
