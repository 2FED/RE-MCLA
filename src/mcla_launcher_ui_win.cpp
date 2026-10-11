#include "mcla_launcher_ui_win.h"

#include <shobjidl.h>
#include <wrl/client.h>

#include <algorithm>
#include <exception>
#include <memory>
#include <utility>

namespace mcla::launcher {
namespace {
constexpr wchar_t kWindowClass[] = L"MclaInternalLauncherV1";
constexpr UINT_PTR kRefreshTimer = 1;
constexpr int kWidth = 960;
constexpr int kHeight = 780;

struct WindowState {
  UiCallback callback;
  UiState model;
  HFONT font = nullptr;
  UINT dpi = 96;
  bool dispatching = false;
  bool testing = false;
  bool window_owned = false;
  int scroll_x = 0;
  int scroll_y = 0;
  int wheel_remainder = 0;
};

std::wstring Wide(std::u16string_view value) {
  return std::wstring(value.begin(), value.end());
}

int Px(const WindowState &state, int value) {
  return MulDiv(value, static_cast<int>(state.dpi), 96);
}

void ScrollTo(HWND window, WindowState &state, int x, int y) {
  SCROLLINFO horizontal{sizeof(horizontal), SIF_POS};
  horizontal.nPos = x;
  SetScrollInfo(window, SB_HORZ, &horizontal, TRUE);
  SCROLLINFO vertical{sizeof(vertical), SIF_POS};
  vertical.nPos = y;
  SetScrollInfo(window, SB_VERT, &vertical, TRUE);
  const int next_x = GetScrollPos(window, SB_HORZ);
  const int next_y = GetScrollPos(window, SB_VERT);
  const int dx = state.scroll_x - next_x;
  const int dy = state.scroll_y - next_y;
  state.scroll_x = next_x;
  state.scroll_y = next_y;
  if (dx || dy) ScrollWindowEx(window, dx, dy, nullptr, nullptr, nullptr, nullptr,
                               SW_SCROLLCHILDREN | SW_INVALIDATE | SW_ERASE);
}

void UpdateScrollbars(HWND window, WindowState &state) {
  RECT client{};
  GetClientRect(window, &client);
  SCROLLINFO info{sizeof(info), SIF_RANGE | SIF_PAGE | SIF_POS | SIF_DISABLENOSCROLL};
  info.nMax = Px(state, kWidth) - 1;
  info.nPage = static_cast<UINT>(std::max(client.right, 0L));
  info.nPos = state.scroll_x;
  SetScrollInfo(window, SB_HORZ, &info, TRUE);
  info.nMax = Px(state, kHeight) - 1;
  info.nPage = static_cast<UINT>(std::max(client.bottom, 0L));
  info.nPos = state.scroll_y;
  SetScrollInfo(window, SB_VERT, &info, TRUE);
  ScrollTo(window, state, GetScrollPos(window, SB_HORZ), GetScrollPos(window, SB_VERT));
}

void RevealControl(HWND window, WindowState &state, HWND control) {
  if (!control || GetParent(control) != window) return;
  RECT client{}, bounds{};
  GetClientRect(window, &client);
  GetWindowRect(control, &bounds);
  MapWindowPoints(nullptr, window, reinterpret_cast<POINT *>(&bounds), 2);
  const int margin = Px(state, 8);
  int x = state.scroll_x, y = state.scroll_y;
  if (bounds.left < margin) x += bounds.left - margin;
  else if (bounds.right > client.right - margin) x += bounds.right - client.right + margin;
  if (bounds.top < margin) y += bounds.top - margin;
  else if (bounds.bottom > client.bottom - margin) y += bounds.bottom - client.bottom + margin;
  ScrollTo(window, state, x, y);
}

void ScrollMessage(HWND window, WindowState &state, bool vertical, WPARAM wparam) {
  const int bar = vertical ? SB_VERT : SB_HORZ;
  SCROLLINFO info{sizeof(info), SIF_ALL};
  GetScrollInfo(window, bar, &info);
  int position = info.nPos;
  const int page = std::max(static_cast<int>(info.nPage) - Px(state, 24), Px(state, 24));
  switch (LOWORD(wparam)) {
    case SB_LINEUP: position -= Px(state, 24); break;
    case SB_LINEDOWN: position += Px(state, 24); break;
    case SB_PAGEUP: position -= page; break;
    case SB_PAGEDOWN: position += page; break;
    case SB_THUMBPOSITION:
    case SB_THUMBTRACK: position = info.nTrackPos; break;
    case SB_TOP: position = 0; break;
    case SB_BOTTOM: position = info.nMax; break;
    default: return;
  }
  ScrollTo(window, state, vertical ? state.scroll_x : position,
           vertical ? position : state.scroll_y);
}

std::wstring Text(HWND window, int id) {
  HWND control = GetDlgItem(window, id);
  const int length = GetWindowTextLengthW(control);
  std::wstring result(static_cast<size_t>(length) + 1, L'\0');
  const int copied = GetWindowTextW(control, result.data(), length + 1);
  result.resize(static_cast<size_t>(std::max(copied, 0)));
  return result;
}

void SetText(HWND window, int id, const std::wstring &text) {
  SetWindowTextW(GetDlgItem(window, id), text.c_str());
}

HWND Add(HWND window, WindowState &state, const wchar_t *kind,
         const wchar_t *text, DWORD style, int id, int x, int y, int w, int h) {
  HWND control = CreateWindowExW(
      kind == std::wstring_view(L"EDIT") ? WS_EX_CLIENTEDGE : 0,
      kind, text, WS_CHILD | WS_VISIBLE | style,
      Px(state, x), Px(state, y), Px(state, w), Px(state, h), window,
      reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)),
      GetModuleHandleW(nullptr), nullptr);
  if (control) SendMessageW(control, WM_SETFONT,
                            reinterpret_cast<WPARAM>(state.font), TRUE);
  return control;
}

