// KI-026: after a lost Red Light event + Race Back (or at a Red Light race
// start) the aerial warp camera can stay on screen while the car still drives;
// pause and the change-camera button do nothing.
//
// Static analysis (docs/evidence/M9-006-ki-026-camera-analysis-20261029.md):
// - The warp is mcWarper (UILogic + 0x51240, UILogic = *(0x8286D8D4)); its
//   per-frame update is sub_8221BE98 (state at +288, 29 = idle).
// - "RECORD WARP CAM MATRIX" and Warper_OnCameraFinished freeze the camera via
//   UILogic + 0x51382 (sub_82204480 / sub_82204558). It is released only by
//   sub_82204468 (warp states 12/17 after the cutscene starts, the zoom-in
//   camera setup) or by the script call UILogic_UnPauseCamera (sub_822079E8).
// - While +0x51382 is set, sub_82209150 makes the player-camera input update
//   (sub_822B35E8) return early, so RB/R1 does nothing; vehicle input is not
//   affected.
//
// The F10 state block is always on and read-only. Everything else is default
// OFF: mcla_ki026_probe only logs and never changes guest state;
// mcla_ki026_watchdog releases a camera pause only for the two stuck
// signatures below, after they persist, and logs what it did. Every guest
// read in this file except the bare register values goes through
// GuestReader, which checks the guest heap bounds and page protection before
// dereferencing.
//
// KI-035 (docs/evidence/M10-ki035-warp-probe-20261104.md) adds the warp
// camera to the F10 block, default-off warp state logging (--mcla_warp_probe)
// and a default-off state-20 watchdog (--mcla_warp_watchdog_seconds). Their
// decisions are pure and unit-tested (mcla_warp_probe.h).

#include "mcla_ki026_probe.h"

#include "mcla_init.h"
#include "mcla_logging.h"
#include "mcla_warp_probe.h"

#include <rex/cvar.h>
#include <rex/system/kernel_state.h>
#include <rex/system/xmemory.h>

#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <sstream>
#include <string>

REXCVAR_DEFINE_BOOL(mcla_ki026_probe, false, "MCLA",
    "Log KI-026 warp, camera-pause and pause-request transitions")
    .lifecycle(rex::cvar::Lifecycle::kInitOnly);

REXCVAR_DEFINE_BOOL(mcla_ki026_watchdog, false, "MCLA",
    "Release a camera pause left behind by a finished or stalled warp (KI-026)")
    .lifecycle(rex::cvar::Lifecycle::kInitOnly);

REXCVAR_DEFINE_BOOL(mcla_warp_probe, false, "MCLA",
    "Log KI-035 warp state changes with the warp camera time and duration")
    .lifecycle(rex::cvar::Lifecycle::kInitOnly);

REXCVAR_DEFINE_INT32(mcla_warp_watchdog_seconds, 0, "MCLA",
    "Finish a cinematic warp left in state 20 with a frozen camera after this "
    "many seconds (KI-035; 0 = off)")
    .range(0, 600)
    .lifecycle(rex::cvar::Lifecycle::kInitOnly);

