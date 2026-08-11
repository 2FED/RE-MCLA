// mcla - ReXGlue Recompiled Project
//
// Customize your app by overriding virtual hooks from rex::ReXApp.

#pragma once

#include <rex/rex_app.h>

#include <stop_token>
#include <thread>

class MclaApp : public rex::ReXApp {
 public:
  static std::unique_ptr<rex::ui::WindowedApp> Create(rex::ui::WindowedAppContext& ctx);

 protected:
  MclaApp(rex::ui::WindowedAppContext& ctx, rex::PPCImageInfo image_info);

  void OnPostInitLogging() override;
  void OnPreSetup(rex::RuntimeConfig& config) override;
  std::optional<rex::PathConfig> OnFinalizePaths(
      const rex::PathConfig& defaults, std::function<void(rex::PathConfig)> resume) override;
  void LaunchModule() override;
  void OnPostLaunchModule(rex::system::XThread* thread) override;
  void OnShutdown() override;

 private:
  bool ValidateStaticImageContract();
  bool ValidateLoadedImageContract();
  bool ValidateGameVfsContract();
  bool WriteSyntheticCrashReport();
  void RunFirstFrameProbe(std::stop_token stop_token);
  [[noreturn]] void HardExitCrashProbeFromUIThread();

  size_t function_mapping_count_ = 0;
  std::jthread first_frame_probe_thread_;
};