void Controls(HWND window, WindowState &state) {
  state.font = CreateFontW(-Px(state, 15), 0, 0, 0, FW_NORMAL, FALSE, FALSE,
                          FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
                          CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                          DEFAULT_PITCH, L"Segoe UI");
  auto label = [&](const wchar_t *text, int x, int y, int w, int h = 24) {
    Add(window, state, L"STATIC", text, 0, 0, x, y, w, h);
  };
  auto button = [&](int id, const wchar_t *text, int x, int y, int w) {
    Add(window, state, L"BUTTON", text, WS_TABSTOP | BS_PUSHBUTTON | BS_NOTIFY,
        id, x, y, w, 30);
  };
  auto edit = [&](int id, int x, int y, int w, bool read_only = false) {
    HWND control = Add(window, state, L"EDIT", L"",
        WS_TABSTOP | ES_AUTOHSCROLL | (read_only ? ES_READONLY : 0),
        id, x, y, w, 28);
    SendMessageW(control, EM_SETLIMITTEXT,
                 id == UiInGameName ? 15 :
                 id == UiProfileLabel ? kMaximumLabelUnits : kMaximumPathUnits, 0);
  };
  label(L"MCLA-R internal launcher", 20, 16, 900);
  label(L"Choose exact folders. Switching profiles never copies or merges saves.",
        20, 44, 900);
  label(L"Build folder", 20, 80, 130);
  edit(UiBuildRoot, 155, 76, 665);
  button(UiBrowseBuild, L"Browse...", 830, 75, 105);
  label(L"Game-data folder", 20, 116, 135);
  edit(UiGameRoot, 155, 112, 665);
  button(UiBrowseGame, L"Browse...", 830, 111, 105);
  label(L"Save profile", 20, 150, 130);
  Add(window, state, L"COMBOBOX", L"", WS_TABSTOP | CBS_DROPDOWNLIST |
      WS_VSCROLL, UiProfiles, 155, 146, 780, 200);
  label(L"Selected full path", 20, 184, 135);
  edit(UiSelectedRoot, 155, 180, 780, true);
  button(UiOpenSave, L"Open save folder", 155, 214, 190);
  button(UiOpenDiagnostics, L"Open diagnostics", 355, 214, 190);
  label(L"Library label", 20, 250, 130);
  edit(UiProfileLabel, 155, 246, 510);
  button(UiRenameLabel, L"Rename selected label", 675, 245, 260);
  label(L"In-game name", 20, 286, 130);
  edit(UiInGameName, 155, 282, 220);
  button(UiRenameInGameName, L"Apply in-game name", 385, 281, 230);
  label(L"1-15 printable ASCII characters", 625, 286, 310);
  label(L"New profiles use the library label and destination PARENT. Register uses an existing managed root.", 20, 322, 915);
  label(L"Destination / parent", 20, 357, 135);
  edit(UiProfileRoot, 155, 353, 665);
  button(UiBrowseProfile, L"Browse...", 830, 352, 105);
  label(L"Import/snapshot source", 20, 382, 130, 36);
  edit(UiLegacySource, 155, 386, 665);
  button(UiBrowseLegacy, L"Browse...", 830, 385, 105);
  button(UiRegister, L"Register managed folder", 155, 421, 230);
  button(UiCreate, L"Create empty profile", 395, 421, 220);
  button(UiImport, L"Import legacy (copy)", 625, 421, 230);
  button(UiDuplicateProfile, L"Copy selected to NEW profile", 155, 455, 340);
  button(UiRestoreSnapshot, L"Restore snapshot as NEW profile", 505, 455, 360);
  label(L"Copy/restore: original retained; no overwrite and no automatic selection. Import also retains its source.\nRestore: choose a snapshot folder belonging to the selected managed profile; validation is required.",
        20, 493, 915, 48);
  label(L"GPU", 20, 566, 130);
  Add(window, state, L"COMBOBOX", L"", WS_TABSTOP | CBS_DROPDOWNLIST |
      WS_VSCROLL, UiGpu, 155, 562, 780, 200);
  label(L"Target scale", 20, 601, 130);
  HWND scale = Add(window, state, L"COMBOBOX", L"", WS_TABSTOP |
      CBS_DROPDOWNLIST, UiScale, 155, 597, 150, 100);
  SendMessageW(scale, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"1x"));
  SendMessageW(scale, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"2x"));
  Add(window, state, L"BUTTON", L"VSync (stock: ON)", WS_TABSTOP | BS_AUTOCHECKBOX | BS_NOTIFY,
      UiVsync, 325, 597, 180, 28);
  Add(window, state, L"BUTTON", L"Fullscreen", WS_TABSTOP | BS_AUTOCHECKBOX | BS_NOTIFY,
      UiFullscreen, 500, 597, 160, 28);
  label(L"Timing", 20, 635, 130);
  Add(window, state, L"STATIC", L"Stock only. 60 FPS unavailable: not yet validated.",
      0, UiTiming, 155, 631, 780, 28);
  button(UiSaveSettings, L"Save settings", 155, 665, 190);
  button(UiVerifyRoots, L"Verify build / game", 355, 665, 230);
  button(UiPlay, L"Launch selected profile", 595, 665, 260);
  label(L"Status", 20, 705, 130);
  Add(window, state, L"EDIT", L"", WS_TABSTOP | ES_READONLY |
      ES_MULTILINE | ES_AUTOVSCROLL | WS_VSCROLL, UiStatus,
      155, 705, 780, 45);
  label(L"Closing this launcher does not stop a running game.", 20, 756, 910);
}