namespace {

constexpr uint32_t kUiLogicPtr = 0x8286D8D4u;       // UILogic object
constexpr uint32_t kUiManagerPtr = 0x8286D804u;     // +48 -> UILogic (pause writers)
constexpr uint32_t kPauseManagerPtr = 0x828CCEA0u;  // simulation pause requests
constexpr uint32_t kCutscenePtr = 0x8286D8E8u;      // cutscene player
constexpr uint32_t kPlayerManagerPtr = 0x82874374u; // +8 -> local racer
constexpr uint32_t kMainTimer = 0x827D7500u;        // +84 scale, +92 step
constexpr uint32_t kGameTimeScale = 0x827E1C34u;
constexpr uint32_t kPauseOverrideByte = 0x8286E800u;

constexpr uint32_t kWarperOffset = 0x51240u;
constexpr uint32_t kGpsJumping = 0x51381u;
constexpr uint32_t kCameraPaused = 0x51382u;
constexpr uint32_t kCameraPauseRequested = 0x51383u;
constexpr uint32_t kCameraPauseId = 0x51384u;

constexpr uint32_t kWarperQueueCount = 64;
constexpr uint32_t kWarperRequest = 68;
constexpr uint32_t kWarperPrestreamTimer = 264;
constexpr uint32_t kWarperPrestreamFrames = 268;
constexpr uint32_t kWarperFlagsFirst = 272;  // 272..280 bytes
constexpr uint32_t kWarperCutsceneWait = 274;
constexpr uint32_t kWarperState = 288;
constexpr uint32_t kWarperCamera = 296;
constexpr uint32_t kWarperIdle = 29;

// KI-035 warp camera (docs/evidence/M10-ki035-warp-probe-20261104.md).
constexpr uint32_t kWarperExitFlag = 276;      // stb r28(=1),276(r31) at 0x8221C1D8
constexpr uint32_t kWarperCameraCopy = 300;    // stw at 0x82219EA8 (sub_82219DA0)
constexpr uint32_t kRequestCamera = 108;       // stw at 0x82219DE8 (sub_82219DA0)
constexpr uint32_t kUiTransitions = 52;        // *(UIManager) + 52 (sub_82220980 r3)
constexpr uint32_t kActiveTransitions = 0x40850u;  // +0 array, +4 u16 count
constexpr uint32_t kExitGlobalPtr = 0x8286D8F0u;   // lwz r11,-10000(0x82870000)
constexpr uint32_t kExitGlobalByte = 293;          // stb r26(=0),293(r11) at 0x8221C1D4
constexpr uint32_t kMaxActiveScan = 256;

constexpr uint32_t kRacerCamera = 0x370;  // mcPlayerCamera
constexpr uint32_t kRacerState = 656;     // 3 = active (racer reset path)
constexpr uint32_t kRacerCameraInputOff = 908;

constexpr uint32_t kGuestPage = 4096;

bool ProbeOn() {
  static const bool on = REXCVAR_GET(mcla_ki026_probe);
  return on;
}

bool WatchdogOn() {
  static const bool on = REXCVAR_GET(mcla_ki026_watchdog);
  return on;
}

bool WarpProbeOn() {
  static const bool on = REXCVAR_GET(mcla_warp_probe);
  return on;
}

uint64_t WarpWatchdogLimitMs() {
  static const uint64_t limit =
      mcla::warp::WatchdogLimitMs(REXCVAR_GET(mcla_warp_watchdog_seconds));
  return limit;
}

std::atomic<uint32_t> warp_watchdog_fires{0};

// Checked guest reads. A read is allowed only when the address lies inside a
// guest heap and the page protection includes read access. Spans are at most
// one guest page, so checking both ends covers every page they touch. A
// failed read returns 0 and clears `ok`. The check and the read are not
// atomic: a page decommitted in between is a residual (diagnostic-only) risk.
class GuestReader {
 public:
  GuestReader(rex::memory::Memory *memory, uint8_t *base) : memory_(memory), base_(base) {}

  bool Readable(uint32_t address, uint32_t size) const {
    return Accessible(address, size, rex::memory::kMemoryProtectRead);
  }
  // Used only by the KI-035 watchdog before its three guest stores.
  bool Writable(uint32_t address, uint32_t size) const {
    return Accessible(address, size,
                      rex::memory::kMemoryProtectRead | rex::memory::kMemoryProtectWrite);
  }
  bool Accessible(uint32_t address, uint32_t size, uint32_t required) const {
    if (!memory_ || !base_ || !address || size == 0 || size > kGuestPage ||
        address > UINT32_MAX - (size - 1)) {
      return false;
    }
    for (const uint32_t probe : {address, address + size - 1}) {
      auto *heap = memory_->LookupHeap(probe);
      uint32_t protect = 0;
      // Bounds first: BaseHeap::QueryProtect accepts a page index equal to
      // the page-table size.
      if (!heap || probe < heap->heap_base() ||
          probe - heap->heap_base() >= heap->heap_size() ||
          !heap->QueryProtect(probe, &protect) || (protect & required) != required) {
        return false;
      }
    }
    return true;
  }
  uint32_t U32(uint32_t address, bool &ok) const {
    if (!Readable(address, 4)) {
      ok = false;
      return 0;
    }
    uint8_t *base = base_;
    return REX_LOAD_U32(address);
  }
  uint32_t U16(uint32_t address, bool &ok) const {
    if (!Readable(address, 2)) {
      ok = false;
      return 0;
    }
    uint8_t *base = base_;
    return REX_LOAD_U16(address);
  }
  uint32_t U8(uint32_t address, bool &ok) const {
    if (!Readable(address, 1)) {
      ok = false;
      return 0;
    }
    uint8_t *base = base_;
    return REX_LOAD_U8(address);
  }
  float F32(uint32_t address, bool &ok) const {
    const uint32_t bits = U32(address, ok);
    float value = 0.0f;
    std::memcpy(&value, &bits, sizeof(value));
    return value;
  }
  // Up to 96 printable characters of a NUL-terminated guest string. Each
  // page the string touches is checked before it is read.
  std::string Text(uint32_t address) const {
    std::string text;
    uint8_t *base = base_;
    for (uint32_t i = 0; i < 96; ++i) {
      const uint32_t at = address + i;
      if ((i == 0 || at % kGuestPage == 0) && !Readable(at, 1)) {
        break;
      }
      const char c = static_cast<char>(REX_LOAD_U8(at));
      if (c == 0) {
        break;
      }
      text.push_back(c >= 32 && c < 127 && c != '"' && c != '\\' ? c : '?');
    }
    return text;
  }

