// Guest-free native fixture for bounded image/crop/cleanup validation only.
// It does NOT validate game freshness, exact frame timestamps, cadence or FPS.
// No files or rejected pixels are saved; output is aggregate stdout metadata.
// No input injection, foreground forcing, topmost, resize, retry or game launch.
// C++20; link the image client plus user32.lib, gdi32.lib and dwmapi.lib.
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <dwmapi.h>

#include "mcla_diagnostic_image_client_win.h"

#include <array>
#include <atomic>
#include <filesystem>
#include <iostream>
#include <thread>

namespace {
namespace image = mcla::diagnostics::image;
constexpr int kWidth = 640;
constexpr int kHeight = 360;
constexpr UINT_PTR kTimer = 1;
constexpr wchar_t kClass[] = L"MclaF10ImageOnlyFixtureClass";
constexpr wchar_t kTitle[] = L"MCLA F10 image-only fixture";
constexpr std::array<COLORREF, 4> kColors{
    RGB(255, 0, 0), RGB(0, 255, 0), RGB(0, 0, 255), RGB(255, 255, 0)};

struct State {
  std::filesystem::path helper;
  std::string helper_hash;
  HWND hwnd = nullptr;
  ULONGLONG start = 0;
  std::jthread worker;
  std::atomic<bool> complete{false};
  image::CaptureResult result;
  bool started = false;
  bool painted = false;
  bool paint_failed = false;
  bool user_closed = false;
  bool deadline_expired = false;
  bool setup_failed = false;
};

void StopAndClose(State &state, HWND hwnd) {
  state.worker.request_stop();
  KillTimer(hwnd, kTimer);
  DestroyWindow(hwnd); // This fixture's own window only.
}

LRESULT CALLBACK WindowProcedure(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam) {
  State *state = reinterpret_cast<State *>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
  if (message == WM_NCCREATE) {
    const auto *create = reinterpret_cast<const CREATESTRUCTW *>(lparam);
    state = static_cast<State *>(create->lpCreateParams);
    SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(state));
    state->hwnd = hwnd;
  }
  if (!state) return DefWindowProcW(hwnd, message, wparam, lparam);
  switch (message) {
  case WM_ERASEBKGND:
    return 1;
  case WM_PAINT: {
    PAINTSTRUCT paint{};
    const HDC dc = BeginPaint(hwnd, &paint);
    bool okay = dc != nullptr;
    for (int quadrant = 0; quadrant < 4 && dc; ++quadrant) {
      const RECT rectangle{(quadrant % 2) * (kWidth / 2),
                            (quadrant / 2) * (kHeight / 2),
                            (quadrant % 2 + 1) * (kWidth / 2),
                            (quadrant / 2 + 1) * (kHeight / 2)};
      const HBRUSH brush = CreateSolidBrush(kColors[quadrant]);
      if (!brush) {
        okay = false;
      } else {
        if (!FillRect(dc, &rectangle, brush)) okay = false;
        if (!DeleteObject(brush)) okay = false;
      }
    }
    if (!EndPaint(hwnd, &paint)) okay = false;
    state->painted = state->painted || okay;
    state->paint_failed = state->paint_failed || !okay;
    return 0;
  }
  case WM_TIMER:
    if (wparam != kTimer) break;
    if (state->paint_failed) {
      StopAndClose(*state, hwnd);
      return 0;
    }
    if (state->complete.load(std::memory_order_acquire)) {
      StopAndClose(*state, hwnd);
      return 0;
    }
    if (GetTickCount64() - state->start >= 8000) {
      state->deadline_expired = true;
      StopAndClose(*state, hwnd);
      return 0;
    }
    if (!state->started && GetTickCount64() - state->start >= 1000) {
      // No foreground correction: natural ShowWindow activation must suffice.
      // A background fixture fails once, without starting capture or retrying.
      if (!state->painted || GetForegroundWindow() != hwnd) {
        state->setup_failed = true;
        StopAndClose(*state, hwnd);
        return 0;
      }
      state->started = true;
      try {
        state->worker = std::jthread([state, hwnd](std::stop_token stop) {
          try {
            state->result = image::CaptureOwnedWindow(
                reinterpret_cast<uintptr_t>(hwnd), state->helper, state->helper_hash, stop);
          } catch (...) {
            state->result = {};
            state->result.cleanup_complete = false; // Do not infer cleanup.
          }
          state->complete.store(true, std::memory_order_release);
        });
      } catch (...) {
        state->setup_failed = true;
        StopAndClose(*state, hwnd);
      }
    }
    return 0;
  case WM_CLOSE:
    state->user_closed = true;
    StopAndClose(*state, hwnd);
    return 0;
  case WM_DESTROY:
    PostQuitMessage(0);
    return 0;
  case WM_NCDESTROY:
    state->hwnd = nullptr;
    SetWindowLongPtrW(hwnd, GWLP_USERDATA, 0);
    return DefWindowProcW(hwnd, message, wparam, lparam);
  default:
    break;
  }
  return DefWindowProcW(hwnd, message, wparam, lparam);
}

