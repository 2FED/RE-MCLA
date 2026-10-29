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

#include "mcla_ki026_probe.h"

#include "mcla_init.h"
#include "mcla_logging.h"

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

// Checked guest reads. A read is allowed only when the address lies inside a
// guest heap and the page protection includes read access. Spans are at most
// one guest page, so checking both ends covers every page they touch. A
// failed read returns 0 and clears `ok`. The check and the read are not
// atomic: a page decommitted in between is a residual (diagnostic-only) risk.
class GuestReader {
 public:
  GuestReader(rex::memory::Memory *memory, uint8_t *base) : memory_(memory), base_(base) {}

  bool Readable(uint32_t address, uint32_t size) const {
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
          !heap->QueryProtect(probe, &protect) ||
          !(protect & rex::memory::kMemoryProtectRead)) {
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

REX_EXTERN(sub_8221BE98) {
  const uint32_t warper = ctx.r3.u32;
  if (!ProbeOn() && !WatchdogOn()) {
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
        << ", \"camera\": " << Hex(read.U32(w + kWarperCamera, ok)) << "}";
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
