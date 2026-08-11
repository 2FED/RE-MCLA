// mcla - project-owned ReXApp lifecycle

#include "mcla_app.h"

#include "generated/default/mcla_init.h"

#include <rex/cvar.h>
#include <rex/logging.h>

REXCVAR_DEFINE_BOOL(mcla_lifecycle_probe, false, "MCLA",
                    "Exercise the host lifecycle without constructing the guest runtime")
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
  if (!REXCVAR_GET(mcla_lifecycle_probe)) {
    return defaults;
  }

  REXLOG_INFO("MCLA lifecycle: probe requested; guest runtime skipped");
  app_context().CallInUIThreadDeferred([this]() {
    REXLOG_INFO("MCLA lifecycle: probe complete");
    app_context().QuitFromUIThread();
  });
  return std::nullopt;
}

void MclaApp::OnShutdown() {
  REXLOG_INFO("MCLA lifecycle: shutdown");
}