UiRequest Request(HWND window, const WindowState &state) {
  UiRequest request;
  request.build_root = Text(window, UiBuildRoot);
  request.game_root = Text(window, UiGameRoot);
  request.gpu_name = Text(window, UiGpu);
  request.resolution_scale = SendDlgItemMessageW(window, UiScale,
      CB_GETCURSEL, 0, 0) == 1 ? 2 : 1;
  request.vsync = SendDlgItemMessageW(window, UiVsync, BM_GETCHECK, 0, 0) == BST_CHECKED;
  request.fullscreen = SendDlgItemMessageW(window, UiFullscreen, BM_GETCHECK, 0, 0) == BST_CHECKED;
  const auto selected = SendDlgItemMessageW(window, UiProfiles, CB_GETCURSEL, 0, 0);
  if (selected >= 0 && static_cast<size_t>(selected) < state.model.library.entries.size())
    request.selected_profile_id = state.model.library.entries[static_cast<size_t>(selected)].id;
  request.profile_label = Text(window, UiProfileLabel);
  request.profile_root = Text(window, UiProfileRoot);
  request.legacy_source_root = Text(window, UiLegacySource);
  request.in_game_name = Text(window, UiInGameName);
  return request;
}

void Render(HWND window, WindowState &state) {
  SetText(window, UiBuildRoot, state.model.build_root);
  SetText(window, UiGameRoot, state.model.game_root);
  HWND profiles = GetDlgItem(window, UiProfiles);
  SendMessageW(profiles, CB_RESETCONTENT, 0, 0);
  std::wstring selected_root;
  bool managed = false;
  for (size_t i = 0; i < state.model.library.entries.size(); ++i) {
    const auto &entry = state.model.library.entries[i];
    auto label = Wide(entry.label);
    if (entry.layout != Layout::ManagedExternalV1) label += L" [legacy: import required]";
    SendMessageW(profiles, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(label.c_str()));
    if (entry.id == state.model.library.selected_id) {
      SendMessageW(profiles, CB_SETCURSEL, static_cast<WPARAM>(i), 0);
      selected_root = Wide(entry.root);
      managed = entry.layout == Layout::ManagedExternalV1;
    }
  }
  SetText(window, UiSelectedRoot, selected_root.empty() ? L"No profile selected" : selected_root);
  SetText(window, UiInGameName, managed ? state.model.in_game_name : L"");
  HWND gpu = GetDlgItem(window, UiGpu);
  SendMessageW(gpu, CB_RESETCONTENT, 0, 0);
  for (size_t i = 0; i < state.model.gpu_names.size(); ++i) {
    const auto &name = state.model.gpu_names[i];
    SendMessageW(gpu, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(name.c_str()));
    if (name == state.model.gpu_name)
      SendMessageW(gpu, CB_SETCURSEL, static_cast<WPARAM>(i), 0);
  }
  SendDlgItemMessageW(window, UiScale, CB_SETCURSEL,
                     state.model.resolution_scale == 2 ? 1 : 0, 0);
  SendDlgItemMessageW(window, UiVsync, BM_SETCHECK, state.model.vsync ? BST_CHECKED : BST_UNCHECKED, 0);
  SendDlgItemMessageW(window, UiFullscreen, BM_SETCHECK, state.model.fullscreen ? BST_CHECKED : BST_UNCHECKED, 0);
  for (int id = UiBuildRoot; id <= UiPlay; ++id)
    EnableWindow(GetDlgItem(window, id), !state.model.running);
  EnableWindow(GetDlgItem(window, UiPlay), !state.model.running && managed);
  EnableWindow(GetDlgItem(window, UiRenameLabel), !state.model.running && !selected_root.empty());
  EnableWindow(GetDlgItem(window, UiInGameName), !state.model.running && managed);
  EnableWindow(GetDlgItem(window, UiRenameInGameName), !state.model.running && managed);
  EnableWindow(GetDlgItem(window, UiDuplicateProfile), !state.model.running && managed);
  EnableWindow(GetDlgItem(window, UiRestoreSnapshot), !state.model.running && managed);
  EnableWindow(GetDlgItem(window, UiVerifyRoots), !state.model.running);
  // Legacy persisted Off can be explicitly repaired to On; switching a stock
  // profile from On to unvalidated guest cadence is not offered by this UI.
  EnableWindow(GetDlgItem(window, UiVsync), !state.model.running && !state.model.vsync);
  EnableWindow(GetDlgItem(window, UiOpenSave), !selected_root.empty());
  EnableWindow(GetDlgItem(window, UiOpenDiagnostics), !selected_root.empty());
  SetText(window, UiStatus, state.model.status);
  if (!state.testing) {
    if (state.model.running) SetTimer(window, kRefreshTimer, 1000, nullptr);
    else KillTimer(window, kRefreshTimer);
  }
}