 private:
  rex::memory::Memory *memory_;
  uint8_t *base_;
};

// Reader for code running inside guest hooks.
GuestReader HookReader(uint8_t *base) {
  auto *kernel = rex::system::kernel_state();
  return GuestReader(kernel ? kernel->memory() : nullptr, base);
}

// --- KI-035: warp camera (warper+296, a UITransition) ------------------------
// Reads the camera's vtable, the three getter slots of the state-20 wait and
// the fields those getters read (mcla_warp_probe.h); no guest code runs.
struct WarpCamera {
  uint32_t ptr = 0;
  bool read = false;  // vtable, slots and fields all read
  uint32_t vtable = 0;
  uint32_t fn_ready = 0;
  uint32_t fn_duration = 0;
  uint32_t fn_time = 0;
  mcla::warp::CameraFields fields;
  bool ready_known = false;
  bool ready = false;
  mcla::warp::Value duration;
  mcla::warp::Value time;
  bool list_read = false;  // the transition manager's active list was read
  uint32_t active_count = 0;
  int active_index = -1;   // index of the camera in that list, -1 = absent
};

WarpCamera ReadWarpCamera(const GuestReader &read, uint32_t camera) {
  WarpCamera cam;
  cam.ptr = camera;
  if (!camera) {
    return cam;
  }
  bool ok = true;
  cam.vtable = read.U32(camera, ok);
  if (ok && cam.vtable) {
    cam.fn_ready = read.U32(cam.vtable + mcla::warp::kSlotReady, ok);
    cam.fn_duration = read.U32(cam.vtable + mcla::warp::kSlotDuration, ok);
    cam.fn_time = read.U32(cam.vtable + mcla::warp::kSlotTime, ok);
  }
  cam.fields.flags16 = read.U32(camera + 16, ok);
  cam.fields.f76 = read.F32(camera + 76, ok);
  cam.fields.f80 = read.F32(camera + 80, ok);
  cam.fields.f96 = read.F32(camera + 96, ok);
  cam.fields.b471 = static_cast<uint8_t>(read.U8(camera + 471, ok));
  cam.read = ok && cam.vtable != 0;
  if (cam.read) {
    cam.ready = mcla::warp::Ready(mcla::warp::ReadyGetter(cam.fn_ready), cam.fields,
                                  cam.ready_known);
    cam.duration = mcla::warp::Duration(mcla::warp::DurationGetter(cam.fn_duration), cam.fields);
    cam.time = mcla::warp::Time(mcla::warp::TimeGetter(cam.fn_time), cam.fields);
  }
  // sub_82220980 adds a transition to *(*(UIManager)+52)+0x40850 (pointer
  // array, u16 count at +4); sub_822247A8 -> sub_82221DB0 updates that list
  // and drops finished entries (sub_8221E310 removes one).
  bool list_ok = true;
  if (const uint32_t manager = read.U32(kUiManagerPtr, list_ok)) {
    if (const uint32_t transitions = read.U32(manager + kUiTransitions, list_ok)) {
      const uint32_t list = read.U32(transitions + kActiveTransitions, list_ok);
      cam.active_count = read.U16(transitions + kActiveTransitions + 4, list_ok);
      const uint32_t scan = cam.active_count < kMaxActiveScan ? cam.active_count : kMaxActiveScan;
      for (uint32_t i = 0; list_ok && list && i < scan; ++i) {
        if (read.U32(list + 4 * i, list_ok) == camera) {
          cam.active_index = static_cast<int>(i);
          break;
        }
      }
      cam.list_read = list_ok && (list != 0 || cam.active_count == 0);
    }
  }
  return cam;
}

const char *Known(const mcla::warp::Value &value) {
  return value.known ? "known" : "unknown";
}

// --- probe: stripped debug printf -------------------------------------------
// sub_82130000 is a bare blr with about 2100 call sites (the title's stripped
// debug printf). Only exact formats of interest are logged; behavior is kept.
constexpr uint32_t kWarperTextFirst = 0x8201F980u;  // mcWarper messages
constexpr uint32_t kWarperTextLast = 0x8201FCC4u;
constexpr uint32_t kPauseRequestFormat = 0x82039E34u;  // r4; r5 = request name
constexpr uint32_t kGameCameraFormat = 0x820374F8u;    // r3; r4 = camera name

}  // namespace

