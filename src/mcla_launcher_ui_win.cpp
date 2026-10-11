#include "mcla_launcher_ui_win.h"
#include "mcla_render_target_policy.h"
#include "mcla_launcher_capabilities.h"

#include <shobjidl.h>
#include <wrl/client.h>

#include <algorithm>
#include <exception>
#include <future>
#include <chrono>
#include <memory>
#include <map>
#include <stdexcept>
#include <utility>

namespace mcla::launcher {
namespace {
constexpr wchar_t kWindowClass[] = L"MclaInternalLauncherV1";
constexpr UINT_PTR kRefreshTimer = 1;
constexpr UINT_PTR kReadinessTimer = 2;
constexpr UINT kAutoCheckMessage = WM_APP + 41;
constexpr int kWidth = 900;
constexpr int kHeight = 690;

struct WindowState {
  UiCallback callback;
  UiState model;
  HFONT font = nullptr;
  HFONT heading_font = nullptr;
  UINT dpi = 96;
  bool dispatching = false;
  bool testing = false;
  bool window_owned = false;
  bool initialized = false;
  bool rendering = false;
  bool updating_scrollbars = false;
  bool auto_checks = false;
  bool dirty_setup = false;
  bool auto_recheck_pending = false;
  bool closing = false;
  uint64_t generation = 0;
  std::future<UiState> check_worker;
  UiRequest pending_request;
  uint64_t pending_generation = 0;
  std::wstring pending_profile_root;
  std::vector<std::wstring> pending_inventory;
  HWND operation = nullptr;
  std::map<UiAction, UiRequest> operation_drafts;
  LaunchReadiness readiness = LaunchReadiness::Unchecked;
  std::wstring readiness_reason = L"Not checked. CHECK SETUP validates without starting the game.";
  UiRequest checked_request;
  std::wstring checked_profile_root;
  std::vector<std::wstring> checked_inventory;
  uint64_t checked_tick = 0, checked_utc = 0, eligibility_expires_utc = 0;
  bool synthetic_clock = false;
  uint64_t synthetic_tick = 0, synthetic_utc = 0;
  int scroll_x = 0;
  int scroll_y = 0;
  int wheel_remainder = 0;
  int content_width = kWidth, content_height = kHeight;
  int page = 0;
  int adding_page = -1;
  std::vector<std::pair<HWND, int>> page_controls;
  struct Position { HWND control; int x; int y; };
  std::vector<Position> control_positions;
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
  if (dx || dy) {
    // Reposition hidden pages too: ScrollWindowEx only guarantees movement of
    // intersecting children, which would leave inactive sections displaced.
    for (const auto &item : state.control_positions)
      SetWindowPos(item.control, nullptr, Px(state, item.x) - next_x,
          Px(state, item.y) - next_y, 0, 0,
          SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
    InvalidateRect(window, nullptr, TRUE);
  }
}

void UpdateScrollbars(HWND window, WindowState &state) {
  if (state.updating_scrollbars) return; // Showing a native bar sends WM_SIZE.
  state.updating_scrollbars = true;
  RECT client{};
  GetClientRect(window, &client);
  const auto style = GetWindowLongPtrW(window, GWL_STYLE);
  const int bar_width = GetSystemMetricsForDpi(SM_CXVSCROLL, state.dpi);
  const int bar_height = GetSystemMetricsForDpi(SM_CYHSCROLL, state.dpi);
  // Recover the viewport without bars, then solve their coupled space needs.
  // Starting with neither bar avoids retaining a pair that only causes itself.
  const int available_width = client.right + ((style & WS_VSCROLL) ? bar_width : 0);
  const int available_height = client.bottom + ((style & WS_HSCROLL) ? bar_height : 0);
  bool horizontal = false, vertical = false;
  for (int i = 0; i < 3; ++i) {
    horizontal = Px(state, state.content_width) > available_width - (vertical ? bar_width : 0);
    vertical = Px(state, state.content_height) > available_height - (horizontal ? bar_height : 0);
  }
  SCROLLINFO info{sizeof(info), SIF_RANGE | SIF_PAGE | SIF_POS};
  info.nMax = Px(state, state.content_width) - 1;
  info.nPage = static_cast<UINT>(std::max(available_width - (vertical ? bar_width : 0), 1));
  info.nPos = state.scroll_x;
  SetScrollInfo(window, SB_HORZ, &info, TRUE);
  info.nMax = Px(state, state.content_height) - 1;
  info.nPage = static_cast<UINT>(std::max(available_height - (horizontal ? bar_height : 0), 1));
  info.nPos = state.scroll_y;
  SetScrollInfo(window, SB_VERT, &info, TRUE);
  ScrollTo(window, state, GetScrollPos(window, SB_HORZ), GetScrollPos(window, SB_VERT));
  state.updating_scrollbars = false;
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
  if (control && state.adding_page >= 0)
    state.page_controls.emplace_back(control, state.adding_page);
  if (control) state.control_positions.push_back({control, x, y});
  return control;
}

void ShowPage(HWND window, WindowState &state, int page, bool focus = true) {
  if (page < 0 || page > 2) return;
  state.page = page;
  // No Render/Refresh: navigating must not discard drafts or submit actions.
  for (const auto &[control, owner] : state.page_controls)
    ShowWindow(control, owner == page ? SW_SHOWNA : SW_HIDE);
  for (int i = 0; i < 3; ++i)
    SendDlgItemMessageW(window, UiPagePlay + i, BM_SETCHECK,
                       i == page ? BST_CHECKED : BST_UNCHECKED, 0);
  if (focus) {
    ScrollTo(window, state, 0, 0);
    HWND navigation = GetDlgItem(window, UiPagePlay + page);
    SetFocus(navigation);
    // Small viewports may need horizontal scrolling for the Setup button.
    // Reveal after resetting; hidden fixtures may emit no focus notification.
    RevealControl(window, state, navigation);
  }
}

void Controls(HWND window, WindowState &state) {
  state.font = CreateFontW(-Px(state, 15), 0, 0, 0, FW_NORMAL, FALSE, FALSE,
                          FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
                          CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                          DEFAULT_PITCH, L"Segoe UI");
  state.heading_font = CreateFontW(-Px(state, 24), 0, 0, 0, FW_SEMIBOLD,
      FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
      CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe UI");
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
  HWND title = Add(window, state, L"STATIC", L"MCLA-R", 0, 0, 24, 18, 400, 34);
  SendMessageW(title, WM_SETFONT, reinterpret_cast<WPARAM>(state.heading_font), TRUE);
  label(L"Internal test launcher", 650, 27, 226);
  label(L"First time? Set your folders in Setup, then add or import a save in Profiles.",
        24, 59, 852);
  label(L"Save profile", 24, 102, 110);
  Add(window, state, L"COMBOBOX", L"", WS_TABSTOP | CBS_DROPDOWNLIST |
      WS_VSCROLL, UiProfiles, 140, 98, 736, 200);
  label(L"Save location", 24, 140, 110);
  edit(UiSelectedRoot, 140, 136, 736, true);
  const wchar_t *pages[] = {L"&Play", L"P&rofiles", L"&Setup"};
  for (int i = 0; i < 3; ++i)
    Add(window, state, L"BUTTON", pages[i], WS_TABSTOP | BS_RADIOBUTTON |
        BS_PUSHLIKE | BS_NOTIFY | (i == 0 ? WS_GROUP : 0), UiPagePlay + i,
        24 + i * 160, 184, 150, 36);
  label(L"Ctrl+Tab to switch sections", 610, 194, 266);

  state.adding_page = 0;
  Add(window, state, L"STATIC", L"", 0, UiCurrentName, 24, 246, 852, 24);
  label(L"Choose a profile above. Changing profiles never copies or merges saves.", 24, 273, 852);
  label(L"Graphics adapter", 24, 321, 145);
  Add(window, state, L"COMBOBOX", L"", WS_TABSTOP | CBS_DROPDOWNLIST |
      WS_VSCROLL | WS_GROUP, UiGpu, 180, 317, 696, 200);
  label(L"Render target", 24, 365, 145);
  HWND scale = Add(window, state, L"COMBOBOX", L"", WS_TABSTOP |
      CBS_DROPDOWNLIST, UiScale, 180, 361, 155, 160);
  for (const auto &target : kRenderTargets) {
    const auto text = std::to_wstring(target.width) + L" x " + std::to_wstring(target.height);
    const auto index = SendMessageW(scale, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(text.c_str()));
    if (index >= 0) SendMessageW(scale, CB_SETITEMDATA, static_cast<WPARAM>(index), target.id);
  }
  Add(window, state, L"BUTTON", L"VSync (stock: ON)", WS_TABSTOP | BS_AUTOCHECKBOX | BS_NOTIFY,
      UiVsync, 360, 361, 205, 28);
  Add(window, state, L"BUTTON", L"Fullscreen", WS_TABSTOP | BS_AUTOCHECKBOX | BS_NOTIFY,
      UiFullscreen, 570, 361, 160, 28);
  label(L"Frame timing", 24, 409, 145);
  const auto timing=StockTimingDescription();
  const std::wstring timing_label(timing.begin(),timing.end());
  Add(window, state, L"STATIC", timing_label.c_str(),
      0, UiTiming, 180, 405, 696, 28);
  Add(window, state, L"STATIC", L"1080p: experimental 960 x 540 guest mode + 2x rendering. Build support is checked.",
      0, UiRenderTargetNote, 180, 434, 696, 24);
  button(UiOpenSave, L"Open save folder", 180, 460, 205);
  button(UiOpenDiagnostics, L"Open diagnostics", 400, 460, 205);

  state.adding_page = 1;
  label(L"Profile name", 24, 246, 145);
  Add(window, state, L"STATIC", L"", 0, UiSelectedProfileName, 180, 246, 420, 26);
  button(UiRenameLabel, L"Rename profile...", 612, 241, 264);
  label(L"This name is only for the launcher. It does not rename your character.", 180, 277, 696);
  label(L"In-game name", 24, 320, 145);
  edit(UiInGameName, 180, 316, 210);
  button(UiRenameInGameName, L"Apply in-game name", 402, 315, 230);
  label(L"1-15 printable ASCII", 650, 321, 226);
  label(L"Add a save profile", 24, 370, 852);
  button(UiCreate, L"Create empty profile...", 24, 400, 270);
  button(UiRegister, L"Register managed folder...", 310, 400, 270);
  button(UiImport, L"Import old saves...", 596, 400, 280);
  button(UiDuplicateProfile, L"Copy selected profile...", 24, 450, 414);
  button(UiRestoreSnapshot, L"Restore a snapshot...", 454, 450, 422);
  label(L"Each action asks for its own folders. Import, copy and restore create a NEW profile; originals stay intact.",
        24, 494, 852, 24);

  state.adding_page = 2;
  label(L"One-time setup", 24, 246, 852);
  label(L"Build and game data stay separate from your personal saves. Change these when switching builds.",
        24, 273, 852);
  label(L"Build folder", 24, 326, 145);
  edit(UiBuildRoot, 180, 322, 576);
  button(UiBrowseBuild, L"Browse...", 768, 321, 108);
  label(L"Game-data folder", 24, 372, 145);
  edit(UiGameRoot, 180, 368, 576);
  button(UiBrowseGame, L"Browse...", 768, 367, 108);
  button(UiVerifyRoots, L"Verify build / game", 180, 418, 240);
  label(L"Verify checks identity without saving settings or launching the game.\nYour typed folders stay here while you import or switch profiles.", 180, 466, 696, 48);

  state.adding_page = -1;
  button(UiSaveSettings, L"Save settings", 24, 534, 165);
  Add(window, state, L"BUTTON", L"CHECK SETUP", WS_TABSTOP | BS_OWNERDRAW | BS_NOTIFY,
      UiCheckLaunch, 205, 534, 190, 30);
  Add(window, state, L"BUTTON", L"START GAME",
      WS_TABSTOP | BS_OWNERDRAW | BS_NOTIFY, UiPlay, 612, 526, 264, 44);
  Add(window, state, L"STATIC", L"", 0, UiReadiness, 24, 580, 852, 26);
  label(L"Status", 24, 614, 105);
  Add(window, state, L"EDIT", L"", WS_TABSTOP | ES_READONLY |
      ES_MULTILINE | ES_AUTOVSCROLL | WS_VSCROLL, UiStatus,
      140, 610, 736, 50);
  label(L"Closing this launcher does not stop a running game.", 24, 664, 852);
  ShowPage(window, state, 0, false);
}

UiRequest Request(HWND window, const WindowState &state) {
  UiRequest request;
  request.build_root = Text(window, UiBuildRoot);
  request.game_root = Text(window, UiGameRoot);
  request.gpu_name = Text(window, UiGpu);
  const auto target_index = SendDlgItemMessageW(window, UiScale, CB_GETCURSEL, 0, 0);
  request.render_target = 0; // Missing/invalid selection must never silently become720p.
  if (target_index >= 0 && static_cast<size_t>(target_index) < kRenderTargets.size()) {
    const auto target_id = SendDlgItemMessageW(window, UiScale, CB_GETITEMDATA,
        static_cast<WPARAM>(target_index), 0);
    if (target_id == kRenderTargets[static_cast<size_t>(target_index)].id)
      request.render_target = static_cast<uint32_t>(target_id);
  }
  request.vsync = SendDlgItemMessageW(window, UiVsync, BM_GETCHECK, 0, 0) == BST_CHECKED;
  request.fullscreen = SendDlgItemMessageW(window, UiFullscreen, BM_GETCHECK, 0, 0) == BST_CHECKED;
  const auto selected = SendDlgItemMessageW(window, UiProfiles, CB_GETCURSEL, 0, 0);
  if (selected >= 0 && static_cast<size_t>(selected) < state.model.library.entries.size())
    request.selected_profile_id = state.model.library.entries[static_cast<size_t>(selected)].id;
  request.in_game_name = Text(window, UiInGameName);
  return request;
}

std::pair<uint64_t, uint64_t> Clock(const WindowState &state) {
  if (state.synthetic_clock) return {state.synthetic_tick, state.synthetic_utc};
  FILETIME value{};
  GetSystemTimeAsFileTime(&value);
  ULARGE_INTEGER ticks{};
  ticks.LowPart = value.dwLowDateTime;
  ticks.HighPart = value.dwHighDateTime;
  return {GetTickCount64(), (ticks.QuadPart - 116444736000000000ULL) / 10000000ULL};
}

bool SameSetup(const UiRequest &a, const UiRequest &b) {
  return a.build_root == b.build_root && a.game_root == b.game_root &&
      a.gpu_name == b.gpu_name && a.render_target == b.render_target &&
      a.vsync == b.vsync && a.fullscreen == b.fullscreen &&
      a.selected_profile_id == b.selected_profile_id;
}

std::wstring SelectedManagedRoot(const WindowState &state, const UiRequest &request) {
  for (const auto &entry : state.model.library.entries)
    if (entry.id == state.model.library.selected_id && entry.id == request.selected_profile_id &&
        entry.layout == Layout::ManagedExternalV1) return Wide(entry.root);
  return {};
}

void InvalidateReadiness(WindowState &state, const wchar_t *reason =
    L"Setup changed. CHECK SETUP again, or START GAME to validate at launch.", bool dirty = true) {
  if (dirty) {
    ++state.generation;
    state.dirty_setup = true;
    state.auto_recheck_pending = false;
  }
  state.readiness = LaunchReadiness::Unchecked;
  state.readiness_reason = reason;
  state.eligibility_expires_utc = 0;
}

void ReadinessPresentation(HWND window, WindowState &state) {
  const auto request = Request(window, state);
  const auto root = SelectedManagedRoot(state, request);
  if (state.readiness == LaunchReadiness::Ready) {
    const auto [tick, utc] = Clock(state);
    if (!SameSetup(request, state.checked_request) || root != state.checked_profile_root ||
        state.model.gpu_names != state.checked_inventory)
      InvalidateReadiness(state);
    else if (tick < state.checked_tick || tick - state.checked_tick >= 60000 ||
             utc < state.checked_utc || utc >= state.eligibility_expires_utc) {
      const bool renew = state.auto_checks && !state.dirty_setup && !state.closing;
      InvalidateReadiness(state, L"Check expired. CHECK SETUP again; launch always validates afresh.", false);
      state.auto_recheck_pending = renew;
    }
  }
  const bool running = state.model.running;
  const bool pending = state.check_worker.valid();
  std::wstring reason = pending ? L"Checking setup in the background... You can edit settings or switch sections."
      : running ? L"Game running. You may close the launcher without stopping it."
                               : state.readiness_reason;
  if (reason.empty()) reason = L"Not checked. CHECK SETUP validates without starting the game.";
  SetText(window, UiReadiness, reason);
  EnableWindow(GetDlgItem(window, UiPlay), !pending && !state.closing && !running && !root.empty() &&
      state.readiness != LaunchReadiness::Blocked);
  EnableWindow(GetDlgItem(window, UiCheckLaunch), !pending && !state.closing && !running);
  if (pending || state.closing) {
    for (int id : {UiProfiles, UiSaveSettings, UiRegister, UiCreate, UiImport, UiRenameLabel,
         UiInGameName, UiRenameInGameName, UiDuplicateProfile, UiRestoreSnapshot, UiVerifyRoots,
         UiOpenSave, UiOpenDiagnostics}) EnableWindow(GetDlgItem(window, id), FALSE);
  }
  InvalidateRect(GetDlgItem(window, UiPlay), nullptr, TRUE);
  InvalidateRect(GetDlgItem(window, UiCheckLaunch), nullptr, TRUE);
  if (!state.testing) {
    if (pending || state.auto_recheck_pending || (state.readiness == LaunchReadiness::Ready && !running))
      SetTimer(window, kReadinessTimer, pending ? 50 : 1000, nullptr);
    else KillTimer(window, kReadinessTimer);
  }
}

void DrawCheckButton(const DRAWITEMSTRUCT &draw, const WindowState &state) {
  const bool disabled = (draw.itemState & ODS_DISABLED) != 0;
  const bool highlighted = !disabled && !state.check_worker.valid() &&
      (state.readiness == LaunchReadiness::Unchecked || state.readiness == LaunchReadiness::Blocked);
  HIGHCONTRASTW contrast{sizeof(contrast)};
  const bool high_contrast = SystemParametersInfoW(SPI_GETHIGHCONTRAST, sizeof(contrast), &contrast, 0) &&
                             (contrast.dwFlags & HCF_HIGHCONTRASTON);
  const COLORREF fill = highlighted && !high_contrast ? RGB(142, 87, 0) : GetSysColor(COLOR_BTNFACE);
  HBRUSH brush = CreateSolidBrush(fill);
  FillRect(draw.hDC, &draw.rcItem, brush);
  DeleteObject(brush);
  FrameRect(draw.hDC, &draw.rcItem, GetSysColorBrush(COLOR_BTNSHADOW));
  const int saved = SaveDC(draw.hDC);
  SetBkMode(draw.hDC, TRANSPARENT);
  SetTextColor(draw.hDC, highlighted && !high_contrast ? RGB(255, 255, 255) :
      GetSysColor(disabled ? COLOR_GRAYTEXT : COLOR_BTNTEXT));
  SelectObject(draw.hDC, state.font);
  RECT text = draw.rcItem;
  if (draw.itemState & ODS_SELECTED) OffsetRect(&text, 1, 1);
  DrawTextW(draw.hDC, L"CHECK SETUP", -1, &text, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
  if (draw.itemState & ODS_FOCUS) {
    RECT focus = draw.rcItem; InflateRect(&focus, -3, -3); DrawFocusRect(draw.hDC, &focus);
  }
  RestoreDC(draw.hDC, saved);
}

void DrawStartButton(const DRAWITEMSTRUCT &draw, const WindowState &state) {
  const bool disabled = (draw.itemState & ODS_DISABLED) != 0;
  const bool gray = state.model.running || (disabled && state.readiness != LaunchReadiness::Blocked);
  HIGHCONTRASTW contrast{sizeof(contrast)};
  const bool high_contrast = SystemParametersInfoW(SPI_GETHIGHCONTRAST, sizeof(contrast), &contrast, 0) &&
                             (contrast.dwFlags & HCF_HIGHCONTRASTON);
  const COLORREF fill = high_contrast ? GetSysColor(COLOR_BTNFACE) : gray ? RGB(96, 100, 106) :
      state.readiness == LaunchReadiness::Ready ? RGB(25, 110, 61) : RGB(142, 87, 0);
  HBRUSH brush = CreateSolidBrush(fill);
  FillRect(draw.hDC, &draw.rcItem, brush);
  DeleteObject(brush);
  const int saved = SaveDC(draw.hDC);
  SetBkMode(draw.hDC, TRANSPARENT);
  SetTextColor(draw.hDC, high_contrast ? GetSysColor(disabled ? COLOR_GRAYTEXT : COLOR_BTNTEXT)
                                      : RGB(255, 255, 255));
  SelectObject(draw.hDC, state.heading_font ? state.heading_font : state.font);
  RECT text = draw.rcItem;
  if (draw.itemState & ODS_SELECTED) OffsetRect(&text, 1, 1);
  DrawTextW(draw.hDC, L"START GAME", -1, &text, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
  if (draw.itemState & ODS_FOCUS) {
    RECT focus = draw.rcItem;
    InflateRect(&focus, -4, -4);
    DrawFocusRect(draw.hDC, &focus);
  }
  RestoreDC(draw.hDC, saved);
}

void Render(HWND window, WindowState &state, const UiRequest &draft) {
  state.rendering = true;
  // Setup fields are a form, not a read-only projection of stored preferences.
  // Import/select/verify responses may intentionally contain older preferences.
  // Seed the form once; thereafter retain the user's draft until window close.
  // Only explicit backend actions persist it. Profile identity, name, status,
  // running ownership and the available GPU inventory remain authoritative.
  const auto &build = state.initialized ? draft.build_root : state.model.build_root;
  const auto &game = state.initialized ? draft.game_root : state.model.game_root;
  const auto &gpu_name = state.initialized ? draft.gpu_name : state.model.gpu_name;
  const auto render_target = state.initialized ? draft.render_target : state.model.render_target;
  const bool vsync = state.initialized ? draft.vsync : state.model.vsync;
  const bool fullscreen = state.initialized ? draft.fullscreen : state.model.fullscreen;
  SetText(window, UiBuildRoot, build);
  SetText(window, UiGameRoot, game);
  HWND profiles = GetDlgItem(window, UiProfiles);
  SendMessageW(profiles, CB_RESETCONTENT, 0, 0);
  std::wstring selected_root;
  std::wstring selected_name;
  bool managed = false;
  for (size_t i = 0; i < state.model.library.entries.size(); ++i) {
    const auto &entry = state.model.library.entries[i];
    auto label = Wide(entry.label);
    if (entry.layout != Layout::ManagedExternalV1) label += L" [legacy: import required]";
    SendMessageW(profiles, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(label.c_str()));
    if (entry.id == state.model.library.selected_id) {
      SendMessageW(profiles, CB_SETCURSEL, static_cast<WPARAM>(i), 0);
      selected_root = Wide(entry.root);
      selected_name = Wide(entry.label);
      managed = entry.layout == Layout::ManagedExternalV1;
    }
  }
  SetText(window, UiSelectedRoot, selected_root.empty() ? L"No profile selected" : selected_root);
  SetText(window, UiSelectedProfileName, selected_name.empty() ? L"No profile selected" : selected_name);
  SetText(window, UiInGameName, managed ? state.model.in_game_name : L"");
  SetText(window, UiCurrentName, managed
      ? L"In-game name: " + state.model.in_game_name
      : L"Select a managed save profile to play.");
  HWND gpu = GetDlgItem(window, UiGpu);
  SendMessageW(gpu, CB_RESETCONTENT, 0, 0);
  for (size_t i = 0; i < state.model.gpu_names.size(); ++i) {
    const auto &name = state.model.gpu_names[i];
    SendMessageW(gpu, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(name.c_str()));
    if (name == gpu_name)
      SendMessageW(gpu, CB_SETCURSEL, static_cast<WPARAM>(i), 0);
  }
  int target_index = -1;
  for (size_t i = 0; i < kRenderTargets.size(); ++i)
    if (kRenderTargets[i].id == render_target) target_index = static_cast<int>(i);
  SendDlgItemMessageW(window, UiScale, CB_SETCURSEL, target_index, 0);
  SendDlgItemMessageW(window, UiVsync, BM_SETCHECK, vsync ? BST_CHECKED : BST_UNCHECKED, 0);
  SendDlgItemMessageW(window, UiFullscreen, BM_SETCHECK, fullscreen ? BST_CHECKED : BST_UNCHECKED, 0);
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
  EnableWindow(GetDlgItem(window, UiVsync), !state.model.running && !vsync);
  EnableWindow(GetDlgItem(window, UiOpenSave), !selected_root.empty());
  EnableWindow(GetDlgItem(window, UiOpenDiagnostics), !selected_root.empty());
  SetText(window, UiStatus, state.model.status);
  state.initialized = true;
  state.rendering = false;
  ReadinessPresentation(window, state);
  if (!state.testing) {
    if (state.model.running) SetTimer(window, kRefreshTimer, 1000, nullptr);
    else KillTimer(window, kRefreshTimer);
  }
}

void AcceptCheck(WindowState &state, const UiRequest &request) {
  const auto [tick, utc] = Clock(state);
  const auto root = SelectedManagedRoot(state, request);
  const bool exact_gpu = std::find(state.model.gpu_names.begin(), state.model.gpu_names.end(),
                                  request.gpu_name) != state.model.gpu_names.end();
  state.dirty_setup = false;
  if (state.model.readiness == LaunchReadiness::Ready && !state.model.running && !root.empty() && exact_gpu &&
      FindRenderTarget(request.render_target) &&
      state.model.eligibility_expires_utc > utc) {
    state.readiness = LaunchReadiness::Ready;
    state.readiness_reason = L"Ready. This exact setup passed its check; launch validates again.";
    state.checked_request = request;
    state.checked_profile_root = root;
    state.checked_inventory = state.model.gpu_names;
    state.checked_tick = tick;
    state.checked_utc = utc;
    state.eligibility_expires_utc = state.model.eligibility_expires_utc;
  } else {
    state.readiness = LaunchReadiness::Blocked;
    state.readiness_reason = L"Blocked: " + (state.model.readiness_reason.empty()
        ? std::wstring(L"setup did not return a current, exact launch approval. Recheck or edit setup.")
        : state.model.readiness_reason);
  }
}

void StartCheck(HWND window, WindowState &state, bool automatic) {
  if (state.check_worker.valid() || state.dispatching || state.operation || state.closing || state.model.running)
    return;
  if (automatic && (!state.auto_checks || state.dirty_setup)) return;
  state.auto_recheck_pending = false;
  const auto request = Request(window, state);
  state.pending_request = request;
  state.pending_generation = state.generation;
  state.pending_profile_root = SelectedManagedRoot(state, request);
  state.pending_inventory = state.model.gpu_names;
  InvalidateReadiness(state, L"Checking setup in the background...", false);
  try {
    // The worker owns ONLY immutable request and callback copies. It never
    // reads WindowState/controls or posts to HWNDs. The GUI drains the future
    // before destroying this window/returning control to the controller owner.
    state.check_worker = std::async(std::launch::async,
        [callback = state.callback, request]() -> UiState {
          if (!callback) throw std::runtime_error("No setup checker");
          return callback(UiAction::CheckLaunch, request);
        });
  } catch (...) {
    state.readiness = LaunchReadiness::Blocked;
    state.readiness_reason = L"Could not start the background check. Use CHECK SETUP to retry.";
    state.model.status = L"No launch or profile operation was submitted.";
  }
  Render(window, state, request);
}

bool PollCheck(HWND window, WindowState &state) {
  if (state.check_worker.valid() &&
      state.check_worker.wait_for(std::chrono::milliseconds(0)) == std::future_status::ready) {
    const auto draft = Request(window, state);
    const bool current = state.pending_generation == state.generation &&
        SameSetup(draft, state.pending_request) &&
        SelectedManagedRoot(state, draft) == state.pending_profile_root &&
        state.model.gpu_names == state.pending_inventory;
    try {
      UiState result = state.check_worker.get(); // Ready: no blocking GUI wait.
      if (!state.closing && current) {
        state.model = std::move(result);
        if (SelectedManagedRoot(state, state.pending_request) == state.pending_profile_root &&
            state.model.gpu_names == state.pending_inventory)
          AcceptCheck(state, state.pending_request);
        else
          InvalidateReadiness(state, L"Profile or GPU inventory changed while checking. CHECK SETUP again.");
      }
    } catch (...) {
      if (!state.closing && current) {
        state.readiness = LaunchReadiness::Blocked;
        state.readiness_reason = L"Setup check failed. CHECK SETUP can retry; no game was launched.";
        state.model.status = L"The background checker returned an error. Check local diagnostics.";
      }
    }
    if (state.closing) { DestroyWindow(window); return false; }
    if (!current) InvalidateReadiness(state,
        L"Settings changed while checking. CHECK SETUP when you are ready.");
    Render(window, state, draft);
  }
  if (state.closing) return state.check_worker.valid();
  ReadinessPresentation(window, state);
  if (state.auto_recheck_pending && !state.check_worker.valid() && !state.operation && !state.dispatching &&
      !state.model.running && !state.dirty_setup) StartCheck(window, state, true);
  return state.check_worker.valid();
}

void Dispatch(HWND window, WindowState &state, UiAction action, const UiRequest *operation = nullptr) {
  if (state.dispatching || state.operation || state.check_worker.valid() || state.closing) return;
  if (action == UiAction::CheckLaunch) { StartCheck(window, state, false); return; }
  if (state.model.running && action != UiAction::Refresh &&
      action != UiAction::OpenSave && action != UiAction::OpenDiagnostics) return;
  state.dispatching = true;
  UiRequest request;
  try {
    request = Request(window, state);
    if (operation) {
      request.profile_label = operation->profile_label;
      request.profile_root = operation->profile_root;
      request.legacy_source_root = operation->legacy_source_root;
    }
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
    const bool was_running = state.model.running;
    const bool preserves_check = action == UiAction::OpenSave || action == UiAction::OpenDiagnostics ||
        action == UiAction::VerifyRoots || action == UiAction::Refresh || action == UiAction::SaveSettings;
    if (!preserves_check)
      InvalidateReadiness(state, L"Not checked. CHECK SETUP validates without starting the game.", state.initialized);
    if (state.callback) state.model = state.callback(action, request);
    else state.model.status = L"No launcher backend is connected.";
    if (was_running && !state.model.running)
      InvalidateReadiness(state, L"Game session ended. CHECK SETUP before the next launch.");
    if (action == UiAction::SaveSettings &&
        (state.model.build_root != request.build_root || state.model.game_root != request.game_root ||
         state.model.gpu_name != request.gpu_name || state.model.render_target != request.render_target ||
         state.model.vsync != request.vsync || state.model.fullscreen != request.fullscreen))
      InvalidateReadiness(state, L"Settings were not confirmed as saved. Check the status below.");
    if (action == UiAction::VerifyRoots && state.model.verification_failed) {
      InvalidateReadiness(state);
      state.readiness = LaunchReadiness::Blocked;
      state.readiness_reason = L"Build/game verification failed. Check the status below; edit setup or CHECK SETUP to retry.";
    }
    if (action == UiAction::Play && !state.model.running) {
      state.readiness = LaunchReadiness::Blocked;
      state.readiness_reason = L"Launch did not start. Check the status below; edit setup or CHECK SETUP to retry.";
    }
  } catch (const std::exception &) {
    // Do not expose arbitrary exception contents (which may contain private
    // paths or credentials) or infer that a partially completed action failed.
    state.model.status = L"The launcher action returned an error. Check local diagnostics before retrying.";
    // The callback may have started a child before throwing. Refuse further
    // mutations until a successful Refresh returns authoritative ownership.
    state.model.running = true;
    InvalidateReadiness(state);
  } catch (...) {
    state.model.status = L"The launcher action returned an unexpected error. Check local diagnostics before retrying.";
    state.model.running = true;
    InvalidateReadiness(state);
  }
  state.dispatching = false;
  Render(window, state, request);
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

constexpr wchar_t kOperationClass[] = L"MclaLauncherProfileOperationV1";
struct OperationState {
  HWND owner = nullptr;
  UiAction action = UiAction::CreateProfile;
  UiRequest request;
  std::wstring source_profile_root;
  WindowState layout;
  bool window_owned = false;
};

bool NeedsSource(UiAction action) {
  return action == UiAction::ImportLegacy || action == UiAction::RestoreSnapshot;
}

const wchar_t *OperationTitle(UiAction action) {
  switch (action) {
    case UiAction::RenameLabel: return L"Rename profile";
    case UiAction::RegisterManaged: return L"Register managed profile";
    case UiAction::ImportLegacy: return L"Import old saves into a NEW profile";
    case UiAction::DuplicateProfile: return L"Copy selected profile";
    case UiAction::RestoreSnapshot: return L"Restore snapshot as a NEW profile";
    default: return L"Create empty profile";
  }
}

UiRequest OperationRequest(HWND dialog, const OperationState &operation) {
  UiRequest request = operation.request;
  request.profile_label = Text(dialog, UiProfileLabel);
  request.profile_root = Text(dialog, UiProfileRoot);
  request.legacy_source_root = Text(dialog, UiLegacySource);
  return request;
}

void CloseOperation(HWND dialog, OperationState &operation, bool submit) {
  auto *owner = reinterpret_cast<WindowState *>(GetWindowLongPtrW(operation.owner, GWLP_USERDATA));
  if (!owner) { DestroyWindow(dialog); return; }
  UiRequest request = OperationRequest(dialog, operation);
  if (submit) {
    if (request.profile_label.empty() ||
        (operation.action != UiAction::RenameLabel && request.profile_root.empty()) ||
        (NeedsSource(operation.action) && request.legacy_source_root.empty())) {
      SetText(dialog, UiStatus, L"Complete the fields above. No action was submitted.");
      return;
    }
    if (operation.action == UiAction::RenameLabel || operation.action == UiAction::DuplicateProfile ||
        operation.action == UiAction::RestoreSnapshot) {
      const auto current = Request(operation.owner, *owner);
      if (current.selected_profile_id != operation.request.selected_profile_id ||
          current.selected_profile_id != owner->model.library.selected_id ||
          (operation.action != UiAction::RenameLabel &&
           SelectedManagedRoot(*owner, current) != operation.source_profile_root)) {
        SetText(dialog, UiStatus, L"Selected profile changed. Cancel and reopen this action.");
        return;
      }
    }
  }
  // Retain drafts only in this window's memory for an explicit retry. Cancel
  // writes no preferences and submits no callback.
  owner->operation_drafts[operation.action] = request;
  const UiAction action = operation.action;
  const HWND parent = operation.owner;
  DestroyWindow(dialog);
  if (submit && IsWindow(parent)) Dispatch(parent, *owner, action, &request);
}

LRESULT CALLBACK OperationProc(HWND dialog, UINT message, WPARAM wparam, LPARAM lparam) {
  auto *state = reinterpret_cast<OperationState *>(GetWindowLongPtrW(dialog, GWLP_USERDATA));
  if (message == WM_NCCREATE) {
    state = static_cast<OperationState *>(reinterpret_cast<CREATESTRUCTW *>(lparam)->lpCreateParams);
    SetWindowLongPtrW(dialog, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(state));
  }
  if (!state) return DefWindowProcW(dialog, message, wparam, lparam);
  switch (message) {
    case WM_CREATE: {
      auto &layout = state->layout;
      auto label = [&](const wchar_t *text, int y, int height = 26) {
        Add(dialog, layout, L"STATIC", text, 0, 0, 24, y, 632, height);
      };
      auto edit = [&](int id, int y, const std::wstring &value, bool browse) {
        HWND field = Add(dialog, layout, L"EDIT", value.c_str(), WS_TABSTOP | ES_AUTOHSCROLL,
            id, 24, y, browse ? 520 : 632, 30);
        SendMessageW(field, EM_SETLIMITTEXT,
            id == UiProfileLabel ? kMaximumLabelUnits : kMaximumPathUnits, 0);
        if (browse) Add(dialog, layout, L"BUTTON", L"Browse...", WS_TABSTOP | BS_PUSHBUTTON | BS_NOTIFY,
            id == UiProfileRoot ? UiBrowseProfile : UiBrowseLegacy, 556, y, 100, 30);
      };
      const wchar_t *help = state->action == UiAction::RenameLabel
          ? L"Profile name is shown only in the launcher. In-game name, saves and folders stay unchanged."
          : state->action == UiAction::RegisterManaged
          ? L"Register an existing managed MCLA profile root. This does not import an old bundle's user folder."
          : state->action == UiAction::RestoreSnapshot
          ? L"Choose a snapshot belonging to the selected profile. A NEW independent profile is created; nothing is overwritten."
          : state->action == UiAction::ImportLegacy
          ? L"Choose the old bundle's full user folder. It is copied into a NEW profile; the original is retained."
          : state->action == UiAction::DuplicateProfile
          ? L"Copy the selected profile into a NEW independent profile. The original and its save files are retained."
          : L"Create a NEW empty profile under a parent folder you choose. Existing profiles are never overwritten.";
      label(help, 20, 48);
      label(L"Profile name (launcher only)", 80);
      edit(UiProfileLabel, 106, state->request.profile_label, false);
      int y = 154;
      if (state->action != UiAction::RenameLabel) {
        label(state->action == UiAction::RegisterManaged ? L"Existing managed profile root"
                                                       : L"Destination parent for the NEW profile", y);
        edit(UiProfileRoot, y + 26, state->request.profile_root, true);
        y += 74;
      }
      if (NeedsSource(state->action)) {
        label(state->action == UiAction::ImportLegacy ? L"Source: old bundle's user folder (read-only)"
                                                    : L"Source: snapshot of the selected profile (read-only)", y);
        edit(UiLegacySource, y + 26, state->request.legacy_source_root, true);
        y += 74;
      }
      if (state->action != UiAction::RenameLabel)
        label(L"After completion, select the registered/new profile yourself. No automatic launch or profile switch.", y, 44);
      Add(dialog, layout, L"STATIC", L"", 0, UiStatus, 24, y + 48, 632, 42);
      Add(dialog, layout, L"BUTTON", L"Cancel", WS_TABSTOP | BS_PUSHBUTTON,
          UiOperationCancel, 374, y + 96, 130, 34);
      Add(dialog, layout, L"BUTTON", L"Confirm", WS_TABSTOP | BS_DEFPUSHBUTTON,
          UiOperationConfirm, 520, y + 96, 136, 34);
      layout.content_height = y + 148;
      UpdateScrollbars(dialog, layout);
      return 0;
    }
    case WM_COMMAND: {
      const int id = LOWORD(wparam);
      if (HIWORD(wparam) == EN_SETFOCUS || HIWORD(wparam) == BN_SETFOCUS) {
        RevealControl(dialog, state->layout, GetDlgItem(dialog, id)); return 0;
      }
      if (HIWORD(wparam) != BN_CLICKED) break;
      if (id == UiOperationCancel || id == IDCANCEL) CloseOperation(dialog, *state, false);
      else if (id == UiOperationConfirm || id == IDOK) CloseOperation(dialog, *state, true);
      else if (id == UiBrowseProfile) PickFolder(dialog, UiProfileRoot);
      else if (id == UiBrowseLegacy) PickFolder(dialog, UiLegacySource);
      return 0;
    }
    case WM_CLOSE: CloseOperation(dialog, *state, false); return 0;
    case WM_SIZE: UpdateScrollbars(dialog, state->layout); return 0;
    case WM_GETMINMAXINFO: {
      auto *limits = reinterpret_cast<MINMAXINFO *>(lparam);
      limits->ptMinTrackSize = {Px(state->layout, 380), Px(state->layout, 250)};
      return 0;
    }
    case WM_MOUSEWHEEL:
      state->layout.wheel_remainder += GET_WHEEL_DELTA_WPARAM(wparam);
      if (const int steps = state->layout.wheel_remainder / WHEEL_DELTA) {
        state->layout.wheel_remainder %= WHEEL_DELTA;
        ScrollTo(dialog, state->layout, state->layout.scroll_x,
                 state->layout.scroll_y - steps * Px(state->layout, 72));
      }
      return 0;
    case WM_VSCROLL:
    case WM_HSCROLL:
      if (!lparam) ScrollMessage(dialog, state->layout, message == WM_VSCROLL, wparam);
      return 0;
    case WM_DESTROY: {
      auto *owner = reinterpret_cast<WindowState *>(GetWindowLongPtrW(state->owner, GWLP_USERDATA));
      if (owner) {
        owner->operation = nullptr;
        EnableWindow(state->owner, TRUE);
        if (!owner->testing) SetForegroundWindow(state->owner);
      }
      return 0;
    }
    case WM_NCDESTROY:
      SetWindowLongPtrW(dialog, GWLP_USERDATA, 0);
      if (state->window_owned) delete state;
      return DefWindowProcW(dialog, message, wparam, lparam);
  }
  return DefWindowProcW(dialog, message, wparam, lparam);
}

void OpenOperation(HWND window, WindowState &owner, UiAction action) {
  if (owner.operation || owner.model.running || owner.check_worker.valid() || owner.closing) return;
  WNDCLASSEXW wc{sizeof(wc)};
  wc.lpfnWndProc = OperationProc;
  wc.hInstance = GetModuleHandleW(nullptr);
  wc.hCursor = LoadCursorW(nullptr, MAKEINTRESOURCEW(32512));
  wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
  wc.lpszClassName = kOperationClass;
  if (!RegisterClassExW(&wc) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) return;
  auto operation = std::make_unique<OperationState>();
  operation->owner = window;
  operation->action = action;
  operation->request = Request(window, owner);
  operation->source_profile_root = SelectedManagedRoot(owner, operation->request);
  const auto previous = owner.operation_drafts.find(action);
  const bool has_draft = previous != owner.operation_drafts.end() &&
      (action == UiAction::CreateProfile ||
       previous->second.selected_profile_id == operation->request.selected_profile_id);
  if (has_draft) {
    operation->request.profile_label = previous->second.profile_label;
    operation->request.profile_root = previous->second.profile_root;
    operation->request.legacy_source_root = previous->second.legacy_source_root;
  }
  if (action == UiAction::CreateProfile && !has_draft)
    operation->request.profile_root = owner.model.collection_parent;
  if (action == UiAction::RenameLabel)
    for (const auto &entry : owner.model.library.entries)
      if (entry.id == owner.model.library.selected_id) operation->request.profile_label = Wide(entry.label);
  auto &layout = operation->layout;
  layout.dpi = owner.dpi;
  layout.font = owner.font;
  layout.content_width = 680;
  layout.content_height = action == UiAction::RenameLabel ? 302 : NeedsSource(action) ? 450 : 376;
  RECT bounds{0, 0, Px(layout, layout.content_width), Px(layout, layout.content_height)};
  constexpr DWORD style = WS_CAPTION | WS_SYSMENU;
  AdjustWindowRectExForDpi(&bounds, style, FALSE, WS_EX_CONTROLPARENT, layout.dpi);
  MONITORINFO monitor{sizeof(monitor)};
  if (!GetMonitorInfoW(MonitorFromWindow(window, MONITOR_DEFAULTTONEAREST), &monitor)) return;
  const int width = std::min(bounds.right - bounds.left,
                             monitor.rcWork.right - monitor.rcWork.left);
  const int height = std::min(bounds.bottom - bounds.top,
                              monitor.rcWork.bottom - monitor.rcWork.top);
  HWND dialog = CreateWindowExW(WS_EX_CONTROLPARENT, kOperationClass, OperationTitle(action), style,
      monitor.rcWork.left + (monitor.rcWork.right - monitor.rcWork.left - width) / 2,
      monitor.rcWork.top + (monitor.rcWork.bottom - monitor.rcWork.top - height) / 2,
      width, height, window, nullptr, wc.hInstance, operation.get());
  if (!dialog) return;
  operation->window_owned = true;
  operation.release();
  owner.operation = dialog;
  EnableWindow(window, FALSE);
  if (!owner.testing) {
    ShowWindow(dialog, SW_SHOWNORMAL);
    SetFocus(GetDlgItem(dialog, UiProfileLabel));
  }
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
      if (!state->testing) PostMessageW(window, kAutoCheckMessage, 0, 0);
      return 0;
    case kAutoCheckMessage:
      StartCheck(window, *state, true);
      return 0;
    case WM_COMMAND: {
      if (state->dispatching || state->rendering || state->operation) return 0;
      const int id = LOWORD(wparam);
      const int notification = HIWORD(wparam);
      if (state->initialized &&
          (((id == UiBuildRoot || id == UiGameRoot) && notification == EN_CHANGE) ||
           ((id == UiGpu || id == UiScale) && notification == CBN_SELCHANGE) ||
           ((id == UiVsync || id == UiFullscreen) && notification == BN_CLICKED))) {
        InvalidateReadiness(*state);
        ReadinessPresentation(window, *state);
      }
      if (notification == EN_SETFOCUS || notification == CBN_SETFOCUS || notification == BN_SETFOCUS) {
        RevealControl(window, *state, GetDlgItem(window, id));
        return 0;
      }
      if (id == UiProfiles && HIWORD(wparam) == CBN_SELCHANGE) {
        Dispatch(window, *state, UiAction::SelectProfile);
        return 0;
      }
      if (HIWORD(wparam) != BN_CLICKED) break;
      if (id >= UiPagePlay && id <= UiPageSetup) {
        ShowPage(window, *state, id - UiPagePlay);
        return 0;
      }
      if (!IsWindowEnabled(GetDlgItem(window, id))) return 0;
      switch (id) {
        case UiVsync:
          // A persisted Off value is repairable, not a toggle back to an
          // unsupported stock mode after the user has checked it On.
          if (SendDlgItemMessageW(window, UiVsync, BM_GETCHECK, 0, 0) == BST_CHECKED)
            EnableWindow(GetDlgItem(window, UiVsync), FALSE);
          break;
        case UiBrowseBuild: PickFolder(window, UiBuildRoot); break;
        case UiBrowseGame: PickFolder(window, UiGameRoot); break;
        case UiBrowseProfile: PickFolder(window, UiProfileRoot); break;
        case UiBrowseLegacy: PickFolder(window, UiLegacySource); break;
        case UiSaveSettings: Dispatch(window, *state, UiAction::SaveSettings); break;
        case UiRegister: OpenOperation(window, *state, UiAction::RegisterManaged); break;
        case UiCreate: OpenOperation(window, *state, UiAction::CreateProfile); break;
        case UiImport: OpenOperation(window, *state, UiAction::ImportLegacy); break;
        case UiCheckLaunch: Dispatch(window, *state, UiAction::CheckLaunch); break;
        case UiPlay:
          ReadinessPresentation(window, *state);
          if (IsWindowEnabled(GetDlgItem(window, UiPlay))) Dispatch(window, *state, UiAction::Play);
          break;
        case UiOpenSave: Dispatch(window, *state, UiAction::OpenSave); break;
        case UiOpenDiagnostics: Dispatch(window, *state, UiAction::OpenDiagnostics); break;
        case UiRenameLabel: OpenOperation(window, *state, UiAction::RenameLabel); break;
        case UiRenameInGameName: Dispatch(window, *state, UiAction::RenameInGameName); break;
        case UiDuplicateProfile: OpenOperation(window, *state, UiAction::DuplicateProfile); break;
        case UiRestoreSnapshot: OpenOperation(window, *state, UiAction::RestoreSnapshot); break;
        case UiVerifyRoots: Dispatch(window, *state, UiAction::VerifyRoots); break;
      }
      return 0;
    }
    case WM_TIMER:
      if (wparam == kRefreshTimer) Dispatch(window, *state, UiAction::Refresh);
      if (wparam == kReadinessTimer) PollCheck(window, *state);
      return 0;
    case WM_DRAWITEM:
      if (const auto *draw = reinterpret_cast<const DRAWITEMSTRUCT *>(lparam);
          draw && draw->CtlID == UiPlay) { DrawStartButton(*draw, *state); return TRUE; }
      if (const auto *draw = reinterpret_cast<const DRAWITEMSTRUCT *>(lparam);
          draw && draw->CtlID == UiCheckLaunch) { DrawCheckButton(*draw, *state); return TRUE; }
      break;
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
      if (state->check_worker.valid()) {
        state->closing = true;
        state->auto_recheck_pending = false;
        ShowWindow(window, SW_HIDE);
        EnableWindow(window, FALSE);
        KillTimer(window, kRefreshTimer);
        SetTimer(window, kReadinessTimer, 50, nullptr);
        return 0;
      }
      DestroyWindow(window);
      return 0;
    case WM_DESTROY:
      if (state->operation) DestroyWindow(state->operation);
      KillTimer(window, kRefreshTimer);
      KillTimer(window, kReadinessTimer);
      if (!state->testing) PostQuitMessage(0);
      return 0;
    case WM_NCDESTROY:
      SetWindowLongPtrW(window, GWLP_USERDATA, 0);
      // Also covers external DestroyWindow/GetMessage failure/WM_QUIT. Never
      // let a callback still executing on the worker outlive its controller owner.
      if (state->check_worker.valid()) state->check_worker.wait();
      if (state->font) DeleteObject(state->font);
      if (state->heading_font) DeleteObject(state->heading_font);
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
  state->auto_checks = !testing;
  state->dpi = GetDpiForSystem();
  RECT bounds{0, 0, Px(*state, kWidth), Px(*state, kHeight)};
  constexpr DWORD style = WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX;
  AdjustWindowRectExForDpi(&bounds, style, FALSE, WS_EX_CONTROLPARENT, state->dpi);
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

bool HandleLauncherNavigationKey(HWND window, UINT message, WPARAM key,
                                 bool control_down, bool shift_down) {
  if (message != WM_KEYDOWN || key != VK_TAB || !control_down) return false;
  auto *state = reinterpret_cast<WindowState *>(GetWindowLongPtrW(window, GWLP_USERDATA));
  if (!state || state->operation) return false;
  ShowPage(window, *state, (state->page + (shift_down ? 2 : 1)) % 3);
  return true;
}

void SetLauncherClockForTest(HWND window, uint64_t monotonic_ms, uint64_t utc_seconds) {
  auto *state = reinterpret_cast<WindowState *>(GetWindowLongPtrW(window, GWLP_USERDATA));
  if (!state || !state->testing) return;
  state->synthetic_clock = true;
  state->synthetic_tick = monotonic_ms;
  state->synthetic_utc = utc_seconds;
  ReadinessPresentation(window, *state);
}

void EnableLauncherAutoChecksForTest(HWND window) {
  auto *state = reinterpret_cast<WindowState *>(GetWindowLongPtrW(window, GWLP_USERDATA));
  if (!state || !state->testing) return;
  state->auto_checks = true;
  PostMessageW(window, kAutoCheckMessage, 0, 0);
}

bool PollLauncherCheckForTest(HWND window) {
  auto *state = reinterpret_cast<WindowState *>(GetWindowLongPtrW(window, GWLP_USERDATA));
  if (!state || !state->testing) return false;
  return PollCheck(window, *state);
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
    if (HandleLauncherNavigationKey(window, message.message, message.wParam,
        (GetKeyState(VK_CONTROL) & 0x8000) != 0,
        (GetKeyState(VK_SHIFT) & 0x8000) != 0)) continue;
    auto *state = reinterpret_cast<WindowState *>(GetWindowLongPtrW(window, GWLP_USERDATA));
    const HWND target = state && state->operation ? state->operation : window;
    if (!IsDialogMessageW(target, &message)) {
      TranslateMessage(&message);
      DispatchMessageW(&message);
    }
  }
  if (IsWindow(window)) {
    ShowWindow(window, SW_HIDE);
    DestroyWindow(window); // WM_NCDESTROY drains any read-only worker.
  }
  CoUninitialize();
  return result == -1 ? 1 : static_cast<int>(message.wParam);
}

} // namespace mcla::launcher