void Dispatch(HWND window, WindowState &state, UiAction action) {
  if (state.dispatching) return;
  if (state.model.running && action != UiAction::Refresh &&
      action != UiAction::OpenSave && action != UiAction::OpenDiagnostics) return;
  state.dispatching = true;
  try {
    const UiRequest request = Request(window, state);
    if (action == UiAction::RenameLabel || action == UiAction::RenameInGameName ||
        action == UiAction::DuplicateProfile || action == UiAction::RestoreSnapshot) {
      const auto selected = std::find_if(state.model.library.entries.begin(),
          state.model.library.entries.end(), [&](const ProfileEntry &entry) {
            return entry.id == state.model.library.selected_id && entry.id == request.selected_profile_id;
          });
      if (selected == state.model.library.entries.end()) {
        state.dispatching = false;
        return;
      }
      if (action != UiAction::RenameLabel && selected->layout != Layout::ManagedExternalV1) {
        state.dispatching = false;
        return;
      }
      if (action == UiAction::RenameInGameName) {
        if (request.in_game_name.empty() || request.in_game_name.size() > 15 ||
            !std::all_of(request.in_game_name.begin(), request.in_game_name.end(),
                [](wchar_t c) { return c >= L' ' && c <= L'~'; })) {
          SetText(window, UiStatus, L"In-game name must contain 1-15 printable ASCII characters. No action was submitted.");
          RevealControl(window, state, GetDlgItem(window, UiStatus));
          state.dispatching = false;
          return;
        }
      }
    }
    if (state.callback) state.model = state.callback(action, request);
    else state.model.status = L"No launcher backend is connected.";
  } catch (const std::exception &) {
    // Do not expose arbitrary exception contents (which may contain private
    // paths or credentials) or infer that a partially completed action failed.
    state.model.status = L"The launcher action returned an error. Check local diagnostics before retrying.";
    // The callback may have started a child before throwing. Refuse further
    // mutations until a successful Refresh returns authoritative ownership.
    state.model.running = true;
  } catch (...) {
    state.model.status = L"The launcher action returned an unexpected error. Check local diagnostics before retrying.";
    state.model.running = true;
  }
  state.dispatching = false;
  Render(window, state);
}

