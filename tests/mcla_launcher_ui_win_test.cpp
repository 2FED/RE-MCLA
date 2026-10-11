#include "mcla_launcher_ui_win.h"

#include <iostream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace {
using namespace mcla::launcher;
unsigned checks = 0;
unsigned failures = 0;
constexpr char kMainId[] = "1234567890abcdef1234567890abcdef";
constexpr char kCleanId[] = "2234567890abcdef1234567890abcdef";
constexpr char kLegacyId[] = "3234567890abcdef1234567890abcdef";

void Check(bool value, const char *label) {
  ++checks;
  if (!value) { ++failures; std::cerr << "FAIL " << label << '\n'; }
}

UiState Model() {
  UiState state;
  state.library = {1, {
      {kMainId, u"MAIN", u"D:/SyntheticSaves/MAIN", Layout::ManagedExternalV1},
      {kCleanId, u"Clean", u"D:/SyntheticSaves/Clean", Layout::ManagedExternalV1},
      {kLegacyId, u"Subtitle fixture", u"E:/SyntheticLegacy/user", Layout::LegacyBundleUserReadOnlyV1}}, kMainId};
  state.build_root = L"C:/SyntheticBuild";
  state.game_root = L"F:/SyntheticGame";
  state.gpu_names = {L"Synthetic integrated GPU", L"Synthetic discrete GPU"};
  state.gpu_name = state.gpu_names[1];
  state.in_game_name = L"User";
  state.status = L"Synthetic callback only; no runtime validation or launch.";
  return state;
}

struct Call { UiAction action; UiRequest request; };
struct Recorder {
  UiState model = Model();
  std::vector<Call> calls;
  bool throw_standard = false;
  bool throw_unknown = false;
  UiState Invoke(UiAction action, const UiRequest &request) {
    calls.push_back({action, request});
    if (throw_standard) throw std::runtime_error("DO_NOT_LEAK_EXCEPTION_BODY");
    if (throw_unknown) throw 42;
    if (action == UiAction::SelectProfile) model.library.selected_id = request.selected_profile_id;
    return model;
  }
};

class InvisibleWindow {
 public:
  explicit InvisibleWindow(Recorder &recorder) : window_(CreateLauncherUiForTest(
      [&recorder](UiAction action, const UiRequest &request) {
        return recorder.Invoke(action, request);
      })) {
    Check(window_ != nullptr, "hidden native window created");
  }
  ~InvisibleWindow() { if (IsWindow(window_)) DestroyWindow(window_); }
  InvisibleWindow(const InvisibleWindow &) = delete;
  InvisibleWindow &operator=(const InvisibleWindow &) = delete;
  operator HWND() const { return window_; }
 private:
  HWND window_ = nullptr;
};

std::wstring Text(HWND window, int id) {
  HWND control = GetDlgItem(window, id);
  const auto length = GetWindowTextLengthW(control);
  std::wstring text(static_cast<size_t>(length) + 1, L'\0');
  const int copied = GetWindowTextW(control, text.data(), length + 1);
  text.resize(static_cast<size_t>(copied));
  return text;
}

std::wstring Class(HWND window, int id) {
  wchar_t name[64]{};
  GetClassNameW(GetDlgItem(window, id), name, 64);
  return name;
}

void Click(HWND window, int id) {
  // Direct parent-message injection intentionally tests handler guards, not
  // only the normal disabled-button behavior. No keyboard/mouse is injected.
  SendMessageW(window, WM_COMMAND, MAKEWPARAM(id, BN_CLICKED),
               reinterpret_cast<LPARAM>(GetDlgItem(window, id)));
}

void SelectProfile(HWND window, int index) {
  SendDlgItemMessageW(window, UiProfiles, CB_SETCURSEL, index, 0);
  SendMessageW(window, WM_COMMAND, MAKEWPARAM(UiProfiles, CBN_SELCHANGE),
               reinterpret_cast<LPARAM>(GetDlgItem(window, UiProfiles)));
}

void Edit(HWND window, int id, const wchar_t *value) {
  SetWindowTextW(GetDlgItem(window, id), value);
}

