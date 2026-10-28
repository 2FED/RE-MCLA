#include "mcla_diagnostic_export_ui_win.h"

#include <iostream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace {
using namespace mcla::launcher::diagnostic_export;
unsigned checks = 0, failures = 0;
void Check(bool value, const char *label) {
  ++checks;
  if (!value) { ++failures; std::cerr << "FAIL " << label << '\n'; }
}
void Click(HWND window, int id) {
  SendMessageW(window, WM_COMMAND, MAKEWPARAM(id, BN_CLICKED), reinterpret_cast<LPARAM>(GetDlgItem(window, id)));
}
std::wstring Text(HWND window, int id) {
  const auto control = GetDlgItem(window, id);
  const auto length = GetWindowTextLengthW(control);
  std::wstring value(static_cast<size_t>(length) + 1, L'\0');
  value.resize(static_cast<size_t>(GetWindowTextW(control, value.data(), length + 1)));
  return value;
}
bool Enabled(HWND window, int id) { return IsWindowEnabled(GetDlgItem(window, id)) != FALSE; }
DialogResult Ready() {
  DialogResult result;
  result.ready = true;
  result.destination = L"C:\\SyntheticExports\\metadata-fixture";
  result.report_json = "{\"schema\":\"synthetic\",\"label\":\"\xC3\xA9\"}\n";
  result.inventory_text = "MCLA-METADATA-EXPORT-V1\nABC123 36 report.json\n";
  result.confirmation_token = "fixture-token-EXACT";
  return result;
}
struct Call { DialogAction action; DialogRequest request; };
struct Recorder {
  std::vector<Call> calls;
  DialogResult prepared = Ready();
  DialogResult confirmed;
  std::function<void(DialogAction)> during;
  bool throw_prepare = false, throw_confirm = false, throw_cancel = false;
  Recorder() { confirmed.complete = true; }
  unsigned Count(DialogAction action) const {
    unsigned count = 0;
    for (const auto &call : calls) if (call.action == action) ++count;
    return count;
  }
  DialogResult Invoke(DialogAction action, const DialogRequest &request) {
    calls.push_back({action, request});
    if (during) during(action);
    if ((action == DialogAction::Prepare && throw_prepare) ||
        (action == DialogAction::Confirm && throw_confirm) ||
        (action == DialogAction::Cancel && throw_cancel)) throw std::runtime_error("PRIVATE_EXCEPTION_SENTINEL");
    if (action == DialogAction::Prepare) return prepared;
    if (action == DialogAction::Confirm) return confirmed;
    return {};
  }
};
LRESULT CALLBACK OwnerProc(HWND window, UINT message, WPARAM wp, LPARAM lp) {
  if (message == WM_NCCREATE) {
    const auto *create = reinterpret_cast<CREATESTRUCTW *>(lp);
    SetWindowLongPtrW(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(create->lpCreateParams));
  }
  if (message == kExportDialogClosedMessage) {
    const auto count = reinterpret_cast<unsigned *>(GetWindowLongPtrW(window, GWLP_USERDATA));
    if (count) ++*count;
    return 0;
  }
  return DefWindowProcW(window, message, wp, lp);
}
struct Fixture {
  Recorder record;
  unsigned notifications = 0;
  HWND owner = nullptr, dialog = nullptr;
  Fixture() {
    owner = CreateWindowExW(0, L"MclaExportHiddenFixtureOwner", L"Hidden owner", WS_OVERLAPPED,
        0, 0, 1200, 900, nullptr, nullptr, GetModuleHandleW(nullptr), &notifications);
    Check(owner != nullptr, "hidden owner created");
    dialog = CreateExportDialog(owner, L"C:\\SyntheticDiagnosticsHint", [this](DialogAction action, const DialogRequest &request) {
      return record.Invoke(action, request);
    }, true);
    Check(dialog != nullptr, "hidden export dialog created");
  }
  ~Fixture() {
    record.during = {};
    if (IsWindow(dialog)) DestroyWindow(dialog);
    if (IsWindow(owner)) DestroyWindow(owner);
  }
  void Paths() {
    SetDlgItemTextW(dialog, ExportPackage, L"C:\\SyntheticInput\\live-20261011T120000Z-123-1");
    SetDlgItemTextW(dialog, ExportParent, L"C:\\SyntheticExports");
  }
  void Preview() { Paths(); Click(dialog, ExportPreview); }
  void DrainNotifications() {
    MSG message{};
    while (PeekMessageW(&message, owner, kExportDialogClosedMessage, kExportDialogClosedMessage, PM_REMOVE)) DispatchMessageW(&message);
  }
};
void InitialAndPreview() {
  Fixture f;
  Check(!IsWindowVisible(f.dialog) && !IsWindowVisible(f.owner), "tests never show either window");
  Check(IsWindowEnabled(f.owner), "hidden dialog does not disable owner");
  Check(GetWindow(f.dialog, GW_OWNER) == f.owner, "dialog is owned");
  Check((GetWindowLongPtrW(f.dialog, GWL_STYLE) & (WS_THICKFRAME | WS_MAXIMIZEBOX | WS_MINIMIZEBOX)) == 0, "fixed non-resizable window");
  Check(Text(f.dialog, ExportPackage).empty(), "hint does not auto-select a source");
  Check(f.record.calls.empty(), "creation never prepares or exports");
  Check(!Enabled(f.dialog, ExportSave), "save initially disabled");
  Check(LOWORD(SendMessageW(f.dialog, DM_GETDEFID, 0, 0)) == ExportPreview, "Preview is default action");
  SendMessageW(f.dialog, DM_SETDEFID, ExportSave, 0);
  Check(LOWORD(SendMessageW(f.dialog, DM_GETDEFID, 0, 0)) == ExportPreview, "Save cannot become default");
  Click(f.dialog, ExportSave);
  Check(f.record.Count(DialogAction::Confirm) == 0, "synthetic Save before preview cannot confirm");
  f.Preview();
  Check(f.record.Count(DialogAction::Prepare) == 1, "one explicit preview");
  const auto &request = f.record.calls.back().request;
  Check(request.package_path == L"C:\\SyntheticInput\\live-20261011T120000Z-123-1", "exact source passed to callback");
  Check(request.destination_parent == L"C:\\SyntheticExports" && request.confirmation_token.empty(), "prepare has destination and no token");
  Check(Enabled(f.dialog, ExportSave), "valid preview enables Save");
  const auto contents = Text(f.dialog, ExportContents);
  Check(contents.find(f.record.prepared.destination) != contents.npos, "preview includes exact destination");
  Check(contents.find(L"{\"schema\":\"synthetic\",\"label\":\"\x00E9\"}\r\n") != contents.npos, "strict UTF-8 decoded without content truncation");
  Check(contents.find(L"MCLA-METADATA-EXPORT-V1\r\nABC123 36 report.json\r\n") != contents.npos, "inventory displayed with native newlines");
  const auto style = GetWindowLongPtrW(GetDlgItem(f.dialog, ExportContents), GWL_STYLE);
  Check((style & ES_READONLY) && (style & ES_MULTILINE) && (style & WS_HSCROLL) && (style & WS_VSCROLL), "preview read-only and scrollable both ways");
  const auto caveat = Text(f.dialog, ExportCaveat);
  Check(caveat.find(L"no images, logs, dumps, saves or uploads") != caveat.npos &&
      caveat.find(L"crash packages are not supported") != caveat.npos, "privacy and crash scope visibly stated");
  const auto count = f.record.calls.size();
  Click(f.dialog, ExportBrowsePackage); Click(f.dialog, ExportBrowseParent);
  Check(f.record.calls.size() == count && Enabled(f.dialog, ExportSave), "hidden pickers cannot open or alter plan");
  Check(Text(f.dialog, ExportContents) == contents, "picker refusal preserves exact preview");
}
void InvalidationAndConfirmation() {
  for (int field : {ExportPackage, ExportParent}) {
    Fixture f; f.Preview();
    const auto canceled = f.record.Count(DialogAction::Cancel);
    SetDlgItemTextW(f.dialog, field, L"C:\\ChangedExplicitInput");
    Check(f.record.Count(DialogAction::Cancel) == canceled + 1, "changed input cancels held plan once");
    Check(!Enabled(f.dialog, ExportSave) && Text(f.dialog, ExportContents).empty(), "changed input clears preview and disables save");
    Click(f.dialog, ExportSave);
    Check(f.record.Count(DialogAction::Confirm) == 0, "stale token cannot reach callback");
    Check(f.record.Count(DialogAction::Cancel) == canceled + 1, "repeated invalid action does not double-cancel");
  }
  Fixture f; f.Preview();
  const auto preview = Text(f.dialog, ExportContents);
  Click(f.dialog, ExportSave);
  Check(f.record.Count(DialogAction::Confirm) == 1, "explicit Save confirms once");
  const auto &call = f.record.calls.back();
  Check(call.request.confirmation_token == "fixture-token-EXACT", "confirmation uses exact preview token");
  Check(call.request.package_path == Text(f.dialog, ExportPackage) && call.request.destination_parent == Text(f.dialog, ExportParent), "confirmation bound to unchanged paths");
  Check(!Enabled(f.dialog, ExportSave), "success consumes UI confirmation");
  Check(Text(f.dialog, ExportStatus).find(L"Nothing was uploaded") != std::wstring::npos, "success says local only");
  Check(Text(f.dialog, ExportContents) == preview, "saved preview remains readable");
  Click(f.dialog, ExportSave);
  Check(f.record.Count(DialogAction::Confirm) == 1, "repeated command cannot confirm twice");
}
void InvalidAndFailedResults() {
  for (int mode = 0; mode < 6; ++mode) {
    Fixture f;
    if (mode == 0) f.record.prepared.confirmation_token.clear();
    if (mode == 1) f.record.prepared.destination.clear();
    if (mode == 2) f.record.prepared.report_json = "\xFF";
    if (mode == 3) f.record.prepared.inventory_text = std::string("x\0y", 3);
    if (mode == 4) f.record.prepared.complete = true;
    if (mode == 5) f.record.throw_prepare = true;
    f.Preview();
    Check(!Enabled(f.dialog, ExportSave), "invalid or throwing preview cannot enable Save");
    Check(Text(f.dialog, ExportContents).empty(), "invalid preview never displays payload");
    Check(Text(f.dialog, ExportStatus).find(L"PRIVATE_EXCEPTION_SENTINEL") == std::wstring::npos, "exception internals not displayed");
    Check(f.record.Count(DialogAction::Cancel) == 2, "invalid preview releases its plan");
    Click(f.dialog, ExportSave);
    Check(f.record.Count(DialogAction::Confirm) == 0, "invalid preview cannot confirm");
  }
  for (bool throws : {false, true}) {
    Fixture f; f.record.confirmed.complete = false; f.record.throw_confirm = throws; f.Preview();
    Click(f.dialog, ExportSave); Click(f.dialog, ExportSave);
    Check(f.record.Count(DialogAction::Confirm) == 1 && !Enabled(f.dialog, ExportSave), "failed confirmation still one-shot");
    Check(Text(f.dialog, ExportStatus).find(L"PRIVATE_EXCEPTION_SENTINEL") == std::wstring::npos, "confirmation exception details hidden");
  }
}
void DefaultAndReentrancy() {
  Fixture f; f.Preview();
  SendMessageW(f.dialog, WM_COMMAND, IDOK, 0);
  Check(f.record.Count(DialogAction::Prepare) == 2 && f.record.Count(DialogAction::Confirm) == 0, "dialog default command previews, never confirms");
  SendMessageW(f.dialog, WM_KEYDOWN, VK_RETURN, 0);
  Check(f.record.Count(DialogAction::Prepare) == 3 && f.record.Count(DialogAction::Confirm) == 0, "dialog Return previews, never confirms");
  MSG enter{}; enter.hwnd = GetDlgItem(f.dialog, ExportPackage); enter.message = WM_KEYDOWN; enter.wParam = VK_RETURN;
  Check(IsDialogMessageW(f.dialog, &enter), "caller's native dialog routing consumes Enter");
  Check(f.record.Count(DialogAction::Prepare) == 4 && f.record.Count(DialogAction::Confirm) == 0, "native Enter routing previews, never confirms");
  f.record.during = [&](DialogAction action) {
    if (action != DialogAction::Prepare) return;
    Check(!Enabled(f.dialog, ExportSave) && !Enabled(f.dialog, ExportPreview) && !Enabled(f.dialog, ExportBrowsePackage), "callback disables mutating controls");
    Click(f.dialog, ExportBrowsePackage); Click(f.dialog, ExportBrowseParent);
    Click(f.dialog, ExportSave); Click(f.dialog, ExportPreview);
  };
  Click(f.dialog, ExportPreview);
  Check(f.record.Count(DialogAction::Prepare) == 5 && f.record.Count(DialogAction::Confirm) == 0, "reentrant actions suppressed while callback runs");
  f.record.during = [&](DialogAction action) {
    if (action == DialogAction::Prepare) SetDlgItemTextW(f.dialog, ExportParent, L"C:\\ChangedDuringCallback");
  };
  Click(f.dialog, ExportPreview);
  Check(!Enabled(f.dialog, ExportSave) && Text(f.dialog, ExportContents).empty(), "reentrant path change discards returned plan");
}
void NonClickNotifications() {
  for(const int id:{ExportSave,ExportCancel,ExportPreview,ExportBrowsePackage,ExportBrowseParent}) {
    Fixture f; f.Preview();
    const auto calls=f.record.calls.size();
    const auto preview=Text(f.dialog,ExportContents), status=Text(f.dialog,ExportStatus);
    SendMessageW(f.dialog,WM_COMMAND,MAKEWPARAM(id,BN_KILLFOCUS),
                 reinterpret_cast<LPARAM>(GetDlgItem(f.dialog,id)));
    Check(IsWindow(f.dialog),"button losing focus must not close export dialog");
    Check(f.record.calls.size()==calls && f.record.Count(DialogAction::Confirm)==0,
          "button losing focus must never prepare confirm cancel or open picker");
    if(IsWindow(f.dialog))
      Check(Text(f.dialog,ExportContents)==preview && Text(f.dialog,ExportStatus)==status && Enabled(f.dialog,ExportSave),
            "button blur preserves exact preview and ready state");
  }
}
void Closing() {
  for (int mode = 0; mode < 4; ++mode) {
    Fixture f; f.Preview();
    const auto canceled = f.record.Count(DialogAction::Cancel);
    if (mode == 0) Click(f.dialog, ExportCancel);
    if (mode == 1) SendMessageW(f.dialog, WM_KEYDOWN, VK_ESCAPE, 0);
    if (mode == 2) SendMessageW(f.dialog, WM_CLOSE, 0, 0);
    if (mode == 3) SendMessageW(f.dialog, WM_COMMAND, IDCANCEL, 0);
    Check(!IsWindow(f.dialog), "Cancel/Escape/close destroys dialog");
    Check(f.record.Count(DialogAction::Cancel) == canceled + 1, "close releases plan exactly once");
    Check(IsWindowEnabled(f.owner), "owner remains enabled after hidden close");
    f.DrainNotifications();
    Check(f.notifications == 1, "owner receives one close notification");
  }
  {
    Fixture f; f.Preview();
    const auto canceled = f.record.Count(DialogAction::Cancel);
    DestroyWindow(f.owner);
    Check(!IsWindow(f.dialog), "owner destruction destroys owned dialog");
    Check(f.record.Count(DialogAction::Cancel) == canceled + 1, "owner destruction releases plan");
  }
  for (bool destroy_owner : {false, true}) {
    Fixture f; f.Paths();
    f.record.during = [&](DialogAction action) {
      if (action != DialogAction::Prepare) return;
      if (destroy_owner) DestroyWindow(f.owner);
      else {
        SendMessageW(f.dialog, WM_CLOSE, 0, 0);
        Check(IsWindow(f.dialog), "callback close deferred until callback returns");
      }
    };
    Click(f.dialog, ExportPreview);
    Check(!IsWindow(f.dialog), "reentrant close safely destroys window");
    Check(f.record.Count(DialogAction::Prepare) == 1 && f.record.Count(DialogAction::Cancel) == 2, "reentrant close cancels exactly returned plan");
  }
  {
    Fixture f; f.Preview(); f.record.throw_cancel = true;
    SendMessageW(f.dialog, WM_CLOSE, 0, 0);
    Check(!IsWindow(f.dialog), "throwing cancellation cannot prevent window closure");
  }
}
void Layout120() {
  Fixture f;
  Check(SetExportDialogWorkAreaForTest(f.dialog, RECT{0, 0, 1920, 1080}), "deterministic normal work area accepted");
  Check(SetExportDialogDpiForTest(f.dialog, 120), "hidden layout can be measured at120DPI");
  Check(!(GetWindowLongPtrW(f.dialog, GWL_STYLE) & (WS_VSCROLL | WS_HSCROLL)), "normal work area has no outer scrollbars");
  Check(!SetExportDialogDpiForTest(f.dialog, 0) && !SetExportDialogDpiForTest(f.dialog, 500), "invalid fixture DPI refused");
  RECT client{}; GetClientRect(f.dialog, &client);
  for (int id = ExportPackage; id <= ExportPreviewHint; ++id) {
    HWND child = GetDlgItem(f.dialog, id); RECT rectangle{}; GetWindowRect(child, &rectangle);
    MapWindowPoints(nullptr, f.dialog, reinterpret_cast<POINT *>(&rectangle), 2);
    Check(child && rectangle.left >= 0 && rectangle.top >= 0 && rectangle.right <= client.right && rectangle.bottom <= client.bottom,
        "every120DPI control fits client area");
  }
  for (int id : {ExportHeading, ExportCaveat, ExportPackageLabel, ExportParentLabel, ExportPreviewHint, ExportStatus}) {
    const auto child = GetDlgItem(f.dialog, id);
    RECT bounds{}; GetClientRect(child, &bounds);
    HDC dc = GetDC(child);
    auto old = SelectObject(dc, reinterpret_cast<HFONT>(SendMessageW(child, WM_GETFONT, 0, 0)));
    auto measured = bounds; const auto text = Text(f.dialog, id);
    DrawTextW(dc, text.c_str(), static_cast<int>(text.size()), &measured, DT_CALCRECT | DT_WORDBREAK | DT_NOPREFIX);
    Check(measured.bottom - measured.top <= bounds.bottom - bounds.top, "120DPI visible explanatory text fits height");
    SelectObject(dc, old); ReleaseDC(child, dc);
  }
  Check(!IsWindowVisible(f.dialog), "DPI test never shows window");
}
RECT ChildRect(HWND dialog, int id) {
  RECT result{}; GetWindowRect(GetDlgItem(dialog, id), &result);
  MapWindowPoints(nullptr, dialog, reinterpret_cast<POINT *>(&result), 2); return result;
}
void WorkAreaLayouts() {
  struct Scenario { UINT dpi; RECT work; };
  for (const auto scenario : {Scenario{120, {40, 30, 1064, 798}}, Scenario{192, {-1280, 0, 0, 720}},
      Scenario{144, {0, 0, 800, 600}}, Scenario{240, {20, 30, 500, 510}}}) {
    Fixture f; f.Preview();
    const auto calls = f.record.calls.size();
    const auto content = Text(f.dialog, ExportContents);
    Check(SetExportDialogWorkAreaForTest(f.dialog, scenario.work) && SetExportDialogDpiForTest(f.dialog, scenario.dpi), "small work area and high DPI accepted");
    RECT outer{}, client{}; GetWindowRect(f.dialog, &outer); GetClientRect(f.dialog, &client);
    Check(outer.left >= scenario.work.left && outer.top >= scenario.work.top && outer.right <= scenario.work.right && outer.bottom <= scenario.work.bottom,
        "entire dialog stays within work area including negative monitor origin");
    Check(!IsWindowVisible(f.dialog) && IsWindowEnabled(f.owner), "adaptive hidden layout never shows or disables owner");
    Check(f.record.calls.size() == calls && Text(f.dialog, ExportContents) == content && Enabled(f.dialog, ExportSave), "layout changes preserve preview/token without callback or export");
    const auto style = GetWindowLongPtrW(f.dialog, GWL_STYLE);
    Check(!(style & (WS_THICKFRAME | WS_MAXIMIZEBOX)), "adaptive dialog stays fixed-size");
    // Native focus notifications reveal keyboard targets even when overflow is needed.
    SendMessageW(f.dialog, WM_COMMAND, MAKEWPARAM(ExportCancel, BN_SETFOCUS), reinterpret_cast<LPARAM>(GetDlgItem(f.dialog, ExportCancel)));
    auto cancel = ChildRect(f.dialog, ExportCancel);
    Check(cancel.top >= 0 && cancel.bottom <= client.bottom && cancel.left >= 0 && cancel.right <= client.right,
        "Cancel becomes fully reachable after keyboard focus");
    Check(f.record.calls.size() == calls, "focus reveal never cancels or confirms");
    SendMessageW(f.dialog, WM_VSCROLL, SB_TOP, 0); SendMessageW(f.dialog, WM_HSCROLL, SB_LEFT, 0);
    auto source = ChildRect(f.dialog, ExportPackage);
    if (style & WS_VSCROLL) {
      SendMessageW(f.dialog, WM_VSCROLL, SB_BOTTOM, 0);
      Check(ChildRect(f.dialog, ExportPackage).top < source.top, "vertical scrollbar reaches lower controls");
      SendMessageW(f.dialog, WM_VSCROLL, SB_TOP, 0);
      Check(ChildRect(f.dialog, ExportPackage).top == source.top, "vertical scroll returns to start");
    }
    if (style & WS_HSCROLL) {
      SendMessageW(f.dialog, WM_HSCROLL, SB_RIGHT, 0);
      Check(ChildRect(f.dialog, ExportPackage).left < source.left, "extreme width fallback scrolls horizontally");
    }
    // Layout is not an implicit user confirmation.
    Check(f.record.Count(DialogAction::Confirm) == 0, "responsive and overflow paths never export");
  }
  Fixture f;
  Check(!SetExportDialogWorkAreaForTest(f.dialog, RECT{0, 0, 100, 100}), "invalid tiny test work area refused");
  Check(SetExportDialogWorkAreaForTest(f.dialog, RECT{0, 0, 1100, 650}), "compact work area accepted");
  Check(SetExportDialogDpiForTest(f.dialog, 120), "compact120DPI accepted");
  if(GetWindowLongPtrW(f.dialog,GWL_STYLE)&(WS_VSCROLL|WS_HSCROLL)) {
    RECT outer{},client{}; GetWindowRect(f.dialog,&outer); GetClientRect(f.dialog,&client);
    SCROLLINFO vertical{sizeof(vertical),SIF_ALL}; GetScrollInfo(f.dialog,SB_VERT,&vertical);
    std::cerr<<"Compact layout: outer="<<outer.right-outer.left<<"x"<<outer.bottom-outer.top
             <<" client="<<client.right<<"x"<<client.bottom<<" vertical="<<vertical.nMax<<"/"<<vertical.nPage<<'\n';
  }
  Check(!(GetWindowLongPtrW(f.dialog, GWL_STYLE) & (WS_VSCROLL | WS_HSCROLL)), "preview shrinks before adding outer scrollbars");
  const auto preview = ChildRect(f.dialog, ExportContents);
  Check(preview.bottom - preview.top < 250, "compact mode contracts preview height");
}
} // namespace

int main() {
  SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
  WNDCLASSEXW wc{sizeof(wc)}; wc.lpfnWndProc = OwnerProc;
  wc.hInstance = GetModuleHandleW(nullptr); wc.lpszClassName = L"MclaExportHiddenFixtureOwner";
  if (!RegisterClassExW(&wc) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) return 2;
  InitialAndPreview(); InvalidationAndConfirmation(); InvalidAndFailedResults();
  DefaultAndReentrancy(); NonClickNotifications(); Closing(); Layout120(); WorkAreaLayouts();
  std::cout << checks << " checks, " << failures << " failures\n";
  return failures ? 1 : 0;
}