void PickFolder(HWND window, int target) {
  Microsoft::WRL::ComPtr<IFileOpenDialog> picker;
  HRESULT result = CoCreateInstance(CLSID_FileOpenDialog, nullptr,
      CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&picker));
  if (FAILED(result)) {
    SetText(window, UiStatus, L"Folder picker could not be opened. Enter the full path directly.");
    return;
  }
  FILEOPENDIALOGOPTIONS flags = 0;
  result = picker->GetOptions(&flags);
  if (SUCCEEDED(result)) result = picker->SetOptions(flags | FOS_PICKFOLDERS |
      FOS_FORCEFILESYSTEM | FOS_PATHMUSTEXIST | FOS_DONTADDTORECENT);
  if (SUCCEEDED(result)) result = picker->SetTitle(
      target == UiProfileRoot ? L"Choose NEW profile parent / existing root to register (no writes yet)" :
      target == UiLegacySource ? L"Choose import / selected profile snapshot source (read-only source)" :
      target == UiBuildRoot ? L"Choose MCLA-R build folder" : L"Choose game-data folder");
  if (SUCCEEDED(result)) result = picker->Show(window);
  if (result == HRESULT_FROM_WIN32(ERROR_CANCELLED)) return;
  Microsoft::WRL::ComPtr<IShellItem> item;
  if (SUCCEEDED(result)) result = picker->GetResult(&item);
  PWSTR path = nullptr;
  if (SUCCEEDED(result)) result = item->GetDisplayName(SIGDN_FILESYSPATH, &path);
  if (SUCCEEDED(result) && path) SetText(window, target, path);
  else SetText(window, UiStatus, L"Folder selection failed. No launcher action was submitted.");
  CoTaskMemFree(path);
}

