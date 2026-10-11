#include "mcla_launcher_ui_win.h"

#include <iostream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>
#include <future>
#include <atomic>
#include <memory>
#include <thread>

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

void RawClick(HWND window, int id) {
  // Direct parent-message injection intentionally tests handler guards, not
  // only the normal disabled-button behavior. No keyboard/mouse is injected.
  SendMessageW(window, WM_COMMAND, MAKEWPARAM(id, BN_CLICKED),
               reinterpret_cast<LPARAM>(GetDlgItem(window, id)));
}

void DrainCheck(HWND window) {
  const auto deadline = GetTickCount64() + 5000;
  while (IsWindow(window) && PollLauncherCheckForTest(window)) {
    if (GetTickCount64() >= deadline) { Check(false, "synthetic check completes within bounded test wait"); return; }
    std::this_thread::yield();
  }
}

void Click(HWND window, int id) {
  RawClick(window, id);
  if (id == UiCheckLaunch) DrainCheck(window);
}

HWND Operation(HWND owner) {
  struct Search { HWND owner; HWND result = nullptr; } search{owner};
  EnumThreadWindows(GetCurrentThreadId(), [](HWND item, LPARAM value) -> BOOL {
    auto &search = *reinterpret_cast<Search *>(value);
    wchar_t name[64]{};
    GetClassNameW(item, name, 64);
    if (GetWindow(item, GW_OWNER) == search.owner &&
        std::wstring_view(name) == L"MclaLauncherProfileOperationV1") {
      search.result = item; return FALSE;
    }
    return TRUE;
  }, reinterpret_cast<LPARAM>(&search));
  return search.result;
}

bool IsOperationButton(int id) {
  return id == UiRenameLabel || id == UiRegister || id == UiCreate ||
      id == UiImport || id == UiDuplicateProfile || id == UiRestoreSnapshot;
}

void Submit(HWND window, int id) {
  if (!IsOperationButton(id)) { Click(window, id); return; }
  Click(window, id);
  HWND dialog = Operation(window);
  if (!dialog) return; // Disabled/missing-selection negative cases stay blocked.
  SetWindowTextW(GetDlgItem(dialog, UiProfileLabel), L"My named fixture");
  if (GetDlgItem(dialog, UiProfileRoot))
    SetWindowTextW(GetDlgItem(dialog, UiProfileRoot), L"D:/ExplicitDestination");
  if (GetDlgItem(dialog, UiLegacySource))
    SetWindowTextW(GetDlgItem(dialog, UiLegacySource), L"E:/ExplicitSource/user");
  Click(dialog, UiOperationConfirm);
}

bool Shown(HWND window, int id) {
  // Parent is deliberately hidden; inspect the control's own visibility bit.
  return (GetWindowLongPtrW(GetDlgItem(window, id), GWL_STYLE) & WS_VISIBLE) != 0;
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
  Edit(window, UiInGameName, L"RoadDriver");
  SendDlgItemMessageW(window, UiGpu, CB_SETCURSEL, 0, 0);
  SendDlgItemMessageW(window, UiScale, CB_SETCURSEL, 1, 0);
  SendDlgItemMessageW(window, UiVsync, BM_SETCHECK, BST_UNCHECKED, 0);
  SendDlgItemMessageW(window, UiFullscreen, BM_SETCHECK, BST_CHECKED, 0);
}

void CheckRequest(const UiRequest &request, UiAction action) {
  Check(request.build_root == L"C:/SelectedBuild", "exact requested build");
  Check(request.game_root == L"F:/SelectedGame", "exact requested game");
  Check(request.gpu_name == L"Synthetic integrated GPU", "explicit named GPU collected");
  Check(request.resolution_scale == 2, "2x selection collected");
  Check(!request.vsync, "VSync false collected");
  Check(request.fullscreen, "fullscreen true collected");
  Check(request.selected_profile_id == kCleanId, "explicit selected opaque profile ID");
  const bool operation = action == UiAction::RenameLabel || action == UiAction::RegisterManaged ||
      action == UiAction::CreateProfile || action == UiAction::ImportLegacy ||
      action == UiAction::DuplicateProfile || action == UiAction::RestoreSnapshot;
  Check(request.profile_label == (operation ? L"My named fixture" : L""), "only explicit modal provides profile name");
  Check(request.profile_root == (operation && action != UiAction::RenameLabel ? L"D:/ExplicitDestination" : L""),
        "only applicable modal provides explicit destination");
  Check(request.legacy_source_root == (action == UiAction::ImportLegacy || action == UiAction::RestoreSnapshot
        ? L"E:/ExplicitSource/user" : L""), "only applicable modal provides explicit source");
  Check(request.in_game_name == L"RoadDriver", "separate in-game name collected");
}