void SetRequestFields(HWND window) {
  Edit(window, UiBuildRoot, L"C:/SelectedBuild");
  Edit(window, UiGameRoot, L"F:/SelectedGame");
  Edit(window, UiProfileLabel, L"My named fixture");
  Edit(window, UiProfileRoot, L"D:/ExplicitDestination");
  Edit(window, UiLegacySource, L"E:/ExplicitSource/user");
  Edit(window, UiInGameName, L"RoadDriver");
  SendDlgItemMessageW(window, UiGpu, CB_SETCURSEL, 0, 0);
  SendDlgItemMessageW(window, UiScale, CB_SETCURSEL, 1, 0);
  SendDlgItemMessageW(window, UiVsync, BM_SETCHECK, BST_UNCHECKED, 0);
  SendDlgItemMessageW(window, UiFullscreen, BM_SETCHECK, BST_CHECKED, 0);
}

void CheckRequest(const UiRequest &request) {
  Check(request.build_root == L"C:/SelectedBuild", "exact requested build");
  Check(request.game_root == L"F:/SelectedGame", "exact requested game");
  Check(request.gpu_name == L"Synthetic integrated GPU", "explicit named GPU collected");
  Check(request.resolution_scale == 2, "2x selection collected");
  Check(!request.vsync, "VSync false collected");
  Check(request.fullscreen, "fullscreen true collected");
  Check(request.selected_profile_id == kCleanId, "explicit selected opaque profile ID");
  Check(request.profile_label == L"My named fixture", "requested library label collected");
  Check(request.profile_root == L"D:/ExplicitDestination", "explicit destination collected");
  Check(request.legacy_source_root == L"E:/ExplicitSource/user", "explicit separate legacy source collected");
  Check(request.in_game_name == L"RoadDriver", "separate in-game name collected");
}

void CheckMutationControls(HWND window, bool enabled) {
  for (int id : {UiBuildRoot, UiBrowseBuild, UiGameRoot, UiBrowseGame,
       UiProfiles, UiProfileLabel, UiProfileRoot, UiBrowseProfile,
       UiLegacySource, UiBrowseLegacy, UiRegister, UiCreate, UiImport,
       UiGpu, UiScale, UiFullscreen, UiSaveSettings, UiPlay,
       UiRenameLabel, UiInGameName, UiRenameInGameName,
       UiDuplicateProfile, UiRestoreSnapshot, UiVerifyRoots}) {
    Check((IsWindowEnabled(GetDlgItem(window, id)) != FALSE) == enabled,
          "all mutating controls match running guard");
  }
  const bool repair_vsync = SendDlgItemMessageW(window, UiVsync, BM_GETCHECK, 0, 0) != BST_CHECKED;
  Check((IsWindowEnabled(GetDlgItem(window, UiVsync)) != FALSE) == (enabled && repair_vsync),
        "VSync On cannot be disabled in stock; legacy Off can be repaired when idle");
}

