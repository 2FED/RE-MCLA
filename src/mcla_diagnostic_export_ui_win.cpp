#include "mcla_diagnostic_export_ui_win.h"

#include <shobjidl.h>
#include <wrl/client.h>

#include <algorithm>
#include <memory>
#include <new>
#include <optional>
#include <utility>
#include <vector>

namespace mcla::launcher::diagnostic_export {
namespace {
constexpr wchar_t kClass[] = L"MclaMetadataExportDialogV1";
constexpr int kWidth = 780, kHeight = 600;
constexpr DWORD kStyle = WS_CAPTION | WS_SYSMENU | WS_CLIPCHILDREN;
constexpr DWORD kExtendedStyle = WS_EX_DLGMODALFRAME | WS_EX_CONTROLPARENT;
struct Control { HWND window; int x, y, width, height; };
struct State {
  HWND window = nullptr, owner = nullptr;
  DialogCallback callback;
  std::wstring hint;
  UINT dpi = 96;
  HFONT font = nullptr, heading = nullptr;
  bool testing = false, rendering = false, busy = false, closing = false;
  bool destroyed = false, close_pending = false, cancel_sent = false, owner_disabled = false;
  DialogResult result;
  DialogRequest prepared;
  std::vector<Control> controls;
  std::optional<RECT> work_area;
  int scroll_x = 0, scroll_y = 0, content_width = 0, content_height = 0;
  Microsoft::WRL::ComPtr<IFileDialog> picker;
  ~State() { if (font) DeleteObject(font); if (heading) DeleteObject(heading); }
};
using Shared = std::shared_ptr<State>;
int Px(const State &s, int value) { return MulDiv(value, static_cast<int>(s.dpi), 96); }
Shared Get(HWND window) {
  const auto *holder = reinterpret_cast<Shared *>(GetWindowLongPtrW(window, GWLP_USERDATA));
  return holder ? *holder : Shared{};
}
std::wstring Text(HWND window, int id) {
  const auto control = GetDlgItem(window, id);
  const int count = GetWindowTextLengthW(control);
  if (count <= 0 || count > 32768) return {};
  std::wstring text(static_cast<size_t>(count) + 1, L'\0');
  const int copied = GetWindowTextW(control, text.data(), count + 1);
  text.resize(static_cast<size_t>(std::max(copied, 0))); return text;
}
DialogRequest Request(const State &s) {
  return {Text(s.window, ExportPackage), Text(s.window, ExportParent), {}};
}
bool SameInput(const DialogRequest &a, const DialogRequest &b) {
  return a.package_path == b.package_path && a.destination_parent == b.destination_parent;
}
std::optional<std::wstring> Utf8(std::string_view bytes) {
  if (bytes.empty() || bytes.size() > 1024 * 1024 || bytes.find('\0') != bytes.npos) return {};
  const int count = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, bytes.data(), static_cast<int>(bytes.size()), nullptr, 0);
  if (!count) return {};
  std::wstring result(static_cast<size_t>(count), L'\0');
  if (MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, bytes.data(), static_cast<int>(bytes.size()), result.data(), count) != count) return {};
  // Windows EDIT renders CRLF lines; source UTF-8 bytes and token stay untouched.
  std::wstring display; display.reserve(result.size() * 2);
  wchar_t previous = 0;
  for (const auto character : result) {
    if (character == L'\n' && previous != L'\r') display += L'\r';
    display += character; previous = character;
  }
  return display;
}
void EnableControls(State &s) {
  if (s.destroyed) return;
  for (int id : {ExportPackage, ExportBrowsePackage, ExportParent, ExportBrowseParent, ExportPreview})
    EnableWindow(GetDlgItem(s.window, id), !s.busy);
  EnableWindow(GetDlgItem(s.window, ExportSave), !s.busy && s.result.ready && !s.result.complete &&
      !s.result.confirmation_token.empty() && SameInput(Request(s), s.prepared));
  EnableWindow(GetDlgItem(s.window, ExportCancel), !s.busy);
}
void Status(State &s, std::wstring_view text) {
  if (!s.destroyed) SetDlgItemTextW(s.window, ExportStatus, std::wstring(text).c_str());
}
void CancelPlan(State &s) {
  if (s.cancel_sent) return;
  s.cancel_sent = true; // Set before callback to reject reentrant cancellation.
  const bool was_busy = s.busy; s.busy = true;
  try { if (s.callback) s.callback(DialogAction::Cancel, {}); } catch (...) {}
  s.busy = was_busy;
}
void FinishClose(const Shared &s) {
  if (s->closing) return;
  if (s->busy) {
    s->close_pending = true;
    if (s->picker) s->picker->Close(HRESULT_FROM_WIN32(ERROR_CANCELLED));
    return;
  }
  s->closing = true;
  CancelPlan(*s);
  if (s->destroyed) return;
  if (!s->destroyed && IsWindow(s->window)) DestroyWindow(s->window);
}
void Invalidate(const Shared &s) {
  if (s->rendering || s->destroyed) return;
  if (s->busy) return; // Reentrant input is also compared after callback returns.
  CancelPlan(*s);
  if (s->destroyed) return;
  s->result = {}; s->prepared = {};
  SetDlgItemTextW(s->window, ExportContents, L"");
  Status(*s, L"Selection changed. Preview again before saving; nothing has been exported.");
  if (s->close_pending) FinishClose(s); else EnableControls(*s);
}
void Dispatch(const Shared &s, DialogAction action) {
  if (s->busy || s->closing || s->destroyed || !s->callback) return;
  auto request = Request(*s);
  if (action == DialogAction::Confirm) {
    if (!s->result.ready || s->result.complete || s->result.confirmation_token.empty() || !SameInput(request, s->prepared)) {
      Invalidate(s); return;
    }
    request.confirmation_token = s->result.confirmation_token;
  } else {
    CancelPlan(*s);
    if (s->close_pending || s->destroyed) { FinishClose(s); return; }
    s->cancel_sent = false;
    s->result = {};
    SetDlgItemTextW(s->window, ExportContents, L"");
  }
  s->busy = true; EnableControls(*s);
  DialogResult result;
  try { result = s->callback(action, request); }
  catch (...) { result.status = L"Export operation failed. No completion is confirmed; inspect any retained partial output."; }
  s->busy = false;
  if (s->destroyed) { CancelPlan(*s); return; }
  if (s->close_pending) { FinishClose(s); return; }
  if (!SameInput(request, Request(*s))) { Invalidate(s); return; }
  if (action == DialogAction::Prepare) {
    const auto report = Utf8(result.report_json), inventory = Utf8(result.inventory_text);
    if (result.ready && !result.complete && report && inventory && !result.destination.empty() &&
        !result.confirmation_token.empty()) {
      s->prepared = request;
      s->result = std::move(result);
      const auto preview = L"DESTINATION\r\n" + s->result.destination + L"\r\n\r\nreport.json (UTF-8)\r\n" +
          *report + L"\r\nEXPORT-COMPLETE.txt (UTF-8)\r\n" + *inventory;
      if (!SetDlgItemTextW(s->window, ExportContents, preview.c_str()) ||
          GetWindowTextLengthW(GetDlgItem(s->window, ExportContents)) != static_cast<int>(preview.size())) {
        s->result = {}; CancelPlan(*s);
        if (!s->destroyed) SetDlgItemTextW(s->window, ExportContents, L"");
        Status(*s, L"The exact preview could not be displayed. Nothing can be exported.");
      } else {
        Status(*s, s->result.status.empty() ? L"Preview ready. Save report creates only these two files locally." : s->result.status);
      }
    } else {
      s->result = {};
      CancelPlan(*s);
      Status(*s, result.status.empty() ? L"Preview unavailable. Select a completed F10 package and a separate destination folder." : result.status);
    }
  } else {
    // A confirmation is one-shot, even if the backend reports failure.
    s->result.ready = false; s->result.confirmation_token.clear();
    s->result.complete = result.complete;
    Status(*s, result.status.empty() ? (result.complete ? L"Report saved locally. Nothing was uploaded." :
        L"Export did not complete. Any partial output is preserved; preview again for a new attempt.") : result.status);
  }
  if (s->close_pending) FinishClose(s); else EnableControls(*s);
}
void PickFolder(const Shared &s, int id) {
  if (s->busy || s->closing || s->destroyed) return;
  if (s->testing) { Status(*s, L"Folder pickers are disabled in hidden test windows."); return; }
  Microsoft::WRL::ComPtr<IFileDialog> picker;
  if (FAILED(CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&picker)))) {
    Status(*s, L"Folder picker unavailable. Enter an explicit folder path instead."); return;
  }
  DWORD flags = 0;
  if (FAILED(picker->GetOptions(&flags)) || FAILED(picker->SetOptions(flags | FOS_PICKFOLDERS | FOS_FORCEFILESYSTEM | FOS_PATHMUSTEXIST | FOS_NOCHANGEDIR))) return;
  picker->SetTitle(id == ExportPackage ? L"Choose one completed F10 package folder (not .partial)" : L"Choose a separate destination parent folder");
  const auto initial = id == ExportPackage ? s->hint : Text(s->window, id);
  if (!initial.empty()) {
    Microsoft::WRL::ComPtr<IShellItem> folder;
    if (SUCCEEDED(SHCreateItemFromParsingName(initial.c_str(), nullptr, IID_PPV_ARGS(&folder)))) picker->SetDefaultFolder(folder.Get());
  }
  s->picker = picker; s->busy = true; EnableControls(*s);
  const auto shown = picker->Show(s->window);
  std::wstring chosen;
  if (SUCCEEDED(shown)) {
    Microsoft::WRL::ComPtr<IShellItem> selected;
    PWSTR path = nullptr;
    if (SUCCEEDED(picker->GetResult(&selected)) && SUCCEEDED(selected->GetDisplayName(SIGDN_FILESYSPATH, &path))) {
      chosen = path; CoTaskMemFree(path);
    }
  }
  s->picker.Reset(); s->busy = false;
  if (s->destroyed) { CancelPlan(*s); return; }
  if (s->close_pending) { FinishClose(s); return; }
  if (!chosen.empty()) SetDlgItemTextW(s->window, id, chosen.c_str());
  EnableControls(*s);
}
RECT WorkArea(const State &s) {
  if (s.work_area) return *s.work_area;
  MONITORINFO monitor{sizeof(monitor)};
  if (GetMonitorInfoW(MonitorFromWindow(s.window, MONITOR_DEFAULTTONEAREST), &monitor)) return monitor.rcWork;
  RECT area{}; SystemParametersInfoW(SPI_GETWORKAREA, 0, &area, 0); return area;
}
void Positions(State &s) {
  for (const auto &c : s.controls)
    SetWindowPos(c.window, nullptr, Px(s, c.x) - s.scroll_x, Px(s, c.y) - s.scroll_y,
        Px(s, c.width), Px(s, c.height), SWP_NOZORDER | SWP_NOACTIVATE);
  InvalidateRect(s.window, nullptr, TRUE);
}
void ScrollBounds(State &s) {
  RECT client{}; GetClientRect(s.window, &client);
  s.scroll_x = std::clamp(s.scroll_x, 0, std::max(0, s.content_width - static_cast<int>(client.right)));
  s.scroll_y = std::clamp(s.scroll_y, 0, std::max(0, s.content_height - static_cast<int>(client.bottom)));
  SCROLLINFO horizontal{sizeof(horizontal), SIF_RANGE | SIF_PAGE | SIF_POS, 0, s.content_width - 1,
      static_cast<UINT>(client.right), s.scroll_x};
  SCROLLINFO vertical{sizeof(vertical), SIF_RANGE | SIF_PAGE | SIF_POS, 0, s.content_height - 1,
      static_cast<UINT>(client.bottom), s.scroll_y};
  SetScrollInfo(s.window, SB_HORZ, &horizontal, TRUE); SetScrollInfo(s.window, SB_VERT, &vertical, TRUE);
}
void Reveal(State &s, int id) {
  RECT client{}; GetClientRect(s.window, &client);
  for (const auto &c : s.controls) if (GetDlgCtrlID(c.window) == id) {
    const int left = Px(s, c.x), right = Px(s, c.x + c.width);
    const int top = Px(s, c.y), bottom = Px(s, c.y + c.height);
    if (left < s.scroll_x) s.scroll_x = left;
    else if (right > s.scroll_x + client.right) s.scroll_x = right - client.right;
    if (top < s.scroll_y) s.scroll_y = top;
    else if (bottom > s.scroll_y + client.bottom) s.scroll_y = bottom - client.bottom;
    ScrollBounds(s); Positions(s); return;
  }
}
void Scroll(State &s, int bar, int code) {
  SCROLLINFO info{sizeof(info), SIF_ALL}; GetScrollInfo(s.window, bar, &info);
  auto &position = bar == SB_VERT ? s.scroll_y : s.scroll_x;
  if (code == SB_LINEUP) position -= Px(s, 28);
  else if (code == SB_LINEDOWN) position += Px(s, 28);
  else if (code == SB_PAGEUP) position -= static_cast<int>(info.nPage);
  else if (code == SB_PAGEDOWN) position += static_cast<int>(info.nPage);
  else if (code == SB_TOP) position = 0;
  else if (code == SB_BOTTOM) position = info.nMax;
  else if (code == SB_THUMBTRACK || code == SB_THUMBPOSITION) position = info.nTrackPos;
  ScrollBounds(s); Positions(s);
}
int TextHeight(State &s, int id, int width, int minimum) {
  const auto control = GetDlgItem(s.window, id);
  const auto text = Text(s.window, id);
  HDC dc = GetDC(control);
  const auto old = SelectObject(dc, id == ExportHeading ? s.heading : s.font);
  RECT measure{0, 0, Px(s, width), 0};
  DrawTextW(dc, text.c_str(), static_cast<int>(text.size()), &measure, DT_CALCRECT | DT_WORDBREAK | DT_NOPREFIX);
  SelectObject(dc, old); ReleaseDC(control, dc);
  return std::max(minimum, static_cast<int>((static_cast<long long>(measure.bottom) * 96 + s.dpi - 1) / s.dpi) + 2);
}
void Arrange(State &s, int width, int height) {
  const auto place = [&](int id, int x, int y, int w, int h) {
    for (auto &c : s.controls) if (GetDlgCtrlID(c.window) == id) { c.x=x; c.y=y; c.width=w; c.height=h; return; }
  };
  const int inner = width - 40;
  int y = 16;
  const int title = TextHeight(s, ExportHeading, inner, 28);
  place(ExportHeading, 20, y, inner, title); y += title + 4;
  const int caveat = TextHeight(s, ExportCaveat, inner, 54);
  place(ExportCaveat, 20, y, inner, caveat); y += caveat + 8;
  for (const auto ids : {std::pair{ExportPackageLabel, ExportPackage}, std::pair{ExportParentLabel, ExportParent}}) {
    const int label = TextHeight(s, ids.first, inner, 20);
    place(ids.first, 20, y, inner, label); y += label + 4;
    place(ids.second, 20, y, inner - 112, 28);
    place(ids.second == ExportPackage ? ExportBrowsePackage : ExportBrowseParent, width - 120, y, 100, 28);
    y += 42;
  }
  place(ExportPreview, 20, y, 160, 32);
  if (width >= 650) {
    const int hint = TextHeight(s, ExportPreviewHint, width - 216, 24);
    place(ExportPreviewHint, 196, y + 3, width - 216, hint); y += std::max(32, hint + 3) + 10;
  } else {
    y += 38;
    const int hint = TextHeight(s, ExportPreviewHint, inner, 24);
    place(ExportPreviewHint, 20, y, inner, hint); y += hint + 8;
  }
  // Preview contracts first. Only genuinely smaller work areas need outer overflow.
  const int preview = std::clamp(height - y - 116, 64, 200);
  place(ExportContents, 20, y, inner, preview); y += preview + 12;
  place(ExportStatus, 20, y, inner, 44); y += 56;
  place(ExportSave, width - 320, y, 180, 32);
  place(ExportCancel, width - 120, y, 100, 32); y += 48;
  s.content_width = Px(s, width); s.content_height = Px(s, y);
}
void Layout(State &s) {
  if (s.destroyed) return;
  if (s.font) DeleteObject(s.font);
  if (s.heading) DeleteObject(s.heading);
  s.font = CreateFontW(-MulDiv(9, s.dpi, 72), 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
      DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe UI");
  s.heading = CreateFontW(-MulDiv(13, s.dpi, 72), 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE,
      DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe UI");
  for (const auto &c : s.controls) {
    SendMessageW(c.window, WM_SETFONT, reinterpret_cast<WPARAM>(GetDlgCtrlID(c.window) == ExportHeading ? s.heading : s.font), TRUE);
  }
  RECT bounds{0, 0, Px(s, kWidth), Px(s, kHeight)};
  AdjustWindowRectExForDpi(&bounds, kStyle, FALSE, kExtendedStyle, s.dpi);
  const auto work = WorkArea(s);
  const int outer_width = std::min(static_cast<int>(bounds.right - bounds.left), std::max(1, static_cast<int>(work.right - work.left) - 16));
  const int outer_height = std::min(static_cast<int>(bounds.bottom - bounds.top), std::max(1, static_cast<int>(work.bottom - work.top) - 16));
  RECT current{}; GetWindowRect(s.window, &current);
  const int x = std::clamp(static_cast<int>(current.left), static_cast<int>(work.left), static_cast<int>(work.right) - outer_width);
  const int y = std::clamp(static_cast<int>(current.top), static_cast<int>(work.top), static_cast<int>(work.bottom) - outer_height);
  const DWORD base_style = kStyle | (static_cast<DWORD>(GetWindowLongPtrW(s.window, GWL_STYLE)) & (WS_VISIBLE | WS_DISABLED));
  DWORD style = base_style;
  for (unsigned pass = 0; pass < 3; ++pass) {
    SetWindowLongPtrW(s.window, GWL_STYLE, style);
    SetWindowPos(s.window, nullptr, x, y, outer_width, outer_height, SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED);
    RECT client{}; GetClientRect(s.window, &client);
    Arrange(s, std::max(360, static_cast<int>(static_cast<long long>(client.right) * 96 / s.dpi)),
        static_cast<int>(static_cast<long long>(client.bottom) * 96 / s.dpi));
    const DWORD desired = base_style | (s.content_width > client.right ? WS_HSCROLL : 0) | (s.content_height > client.bottom ? WS_VSCROLL : 0);
    if (desired == style) break;
    style = desired;
  }
  ScrollBounds(s); Positions(s);
}
bool Controls(State &s) {
  s.rendering = true;
  const auto add = [&](const wchar_t *type, const wchar_t *text, DWORD style, int id, int x, int y, int w, int h) {
    if (std::wstring_view(type) == L"BUTTON") style |= BS_NOTIFY;
    const auto child = CreateWindowExW(0, type, text, WS_CHILD | WS_VISIBLE | style,
        0, 0, 0, 0, s.window, reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)), GetModuleHandleW(nullptr), nullptr);
    if (!child) return false;
    s.controls.push_back({child,x,y,w,h}); return true;
  };
  bool ok = true;
  ok &= add(L"STATIC", L"Export diagnostic metadata", 0, ExportHeading, 20, 16, 740, 28);
  ok &= add(L"STATIC", L"Local report only: no images, logs, dumps, saves or uploads.\r\nChoose one completed F10 package explicitly; no latest-package selection.\r\nNative crash packages are not supported in this export.", 0, ExportCaveat, 20, 48, 740, 54);
  ok &= add(L"STATIC", L"Completed F10 package folder", 0, ExportPackageLabel, 20, 110, 740, 20);
  ok &= add(L"EDIT", L"", WS_BORDER | WS_TABSTOP | ES_AUTOHSCROLL, ExportPackage, 20, 134, 628, 28);
  ok &= add(L"BUTTON", L"Browse...", WS_TABSTOP | BS_PUSHBUTTON, ExportBrowsePackage, 660, 134, 100, 28);
  ok &= add(L"STATIC", L"Destination parent folder", 0, ExportParentLabel, 20, 176, 740, 20);
  ok &= add(L"EDIT", L"", WS_BORDER | WS_TABSTOP | ES_AUTOHSCROLL, ExportParent, 20, 200, 628, 28);
  ok &= add(L"BUTTON", L"Browse...", WS_TABSTOP | BS_PUSHBUTTON, ExportBrowseParent, 660, 200, 100, 28);
  ok &= add(L"BUTTON", L"Preview report", WS_TABSTOP | BS_DEFPUSHBUTTON, ExportPreview, 20, 242, 160, 32);
  ok &= add(L"STATIC", L"Preview is read-only. Review both files and their destination below.", 0, ExportPreviewHint, 196, 247, 564, 24);
  ok &= add(L"EDIT", L"", WS_BORDER | WS_TABSTOP | ES_MULTILINE | ES_READONLY | ES_AUTOVSCROLL | ES_AUTOHSCROLL | WS_VSCROLL | WS_HSCROLL,
      ExportContents, 20, 284, 740, 200);
  ok &= add(L"EDIT", L"Select folders, then Preview report. Nothing is exported automatically.", WS_TABSTOP | ES_MULTILINE | ES_READONLY,
      ExportStatus, 20, 496, 740, 44);
  ok &= add(L"BUTTON", L"Save report", WS_TABSTOP | BS_PUSHBUTTON, ExportSave, 460, 552, 180, 32);
  ok &= add(L"BUTTON", L"Cancel", WS_TABSTOP | BS_PUSHBUTTON, ExportCancel, 660, 552, 100, 32);
  SendDlgItemMessageW(s.window, ExportPackage, EM_SETLIMITTEXT, 32768, 0);
  SendDlgItemMessageW(s.window, ExportParent, EM_SETLIMITTEXT, 32768, 0);
  SendDlgItemMessageW(s.window, ExportContents, EM_SETLIMITTEXT, 5 * 1024 * 1024, 0);
  s.rendering = false; Layout(s); EnableControls(s); return ok;
}
LRESULT CALLBACK Procedure(HWND window, UINT message, WPARAM wp, LPARAM lp) {
  auto s = Get(window);
  if (message == WM_NCCREATE) {
    const auto *create = reinterpret_cast<CREATESTRUCTW *>(lp);
    auto *holder = new (std::nothrow) Shared(*static_cast<Shared *>(create->lpCreateParams));
    if (!holder) return FALSE;
    SetWindowLongPtrW(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(holder));
    s = *holder; s->window = window;
    return TRUE;
  }
  if (!s) return DefWindowProcW(window, message, wp, lp);
  switch (message) {
    case WM_CREATE: return Controls(*s) ? 0 : -1;
    case DM_GETDEFID: return MAKELRESULT(ExportPreview, DC_HASDEFID);
    case DM_SETDEFID: return TRUE; // Never promote Save to the default action.
    case WM_COMMAND: {
      const int id = LOWORD(wp);
      if (HIWORD(wp) == EN_SETFOCUS || HIWORD(wp) == BN_SETFOCUS) { Reveal(*s, id); return 0; }
      if ((id == ExportPackage || id == ExportParent) && HIWORD(wp) == EN_CHANGE) { Invalidate(s); return 0; }
      // BS_NOTIFY also emits blur/paint notifications. They are not user
      // activation and must never prepare, publish, cancel or open a picker.
      if (HIWORD(wp) != BN_CLICKED) return 0;
      if (id == IDCANCEL || id == ExportCancel) { FinishClose(s); return 0; }
      if (s->busy || s->closing) return 0;
      if (id == IDOK || id == ExportPreview) Dispatch(s, DialogAction::Prepare);
      else if (id == ExportSave) Dispatch(s, DialogAction::Confirm);
      else if (id == ExportBrowsePackage) PickFolder(s, ExportPackage);
      else if (id == ExportBrowseParent) PickFolder(s, ExportParent);
      return 0;
    }
    case WM_KEYDOWN:
      if (wp == VK_ESCAPE) { FinishClose(s); return 0; }
      if (wp == VK_RETURN) { Dispatch(s, DialogAction::Prepare); return 0; }
      break;
    case WM_CLOSE: FinishClose(s); return 0;
    case WM_VSCROLL: Scroll(*s, SB_VERT, LOWORD(wp)); return 0;
    case WM_HSCROLL: Scroll(*s, SB_HORZ, LOWORD(wp)); return 0;
    case WM_MOUSEWHEEL:
      Scroll(*s, SB_VERT, GET_WHEEL_DELTA_WPARAM(wp) > 0 ? SB_LINEUP : SB_LINEDOWN); return 0;
    case WM_DPICHANGED: {
      s->dpi = HIWORD(wp) ? HIWORD(wp) : 96;
      const auto *rect = reinterpret_cast<const RECT *>(lp);
      if (rect) SetWindowPos(window, nullptr, rect->left, rect->top, 0, 0, SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
      Layout(*s); return 0;
    }
    case WM_NCDESTROY: {
      s->destroyed = true;
      if (s->picker) s->picker->Close(HRESULT_FROM_WIN32(ERROR_CANCELLED));
      if (!s->busy) CancelPlan(*s); else s->close_pending = true;
      if (IsWindow(s->owner)) {
        if (s->owner_disabled) EnableWindow(s->owner, TRUE);
        PostMessageW(s->owner, kExportDialogClosedMessage, 0, 0);
      }
      auto *holder = reinterpret_cast<Shared *>(GetWindowLongPtrW(window, GWLP_USERDATA));
      SetWindowLongPtrW(window, GWLP_USERDATA, 0); delete holder;
      return DefWindowProcW(window, message, wp, lp);
    }
  }
  return DefWindowProcW(window, message, wp, lp);
}
} // namespace

