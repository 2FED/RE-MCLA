#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <dwmapi.h>
#include <bcrypt.h>

#include "mcla_diagnostic_image_client_win.h"
#include <atomic>
#include <array>
#include <cstring>
#include <functional>
#include <string>
#include <vector>

namespace mcla::diagnostics::image {
namespace {
struct Handle {
  HANDLE value = nullptr;
  explicit Handle(HANDLE h = nullptr) : value(h == INVALID_HANDLE_VALUE ? nullptr : h) {}
  Handle(const Handle &) = delete;
  Handle &operator=(const Handle &) = delete;
  ~Handle() { if (value) CloseHandle(value); }
};
struct MappingView {
  void *value = nullptr;
  ~MappingView() { if (value) UnmapViewOfFile(value); }
};
struct DpiScope {
  DPI_AWARENESS_CONTEXT previous =
      SetThreadDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
  ~DpiScope() { if (previous) SetThreadDpiAwarenessContext(previous); }
};
struct Attributes {
  std::vector<std::byte> storage;
  LPPROC_THREAD_ATTRIBUTE_LIST value = nullptr;
  HANDLE inherited_handle = nullptr;
  bool Init(HANDLE inherited) {
    inherited_handle = inherited;
    SIZE_T bytes = 0;
    InitializeProcThreadAttributeList(nullptr, 1, 0, &bytes);
    if (!bytes) return false;
    storage.resize(bytes);
    auto *candidate = reinterpret_cast<LPPROC_THREAD_ATTRIBUTE_LIST>(storage.data());
    if (!InitializeProcThreadAttributeList(candidate, 1, 0, &bytes)) return false;
    value = candidate;
    return UpdateProcThreadAttribute(value, 0, PROC_THREAD_ATTRIBUTE_HANDLE_LIST,
                                    &inherited_handle, sizeof(inherited_handle), nullptr, nullptr) != FALSE;
  }
  ~Attributes() { if (value) DeleteProcThreadAttributeList(value); }
};
bool Owned(HWND hwnd, const Request *expected, Request &request, RECT &bounds) {
  DWORD pid = 0;
  const DWORD thread = GetWindowThreadProcessId(hwnd, &pid);
  if (!thread || pid != GetCurrentProcessId() || !IsWindow(hwnd) ||
      !IsWindowVisible(hwnd) || IsIconic(hwnd) || GetForegroundWindow() != hwnd ||
      GetAncestor(hwnd, GA_ROOT) != hwnd ||
      !AreDpiAwarenessContextsEqual(GetWindowDpiAwarenessContext(hwnd),
                                   DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2)) return false;
  FILETIME born{}, exit{}, kernel{}, user{};
  if (!GetProcessTimes(GetCurrentProcess(), &born, &exit, &kernel, &user)) return false;
  RECT client{};
  POINT origin{};
  DWORD cloaked = 0;
  if (!GetClientRect(hwnd, &client) || client.left != 0 || client.top != 0 ||
      client.right <= 0 || client.bottom <= 0 || !ClientToScreen(hwnd, &origin) ||
      FAILED(DwmGetWindowAttribute(hwnd, DWMWA_EXTENDED_FRAME_BOUNDS, &bounds, sizeof(bounds))) ||
      FAILED(DwmGetWindowAttribute(hwnd, DWMWA_CLOAKED, &cloaked, sizeof(cloaked))) || cloaked) return false;
  request.owner_pid = pid;
  request.owner_thread = thread;
  request.owner_birth = (uint64_t(born.dwHighDateTime) << 32) | born.dwLowDateTime;
  request.hwnd = reinterpret_cast<uintptr_t>(hwnd);
  request.client_x = origin.x;
  request.client_y = origin.y;
  request.client_width = static_cast<uint64_t>(client.right);
  request.client_height = static_cast<uint64_t>(client.bottom);
  request.dpi = GetDpiForWindow(hwnd);
  uint64_t row = 0;
  if (!ImageShape(request.client_width, request.client_height, row, request.payload_capacity)) return false;
  return ValidateRequest(request) && (!expected || request == *expected);
}
bool SafeHelper(const std::filesystem::path &path) {
  // Do not resolve an executable via PATH, a share, or redirected directories.
  const auto root = path.root_name().wstring();
  if (!path.is_absolute() || root.size() != 2 || root[1] != L':' ||
      path.extension() != L".exe") return false;
  for (auto cursor = path; !cursor.empty();) {
    const DWORD flags = GetFileAttributesW(cursor.c_str());
    if (flags == INVALID_FILE_ATTRIBUTES || (flags & FILE_ATTRIBUTE_REPARSE_POINT)) return false;
    const auto parent = cursor.parent_path();
    if (parent == cursor) break;
    cursor = parent;
  }
  return !(GetFileAttributesW(path.c_str()) & FILE_ATTRIBUTE_DIRECTORY);
}
bool VerifyHelper(HANDLE file, std::string_view expected, std::stop_token stop) {
  if (expected.size() != 64 || expected.find_first_not_of("0123456789ABCDEF") != expected.npos) return false;
  LARGE_INTEGER size{};
  if (!GetFileSizeEx(file, &size) || size.QuadPart <= 0 || size.QuadPart > 64 * 1024 * 1024) return false;
  struct Hash {
    BCRYPT_ALG_HANDLE algorithm = nullptr;
    BCRYPT_HASH_HANDLE hash = nullptr;
    ~Hash() { if (hash) BCryptDestroyHash(hash); if (algorithm) BCryptCloseAlgorithmProvider(algorithm, 0); }
  } state;
  if (BCryptOpenAlgorithmProvider(&state.algorithm, BCRYPT_SHA256_ALGORITHM, nullptr, 0) < 0 ||
      BCryptCreateHash(state.algorithm, &state.hash, nullptr, 0, nullptr, 0, 0) < 0) return false;
  std::array<unsigned char, 65536> buffer{};
  uint64_t total = 0;
  for (;;) {
    if (stop.stop_requested()) return false;
    DWORD read = 0;
    if (!ReadFile(file, buffer.data(), static_cast<DWORD>(buffer.size()), &read, nullptr)) return false;
    if (!read) break;
    total += read;
    if (total > static_cast<uint64_t>(size.QuadPart) ||
        BCryptHashData(state.hash, buffer.data(), read, 0) < 0) return false;
  }
  std::array<unsigned char, 32> digest{};
  if (total != static_cast<uint64_t>(size.QuadPart) ||
      BCryptFinishHash(state.hash, digest.data(), static_cast<ULONG>(digest.size()), 0) < 0) return false;
  constexpr char hex[] = "0123456789ABCDEF";
  for (size_t index = 0; index < digest.size(); ++index) {
    if (expected[index * 2] != hex[digest[index] >> 4] ||
        expected[index * 2 + 1] != hex[digest[index] & 15]) return false;
  }
  return true;
}
bool EmptyJob(HANDLE job) {
  JOBOBJECT_BASIC_ACCOUNTING_INFORMATION info{};
  return QueryInformationJobObject(job, JobObjectBasicAccountingInformation,
                                  &info, sizeof(info), nullptr) && info.ActiveProcesses == 0;
}
bool DrainJob(HANDLE job) {
  // Process-handle signaling can precede Job accounting updates. Still require
  // an observed empty Job, with a separate bounded grace period, never infer it.
  const auto begin = GetTickCount64();
  do {
    if (EmptyJob(job)) return true;
    Sleep(2);
  } while (GetTickCount64() - begin < 500);
  return EmptyJob(job);
}
bool SameBounds(const RECT &a, const RECT &b) {
  return a.left == b.left && a.top == b.top && a.right == b.right && a.bottom == b.bottom;
}
CaptureResult RunHelper(const Request &request, const std::filesystem::path &helper,
                        std::string_view expected_sha256, std::stop_token stop,
                        const std::function<bool(const Result &)> &admit) {
  CaptureResult output;
  // No unbounded wait or asynchronous GPU object lives in this process.
  try {
    if (stop.stop_requested()) { output.failure = ClientFailure::kCancelled; return output; }
    if (!ValidateRequest(request)) {
      output.failure = ClientFailure::kInvalidOwner; return output;
    }
    if (!SafeHelper(helper)) { output.failure = ClientFailure::kInvalidHelper; return output; }
    // Hold the exact executable against ordinary replacement through capture.
    Handle executable(CreateFileW(helper.c_str(), GENERIC_READ, FILE_SHARE_READ,
                                  nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr));
    if (!executable.value || !VerifyHelper(executable.value, expected_sha256, stop)) {
      output.failure = stop.stop_requested() ? ClientFailure::kCancelled : ClientFailure::kInvalidHelper;
      return output;
    }
    const uint64_t mapping_bytes = sizeof(Result) + request.payload_capacity;
    SECURITY_ATTRIBUTES security{sizeof(security), nullptr, TRUE};
    Handle mapping(CreateFileMappingW(INVALID_HANDLE_VALUE, &security, PAGE_READWRITE,
                                      0, static_cast<DWORD>(mapping_bytes), nullptr));
    if (!mapping.value) return output;
    MappingView view{MapViewOfFile(mapping.value, FILE_MAP_READ | FILE_MAP_WRITE,
                                  0, 0, static_cast<SIZE_T>(mapping_bytes))};
    if (!view.value) return output;
    Result pending;
    pending.request = request;
    std::memcpy(view.value, &pending, sizeof(pending)); // New mapping payload is zero.
    Handle job(CreateJobObjectW(nullptr, nullptr));
    if (!job.value) return output;
    JOBOBJECT_EXTENDED_LIMIT_INFORMATION limits{};
    limits.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE |
                                             JOB_OBJECT_LIMIT_ACTIVE_PROCESS;
    limits.BasicLimitInformation.ActiveProcessLimit = 1;
    if (!SetInformationJobObject(job.value, JobObjectExtendedLimitInformation,
                                 &limits, sizeof(limits))) return output;
    Attributes attributes;
    if (!attributes.Init(mapping.value)) return output;
    STARTUPINFOEXW startup{};
    startup.StartupInfo.cb = sizeof(startup);
    startup.lpAttributeList = attributes.value;
    PROCESS_INFORMATION created{};
    std::wstring command = L"\"" + helper.wstring() + L"\" " +
        std::to_wstring(reinterpret_cast<uintptr_t>(mapping.value)) + L" " +
        std::to_wstring(mapping_bytes);
    if (!CreateProcessW(helper.c_str(), command.data(), nullptr, nullptr, TRUE,
                         CREATE_NO_WINDOW | CREATE_SUSPENDED | EXTENDED_STARTUPINFO_PRESENT,
                         nullptr, helper.parent_path().c_str(), &startup.StartupInfo, &created)) return output;
    Handle process(created.hProcess);
    Handle thread(created.hThread);
    output.cleanup_complete = false;
    if (!SetHandleInformation(mapping.value, HANDLE_FLAG_INHERIT, 0) ||
        !AssignProcessToJobObject(job.value, process.value)) {
      TerminateProcess(process.value, ERROR_CANCELLED);
      output.cleanup_complete = WaitForSingleObject(process.value, 500) == WAIT_OBJECT_0;
      return output;
    }
    const auto begin = GetTickCount64();
    if (ResumeThread(thread.value) == DWORD(-1)) {
      TerminateJobObject(job.value, ERROR_CANCELLED);
      output.cleanup_complete = WaitForSingleObject(process.value, 500) == WAIT_OBJECT_0 && EmptyJob(job.value);
      return output;
    }
    DWORD waited = WAIT_TIMEOUT;
    while (!stop.stop_requested() && GetTickCount64() - begin < 4000) {
      waited = WaitForSingleObject(process.value, 20);
      if (waited != WAIT_TIMEOUT) break;
    }
    if (waited != WAIT_OBJECT_0 || stop.stop_requested() || GetTickCount64() - begin >= 4000) {
      output.failure = stop.stop_requested() ? ClientFailure::kCancelled : ClientFailure::kTimeout;
      TerminateJobObject(job.value, ERROR_TIMEOUT);
      output.cleanup_complete = WaitForSingleObject(process.value, 500) == WAIT_OBJECT_0 && EmptyJob(job.value);
      return output; // Never inspect or save rejected shared pixels.
    }
    output.cleanup_complete = DrainJob(job.value);
    if (!output.cleanup_complete) { output.failure = ClientFailure::kCleanup; return output; }
    DWORD code = 0;
    if (!GetExitCodeProcess(process.value, &code)) return output;
    output.helper_exit = code;
    if (code != 0) {
      output.failure = ClientFailure::kHelperExit;
      // After observed exit/drain, retain only a structurally valid failure
      // header. Never copy rejected pixels or a success-looking error result.
      Result failed;
      std::memcpy(&failed, view.value, sizeof(failed));
      if (failed.status == Status::kFailure &&
          ValidateResult(request, failed, request.payload_capacity)) output.metadata = failed;
      return output;
    }
    Result result;
    std::memcpy(&result, view.value, sizeof(result));
    if (!ValidateResult(request, result, request.payload_capacity) || result.status != Status::kSuccess) {
      output.failure = ClientFailure::kProtocol; return output;
    }
    if (!admit(result)) { output.failure = ClientFailure::kInvalidOwner; return output; }
    if (stop.stop_requested()) { output.failure = ClientFailure::kCancelled; return output; }
    output.pixels.resize(static_cast<size_t>(result.payload_bytes));
    std::memcpy(output.pixels.data(), static_cast<std::byte *>(view.value) + sizeof(Result), output.pixels.size());
    output.metadata = result;
    output.captured = true;
    output.failure = ClientFailure::kNone;
    return output;
  } catch (...) {
    output.pixels.clear();
    output.captured = false;
    output.failure = ClientFailure::kSetup;
    return output;
  }
}
} // namespace

CaptureResult CaptureOwnedWindow(uint64_t hwnd_value, const std::filesystem::path &helper,
                                 std::string_view expected_sha256, std::stop_token stop) {
  CaptureResult rejected;
  if (stop.stop_requested()) { rejected.failure = ClientFailure::kCancelled; return rejected; }
  DpiScope dpi;
  static std::atomic<uint64_t> sequence{1};
  Request request;
  request.request_id = sequence.fetch_add(1, std::memory_order_relaxed);
  RECT before{};
  const HWND hwnd = reinterpret_cast<HWND>(static_cast<uintptr_t>(hwnd_value));
  if (!dpi.previous || !Owned(hwnd, nullptr, request, before)) {
    rejected.failure = ClientFailure::kInvalidOwner; return rejected;
  }
  return RunHelper(request, helper, expected_sha256, stop, [&](const Result &result) {
    Request after = request;
    RECT bounds{};
    return Owned(hwnd, &request, after, bounds) && SameBounds(before, bounds) &&
        MatchesSurface(result, bounds.left, bounds.top,
                       int64_t(bounds.right) - bounds.left, int64_t(bounds.bottom) - bounds.top);
  });
}

#if defined(MCLA_IMAGE_CLIENT_TEST_SEAM)
// Nonshipping supervision tests use a fake process, no HWND or WGC. Not exposed
// in the production client library; production always supplies live owner checks.
CaptureResult RunImageHelperForTest(const Request &request, const std::filesystem::path &helper,
                                    std::string_view expected_sha256, std::stop_token stop) {
  return RunHelper(request, helper, expected_sha256, stop, [&](const Result &result) {
    return MatchesSurface(result, request.client_x, request.client_y,
                          request.client_width, request.client_height);
  });
}
#endif

} // namespace mcla::diagnostics::image
