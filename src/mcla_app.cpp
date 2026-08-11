// mcla - project-owned ReXApp lifecycle

#include "mcla_app.h"

#include <rex/cvar.h>
#include <rex/filesystem/file.h>
#include <rex/filesystem/vfs.h>
#include <rex/logging.h>
#include <rex/memory/mapped_memory.h>
#include <rex/runtime.h>
#include <rex/system/crash_report.h>
#include <rex/system/function_dispatcher.h>
#include <rex/system/kernel_state.h>
#include <rex/system/thread_state.h>
#include <rex/system/user_module.h>

#include <array>
#include <fstream>
#include <utility>

#include "generated/default/mcla_init.h"
#include "mcla_logging.h"

namespace {

constexpr uint32_t kExpectedImageBase = 0x82000000;
constexpr uint32_t kExpectedImageSize = 0x009E0000;
constexpr uint32_t kExpectedCodeBase = 0x82130000;
constexpr uint32_t kExpectedCodeSize = 0x0069D054;
constexpr uint32_t kExpectedEntryPoint = 0x821322B8;
constexpr rex::X_STATUS kAccessDenied = 0xC0000022u;

}  // namespace

REXCVAR_DEFINE_BOOL(mcla_lifecycle_probe, false, "MCLA",
                    "Exercise the host lifecycle without constructing the guest runtime")
    .lifecycle(rex::cvar::Lifecycle::kInitOnly);

REXCVAR_DEFINE_BOOL(mcla_module_config_probe, false, "MCLA",
                    "Validate the loaded image and dispatch table without launching guest code")
    .lifecycle(rex::cvar::Lifecycle::kInitOnly);

REXCVAR_DEFINE_BOOL(mcla_vfs_probe, false, "MCLA",
                    "Validate the guest disc mount and write containment "
                    "without launching guest code")
    .lifecycle(rex::cvar::Lifecycle::kInitOnly);

REXCVAR_DEFINE_BOOL(mcla_crash_probe, false, "MCLA",
                    "Write a synthetic privacy-safe guest crash report without guest execution")
    .lifecycle(rex::cvar::Lifecycle::kInitOnly);

REXCVAR_DEFINE_BOOL(mcla_logging_probe, false, "MCLA",
                    "Emit one schema marker for every MCLA-R logging category")
    .lifecycle(rex::cvar::Lifecycle::kInitOnly);

#define MCLA_DEFINE_LOG_LEVEL_CVAR(name)                                                  \
  REXCVAR_DEFINE_STRING(mcla_log_##name, "inherit", "MCLA Logging",                       \
                        "Category level override: inherit, trace, debug, info, warn, "    \
                        "error, critical, off")                                           \
      .allowed({"inherit", "trace", "debug", "info", "warn", "error", "critical", "off"}) \
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

std::unique_ptr<rex::ui::WindowedApp> MclaApp::Create(rex::ui::WindowedAppContext &ctx) {
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
  const bool guest_free_probe =
      REXCVAR_GET(mcla_lifecycle_probe) || REXCVAR_GET(mcla_module_config_probe) ||
      REXCVAR_GET(mcla_vfs_probe) || REXCVAR_GET(mcla_crash_probe) ||
      REXCVAR_GET(mcla_logging_probe);
  if (config.gpu_plugin.empty() && !guest_free_probe) {
    config.gpu_plugin = "xenos";
    MCLA_GPU_INFO("MCLA graphics: selected GPU plugin 'xenos'");
  }
}