void TestInitialAndRequests() {
  Recorder recorder;
  InvisibleWindow window(recorder);
  if (!window) return;
  Check(!IsWindowVisible(window), "test seam never shows UI");
  Check(recorder.calls.size() == 1 && recorder.calls.front().action == UiAction::Refresh,
        "exactly one initial Refresh and no implicit action");
  for (int id = UiBuildRoot; id <= UiVerifyRoots; ++id)
    Check(GetDlgItem(window, id) != nullptr, "all stable control IDs exist");
  Check(Text(window, UiBuildRoot) == recorder.model.build_root, "initial build path");
  Check(Text(window, UiGameRoot) == recorder.model.game_root, "initial game path");
  Check(Text(window, UiProfiles) == L"MAIN", "explicit MAIN label shown");
  Check(Text(window, UiSelectedRoot) == L"D:/SyntheticSaves/MAIN", "selected full path shown");
  Check(Text(window, UiInGameName) == L"User", "authoritative in-game name shown separately");
  Check(SendDlgItemMessageW(window, UiInGameName, EM_GETLIMITTEXT, 0, 0) == 15,
        "in-game edit has 15-character UI limit");
  Check(Text(window, UiRenameLabel) == L"Rename selected label" &&
        Text(window, UiRenameInGameName) == L"Apply in-game name", "rename buttons distinguish label from game name");
  Check(Text(window, UiDuplicateProfile) == L"Copy selected to NEW profile" &&
        Text(window, UiRestoreSnapshot) == L"Restore snapshot as NEW profile",
        "copy and recovery buttons explicitly create NEW profiles");
  Check(Text(window, UiVerifyRoots) == L"Verify build / game",
        "explicit build/game verification button exists");
  Check((GetWindowLongPtrW(GetDlgItem(window, UiSelectedRoot), GWL_STYLE) & ES_READONLY) != 0,
        "selected full path read-only");
  Check(Text(window, UiStatus) == recorder.model.status, "backend status copied without success inference");
  Check(Text(window, UiProfileLabel).empty() && Text(window, UiProfileRoot).empty() &&
        Text(window, UiLegacySource).empty(), "new/import request fields initially empty");
  Check(Text(window, UiGpu) == L"Synthetic discrete GPU", "exact persisted named GPU selected");
  Check(SendDlgItemMessageW(window, UiScale, CB_GETCOUNT, 0, 0) == 2, "scale has only 1x and 2x");
  Check(SendDlgItemMessageW(window, UiScale, CB_GETCURSEL, 0, 0) == 0, "default scale 1x");
  Check(SendDlgItemMessageW(window, UiVsync, BM_GETCHECK, 0, 0) == BST_CHECKED, "default VSync checked");
  Check(SendDlgItemMessageW(window, UiFullscreen, BM_GETCHECK, 0, 0) == BST_UNCHECKED, "default fullscreen unchecked");
  Check(Class(window, UiTiming) == L"Static", "timing is text, not selectable control");
  Check(Text(window, UiTiming).find(L"Stock only") != std::wstring::npos &&
        Text(window, UiTiming).find(L"60 FPS unavailable") != std::wstring::npos,
        "stock only and unavailable 60 explicit");
  Check((GetWindowLongPtrW(GetDlgItem(window, UiTiming), GWL_STYLE) & WS_TABSTOP) == 0,
        "unsupported timing not keyboard selectable");
  CheckMutationControls(window, true);
  SelectProfile(window, 1);
  Check(recorder.calls.size() == 2 && recorder.calls.back().action == UiAction::SelectProfile,
        "one explicit selection action");
  Check(recorder.calls.back().request.selected_profile_id == kCleanId,
        "selection dispatch uses opaque Clean ID");
  Check(Text(window, UiProfiles) == L"Clean" && Text(window, UiSelectedRoot) == L"D:/SyntheticSaves/Clean",
        "accepted selection updates label and exact root");
  for (const auto &[id, action] : std::vector<std::pair<int, UiAction>>{
       {UiSaveSettings, UiAction::SaveSettings}, {UiPlay, UiAction::Play},
       {UiRegister, UiAction::RegisterManaged}, {UiCreate, UiAction::CreateProfile},
       {UiImport, UiAction::ImportLegacy}, {UiRenameLabel, UiAction::RenameLabel},
       {UiRenameInGameName, UiAction::RenameInGameName},
       {UiDuplicateProfile, UiAction::DuplicateProfile}, {UiRestoreSnapshot, UiAction::RestoreSnapshot},
       {UiVerifyRoots, UiAction::VerifyRoots}}) {
    SetRequestFields(window);
    const size_t before = recorder.calls.size();
    Click(window, id);
    Check(recorder.calls.size() == before + 1 && recorder.calls.back().action == action,
          "one exact callback per explicit button");
    CheckRequest(recorder.calls.back().request);
    Check(recorder.model.library.selected_id == kCleanId,
          "action does not invent automatic selection outside backend result");
  }
  Click(window, UiOpenSave);
  Check(recorder.calls.back().action == UiAction::OpenSave, "open-save delegated only");
  Click(window, UiOpenDiagnostics);
  Check(recorder.calls.back().action == UiAction::OpenDiagnostics, "open-diagnostics delegated only");
  SelectProfile(window, 2);
  Check(Text(window, UiProfiles).find(L"legacy: import required") != std::wstring::npos,
        "legacy selection explicitly inspection-only");
  Check(!IsWindowEnabled(GetDlgItem(window, UiPlay)), "legacy Play disabled");
  Check(IsWindowEnabled(GetDlgItem(window, UiRenameLabel)), "legacy library label may be explicitly renamed");
  Check(!IsWindowEnabled(GetDlgItem(window, UiInGameName)) &&
        !IsWindowEnabled(GetDlgItem(window, UiRenameInGameName)) && Text(window, UiInGameName).empty(),
        "legacy source in-game name cannot be mutated");
  Check(!IsWindowEnabled(GetDlgItem(window, UiDuplicateProfile)) &&
        !IsWindowEnabled(GetDlgItem(window, UiRestoreSnapshot)), "legacy profile cannot be copied or restored as managed source");
  const size_t before = recorder.calls.size();
  Click(window, UiPlay);
  Click(window, UiRenameInGameName);
  Click(window, UiDuplicateProfile);
  Click(window, UiRestoreSnapshot);
  Check(recorder.calls.size() == before, "injected legacy Play blocked");
  SendMessageW(window, WM_CLOSE, 0, 0);
  Check(!IsWindow(window), "close destroys hidden window");
  Check(recorder.calls.size() == before, "close has no launch or stop callback");
}

