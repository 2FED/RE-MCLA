// mcla - ReXGlue Recompiled Project
//
// Customize your app by overriding virtual hooks from rex::ReXApp.

#pragma once

#include <rex/rex_app.h>

class MclaApp : public rex::ReXApp {
 public:
  static std::unique_ptr<rex::ui::WindowedApp> Create(rex::ui::WindowedAppContext& ctx);

 protected:
  MclaApp(rex::ui::WindowedAppContext& ctx, rex::PPCImageInfo image_info);

  void OnPostInitLogging() override;
  std::optional<rex::PathConfig> OnFinalizePaths(
      const rex::PathConfig& defaults, std::function<void(rex::PathConfig)> resume) override;
  void LaunchModule() override;
  void OnShutdown() override;

 private:
  bool ValidateStaticImageContract();
  bool ValidateLoadedImageContract();
  bool ValidateGameVfsContract();
  bool WriteSyntheticCrashReport();

  size_t function_mapping_count_ = 0;
};
