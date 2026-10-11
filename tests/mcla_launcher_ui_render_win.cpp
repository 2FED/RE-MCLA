// Synthetic owned-window visual QA only; not a desktop/window-capture tool.
// Usage: mcla_launcher_ui_render_win.exe --output C:\explicit\new-fixture.png
// Link UI source with user32 gdi32 ole32 uuid gdiplus. No game backend.
#include "mcla_launcher_ui_win.h"

#include <objidl.h>
#include <gdiplus.h>
#include <wrl/client.h>

#include <algorithm>
#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>

namespace {
using namespace mcla::launcher;

void Require(bool condition, const char *message) {
  if (!condition) throw std::runtime_error(message);
}

struct OwnedWindow {
  HWND value = nullptr;
  ~OwnedWindow() { if (value) DestroyWindow(value); }
};

struct MemoryBitmap {
  HDC dc = nullptr;
  HBITMAP bitmap = nullptr;
  HGDIOBJ previous = nullptr;
  void *pixels = nullptr;
  ~MemoryBitmap() {
    if (dc && previous && previous != HGDI_ERROR) SelectObject(dc, previous);
    if (bitmap) DeleteObject(bitmap);
    if (dc) DeleteDC(dc);
  }
};

struct OutputFile {
  HANDLE value = INVALID_HANDLE_VALUE;
  ~OutputFile() { if (value != INVALID_HANDLE_VALUE) CloseHandle(value); }
};

UiState SyntheticState() {
  UiState state;
  state.library = {1, {
      {"1234567890abcdef1234567890abcdef", u"MAIN - synthetic campaign",
       u"D:/SyntheticProfiles/MAIN", Layout::ManagedExternalV1},
      {"2234567890abcdef1234567890abcdef", u"Clean - synthetic test",
       u"D:/SyntheticProfiles/Clean", Layout::ManagedExternalV1}},
      "1234567890abcdef1234567890abcdef"};
  state.build_root = L"C:/SyntheticBuilds/Internal-Candidate";
  state.game_root = L"E:/SyntheticGameData/MCLA";
  state.gpu_names = {L"Synthetic integrated GPU", L"Synthetic discrete GPU"};
  state.gpu_name = state.gpu_names[1];
  state.resolution_scale = 2;
  state.vsync = true;
  state.fullscreen = true;
  state.in_game_name = L"RoadDriver";
  state.status = L"SYNTHETIC VISUAL FIXTURE ONLY. No files verified, profile saved or game launched.";
  return state;
}

struct ContentBounds { HWND parent; LONG right = 0; LONG bottom = 0; };
BOOL CALLBACK MeasureChild(HWND child, LPARAM parameter) {
  auto &bounds = *reinterpret_cast<ContentBounds *>(parameter);
  // Combo-box internal children are not independent launcher controls.
  if (GetParent(child) != bounds.parent) return TRUE;
  RECT rect{};
  if (!GetWindowRect(child, &rect)) return TRUE;
  MapWindowPoints(nullptr, bounds.parent, reinterpret_cast<POINT *>(&rect), 2);
  bounds.right = std::max(bounds.right, rect.right);
  bounds.bottom = std::max(bounds.bottom, rect.bottom);
  return TRUE;
}

struct RenderChildren {
  HWND parent;
  HDC dc;
  unsigned controls = 0;
  bool valid = true;
};
BOOL CALLBACK PrintChild(HWND child, LPARAM parameter) {
  auto &render = *reinterpret_cast<RenderChildren *>(parameter);
  if (GetParent(child) != render.parent) return TRUE;
  RECT bounds{};
  if (!GetWindowRect(child, &bounds)) { render.valid = false; return FALSE; }
  MapWindowPoints(nullptr, render.parent, reinterpret_cast<POINT *>(&bounds), 2);
  const int saved = SaveDC(render.dc);
  if (!saved) { render.valid = false; return FALSE; }
  // WM_PRINT uses whole-window coordinates. Printing each owned control with
  // its nonclient border prevents the top-level caption/client translation
  // from leaking into a client-sized image. Never traverse desktop HWNDs.
  SetViewportOrgEx(render.dc, bounds.left, bounds.top, nullptr);
  IntersectClipRect(render.dc, 0, 0, bounds.right - bounds.left, bounds.bottom - bounds.top);
  SendMessageW(child, WM_PRINT, reinterpret_cast<WPARAM>(render.dc),
               PRF_NONCLIENT | PRF_CLIENT | PRF_ERASEBKGND);
  RestoreDC(render.dc, saved);

  RECT client{};
  if (!GetClientRect(child, &client)) { render.valid = false; return FALSE; }
  POINT origin{};
  MapWindowPoints(child, render.parent, &origin, 1);
  const int client_saved = SaveDC(render.dc);
  if (!client_saved) { render.valid = false; return FALSE; }
  SetViewportOrgEx(render.dc, origin.x, origin.y, nullptr);
  IntersectClipRect(render.dc, 0, 0, client.right, client.bottom);
  // Explicitly ask hidden edit/combo controls for client painting as well as
  // whole-window painting. Do not fake absent text if a native control refuses
  // to paint while hidden: this render remains visual-review evidence only.
  SendMessageW(child, WM_PRINTCLIENT, reinterpret_cast<WPARAM>(render.dc),
               PRF_CLIENT | PRF_ERASEBKGND);
  RestoreDC(render.dc, client_saved);
  ++render.controls;
  return TRUE;
}

void WriteAll(HANDLE file, const void *bytes, DWORD size) {
  const auto *cursor = static_cast<const unsigned char *>(bytes);
  while (size) {
    DWORD written = 0;
    Require(WriteFile(file, cursor, size, &written, nullptr) && written,
            "Image write failed; any partial new file was preserved.");
    cursor += written;
    size -= written;
  }
}

void WritePng(HANDLE file, const MemoryBitmap &surface, LONG width, LONG height, UINT dpi) {
  struct GdiPlusSession {
    ULONG_PTR token = 0;
    ~GdiPlusSession() { if (token) Gdiplus::GdiplusShutdown(token); }
  } session;
  Gdiplus::GdiplusStartupInput startup;
  Require(Gdiplus::GdiplusStartup(&session.token, &startup, nullptr) == Gdiplus::Ok,
          "Cannot start PNG serializer.");
  // BGRX source is serialized as opaque RGB; no scaling, drawing or pixel edits.
  Gdiplus::Bitmap bitmap(width, height, width * 4, PixelFormat32bppRGB,
                         static_cast<BYTE *>(surface.pixels));
  Require(bitmap.GetLastStatus() == Gdiplus::Ok &&
          bitmap.SetResolution(static_cast<Gdiplus::REAL>(dpi), static_cast<Gdiplus::REAL>(dpi)) == Gdiplus::Ok,
          "Cannot describe PNG source bitmap.");
  Microsoft::WRL::ComPtr<IStream> stream;
  Require(SUCCEEDED(CreateStreamOnHGlobal(nullptr, TRUE, &stream)), "Cannot allocate PNG stream.");
  constexpr CLSID png_encoder{0x557cf406, 0x1a04, 0x11d3,
                              {0x9a, 0x73, 0x00, 0x00, 0xf8, 0x1e, 0xf3, 0x2e}};
  Require(bitmap.Save(stream.Get(), &png_encoder, nullptr) == Gdiplus::Ok, "PNG serialization failed.");
  STATSTG statistics{};
  Require(SUCCEEDED(stream->Stat(&statistics, STATFLAG_NONAME)) &&
          statistics.cbSize.QuadPart > 0 && statistics.cbSize.QuadPart <= 128 * 1024 * 1024,
          "Encoded PNG exceeds bounded size.");
  HGLOBAL buffer = nullptr;
  Require(SUCCEEDED(GetHGlobalFromStream(stream.Get(), &buffer)), "Cannot access encoded PNG.");
  const void *bytes = GlobalLock(buffer);
  Require(bytes != nullptr, "Cannot lock encoded PNG.");
  try { WriteAll(file, bytes, static_cast<DWORD>(statistics.cbSize.QuadPart)); }
  catch (...) { GlobalUnlock(buffer); throw; }
  GlobalUnlock(buffer);
}

bool ExplicitOutputPath(std::wstring_view path) {
  return path.size() > 7 &&
      ((path[0] >= L'A' && path[0] <= L'Z') || (path[0] >= L'a' && path[0] <= L'z')) &&
      path[1] == L':' && (path[2] == L'\\' || path[2] == L'/') &&
      path.find(L':', 2) == std::wstring_view::npos &&
      (path.substr(path.size() - 4) == L".bmp" || path.substr(path.size() - 4) == L".png");
}
} // namespace

