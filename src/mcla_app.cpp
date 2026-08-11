// mcla - project-owned ReXApp lifecycle

#include "mcla_app.h"

#include "generated/default/mcla_init.h"

#include <rex/cvar.h>
#include <rex/logging.h>
#include <rex/runtime.h>
#include <rex/system/function_dispatcher.h>
#include <rex/system/kernel_state.h>
#include <rex/system/user_module.h>

namespace {

constexpr uint32_t kExpectedImageBase = 0x82000000;
constexpr uint32_t kExpectedImageSize = 0x009E0000;
constexpr uint32_t kExpectedCodeBase = 0x82130000;
constexpr uint32_t kExpectedCodeSize = 0x0069D054;
constexpr uint32_t kExpectedEntryPoint = 0x821322B8;

}  // namespace

REXCVAR_DEFINE_BOOL(mcla_lifecycle_probe, false, "MCLA",
                    "Exercise the host lifecycle without constructing the guest runtime")
    .lifecycle(rex::cvar::Lifecycle::kInitOnly);

REXCVAR_DEFINE_BOOL(mcla_module_config_probe, false, "MCLA",
                    "Validate the loaded image and dispatch table without launching guest code")
    .lifecycle(rex::cvar::Lifecycle::kInitOnly);

MclaApp::MclaApp(rex::ui::WindowedAppContext& ctx, rex::PPCImageInfo image_info)
    : ReXApp(ctx, "mcla", image_info) {}

std::unique_ptr<rex::ui::WindowedApp> MclaApp::Create(rex::ui::WindowedAppContext& ctx) {
  return std::unique_ptr<MclaApp>(new MclaApp(ctx, PPCImageConfig));
}

void MclaApp::OnPostInitLogging() {
  REXLOG_INFO("MCLA lifecycle: logging ready");
}

std::optional<rex::PathConfig> MclaApp::OnFinalizePaths(
    const rex::PathConfig& defaults, std::function<void(rex::PathConfig)> resume) {
  (void)resume;
  if (REXCVAR_GET(mcla_lifecycle_probe)) {
    REXLOG_INFO("MCLA lifecycle: probe requested; guest runtime skipped");
    app_context().CallInUIThreadDeferred([this]() {
      REXLOG_INFO("MCLA lifecycle: probe complete");
      app_context().QuitFromUIThread();
    });
    return std::nullopt;
  }

  if (!ValidateStaticImageContract()) {
    REXLOG_ERROR("MCLA module config: static image contract rejected");
    app_context().CallInUIThreadDeferred([this]() { app_context().QuitFromUIThread(); });
    return std::nullopt;
  }

  return defaults;
}

bool MclaApp::ValidateStaticImageContract() {
  if (PPCImageConfig.image_base != kExpectedImageBase ||
      PPCImageConfig.image_size != kExpectedImageSize ||
      PPCImageConfig.code_base != kExpectedCodeBase ||
      PPCImageConfig.code_size != kExpectedCodeSize || !PPCImageConfig.func_mappings) {
    return false;
  }

  const uint64_t image_end = uint64_t(PPCImageConfig.image_base) + PPCImageConfig.image_size;
  const uint64_t code_end = uint64_t(PPCImageConfig.code_base) + PPCImageConfig.code_size;
  const uint64_t dispatch_end = image_end + (uint64_t(PPCImageConfig.code_size) +
                                             rex::runtime::FunctionDispatcher::kThunkReserveSize) *
                                                2;
  if (PPCImageConfig.image_size == 0 || PPCImageConfig.code_size == 0 ||
      PPCImageConfig.code_base < PPCImageConfig.image_base || code_end > image_end ||
      dispatch_end > (uint64_t{1} << 32)) {
    return false;
  }

  const size_t max_mappings = PPCImageConfig.code_size / sizeof(uint32_t) + 1;
  size_t mapping_count = 0;
  size_t entry_count = 0;
  uint32_t previous_guest = 0;
  bool found_sentinel = false;
  for (; mapping_count < max_mappings; ++mapping_count) {
    const auto& mapping = PPCImageConfig.func_mappings[mapping_count];
    if (mapping.guest == 0) {
      found_sentinel = mapping.host == nullptr;
      break;
    }
    if (mapping.guest > UINT32_MAX || !mapping.host) {
      return false;
    }
    const auto guest = static_cast<uint32_t>(mapping.guest);
    if (guest < PPCImageConfig.code_base || uint64_t(guest) >= code_end ||
        (guest & (alignof(uint32_t) - 1)) != 0 || (mapping_count != 0 && guest <= previous_guest)) {
      return false;
    }
    previous_guest = guest;
    entry_count += guest == kExpectedEntryPoint ? 1 : 0;
  }
  if (!found_sentinel || mapping_count == 0 || entry_count != 1) {
    return false;
  }

  function_mapping_count_ = mapping_count;
  REXLOG_INFO("MCLA module config: static image {:08X}-{:08X}, code {:08X}-{:08X}, {} mappings",
              PPCImageConfig.image_base, static_cast<uint32_t>(image_end), PPCImageConfig.code_base,
              static_cast<uint32_t>(code_end), function_mapping_count_);
  return true;
}

bool MclaApp::ValidateLoadedImageContract() {
  if (!runtime() || !runtime()->kernel_state() || !runtime()->function_dispatcher()) {
    return false;
  }

  auto executable = runtime()->kernel_state()->GetExecutableModule();
  if (!executable || !executable->is_executable() ||
      executable->xex_module()->base_address() != kExpectedImageBase ||
      executable->entry_point() != kExpectedEntryPoint) {
    return false;
  }

  auto* dispatcher = runtime()->function_dispatcher();
  const uint32_t code_last = kExpectedCodeBase + kExpectedCodeSize - 1;
  if (!dispatcher->HasAnyFunctionTable() ||
      dispatcher->FindCallerModuleBase(kExpectedCodeBase) != kExpectedCodeBase ||
      dispatcher->FindCallerModuleBase(code_last) != kExpectedCodeBase ||
      !dispatcher->GetFunction(kExpectedEntryPoint)) {
    return false;
  }

  REXLOG_INFO("MCLA module config: loaded XEX base {:08X}, entry {:08X}",
              executable->xex_module()->base_address(), executable->entry_point());
  REXLOG_INFO("MCLA module config: entry {:08X} registered in dispatch range {:08X}-{:08X}",
              kExpectedEntryPoint, kExpectedCodeBase, kExpectedCodeBase + kExpectedCodeSize);
  return true;
}

void MclaApp::LaunchModule() {
  if (!ValidateLoadedImageContract()) {
    REXLOG_ERROR("MCLA module config: loaded image contract rejected; guest launch blocked");
    app_context().CallInUIThreadDeferred([this]() { app_context().QuitFromUIThread(); });
    return;
  }

  if (REXCVAR_GET(mcla_module_config_probe)) {
    REXLOG_INFO("MCLA module config: probe complete; guest launch skipped");
    app_context().CallInUIThreadDeferred([this]() { app_context().QuitFromUIThread(); });
    return;
  }

  rex::ReXApp::LaunchModule();
}

void MclaApp::OnShutdown() {
  REXLOG_INFO("MCLA lifecycle: shutdown");
}
