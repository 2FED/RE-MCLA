#pragma once

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include <functional>
#include <cstdint>
#include <string>
#include <vector>

#include "mcla_launcher_profile_policy.h"
#include "mcla_diagnostic_export_ui_win.h"

namespace mcla::launcher {

enum class UiAction {
  Refresh, SaveSettings, SelectProfile, RegisterManaged, CreateProfile,
  ImportLegacy, Play, OpenSave, OpenDiagnostics, RenameLabel, RenameInGameName,
  DuplicateProfile, RestoreSnapshot, VerifyRoots, CheckLaunch,
  ExportPrepare, ExportConfirm, ExportCancel,
};

enum class LaunchReadiness { Unchecked, Ready, Blocked, Running };

struct UiState {
  ProfileLibrary library;
  std::wstring build_root;
  std::wstring game_root;
  std::vector<std::wstring> gpu_names;
  std::wstring gpu_name;
  uint32_t render_target = 720;
  bool vsync = true;
  bool fullscreen = false;
  TimingMode timing = TimingMode::Stock;
  bool running = false;
  std::wstring status;
  std::wstring in_game_name;
  LaunchReadiness readiness = LaunchReadiness::Unchecked;
  std::wstring readiness_reason;
  uint64_t eligibility_expires_utc = 0;
  // Available collection parent, a convenience hint only; never an admission.
  std::wstring collection_parent;
  // Explicit VerifyRoots failure contradicts any prior readiness approval.
  bool verification_failed = false;
  diagnostic_export::DialogResult export_result;
};

struct UiRequest {
  std::wstring build_root;
  std::wstring game_root;
  std::wstring gpu_name;
  uint32_t render_target = 720;
  bool vsync = true;
  bool fullscreen = false;
  TimingMode timing = TimingMode::Stock;
  std::string selected_profile_id;
  std::wstring profile_label;
  std::wstring profile_root;
  std::wstring legacy_source_root;
  std::wstring in_game_name;
  diagnostic_export::DialogRequest export_request;
};

using UiCallback = std::function<UiState(UiAction, const UiRequest &)>;

// Stable IDs let native tests inspect controls without a desktop screenshot or
// game process. These are UI controls, not authorization to perform an action.
enum UiControlId : int {
  UiBuildRoot = 100, UiBrowseBuild, UiGameRoot, UiBrowseGame,
  UiProfiles, UiSelectedRoot, UiProfileLabel, UiProfileRoot, UiBrowseProfile,
  UiLegacySource, UiBrowseLegacy, UiRegister, UiCreate, UiImport,
  UiGpu, UiScale, UiVsync, UiFullscreen, UiTiming,
  UiSaveSettings, UiPlay, UiOpenSave, UiOpenDiagnostics, UiStatus,
  UiRenameLabel, UiInGameName, UiRenameInGameName,
  UiDuplicateProfile, UiRestoreSnapshot, UiVerifyRoots,
  UiPagePlay, UiPageProfiles, UiPageSetup,
  UiCurrentName,
  UiSelectedProfileName, UiOperationConfirm, UiOperationCancel,
  UiCheckLaunch, UiReadiness, UiRenderTargetNote,
  UiExportReport,
  UiCompatibilitySummary, UiAccessibilitySummary, UiCheatsSummary,
  UiTimingNote,
};

// Callbacks are serialized. Only the read-only CheckLaunch may run on a worker;
// all other callbacks run on the GUI thread. The UI drains its worker before
// returning, preserving caller lifetime. Closing never sends game stop/kill.
int RunLauncherUi(UiCallback callback);

// Creates an invisible real native window, initializes controls and invokes
// Refresh once. No timer or message loop is started. Caller uses DestroyWindow
// on the same thread. No COM initialization is required unless testing pickers.
HWND CreateLauncherUiForTest(UiCallback callback);

// Presentation-only keyboard routing for this launcher's HWND. The production
// loop supplies modifier state; native tests supply values without injecting
// keyboard input. Returns true only for a consumed Ctrl(+Shift)+Tab key-down.
bool HandleLauncherNavigationKey(HWND window, UINT message, WPARAM key,
                                 bool control_down, bool shift_down);

// Deterministic clock only for hidden fixture windows; ignored in production.
void SetLauncherClockForTest(HWND window, uint64_t monotonic_ms, uint64_t utc_seconds);

// Opt-in background checks for hidden fixtures only; both ignore production HWNDs.
void EnableLauncherAutoChecksForTest(HWND window);
bool PollLauncherCheckForTest(HWND window);

} // namespace mcla::launcher