void CheckMutationControls(HWND window, bool enabled) {
  for (int id : {UiBuildRoot, UiBrowseBuild, UiGameRoot, UiBrowseGame,
       UiProfiles, UiRegister, UiCreate, UiImport,
       UiGpu, UiScale, UiFullscreen, UiSaveSettings, UiPlay,
       UiRenameLabel, UiInGameName, UiRenameInGameName,
       UiDuplicateProfile, UiRestoreSnapshot, UiVerifyRoots, UiCheckLaunch}) {
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
    if (id != UiProfileLabel && id != UiProfileRoot && id != UiBrowseProfile &&
        id != UiLegacySource && id != UiBrowseLegacy)
      Check(GetDlgItem(window, id) != nullptr, "all non-modal stable controls exist");
  Check(Text(window, UiBuildRoot) == recorder.model.build_root, "initial build path");
  Check(Text(window, UiGameRoot) == recorder.model.game_root, "initial game path");
  Check(Text(window, UiProfiles) == L"MAIN", "explicit MAIN label shown");
  Check(Text(window, UiSelectedRoot) == L"D:/SyntheticSaves/MAIN", "selected full path shown");
  Check(Text(window, UiInGameName) == L"User", "authoritative in-game name shown separately");
  Check(SendDlgItemMessageW(window, UiInGameName, EM_GETLIMITTEXT, 0, 0) == 15,
        "in-game edit has 15-character UI limit");
  Check(Text(window, UiRenameLabel) == L"Rename profile..." &&
        Text(window, UiRenameInGameName) == L"Apply in-game name", "rename buttons distinguish label from game name");
  Check(Text(window, UiDuplicateProfile) == L"Copy selected profile..." &&
        Text(window, UiRestoreSnapshot) == L"Restore a snapshot...",
        "copy and recovery buttons open focused dialogs");
  Check(Text(window, UiVerifyRoots) == L"Verify build / game",
        "explicit build/game verification button exists");
  Check((GetWindowLongPtrW(GetDlgItem(window, UiSelectedRoot), GWL_STYLE) & ES_READONLY) != 0,
        "selected full path read-only");
  Check(Text(window, UiStatus) == recorder.model.status, "backend status copied without success inference");
  Check(!GetDlgItem(window, UiProfileLabel) && !GetDlgItem(window, UiProfileRoot) &&
        !GetDlgItem(window, UiLegacySource), "operation fields are absent from permanent view");
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
    Submit(window, id);
    Check(recorder.calls.size() == before + 1 && recorder.calls.back().action == action,
          "one exact callback per explicit button");
    CheckRequest(recorder.calls.back().request, action);
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

void TestFixedWindowAndOverflow() {
  Recorder recorder;
  InvisibleWindow window(recorder);
  if (!window) return;
  const UINT dpi = GetDpiForSystem();
  const auto fit = [&](HWND target, int width, int height) {
    RECT bounds{0, 0, MulDiv(width, dpi, 96), MulDiv(height, dpi, 96)};
    const auto style = static_cast<DWORD>(GetWindowLongPtrW(target, GWL_STYLE)) &
        ~(WS_HSCROLL | WS_VSCROLL);
    AdjustWindowRectExForDpi(&bounds, style, FALSE,
        static_cast<DWORD>(GetWindowLongPtrW(target, GWL_EXSTYLE)), dpi);
    SetWindowPos(target, nullptr, 0, 0, bounds.right - bounds.left, bounds.bottom - bounds.top,
                 SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
  };
  const auto bars = [](HWND target) {
    return GetWindowLongPtrW(target, GWL_STYLE) & (WS_HSCROLL | WS_VSCROLL);
  };
  Check((GetWindowLongPtrW(window, GWL_STYLE) & (WS_THICKFRAME | WS_MAXIMIZEBOX)) == 0,
        "launcher has no user resize frame or maximize button");
  fit(window, 900, 690);
  Check(bars(window) == 0, "fitting launcher content has neither scrollbar");
  const auto calls = recorder.calls.size();
  fit(window, 920, 400);
  Check(bars(window) == WS_VSCROLL, "only height overflow shows vertical scrollbar");
  fit(window, 600, 720);
  Check(bars(window) == WS_HSCROLL, "only width overflow shows horizontal scrollbar");
  fit(window, 900, 400);
  Check(bars(window) == (WS_HSCROLL | WS_VSCROLL), "vertical bar can make a tight width overflow too");
  fit(window, 600, 690);
  Check(bars(window) == (WS_HSCROLL | WS_VSCROLL), "horizontal bar can make a tight height overflow too");
  for (int repeat = 0; repeat < 4; ++repeat) {
    RECT client{};
    GetClientRect(window, &client);
    SendMessageW(window, WM_SIZE, SIZE_RESTORED, MAKELPARAM(client.right, client.bottom));
    Check(bars(window) == (WS_HSCROLL | WS_VSCROLL), "overflow scrollbar updates are stable");
  }
  SendMessageW(window, WM_HSCROLL, SB_BOTTOM, 0);
  SendMessageW(window, WM_VSCROLL, SB_BOTTOM, 0);
  fit(window, 900, 690);
  Check(bars(window) == 0 && GetScrollPos(window, SB_HORZ) == 0 && GetScrollPos(window, SB_VERT) == 0,
        "return to fitting size removes both bars and resets offsets");
  for (int repeat = 0; repeat < 4; ++repeat) {
    RECT client{};
    GetClientRect(window, &client);
    SendMessageW(window, WM_SIZE, SIZE_RESTORED, MAKELPARAM(client.right, client.bottom));
    Check(bars(window) == 0, "fitting scrollbar updates remain stable");
  }
  Check(recorder.calls.size() == calls && !IsWindowVisible(window), "geometry changes stay hidden and submit no actions");
  for (int id : {UiRenameLabel, UiCreate, UiRegister, UiImport, UiDuplicateProfile, UiRestoreSnapshot}) {
    Click(window, id);
    const HWND dialog = Operation(window);
    Check(dialog != nullptr, "fixed-size operation dialog exists");
    if (!dialog) continue;
    Check((GetWindowLongPtrW(dialog, GWL_STYLE) & (WS_THICKFRAME | WS_MAXIMIZEBOX)) == 0,
          "operation dialog has no resize frame or maximize button");
    fit(dialog, 700, 500);
    Check(bars(dialog) == 0, "fitting operation dialog has no scrollbars");
    fit(dialog, 380, 250);
    Check(bars(dialog) == (WS_HSCROLL | WS_VSCROLL), "constrained operation still exposes overflowing content");
    fit(dialog, 700, 500);
    Check(bars(dialog) == 0, "operation bars disappear again when content fits");
    Click(dialog, UiOperationCancel);
  }
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
  SetWindowPos(window, nullptr, 0, 0, MulDiv(420, dpi, 96), MulDiv(300, dpi, 96),
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
  bool found_play = false, found_profile = false, found_label = false, found_name = false;
  bool found_duplicate = false, found_restore = false, found_verify = false;
  for (int page : {UiPagePlay, UiPageProfiles, UiPageSetup}) {
    Click(window, page);
    RECT navigation{};
    GetWindowRect(GetDlgItem(window, page), &navigation);
    MapWindowPoints(nullptr, window, reinterpret_cast<POINT *>(&navigation), 2);
    GetClientRect(window, &client);
    Check(navigation.left >= 0 && navigation.right <= client.right &&
          navigation.top >= 0 && navigation.bottom <= client.bottom,
          "selected section navigation remains visible in a small viewport");
    HWND tab = GetNextDlgTabItem(window, nullptr, FALSE);
    for (int i = 0; tab && i < 40; ++i) {
      found_play |= tab == GetDlgItem(window, UiPlay);
      found_profile |= tab == GetDlgItem(window, UiProfiles);
      found_label |= tab == GetDlgItem(window, UiRenameLabel);
      found_name |= tab == GetDlgItem(window, UiRenameInGameName);
      found_duplicate |= tab == GetDlgItem(window, UiDuplicateProfile);
      found_restore |= tab == GetDlgItem(window, UiRestoreSnapshot);
      found_verify |= tab == GetDlgItem(window, UiVerifyRoots);
      Check((GetWindowLongPtrW(tab, GWL_STYLE) & WS_VISIBLE) != 0,
            "keyboard traversal excludes hidden sections");
      tab = GetNextDlgTabItem(window, tab, FALSE);
    }
  }
  Check(found_play && found_profile && found_label && found_name && found_duplicate && found_restore && found_verify,
        "keyboard tab chain reaches profile, rename, recovery, verify and launch controls");
  Check(!IsWindowVisible(window), "scrolling never shows hidden fixture");
}

void TestSections() {
  Recorder recorder;
  InvisibleWindow window(recorder);
  if (!window) return;
  Check(Shown(window, UiGpu) && !Shown(window, UiBuildRoot) &&
        !Shown(window, UiImport), "default Play hides technical setup and profile tools");
  Check(Text(window, UiCurrentName) == L"In-game name: User",
        "Play shows authoritative in-game name");
  Edit(window, UiBuildRoot, L"C:/UnsubmittedSetup");
  Edit(window, UiInGameName, L"PendingName");
  const size_t before = recorder.calls.size();
  for (int page : {UiPageProfiles, UiPageSetup, UiPagePlay}) {
    Click(window, page);
    Check(SendDlgItemMessageW(window, page, BM_GETCHECK, 0, 0) == BST_CHECKED,
          "current native section button has selected state");
    Check(Shown(window, UiProfiles) && Shown(window, UiSelectedRoot) &&
          Shown(window, UiPlay) && Shown(window, UiStatus),
          "profile identity launch and status remain visible on each section");
    Check(Shown(window, UiGpu) == (page == UiPagePlay) &&
          Shown(window, UiImport) == (page == UiPageProfiles) &&
          Shown(window, UiBuildRoot) == (page == UiPageSetup),
          "only controls for current section are shown");
    Check(Text(window, UiBuildRoot) == L"C:/UnsubmittedSetup" &&
          Text(window, UiInGameName) == L"PendingName",
          "section switches preserve setup and profile-tool drafts");
  }
  Check(recorder.calls.size() == before, "section navigation performs no backend action");
  SendMessageW(window, WM_VSCROLL, SB_BOTTOM, 0);
  Click(window, UiPageSetup);
  Check(GetScrollPos(window, SB_VERT) == 0 && GetScrollPos(window, SB_HORZ) == 0,
        "section navigation restores top-left including hidden controls");
  RECT root{};
  GetWindowRect(GetDlgItem(window, UiBuildRoot), &root);
  MapWindowPoints(nullptr, window, reinterpret_cast<POINT *>(&root), 2);
  Check(root.top == MulDiv(322, GetDpiForSystem(), 96),
        "hidden setup controls retain original coordinates after scroll and switch");
  recorder.model.running = true;
  SendMessageW(window, WM_TIMER, 1, 0);
  const size_t running_calls = recorder.calls.size();
  Click(window, UiPageProfiles);
  Check(Shown(window, UiImport) && !IsWindowEnabled(GetDlgItem(window, UiImport)),
        "running session may inspect sections but mutations stay disabled");
  Check(recorder.calls.size() == running_calls, "navigation while running never refreshes or mutates");
}

void TestNavigationKeys() {
  Recorder recorder;
  InvisibleWindow window(recorder);
  if (!window) return;
  Edit(window, UiBuildRoot, L"C:/KeyboardDraft");
  Edit(window, UiInGameName, L"KeyboardName");
  const size_t before = recorder.calls.size();
  for (int page : {UiPageProfiles, UiPageSetup, UiPagePlay}) {
    Check(HandleLauncherNavigationKey(window, WM_KEYDOWN, VK_TAB, true, false),
          "Ctrl+Tab key-down consumed by shared production handler");
    Check(SendDlgItemMessageW(window, page, BM_GETCHECK, 0, 0) == BST_CHECKED,
          "forward keyboard navigation cycles and wraps");
  }
  for (int page : {UiPageSetup, UiPageProfiles, UiPagePlay}) {
    Check(HandleLauncherNavigationKey(window, WM_KEYDOWN, VK_TAB, true, true),
          "Ctrl+Shift+Tab key-down consumed");
    Check(SendDlgItemMessageW(window, page, BM_GETCHECK, 0, 0) == BST_CHECKED,
          "reverse keyboard navigation cycles and wraps");
  }
  Check(!HandleLauncherNavigationKey(window, WM_KEYUP, VK_TAB, true, false) &&
        !HandleLauncherNavigationKey(window, WM_KEYDOWN, VK_TAB, false, false) &&
        !HandleLauncherNavigationKey(window, WM_KEYDOWN, VK_RETURN, true, false),
        "keyup ordinary Tab and unrelated keys are not intercepted");
  Check(SendDlgItemMessageW(window, UiPagePlay, BM_GETCHECK, 0, 0) == BST_CHECKED &&
        Text(window, UiBuildRoot) == L"C:/KeyboardDraft" &&
        Text(window, UiInGameName) == L"KeyboardName" && recorder.calls.size() == before,
        "keyboard page changes preserve all drafts and dispatch no backend action");
}

void CheckSetupDraft(HWND window) {
  Check(Text(window, UiBuildRoot) == L"C:/SelectedBuild" &&
        Text(window, UiGameRoot) == L"F:/SelectedGame", "both draft roots survive callback");
  Check(Text(window, UiGpu) == L"Synthetic integrated GPU", "exact draft GPU survives callback");
  Check(SendDlgItemMessageW(window, UiScale, CB_GETCURSEL, 0, 0) == 1,
        "draft scale survives callback");
  Check(SendDlgItemMessageW(window, UiVsync, BM_GETCHECK, 0, 0) == BST_CHECKED &&
        !IsWindowEnabled(GetDlgItem(window, UiVsync)), "repaired stock VSync On survives old Off preferences");
  Check(SendDlgItemMessageW(window, UiFullscreen, BM_GETCHECK, 0, 0) == BST_CHECKED,
        "draft fullscreen survives callback");
}

void TestDraftCallbackMatrix() {
  Recorder recorder;
  recorder.model.vsync = false; // Historical preferences may need explicit repair.
  InvisibleWindow window(recorder);
  if (!window) return;
  SetRequestFields(window);
  SendDlgItemMessageW(window, UiVsync, BM_SETCHECK, BST_CHECKED, 0);
  Click(window, UiVsync);
  Check(!IsWindowEnabled(GetDlgItem(window, UiVsync)), "repair immediately disables unsupported Off toggle");
  for (int id : {UiImport, UiVerifyRoots, UiSaveSettings, UiPlay}) {
    // Save/Play deliberately return old preferences and a failure status.
    recorder.model.status = L"Synthetic action failed; stored preferences unchanged.";
    Submit(window, id);
    CheckSetupDraft(window);
    Check(recorder.calls.back().request.vsync, "explicit action submits repaired On, not stored Off");
  }
  recorder.model.in_game_name = L"OtherDriver";
  SelectProfile(window, 1);
  CheckSetupDraft(window);
  Check(Text(window, UiProfiles) == L"Clean" &&
        Text(window, UiSelectedRoot) == L"D:/SyntheticSaves/Clean" &&
        Text(window, UiInGameName) == L"OtherDriver",
        "preserving setup drafts never masks authoritative selected profile and name");
  recorder.model.gpu_names = {L"Synthetic discrete GPU", L"Synthetic integrated GPU"};
  SendMessageW(window, WM_TIMER, 1, 0);
  CheckSetupDraft(window);
  Check(SendDlgItemMessageW(window, UiGpu, CB_GETCURSEL, 0, 0) == 1,
        "GPU inventory reorder preserves exact name, not ordinal");
  recorder.throw_standard = true;
  Click(window, UiSaveSettings);
  Check(!IsWindowEnabled(GetDlgItem(window, UiPlay)), "exception blocks mutations while retaining drafts");
  CheckSetupDraft(window);
  recorder.throw_standard = false;
  SendMessageW(window, WM_TIMER, 1, 0);
  CheckSetupDraft(window);
  Check(IsWindowEnabled(GetDlgItem(window, UiPlay)), "authoritative refresh recovers exception state");
  recorder.model.running = true;
  Click(window, UiPlay);
  CheckMutationControls(window, false);
  CheckSetupDraft(window);
  recorder.model.running = false;
  recorder.model.gpu_names = {L"Synthetic discrete GPU"};
  SendMessageW(window, WM_TIMER, 1, 0);
  Check(SendDlgItemMessageW(window, UiGpu, CB_GETCURSEL, 0, 0) == CB_ERR &&
        Text(window, UiGpu).empty(), "removed draft GPU never falls back to another adapter");
  Click(window, UiVerifyRoots);
  Check(recorder.calls.back().request.gpu_name.empty(),
        "missing GPU remains explicit empty selection for backend validation");
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

void TestImportPreservesUnsavedSetup() {
  UiState stored = Model();
  stored.build_root.clear();
  stored.game_root.clear();
  std::vector<Call> calls;
  HWND window = CreateLauncherUiForTest([&](UiAction action, const UiRequest &request) {
    calls.push_back({action, request});
    // Import changes the profile library, not persisted launch settings. This
    // is the production callback contract, with no filesystem/game involved.
    if (action == UiAction::ImportLegacy) {
      stored.library.entries.push_back({"4234567890abcdef1234567890abcdef",
          u"Imported MAIN", u"D:/SyntheticSaves/Imported", Layout::ManagedExternalV1});
      stored.status = L"Imported; select the new profile explicitly.";
    }
    if (action == UiAction::SaveSettings) {
      stored.build_root = request.build_root;
      stored.game_root = request.game_root;
    }
    return stored;
  });
  Check(window != nullptr, "draft regression hidden window created");
  if (!window) return;
  Edit(window, UiBuildRoot, L"C:/DraftBuild");
  Edit(window, UiGameRoot, L"F:/DraftGame");
  Submit(window, UiImport);
  Check(calls.size() == 2 && calls.back().action == UiAction::ImportLegacy,
        "import submits exactly one explicit callback");
  Check(stored.build_root.empty() && stored.game_root.empty(),
        "import never implicitly persists setup drafts");
  Check(Text(window, UiBuildRoot) == L"C:/DraftBuild" &&
        Text(window, UiGameRoot) == L"F:/DraftGame",
        "import must not clear unsaved build and game folders");
  Check(SendDlgItemMessageW(window, UiProfiles, CB_GETCOUNT, 0, 0) == 4 &&
        Text(window, UiProfiles) == L"MAIN",
        "draft preservation does not mask authoritative library or auto-select import");
  Click(window, UiVerifyRoots);
  Check(calls.back().action == UiAction::VerifyRoots &&
        calls.back().request.build_root == L"C:/DraftBuild" &&
        calls.back().request.game_root == L"F:/DraftGame",
        "verify after import receives the still-unsaved exact roots");
  Check(Text(window, UiBuildRoot) == L"C:/DraftBuild" &&
        Text(window, UiGameRoot) == L"F:/DraftGame",
        "read-only verification preserves setup drafts");
  Click(window, UiSaveSettings);
  Check(calls.back().action == UiAction::SaveSettings &&
        stored.build_root == L"C:/DraftBuild" && stored.game_root == L"F:/DraftGame",
        "explicit Save settings persists original roots without re-entry");
  Check(calls.size() == 4, "import verify save sequence has no implicit action");
  DestroyWindow(window);
}

void TestOperationDialogs() {
  Recorder recorder;
  InvisibleWindow window(recorder);
  if (!window) return;
  Edit(window, UiBuildRoot, L"C:/ModalDraftBuild");
  Edit(window, UiGameRoot, L"F:/ModalDraftGame");
  Check(Text(window, UiSelectedProfileName) == L"MAIN", "profile name is explicit and launcher-only");
  for (const auto &[id, action] : std::vector<std::pair<int, UiAction>>{
      {UiRenameLabel, UiAction::RenameLabel}, {UiCreate, UiAction::CreateProfile},
      {UiRegister, UiAction::RegisterManaged}, {UiImport, UiAction::ImportLegacy},
      {UiDuplicateProfile, UiAction::DuplicateProfile}, {UiRestoreSnapshot, UiAction::RestoreSnapshot}}) {
    size_t before = recorder.calls.size();
    Click(window, id);
    HWND dialog = Operation(window);
    Check(dialog && !IsWindowVisible(dialog), "operation creates only an owned hidden test dialog");
    if (!dialog) continue;
    Check(!IsWindowEnabled(window), "operation disables parent until completion or cancel");
    Check(GetDlgItem(dialog, UiProfileLabel) != nullptr, "operation requests launcher profile name");
    Check((GetDlgItem(dialog, UiProfileRoot) != nullptr) == (id != UiRenameLabel),
          "rename does not request destination; other operations do");
    Check((GetDlgItem(dialog, UiLegacySource) != nullptr) == (id == UiImport || id == UiRestoreSnapshot),
          "source field exists only for import and restore");
    Check(recorder.calls.size() == before, "opening form performs no backend action");
    Click(window, UiSaveSettings);
    Click(window, UiPlay);
    Click(window, UiPageSetup);
    Check(!HandleLauncherNavigationKey(window, WM_KEYDOWN, VK_TAB, true, false) &&
          recorder.calls.size() == before, "modal blocks parent commands and page shortcuts");
    Edit(dialog, UiProfileLabel, L"");
    Click(dialog, UiOperationConfirm);
    Check(Operation(window) == dialog && recorder.calls.size() == before &&
          Text(dialog, UiStatus).find(L"No action was submitted") != std::wstring::npos,
          "empty required form stays open without submitting");
    Click(dialog, UiOperationCancel);
    Check(!Operation(window), "Cancel destroys owned operation dialog");
    Check(IsWindowEnabled(window), "Cancel re-enables parent");
    Check(recorder.calls.size() == before, "Cancel performs no callback");
    Submit(window, id);
    Check(!Operation(window) && IsWindowEnabled(window), "Confirm destroys modal and restores parent");
    Check(recorder.calls.size() == before + 1 &&
          recorder.calls.back().action == action, "modal confirm submits exactly one intended action");
    Check(recorder.calls.back().request.build_root == L"C:/ModalDraftBuild" &&
          recorder.calls.back().request.game_root == L"F:/ModalDraftGame",
          "modal submission merges unchanged global setup drafts");
    Check(recorder.model.library.selected_id == kMainId, "modal never invents new selection");
  }
  size_t before = recorder.calls.size();
  Click(window, UiImport);
  HWND dialog = Operation(window);
  Edit(dialog, UiProfileLabel, L"Keep this draft");
  Edit(dialog, UiProfileRoot, L"D:/CancelParent");
  Edit(dialog, UiLegacySource, L"E:/CancelSource/user");
  SendMessageW(dialog, WM_CLOSE, 0, 0);
  Click(window, UiImport);
  dialog = Operation(window);
  Check(Text(dialog, UiProfileLabel) == L"Keep this draft" &&
        Text(dialog, UiProfileRoot) == L"D:/CancelParent" &&
        Text(dialog, UiLegacySource) == L"E:/CancelSource/user" && recorder.calls.size() == before,
        "close/cancel retains only in-memory operation drafts for explicit retry");
  const UINT dpi = GetDpiForSystem();
  SetWindowPos(dialog, nullptr, 0, 0, MulDiv(380, dpi, 96), MulDiv(250, dpi, 96),
               SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
  SendMessageW(dialog, WM_COMMAND, MAKEWPARAM(UiOperationConfirm, BN_SETFOCUS),
               reinterpret_cast<LPARAM>(GetDlgItem(dialog, UiOperationConfirm)));
  RECT client{}, confirm{};
  GetClientRect(dialog, &client);
  GetWindowRect(GetDlgItem(dialog, UiOperationConfirm), &confirm);
  MapWindowPoints(nullptr, dialog, reinterpret_cast<POINT *>(&confirm), 2);
  Check(confirm.left >= 0 && confirm.right <= client.right && confirm.top >= 0 && confirm.bottom <= client.bottom,
        "operation submit reachable at minimum viewport by keyboard focus");
  Click(dialog, IDCANCEL);
  Check(!Operation(window), "dialog Escape command destroys owned modal");
  Check(recorder.calls.size() == before, "dialog Escape performs no callback");
  Click(window, UiDuplicateProfile);
  dialog = Operation(window);
  SendDlgItemMessageW(window, UiProfiles, CB_SETCURSEL, 1, 0);
  Edit(dialog, UiProfileLabel, L"Wrong source");
  Edit(dialog, UiProfileRoot, L"D:/Parent");
  Click(dialog, UiOperationConfirm);
  Check(Operation(window) == dialog && recorder.calls.size() == before,
        "modal refuses unacknowledged source profile change");
  Click(dialog, UiOperationCancel);
  Click(window, UiImport);
  dialog = Operation(window);
  Check(dialog != nullptr, "owned modal present before parent teardown");
  DestroyWindow(window);
  Check(!IsWindow(window) && !IsWindow(dialog) && recorder.calls.size() == before,
        "parent teardown destroys its modal without submitting an operation");
}

void CheckStartColor(HWND window, COLORREF expected, int control = UiPlay) {
  HIGHCONTRASTW contrast{sizeof(contrast)};
  const bool high_contrast = SystemParametersInfoW(SPI_GETHIGHCONTRAST, sizeof(contrast), &contrast, 0) &&
                             (contrast.dwFlags & HCF_HIGHCONTRASTON);
  if (high_contrast) {
    // Do not override the user's system accessibility palette for a test.
    Check(Text(window, control) == (control == UiPlay ? L"START GAME" : L"CHECK SETUP") &&
          !Text(window, UiReadiness).empty(),
          "high-contrast readiness retains native caption and explicit textual state");
    return;
  }
  // Render ONLY this owned synthetic button into a small memory DIB. No
  // desktop/window screenshot, file output, external HWND or input injection.
  HDC dc = CreateCompatibleDC(nullptr);
  BITMAPINFO info{};
  info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
  info.bmiHeader.biWidth = 300;
  info.bmiHeader.biHeight = -80;
  info.bmiHeader.biPlanes = 1;
  info.bmiHeader.biBitCount = 32;
  info.bmiHeader.biCompression = BI_RGB;
  void *pixels = nullptr;
  HBITMAP bitmap = dc ? CreateDIBSection(dc, &info, DIB_RGB_COLORS, &pixels, nullptr, 0) : nullptr;
  Check(dc && bitmap && pixels, "bounded owned-button memory surface allocated");
  if (!dc || !bitmap || !pixels) {
    if (bitmap) DeleteObject(bitmap);
    if (dc) DeleteDC(dc);
    return;
  }
  HGDIOBJ previous = SelectObject(dc, bitmap);
  DRAWITEMSTRUCT draw{};
  draw.CtlType = ODT_BUTTON;
  draw.CtlID = control;
  draw.itemAction = ODA_DRAWENTIRE;
  draw.itemState = IsWindowEnabled(GetDlgItem(window, control)) ? 0 : ODS_DISABLED;
  draw.hwndItem = GetDlgItem(window, control);
  draw.hDC = dc;
  draw.rcItem = {0, 0, 300, 80};
  Check(SendMessageW(window, WM_DRAWITEM, control, reinterpret_cast<LPARAM>(&draw)) == TRUE,
        "owned Start button is painted through production handler");
  GdiFlush();
  Check(GetPixel(dc, 2, 2) == expected, "Start readiness palette matches independently asserted state");
  SelectObject(dc, previous);
  DeleteObject(bitmap);
  DeleteDC(dc);
}

struct ControlledChecks {
  struct Gate {
    std::promise<void> release;
    std::shared_future<void> wait = release.get_future().share();
    UiState result = Model();
    UiRequest request;
    bool throw_error = false;
    bool released = false;
  };
  std::vector<std::unique_ptr<Gate>> gates;
  std::atomic<unsigned> calls{0}, checks{0}, started{0}, active{0};
  std::atomic<bool> concurrent{false}, wrong_thread{false};
  DWORD gui_thread = GetCurrentThreadId();
  UiState initial = Model();
  explicit ControlledChecks(unsigned count) {
    for (unsigned i = 0; i < count; ++i) {
      auto gate = std::make_unique<Gate>();
      gate->result.readiness = LaunchReadiness::Ready;
      gate->result.eligibility_expires_utc = 1800100000;
      gates.push_back(std::move(gate));
    }
  }
  UiState Invoke(UiAction action, const UiRequest &request) {
    ++calls;
    if (active.fetch_add(1)) concurrent = true;
    if (action != UiAction::CheckLaunch) {
      if (GetCurrentThreadId() != gui_thread) wrong_thread = true;
      --active;
      return initial;
    }
    if (GetCurrentThreadId() == gui_thread) wrong_thread = true;
    const unsigned index = checks.fetch_add(1);
    if (index >= gates.size()) { --active; throw std::runtime_error("Unexpected extra check"); }
    auto &gate = *gates[index];
    gate.request = request;
    UiState result = gate.result;
    const bool error = gate.throw_error;
    ++started; // Publishing captured data before the fixture reads it.
    gate.wait.wait();
    --active;
    if (error) throw std::runtime_error("DO_NOT_LEAK_ASYNC_ERROR");
    return result;
  }
  void Release(unsigned index) {
    if (!gates[index]->released) { gates[index]->released = true; gates[index]->release.set_value(); }
  }
  void WaitStarted(unsigned count) {
    const auto deadline = GetTickCount64() + 5000;
    while (started.load() < count && GetTickCount64() < deadline) std::this_thread::yield();
    Check(started.load() >= count, "controlled worker reached its deterministic promise gate");
  }
};

void PumpStartup(HWND window) {
  MSG message{};
  while (PeekMessageW(&message, window, WM_APP, WM_APP + 255, PM_REMOVE)) DispatchMessageW(&message);
}

void TestAutomaticChecks() {
  ControlledChecks fixture(2);
  fixture.gates[1]->result.readiness = LaunchReadiness::Blocked;
  fixture.gates[1]->result.readiness_reason = L"Synthetic expired lease";
  HWND window = CreateLauncherUiForTest([&](UiAction action, const UiRequest &request) {
    return fixture.Invoke(action, request);
  });
  Check(window != nullptr, "automatic check fixture created");
  if (!window) return;
  SetLauncherClockForTest(window, 1000, 1800000000);
  Check(fixture.checks == 0 && fixture.calls == 1, "ordinary hidden fixture never automatically checks");
  EnableLauncherAutoChecksForTest(window);
  Check(fixture.checks == 0, "startup check is posted after initialization, not run inline");
  PumpStartup(window);
  fixture.WaitStarted(1);
  Check(Text(window, UiReadiness).starts_with(L"Checking") &&
        !IsWindowEnabled(GetDlgItem(window, UiPlay)) && !IsWindowEnabled(GetDlgItem(window, UiCheckLaunch)),
        "background check clears green and disables Start/recheck while pending");
  CheckStartColor(window, GetSysColor(COLOR_BTNFACE), UiCheckLaunch);
  fixture.Release(0);
  DrainCheck(window);
  Check(Text(window, UiReadiness).starts_with(L"Ready.") && fixture.checks == 1,
        "automatic startup result becomes ready without another callback");
  CheckStartColor(window, GetSysColor(COLOR_BTNFACE), UiCheckLaunch);
  Click(window, UiPageProfiles);
  Click(window, UiCreate);
  HWND dialog = Operation(window);
  Click(dialog, UiOperationCancel);
  Check(Text(window, UiReadiness).starts_with(L"Ready.") && fixture.calls == 2,
        "navigation and modal open/cancel preserve unexpired readiness");
  SetLauncherClockForTest(window, 61000, 1800000060);
  PollLauncherCheckForTest(window);
  fixture.WaitStarted(2);
  Check(Text(window, UiReadiness).starts_with(L"Checking"), "unchanged aged-out ready starts one automatic renewal");
  fixture.Release(1);
  DrainCheck(window);
  for (int i = 0; i < 4; ++i) PollLauncherCheckForTest(window);
  Check(Text(window, UiReadiness).starts_with(L"Blocked:") && fixture.checks == 2,
        "failed automatic renewal stays blocked without a retry loop");
  CheckStartColor(window, RGB(142, 87, 0), UiCheckLaunch);
  Check(!fixture.concurrent && !fixture.wrong_thread, "all callback access serialized on intended threads");
  DestroyWindow(window);
}

void TestAsyncStaleAndSerialization() {
  ControlledChecks fixture(2);
  HWND window = CreateLauncherUiForTest([&](UiAction action, const UiRequest &request) {
    return fixture.Invoke(action, request);
  });
  Check(window != nullptr, "stale result fixture created");
  if (!window) return;
  SetLauncherClockForTest(window, 1000, 1800000000);
  EnableLauncherAutoChecksForTest(window);
  PumpStartup(window);
  fixture.WaitStarted(1);
  Edit(window, UiBuildRoot, L"C:/EditedDuringCheck");
  Edit(window, UiBuildRoot, L"C:/SyntheticBuild");
  Click(window, UiPageSetup);
  for (int id : {UiCheckLaunch, UiPlay, UiSaveSettings, UiVerifyRoots, UiCreate,
       UiRenameInGameName, UiOpenSave, UiOpenDiagnostics}) RawClick(window, id);
  SendMessageW(window, WM_TIMER, 1, 0);
  Check(fixture.calls == 2 && fixture.checks == 1 && !Operation(window),
        "pending worker blocks every backend action including refresh and folder callbacks");
  fixture.Release(0);
  DrainCheck(window);
  Check(!Text(window, UiReadiness).starts_with(L"Ready.") &&
        Text(window, UiBuildRoot) == L"C:/SyntheticBuild",
        "edit then revert rejects stale result by generation without clobbering draft");
  CheckStartColor(window, RGB(142, 87, 0), UiCheckLaunch);
  for (int i = 0; i < 3; ++i) PollLauncherCheckForTest(window);
  Check(fixture.checks == 1, "dirty result does not automatically recheck user edits");
  RawClick(window, UiCheckLaunch);
  fixture.WaitStarted(2);
  fixture.Release(1);
  DrainCheck(window);
  Check(Text(window, UiReadiness).starts_with(L"Ready.") && !fixture.concurrent && !fixture.wrong_thread,
        "new explicit check follows drained stale check with no out-of-order overlap");
  Edit(window, UiGameRoot, L"F:/ChangedAfterReady");
  SetLauncherClockForTest(window, 90000, 1800000090);
  PollLauncherCheckForTest(window);
  Check(fixture.checks == 2 && !Text(window, UiReadiness).starts_with(L"Ready."),
        "dirty setup suppresses automatic expiry renewal");
  DestroyWindow(window);
}

void TestAsyncCloseAndFailure() {
  ControlledChecks fixture(1);
  HWND window = CreateLauncherUiForTest([&](UiAction action, const UiRequest &request) {
    return fixture.Invoke(action, request);
  });
  Check(window != nullptr, "close-drain fixture created");
  if (!window) return;
  RawClick(window, UiCheckLaunch);
  fixture.WaitStarted(1);
  SendMessageW(window, WM_CLOSE, 0, 0);
  Check(IsWindow(window) && !IsWindowVisible(window) && !IsWindowEnabled(window),
        "close hides window but retains callback owner until worker drains");
  fixture.Release(0);
  DrainCheck(window);
  Check(!IsWindow(window) && fixture.active == 0, "worker drains before actual window destruction");

  ControlledChecks failure(1);
  failure.gates[0]->throw_error = true;
  window = CreateLauncherUiForTest([&](UiAction action, const UiRequest &request) {
    return failure.Invoke(action, request);
  });
  if (!window) { Check(false, "failure fixture created"); return; }
  RawClick(window, UiCheckLaunch);
  failure.WaitStarted(1);
  failure.Release(0);
  DrainCheck(window);
  Check(IsWindowEnabled(GetDlgItem(window, UiCheckLaunch)) &&
        !Text(window, UiReadiness).starts_with(L"Checking") &&
        Text(window, UiStatus).find(L"DO_NOT_LEAK_ASYNC_ERROR") == std::wstring::npos,
        "worker exception leaves retry enabled, sanitized and never permanently checking");
  DestroyWindow(window);

  ControlledChecks destruction(1);
  window = CreateLauncherUiForTest([&](UiAction action, const UiRequest &request) {
    return destruction.Invoke(action, request);
  });
  if (!window) { Check(false, "direct-destroy fixture created"); return; }
  RawClick(window, UiCheckLaunch);
  destruction.WaitStarted(1);
  destruction.Release(0);
  DestroyWindow(window);
  Check(!IsWindow(window) && destruction.active == 0,
        "direct destruction drains released worker even without normal WM_CLOSE");
}

void TestReturnedCheckBinding() {
  for (bool change_root : {true, false}) {
    ControlledChecks fixture(1);
    if (change_root) fixture.gates[0]->result.library.entries[0].root = u"D:/DifferentRootSameId";
    else fixture.gates[0]->result.gpu_names.push_back(L"New inventory entry");
    HWND window = CreateLauncherUiForTest([&](UiAction action, const UiRequest &request) {
      return fixture.Invoke(action, request);
    });
    Check(window != nullptr, "returned binding fixture created");
    if (!window) continue;
    SetLauncherClockForTest(window, 1000, 1800000000);
    RawClick(window, UiCheckLaunch);
    fixture.WaitStarted(1);
    fixture.Release(0);
    DrainCheck(window);
    Check(!Text(window, UiReadiness).starts_with(L"Ready.") &&
          Text(window, UiBuildRoot) == L"C:/SyntheticBuild" &&
          Text(window, UiGameRoot) == L"F:/SyntheticGame",
          "returned root/inventory drift cannot approve different setup or clobber drafts");
    if (change_root)
      Check(Text(window, UiSelectedRoot) == L"D:/DifferentRootSameId",
            "changed authoritative root is displayed but not approved");
    else
      Check(SendDlgItemMessageW(window, UiGpu, CB_GETCOUNT, 0, 0) == 3,
            "changed authoritative inventory is displayed but not approved");
    DestroyWindow(window);
  }
}

void TestLeaseAutomaticRenewal() {
  ControlledChecks fixture(2);
  fixture.gates[0]->result.eligibility_expires_utc = 1800000002;
  HWND window = CreateLauncherUiForTest([&](UiAction action, const UiRequest &request) {
    return fixture.Invoke(action, request);
  });
  Check(window != nullptr, "lease renewal fixture created");
  if (!window) return;
  SetLauncherClockForTest(window, 1000, 1800000000);
  EnableLauncherAutoChecksForTest(window);
  PumpStartup(window);
  fixture.WaitStarted(1);
  fixture.Release(0);
  DrainCheck(window);
  SetLauncherClockForTest(window, 3000, 1800000002);
  PollLauncherCheckForTest(window);
  fixture.WaitStarted(2);
  fixture.Release(1);
  DrainCheck(window);
  Check(fixture.checks == 2 && Text(window, UiReadiness).starts_with(L"Ready."),
        "lease expiry before TTL triggers one automatic renewal of unchanged setup");
  DestroyWindow(window);
}

void TestUnchangedBackendActions() {
  Recorder recorder;
  recorder.model.readiness = LaunchReadiness::Ready;
  recorder.model.eligibility_expires_utc = 1800100000;
  InvisibleWindow window(recorder);
  if (!window) return;
  SetLauncherClockForTest(window, 1000, 1800000000);
  Click(window, UiCheckLaunch);
  for (int id : {UiSaveSettings, UiVerifyRoots}) {
    recorder.model.status = L"Synthetic authoritative action result";
    Click(window, id);
    Check(Text(window, UiReadiness).starts_with(L"Ready.") &&
          Text(window, UiStatus) == recorder.model.status,
          "no-op Save and read-only Verify preserve exact approval and authoritative status");
  }
  recorder.model.status = L"Synthetic Save failed; original preferences retained";
  Click(window, UiSaveSettings);
  Check(Text(window, UiStatus) == recorder.model.status && Text(window, UiReadiness).starts_with(L"Ready."),
        "failed no-op Save is not claimed successful; prior exact readiness is separate");
  recorder.model.build_root = L"C:/DifferentStoredValue";
  Click(window, UiSaveSettings);
  Check(!Text(window, UiReadiness).starts_with(L"Ready.") &&
        Text(window, UiBuildRoot) == L"C:/SyntheticBuild" && Text(window, UiStatus) == recorder.model.status,
        "unconfirmed Save invalidates approval while preserving draft and failure status");
  recorder.model.build_root = L"C:/SyntheticBuild";
  Click(window, UiCheckLaunch);
  recorder.model.verification_failed = true;
  recorder.model.status = L"Synthetic verification failure details";
  Click(window, UiVerifyRoots);
  Check(!Text(window, UiReadiness).starts_with(L"Ready.") &&
        !IsWindowEnabled(GetDlgItem(window, UiPlay)) &&
        IsWindowEnabled(GetDlgItem(window, UiCheckLaunch)) && Text(window, UiStatus) == recorder.model.status,
        "structured failed Verify blocks old readiness and retains diagnostics with explicit retry");
  recorder.model.verification_failed = false;
  Click(window, UiVerifyRoots);
  Check(!Text(window, UiReadiness).starts_with(L"Ready."),
        "successful Verify alone cannot resurrect invalidated full readiness");
  Click(window, UiCheckLaunch);
  Click(window, UiVerifyRoots);
  Check(Text(window, UiReadiness).starts_with(L"Ready."),
        "successful Verify preserves current full check without parsing status text");
}

void TestCreateParentSuggestion() {
  Recorder recorder;
  recorder.model.collection_parent = L"D:/SuggestedCollection";
  InvisibleWindow window(recorder);
  if (!window) return;
  const auto before = recorder.calls.size();
  Click(window, UiCreate);
  HWND dialog = Operation(window);
  Check(Text(dialog, UiProfileRoot) == L"D:/SuggestedCollection",
        "new Create form prefills controller-provided collection parent");
  Edit(dialog, UiProfileRoot, L"E:/MyOwnChoice");
  Click(dialog, UiOperationCancel);
  Click(window, UiCreate);
  dialog = Operation(window);
  Check(Text(dialog, UiProfileRoot) == L"E:/MyOwnChoice", "user's cancelled draft overrides default parent");
  Edit(dialog, UiProfileRoot, L"");
  Click(dialog, UiOperationCancel);
  Click(window, UiCreate);
  dialog = Operation(window);
  Check(Text(dialog, UiProfileRoot).empty(), "deliberately cleared parent remains an explicit draft");
  Click(dialog, UiOperationCancel);
  Check(recorder.calls.size() == before, "suggestion and cancellation perform no filesystem/backend action");
}

void TestLaunchReadiness() {
  Recorder recorder;
  InvisibleWindow window(recorder);
  if (!window) return;
  constexpr uint64_t now = 1800000000;
  SetLauncherClockForTest(window, 1000, now);
  Check(Text(window, UiPlay) == L"START GAME" &&
        Text(window, UiReadiness).find(L"Not checked") != std::wstring::npos,
        "START GAME has native accessible caption and unchecked textual state");
  CheckStartColor(window, RGB(142, 87, 0));
  recorder.model.readiness = LaunchReadiness::Ready;
  recorder.model.eligibility_expires_utc = now + 3600;
  const size_t initial = recorder.calls.size();
  Click(window, UiCheckLaunch);
  Check(recorder.calls.size() == initial + 1 && recorder.calls.back().action == UiAction::CheckLaunch &&
        Text(window, UiReadiness).starts_with(L"Ready.") && IsWindowEnabled(GetDlgItem(window, UiPlay)),
        "only explicit full check establishes ready state");
  CheckStartColor(window, RGB(25, 110, 61));
  Click(window, UiPageProfiles);
  HandleLauncherNavigationKey(window, WM_KEYDOWN, VK_TAB, true, false);
  Check(Text(window, UiReadiness).starts_with(L"Ready.") && recorder.calls.size() == initial + 1,
        "page navigation keeps exact readiness without network/backend request");
  Edit(window, UiBuildRoot, L"C:/ChangedBuild");
  Edit(window, UiBuildRoot, L"C:/SyntheticBuild");
  Check(!Text(window, UiReadiness).starts_with(L"Ready.") && recorder.calls.size() == initial + 1,
        "editing then undoing cannot resurrect readiness or auto-check");
  Click(window, UiCheckLaunch);
  SetLauncherClockForTest(window, 60999, now + 59);
  Check(Text(window, UiReadiness).starts_with(L"Ready."), "readiness remains valid just below60seconds");
  SetLauncherClockForTest(window, 61000, now + 60);
  Check(Text(window, UiReadiness).starts_with(L"Check expired"), "readiness expires exactly at60seconds");
  recorder.model.eligibility_expires_utc = now + 61;
  Click(window, UiCheckLaunch);
  SetLauncherClockForTest(window, 61500, now + 61);
  Check(Text(window, UiReadiness).starts_with(L"Check expired"), "lease UTC expiry wins over60second freshness");
  recorder.model.eligibility_expires_utc = now + 3600;
  Click(window, UiCheckLaunch);
  SetLauncherClockForTest(window, 61499, now + 60);
  Check(Text(window, UiReadiness).starts_with(L"Check expired"), "backward clocks invalidate approval");
  recorder.model.readiness = LaunchReadiness::Blocked;
  recorder.model.readiness_reason = L"Synthetic missing game data";
  Click(window, UiCheckLaunch);
  const size_t blocked = recorder.calls.size();
  Click(window, UiPlay);
  Check(!IsWindowEnabled(GetDlgItem(window, UiPlay)) && recorder.calls.size() == blocked &&
        Text(window, UiReadiness).find(L"Synthetic missing game data") != std::wstring::npos,
        "blocked check disables Start and exposes reason independently of generic status");
  CheckStartColor(window, RGB(142, 87, 0));
  Edit(window, UiGameRoot, L"F:/ChangedGame");
  Check(IsWindowEnabled(GetDlgItem(window, UiPlay)) && !Text(window, UiReadiness).starts_with(L"Ready."),
        "editing blocked setup returns amber unchecked, not green");
  recorder.model.readiness = LaunchReadiness::Ready;
  Click(window, UiCheckLaunch);
  SendDlgItemMessageW(window, UiScale, CB_SETCURSEL, 1, 0);
  SendMessageW(window, WM_COMMAND, MAKEWPARAM(UiScale, CBN_SELCHANGE),
               reinterpret_cast<LPARAM>(GetDlgItem(window, UiScale)));
  Check(!Text(window, UiReadiness).starts_with(L"Ready."), "resolution edit invalidates check");
  Click(window, UiCheckLaunch);
  SelectProfile(window, 1);
  Check(!Text(window, UiReadiness).starts_with(L"Ready."), "profile change invalidates check");
  Click(window, UiCheckLaunch);
  Submit(window, UiRenameLabel);
  Check(!Text(window, UiReadiness).starts_with(L"Ready."), "profile operation invalidates check");
  Click(window, UiCheckLaunch);
  SendMessageW(window, WM_TIMER, 1, 0);
  Check(Text(window, UiReadiness).starts_with(L"Ready."), "idle authoritative refresh preserves exact current approval");
  Click(window, UiCheckLaunch);
  Click(window, UiPlay);
  Check(!IsWindowEnabled(GetDlgItem(window, UiPlay)) && !Text(window, UiReadiness).starts_with(L"Ready."),
        "failed Play cannot leave ready green or permit unchanged blind retry");
  Click(window, UiCheckLaunch);
  recorder.model.running = true;
  Click(window, UiPlay);
  Check(!IsWindowEnabled(GetDlgItem(window, UiPlay)) && !IsWindowEnabled(GetDlgItem(window, UiCheckLaunch)) &&
        Text(window, UiReadiness).starts_with(L"Game running"), "running state disables Start and recheck");
  CheckStartColor(window, RGB(96, 100, 106));
  recorder.model.running = false;
  SendMessageW(window, WM_TIMER, 1, 0);
  Check(!Text(window, UiReadiness).starts_with(L"Ready."), "completed running session requires a fresh check");
}
} // namespace

int main() {
  TestInitialAndRequests();
  TestRunning();
  TestMissingSelectionAndGpu();
  TestExceptions(false);
  TestExceptions(true);
  TestSmallViewport();
  TestFixedWindowAndOverflow();
  TestRenameValidation();
  TestImportPreservesUnsavedSetup();
  TestSections();
  TestNavigationKeys();
  TestDraftCallbackMatrix();
  TestOperationDialogs();
  TestLaunchReadiness();
  TestAutomaticChecks();
  TestAsyncStaleAndSerialization();
  TestAsyncCloseAndFailure();
  TestReturnedCheckBinding();
  TestLeaseAutomaticRenewal();
  TestUnchangedBackendActions();
  TestCreateParentSuggestion();
  std::cout << checks << " checks, " << failures << " failures\n";
  return failures ? 1 : 0;
}