REX_EXTERN(sub_82130000) {
  if (ProbeOn()) {
    static std::atomic<uint32_t> last_warper_format{0};
    static std::atomic<uint32_t> repeats{0};
    const uint32_t r3 = ctx.r3.u32;
    const uint32_t r4 = ctx.r4.u32;
    if (r3 >= kWarperTextFirst && r3 <= kWarperTextLast) {
      // Waits print every frame: log on change, then every 600th repeat.
      const uint32_t previous = last_warper_format.exchange(r3);
      uint32_t count = 0;
      if (previous == r3) {
        count = repeats.fetch_add(1) + 1;
      } else {
        repeats = 0;
      }
      if (previous != r3 || count % 600 == 0) {
        MCLA_APP_INFO("MCLA_KI026_WARPER_TEXT v=1 fmt={:08X} repeat={} lr={:08X} text=\"{}\"",
                      r3, count, static_cast<uint32_t>(ctx.lr), HookReader(base).Text(r3));
      }
    } else if (r4 == kPauseRequestFormat) {
      MCLA_APP_INFO("MCLA_KI026_PAUSE_REQUEST v=1 name=\"{}\" lr={:08X}",
                    HookReader(base).Text(ctx.r5.u32), static_cast<uint32_t>(ctx.lr));
    } else if (r3 == kGameCameraFormat) {
      MCLA_APP_INFO("MCLA_KI026_GAME_CAMERA v=1 name=\"{}\" lr={:08X}",
                    HookReader(base).Text(r4), static_cast<uint32_t>(ctx.lr));
    }
  }
  // The original is a bare blr; calling it keeps the guest breadcrumbs.
  __imp__sub_82130000(ctx, base);
}

// --- probe: transition edges (every stock call still runs) -----------------
// Register values only; no guest memory is read.
#define MCLA_KI026_EDGE(fn, label)                                              \
  REX_EXTERN(fn) {                                                              \
    if (ProbeOn()) {                                                            \
      MCLA_APP_INFO("MCLA_KI026_EDGE v=1 fn=" label                             \
                    " r3={:08X} r4={:08X} r5={:08X} f1={} lr={:08X}",           \
                    ctx.r3.u32, ctx.r4.u32, ctx.r5.u32, ctx.f1.f64,             \
                    static_cast<uint32_t>(ctx.lr));                             \
    }                                                                           \
    __imp__##fn(ctx, base);                                                     \
  }

MCLA_KI026_EDGE(sub_82204480, "CameraPauseAtCurrent")
MCLA_KI026_EDGE(sub_82204558, "CameraPauseAtMatrix")
MCLA_KI026_EDGE(sub_82204468, "CameraUnpause")
MCLA_KI026_EDGE(sub_822079C8, "Script_UILogic_PauseCamera")
MCLA_KI026_EDGE(sub_822079E8, "Script_UILogic_UnPauseCamera")
MCLA_KI026_EDGE(sub_82207A48, "Script_Warper_SetPauseCameraAtEndOfWarp")
MCLA_KI026_EDGE(sub_8221A440, "Warper_BeginWarp")
MCLA_KI026_EDGE(sub_8221B790, "Warper_Finish")
MCLA_KI026_EDGE(sub_8221BB90, "Warper_Abort")
MCLA_KI026_EDGE(sub_8221B8C8, "Warper_OnCameraFinished")
// All 17 direct callers of the camera-apply function (the v2 race-back probe
// covered six); lr identifies the call site, r4 the camera mode.
MCLA_KI026_EDGE(sub_822B0F10, "PlayerCamera_Apply")
MCLA_KI026_EDGE(sub_822B34B0, "PlayerCamera_Reset")
MCLA_KI026_EDGE(sub_822B33F0, "PlayerCamera_Restore")

#undef MCLA_KI026_EDGE

