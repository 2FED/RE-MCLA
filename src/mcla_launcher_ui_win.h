#pragma once

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include <functional>
#include <string>
#include <vector>

#include "mcla_launcher_profile_policy.h"

namespace mcla::launcher {

enum class UiAction {
  Refresh, SaveSettings, SelectProfile, RegisterManaged, CreateProfile,
  ImportLegacy, Play, OpenSave, OpenDiagnostics, RenameLabel, RenameInGameName,
  DuplicateProfile, RestoreSnapshot, VerifyRoots,
};

struct UiState {
  ProfileLibrary library;
  std::wstring build_root;
  std::wstring game_root;
  std::vector<std::wstring> gpu_names;
  std::wstring gpu_name;
  int resolution_scale = 1;
  bool vsync = true;
  bool fullscreen = false;
  bool running = false;
  std::wstring status;
  std::wstring in_game_name;
};

struct UiRequest {
  std::wstring build_root;
  std::wstring game_root;
  std::wstring gpu_name;
  int resolution_scale = 1;
  bool vsync = true;
  bool fullscreen = false;
  std::string selected_profile_id;
  std::wstring profile_label;
  std::wstring profile_root;
  std::wstring legacy_source_root;
  std::wstring in_game_name;
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
};

// Single GUI-thread callback. All validation, persistence, locking and process
// ownership belong to the caller. Closing the UI never sends a game stop/kill.
int RunLauncherUi(UiCallback callback);

// Creates an invisible real native window, initializes controls and invokes
// Refresh once. No timer or message loop is started. Caller uses DestroyWindow
// on the same thread. No COM initialization is required unless testing pickers.
HWND CreateLauncherUiForTest(UiCallback callback);

} // namespace mcla::launcher