LRESULT CALLBACK WindowProc(HWND window, UINT message, WPARAM wparam, LPARAM lparam) {
  auto *state = reinterpret_cast<WindowState *>(GetWindowLongPtrW(window, GWLP_USERDATA));
  if (message == WM_NCCREATE) {
    state = static_cast<WindowState *>(reinterpret_cast<CREATESTRUCTW *>(lparam)->lpCreateParams);
    SetWindowLongPtrW(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(state));
  }
  if (!state) return DefWindowProcW(window, message, wparam, lparam);
  switch (message) {
    case WM_CREATE:
      Controls(window, *state);
      UpdateScrollbars(window, *state);
      Dispatch(window, *state, UiAction::Refresh);
      return 0;
    case WM_COMMAND: {
      if (state->dispatching) return 0;
      const int id = LOWORD(wparam);
      const int notification = HIWORD(wparam);
      if (notification == EN_SETFOCUS || notification == CBN_SETFOCUS || notification == BN_SETFOCUS) {
        RevealControl(window, *state, GetDlgItem(window, id));
        return 0;
      }
      if (id == UiProfiles && HIWORD(wparam) == CBN_SELCHANGE) {
        Dispatch(window, *state, UiAction::SelectProfile);
        return 0;
      }
      if (HIWORD(wparam) != BN_CLICKED) break;
      if (!IsWindowEnabled(GetDlgItem(window, id))) return 0;
      switch (id) {
        case UiBrowseBuild: PickFolder(window, UiBuildRoot); break;
        case UiBrowseGame: PickFolder(window, UiGameRoot); break;
        case UiBrowseProfile: PickFolder(window, UiProfileRoot); break;
        case UiBrowseLegacy: PickFolder(window, UiLegacySource); break;
        case UiSaveSettings: Dispatch(window, *state, UiAction::SaveSettings); break;
        case UiRegister: Dispatch(window, *state, UiAction::RegisterManaged); break;
        case UiCreate: Dispatch(window, *state, UiAction::CreateProfile); break;
        case UiImport: Dispatch(window, *state, UiAction::ImportLegacy); break;
        case UiPlay: Dispatch(window, *state, UiAction::Play); break;
        case UiOpenSave: Dispatch(window, *state, UiAction::OpenSave); break;
        case UiOpenDiagnostics: Dispatch(window, *state, UiAction::OpenDiagnostics); break;
        case UiRenameLabel: Dispatch(window, *state, UiAction::RenameLabel); break;
        case UiRenameInGameName: Dispatch(window, *state, UiAction::RenameInGameName); break;
        case UiDuplicateProfile: Dispatch(window, *state, UiAction::DuplicateProfile); break;
        case UiRestoreSnapshot: Dispatch(window, *state, UiAction::RestoreSnapshot); break;
        case UiVerifyRoots: Dispatch(window, *state, UiAction::VerifyRoots); break;
      }
      return 0;
    }
    case WM_TIMER:
      if (wparam == kRefreshTimer) Dispatch(window, *state, UiAction::Refresh);
      return 0;
    case WM_SIZE:
      UpdateScrollbars(window, *state);
      return 0;
    case WM_VSCROLL:
    case WM_HSCROLL:
      if (!lparam) ScrollMessage(window, *state, message == WM_VSCROLL, wparam);
      return 0;
    case WM_MOUSEWHEEL:
      state->wheel_remainder += GET_WHEEL_DELTA_WPARAM(wparam);
      if (const int steps = state->wheel_remainder / WHEEL_DELTA) {
        state->wheel_remainder %= WHEEL_DELTA;
        ScrollTo(window, *state, state->scroll_x, state->scroll_y - steps * Px(*state, 72));
      }
      return 0;
    case WM_GETMINMAXINFO: {
      auto *limits = reinterpret_cast<MINMAXINFO *>(lparam);
      limits->ptMinTrackSize = {Px(*state, 420), Px(*state, 300)};
      return 0;
    }
    case WM_CLOSE:
      DestroyWindow(window);
      return 0;
    case WM_DESTROY:
      KillTimer(window, kRefreshTimer);
      if (!state->testing) PostQuitMessage(0);
      return 0;
    case WM_NCDESTROY:
      SetWindowLongPtrW(window, GWLP_USERDATA, 0);
      if (state->font) DeleteObject(state->font);
      if (state->window_owned) delete state;
      return DefWindowProcW(window, message, wparam, lparam);
  }
  return DefWindowProcW(window, message, wparam, lparam);
}