std::optional<rex::PathConfig> MclaApp::OnFinalizePaths(
    const rex::PathConfig &defaults, std::function<void(rex::PathConfig)> resume) {
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
  MCLA_PPC_INFO(
      "MCLA module config: static image {:08X}-{:08X}, code "
      "{:08X}-{:08X}, {} mappings",
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

  auto *dispatcher = runtime()->function_dispatcher();
  const uint32_t code_last = kExpectedCodeBase + kExpectedCodeSize - 1;
  if (!dispatcher->HasAnyFunctionTable() ||
      dispatcher->FindCallerModuleBase(kExpectedCodeBase) != kExpectedCodeBase ||
      dispatcher->FindCallerModuleBase(code_last) != kExpectedCodeBase ||
      !dispatcher->GetFunction(kExpectedEntryPoint)) {
    return false;
  }

  MCLA_PPC_INFO("MCLA module config: loaded XEX base {:08X}, entry {:08X}",
                executable->xex_module()->base_address(), executable->entry_point());
  MCLA_PPC_INFO(
      "MCLA module config: entry {:08X} registered in dispatch range "
      "{:08X}-{:08X}",
      kExpectedEntryPoint, kExpectedCodeBase, kExpectedCodeBase + kExpectedCodeSize);
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
  if (!vfs->FindSymbolicLink("game:", game_target) || !vfs->FindSymbolicLink("d:", d_target) ||
      game_target != kMount || d_target != kMount) {
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
    const std::string game_path = "game:\\" + std::string(expected.relative_path);
    const std::string d_path = "d:\\" + std::string(expected.relative_path);
    const std::string device_path =
        std::string(kMount) + "\\" + std::string(expected.relative_path);
    auto *game_entry = vfs->ResolvePath(game_path);
    if (!game_entry || vfs->ResolvePath(d_path) != game_entry ||
        vfs->ResolvePath(device_path) != game_entry || !game_entry->is_read_only() ||
        game_entry->size() != expected.size) {
      return false;
    }

    rex::filesystem::File *file = nullptr;
    rex::filesystem::FileAction action{};
    const auto status =
        vfs->OpenFile(nullptr, game_path, rex::filesystem::FileDisposition::kOpen,
                      rex::filesystem::FileAccess::kGenericRead, false, true, &file, &action);
    if (XFAILED(status) || !file) {
      return false;
    }
    file->Destroy();
  }
  MCLA_VFS_INFO("MCLA VFS: game: and d: resolve 3/3 expected disc files on {}", kMount);

  if (vfs->ResolvePath("game:\\..\\default.xex") ||
      vfs->ResolvePath("\\Device\\Harddisk0\\Partition1\\..\\Partition1\\default.xex")) {
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
      rex::filesystem::FileAccess::kGenericWrite, false, true, &write_file, &write_action);
  if (existing_write != kAccessDenied || write_file ||
      xex_entry->OpenMapped(rex::memory::MappedMemory::Mode::kReadWrite) ||
      vfs->DeletePath("game:\\default.xex")) {
    return false;
  }

  constexpr std::string_view kCreateProbe = "game:\\__mcla_vfs_write_probe.tmp";
  const auto create_write = vfs->OpenFile(
      nullptr, kCreateProbe, rex::filesystem::FileDisposition::kCreate,
      rex::filesystem::FileAccess::kGenericWrite, false, true, &write_file, &write_action);
  if (create_write != kAccessDenied || write_file || vfs->ResolvePath(kCreateProbe)) {
    return false;
  }
  MCLA_VFS_INFO("MCLA VFS: write, create, delete, and writable-map requests denied");
  return true;
}

bool MclaApp::WriteSyntheticCrashReport() {
  if (!runtime() || !runtime()->memory() || runtime()->user_data_root().empty()) {
    return false;
  }

  rex::runtime::ThreadState thread_state(0x4D434C41, 0, 0, runtime()->memory());
  auto *context = thread_state.context();
  rex::diagnostics::GuestCrashReport report;
  {
    rex::ppc::GuestFunctionScope function_scope(*context, kExpectedEntryPoint);
    rex::ppc::SetGuestProgramCounter(*context, kExpectedEntryPoint + 4);
    rex::ppc::RecordGuestImport(*context, "__imp__XGetAVPack");
    report =
        rex::diagnostics::CaptureGuestCrashReport("MCLA synthetic crash probe", &thread_state, 1);
  }

  const auto report_path = runtime()->user_data_root() / "mcla-crash-report.txt";
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
    MCLA_PPC_ERROR(
        "MCLA module config: loaded image contract rejected; guest "
        "launch blocked");
    app_context().CallInUIThreadDeferred([this]() { app_context().QuitFromUIThread(); });
    return;
  }

  if (REXCVAR_GET(mcla_module_config_probe)) {
    MCLA_PPC_INFO("MCLA module config: probe complete; guest launch skipped");
    app_context().CallInUIThreadDeferred([this]() { app_context().QuitFromUIThread(); });
    return;
  }

  if (REXCVAR_GET(mcla_crash_probe)) {
    if (!WriteSyntheticCrashReport()) {
      MCLA_APP_ERROR("MCLA crash probe: report generation failed");
    } else {
      MCLA_APP_INFO("MCLA crash probe: complete; guest launch skipped");
    }
    app_context().CallInUIThreadDeferred([this]() { app_context().QuitFromUIThread(); });
    return;
  }

  if (!ValidateGameVfsContract()) {
    MCLA_VFS_ERROR("MCLA VFS: disc-root contract rejected; guest launch blocked");
    app_context().CallInUIThreadDeferred([this]() { app_context().QuitFromUIThread(); });
    return;
  }

  if (REXCVAR_GET(mcla_vfs_probe)) {
    MCLA_VFS_INFO("MCLA VFS: probe complete; guest launch skipped");
    app_context().CallInUIThreadDeferred([this]() { app_context().QuitFromUIThread(); });
    return;
  }

  rex::ReXApp::LaunchModule();
}

void MclaApp::OnShutdown() { MCLA_APP_INFO("MCLA lifecycle: shutdown"); }