HWND CreateExportDialog(HWND owner, std::wstring_view diagnostics_hint, DialogCallback callback, bool hidden_for_test) {
  if (!IsWindow(owner) || !callback) return nullptr;
  WNDCLASSEXW wc{sizeof(wc)};
  wc.lpfnWndProc = Procedure; wc.hInstance = GetModuleHandleW(nullptr); wc.lpszClassName = kClass;
  wc.hCursor = LoadCursorW(nullptr, MAKEINTRESOURCEW(32512)); wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_BTNFACE + 1);
  if (!RegisterClassExW(&wc) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) return nullptr;
  auto state = std::make_shared<State>();
  state->owner = owner; state->callback = std::move(callback); state->hint = diagnostics_hint;
  state->testing = hidden_for_test; state->dpi = GetDpiForWindow(owner); if (!state->dpi) state->dpi = 96;
  const auto dialog = CreateWindowExW(kExtendedStyle, kClass, L"Export diagnostic metadata", kStyle,
      CW_USEDEFAULT, CW_USEDEFAULT, 0, 0, owner, nullptr, GetModuleHandleW(nullptr), &state);
  if (!dialog) return nullptr;
  if (!hidden_for_test) {
    state->owner_disabled = IsWindowEnabled(owner) != FALSE;
    if (state->owner_disabled) EnableWindow(owner, FALSE);
    RECT parent{}, bounds{}; GetWindowRect(owner, &parent); GetWindowRect(dialog, &bounds);
    SetWindowPos(dialog, nullptr, parent.left + std::max(0L, ((parent.right-parent.left)-(bounds.right-bounds.left))/2),
        parent.top + std::max(0L, ((parent.bottom-parent.top)-(bounds.bottom-bounds.top))/2), 0, 0,
        SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
    Layout(*state); // Re-clamp centered placement to the actual monitor work area.
    ShowWindow(dialog, SW_SHOW); SetFocus(GetDlgItem(dialog, ExportPackage));
  }
  return dialog;
}
bool SetExportDialogDpiForTest(HWND dialog, UINT dpi) {
  auto state = Get(dialog);
  if (!state || !state->testing || dpi < 96 || dpi > 240) return false;
  state->dpi = dpi; Layout(*state); return true;
}
bool SetExportDialogWorkAreaForTest(HWND dialog, const RECT &work_area) {
  auto state = Get(dialog);
  if (!state || !state->testing || work_area.right - work_area.left < 240 || work_area.bottom - work_area.top < 240) return false;
  state->work_area = work_area; state->scroll_x = state->scroll_y = 0; Layout(*state); return true;
}
} // namespace mcla::launcher::diagnostic_export