void TestRunning() {
  Recorder recorder;
  recorder.model.running = true;
  InvisibleWindow window(recorder);
  if (!window) return;
  CheckMutationControls(window, false);
  const size_t before = recorder.calls.size();
  for (int id : {UiSaveSettings, UiPlay, UiRegister, UiCreate, UiImport,
       UiRenameLabel, UiRenameInGameName, UiDuplicateProfile, UiRestoreSnapshot, UiVerifyRoots,
       UiBrowseBuild, UiBrowseGame, UiBrowseProfile, UiBrowseLegacy}) Click(window, id);
  SelectProfile(window, 1);
  Check(recorder.calls.size() == before, "injected commands cannot bypass running guard");
  Check(recorder.model.library.selected_id == kMainId, "running backend selection immutable");
  Check(IsWindowEnabled(GetDlgItem(window, UiOpenSave)) &&
        IsWindowEnabled(GetDlgItem(window, UiOpenDiagnostics)), "read-only folder actions stay available");
  // No real timer/message loop is run: this delivers exactly one synthetic tick.
  SendMessageW(window, WM_TIMER, 1, 0);
  Check(recorder.calls.size() == before + 1 && recorder.calls.back().action == UiAction::Refresh,
        "one timer tick refreshes authoritative state");
  Check(Text(window, UiProfiles) == L"MAIN", "authoritative refresh restores exact selection");
  const size_t close_before = recorder.calls.size();
  SendMessageW(window, WM_CLOSE, 0, 0);
  Check(!IsWindow(window) && recorder.calls.size() == close_before,
        "close while running neither stops nor relaunches child");
}

void TestMissingSelectionAndGpu() {
  Recorder recorder;
  recorder.model.library.selected_id = "4234567890abcdef1234567890abcdef";
  recorder.model.gpu_name = L"Unavailable old GPU";
  InvisibleWindow window(recorder);
  if (!window) return;
  Check(SendDlgItemMessageW(window, UiProfiles, CB_GETCURSEL, 0, 0) == CB_ERR,
        "missing profile never falls back to first/newest");
  Check(Text(window, UiSelectedRoot) == L"No profile selected", "missing profile explicit status");
  Check(!IsWindowEnabled(GetDlgItem(window, UiPlay)), "missing profile cannot launch");
  Check(!IsWindowEnabled(GetDlgItem(window, UiRenameLabel)) &&
        !IsWindowEnabled(GetDlgItem(window, UiRenameInGameName)) &&
        !IsWindowEnabled(GetDlgItem(window, UiInGameName)) && Text(window, UiInGameName).empty(),
        "missing selection hides stale in-game name and disables both renames");
  Check(!IsWindowEnabled(GetDlgItem(window, UiDuplicateProfile)) &&
        !IsWindowEnabled(GetDlgItem(window, UiRestoreSnapshot)), "copy and restore require selected managed source");
  Check(!IsWindowEnabled(GetDlgItem(window, UiOpenSave)) &&
        !IsWindowEnabled(GetDlgItem(window, UiOpenDiagnostics)), "no selected profile means no folder action");
  Check(SendDlgItemMessageW(window, UiGpu, CB_GETCURSEL, 0, 0) == CB_ERR,
        "missing named GPU never silently falls back");
  const size_t before = recorder.calls.size();
  Click(window, UiPlay);
  Click(window, UiOpenSave);
  Click(window, UiOpenDiagnostics);
  Click(window, UiRenameLabel);
  Click(window, UiRenameInGameName);
  Click(window, UiDuplicateProfile);
  Click(window, UiRestoreSnapshot);
  Check(recorder.calls.size() == before, "injected missing-selection actions refused");
  Check(IsWindowEnabled(GetDlgItem(window, UiVerifyRoots)),
        "idle build/game verification does not require selecting a save profile");
  recorder.model.status = L"Synthetic identity from verification callback: exact-build-id; no game launch.";
  Edit(window, UiBuildRoot, L"C:/ExplicitVerifyBuild");
  Edit(window, UiGameRoot, L"E:/ExplicitVerifyGame");
  Click(window, UiVerifyRoots);
  Check(recorder.calls.size() == before + 1 && recorder.calls.back().action == UiAction::VerifyRoots &&
        recorder.calls.back().request.build_root == L"C:/ExplicitVerifyBuild" &&
        recorder.calls.back().request.game_root == L"E:/ExplicitVerifyGame",
        "verification submits current unsaved roots exactly once without launch");
  Check(Text(window, UiStatus) == recorder.model.status,
        "verification result identity/status comes only from backend");
  SelectProfile(window, 1);
  Check(recorder.model.library.selected_id == kCleanId &&
        Text(window, UiSelectedRoot) == L"D:/SyntheticSaves/Clean", "explicit selection repairs stale ID");
}

