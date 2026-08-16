// mcla - project-owned ReXApp lifecycle

#include "mcla_app.h"

#include <rex/audio/audio_route_audit.h>
#include <rex/chrono/clock.h>
#include <rex/cvar.h>
#include <rex/filesystem/file.h>
#include <rex/filesystem/vfs.h>
#include <rex/input/input_driver.h>
#include <rex/input/input_system.h>
#include <rex/input/sdl/controller_matrix_audit.h>
#include <rex/input/sdl/input_slot_audit.h>
#include <rex/kernel/xam/locale_audit.h>
#include <rex/kernel/xam/offline_service_audit.h>
#include <rex/kernel/xam/profile_audit.h>
#include <rex/kernel/xam/xmp_audit.h>
#include <rex/kernel/xboxkrnl/rtl.h>
#include <rex/logging.h>
#include <rex/memory/mapped_memory.h>
#include <rex/runtime.h>
#include <rex/system/crash_report.h>
#include <rex/system/function_dispatcher.h>
#include <rex/system/kernel_state.h>
#include <rex/system/thread_state.h>
#include <rex/system/user_module.h>
#include <rex/ui/presenter.h>

#include <algorithm>
#include <array>
#include <bit>
#include <chrono>
#include <cmath>
#include <condition_variable>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <limits>
#include <thread>
#include <utility>
#include <vector>

#include "generated/default/mcla_init.h"
#include "mcla_logging.h"

namespace {

constexpr uint32_t kExpectedImageBase = 0x82000000;
constexpr uint32_t kExpectedImageSize = 0x009E0000;
constexpr uint32_t kExpectedCodeBase = 0x82130000;
constexpr uint32_t kExpectedCodeSize = 0x0069D054;
constexpr uint32_t kExpectedEntryPoint = 0x821322B8;
constexpr uint32_t kPhysicsTimerAddress = 0x821BDA90;
constexpr uint32_t kExpectedTitleId = 0x545407F8;
constexpr uint32_t kExpectedMediaId = 0x5940C9DB;
constexpr rex::X_STATUS kAccessDenied = 0xC0000022u;

struct PhysicsTimerRecord {
  uint32_t effective_bits;
  uint32_t clamped_bits;
  uint32_t raw_bits;
};

struct PhysicsTimerSnapshot {
  uint64_t calls = 0;
  uint64_t invalid_values = 0;
  uint32_t effective_us_min = UINT32_MAX;
  uint32_t effective_us_max = 0;
  uint32_t clamped_us_min = UINT32_MAX;
  uint32_t clamped_us_max = 0;
  uint32_t raw_us_min = UINT32_MAX;
  uint32_t raw_us_max = 0;
  std::array<PhysicsTimerRecord, 16> records{};
  size_t record_count = 0;
};

std::mutex physics_timer_mutex;
PPCFunc *physics_timer_original = nullptr;
bool physics_timer_sampling = false;
PhysicsTimerSnapshot physics_timer_snapshot;

bool PhysicsTimerBitsToMicros(uint32_t bits, uint32_t &micros_out) {
  const float seconds = std::bit_cast<float>(bits);
  if (!std::isfinite(seconds) || seconds < 0.0f || seconds > 1.0f) {
    return false;
  }
  micros_out = static_cast<uint32_t>(std::llround(double(seconds) * 1000000.0));
  return true;
}

void BeginPhysicsTimerSampling() {
  std::lock_guard lock(physics_timer_mutex);
  physics_timer_snapshot = {};
  physics_timer_sampling = true;
}

PhysicsTimerSnapshot EndPhysicsTimerSampling() {
  std::lock_guard lock(physics_timer_mutex);
  physics_timer_sampling = false;
  return physics_timer_snapshot;
}

void PhysicsTimerProbe(PPCContext &ctx, uint8_t *base) {
  const uint32_t timer = ctx.r3.u32;
  physics_timer_original(ctx, base);
  std::lock_guard lock(physics_timer_mutex);
  if (!physics_timer_sampling) {
    return;
  }
  const PhysicsTimerRecord record = {
      REX_LOAD_U32(timer + 8),
      REX_LOAD_U32(timer + 88),
      REX_LOAD_U32(timer + 92),
  };
  ++physics_timer_snapshot.calls;
  if (physics_timer_snapshot.record_count <
      physics_timer_snapshot.records.size()) {
    physics_timer_snapshot.records[physics_timer_snapshot.record_count++] =
        record;
  }
  uint32_t effective_us = 0;
  uint32_t clamped_us = 0;
  uint32_t raw_us = 0;
  if (!PhysicsTimerBitsToMicros(record.effective_bits, effective_us) ||
      !PhysicsTimerBitsToMicros(record.clamped_bits, clamped_us) ||
      !PhysicsTimerBitsToMicros(record.raw_bits, raw_us)) {
    ++physics_timer_snapshot.invalid_values;
    return;
  }
  physics_timer_snapshot.effective_us_min =
      std::min(physics_timer_snapshot.effective_us_min, effective_us);
  physics_timer_snapshot.effective_us_max =
      std::max(physics_timer_snapshot.effective_us_max, effective_us);
  physics_timer_snapshot.clamped_us_min =
      std::min(physics_timer_snapshot.clamped_us_min, clamped_us);
  physics_timer_snapshot.clamped_us_max =
      std::max(physics_timer_snapshot.clamped_us_max, clamped_us);
  physics_timer_snapshot.raw_us_min =
      std::min(physics_timer_snapshot.raw_us_min, raw_us);
  physics_timer_snapshot.raw_us_max =
      std::max(physics_timer_snapshot.raw_us_max, raw_us);
}

class FrontendSmokeInputDriver final : public rex::input::InputDriver {
public:
  FrontendSmokeInputDriver() : InputDriver(nullptr, 0) {}

  rex::X_STATUS Setup() override {
    MCLA_INPUT_INFO("MCLA_FRONTEND_SMOKE_CONFIG v=1 slot=0 hold_ms=200 "
                    "gameplay_wait_seconds=30 intertab_wait_seconds=2");
    return rex::X_STATUS{0};
  }
  rex::X_RESULT
  GetCapabilities(uint32_t user_index, uint32_t flags,
                  rex::input::X_INPUT_CAPABILITIES *out_caps) override {
    (void)flags;
    if (user_index != 0) {
      return rex::X_RESULT{0x0000048F};
    }
    if (out_caps) {
      *out_caps = {};
      out_caps->type = rex::input::XINPUT_DEVTYPE_GAMEPAD;
      out_caps->sub_type = 1;
      out_caps->gamepad.buttons = 0xF3FF;
    }
    return rex::X_RESULT{0};
  }

  rex::X_RESULT GetState(uint32_t user_index,
                         rex::input::X_INPUT_STATE *out_state) override {
    if (user_index != 0) {
      return rex::X_RESULT{0x0000048F};
    }
    std::lock_guard lock(mutex_);
    if (out_state) {
      *out_state = state_;
      if (armed_sequence_ != 0 && !observed_) {
        observed_ = true;
        if (gameplay_audit_) {
          MCLA_INPUT_INFO(
              "MCLA_GAMEPLAY_INPUT v=1 side=guest sequence={} "
              "buttons={:04X} lt={} rt={} lx={} ly={} rx={} ry={}",
              armed_sequence_, static_cast<uint16_t>(state_.gamepad.buttons),
              state_.gamepad.left_trigger, state_.gamepad.right_trigger,
              static_cast<int16_t>(state_.gamepad.thumb_lx),
              static_cast<int16_t>(state_.gamepad.thumb_ly),
              static_cast<int16_t>(state_.gamepad.thumb_rx),
              static_cast<int16_t>(state_.gamepad.thumb_ry));
        } else if (render_audit_) {
          MCLA_INPUT_INFO(
              "MCLA_RENDER_SMOKE_INPUT v=1 side=guest sequence={} "
              "buttons={:04X} lt={} rt={} lx={} ly={} rx={} ry={}",
              armed_sequence_, static_cast<uint16_t>(state_.gamepad.buttons),
              state_.gamepad.left_trigger, state_.gamepad.right_trigger,
              static_cast<int16_t>(state_.gamepad.thumb_lx),
              static_cast<int16_t>(state_.gamepad.thumb_ly),
              static_cast<int16_t>(state_.gamepad.thumb_rx),
              static_cast<int16_t>(state_.gamepad.thumb_ry));
        } else {
          MCLA_INPUT_INFO("MCLA_FRONTEND_SMOKE_INPUT v=1 side=guest "
                          "sequence={} buttons={:04X}",
                          armed_sequence_,
                          static_cast<uint16_t>(state_.gamepad.buttons));
        }
        condition_.notify_all();
      }
    }
    return rex::X_RESULT{0};
  }

  rex::X_RESULT SetState(uint32_t user_index,
                         rex::input::X_INPUT_VIBRATION *vibration) override {
    (void)vibration;
    return user_index == 0 ? rex::X_RESULT{0} : rex::X_RESULT{0x0000048F};
  }
  rex::X_RESULT
  GetKeystroke(uint32_t user_index, uint32_t flags,
               rex::input::X_INPUT_KEYSTROKE *out_keystroke) override {
    (void)flags;
    (void)out_keystroke;
    return user_index == 0 ? rex::X_RESULT{0x000010D2}
                           : rex::X_RESULT{0x0000048F};
  }

