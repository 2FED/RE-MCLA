// Standalone, synthetic input-delivery census. Never injects input or runs MCLA.
// Build (Developer Command Prompt):
// cl /nologo /std:c++17 /EHsc /W4 /DUNICODE /D_UNICODE <this-file> user32.lib gdi32.lib /Fe:<new-exe>
// Run: <new-exe> --self-test OR <new-exe> C:\explicit\existing-parent\new-result.json
// A completed census is NOT proof of game input delivery, polling, or root cause.
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <array>
#include <atomic>
#include <cstdint>
#include <cstdio>
#include <cwchar>
#include <functional>
#include <mutex>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

namespace {
constexpr size_t kMaxEvents = 1024;
constexpr size_t kMaxSamples = 2048;
constexpr DWORD kPollMs = 33;
constexpr DWORD kDurationMs = 60000;
constexpr UINT_PTR kUiTimer = 1;
constexpr UINT kWorkerClose = WM_APP + 1;
enum class Kind : unsigned { Down = 1, Up = 2, FocusGain = 3, FocusLoss = 4 };
struct Event { int64_t qpc; Kind kind; unsigned repeat; bool foreground; };
struct Sample {
  int64_t before, after;
  bool foreground, event_down, async_queried, async_down;
};
struct Census {
  std::array<Event, kMaxEvents> events{};
  std::array<Sample, kMaxSamples> samples{};
  size_t event_count = 0, sample_count = 0;
  uint64_t down_count = 0, up_count = 0, focus_transitions = 0;
  uint64_t polls = 0, queried_polls = 0, sampled_down = 0, async_sampled_down = 0;
  uint64_t event_overflow = 0, sample_overflow = 0;
  bool event_down = false;
  void AddEvent(Event e) {
    if (e.kind == Kind::Down) { ++down_count; event_down = true; }
    else if (e.kind == Kind::Up) { ++up_count; event_down = false; }
    else {
      ++focus_transitions;
      if (e.kind == Kind::FocusLoss) event_down = false;
    }
    if (event_count < events.size()) events[event_count++] = e;
    else ++event_overflow;
  }
  void AddSample(Sample s) {
    ++polls;
    if (s.async_queried) ++queried_polls;
    if (s.event_down) ++sampled_down;
    if (s.async_queried && s.async_down) ++async_sampled_down;
    if (sample_count < samples.size()) samples[sample_count++] = s;
    else ++sample_overflow;
  }
};
struct State {
  Census census;
  std::mutex mutex;
  HANDLE stop = nullptr;
  HWND window = nullptr;
  int64_t frequency = 0, start = 0, end = 0;
  ULONGLONG start_tick = 0;
  std::atomic<DWORD> error{ERROR_SUCCESS};
  unsigned close_reason = 0; // 1=user WM_CLOSE, 2=fixed deadline, 3=error
};
int64_t Now() {
  LARGE_INTEGER q{};
  return QueryPerformanceCounter(&q) ? q.QuadPart : 0;
}
void SetError(State& state, DWORD code) {
  DWORD expected = ERROR_SUCCESS;
  state.error.compare_exchange_strong(expected, code ? code : ERROR_GEN_FAILURE);
}
std::string Serialize(const Census& c, int64_t frequency, int64_t start,
                      int64_t end, DWORD error, unsigned close_reason) {
  std::ostringstream o;
  o << "{\"schema\":\"mcla-input-pulse-fixture-v1\","
       "\"game_launched\":false,\"save_writes\":false,\"input_injected\":false,"
       "\"game_input_proven\":false,\"root_cause_proven\":false,"
       "\"sampling\":\"33ms event-derived Return state; clear on key-up/focus-loss\","
       "\"secondary_sampling\":\"GetAsyncKeyState(VK_RETURN) high bit; own foreground only\","
       "\"poll_interval_ms\":" << kPollMs << ",\"duration_limit_ms\":" << kDurationMs
    << ",\"qpc_frequency\":" << frequency << ",\"start_qpc\":" << start
    << ",\"end_qpc\":" << end << ",\"win32_error\":" << error
    << ",\"close_reason\":" << close_reason
    << ",\"completed\":" << (error == 0 && close_reason != 0 ? "true" : "false")
    << ",\"down_messages\":" << c.down_count << ",\"up_messages\":" << c.up_count
    << ",\"focus_transitions\":" << c.focus_transitions << ",\"polls\":" << c.polls
    << ",\"queried_polls\":" << c.queried_polls << ",\"sampled_down\":" << c.sampled_down
    << ",\"async_sampled_down\":" << c.async_sampled_down
    << ",\"event_overflow\":" << c.event_overflow
    << ",\"sample_overflow\":" << c.sample_overflow
    << ",\"event_kind_legend\":{\"1\":\"return_down\",\"2\":\"return_up\","
       "\"3\":\"focus_gain\",\"4\":\"focus_loss\"},\"events\":[";
  for (size_t i = 0; i < c.event_count; ++i) {
    const auto& e = c.events[i];
    if (i) o << ',';
    o << "{\"qpc\":" << e.qpc << ",\"kind\":" << static_cast<unsigned>(e.kind)
      << ",\"repeat\":" << e.repeat << ",\"foreground\":"
      << (e.foreground ? "true" : "false") << '}';
  }
  o << "],\"samples\":[";
  for (size_t i = 0; i < c.sample_count; ++i) {
    const auto& s = c.samples[i];
    if (i) o << ',';
    o << "{\"qpc_before\":" << s.before << ",\"qpc_after\":" << s.after
      << ",\"foreground_before\":" << (s.foreground ? "true" : "false")
      << ",\"event_down\":" << (s.event_down ? "true" : "false")
      << ",\"async_queried\":" << (s.async_queried ? "true" : "false")
      << ",\"async_down\":"
      << (s.async_queried ? (s.async_down ? "true" : "false") : "null") << '}';
  }
  o << "]}\n";
  return o.str();
}

// Hold every existing ancestor without DELETE sharing. Reject junctions/symlinks,
// UNC/device paths, relative components and ADS; never create parent directories.
// This narrow diagnostic deliberately rejects paths >= MAX_PATH.
struct Output {
  HANDLE file = INVALID_HANDLE_VALUE;
  std::vector<HANDLE> ancestors;
  ~Output() {
    if (file != INVALID_HANDLE_VALUE) CloseHandle(file);
    for (auto h : ancestors) CloseHandle(h);
  }
  bool Open(const std::wstring& path, DWORD& error) {
    auto fail = [&](DWORD e) { error = e ? e : ERROR_GEN_FAILURE; return false; };
    if (path.size() < 4 || path.size() >= MAX_PATH || path[1] != L':' ||
        path[2] != L'\\' || !((path[0] >= L'A' && path[0] <= L'Z') ||
                              (path[0] >= L'a' && path[0] <= L'z')))
      return fail(ERROR_BAD_PATHNAME);
    size_t component = 3;
    for (size_t i = 3; i <= path.size(); ++i) {
      if (i < path.size() && (path[i] == L':' || path[i] == L'/' || path[i] < 32))
        return fail(ERROR_BAD_PATHNAME);
      if (i != path.size() && path[i] != L'\\') continue;
      const auto name = path.substr(component, i - component);
      if (name.empty() || name == L"." || name == L".." ||
          name.back() == L'.' || name.back() == L' ')
        return fail(ERROR_BAD_PATHNAME);
      component = i + 1;
    }
    const size_t last = path.find_last_of(L'\\');
    for (size_t end = 3;;) {
      HANDLE h = CreateFileW(path.substr(0, end).c_str(), FILE_READ_ATTRIBUTES,
                            FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING,
                            FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OPEN_REPARSE_POINT, nullptr);
      if (h == INVALID_HANDLE_VALUE) return fail(GetLastError());
      ancestors.push_back(h);
      BY_HANDLE_FILE_INFORMATION info{};
      if (!GetFileInformationByHandle(h, &info)) return fail(GetLastError());
      if (!(info.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) ||
          (info.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT))
        return fail(ERROR_REPARSE_TAG_INVALID);
      if (end >= last) break;
      const auto next = path.find(L'\\', end == 3 ? end : end + 1);
      if (next == std::wstring::npos || next > last) return fail(ERROR_BAD_PATHNAME);
      end = next;
    }
    file = CreateFileW(path.c_str(), GENERIC_WRITE, FILE_SHARE_READ, nullptr,
                       CREATE_NEW, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OPEN_REPARSE_POINT, nullptr);
    if (file == INVALID_HANDLE_VALUE) return fail(GetLastError());
    BY_HANDLE_FILE_INFORMATION info{};
    if (!GetFileInformationByHandle(file, &info)) return fail(GetLastError());
    if (info.dwFileAttributes & (FILE_ATTRIBUTE_REPARSE_POINT | FILE_ATTRIBUTE_DIRECTORY))
      return fail(ERROR_REPARSE_TAG_INVALID);
    return true;
  }
  bool Write(const std::string& bytes) {
    if (bytes.size() > MAXDWORD) return false;
    DWORD written = 0;
    return WriteFile(file, bytes.data(), static_cast<DWORD>(bytes.size()), &written, nullptr) &&
           written == bytes.size() && FlushFileBuffers(file);
  }
};

LRESULT CALLBACK WindowProc(HWND window, UINT message, WPARAM wparam, LPARAM lparam) {
  auto* state = reinterpret_cast<State*>(GetWindowLongPtrW(window, GWLP_USERDATA));
  if (message == WM_NCCREATE) {
    const auto* create = reinterpret_cast<CREATESTRUCTW*>(lparam);
    state = static_cast<State*>(create->lpCreateParams);
    SetWindowLongPtrW(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(state));
  }
  if (!state) return DefWindowProcW(window, message, wparam, lparam);
  if ((message == WM_KEYDOWN || message == WM_KEYUP) && wparam == VK_RETURN) {
    std::lock_guard<std::mutex> lock(state->mutex);
    state->census.AddEvent({Now(), message == WM_KEYDOWN ? Kind::Down : Kind::Up,
                           static_cast<unsigned>(lparam & 0xffff), GetForegroundWindow() == window});
    return 0;
  }
  switch (message) {
    case WM_SETFOCUS:
    case WM_KILLFOCUS: {
      std::lock_guard<std::mutex> lock(state->mutex);
      state->census.AddEvent({Now(), message == WM_SETFOCUS ? Kind::FocusGain : Kind::FocusLoss,
                             0, GetForegroundWindow() == window});
      return 0;
    }
    case WM_TIMER:
      if (wparam == kUiTimer) {
        if (GetTickCount64() - state->start_tick >= kDurationMs) {
          state->close_reason = 2;
          SendMessageW(window, WM_CLOSE, 0, 0);
        } else InvalidateRect(window, nullptr, FALSE);
      }
      return 0;
    case WM_PAINT: {
      wchar_t text[768]{};
      {
        std::lock_guard<std::mutex> lock(state->mutex);
        swprintf_s(text, L"Synthetic Return input census ONLY. No game or save writes.\n"
          L"Closes automatically after 60 seconds. No input is generated.\n"
          L"Return messages: DOWN %llu / UP %llu\n33ms samples: %llu; down observed: %llu\n"
          L"Focus transitions: %llu\nThis is NOT evidence of game input or its failure cause.",
          static_cast<unsigned long long>(state->census.down_count),
          static_cast<unsigned long long>(state->census.up_count),
          static_cast<unsigned long long>(state->census.polls),
          static_cast<unsigned long long>(state->census.sampled_down),
          static_cast<unsigned long long>(state->census.focus_transitions));
      }
      PAINTSTRUCT paint{};
      HDC dc = BeginPaint(window, &paint);
      RECT rect{};
      GetClientRect(window, &rect);
      FillRect(dc, &rect, reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1));
      InflateRect(&rect, -20, -20);
      DrawTextW(dc, text, -1, &rect, DT_LEFT | DT_TOP | DT_WORDBREAK);
      EndPaint(window, &paint);
      return 0;
    }
    case kWorkerClose:
      state->close_reason = 2;
      SendMessageW(window, WM_CLOSE, 0, 0);
      return 0;
    case WM_CLOSE:
      if (!state->close_reason) state->close_reason = 1;
      SetEvent(state->stop);
      DestroyWindow(window);
      return 0;
    case WM_DESTROY:
      KillTimer(window, kUiTimer);
      SetEvent(state->stop);
      PostQuitMessage(0);
      return 0;
    default: return DefWindowProcW(window, message, wparam, lparam);
  }
}
void SampleWorker(State& state) {
  for (;;) {
    const auto wait = WaitForSingleObject(state.stop, kPollMs);
    if (wait == WAIT_OBJECT_0) return;
    if (wait != WAIT_TIMEOUT) {
      SetError(state, GetLastError());
      PostMessageW(state.window, kWorkerClose, 0, 0);
      return;
    }
    if (GetTickCount64() - state.start_tick >= kDurationMs) {
      if (!PostMessageW(state.window, kWorkerClose, 0, 0)) SetError(state, GetLastError());
      return;
    }
    Sample sample{};
    sample.before = Now();
    sample.foreground = GetForegroundWindow() == state.window;
    if (sample.foreground) {
      const bool down = (GetAsyncKeyState(VK_RETURN) & 0x8000) != 0;
      // Discard the value if focus changed during the query. Never store other keys.
      sample.async_queried = GetForegroundWindow() == state.window;
      sample.async_down = sample.async_queried && down;
    }
    std::lock_guard<std::mutex> lock(state.mutex);
    sample.event_down = state.census.event_down;
    sample.after = Now();
    state.census.AddSample(sample);
  }
}
int SelfTest() {
  Census c;
  c.AddEvent({10, Kind::FocusGain, 0, true});
  c.AddEvent({20, Kind::Down, 1, true});
  if (!c.event_down) return 1;
  c.AddEvent({21, Kind::Up, 1, true});
  if (c.event_down) return 1;
  c.AddSample({30, 31, true, false, true, false});
  c.AddEvent({35, Kind::Down, 1, true});
  c.AddSample({40, 41, true, c.event_down, true, true});
  c.AddEvent({60, Kind::FocusLoss, 0, false});
  c.AddSample({65, 66, false, c.event_down, false, false});
  if (c.event_down || c.down_count != 2 || c.up_count != 1 || c.focus_transitions != 2 ||
      c.polls != 3 || c.queried_polls != 2 || c.sampled_down != 1 || c.events[2].qpc != 21)
    return 1;
  auto json = Serialize(c, 1000, 1, 67, 0, 2);
  if (json.find("\"async_down\":null") == std::string::npos ||
      json.find("\"game_input_proven\":false") == std::string::npos ||
      json.find("\"completed\":true") == std::string::npos ||
      json.find("\"qpc\":20,\"kind\":1") == std::string::npos) return 2;
  for (size_t i = 0; i < kMaxEvents; ++i) c.AddEvent({70, Kind::Down, 1, true});
  for (size_t i = 0; i < kMaxSamples; ++i) c.AddSample({80, 81, false, false, false, false});
  if (c.event_count != kMaxEvents || c.event_overflow != 5 ||
      c.sample_count != kMaxSamples || c.sample_overflow != 3) return 3;
  json = Serialize(c, 1000, 1, 90, ERROR_GEN_FAILURE, 3);
  if (json.find("\"completed\":false") == std::string::npos) return 4;
  std::puts("SELF_TEST_PASS: event sequence, sample counters, bounded overflow, serialization flags");
  return 0;
}
int Run(const wchar_t* path) {
  Output output;
  DWORD error = 0;
  if (!output.Open(path, error)) {
    std::fprintf(stderr, "Output admission failed (Win32 %lu); no window opened.\n", error);
    return 2;
  }
  State state;
  LARGE_INTEGER frequency{};
  if (!QueryPerformanceFrequency(&frequency) || frequency.QuadPart <= 0) state.error = ERROR_NOT_SUPPORTED;
  state.frequency = frequency.QuadPart;
  state.start = Now();
  if (!state.start) state.error = ERROR_NOT_SUPPORTED;
  state.stop = CreateEventW(nullptr, TRUE, FALSE, nullptr);
  if (!state.stop) state.error = GetLastError();
  HINSTANCE instance = GetModuleHandleW(nullptr);
  WNDCLASSW cls{};
  cls.lpfnWndProc = WindowProc;
  cls.hInstance = instance;
  cls.hCursor = LoadCursorW(nullptr, IDC_ARROW);
  cls.lpszClassName = L"MclaInputPulseFixtureStandaloneV1";
  const ATOM atom = state.error ? 0 : RegisterClassW(&cls);
  if (!state.error && !atom) state.error = GetLastError();
  if (!state.error) {
    state.window = CreateWindowExW(0, cls.lpszClassName, L"MCLA input pulse fixture",
      WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX, CW_USEDEFAULT, CW_USEDEFAULT,
      720, 280, nullptr, nullptr, instance, &state);
    if (!state.window) state.error = GetLastError();
  }
  std::thread worker;
  if (!state.error) {
    state.start_tick = GetTickCount64();
    if (!SetTimer(state.window, kUiTimer, 200, nullptr)) state.error = GetLastError();
    if (!state.error) {
      try { worker = std::thread(SampleWorker, std::ref(state)); }
      catch (...) { state.error = ERROR_NOT_ENOUGH_MEMORY; }
    }
  }
  if (!state.error) {
    ShowWindow(state.window, SW_SHOWNORMAL); // Only our own window; no foreground forcing.
    MSG message{};
    int result = 0;
    while ((result = static_cast<int>(GetMessageW(&message, nullptr, 0, 0))) > 0) {
      TranslateMessage(&message);
      DispatchMessageW(&message);
    }
    if (result == -1) SetError(state, GetLastError());
  }
  if (state.stop) SetEvent(state.stop);
  if (worker.joinable()) worker.join();
  if (state.window && IsWindow(state.window)) DestroyWindow(state.window);
  if (atom) UnregisterClassW(cls.lpszClassName, instance);
  if (state.stop) CloseHandle(state.stop);
  state.end = Now();
  if (state.error) state.close_reason = 3;
  if (!output.Write(Serialize(state.census, state.frequency, state.start, state.end,
                              state.error.load(), state.close_reason))) {
    std::fprintf(stderr, "Result write failed (Win32 %lu); retain incomplete file.\n", GetLastError());
    return 3;
  }
  std::printf("SYNTHETIC_CENSUS_ONLY win32_error=%lu; no game conclusion.\n", state.error.load());
  return state.error ? 1 : 0;
}
} // namespace

int wmain(int argc, wchar_t** argv) {
  try {
    if (argc == 2 && std::wcscmp(argv[1], L"--self-test") == 0) return SelfTest();
    if (argc != 2) {
      std::fputs("Usage: fixture --self-test OR fixture C:\\existing-parent\\new-result.json\n", stderr);
      return 2;
    }
    return Run(argv[1]);
  } catch (...) {
    std::fputs("Unhandled fixture error; preserve any incomplete output.\n", stderr);
    return 4;
  }
}