void TestExceptions(bool unknown) {
  Recorder recorder;
  InvisibleWindow window(recorder);
  if (!window) return;
  recorder.throw_standard = !unknown;
  recorder.throw_unknown = unknown;
  Click(window, UiSaveSettings);
  CheckMutationControls(window, false);
  Check(Text(window, UiStatus).find(L"error") != std::wstring::npos, "exception status is an error not success");
  Check(Text(window, UiStatus).find(L"DO_NOT_LEAK_EXCEPTION_BODY") == std::wstring::npos,
        "arbitrary exception text not disclosed");
  const size_t before = recorder.calls.size();
  Click(window, UiPlay);
  Click(window, UiCreate);
  Click(window, UiRenameLabel);
  Click(window, UiRenameInGameName);
  Click(window, UiDuplicateProfile);
  Click(window, UiRestoreSnapshot);
  Click(window, UiVerifyRoots);
  SelectProfile(window, 1);
  Check(recorder.calls.size() == before, "exception fail-closes subsequent mutating actions");
  recorder.throw_standard = recorder.throw_unknown = false;
  SendMessageW(window, WM_TIMER, 1, 0);
  Check(recorder.calls.back().action == UiAction::Refresh, "recovery requires authoritative refresh");
  CheckMutationControls(window, true);
}