// --- warper update: state log + optional watchdog ----------------------------
namespace {

struct StuckTracker {
  uint32_t signature = 0;  // 0 none, 1 idle warper + paused camera, 2 cutscene wait
  std::chrono::steady_clock::time_point since{};
  bool fired = false;
};
StuckTracker tracker;  // the warper update runs on one guest thread

void Ki026Watchdog(PPCContext &ctx, uint8_t *base, uint32_t warper, uint32_t state) {
  const GuestReader read = HookReader(base);
  bool ok = true;
  const uint32_t ui_logic = warper - kWarperOffset;
  const bool camera_paused = read.U8(ui_logic + kCameraPaused, ok) != 0;
  const uint32_t pause_manager = read.U32(kPauseManagerPtr, ok);
  const bool simulation_paused = pause_manager && read.U8(pause_manager + 4, ok) &&
                                 !read.U8(pause_manager + 10, ok);
  const bool pause_override = read.U8(kPauseOverrideByte, ok) != 0;

  bool cutscene_playing = false;
  float cutscene_time = -1.0f;
  if (const uint32_t cutscene = read.U32(kCutscenePtr, ok)) {
    if (const uint32_t inner = read.U32(cutscene + 8, ok)) {
      cutscene_time = read.F32(inner + 120, ok);
      cutscene_playing = read.U8(cutscene + 20, ok) == 0 &&
                         read.U32(cutscene + 12, ok) != 3 && cutscene_time >= 0.0f;
    }
  }

  bool racer_active = false;
  if (const uint32_t players = read.U32(kPlayerManagerPtr, ok)) {
    if (const uint32_t racer = read.U32(players + 8, ok)) {
      racer_active = read.U32(racer + kRacerState, ok) == 3 &&
                     read.U8(racer + kRacerCameraInputOff, ok) == 0;
    }
  }

  // The stock code releases the pause through *(UIManager) + 48 (0x8221C940).
  uint32_t pause_owner = 0;
  if (const uint32_t manager = read.U32(kUiManagerPtr, ok)) {
    pause_owner = read.U32(manager + 48, ok);
  }

  uint32_t signature = 0;
  if (ok && pause_owner && camera_paused && !simulation_paused && !pause_override &&
      !cutscene_playing && racer_active) {
    if (state == kWarperIdle && read.U32(warper + kWarperQueueCount, ok) == 0 &&
        read.U32(warper + kWarperRequest, ok) == 0) {
      signature = 1;  // warp finished, camera left paused (script never unpaused)
    } else if ((state == 12 || state == 17) && read.U8(warper + kWarperCutsceneWait, ok) &&
               !(cutscene_time >= 0.0f)) {
      signature = 2;  // warp-to-cutscene waits for a cutscene that never starts
    }
    if (!ok) {
      signature = 0;  // never act on a partially read state
    }
  }

  const auto now = std::chrono::steady_clock::now();
  if (signature != tracker.signature) {
    tracker = {signature, now, false};
    return;
  }
  if (!signature || tracker.fired) {
    return;
  }
  const auto limit = signature == 1 ? std::chrono::seconds(20) : std::chrono::seconds(15);
  if (now - tracker.since < limit) {
    return;
  }
  tracker.fired = true;
  MCLA_APP_INFO("MCLA_KI026_WATCHDOG v=1 signature={} warper_state={} cutscene_time={} "
                "pause_owner={:08X} action=release-camera-pause",
                signature, state, cutscene_time, pause_owner);
  if (signature == 2) {
    // Same exit as the stock success branch at 0x8221C8C8 except that the
    // Career_Loading movie calls are skipped: clear the wait and finish.
    REX_STORE_U8(warper + kWarperCutsceneWait, 0);
    ctx.r3.u64 = warper;
    sub_8221B790(ctx, base);
  }
  ctx.r3.u64 = pause_owner;
  sub_82204468(ctx, base);
}

}  // namespace

// --- KI-035: warp state log + state-20 watchdog --------------------------------
namespace {

struct WarpTracker {
  mcla::warp::Watchdog watchdog;
  mcla::warp::WaitPacer pacer;
  uint32_t state_lines = 0;
};
WarpTracker warp_tracker;  // the warper update runs on one guest thread

uint64_t SteadyMs() {
  return static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(
                                   std::chrono::steady_clock::now().time_since_epoch())
                                   .count());
}

