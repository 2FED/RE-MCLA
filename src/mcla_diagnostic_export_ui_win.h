#pragma once

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include <functional>
#include <string>
#include <string_view>

namespace mcla::launcher::diagnostic_export {
enum class DialogAction { Prepare, Confirm, Cancel };
struct DialogRequest {
  std::wstring package_path, destination_parent;
  std::string confirmation_token;
};
struct DialogResult {
  bool ready = false, complete = false;
  std::wstring destination, status;
  std::string report_json, inventory_text, confirmation_token;
};
using DialogCallback = std::function<DialogResult(DialogAction, const DialogRequest &)>;
inline constexpr UINT kExportDialogClosedMessage = WM_APP + 72;
enum ExportDialogControlId : int {
  ExportPackage = 4100, ExportBrowsePackage, ExportParent, ExportBrowseParent,
  ExportPreview, ExportContents, ExportStatus, ExportSave, ExportCancel,
  ExportHeading, ExportCaveat, ExportPackageLabel, ExportParentLabel, ExportPreviewHint,
};

// Owned non-resizable window bounded by the monitor work area; caller's existing IsDialogMessage loop handles tab
// navigation. No filesystem/export logic here; only explicit folder pickers.
// Hidden fixtures neither show/activate windows nor disable their owner.
HWND CreateExportDialog(HWND owner, std::wstring_view diagnostics_hint,
                        DialogCallback callback, bool hidden_for_test = false);
// Hidden-fixture-only layout seams. Production uses monitor work area / WM_DPICHANGED.
bool SetExportDialogDpiForTest(HWND dialog, UINT dpi);
bool SetExportDialogWorkAreaForTest(HWND dialog, const RECT &work_area);
} // namespace mcla::launcher::diagnostic_export
