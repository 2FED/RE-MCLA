#pragma once

#include "mcla_launcher_preferences.h"
#include "mcla_launcher_fs_win.h"

#include <expected>
#include <memory>
#include <string>
#include <string_view>

namespace mcla::launcher::diagnostic_export {

// Local metadata export, not upload or permission to distribute a game build.
// Only an explicit completed live package from the selected managed profile is
// admitted. Historical receipts supply settings, never current preferences.
class ExportPlan {
 public:
  struct State;
  ExportPlan(ExportPlan &&) noexcept;
  ExportPlan &operator=(ExportPlan &&) noexcept;
  ~ExportPlan();
  ExportPlan(const ExportPlan &) = delete;
  ExportPlan &operator=(const ExportPlan &) = delete;
  const std::string &report_json() const;
  const std::string &inventory_text() const;
  const std::wstring &destination() const;
  const std::string &confirmation_token() const;
 private:
  explicit ExportPlan(std::unique_ptr<State>);
  std::unique_ptr<State> state_;
  friend std::expected<ExportPlan, std::wstring> PrepareLiveExport(
      const Preferences &, const fs::DirectoryGuard &, std::string_view,
      std::wstring_view, std::wstring_view);
  friend std::expected<std::wstring, std::wstring> ConfirmExport(
      ExportPlan &, std::string_view);
};

// Read-only plan. Holds the existing profile lock and consumed input files
// against replacement/writes until destruction. Cancel = destroy; zero writes.
// Registered roots and build/game/settings directories protect the destination;
// all must be accessible, but no game assets or current build lease are read.
std::expected<ExportPlan, std::wstring> PrepareLiveExport(
    const Preferences &preferences, const fs::DirectoryGuard &settings_root,
    std::string_view session_id, std::wstring_view live_package,
    std::wstring_view destination_parent);

// One-shot confirmation of the exact displayed bytes AND destination. A wrong
// token writes nothing. Fresh child + CREATE_NEW files only, no overwrite or
// cleanup. Failures after creation retain an incomplete child for inspection.
// Caller must show both file contents, destination and exclusions before use.
std::expected<std::wstring, std::wstring> ConfirmExport(
    ExportPlan &plan, std::string_view confirmation_token);

#ifdef MCLA_DIAGNOSTIC_EXPORT_TESTING
enum class ExportTestPhase { ChildCreated, ReportWritten };
// Compiled out of the production tool; deterministic retained-output fixtures.
void SetExportTestHook(void (*hook)(ExportTestPhase, std::wstring_view));
#endif

} // namespace mcla::launcher::diagnostic_export