void WarpObserve(uint8_t *base, uint32_t warper, bool before_ok, uint32_t state_before,
                 uint32_t state) {
  const GuestReader read = HookReader(base);
  bool ok = true;
  const uint32_t request = read.U32(warper + kWarperRequest, ok);
  const uint32_t camera_ptr = read.U32(warper + kWarperCamera, ok);
  const uint32_t request_type = request ? read.U32(request + 12, ok) : 0;
  const uint32_t req0 = request ? read.U8(request + 0, ok) : 0;
  const uint32_t req3 = request ? read.U8(request + 3, ok) : 0;
  const uint32_t req7 = request ? read.U8(request + 7, ok) : 0;
  const uint32_t req9 = request ? read.U8(request + 9, ok) : 0;
  const uint32_t exit_flag = read.U8(warper + kWarperExitFlag, ok);
  bool paused = false;
  if (const uint32_t pm = read.U32(kPauseManagerPtr, ok)) {
    paused = read.U8(pm + 4, ok) != 0 && read.U8(pm + 10, ok) == 0;
  }
  const WarpCamera cam = ReadWarpCamera(read, camera_ptr);
  const uint64_t now_ms = SteadyMs();
  const float time = cam.time.known ? cam.time.value : -1.0f;
  const float duration = cam.duration.known ? cam.duration.value : -1.0f;
  const int ready = cam.ready_known ? (cam.ready ? 1 : 0) : -1;

  if (WarpProbeOn()) {
    if (before_ok && state != state_before &&
        warp_tracker.state_lines < mcla::warp::kMaxStateLines) {
      ++warp_tracker.state_lines;
      MCLA_APP_INFO("MCLA_KI035_WARP_STATE v=1 from={} to={} request={:08X} type={} "
                    "req0={} req3={} req7={} req9={} exit276={} camera={:08X} class={} "
                    "ready={} time={} ({}) duration={} ({}) active_index={} active_count={} "
                    "paused={} complete={}",
                    state_before, state, request, request_type, req0, req3, req7, req9,
                    exit_flag, camera_ptr, mcla::warp::TransitionClassName(cam.vtable), ready,
                    time, Known(cam.time), duration, Known(cam.duration), cam.active_index,
                    cam.active_count, paused ? 1 : 0, ok && cam.read ? 1 : 0);
    }
    if (warp_tracker.pacer.Observe(now_ms, state == mcla::warp::kStateCinematicZoom)) {
      MCLA_APP_INFO("MCLA_KI035_WARP_WAIT v=1 state={} request={:08X} type={} req0={} req3={} "
                    "camera={:08X} class={} ready={} time={} duration={} flags16={:08X} "
                    "active_index={} active_count={} paused={} complete={}",
                    state, request, request_type, req0, req3, camera_ptr,
                    mcla::warp::TransitionClassName(cam.vtable), ready, time, duration,
                    cam.fields.flags16, cam.active_index, cam.active_count, paused ? 1 : 0,
                    ok && cam.read ? 1 : 0);
    }
  }

  const uint64_t limit_ms = WarpWatchdogLimitMs();
  if (!limit_ms) {
    return;
  }
  mcla::warp::Sample sample;
  sample.now_ms = now_ms;
  sample.complete = ok && cam.read;
  sample.state = state;
  sample.request = request;
  sample.camera = camera_ptr;
  sample.time_known = cam.time.known;
  sample.time = cam.time.value;
  sample.paused = paused;
  const uint64_t frozen_before = warp_tracker.watchdog.frozen_since_ms();
  if (warp_tracker.watchdog.Observe(sample, limit_ms) != mcla::warp::Verdict::kFire) {
    return;
  }
  const uint64_t frozen_ms = now_ms - frozen_before;
  // The stock exit of state 20 (0x8221C1C4..0x8221C1E0): *(0x8286D8F0)+293 = 0,
  // warper+276 = 1, warper+288 = 29. Every address is checked first.
  bool exit_ok = true;
  const uint32_t global = read.U32(kExitGlobalPtr, exit_ok);
  const bool writable = exit_ok && global && read.Writable(global + kExitGlobalByte, 1) &&
                        read.Writable(warper + kWarperExitFlag, 1) &&
                        read.Writable(warper + kWarperState, 4) &&
                        read.U32(warper + kWarperState, exit_ok) ==
                            mcla::warp::kStateCinematicZoom &&
                        exit_ok;
  REXLOG_CAT_WARN(::mcla::logging::App(),
                  "MCLA_KI035_WARP_WATCHDOG v=1 action={} limit_s={} frozen_ms={} "
                  "request={:08X} type={} req0={} req3={} camera={:08X} class={} ready={} "
                  "time={} duration={} flags16={:08X} active_index={} active_count={} "
                  "global={:08X}",
                  writable ? "finish-state20" : "refused-unwritable", limit_ms / 1000,
                  frozen_ms, request, request_type, req0, req3, camera_ptr,
                  mcla::warp::TransitionClassName(cam.vtable), ready, time, duration,
                  cam.fields.flags16, cam.active_index, cam.active_count, global);
  if (!writable) {
    return;
  }
  REX_STORE_U8(global + kExitGlobalByte, 0);
  REX_STORE_U8(warper + kWarperExitFlag, 1);
  REX_STORE_U32(warper + kWarperState, kWarperIdle);
  warp_watchdog_fires.fetch_add(1);
}

}  // namespace

REX_EXTERN(sub_8221BE98) {
  const uint32_t warper = ctx.r3.u32;
  const bool warp_on = WarpProbeOn() || WarpWatchdogLimitMs() != 0;
  if (!ProbeOn() && !WatchdogOn() && !warp_on) {
    __imp__sub_8221BE98(ctx, base);
    return;
  }
  bool before_ok = true;
  const uint32_t state_before = HookReader(base).U32(warper + kWarperState, before_ok);
  __imp__sub_8221BE98(ctx, base);
  const GuestReader read = HookReader(base);
  bool ok = true;
  const uint32_t state = read.U32(warper + kWarperState, ok);
  if (!ok) {
    return;
  }
  if (ProbeOn() && before_ok && state != state_before) {
    const uint32_t request = read.U32(warper + kWarperRequest, ok);
    const uint32_t ui_logic = warper - kWarperOffset;
    uint32_t flags[9] = {};
    for (uint32_t i = 0; i < 9; ++i) {
      flags[i] = read.U8(warper + kWarperFlagsFirst + i, ok);
    }
    const uint32_t request_type = request ? read.U32(request + 12, ok) : 0;
    const uint32_t camera_paused = read.U8(ui_logic + kCameraPaused, ok);
    const uint32_t queue = read.U32(warper + kWarperQueueCount, ok);
    MCLA_APP_INFO("MCLA_KI026_WARPER_STATE v=1 from={} to={} request={:08X} type={} "
                  "flags272={:02X}{:02X}{:02X}{:02X}{:02X}{:02X}{:02X}{:02X}{:02X} "
                  "camera_paused={} queue={} complete={}",
                  state_before, state, request, request_type, flags[0], flags[1], flags[2],
                  flags[3], flags[4], flags[5], flags[6], flags[7], flags[8], camera_paused,
                  queue, ok ? 1 : 0);
  }
  if (WatchdogOn()) {
    Ki026Watchdog(ctx, base, warper, state);
  }
  if (warp_on) {
    WarpObserve(base, warper, before_ok, state_before, state);
  }
}

