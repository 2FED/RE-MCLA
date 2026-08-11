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
#include <rex/ui/presenter.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
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
constexpr uint32_t kExpectedTitleId = 0x545407F8;
constexpr uint32_t kExpectedMediaId = 0x5940C9DB;
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

REXCVAR_DEFINE_BOOL(mcla_first_frame_probe, false, "MCLA",
                    "Capture the first nontrivial guest frame after presentation starts")
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
  const auto *xex = executable ? executable->xex_module() : nullptr;
  const auto *execution_info = xex ? xex->opt_execution_info() : nullptr;
  if (!executable || !executable->is_executable() ||
      !execution_info || xex->base_address() != kExpectedImageBase ||
      xex->image_size() != kExpectedImageSize ||
      executable->entry_point() != kExpectedEntryPoint ||
      execution_info->title_id != kExpectedTitleId ||
      execution_info->media_id != kExpectedMediaId) {
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

  MCLA_PPC_INFO(
      "MCLA module identity: title {:08X}, media {:08X}, image "
      "{:08X}-{:08X}, entry {:08X}",
      static_cast<uint32_t>(execution_info->title_id),
      static_cast<uint32_t>(execution_info->media_id), xex->base_address(),
      xex->base_address() + xex->image_size(), executable->entry_point());
  MCLA_PPC_INFO("MCLA module config: loaded XEX base {:08X}, entry {:08X}",
                xex->base_address(), executable->entry_point());
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
    app_context().CallInUIThreadDeferred([this]() { HardExitCrashProbeFromUIThread(); });
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

namespace {

struct FrameMetrics {
  uint32_t occupied_rgb555_bins = 0;
  uint32_t luma_p05 = 0;
  uint32_t luma_p95 = 0;
  uint32_t modal_per_mille = 1000;
  uint32_t nonmodal_grid_cells = 0;

  bool IsNontrivial() const {
    return occupied_rgb555_bins >= 16 && luma_p95 >= luma_p05 + 8 && modal_per_mille <= 995 &&
           nonmodal_grid_cells >= 4;
  }
};

bool MeasureFrame(const rex::ui::RawImage& image, FrameMetrics& metrics) {
  constexpr uint32_t kGridWidth = 16;
  constexpr uint32_t kGridHeight = 9;
  if (image.width < 64 || image.height < 64 || image.width > 8192 || image.height > 8192 ||
      image.stride != size_t(image.width) * 4 || image.data.size() != image.stride * image.height) {
    return false;
  }

  std::array<uint32_t, 1u << 15> rgb555_histogram{};
  std::array<uint32_t, 256> luma_histogram{};
  const uint64_t pixel_count = uint64_t(image.width) * image.height;
  for (uint32_t y = 0; y < image.height; ++y) {
    const uint8_t* row = image.data.data() + size_t(y) * image.stride;
    for (uint32_t x = 0; x < image.width; ++x) {
      const uint8_t* pixel = row + size_t(x) * 4;
      const uint32_t bin = (uint32_t(pixel[0] >> 3) << 10) | (uint32_t(pixel[1] >> 3) << 5) |
                           uint32_t(pixel[2] >> 3);
      ++rgb555_histogram[bin];
      const uint32_t luma = (54u * pixel[0] + 183u * pixel[1] + 19u * pixel[2] + 128u) >> 8;
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
  metrics.modal_per_mille = static_cast<uint32_t>((uint64_t(modal_count) * 1000) / pixel_count);

  const uint64_t p05_target = std::max<uint64_t>(1, (pixel_count * 5 + 99) / 100);
  const uint64_t p95_target = std::max<uint64_t>(1, (pixel_count * 95 + 99) / 100);
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
    const uint8_t* row = image.data.data() + size_t(y) * image.stride;
    for (uint32_t x = 0; x < image.width; ++x) {
      const uint8_t* pixel = row + size_t(x) * 4;
      const uint32_t bin = (uint32_t(pixel[0] >> 3) << 10) | (uint32_t(pixel[1] >> 3) << 5) |
                           uint32_t(pixel[2] >> 3);
      if (bin != modal_bin) {
        const uint32_t grid_x = std::min(kGridWidth - 1, x * kGridWidth / image.width);
        const uint32_t grid_y = std::min(kGridHeight - 1, y * kGridHeight / image.height);
        nonmodal_cells[grid_y * kGridWidth + grid_x] = true;
      }
    }
  }
  metrics.nonmodal_grid_cells =
      static_cast<uint32_t>(std::count(nonmodal_cells.begin(), nonmodal_cells.end(), true));
  return true;
}

void WriteLittleEndian16(std::ofstream& stream, uint16_t value) {
  const std::array<uint8_t, 2> bytes = {static_cast<uint8_t>(value),
                                        static_cast<uint8_t>(value >> 8)};
  stream.write(reinterpret_cast<const char*>(bytes.data()), bytes.size());
}

void WriteLittleEndian32(std::ofstream& stream, uint32_t value) {
  const std::array<uint8_t, 4> bytes = {
      static_cast<uint8_t>(value), static_cast<uint8_t>(value >> 8),
      static_cast<uint8_t>(value >> 16), static_cast<uint8_t>(value >> 24)};
  stream.write(reinterpret_cast<const char*>(bytes.data()), bytes.size());
}

bool WriteFrameBmp(const std::filesystem::path& path, const rex::ui::RawImage& image) {
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
    const uint8_t* source = image.data.data() + size_t(y) * image.stride;
    for (uint32_t x = 0; x < image.width; ++x) {
      bgra_row[size_t(x) * 4 + 0] = source[size_t(x) * 4 + 2];
      bgra_row[size_t(x) * 4 + 1] = source[size_t(x) * 4 + 1];
      bgra_row[size_t(x) * 4 + 2] = source[size_t(x) * 4 + 0];
      bgra_row[size_t(x) * 4 + 3] = 0xFF;
    }
    stream.write(reinterpret_cast<const char*>(bgra_row.data()), bgra_row.size());
  }
  stream.close();
  return bool(stream);
}

bool SleepUntilOrStop(std::stop_token stop_token, std::chrono::steady_clock::time_point deadline) {
  while (!stop_token.stop_requested()) {
    const auto now = std::chrono::steady_clock::now();
    if (now >= deadline) {
      return true;
    }
    std::this_thread::sleep_for(
        std::min(std::chrono::milliseconds(100),
                 std::chrono::duration_cast<std::chrono::milliseconds>(deadline - now)));
  }
  return false;
}

}  // namespace

void MclaApp::OnPostLaunchModule(rex::system::XThread* thread) {
  (void)thread;
  if (REXCVAR_GET(mcla_first_frame_probe)) {
    first_frame_probe_thread_ =
        std::jthread([this](std::stop_token stop_token) { RunFirstFrameProbe(stop_token); });
  }
}

void MclaApp::RunFirstFrameProbe(std::stop_token stop_token) {
  using namespace std::chrono_literals;
  rex::ui::RawImage image;
  bool observed_guest_output = false;
  auto settle_deadline = std::chrono::steady_clock::time_point::max();
  while (!stop_token.stop_requested()) {
    auto* graphics = runtime() ? runtime()->graphics_system() : nullptr;
    auto* presenter = graphics ? graphics->presenter() : nullptr;
    if (!presenter || !presenter->CaptureGuestOutput(image)) {
      std::this_thread::sleep_for(100ms);
      continue;
    }

    if (!observed_guest_output) {
      observed_guest_output = true;
      settle_deadline = std::chrono::steady_clock::now() + 3s;
      if (!SleepUntilOrStop(stop_token, settle_deadline)) {
        return;
      }
      continue;
    }

    FrameMetrics metrics;
    if (!MeasureFrame(image, metrics)) {
      MCLA_GPU_ERROR(
          "MCLA graphics: guest frame readback has invalid "
          "dimensions or layout");
      return;
    }
    if (!metrics.IsNontrivial()) {
      std::this_thread::sleep_for(250ms);
      continue;
    }

    const auto frame_path = runtime()->user_data_root() / "mcla-first-frame.bmp";
    if (!WriteFrameBmp(frame_path, image)) {
      MCLA_GPU_ERROR("MCLA graphics: failed to write private first-frame capture");
      return;
    }
    MCLA_GPU_INFO(
        "MCLA graphics: nontrivial guest frame captured {}x{}, rgb555 bins {}, "
        "luma p05 {}, luma p95 {}, modal permille {}, nonmodal grid cells {}",
        image.width, image.height, metrics.occupied_rgb555_bins, metrics.luma_p05, metrics.luma_p95,
        metrics.modal_per_mille, metrics.nonmodal_grid_cells);
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

void MclaApp::OnShutdown() {
  if (first_frame_probe_thread_.joinable()) {
    first_frame_probe_thread_.request_stop();
    first_frame_probe_thread_.join();
  }
  MCLA_APP_INFO("MCLA lifecycle: shutdown");
}