bool ReviewAcceptedPixels(const image::CaptureResult &result) {
  if (!result.captured || !result.cleanup_complete ||
      result.failure != image::ClientFailure::kNone || result.helper_exit != 0 ||
      result.metadata.status != image::Status::kSuccess ||
      result.metadata.request.client_width != kWidth ||
      result.metadata.request.client_height != kHeight ||
      !image::ValidateResult(result.metadata.request, result.metadata, result.pixels.size())) {
    return false; // Never scan rejected/partial pixel buffers.
  }
  uint64_t bad_pixels = 0;
  for (size_t y = 0; y < kHeight; ++y) {
    for (size_t x = 0; x < kWidth; ++x) {
      const size_t quadrant = (y >= kHeight / 2 ? 2 : 0) + (x >= kWidth / 2 ? 1 : 0);
      const COLORREF expected = kColors[quadrant];
      const size_t offset = (y * kWidth + x) * 4;
      // Compare every RGB pixel. Alpha is not a color/freshness oracle.
      if (std::to_integer<unsigned>(result.pixels[offset]) != GetBValue(expected) ||
          std::to_integer<unsigned>(result.pixels[offset + 1]) != GetGValue(expected) ||
          std::to_integer<unsigned>(result.pixels[offset + 2]) != GetRValue(expected)) {
        ++bad_pixels;
      }
    }
  }
  std::cout << "accepted_rgb_pixels=" << kWidth * kHeight
            << " mismatched_rgb_pixels=" << bad_pixels << '\n';
  return bad_pixels == 0;
}
} // namespace

int wmain(int argc, wchar_t **argv) {
  if (argc != 3) {
    std::cout << "FAILED exact absolute helper EXE path and SHA256 required\n";
    return 1;
  }
  State state;
  state.helper = argv[1];
  const std::wstring hash = argv[2];
  if (hash.size() != 64 || hash.find_first_not_of(L"0123456789ABCDEF") != hash.npos) return 1;
  state.helper_hash.assign(hash.begin(), hash.end());
  const auto root = state.helper.root_name().wstring();
  if (!state.helper.is_absolute() || root.size() != 2 || root[1] != L':' ||
      state.helper.extension() != L".exe") {
    std::cout << "FAILED local absolute helper EXE path required\n";
    return 1;
  }
  if (!SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2)) {
    std::cout << "FAILED PMv2 setup\n";
    return 1;
  }
  const HINSTANCE instance = GetModuleHandleW(nullptr);
  WNDCLASSEXW window_class{};
  window_class.cbSize = sizeof(window_class);
  window_class.lpfnWndProc = WindowProcedure;
  window_class.hInstance = instance;
  window_class.lpszClassName = kClass;
  if (!RegisterClassExW(&window_class)) {
    std::cout << "FAILED window class setup\n";
    return 1;
  }
  HWND hwnd = CreateWindowExW(0, kClass, kTitle, WS_POPUP, 80, 80,
                               kWidth, kHeight, nullptr, nullptr, instance, &state);
  if (!hwnd) {
    UnregisterClassW(kClass, instance);
    std::cout << "FAILED window creation\n";
    return 1;
  }
  const DWM_WINDOW_CORNER_PREFERENCE corners = DWMWCP_DONOTROUND;
  state.start = GetTickCount64();
  if (FAILED(DwmSetWindowAttribute(hwnd, DWMWA_WINDOW_CORNER_PREFERENCE,
                                    &corners, sizeof(corners))) ||
      !SetTimer(hwnd, kTimer, 50, nullptr)) {
    state.setup_failed = true;
    StopAndClose(state, hwnd);
  } else {
    std::cout << "MCLA F10 image-only fixture: one capture, no retries, no saved pixels\n";
    ShowWindow(hwnd, SW_SHOWNORMAL);
    UpdateWindow(hwnd);
  }
  MSG message{};
  for (;;) {
    const BOOL received = GetMessageW(&message, nullptr, 0, 0);
    if (received <= 0) {
      if (received < 0) state.setup_failed = true;
      break;
    }
    TranslateMessage(&message);
    DispatchMessageW(&message);
  }
  state.worker.request_stop();
  if (state.hwnd) DestroyWindow(state.hwnd);
  // Always join: no forced process exit bypasses helper/Job cleanup. The client
  // bounds normal helper work/cancellation; an OS API stall is not real-time bounded.
  if (state.worker.joinable()) state.worker.join();
  const bool class_clean = UnregisterClassW(kClass, instance) != FALSE;
  const bool complete = state.complete.load(std::memory_order_acquire);
  const bool image_ok = complete && ReviewAcceptedPixels(state.result);
  const bool okay = image_ok && class_clean && state.painted && !state.paint_failed &&
                    !state.user_closed && !state.deadline_expired && !state.setup_failed;
  std::cout << "capture_started=" << state.started << " capture_complete=" << complete
            << " cleanup_complete=" << state.result.cleanup_complete
            << " client_failure=" << static_cast<unsigned>(state.result.failure)
            << " helper_exit=" << state.result.helper_exit
            << " helper_failure=" << static_cast<unsigned>(state.result.metadata.failure)
            << " user_closed=" << state.user_closed
            << " setup_failed=" << state.setup_failed
            << " paint_failed=" << state.paint_failed
            << " deadline_expired=" << state.deadline_expired << '\n';
  std::cout << (okay ? "IMAGE_ONLY_FIXTURE_PASS" : "FAILED") << '\n';
  std::cout.flush();
  return okay ? 0 : 1;
}