// --- F10 state block ----------------------------------------------------------
namespace {

std::string Hex(uint32_t value) {
  char text[16];
  std::snprintf(text, sizeof(text), "\"%08X\"", value);
  return text;
}

// JSON has no NaN or infinity; guest floats can hold either.
std::string Float(float value) {
  if (!std::isfinite(value)) {
    return "null";
  }
  char text[32];
  std::snprintf(text, sizeof(text), "%.9g", static_cast<double>(value));
  return text;
}

std::string WarpCameraJson(const WarpCamera &cam) {
  std::ostringstream out;
  out << "{\"ptr\": " << Hex(cam.ptr) << ", \"read\": " << (cam.read ? "true" : "false")
      << ", \"vtable\": " << Hex(cam.vtable) << ", \"class\": \""
      << mcla::warp::TransitionClassName(cam.vtable) << "\""
      << ", \"fn_ready92\": " << Hex(cam.fn_ready)
      << ", \"fn_duration592\": " << Hex(cam.fn_duration)
      << ", \"fn_time608\": " << Hex(cam.fn_time) << ", \"getters\": [\""
      << mcla::warp::GetterName(mcla::warp::ReadyGetter(cam.fn_ready)) << "\", \""
      << mcla::warp::GetterName(mcla::warp::DurationGetter(cam.fn_duration)) << "\", \""
      << mcla::warp::GetterName(mcla::warp::TimeGetter(cam.fn_time)) << "\"]"
      << ", \"flags16\": " << Hex(cam.fields.flags16)
      << ", \"f76\": " << Float(cam.fields.f76) << ", \"f80\": " << Float(cam.fields.f80)
      << ", \"f96\": " << Float(cam.fields.f96)
      << ", \"b471\": " << static_cast<uint32_t>(cam.fields.b471)
      << ", \"ready\": " << (cam.ready_known ? (cam.ready ? "1" : "0") : "null")
      << ", \"duration\": " << (cam.duration.known ? Float(cam.duration.value) : "null")
      << ", \"time\": " << (cam.time.known ? Float(cam.time.value) : "null")
      << ", \"exit_due\": "
      << (cam.time.known && cam.duration.known
              ? (cam.time.value + mcla::warp::kExitEpsilon < cam.duration.value ? "0" : "1")
              : "null")
      << ", \"active_list_read\": " << (cam.list_read ? "true" : "false")
      << ", \"active_count\": " << cam.active_count
      << ", \"active_index\": " << cam.active_index << "}";
  return out.str();
}

}  // namespace