void TestSmallViewport() {
  Recorder recorder;
  InvisibleWindow window(recorder);
  if (!window) return;
  RECT initial{};
  GetWindowRect(window, &initial);
  MONITORINFO monitor{sizeof(monitor)};
  Check(GetMonitorInfoW(MonitorFromWindow(window, MONITOR_DEFAULTTOPRIMARY), &monitor) != FALSE,
        "hidden window monitor work area available");
  Check(initial.left >= monitor.rcWork.left && initial.top >= monitor.rcWork.top &&
        initial.right <= monitor.rcWork.right && initial.bottom <= monitor.rcWork.bottom,
        "initial launcher outer bounds fit work area");
  const UINT dpi = GetDpiForSystem();
  SetWindowPos(window, nullptr, 0, 0, MulDiv(500, dpi, 96), MulDiv(350, dpi, 96),
               SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
  Check(!IsWindowVisible(window), "small viewport test remains invisible");
  SCROLLINFO vertical{sizeof(vertical), SIF_ALL};
  SCROLLINFO horizontal{sizeof(horizontal), SIF_ALL};
  GetScrollInfo(window, SB_VERT, &vertical);
  GetScrollInfo(window, SB_HORZ, &horizontal);
  Check(vertical.nMax + 1 > static_cast<int>(vertical.nPage), "small viewport has vertical scroll range");
  Check(horizontal.nMax + 1 > static_cast<int>(horizontal.nPage), "small viewport has horizontal scroll range");
  const size_t calls = recorder.calls.size();
  // This is the notification generated by keyboard Tab entering a button.
  // No focus, key, mouse, desktop screenshot or foreground UI is used here.
  SendMessageW(window, WM_COMMAND, MAKEWPARAM(UiPlay, BN_SETFOCUS),
               reinterpret_cast<LPARAM>(GetDlgItem(window, UiPlay)));
  RECT client{}, button{};
  GetClientRect(window, &client);
  GetWindowRect(GetDlgItem(window, UiPlay), &button);
  MapWindowPoints(nullptr, window, reinterpret_cast<POINT *>(&button), 2);
  Check(button.left >= 0 && button.right <= client.right && button.top >= 0 && button.bottom <= client.bottom,
        "keyboard-focus notification reveals launch button in small viewport");
  Check(recorder.calls.size() == calls, "focus scrolling performs no launcher action");
  // The status box is wider than this deliberately tiny viewport; vertical
  // accessibility is required even when horizontal scrolling is needed.
  SendMessageW(window, WM_COMMAND, MAKEWPARAM(UiStatus, EN_SETFOCUS),
               reinterpret_cast<LPARAM>(GetDlgItem(window, UiStatus)));
  RECT status{};
  GetWindowRect(GetDlgItem(window, UiStatus), &status);
  MapWindowPoints(nullptr, window, reinterpret_cast<POINT *>(&status), 2);
  Check(status.top >= 0 && status.bottom <= client.bottom, "status remains reachable below new rename rows");
  SendMessageW(window, WM_VSCROLL, SB_TOP, 0);
  SendMessageW(window, WM_HSCROLL, SB_TOP, 0);
  Check(GetScrollPos(window, SB_VERT) == 0 && GetScrollPos(window, SB_HORZ) == 0,
        "scroll controls return to top-left");
  HWND tab = GetNextDlgTabItem(window, nullptr, FALSE);
  bool found_play = false, found_profile = false, found_label = false, found_name = false;
  bool found_duplicate = false, found_restore = false, found_verify = false;
  for (int i = 0; tab && i < 40; ++i) {
    found_play |= tab == GetDlgItem(window, UiPlay);
    found_profile |= tab == GetDlgItem(window, UiProfiles);
    found_label |= tab == GetDlgItem(window, UiRenameLabel);
    found_name |= tab == GetDlgItem(window, UiRenameInGameName);
    found_duplicate |= tab == GetDlgItem(window, UiDuplicateProfile);
    found_restore |= tab == GetDlgItem(window, UiRestoreSnapshot);
    found_verify |= tab == GetDlgItem(window, UiVerifyRoots);
    tab = GetNextDlgTabItem(window, tab, FALSE);
  }
  Check(found_play && found_profile && found_label && found_name && found_duplicate && found_restore && found_verify,
        "keyboard tab chain reaches profile, rename, recovery, verify and launch controls");
  Check(!IsWindowVisible(window), "scrolling never shows hidden fixture");
}

void TestRenameValidation() {
  Recorder recorder;
  InvisibleWindow window(recorder);
  if (!window) return;
  const size_t before = recorder.calls.size();
  for (const wchar_t *invalid : {L"", L"1234567890123456", L"Name\x7f", L"NonASCII\u0406"}) {
    // WM_SETTEXT bypasses the normal edit limit: handler must still reject it.
    Edit(window, UiInGameName, invalid);
    Click(window, UiRenameInGameName);
    Check(recorder.calls.size() == before, "invalid injected in-game name cannot reach backend");
    Check(Text(window, UiStatus).find(L"No action was submitted") != std::wstring::npos,
          "invalid name explains no mutation occurred");
  }
  Edit(window, UiInGameName, L"123456789012345");
  Click(window, UiRenameInGameName);
  Check(recorder.calls.size() == before + 1 &&
        recorder.calls.back().action == UiAction::RenameInGameName &&
        recorder.calls.back().request.in_game_name == L"123456789012345",
        "15 printable ASCII characters dispatch exact explicit rename");
  // Programmatic combo changes without an accepted SelectProfile must not
  // cause either rename to target another profile implicitly.
  SendDlgItemMessageW(window, UiProfiles, CB_SETCURSEL, 1, 0);
  Edit(window, UiProfileLabel, L"Unexpected target");
  Edit(window, UiInGameName, L"Unexpected");
  Click(window, UiRenameLabel);
  Click(window, UiRenameInGameName);
  Click(window, UiDuplicateProfile);
  Click(window, UiRestoreSnapshot);
  Check(recorder.calls.size() == before + 1, "rename and recovery refuse selection not acknowledged by backend");
}
} // namespace

int main() {
  TestInitialAndRequests();
  TestRunning();
  TestMissingSelectionAndGpu();
  TestExceptions(false);
  TestExceptions(true);
  TestSmallViewport();
  TestRenameValidation();
  std::cout << checks << " checks, " << failures << " failures\n";
  return failures ? 1 : 0;
}