HWND Create(UiCallback callback, bool testing) {
  WNDCLASSEXW wc{sizeof(wc)};
  wc.lpfnWndProc = WindowProc;
  wc.hInstance = GetModuleHandleW(nullptr);
  wc.hCursor = LoadCursorW(nullptr, MAKEINTRESOURCEW(32512));
  wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
  wc.lpszClassName = kWindowClass;
  if (!RegisterClassExW(&wc) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) return nullptr;
  auto state = std::make_unique<WindowState>();
  state->callback = std::move(callback);
  state->testing = testing;
  state->dpi = GetDpiForSystem();
  RECT bounds{0, 0, Px(*state, kWidth), Px(*state, kHeight)};
  constexpr DWORD style = WS_OVERLAPPEDWINDOW | WS_HSCROLL | WS_VSCROLL;
  AdjustWindowRectExForDpi(&bounds, style, FALSE, 0, state->dpi);
  bounds.right += GetSystemMetricsForDpi(SM_CXVSCROLL, state->dpi);
  bounds.bottom += GetSystemMetricsForDpi(SM_CYHSCROLL, state->dpi);
  MONITORINFO monitor{sizeof(monitor)};
  if (!GetMonitorInfoW(MonitorFromPoint({0, 0}, MONITOR_DEFAULTTOPRIMARY), &monitor)) return nullptr;
  const int width = std::min(bounds.right - bounds.left, monitor.rcWork.right - monitor.rcWork.left);
  const int height = std::min(bounds.bottom - bounds.top, monitor.rcWork.bottom - monitor.rcWork.top);
  const int x = monitor.rcWork.left + (monitor.rcWork.right - monitor.rcWork.left - width) / 2;
  const int y = monitor.rcWork.top + (monitor.rcWork.bottom - monitor.rcWork.top - height) / 2;
  HWND window = CreateWindowExW(WS_EX_CONTROLPARENT, kWindowClass, L"MCLA-R Launcher",
      style, x, y, width, height, nullptr, nullptr, wc.hInstance, state.get());
  if (window) {
    state->window_owned = true;
    state.release(); // WM_NCDESTROY owns it after successful construction.
  }
  return window;
}
} // namespace

HWND CreateLauncherUiForTest(UiCallback callback) {
  return Create(std::move(callback), true);
}

int RunLauncherUi(UiCallback callback) {
  // The launcher executable should also declare DPI awareness in its manifest.
  SetThreadDpiAwarenessContext(DPI_AWARENESS_CONTEXT_SYSTEM_AWARE);
  const HRESULT com = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
  if (FAILED(com)) return 1;
  HWND window = Create(std::move(callback), false);
  if (!window) { CoUninitialize(); return 1; }
  ShowWindow(window, SW_SHOWNORMAL);
  UpdateWindow(window);
  MSG message{};
  BOOL result = 0;
  while ((result = GetMessageW(&message, nullptr, 0, 0)) > 0) {
    if (!IsDialogMessageW(window, &message)) {
      TranslateMessage(&message);
      DispatchMessageW(&message);
    }
  }
  CoUninitialize();
  return result == -1 ? 1 : static_cast<int>(message.wParam);
}

} // namespace mcla::launcher