  bool Pulse(std::stop_token stop_token, uint16_t buttons, uint32_t sequence) {
    {
      std::lock_guard lock(mutex_);
      state_ = {};
      state_.packet_number = ++packet_;
      state_.gamepad.buttons = buttons;
      armed_sequence_ = sequence;
      render_audit_ = false;
      gameplay_audit_ = false;
      observed_ = false;
      MCLA_INPUT_INFO("MCLA_FRONTEND_SMOKE_INPUT v=1 side=source sequence={} "
                      "buttons={:04X}",
                      sequence, buttons);
    }
    std::unique_lock lock(mutex_);
    const bool observed = condition_.wait_for(
        lock, std::chrono::seconds(5), [this, stop_token]() {
          return observed_ || stop_token.stop_requested();
        });
    if (!observed || stop_token.stop_requested()) {
      return false;
    }
    lock.unlock();
    const auto hold_deadline =
        std::chrono::steady_clock::now() + std::chrono::milliseconds(200);
    while (!stop_token.stop_requested() &&
           std::chrono::steady_clock::now() < hold_deadline) {
      std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    if (stop_token.stop_requested()) {
      return false;
    }
    lock.lock();
    state_ = {};
    state_.packet_number = ++packet_;
    armed_sequence_ = sequence;
    observed_ = false;
    MCLA_INPUT_INFO(
        "MCLA_FRONTEND_SMOKE_INPUT v=1 side=source sequence={} buttons=0000",
        sequence);
    return condition_.wait_for(
        lock, std::chrono::seconds(5), [this, stop_token]() {
          return observed_ || stop_token.stop_requested();
        });
  }

  bool SetRenderGamepad(std::stop_token stop_token,
                        const rex::input::X_INPUT_GAMEPAD &gamepad,
                        uint32_t sequence) {
    {
      std::lock_guard lock(mutex_);
      state_ = {};
      state_.packet_number = ++packet_;
      state_.gamepad = gamepad;
      armed_sequence_ = sequence;
      render_audit_ = true;
      gameplay_audit_ = false;
      observed_ = false;
      MCLA_INPUT_INFO(
          "MCLA_RENDER_SMOKE_INPUT v=1 side=source sequence={} buttons={:04X} "
          "lt={} rt={} lx={} ly={} rx={} ry={}",
          sequence, static_cast<uint16_t>(gamepad.buttons),
          gamepad.left_trigger, gamepad.right_trigger,
          static_cast<int16_t>(gamepad.thumb_lx),
          static_cast<int16_t>(gamepad.thumb_ly),
          static_cast<int16_t>(gamepad.thumb_rx),
          static_cast<int16_t>(gamepad.thumb_ry));
    }
    std::unique_lock lock(mutex_);
    return condition_.wait_for(
        lock, std::chrono::seconds(5), [this, stop_token]() {
          return observed_ || stop_token.stop_requested();
        });
  }

  bool ReleaseRenderGamepad(std::stop_token stop_token, uint32_t sequence) {
    rex::input::X_INPUT_GAMEPAD gamepad{};
    return SetRenderGamepad(stop_token, gamepad, sequence);
  }

  bool SetGameplayGamepad(std::stop_token stop_token,
                          const rex::input::X_INPUT_GAMEPAD &gamepad,
                          uint32_t sequence) {
    {
      std::lock_guard lock(mutex_);
      state_ = {};
      state_.packet_number = ++packet_;
      state_.gamepad = gamepad;
      armed_sequence_ = sequence;
      render_audit_ = false;
      gameplay_audit_ = true;
      observed_ = false;
      MCLA_INPUT_INFO(
          "MCLA_GAMEPLAY_INPUT v=1 side=source sequence={} buttons={:04X} "
          "lt={} rt={} lx={} ly={} rx={} ry={}",
          sequence, static_cast<uint16_t>(gamepad.buttons),
          gamepad.left_trigger, gamepad.right_trigger,
          static_cast<int16_t>(gamepad.thumb_lx),
          static_cast<int16_t>(gamepad.thumb_ly),
          static_cast<int16_t>(gamepad.thumb_rx),
          static_cast<int16_t>(gamepad.thumb_ry));
    }
    std::unique_lock lock(mutex_);
    return condition_.wait_for(
        lock, std::chrono::seconds(5), [this, stop_token]() {
          return observed_ || stop_token.stop_requested();
        });
  }

  bool ReleaseGameplayGamepad(std::stop_token stop_token, uint32_t sequence) {
    rex::input::X_INPUT_GAMEPAD gamepad{};
    return SetGameplayGamepad(stop_token, gamepad, sequence);
  }

private:
  std::mutex mutex_;
  std::condition_variable condition_;
  rex::input::X_INPUT_STATE state_{};
  uint32_t packet_ = 0;
  uint32_t armed_sequence_ = 0;
  bool render_audit_ = false;
  bool gameplay_audit_ = false;
  bool observed_ = false;
};

} // namespace

REXCVAR_DEFINE_BOOL(
    mcla_lifecycle_probe, false, "MCLA",
    "Exercise the host lifecycle without constructing the guest runtime")
    .lifecycle(rex::cvar::Lifecycle::kInitOnly);

REXCVAR_DEFINE_BOOL(
    mcla_module_config_probe, false, "MCLA",
    "Validate the loaded image and dispatch table without launching guest code")
    .lifecycle(rex::cvar::Lifecycle::kInitOnly);

REXCVAR_DEFINE_BOOL(mcla_vfs_probe, false, "MCLA",
                    "Validate the guest disc mount and write containment "
                    "without launching guest code")
    .lifecycle(rex::cvar::Lifecycle::kInitOnly);

REXCVAR_DEFINE_BOOL(
    mcla_crash_probe, false, "MCLA",
    "Write a synthetic privacy-safe guest crash report without guest execution")
    .lifecycle(rex::cvar::Lifecycle::kInitOnly);

REXCVAR_DEFINE_BOOL(mcla_logging_probe, false, "MCLA",
                    "Emit one schema marker for every MCLA-R logging category")
    .lifecycle(rex::cvar::Lifecycle::kInitOnly);

REXCVAR_DEFINE_BOOL(
    mcla_first_frame_probe, false, "MCLA",
    "Capture the first nontrivial guest frame after presentation starts")
    .lifecycle(rex::cvar::Lifecycle::kInitOnly);

REXCVAR_DEFINE_BOOL(
    mcla_controller_matrix_probe, false, "MCLA",
    "Run the opt-in host controller rumble diagnostic after title capture")
    .lifecycle(rex::cvar::Lifecycle::kInitOnly);

REXCVAR_DEFINE_BOOL(
    mcla_frontend_smoke_probe, false, "MCLA",
    "Inject a bounded deterministic slot-0 frontend navigation sequence")
    .lifecycle(rex::cvar::Lifecycle::kInitOnly);

REXCVAR_DEFINE_BOOL(
    mcla_rendering_smoke_probe, false, "MCLA",
    "Capture bounded saved-gameplay world, street, and particle frames")
    .lifecycle(rex::cvar::Lifecycle::kInitOnly);

REXCVAR_DEFINE_BOOL(
    mcla_gameplay_input_probe, false, "MCLA",
    "Exercise bounded saved-gameplay steering, pedals, and pause input")
    .lifecycle(rex::cvar::Lifecycle::kInitOnly);

REXCVAR_DEFINE_BOOL(
    mcla_physics_timing_probe, false, "MCLA",
    "Measure saved-gameplay guest clock and output cadence under throttle")
    .lifecycle(rex::cvar::Lifecycle::kInitOnly);

REXCVAR_DEFINE_BOOL(
    mcla_audio_event_probe, false, "MCLA",
    "Exercise bounded saved-gameplay audio event listening windows")
    .lifecycle(rex::cvar::Lifecycle::kInitOnly);

REXCVAR_DEFINE_BOOL(mcla_race_route_probe, false, "MCLA",
                    "Capture operator-confirmed first-race checkpoints from "
                    "fixed user-root requests")
    .lifecycle(rex::cvar::Lifecycle::kInitOnly);

REXCVAR_DEFINE_UINT32(
    mcla_frontend_gameplay_wait_seconds, 30, "MCLA",
    "Seconds to wait for the saved frontend route to enter gameplay")
    .range(30, 60)
    .lifecycle(rex::cvar::Lifecycle::kInitOnly);

REXCVAR_DEFINE_UINT32(
    mcla_frontend_pause_wait_seconds, 2, "MCLA",
    "Seconds to wait for the pause panel animation before capture")
    .range(2, 10)
    .lifecycle(rex::cvar::Lifecycle::kInitOnly);

REXCVAR_DEFINE_UINT32(
    mcla_first_frame_settle_seconds, 3, "MCLA",
    "Seconds to wait after the first guest output before capture")
    .range(1, 60)
    .lifecycle(rex::cvar::Lifecycle::kInitOnly);

REXCVAR_DEFINE_UINT32(
    mcla_audio_route_soak_seconds, 300, "MCLA",
    "Seconds to observe the frontend audio route after title capture")
    .range(60, 600)
    .lifecycle(rex::cvar::Lifecycle::kInitOnly);

#define MCLA_DEFINE_LOG_LEVEL_CVAR(name)                                       \
  REXCVAR_DEFINE_STRING(                                                       \
      mcla_log_##name, "inherit", "MCLA Logging",                              \
      "Category level override: inherit, trace, debug, info, warn, "           \
      "error, critical, off")                                                  \
      .allowed({"inherit", "trace", "debug", "info", "warn", "error",          \
                "critical", "off"})                                            \
      .lifecycle(rex::cvar::Lifecycle::kInitOnly)

MCLA_DEFINE_LOG_LEVEL_CVAR(app);
MCLA_DEFINE_LOG_LEVEL_CVAR(ppc);
MCLA_DEFINE_LOG_LEVEL_CVAR(kernel);
MCLA_DEFINE_LOG_LEVEL_CVAR(xam);
MCLA_DEFINE_LOG_LEVEL_CVAR(vfs);
MCLA_DEFINE_LOG_LEVEL_CVAR(gpu);
MCLA_DEFINE_LOG_LEVEL_CVAR(audio);
MCLA_DEFINE_LOG_LEVEL_CVAR(input);
MCLA_DEFINE_LOG_LEVEL_CVAR(patches);

#undef MCLA_DEFINE_LOG_LEVEL_CVAR

MclaApp::MclaApp(rex::ui::WindowedAppContext &ctx, rex::PPCImageInfo image_info)
    : ReXApp(ctx, "mcla", image_info) {}

std::unique_ptr<rex::ui::WindowedApp>
MclaApp::Create(rex::ui::WindowedAppContext &ctx) {
  return std::unique_ptr<MclaApp>(new MclaApp(ctx, PPCImageConfig));
}

void MclaApp::OnPostInitLogging() {
  const std::array overrides = {
      std::pair{mcla::logging::App(), REXCVAR_GET(mcla_log_app)},
      std::pair{mcla::logging::Ppc(), REXCVAR_GET(mcla_log_ppc)},
      std::pair{mcla::logging::Kernel(), REXCVAR_GET(mcla_log_kernel)},
      std::pair{mcla::logging::Xam(), REXCVAR_GET(mcla_log_xam)},
      std::pair{mcla::logging::Vfs(), REXCVAR_GET(mcla_log_vfs)},
      std::pair{mcla::logging::Gpu(), REXCVAR_GET(mcla_log_gpu)},
      std::pair{mcla::logging::Audio(), REXCVAR_GET(mcla_log_audio)},
      std::pair{mcla::logging::Input(), REXCVAR_GET(mcla_log_input)},
      std::pair{mcla::logging::Patches(), REXCVAR_GET(mcla_log_patches)},
  };
  for (const auto &[category, level_name] : overrides) {
    if (level_name != "inherit") {
      if (const auto level = rex::ParseLogLevel(level_name)) {
        rex::SetCategoryLevel(category, *level);
      }
    }
  }
  if (REXCVAR_GET(mcla_logging_probe)) {
    mcla::logging::EmitSchemaProbe();
  }
  MCLA_APP_INFO("MCLA lifecycle: logging ready");
}

void MclaApp::OnPreSetup(rex::RuntimeConfig &config) {
  REXCVAR_SET(rtl_allow_cross_thread_critical_section_leave, true);
  if (REXCVAR_GET(mcla_frontend_smoke_probe) ||
      REXCVAR_GET(mcla_rendering_smoke_probe) ||
      REXCVAR_GET(mcla_gameplay_input_probe) ||
      REXCVAR_GET(mcla_physics_timing_probe) ||
      REXCVAR_GET(mcla_audio_event_probe)) {
    config.input_factory = [this](bool tool_mode) {
      (void)tool_mode;
      auto input_system = std::make_unique<rex::input::InputSystem>(nullptr);
      auto driver = std::make_unique<FrontendSmokeInputDriver>();
      frontend_smoke_input_ = driver.get();
      if (driver->Setup() != rex::X_STATUS{0}) {
        frontend_smoke_input_ = nullptr;
        return std::unique_ptr<rex::system::IInputSystem>{};
      }
      input_system->AddDriver(std::move(driver));
      return std::unique_ptr<rex::system::IInputSystem>(
          std::move(input_system));
    };
  }
  const bool guest_free_probe =
      REXCVAR_GET(mcla_lifecycle_probe) ||
      REXCVAR_GET(mcla_module_config_probe) || REXCVAR_GET(mcla_vfs_probe) ||
      REXCVAR_GET(mcla_crash_probe) || REXCVAR_GET(mcla_logging_probe);
  if (config.gpu_plugin.empty() && !guest_free_probe) {
    config.gpu_plugin = "xenos";
    MCLA_GPU_INFO("MCLA graphics: selected GPU plugin 'xenos'");
  }
}

std::optional<rex::PathConfig>
MclaApp::OnFinalizePaths(const rex::PathConfig &defaults,
                         std::function<void(rex::PathConfig)> resume) {
  (void)resume;
  if (REXCVAR_GET(mcla_lifecycle_probe)) {
    MCLA_APP_INFO("MCLA lifecycle: probe requested; guest runtime skipped");
    app_context().CallInUIThreadDeferred([this]() {
      MCLA_APP_INFO("MCLA lifecycle: probe complete");
      app_context().QuitFromUIThread();
    });
    return std::nullopt;
  }

  if (!ValidateStaticImageContract()) {
    MCLA_PPC_ERROR("MCLA module config: static image contract rejected");
    app_context().CallInUIThreadDeferred(
        [this]() { app_context().QuitFromUIThread(); });
    return std::nullopt;
  }

  return defaults;
}

bool MclaApp::ValidateStaticImageContract() {
  if (PPCImageConfig.image_base != kExpectedImageBase ||
      PPCImageConfig.image_size != kExpectedImageSize ||
      PPCImageConfig.code_base != kExpectedCodeBase ||
      PPCImageConfig.code_size != kExpectedCodeSize ||
      !PPCImageConfig.func_mappings) {
    return false;
  }

  const uint64_t image_end =
      uint64_t(PPCImageConfig.image_base) + PPCImageConfig.image_size;
  const uint64_t code_end =
      uint64_t(PPCImageConfig.code_base) + PPCImageConfig.code_size;
  const uint64_t dispatch_end =
      image_end + (uint64_t(PPCImageConfig.code_size) +
                   rex::runtime::FunctionDispatcher::kThunkReserveSize) *
                      2;
  if (PPCImageConfig.image_size == 0 || PPCImageConfig.code_size == 0 ||
      PPCImageConfig.code_base < PPCImageConfig.image_base ||
      code_end > image_end || dispatch_end > (uint64_t{1} << 32)) {
    return false;
  }

  const size_t max_mappings = PPCImageConfig.code_size / sizeof(uint32_t) + 1;
  size_t mapping_count = 0;
  size_t entry_count = 0;
  uint32_t previous_guest = 0;
  bool found_sentinel = false;
  for (; mapping_count < max_mappings; ++mapping_count) {
    const auto &mapping = PPCImageConfig.func_mappings[mapping_count];
    if (mapping.guest == 0) {
      found_sentinel = mapping.host == nullptr;
      break;
    }
    if (mapping.guest > UINT32_MAX || !mapping.host) {
      return false;
    }
    const auto guest = static_cast<uint32_t>(mapping.guest);
    if (guest < PPCImageConfig.code_base || uint64_t(guest) >= code_end ||
        (guest & (alignof(uint32_t) - 1)) != 0 ||
        (mapping_count != 0 && guest <= previous_guest)) {
      return false;
    }
    previous_guest = guest;
    entry_count += guest == kExpectedEntryPoint ? 1 : 0;
  }
  if (!found_sentinel || mapping_count == 0 || entry_count != 1) {
    return false;
  }

  function_mapping_count_ = mapping_count;
  MCLA_PPC_INFO("MCLA module config: static image {:08X}-{:08X}, code "
                "{:08X}-{:08X}, {} mappings",
                PPCImageConfig.image_base, static_cast<uint32_t>(image_end),
                PPCImageConfig.code_base, static_cast<uint32_t>(code_end),
                function_mapping_count_);
  return true;
}

bool MclaApp::ValidateLoadedImageContract() {
  if (!runtime() || !runtime()->kernel_state() ||
      !runtime()->function_dispatcher()) {
    return false;
  }

  auto executable = runtime()->kernel_state()->GetExecutableModule();
  const auto *xex = executable ? executable->xex_module() : nullptr;
  const auto *execution_info = xex ? xex->opt_execution_info() : nullptr;
  if (!executable || !executable->is_executable() || !execution_info ||
      xex->base_address() != kExpectedImageBase ||
      xex->image_size() != kExpectedImageSize ||
      executable->entry_point() != kExpectedEntryPoint ||
      execution_info->title_id != kExpectedTitleId ||
      execution_info->media_id != kExpectedMediaId) {
    return false;
  }

  auto *dispatcher = runtime()->function_dispatcher();
  const uint32_t code_last = kExpectedCodeBase + kExpectedCodeSize - 1;
  if (!dispatcher->HasAnyFunctionTable() ||
      dispatcher->FindCallerModuleBase(kExpectedCodeBase) !=
          kExpectedCodeBase ||
      dispatcher->FindCallerModuleBase(code_last) != kExpectedCodeBase ||
      !dispatcher->GetFunction(kExpectedEntryPoint)) {
    return false;
  }

  MCLA_PPC_INFO("MCLA module identity: title {:08X}, media {:08X}, image "
                "{:08X}-{:08X}, entry {:08X}",
                static_cast<uint32_t>(execution_info->title_id),
                static_cast<uint32_t>(execution_info->media_id),
                xex->base_address(), xex->base_address() + xex->image_size(),
                executable->entry_point());
  MCLA_PPC_INFO("MCLA module config: loaded XEX base {:08X}, entry {:08X}",
                xex->base_address(), executable->entry_point());
  MCLA_PPC_INFO("MCLA module config: entry {:08X} registered in dispatch range "
                "{:08X}-{:08X}",
                kExpectedEntryPoint, kExpectedCodeBase,
                kExpectedCodeBase + kExpectedCodeSize);
  return true;
}

bool MclaApp::ValidateGameVfsContract() {
  if (!runtime() || !runtime()->file_system()) {
    return false;
  }

  auto *vfs = runtime()->file_system();
  constexpr std::string_view kMount = "\\Device\\Harddisk0\\Partition1";
  std::string game_target;
  std::string d_target;
  if (!vfs->FindSymbolicLink("game:", game_target) ||
      !vfs->FindSymbolicLink("d:", d_target) || game_target != kMount ||
      d_target != kMount) {
    return false;
  }

  struct ExpectedFile {
    std::string_view relative_path;
    size_t size;
  };
  constexpr ExpectedFile kExpectedFiles[] = {
      {"default.xex", 9252864},
      {"intro720.bik", 123836692},
      {"xarchive_cache.rpf", 2130739200},
  };

  for (const auto &expected : kExpectedFiles) {
    const std::string game_path =
        "game:\\" + std::string(expected.relative_path);
    const std::string d_path = "d:\\" + std::string(expected.relative_path);
    const std::string device_path =
        std::string(kMount) + "\\" + std::string(expected.relative_path);
    auto *game_entry = vfs->ResolvePath(game_path);
    if (!game_entry || vfs->ResolvePath(d_path) != game_entry ||
        vfs->ResolvePath(device_path) != game_entry ||
        !game_entry->is_read_only() || game_entry->size() != expected.size) {
      return false;
    }

    rex::filesystem::File *file = nullptr;
    rex::filesystem::FileAction action{};
    const auto status = vfs->OpenFile(
        nullptr, game_path, rex::filesystem::FileDisposition::kOpen,
        rex::filesystem::FileAccess::kGenericRead, false, true, &file, &action);
    if (XFAILED(status) || !file) {
      return false;
    }
    file->Destroy();
  }
  MCLA_VFS_INFO("MCLA VFS: game: and d: resolve 3/3 expected disc files on {}",
                kMount);

  auto *lowercase_archive = vfs->ResolvePath("game:\\xarchive_cache.rpf");
  if (!lowercase_archive ||
      vfs->ResolvePath("GAME:\\XARCHIVE_CACHE.RPF") != lowercase_archive ||
      vfs->ResolvePath("D:\\XaRcHiVe_CaChE.RpF") != lowercase_archive) {
    return false;
  }
  MCLA_VFS_INFO("MCLA VFS: mixed-case RPF path resolution verified");

  if (vfs->ResolvePath("game:\\..\\default.xex") ||
      vfs->ResolvePath(
          "\\Device\\Harddisk0\\Partition1\\..\\Partition1\\default.xex")) {
    return false;
  }
  MCLA_VFS_INFO("MCLA VFS: root-escape paths rejected");

  auto *xex_entry = vfs->ResolvePath("game:\\default.xex");
  if (!xex_entry) {
    return false;
  }
  rex::filesystem::File *write_file = nullptr;
  rex::filesystem::FileAction write_action{};
  const auto existing_write = vfs->OpenFile(
      nullptr, "game:\\default.xex", rex::filesystem::FileDisposition::kOpen,
      rex::filesystem::FileAccess::kGenericWrite, false, true, &write_file,
      &write_action);
  if (existing_write != kAccessDenied || write_file ||
      xex_entry->OpenMapped(rex::memory::MappedMemory::Mode::kReadWrite) ||
      vfs->DeletePath("game:\\default.xex")) {
    return false;
  }

  constexpr std::string_view kCreateProbe = "game:\\__mcla_vfs_write_probe.tmp";
  const auto create_write = vfs->OpenFile(
      nullptr, kCreateProbe, rex::filesystem::FileDisposition::kCreate,
      rex::filesystem::FileAccess::kGenericWrite, false, true, &write_file,
      &write_action);
  if (create_write != kAccessDenied || write_file ||
      vfs->ResolvePath(kCreateProbe)) {
    return false;
  }
  MCLA_VFS_INFO(
      "MCLA VFS: write, create, delete, and writable-map requests denied");
  return true;
}

bool MclaApp::WriteSyntheticCrashReport() {
  if (!runtime() || !runtime()->memory() ||
      runtime()->user_data_root().empty()) {
    return false;
  }

  rex::runtime::ThreadState thread_state(0x4D434C41, 0, 0, runtime()->memory());
  auto *context = thread_state.context();
  rex::diagnostics::GuestCrashReport report;
  {
    rex::ppc::GuestFunctionScope function_scope(*context, kExpectedEntryPoint);
    rex::ppc::SetGuestProgramCounter(*context, kExpectedEntryPoint + 4);
    rex::ppc::RecordGuestImport(*context, "__imp__XGetAVPack");
    report = rex::diagnostics::CaptureGuestCrashReport(
        "MCLA synthetic crash probe", &thread_state, 1);
  }

  const auto report_path =
      runtime()->user_data_root() / "mcla-crash-report.txt";
  std::ofstream stream(report_path, std::ios::binary | std::ios::trunc);
  if (!stream) {
    return false;
  }
  const std::string text = rex::diagnostics::FormatGuestCrashReport(report);
  stream.write(text.data(), static_cast<std::streamsize>(text.size()));
  stream.close();
  if (!stream) {
    return false;
  }

  MCLA_APP_INFO("MCLA crash probe: privacy-safe report written");
  return true;
}

void MclaApp::LaunchModule() {
  if (!ValidateLoadedImageContract()) {
    MCLA_PPC_ERROR("MCLA module config: loaded image contract rejected; guest "
                   "launch blocked");
    app_context().CallInUIThreadDeferred(
        [this]() { app_context().QuitFromUIThread(); });
    return;
  }

  if (REXCVAR_GET(mcla_module_config_probe)) {
    MCLA_PPC_INFO("MCLA module config: probe complete; guest launch skipped");
    app_context().CallInUIThreadDeferred(
        [this]() { app_context().QuitFromUIThread(); });
    return;
  }

  if (REXCVAR_GET(mcla_crash_probe)) {
    if (!WriteSyntheticCrashReport()) {
      MCLA_APP_ERROR("MCLA crash probe: report generation failed");
    } else {
      MCLA_APP_INFO("MCLA crash probe: complete; guest launch skipped");
    }
    app_context().CallInUIThreadDeferred(
        [this]() { HardExitCrashProbeFromUIThread(); });
    return;
  }

  if (!ValidateGameVfsContract()) {
    MCLA_VFS_ERROR(
        "MCLA VFS: disc-root contract rejected; guest launch blocked");
    app_context().CallInUIThreadDeferred(
        [this]() { app_context().QuitFromUIThread(); });
    return;
  }

  if (REXCVAR_GET(mcla_vfs_probe)) {
    MCLA_VFS_INFO("MCLA VFS: probe complete; guest launch skipped");
    app_context().CallInUIThreadDeferred(
        [this]() { app_context().QuitFromUIThread(); });
    return;
  }

  if (REXCVAR_GET(mcla_physics_timing_probe)) {
    auto *dispatcher = runtime()->function_dispatcher();
    physics_timer_original = dispatcher->GetFunction(kPhysicsTimerAddress);
    if (!physics_timer_original ||
        physics_timer_original == PhysicsTimerProbe ||
        !dispatcher->SetFunction(kPhysicsTimerAddress, PhysicsTimerProbe)) {
      MCLA_INPUT_ERROR("MCLA physics timing: stock timer hook {:08X} rejected",
                       kPhysicsTimerAddress);
      app_context().CallInUIThreadDeferred(
          [this]() { app_context().QuitFromUIThread(); });
      return;
    }
    MCLA_INPUT_INFO("MCLA_PHYSICS_TIMING_HOOK v=1 address={:08X} status=READY",
                    kPhysicsTimerAddress);
  }

  rex::ReXApp::LaunchModule();
}

namespace {

struct FrameMetrics {
  uint32_t occupied_rgb555_bins = 0;
  uint32_t luma_p05 = 0;
  uint32_t luma_p95 = 0;
  uint32_t modal_per_mille = 1000;
  uint32_t nonmodal_grid_cells = 0;

