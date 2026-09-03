// mcla - ReXGlue Recompiled Project
//
// Customize your app by overriding virtual hooks from rex::ReXApp.

#pragma once

#include <rex/rex_app.h>

#include <stop_token>
#include <thread>

namespace rex::input {
class InputDriver;
}

namespace mcla::diagnostics {
class Manager;
struct RuntimeState;
struct UiState;
} // namespace mcla::diagnostics

class MclaApp : public rex::ReXApp {
 public:
  static std::unique_ptr<rex::ui::WindowedApp> Create(rex::ui::WindowedAppContext& ctx);

 protected:
  MclaApp(rex::ui::WindowedAppContext& ctx, rex::PPCImageInfo image_info);

  void OnPostInitLogging() override;
  void OnPreSetup(rex::RuntimeConfig& config) override;
  void OnPostSetup() override;
  void OnPostLoadGraphicsPlugin() override;
  std::optional<rex::PathConfig> OnFinalizePaths(
      const rex::PathConfig& defaults, std::function<void(rex::PathConfig)> resume) override;
  void LaunchModule() override;
  void OnPostLaunchModule(rex::system::XThread* thread) override;
  void OnGuestThreadExit(rex::system::XThread* thread) override;
  bool OnWindowCloseRequested() override;
  void OnShutdown() override;

 private:
  bool ValidateStaticImageContract();
  bool ValidateLoadedImageContract();
  bool ValidateGameVfsContract();
  bool WriteSyntheticCrashReport();
  void RunFirstFrameProbe(std::stop_token stop_token);
  void StopFirstFrameProbe();
  mcla::diagnostics::RuntimeState CaptureDiagnosticState();
  mcla::diagnostics::UiState CaptureDiagnosticUiState() const;
  void QueueDiagnosticSnapshot(const char *reason);
  [[noreturn]] void HardExitCrashProbeFromUIThread();

  size_t function_mapping_count_ = 0;
  rex::input::InputDriver *frontend_smoke_input_ = nullptr;
  std::jthread first_frame_probe_thread_;
  std::unique_ptr<mcla::diagnostics::Manager> diagnostics_;
  bool diagnostic_keybind_registered_ = false;
};