std::string MclaKi026StateJson(rex::memory::Memory *memory, uint8_t *base) {
  if (!memory || !base) {
    return {};
  }
  GuestReader read(memory, base);
  bool ok = true;
  std::ostringstream out;
  const uint32_t ui = read.U32(kUiLogicPtr, ok);
  const uint32_t manager = read.U32(kUiManagerPtr, ok);
  const uint32_t ui_via_manager = manager ? read.U32(manager + 48, ok) : 0;
  out << "{\"ui_logic\": " << Hex(ui) << ", \"ui_logic_via_manager\": " << Hex(ui_via_manager)
      << ", \"pause_override\": " << read.U8(kPauseOverrideByte, ok);
  if (ui) {
    const uint32_t w = ui + kWarperOffset;
    const uint32_t request = read.U32(w + kWarperRequest, ok);
    out << ", \"gps_jumping\": " << read.U8(ui + kGpsJumping, ok)
        << ", \"camera_paused\": " << read.U8(ui + kCameraPaused, ok)
        << ", \"camera_pause_requested\": " << read.U8(ui + kCameraPauseRequested, ok)
        << ", \"camera_pause_id\": " << Hex(read.U32(ui + kCameraPauseId, ok))
        << ", \"warper\": {\"state\": " << read.U32(w + kWarperState, ok)
        << ", \"queue\": " << read.U32(w + kWarperQueueCount, ok)
        << ", \"request\": " << Hex(request)
        << ", \"request_type\": " << (request ? read.U32(request + 12, ok) : 0)
        << ", \"request_flags\": [";
    for (uint32_t i = 0; i <= 10; ++i) {
      out << (i ? "," : "") << (request ? read.U8(request + i, ok) : 0);
    }
    out << "], \"flags272\": [";
    for (uint32_t i = 0; i <= 8; ++i) {
      out << (i ? "," : "") << read.U8(w + kWarperFlagsFirst + i, ok);
    }
    out << "], \"prestream_frames\": " << read.U32(w + kWarperPrestreamFrames, ok)
        << ", \"prestream_timer\": " << Float(read.F32(w + kWarperPrestreamTimer, ok))
        << ", \"camera\": " << Hex(read.U32(w + kWarperCamera, ok));
    // KI-035: the camera behind the state-20 wait. Its reads have their own
    // "read" flag and do not change "complete".
    const uint32_t camera = read.U32(w + kWarperCamera, ok);
    out << ", \"camera300\": " << Hex(read.U32(w + kWarperCameraCopy, ok))
        << ", \"request_camera108\": "
        << Hex(request ? read.U32(request + kRequestCamera, ok) : 0)
        << ", \"warp_camera\": " << WarpCameraJson(ReadWarpCamera(read, camera)) << "}";
    out << ", \"warp_probe\": " << (WarpProbeOn() ? "true" : "false")
        << ", \"warp_watchdog_limit_s\": " << WarpWatchdogLimitMs() / 1000
        << ", \"warp_watchdog_fires\": " << warp_watchdog_fires.load();
  }
  if (const uint32_t pm = read.U32(kPauseManagerPtr, ok)) {
    out << ", \"pause_manager\": {\"paused\": " << read.U8(pm + 4, ok)
        << ", \"paused_locally\": " << read.U8(pm + 6, ok)
        << ", \"fast_forward\": " << read.U8(pm + 8, ok)
        << ", \"frame_advance\": " << read.U8(pm + 10, ok)
        << ", \"queue_count\": " << read.U32(pm + 68, ok) << ", \"queue\": [";
    for (uint32_t i = 0; i < 10; ++i) {
      out << (i ? "," : "") << read.U32(pm + 20 + 4 * i, ok);
    }
    out << "]}";
  }
  out << ", \"timer_scale\": " << Float(read.F32(kMainTimer + 84, ok))
      << ", \"timer_step\": " << Float(read.F32(kMainTimer + 92, ok))
      << ", \"game_time_scale\": " << Float(read.F32(kGameTimeScale, ok));
  if (const uint32_t cutscene = read.U32(kCutscenePtr, ok)) {
    const uint32_t inner = read.U32(cutscene + 8, ok);
    out << ", \"cutscene\": {\"state12\": " << read.U32(cutscene + 12, ok)
        << ", \"flag20\": " << read.U8(cutscene + 20, ok)
        << ", \"time\": " << (inner ? Float(read.F32(inner + 120, ok)) : std::string("-1"))
        << "}";
  }
  if (const uint32_t players = read.U32(kPlayerManagerPtr, ok)) {
    const uint32_t racer = read.U32(players + 8, ok);
    if (racer) {
      const uint32_t camera = read.U32(racer + kRacerCamera, ok);
      out << ", \"racer\": {\"ptr\": " << Hex(racer)
          << ", \"state656\": " << read.U32(racer + kRacerState, ok)
          << ", \"camera_input_off908\": " << read.U8(racer + kRacerCameraInputOff, ok)
          << ", \"pad904\": " << Hex(read.U32(racer + 904, ok)) << "}";
      if (camera) {
        const uint32_t view = read.U32(camera + 16, ok);
        const uint32_t current = view ? read.U32(view + 48, ok) : 0;
        int slot = -1;  // which mcPlayerCamera member is on screen
        for (uint32_t offset = 44; offset <= 96; offset += 4) {
          if (current && read.U32(camera + offset, ok) == current) {
            slot = static_cast<int>(offset);
            break;
          }
        }
        out << ", \"player_camera\": {\"index80\": " << read.U32(camera + 80, ok)
            << ", \"spin100\": " << read.U8(camera + 100, ok)
            << ", \"view_current\": " << Hex(current)
            << ", \"view_current_vtable\": " << Hex(current ? read.U32(current, ok) : 0)
            << ", \"view_current_slot\": " << slot
            << ", \"view_transition\": " << Hex(view ? read.U32(view + 56, ok) : 0) << "}";
      }
    }
  }
  out << ", \"complete\": " << (ok ? "true" : "false") << "}";
  return out.str();
}