  bool IsNontrivial() const {
    return occupied_rgb555_bins >= 16 && luma_p95 >= luma_p05 + 8 &&
           modal_per_mille <= 995 && nonmodal_grid_cells >= 4;
  }
};

bool MeasureFrame(const rex::ui::RawImage &image, FrameMetrics &metrics) {
  constexpr uint32_t kGridWidth = 16;
  constexpr uint32_t kGridHeight = 9;
  if (image.width < 64 || image.height < 64 || image.width > 8192 ||
      image.height > 8192 || image.stride != size_t(image.width) * 4 ||
      image.data.size() != image.stride * image.height) {
    return false;
  }

  std::array<uint32_t, 1u << 15> rgb555_histogram{};
  std::array<uint32_t, 256> luma_histogram{};
  const uint64_t pixel_count = uint64_t(image.width) * image.height;
  for (uint32_t y = 0; y < image.height; ++y) {
    const uint8_t *row = image.data.data() + size_t(y) * image.stride;
    for (uint32_t x = 0; x < image.width; ++x) {
      const uint8_t *pixel = row + size_t(x) * 4;
      const uint32_t bin = (uint32_t(pixel[0] >> 3) << 10) |
                           (uint32_t(pixel[1] >> 3) << 5) |
                           uint32_t(pixel[2] >> 3);
      ++rgb555_histogram[bin];
      const uint32_t luma =
          (54u * pixel[0] + 183u * pixel[1] + 19u * pixel[2] + 128u) >> 8;
      ++luma_histogram[luma];
    }
  }

  uint32_t modal_bin = 0;
  uint32_t modal_count = 0;
  for (uint32_t i = 0; i < rgb555_histogram.size(); ++i) {
    if (rgb555_histogram[i]) {
      ++metrics.occupied_rgb555_bins;
    }
    if (rgb555_histogram[i] > modal_count) {
      modal_count = rgb555_histogram[i];
      modal_bin = i;
    }
  }
  metrics.modal_per_mille =
      static_cast<uint32_t>((uint64_t(modal_count) * 1000) / pixel_count);

  const uint64_t p05_target =
      std::max<uint64_t>(1, (pixel_count * 5 + 99) / 100);
  const uint64_t p95_target =
      std::max<uint64_t>(1, (pixel_count * 95 + 99) / 100);
  uint64_t cumulative = 0;
  bool found_p05 = false;
  for (uint32_t i = 0; i < luma_histogram.size(); ++i) {
    cumulative += luma_histogram[i];
    if (!found_p05 && cumulative >= p05_target) {
      metrics.luma_p05 = i;
      found_p05 = true;
    }
    if (cumulative >= p95_target) {
      metrics.luma_p95 = i;
      break;
    }
  }

  std::array<bool, kGridWidth * kGridHeight> nonmodal_cells{};
  for (uint32_t y = 0; y < image.height; ++y) {
    const uint8_t *row = image.data.data() + size_t(y) * image.stride;
    for (uint32_t x = 0; x < image.width; ++x) {
      const uint8_t *pixel = row + size_t(x) * 4;
      const uint32_t bin = (uint32_t(pixel[0] >> 3) << 10) |
                           (uint32_t(pixel[1] >> 3) << 5) |
                           uint32_t(pixel[2] >> 3);
      if (bin != modal_bin) {
        const uint32_t grid_x =
            std::min(kGridWidth - 1, x * kGridWidth / image.width);
        const uint32_t grid_y =
            std::min(kGridHeight - 1, y * kGridHeight / image.height);
        nonmodal_cells[grid_y * kGridWidth + grid_x] = true;
      }
    }
  }
  metrics.nonmodal_grid_cells = static_cast<uint32_t>(
      std::count(nonmodal_cells.begin(), nonmodal_cells.end(), true));
  return true;
}

void WriteLittleEndian16(std::ofstream &stream, uint16_t value) {
  const std::array<uint8_t, 2> bytes = {static_cast<uint8_t>(value),
                                        static_cast<uint8_t>(value >> 8)};
  stream.write(reinterpret_cast<const char *>(bytes.data()), bytes.size());
}

void WriteLittleEndian32(std::ofstream &stream, uint32_t value) {
  const std::array<uint8_t, 4> bytes = {
      static_cast<uint8_t>(value), static_cast<uint8_t>(value >> 8),
      static_cast<uint8_t>(value >> 16), static_cast<uint8_t>(value >> 24)};
  stream.write(reinterpret_cast<const char *>(bytes.data()), bytes.size());
}

bool WriteFrameBmp(const std::filesystem::path &path,
                   const rex::ui::RawImage &image) {
  constexpr uint32_t kFileHeaderBytes = 14;
  constexpr uint32_t kInfoHeaderBytes = 40;
  const uint64_t pixel_bytes = uint64_t(image.width) * image.height * 4;
  const uint64_t file_bytes = kFileHeaderBytes + kInfoHeaderBytes + pixel_bytes;
  if (file_bytes > std::numeric_limits<uint32_t>::max()) {
    return false;
  }

  std::ofstream stream(path, std::ios::binary | std::ios::trunc);
  if (!stream) {
    return false;
  }
  stream.put('B');
  stream.put('M');
  WriteLittleEndian32(stream, static_cast<uint32_t>(file_bytes));
  WriteLittleEndian32(stream, 0);
  WriteLittleEndian32(stream, kFileHeaderBytes + kInfoHeaderBytes);
  WriteLittleEndian32(stream, kInfoHeaderBytes);
  WriteLittleEndian32(stream, image.width);
  WriteLittleEndian32(stream, image.height);
  WriteLittleEndian16(stream, 1);
  WriteLittleEndian16(stream, 32);
  WriteLittleEndian32(stream, 0);
  WriteLittleEndian32(stream, static_cast<uint32_t>(pixel_bytes));
  WriteLittleEndian32(stream, 0);
  WriteLittleEndian32(stream, 0);
  WriteLittleEndian32(stream, 0);
  WriteLittleEndian32(stream, 0);

  std::vector<uint8_t> bgra_row(size_t(image.width) * 4);
  for (uint32_t y = image.height; y-- > 0;) {
    const uint8_t *source = image.data.data() + size_t(y) * image.stride;
    for (uint32_t x = 0; x < image.width; ++x) {
      bgra_row[size_t(x) * 4 + 0] = source[size_t(x) * 4 + 2];
      bgra_row[size_t(x) * 4 + 1] = source[size_t(x) * 4 + 1];
      bgra_row[size_t(x) * 4 + 2] = source[size_t(x) * 4 + 0];
      bgra_row[size_t(x) * 4 + 3] = 0xFF;
    }
    stream.write(reinterpret_cast<const char *>(bgra_row.data()),
                 bgra_row.size());
  }
  stream.close();
  return bool(stream);
}

bool SleepUntilOrStop(std::stop_token stop_token,
                      std::chrono::steady_clock::time_point deadline) {
  while (!stop_token.stop_requested()) {
    const auto now = std::chrono::steady_clock::now();
    if (now >= deadline) {
      return true;
    }
    std::this_thread::sleep_for(std::min(
        std::chrono::milliseconds(100),
        std::chrono::duration_cast<std::chrono::milliseconds>(deadline - now)));
  }
  return false;
}

rex::X_RESULT
SetControllerMatrixVibration(rex::input::InputSystem *input_system,
                             uint16_t left_motor_speed,
                             uint16_t right_motor_speed) {
  rex::input::X_INPUT_VIBRATION vibration{};
  vibration.left_motor_speed = left_motor_speed;
  vibration.right_motor_speed = right_motor_speed;
  return input_system->SetState(0, &vibration);
}

void RunControllerMatrixRumbleProbe(std::stop_token stop_token,
                                    rex::input::InputSystem *input_system) {
  using namespace std::chrono_literals;
  if (!input_system ||
      !SleepUntilOrStop(stop_token, std::chrono::steady_clock::now() + 5s)) {
    return;
  }

  constexpr std::array<std::array<uint16_t, 2>, 3> kPulses = {
      std::array<uint16_t, 2>{0x8000, 0x0000},
      std::array<uint16_t, 2>{0x0000, 0x8000},
      std::array<uint16_t, 2>{0x8000, 0x8000},
  };
  constexpr auto kPulseDuration = 3s;
  constexpr auto kPulseGap = 2s;
  for (size_t i = 0; i < kPulses.size(); ++i) {
    if (stop_token.stop_requested() ||
        SetControllerMatrixVibration(input_system, kPulses[i][0],
                                     kPulses[i][1]) != rex::X_RESULT{0}) {
      return;
    }

    const bool pulse_completed = SleepUntilOrStop(
        stop_token, std::chrono::steady_clock::now() + kPulseDuration);
    SetControllerMatrixVibration(input_system, 0, 0);
    if (!pulse_completed) {
      return;
    }
    if (i + 1 != kPulses.size() &&
        !SleepUntilOrStop(stop_token,
                          std::chrono::steady_clock::now() + kPulseGap)) {
      return;
    }
  }
}

} // namespace

void MclaApp::OnPostLaunchModule(rex::system::XThread *thread) {
  (void)thread;
  if (REXCVAR_GET(mcla_first_frame_probe) ||
      REXCVAR_GET(mcla_controller_matrix_probe) ||
      REXCVAR_GET(mcla_frontend_smoke_probe) ||
      REXCVAR_GET(mcla_rendering_smoke_probe) ||
      REXCVAR_GET(mcla_gameplay_input_probe) ||
      REXCVAR_GET(mcla_physics_timing_probe) ||
      REXCVAR_GET(mcla_audio_event_probe) ||
      REXCVAR_GET(mcla_race_route_probe)) {
    first_frame_probe_thread_ = std::jthread(
        [this](std::stop_token stop_token) { RunFirstFrameProbe(stop_token); });
  }
}

void MclaApp::RunFirstFrameProbe(std::stop_token stop_token) {
  using namespace std::chrono_literals;
  rex::ui::RawImage image;
  bool observed_guest_output = false;
  auto settle_deadline = std::chrono::steady_clock::time_point::max();
  while (!stop_token.stop_requested()) {
    auto *graphics = runtime() ? runtime()->graphics_system() : nullptr;
    auto *presenter = graphics ? graphics->presenter() : nullptr;
    if (!presenter || !presenter->CaptureGuestOutput(image)) {
      std::this_thread::sleep_for(100ms);
      continue;
    }

    if (!observed_guest_output) {
      observed_guest_output = true;
      settle_deadline =
          std::chrono::steady_clock::now() +
          std::chrono::seconds(REXCVAR_GET(mcla_first_frame_settle_seconds));
      if (!SleepUntilOrStop(stop_token, settle_deadline)) {
        return;
      }
      continue;
    }

    FrameMetrics metrics;
    if (!MeasureFrame(image, metrics)) {
      MCLA_GPU_ERROR("MCLA graphics: guest frame readback has invalid "
                     "dimensions or layout");
      return;
    }
    if (!metrics.IsNontrivial()) {
      std::this_thread::sleep_for(250ms);
      continue;
    }

    const auto frame_path =
        runtime()->user_data_root() / "mcla-first-frame.bmp";
    if (!WriteFrameBmp(frame_path, image)) {
      MCLA_GPU_ERROR(
          "MCLA graphics: failed to write private first-frame capture");
      return;
    }
    rex::input::sdl::ArmControllerMatrixAudit("title");
    rex::input::sdl::ArmInputSlotAudit("title");
    if (!REXCVAR_GET(mcla_rendering_smoke_probe)) {
      graphics->RequestRenderAuditCheckpoint();
    }
    rex::kernel::xam::EmitXamProfileAuditSummary("checkpoint");
    MCLA_GPU_INFO(
        "MCLA graphics: nontrivial guest frame captured {}x{}, rgb555 bins {}, "
        "luma p05 {}, luma p95 {}, modal permille {}, nonmodal grid cells {}",
        image.width, image.height, metrics.occupied_rgb555_bins,
        metrics.luma_p05, metrics.luma_p95, metrics.modal_per_mille,
        metrics.nonmodal_grid_cells);
    if (REXCVAR_GET(mcla_race_route_probe)) {
      MCLA_INPUT_INFO(
          "MCLA_RACE_ROUTE_CONFIG v=1 phases=race-start,results,return "
          "operator_confirmed=1 external_close_required=1");
      constexpr std::array<std::string_view, 3> kRacePhases = {
          "race-start", "results", "return"};
      uint32_t captured = 0;
      for (std::string_view phase : kRacePhases) {
        const auto request_path =
            runtime()->user_data_root() /
            (".mcla-race-" + std::string(phase) + ".request");
        while (!stop_token.stop_requested() &&
               !std::filesystem::exists(request_path)) {
          std::this_thread::sleep_for(100ms);
        }
        if (stop_token.stop_requested()) {
          return;
        }

        const auto frame_path = runtime()->user_data_root() /
                                ("mcla-race-" + std::string(phase) + ".bmp");
        rex::ui::RawImage race_image;
        const auto race_capture_deadline =
            std::chrono::steady_clock::now() + 10s;
        bool race_capture_succeeded = false;
        while (!stop_token.stop_requested() &&
               std::chrono::steady_clock::now() < race_capture_deadline) {
          if (presenter->CaptureGuestOutput(race_image) &&
              WriteFrameBmp(frame_path, race_image)) {
            race_capture_succeeded = true;
            break;
          }
          std::this_thread::sleep_for(100ms);
        }
        if (!race_capture_succeeded) {
          MCLA_GPU_ERROR("MCLA race route: failed to capture {} frame", phase);
          return;
        }
        std::error_code remove_error;
        if (!std::filesystem::remove(request_path, remove_error) ||
            remove_error) {
          MCLA_GPU_ERROR("MCLA race route: failed to consume {} request",
                         phase);
          return;
        }
        ++captured;
        MCLA_INPUT_INFO("MCLA_RACE_ROUTE_FRAME v=1 phase={} width={} height={} "
                        "present_seq={} status=PASS",
                        phase, race_image.width, race_image.height,
                        presenter->GetGuestOutputSequence());
      }
      MCLA_INPUT_INFO("MCLA_RACE_ROUTE_SUMMARY v=1 status=PASS frames={} "
                      "external_close_required=1",
                      captured);
    }
    if (REXCVAR_GET(sdl_audio_route_audit) &&
        !REXCVAR_GET(mcla_audio_event_probe)) {
      const uint32_t soak_seconds = REXCVAR_GET(mcla_audio_route_soak_seconds);
      MCLA_AUDIO_INFO("MCLA audio: title soak started seconds {}",
                      soak_seconds);
      if (!SleepUntilOrStop(stop_token,
                            std::chrono::steady_clock::now() +
                                std::chrono::seconds(soak_seconds))) {
        return;
      }
      rex::audio::EmitAudioRouteAuditSummary("title");
      MCLA_AUDIO_INFO("MCLA audio: title soak completed seconds {}",
                      soak_seconds);
    }
    if (REXCVAR_GET(xmp_route_audit)) {
      rex::kernel::xam::EmitXmpRouteAuditSummary("title");
      MCLA_AUDIO_INFO("MCLA audio: XMP title route summarized");
    }
    if (REXCVAR_GET(xam_offline_service_audit)) {
      rex::kernel::xam::EmitOfflineServiceAuditSummary("title");
      MCLA_APP_INFO("MCLA offline services: title route summarized");
    }
    if (REXCVAR_GET(xam_locale_audit)) {
      rex::kernel::xam::EmitLocaleAuditSummary("title");
      MCLA_APP_INFO("MCLA locale: title route summarized");
    }
    if (REXCVAR_GET(mcla_audio_event_probe)) {
      auto *audio_driver =
          static_cast<FrontendSmokeInputDriver *>(frontend_smoke_input_);
      if (!audio_driver) {
        MCLA_AUDIO_ERROR("MCLA audio event: input dependency missing");
        return;
      }
      MCLA_AUDIO_INFO(
          "MCLA_AUDIO_EVENT_CONFIG v=1 slot=0 gameplay_wait_seconds={} "
          "dismiss_pulses=6 music_seconds=8 ambient_seconds=8 "
          "voice_seconds=30 engine_seconds=8 collision_seconds=15 "
          "ui_seconds=4 external_listening_required=1",
          REXCVAR_GET(mcla_frontend_gameplay_wait_seconds));
      auto wait_phase = [&](rex::audio::AudioEventPhase phase,
                            std::chrono::seconds duration) {
        if (!rex::audio::BeginAudioEventPhase(phase)) {
          return false;
        }
        MCLA_AUDIO_INFO("MCLA_AUDIO_EVENT_WINDOW v=1 phase={} event=LISTEN",
                        rex::audio::AudioEventPhaseName(phase));
        return SleepUntilOrStop(stop_token,
                                std::chrono::steady_clock::now() + duration) &&
               rex::audio::EndAudioEventPhase(phase);
      };
      auto set_gamepad = [&](const rex::input::X_INPUT_GAMEPAD &gamepad,
                             uint32_t sequence) {
        return audio_driver->SetGameplayGamepad(stop_token, gamepad, sequence);
      };
      auto release_gamepad = [&](uint32_t sequence) {
        return audio_driver->ReleaseGameplayGamepad(stop_token, sequence);
      };
      auto press_and_release = [&](uint16_t buttons, uint32_t sequence) {
        rex::input::X_INPUT_GAMEPAD gamepad{};
        gamepad.buttons = buttons;
        return set_gamepad(gamepad, sequence) &&
               SleepUntilOrStop(stop_token,
                                std::chrono::steady_clock::now() + 250ms) &&
               release_gamepad(sequence);
      };

      if (!wait_phase(rex::audio::AudioEventPhase::kMusic, 8s) ||
          !press_and_release(rex::input::X_INPUT_GAMEPAD_START, 101) ||
          !SleepUntilOrStop(stop_token,
                            std::chrono::steady_clock::now() +
                                std::chrono::seconds(REXCVAR_GET(
                                    mcla_frontend_gameplay_wait_seconds)))) {
        MCLA_AUDIO_ERROR("MCLA audio event: title/gameplay transition failed");
        return;
      }
      for (uint32_t dismiss = 1; dismiss <= 6; ++dismiss) {
        if (!audio_driver->Pulse(stop_token, rex::input::X_INPUT_GAMEPAD_A,
                                 110 + dismiss) ||
            !SleepUntilOrStop(stop_token,
                              std::chrono::steady_clock::now() + 5s)) {
          MCLA_AUDIO_ERROR("MCLA audio event: overlay dismissal {} failed",
                           dismiss);
          return;
        }
      }
      if (!wait_phase(rex::audio::AudioEventPhase::kAmbient, 8s) ||
          !wait_phase(rex::audio::AudioEventPhase::kVoice, 30s)) {
        MCLA_AUDIO_ERROR("MCLA audio event: passive listening window failed");
        return;
      }

      rex::input::X_INPUT_GAMEPAD engine{};
      engine.right_trigger = 255;
      if (!rex::audio::BeginAudioEventPhase(
              rex::audio::AudioEventPhase::kEngine)) {
        MCLA_AUDIO_ERROR("MCLA audio event: engine window failed");
        return;
      }
      MCLA_AUDIO_INFO("MCLA_AUDIO_EVENT_WINDOW v=1 phase=engine event=LISTEN");
      if (!set_gamepad(engine, 102) ||
          !SleepUntilOrStop(stop_token,
                            std::chrono::steady_clock::now() + 8s) ||
          !release_gamepad(102) ||
          !rex::audio::EndAudioEventPhase(
              rex::audio::AudioEventPhase::kEngine)) {
        MCLA_AUDIO_ERROR("MCLA audio event: engine window failed");
        return;
      }

      rex::input::X_INPUT_GAMEPAD collision{};
      collision.right_trigger = 255;
      collision.thumb_lx = 32767;
      if (!rex::audio::BeginAudioEventPhase(
              rex::audio::AudioEventPhase::kCollision)) {
        MCLA_AUDIO_ERROR("MCLA audio event: collision window failed");
        return;
      }
      MCLA_AUDIO_INFO(
          "MCLA_AUDIO_EVENT_WINDOW v=1 phase=collision event=LISTEN");
      if (!set_gamepad(collision, 103) ||
          !SleepUntilOrStop(stop_token,
                            std::chrono::steady_clock::now() + 15s) ||
          !release_gamepad(103) ||
          !rex::audio::EndAudioEventPhase(
              rex::audio::AudioEventPhase::kCollision)) {
        MCLA_AUDIO_ERROR("MCLA audio event: collision window failed");
        return;
      }

      if (!rex::audio::BeginAudioEventPhase(rex::audio::AudioEventPhase::kUi)) {
        MCLA_AUDIO_ERROR("MCLA audio event: UI window failed");
        return;
      }
      MCLA_AUDIO_INFO("MCLA_AUDIO_EVENT_WINDOW v=1 phase=ui event=LISTEN");
      if (!press_and_release(rex::input::X_INPUT_GAMEPAD_START, 104) ||
          !SleepUntilOrStop(stop_token,
                            std::chrono::steady_clock::now() + 2s) ||
          !press_and_release(rex::input::X_INPUT_GAMEPAD_DPAD_DOWN, 105) ||
          !SleepUntilOrStop(stop_token,
                            std::chrono::steady_clock::now() + 2s) ||
          !rex::audio::EndAudioEventPhase(rex::audio::AudioEventPhase::kUi)) {
        MCLA_AUDIO_ERROR("MCLA audio event: UI window failed");
        return;
      }
      rex::audio::EmitAudioEventAuditSummary();
      if (REXCVAR_GET(sdl_audio_route_audit)) {
        rex::audio::EmitAudioRouteAuditSummary("title");
      }
      MCLA_AUDIO_INFO(
          "MCLA_AUDIO_EVENT_SUMMARY v=1 status=COMPLETE phases=6 "
          "external_listening_required=1 external_close_required=1");
      return;
    }
    if (REXCVAR_GET(mcla_gameplay_input_probe) ||
        REXCVAR_GET(mcla_physics_timing_probe)) {
      using namespace std::chrono_literals;
      const bool physics_timing = REXCVAR_GET(mcla_physics_timing_probe);
      const uint64_t guest_tick_frequency =
          rex::chrono::Clock::guest_tick_frequency();
      if (physics_timing) {
        MCLA_INPUT_INFO(
            "MCLA_PHYSICS_TIMING_CONFIG v=1 slot=0 gameplay_wait_seconds={} "
            "dismiss_pulses=6 dismiss_interval_ms=5000 sample_seconds=10 "
            "guest_tick_frequency={} expected_vblank_millihz=60000 "
            "expected_present_millihz=30000",
            REXCVAR_GET(mcla_frontend_gameplay_wait_seconds),
            guest_tick_frequency);
      } else {
        MCLA_INPUT_INFO(
            "MCLA_GAMEPLAY_INPUT_CONFIG v=1 slot=0 gameplay_wait_seconds={} "
            "dismiss_pulses=6 dismiss_interval_ms=5000 button_hold_ms=250 "
            "control_hold_ms=3000 "
            "steer_hold_ms=2000 frames=8",
            REXCVAR_GET(mcla_frontend_gameplay_wait_seconds));
      }
      auto *gameplay_driver =
          static_cast<FrontendSmokeInputDriver *>(frontend_smoke_input_);
      auto *presenter = graphics->presenter();
      if (!gameplay_driver || !presenter) {
        MCLA_INPUT_ERROR("MCLA gameplay input: probe dependencies missing");
        return;
      }

      auto capture_gameplay_frame = [&](std::string_view phase,
                                        std::string_view file_name) {
        const auto deadline = std::chrono::steady_clock::now() + 5s;
        while (!stop_token.stop_requested() &&
               std::chrono::steady_clock::now() < deadline) {
          rex::ui::RawImage gameplay_image;
          const auto path = runtime()->user_data_root() / file_name;
          if (presenter->CaptureGuestOutput(gameplay_image) &&
              WriteFrameBmp(path, gameplay_image)) {
            if (physics_timing) {
              MCLA_INPUT_INFO(
                  "MCLA_PHYSICS_TIMING_FRAME v=1 phase={} width={} height={} "
                  "status=PASS",
                  phase, gameplay_image.width, gameplay_image.height);
            } else {
              MCLA_INPUT_INFO(
                  "MCLA_GAMEPLAY_INPUT_FRAME v=1 phase={} width={} height={} "
                  "status=PASS",
                  phase, gameplay_image.width, gameplay_image.height);
            }
            return true;
          }
          std::this_thread::sleep_for(100ms);
        }
        MCLA_GPU_ERROR("MCLA gameplay input: failed to capture {} frame",
                       phase);
        return false;
      };

      auto press_and_release = [&](uint16_t buttons, uint32_t sequence) {
        rex::input::X_INPUT_GAMEPAD gamepad{};
        gamepad.buttons = buttons;
        return gameplay_driver->SetGameplayGamepad(stop_token, gamepad,
                                                   sequence) &&
               SleepUntilOrStop(stop_token,
                                std::chrono::steady_clock::now() + 250ms) &&
               gameplay_driver->ReleaseGameplayGamepad(stop_token, sequence);
      };

      if (!press_and_release(rex::input::X_INPUT_GAMEPAD_START, 1) ||
          !SleepUntilOrStop(stop_token,
                            std::chrono::steady_clock::now() +
                                std::chrono::seconds(REXCVAR_GET(
                                    mcla_frontend_gameplay_wait_seconds)))) {
        MCLA_INPUT_ERROR(
            "MCLA gameplay input: title-to-gameplay transition failed");
        return;
      }

      for (uint32_t dismiss = 1; dismiss <= 6; ++dismiss) {
        if (!gameplay_driver->Pulse(stop_token, rex::input::X_INPUT_GAMEPAD_A,
                                    10 + dismiss) ||
            !SleepUntilOrStop(stop_token,
                              std::chrono::steady_clock::now() + 5s)) {
          MCLA_INPUT_ERROR("MCLA gameplay input: overlay dismissal {} failed",
                           dismiss);
          return;
        }
      }

      if (!capture_gameplay_frame(physics_timing ? "start" : "neutral-before",
                                  physics_timing
                                      ? "mcla-physics-start.bmp"
                                      : "mcla-gameplay-neutral-before.bmp")) {
        return;
      }

      if (physics_timing) {
        rex::input::X_INPUT_GAMEPAD timing_throttle{};
        timing_throttle.right_trigger = 255;
        if (!gameplay_driver->SetGameplayGamepad(stop_token, timing_throttle,
                                                 2)) {
          MCLA_INPUT_ERROR("MCLA physics timing: throttle was not observed");
          return;
        }
        const auto host_start = std::chrono::steady_clock::now();
        BeginPhysicsTimerSampling();
        const uint64_t guest_start = rex::chrono::Clock::QueryGuestTickCount();
        const uint64_t vblank_start = presenter->GetGuestVblankSequence();
        const uint64_t present_start = presenter->GetGuestOutputSequence();
        if (!present_start || !SleepUntilOrStop(stop_token, host_start + 10s)) {
          MCLA_INPUT_ERROR("MCLA physics timing: sample window failed");
          return;
        }
        const auto host_end = std::chrono::steady_clock::now();
        const PhysicsTimerSnapshot timer_snapshot = EndPhysicsTimerSampling();
        const uint64_t guest_end = rex::chrono::Clock::QueryGuestTickCount();
        const uint64_t vblank_end = presenter->GetGuestVblankSequence();
        const uint64_t present_end = presenter->GetGuestOutputSequence();
        if (!gameplay_driver->ReleaseGameplayGamepad(stop_token, 2) ||
            !capture_gameplay_frame("end", "mcla-physics-end.bmp") ||
            guest_end <= guest_start || vblank_end <= vblank_start ||
            present_end <= present_start) {
          MCLA_INPUT_ERROR("MCLA physics timing: final sample failed");
          return;
        }
        const uint64_t host_us = static_cast<uint64_t>(
            std::chrono::duration_cast<std::chrono::microseconds>(host_end -
                                                                  host_start)
                .count());
        const uint64_t guest_ticks = guest_end - guest_start;
        const uint64_t vblank_delta = vblank_end - vblank_start;
        const uint64_t present_delta = present_end - present_start;
        const uint64_t guest_host_ratio_ppm =
            host_us && guest_tick_frequency
                ? static_cast<uint64_t>(static_cast<long double>(guest_ticks) *
                                        1000000000000.0L /
                                        (static_cast<long double>(host_us) *
                                         guest_tick_frequency))
                : 0;
        const uint64_t vblank_millihz =
            host_us ? vblank_delta * 1000000000ull / host_us : 0;
        const uint64_t present_millihz =
            host_us ? present_delta * 1000000000ull / host_us : 0;
        const uint64_t present_to_vblank_ppm =
            vblank_delta ? present_delta * 1000000ull / vblank_delta : 0;
        const uint64_t simulated_time_to_wall_ppm =
            host_us && timer_snapshot.effective_us_min != UINT32_MAX
                ? timer_snapshot.calls * timer_snapshot.effective_us_min *
                      1000000ull / host_us
                : 0;
        for (size_t i = 0; i < timer_snapshot.record_count; ++i) {
          const auto &record = timer_snapshot.records[i];
          MCLA_INPUT_INFO(
              "MCLA_PHYSICS_TIMER_RECORD v=1 id={} effective_bits={:08X} "
              "clamped_bits={:08X} raw_bits={:08X}",
              i, record.effective_bits, record.clamped_bits, record.raw_bits);
        }
        MCLA_INPUT_INFO(
            "MCLA_PHYSICS_TIMER_SUMMARY v=1 calls={} records={} "
            "invalid_values={} effective_us_min={} effective_us_max={} "
            "clamped_us_min={} clamped_us_max={} raw_us_min={} raw_us_max={}",
            timer_snapshot.calls, timer_snapshot.record_count,
            timer_snapshot.invalid_values,
            timer_snapshot.effective_us_min == UINT32_MAX
                ? 0
                : timer_snapshot.effective_us_min,
            timer_snapshot.effective_us_max,
            timer_snapshot.clamped_us_min == UINT32_MAX
                ? 0
                : timer_snapshot.clamped_us_min,
            timer_snapshot.clamped_us_max,
            timer_snapshot.raw_us_min == UINT32_MAX ? 0
                                                    : timer_snapshot.raw_us_min,
            timer_snapshot.raw_us_max);
        MCLA_INPUT_INFO(
            "MCLA_PHYSICS_TIMING_SAMPLE v=1 host_us={} guest_ticks={} "
            "guest_host_ratio_ppm={} vblank_delta={} vblank_millihz={} "
            "present_delta={} present_millihz={} present_to_vblank_ppm={} "
            "simulated_time_to_wall_ppm={}",
            host_us, guest_ticks, guest_host_ratio_ppm, vblank_delta,
            vblank_millihz, present_delta, present_millihz,
            present_to_vblank_ppm, simulated_time_to_wall_ppm);
        MCLA_INPUT_INFO(
            "MCLA_PHYSICS_TIMING_SUMMARY v=1 status=COMPLETE samples=1 "
            "frames=2 gameplay_input_records=8 external_close_required=1");
        return;
      }

      rex::input::X_INPUT_GAMEPAD throttle{};
      throttle.right_trigger = 255;
      if (!gameplay_driver->SetGameplayGamepad(stop_token, throttle, 2) ||
          !SleepUntilOrStop(stop_token,
                            std::chrono::steady_clock::now() + 3s) ||
          !capture_gameplay_frame("throttle", "mcla-gameplay-throttle.bmp") ||
          !gameplay_driver->ReleaseGameplayGamepad(stop_token, 2) ||
          !SleepUntilOrStop(stop_token,
                            std::chrono::steady_clock::now() + 1s) ||
          !capture_gameplay_frame("throttle-release",
                                  "mcla-gameplay-throttle-release.bmp")) {
        return;
      }

      rex::input::X_INPUT_GAMEPAD brake{};
      brake.left_trigger = 255;
      if (!gameplay_driver->SetGameplayGamepad(stop_token, brake, 3) ||
          !SleepUntilOrStop(stop_token,
                            std::chrono::steady_clock::now() + 3s) ||
          !capture_gameplay_frame("brake", "mcla-gameplay-brake.bmp") ||
          !gameplay_driver->ReleaseGameplayGamepad(stop_token, 3) ||
          !SleepUntilOrStop(stop_token,
                            std::chrono::steady_clock::now() + 1s) ||
          !capture_gameplay_frame("brake-release",
                                  "mcla-gameplay-brake-release.bmp")) {
        return;
      }

      rex::input::X_INPUT_GAMEPAD steer_left{};
      steer_left.right_trigger = 96;
      steer_left.thumb_lx = -32768;
      if (!gameplay_driver->SetGameplayGamepad(stop_token, steer_left, 4) ||
          !SleepUntilOrStop(stop_token,
                            std::chrono::steady_clock::now() + 2s) ||
          !capture_gameplay_frame("steer-left",
                                  "mcla-gameplay-steer-left.bmp") ||
          !gameplay_driver->ReleaseGameplayGamepad(stop_token, 4) ||
          !SleepUntilOrStop(stop_token,
                            std::chrono::steady_clock::now() + 1s)) {
        return;
      }

      rex::input::X_INPUT_GAMEPAD steer_right{};
      steer_right.right_trigger = 96;
      steer_right.thumb_lx = 32767;
      if (!gameplay_driver->SetGameplayGamepad(stop_token, steer_right, 5) ||
          !SleepUntilOrStop(stop_token,
                            std::chrono::steady_clock::now() + 2s) ||
          !capture_gameplay_frame("steer-right",
                                  "mcla-gameplay-steer-right.bmp") ||
          !gameplay_driver->ReleaseGameplayGamepad(stop_token, 5) ||
          !SleepUntilOrStop(stop_token,
                            std::chrono::steady_clock::now() + 1s) ||
          !press_and_release(rex::input::X_INPUT_GAMEPAD_START, 6) ||
          !SleepUntilOrStop(stop_token,
                            std::chrono::steady_clock::now() + 2s) ||
          !capture_gameplay_frame("pause", "mcla-gameplay-pause.bmp")) {
        return;
      }

      MCLA_INPUT_INFO(
          "MCLA_GAMEPLAY_INPUT_SUMMARY v=1 status=PASS frames=8 "
          "gameplay_input_records=24 dismiss_input_records=24 "
          "physical_reconnect_evidence=external external_close_required=1");
      return;
    }
    if (REXCVAR_GET(mcla_rendering_smoke_probe)) {
      MCLA_INPUT_INFO(
          "MCLA_RENDER_SMOKE_CONFIG v=1 slot=0 gameplay_wait_seconds={} "
          "traffic_samples=30 traffic_interval_ms=1000 camera_hold_ms=1200 "
          "particle_hold_ms=15000 dismiss_pulses=6 frames=36",
          REXCVAR_GET(mcla_frontend_gameplay_wait_seconds));
      auto *render_driver =
          static_cast<FrontendSmokeInputDriver *>(frontend_smoke_input_);
      if (!render_driver ||
          !render_driver->Pulse(stop_token, rex::input::X_INPUT_GAMEPAD_START,
                                1)) {
        MCLA_INPUT_ERROR(
            "MCLA rendering smoke: title Start pulse was not observed");
        return;
      }
      auto *presenter = graphics->presenter();
      auto capture_render_frame = [&](std::string_view phase,
                                      std::string_view file_name) {
        const auto deadline = std::chrono::steady_clock::now() + 5s;
        while (!stop_token.stop_requested() &&
               std::chrono::steady_clock::now() < deadline) {
          rex::ui::RawImage render_image;
          const auto path = runtime()->user_data_root() / file_name;
          if (presenter && presenter->CaptureGuestOutput(render_image) &&
              WriteFrameBmp(path, render_image)) {
            MCLA_INPUT_INFO(
                "MCLA_RENDER_SMOKE_FRAME v=1 phase={} width={} height={} "
                "status=PASS",
                phase, render_image.width, render_image.height);
            return true;
          }
          std::this_thread::sleep_for(100ms);
        }
        MCLA_GPU_ERROR("MCLA rendering smoke: failed to capture {} frame",
                       phase);
        return false;
      };
      if (!SleepUntilOrStop(stop_token,
                            std::chrono::steady_clock::now() +
                                std::chrono::seconds(REXCVAR_GET(
                                    mcla_frontend_gameplay_wait_seconds))) ||
          !capture_render_frame("world", "mcla-render-world.bmp")) {
        return;
      }

      for (uint32_t sample = 1; sample <= 30; ++sample) {
        char phase[16]{};
        char file_name[40]{};
        std::snprintf(phase, sizeof(phase), "traffic-%02u", sample);
        std::snprintf(file_name, sizeof(file_name),
                      "mcla-render-traffic-%02u.bmp", sample);
        if (!SleepUntilOrStop(stop_token,
                              std::chrono::steady_clock::now() + 1000ms) ||
            (sample % 5 == 0 &&
             !render_driver->Pulse(stop_token, rex::input::X_INPUT_GAMEPAD_A,
                                   1 + sample / 5)) ||
            !capture_render_frame(phase, file_name)) {
          return;
        }
      }

      rex::input::X_INPUT_GAMEPAD camera{};
      camera.thumb_ry = 32767;
      if (!render_driver->SetRenderGamepad(stop_token, camera, 1) ||
          !SleepUntilOrStop(stop_token,
                            std::chrono::steady_clock::now() + 1200ms) ||
          !capture_render_frame("sky", "mcla-render-sky.bmp") ||
          !render_driver->ReleaseRenderGamepad(stop_token, 1) ||
          !SleepUntilOrStop(stop_token,
                            std::chrono::steady_clock::now() + 1500ms) ||
          !capture_render_frame("street", "mcla-render-street.bmp")) {
        return;
      }

      rex::input::X_INPUT_GAMEPAD particle{};
      particle.left_trigger = 255;
      particle.right_trigger = 255;
      if (!render_driver->SetRenderGamepad(stop_token, particle, 2) ||
          !SleepUntilOrStop(stop_token,
                            std::chrono::steady_clock::now() + 1000ms) ||
          !capture_render_frame("particle-a", "mcla-render-particle-a.bmp") ||
          !SleepUntilOrStop(stop_token,
                            std::chrono::steady_clock::now() + 7000ms) ||
          !capture_render_frame("particle-b", "mcla-render-particle-b.bmp") ||
          !SleepUntilOrStop(stop_token,
                            std::chrono::steady_clock::now() + 7000ms) ||
          !capture_render_frame("particle-c", "mcla-render-particle-c.bmp") ||
          !render_driver->ReleaseRenderGamepad(stop_token, 2)) {
        return;
      }
      graphics->RequestRenderAuditCheckpoint();
      MCLA_INPUT_INFO("MCLA_RENDER_SMOKE_SUMMARY v=1 status=PASS frames=36 "
                      "frontend_input_records=28 render_input_records=8 "
                      "external_close_required=1");
      return;
    }
    if (REXCVAR_GET(mcla_frontend_smoke_probe)) {
      MCLA_INPUT_INFO(
          "MCLA_FRONTEND_SMOKE_TIMING v=1 first_frame_settle_seconds={} "
          "gameplay_wait_seconds={} pause_wait_seconds={}",
          REXCVAR_GET(mcla_first_frame_settle_seconds),
          REXCVAR_GET(mcla_frontend_gameplay_wait_seconds),
          REXCVAR_GET(mcla_frontend_pause_wait_seconds));
      auto *frontend_driver =
          static_cast<FrontendSmokeInputDriver *>(frontend_smoke_input_);
      if (!frontend_driver ||
          !frontend_driver->Pulse(stop_token, rex::input::X_INPUT_GAMEPAD_START,
                                  1)) {
        MCLA_INPUT_ERROR(
            "MCLA frontend smoke: title Start pulse was not observed");
        return;
      }
      auto *presenter = graphics->presenter();
      auto capture_frontend_frame = [&](std::string_view phase,
                                        std::string_view file_name) {
        const auto deadline = std::chrono::steady_clock::now() + 5s;
        while (!stop_token.stop_requested() &&
               std::chrono::steady_clock::now() < deadline) {
          rex::ui::RawImage frontend_image;
          const auto path = runtime()->user_data_root() / file_name;
          if (presenter && presenter->CaptureGuestOutput(frontend_image) &&
              WriteFrameBmp(path, frontend_image)) {
            MCLA_INPUT_INFO(
                "MCLA_FRONTEND_SMOKE_FRAME v=1 phase={} width={} height={} "
                "status=PASS",
                phase, frontend_image.width, frontend_image.height);
            return true;
          }
          std::this_thread::sleep_for(100ms);
        }
        MCLA_GPU_ERROR("MCLA frontend smoke: failed to capture {} frame",
                       phase);
        return false;
      };

      if (!SleepUntilOrStop(stop_token,
                            std::chrono::steady_clock::now() +
                                std::chrono::seconds(REXCVAR_GET(
                                    mcla_frontend_gameplay_wait_seconds))) ||
          !capture_frontend_frame("gameplay", "mcla-frontend-gameplay.bmp")) {
        return;
      }
      if (!frontend_driver->Pulse(stop_token, rex::input::X_INPUT_GAMEPAD_START,
                                  2)) {
        MCLA_INPUT_ERROR(
            "MCLA frontend smoke: pause Start pulse was not observed");
        return;
      }
      if (!SleepUntilOrStop(stop_token,
                            std::chrono::steady_clock::now() +
                                std::chrono::seconds(REXCVAR_GET(
                                    mcla_frontend_pause_wait_seconds))) ||
          !capture_frontend_frame("pause", "mcla-frontend-pause.bmp")) {
        return;
      }
      if (!frontend_driver->Pulse(
              stop_token, rex::input::X_INPUT_GAMEPAD_RIGHT_SHOULDER, 3)) {
        MCLA_INPUT_ERROR(
            "MCLA frontend smoke: modes shoulder pulse was not observed");
        return;
      }
      if (!SleepUntilOrStop(stop_token,
                            std::chrono::steady_clock::now() + 2s)) {
        return;
      }
      if (!frontend_driver->Pulse(
              stop_token, rex::input::X_INPUT_GAMEPAD_RIGHT_SHOULDER, 4)) {
        MCLA_INPUT_ERROR(
            "MCLA frontend smoke: options shoulder pulse was not observed");
        return;
      }
      if (!SleepUntilOrStop(stop_token,
                            std::chrono::steady_clock::now() + 2s) ||
          !capture_frontend_frame("options", "mcla-frontend-options.bmp")) {
        return;
      }
      MCLA_INPUT_INFO(
          "MCLA_FRONTEND_SMOKE_SUMMARY v=1 status=PASS pulses=4 "
          "source_records=8 guest_records=8 frames=3 gameplay=1 pause=1 "
          "options=1 external_close_required=1");
    }
    if (REXCVAR_GET(mcla_controller_matrix_probe)) {
      RunControllerMatrixRumbleProbe(
          stop_token,
          static_cast<rex::input::InputSystem *>(runtime()->input_system()));
    }
    return;
  }
}

[[noreturn]] void MclaApp::HardExitCrashProbeFromUIThread() {
  // Runtime teardown may force-terminate the idle audio XThread while it owns
  // a guest-heap lock, permanently poisoning that lock before FreeStack. The
  // crash probe has already closed its report and intentionally launches no
  // guest code, so use the same flush-and-hard-exit containment as WM_CLOSE.
  OnShutdown();
  MCLA_APP_INFO("MCLA crash probe: controlled hard exit");
  rex::FlushLogging();
  std::_Exit(0);
}

void MclaApp::StopFirstFrameProbe() {
  if (first_frame_probe_thread_.joinable()) {
    first_frame_probe_thread_.request_stop();
    first_frame_probe_thread_.join();
  }
}

bool MclaApp::OnWindowCloseRequested() {
  StopFirstFrameProbe();
  return true;
}

void MclaApp::OnShutdown() {
  StopFirstFrameProbe();
  MCLA_APP_INFO("MCLA lifecycle: shutdown");
}