int wmain(int argc, wchar_t **argv) {
  try {
    Require(argc == 3 && std::wstring_view(argv[1]) == L"--output" && ExplicitOutputPath(argv[2]),
            "Usage: renderer --output <explicit new absolute local .png or .bmp path>");
    Require(SetThreadDpiAwarenessContext(DPI_AWARENESS_CONTEXT_SYSTEM_AWARE) != nullptr,
            "Cannot set deterministic system-aware DPI context.");
    const UINT dpi = GetDpiForSystem();
    unsigned refreshes = 0;
    bool unexpected_callback = false;
    OwnedWindow window;
    window.value = CreateLauncherUiForTest([&](UiAction action, const UiRequest &) {
      if (action == UiAction::Refresh) ++refreshes;
      else unexpected_callback = true;
      return SyntheticState();
    });
    Require(window.value && !IsWindowVisible(window.value), "Fixture was not created hidden.");
    Require(refreshes == 1 && !unexpected_callback, "Unexpected fixture backend action.");
    SetWindowTextW(GetDlgItem(window.value, UiProfileLabel), L"Copy for testing");
    SetWindowTextW(GetDlgItem(window.value, UiProfileRoot), L"D:/SyntheticProfiles/NewProfiles");
    SetWindowTextW(GetDlgItem(window.value, UiLegacySource),
                   L"D:/SyntheticProfiles/MAIN/backups/synthetic-snapshot");

    SendMessageW(window.value, WM_VSCROLL, SB_TOP, 0);
    SendMessageW(window.value, WM_HSCROLL, SB_TOP, 0);
    ContentBounds content{window.value};
    EnumChildWindows(window.value, MeasureChild, reinterpret_cast<LPARAM>(&content));
    Require(content.right > 0 && content.bottom > 0, "Could not measure own fixture controls.");
    const int margin = MulDiv(12, static_cast<int>(dpi), 96);
    RECT outer{0, 0, content.right + margin, content.bottom + margin};
    const auto style = static_cast<DWORD>(GetWindowLongPtrW(window.value, GWL_STYLE));
    const auto ex_style = static_cast<DWORD>(GetWindowLongPtrW(window.value, GWL_EXSTYLE));
    Require(AdjustWindowRectExForDpi(&outer, style, FALSE, ex_style, dpi) != FALSE,
            "Cannot calculate fixture-only full-content size.");
    const int width = outer.right - outer.left + GetSystemMetricsForDpi(SM_CXVSCROLL, dpi);
    const int height = outer.bottom - outer.top + GetSystemMetricsForDpi(SM_CYHSCROLL, dpi);
    Require(SetWindowPos(window.value, nullptr, 0, 0, width, height,
                        SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE) != FALSE,
            "Cannot size own hidden fixture.");
    SendMessageW(window.value, WM_VSCROLL, SB_TOP, 0);
    SendMessageW(window.value, WM_HSCROLL, SB_TOP, 0);
    // Resize/scroll handlers may reposition children. The final client bounds,
    // not pre-resize measurements or window-frame coordinates, own the proof.
    content = ContentBounds{window.value};
    EnumChildWindows(window.value, MeasureChild, reinterpret_cast<LPARAM>(&content));
    RECT client{};
    Require(GetClientRect(window.value, &client) != FALSE &&
            client.right >= content.right && client.bottom >= content.bottom,
            "Full fixture content does not fit the owned render surface.");
    const auto pixel_bytes = static_cast<uint64_t>(client.right) * client.bottom * 4;
    Require(pixel_bytes && pixel_bytes <= 128 * 1024 * 1024,
            "Fixture render surface exceeds bounded memory limit.");

    BITMAPINFO info{};
    info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    info.bmiHeader.biWidth = client.right;
    info.bmiHeader.biHeight = -client.bottom; // Top-down BGRX; no flipping pass.
    info.bmiHeader.biPlanes = 1;
    info.bmiHeader.biBitCount = 32;
    info.bmiHeader.biCompression = BI_RGB;
    info.bmiHeader.biSizeImage = static_cast<DWORD>(pixel_bytes);
    info.bmiHeader.biXPelsPerMeter = info.bmiHeader.biYPelsPerMeter =
        MulDiv(static_cast<int>(dpi), 10000, 254);
    MemoryBitmap surface;
    surface.dc = CreateCompatibleDC(nullptr); // Memory DC only, not desktop capture.
    Require(surface.dc != nullptr, "Cannot allocate memory DC.");
    surface.bitmap = CreateDIBSection(surface.dc, &info, DIB_RGB_COLORS,
                                     &surface.pixels, nullptr, 0);
    Require(surface.bitmap && surface.pixels, "Cannot allocate fixture DIB.");
    surface.previous = SelectObject(surface.dc, surface.bitmap);
    Require(surface.previous && surface.previous != HGDI_ERROR, "Cannot select fixture DIB.");
    FillRect(surface.dc, &client, GetSysColorBrush(COLOR_WINDOW));
    // Omit top-level WM_PRINT: DefWindowProc can offset children by the frame
    // even without PRF_NONCLIENT. Render each direct owned child at its actual
    // mapped client location. No BitBlt/GetDC(0), ShowWindow or external HWND.
    RenderChildren rendered{window.value, surface.dc};
    EnumChildWindows(window.value, PrintChild, reinterpret_cast<LPARAM>(&rendered));
    Require(rendered.valid && rendered.controls > 0, "Owned control rendering failed.");
    GdiFlush();
    Require(!IsWindowVisible(window.value) && refreshes == 1 && !unexpected_callback,
            "Fixture unexpectedly became visible or dispatched an action.");

    BITMAPFILEHEADER header{};
    header.bfType = 0x4d42;
    header.bfOffBits = sizeof(header) + sizeof(info.bmiHeader);
    header.bfSize = header.bfOffBits + static_cast<DWORD>(pixel_bytes);
    OutputFile output;
    output.value = CreateFileW(argv[2], GENERIC_WRITE, 0, nullptr, CREATE_NEW,
                               FILE_ATTRIBUTE_NORMAL, nullptr);
    Require(output.value != INVALID_HANDLE_VALUE,
            "Output must be a new file in an existing directory; no overwrite or directory creation.");
    const std::wstring_view output_path(argv[2]);
    if (output_path.substr(output_path.size() - 4) == L".png") {
      WritePng(output.value, surface, client.right, client.bottom, dpi);
    } else {
      WriteAll(output.value, &header, sizeof(header));
      WriteAll(output.value, &info.bmiHeader, sizeof(info.bmiHeader));
      WriteAll(output.value, surface.pixels, static_cast<DWORD>(pixel_bytes));
    }
    Require(FlushFileBuffers(output.value) != FALSE, "Cannot flush new image; partial file preserved.");
    std::cout << "OWNED_SYNTHETIC_WM_PRINT_IMAGE_WRITTEN visual review pending\n"
              << "dpi=" << dpi << " width=" << client.right << " height=" << client.bottom
              << " hidden=1 refreshes=" << refreshes << " controls=" << rendered.controls
              << " content_right=" << content.right << " content_bottom=" << content.bottom << "\n";
    return 0;
  } catch (const std::exception &error) {
    std::cerr << "Fixture render failed: " << error.what() << '\n';
    return 1;
  }
}
