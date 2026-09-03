// mcla - Windows out-of-process crash capture client

#include "mcla_native_crash.h"

#include "mcla_crash_ipc.h"

#include <Windows.h>

#include <algorithm>
#include <atomic>
#include <csignal>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <exception>
#include <filesystem>
#include <mutex>
#include <string>
#include <string_view>
#include <vector>

#include <intrin.h>

namespace mcla::diagnostics::native {
namespace {

struct ClientState {
  HANDLE mapping = nullptr;
  CrashIpcState *shared = nullptr;
  HANDLE live_request = nullptr;
  HANDLE live_done = nullptr;
  HANDLE crash_request = nullptr;
  HANDLE crash_done = nullptr;
  HANDLE stop = nullptr;
  HANDLE helper_process = nullptr;
  LPTOP_LEVEL_EXCEPTION_FILTER previous_filter = nullptr;
  std::terminate_handler previous_terminate = nullptr;
  void (*previous_abort)(int) = SIG_DFL;
  std::mutex live_mutex;
  std::atomic<bool> started{false};
};

ClientState state;

std::wstring HandleArgument(std::wstring_view name, HANDLE handle) {
  return std::wstring(name) + L"=" +
         std::to_wstring(reinterpret_cast<uintptr_t>(handle));
}

bool CopyPath(wchar_t *destination, size_t capacity,
              const std::filesystem::path &source) {
  const std::wstring value = source.wstring();
  if (value.size() + 1 > capacity) {
    return false;
  }
  return wcscpy_s(destination, capacity, value.c_str()) == 0;
}

bool CopyText(wchar_t *destination, size_t capacity, std::string_view source) {
  if (source.size() + 1 > capacity) {
    return false;
  }
  size_t converted = 0;
  return mbstowcs_s(&converted, destination, capacity, source.data(),
                    source.size()) == 0;
}

void CloseHandleIfSet(HANDLE &handle) {
  if (handle) {
    CloseHandle(handle);
    handle = nullptr;
  }
}

void CleanupStartupFailure() {
  CloseHandleIfSet(state.helper_process);
  if (state.shared) {
    UnmapViewOfFile(state.shared);
    state.shared = nullptr;
  }
  CloseHandleIfSet(state.mapping);
  CloseHandleIfSet(state.live_request);
  CloseHandleIfSet(state.live_done);
  CloseHandleIfSet(state.crash_request);
  CloseHandleIfSet(state.crash_done);
  CloseHandleIfSet(state.stop);
}

void RequestCrash(EXCEPTION_POINTERS *exception) {
  if (!state.started.load(std::memory_order_acquire) || !state.shared) {
    return;
  }
  const bool first =
      InterlockedCompareExchange(&state.shared->crash_started, 1, 0) == 0;
  if (first) {
    state.shared->crash_thread_id = GetCurrentThreadId();
    if (exception && exception->ExceptionRecord) {
      state.shared->exception_code = exception->ExceptionRecord->ExceptionCode;
      state.shared->exception_address = reinterpret_cast<uint64_t>(
          exception->ExceptionRecord->ExceptionAddress);
      state.shared->exception_pointers = reinterpret_cast<uint64_t>(exception);
    }
    MemoryBarrier();
    SetEvent(state.crash_request);
  }
  // This manual-reset event is signaled as soon as MiniDumpWriteDump has
  // finished. Every simultaneously crashing thread waits for the same dump;
  // none may terminate the process while the first capture still reads it.
  WaitForSingleObject(state.crash_done, 30000);
}

LONG WINAPI LastChanceExceptionFilter(EXCEPTION_POINTERS *exception) {
  RequestCrash(exception);
  return EXCEPTION_EXECUTE_HANDLER;
}

[[noreturn]] void TerminateBridge() {
  CONTEXT context{};
  RtlCaptureContext(&context);
  EXCEPTION_RECORD record{};
  record.ExceptionCode = 0xE0434D43;
  record.ExceptionFlags = EXCEPTION_NONCONTINUABLE;
  record.ExceptionAddress = _ReturnAddress();
  EXCEPTION_POINTERS pointers{&record, &context};
  RequestCrash(&pointers);
  TerminateProcess(GetCurrentProcess(), record.ExceptionCode);
  std::abort();
}

void AbortBridge(int) { TerminateBridge(); }

} // namespace

bool Start(const std::filesystem::path &diagnostics_root,
           const std::filesystem::path &user_data_root,
           const std::filesystem::path &journal_path, std::string_view version,
           bool show_reporter_dialog, std::string &error) {
  if (state.started.load(std::memory_order_acquire)) {
    return true;
  }
  wchar_t module_path[32768]{};
  const DWORD module_length =
      GetModuleFileNameW(nullptr, module_path, std::size(module_path));
  if (!module_length || module_length >= std::size(module_path)) {
    error = "cannot resolve executable path";
    return false;
  }
  const auto helper_path = std::filesystem::path(module_path).parent_path() /
                           "mcla_crash_handler.exe";
  std::error_code ec;
  if (!std::filesystem::is_regular_file(helper_path, ec)) {
    error = "mcla_crash_handler.exe is missing";
    return false;
  }

  SECURITY_ATTRIBUTES inherit{};
  inherit.nLength = sizeof(inherit);
  inherit.bInheritHandle = TRUE;
  state.mapping =
      CreateFileMappingW(INVALID_HANDLE_VALUE, &inherit, PAGE_READWRITE, 0,
                         sizeof(CrashIpcState), nullptr);
  state.live_request = CreateEventW(&inherit, FALSE, FALSE, nullptr);
  state.live_done = CreateEventW(&inherit, FALSE, FALSE, nullptr);
  state.crash_request = CreateEventW(&inherit, FALSE, FALSE, nullptr);
  state.crash_done = CreateEventW(&inherit, TRUE, FALSE, nullptr);
  state.stop = CreateEventW(&inherit, TRUE, FALSE, nullptr);
  HANDLE parent_process =
      OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ |
                      PROCESS_DUP_HANDLE | SYNCHRONIZE,
                  TRUE, GetCurrentProcessId());
  if (!state.mapping || !state.live_request || !state.live_done ||
      !state.crash_request || !state.crash_done || !state.stop ||
      !parent_process) {
    error = "cannot create crash-helper IPC handles (win32=" +
            std::to_string(GetLastError()) + ")";
    CloseHandleIfSet(parent_process);
    CleanupStartupFailure();
    return false;
  }
  state.shared = static_cast<CrashIpcState *>(MapViewOfFile(
      state.mapping, FILE_MAP_ALL_ACCESS, 0, 0, sizeof(CrashIpcState)));
  if (!state.shared) {
    error = "cannot map crash-helper IPC state";
    CloseHandleIfSet(parent_process);
    CleanupStartupFailure();
    return false;
  }
  *state.shared = {};
  state.shared->magic = kCrashIpcMagic;
  state.shared->version = kCrashIpcVersion;
  state.shared->parent_process_id = GetCurrentProcessId();
  state.shared->show_reporter_dialog = show_reporter_dialog ? 1 : 0;
  if (!CopyPath(state.shared->diagnostics_root, kCrashIpcPathCapacity,
                diagnostics_root) ||
      !CopyPath(state.shared->user_data_root, kCrashIpcPathCapacity,
                user_data_root) ||
      !CopyPath(state.shared->journal_path, kCrashIpcPathCapacity,
                journal_path) ||
      !CopyText(state.shared->app_version, kCrashIpcVersionCapacity, version)) {
    error = "diagnostic path or version exceeds the crash protocol limit";
    CloseHandleIfSet(parent_process);
    CleanupStartupFailure();
    return false;
  }

  std::wstring command =
      L"\"" + helper_path.wstring() + L"\" " +
      HandleArgument(L"--parent", parent_process) + L" " +
      HandleArgument(L"--mapping", state.mapping) + L" " +
      HandleArgument(L"--live-request", state.live_request) + L" " +
      HandleArgument(L"--live-done", state.live_done) + L" " +
      HandleArgument(L"--crash-request", state.crash_request) + L" " +
      HandleArgument(L"--crash-done", state.crash_done) + L" " +
      HandleArgument(L"--stop", state.stop);
  SIZE_T attribute_bytes = 0;
  InitializeProcThreadAttributeList(nullptr, 1, 0, &attribute_bytes);
  if (!attribute_bytes) {
    error = "cannot size crash-helper handle whitelist";
    CloseHandleIfSet(parent_process);
    CleanupStartupFailure();
    return false;
  }
  std::vector<std::byte> attribute_storage(attribute_bytes);
  auto *attribute_list =
      reinterpret_cast<LPPROC_THREAD_ATTRIBUTE_LIST>(attribute_storage.data());
  if (!InitializeProcThreadAttributeList(attribute_list, 1, 0,
                                         &attribute_bytes)) {
    error = "cannot initialize crash-helper handle whitelist";
    CloseHandleIfSet(parent_process);
    CleanupStartupFailure();
    return false;
  }
  HANDLE inherited_handles[] = {parent_process,      state.mapping,
                                state.live_request,  state.live_done,
                                state.crash_request, state.crash_done,
                                state.stop};
  if (!UpdateProcThreadAttribute(
          attribute_list, 0, PROC_THREAD_ATTRIBUTE_HANDLE_LIST,
          inherited_handles, sizeof(inherited_handles), nullptr, nullptr)) {
    error = "cannot set crash-helper handle whitelist";
    DeleteProcThreadAttributeList(attribute_list);
    CloseHandleIfSet(parent_process);
    CleanupStartupFailure();
    return false;
  }
  STARTUPINFOEXW startup{};
  startup.StartupInfo.cb = sizeof(startup);
  startup.lpAttributeList = attribute_list;
  PROCESS_INFORMATION process{};
  if (!CreateProcessW(helper_path.c_str(), command.data(), nullptr, nullptr,
                      TRUE, CREATE_NO_WINDOW | EXTENDED_STARTUPINFO_PRESENT,
                      nullptr, helper_path.parent_path().c_str(),
                      &startup.StartupInfo, &process)) {
    error =
        "cannot start crash helper (win32=" + std::to_string(GetLastError()) +
        ")";
    DeleteProcThreadAttributeList(attribute_list);
    CloseHandleIfSet(parent_process);
    CleanupStartupFailure();
    return false;
  }
  DeleteProcThreadAttributeList(attribute_list);
  CloseHandleIfSet(parent_process);
  CloseHandle(process.hThread);
  state.helper_process = process.hProcess;
  bool inheritance_cleared = true;
  for (HANDLE handle : {state.mapping, state.live_request, state.live_done,
                        state.crash_request, state.crash_done, state.stop}) {
    inheritance_cleared =
        SetHandleInformation(handle, HANDLE_FLAG_INHERIT, 0) != FALSE &&
        inheritance_cleared;
  }
  if (!inheritance_cleared) {
    error = "cannot clear crash-helper handle inheritance";
    SetEvent(state.stop);
    WaitForSingleObject(state.helper_process, 3000);
    CleanupStartupFailure();
    return false;
  }
  state.previous_filter =
      SetUnhandledExceptionFilter(LastChanceExceptionFilter);
  state.previous_terminate = std::set_terminate(TerminateBridge);
  state.previous_abort = std::signal(SIGABRT, AbortBridge);
  state.started.store(true, std::memory_order_release);
  return true;
}

bool WriteLiveMiniDump(const std::filesystem::path &path,
                       std::chrono::milliseconds timeout, std::string &error) {
  if (!state.started.load(std::memory_order_acquire) || !state.shared) {
    error = "native crash helper is unavailable";
    return false;
  }
  std::lock_guard lock(state.live_mutex);
  if (!CopyPath(state.shared->live_dump_path, kCrashIpcPathCapacity, path)) {
    error = "minidump path exceeds the crash protocol limit";
    return false;
  }
  InterlockedExchange(&state.shared->live_result, 0);
  const LONG requested_sequence =
      InterlockedIncrement(&state.shared->live_sequence);
  MemoryBarrier();
  if (!SetEvent(state.live_request)) {
    error = "cannot signal crash helper";
    return false;
  }
  HANDLE waits[] = {state.live_done, state.helper_process};
  const auto deadline = std::chrono::steady_clock::now() + timeout;
  for (;;) {
    const auto remaining =
        std::chrono::duration_cast<std::chrono::milliseconds>(
            deadline - std::chrono::steady_clock::now());
    if (remaining.count() <= 0) {
      error = "crash helper timed out";
      return false;
    }
    const DWORD wait = WaitForMultipleObjects(
        std::size(waits), waits, FALSE,
        static_cast<DWORD>(std::clamp<int64_t>(remaining.count(), 1, 60000)));
    if (wait == WAIT_OBJECT_0 + 1) {
      error = "crash helper exited";
      return false;
    }
    if (wait == WAIT_TIMEOUT) {
      error = "crash helper timed out";
      return false;
    }
    if (wait != WAIT_OBJECT_0) {
      error = "crash helper wait failed";
      return false;
    }
    const LONG completed_sequence = InterlockedCompareExchange(
        &state.shared->live_completed_sequence, 0, 0);
    if (completed_sequence != requested_sequence) {
      continue;
    }
    if (InterlockedCompareExchange(&state.shared->live_result, 0, 0) != 1) {
      error = "MiniDumpWriteDump failed in crash helper";
      return false;
    }
    return true;
  }
}

void RefreshCrashHandlers() {
  if (state.started.load(std::memory_order_acquire)) {
    SetUnhandledExceptionFilter(LastChanceExceptionFilter);
  }
}

void Stop() {
  const bool was_started =
      state.started.exchange(false, std::memory_order_acq_rel);
  if (was_started) {
    const auto active_filter =
        SetUnhandledExceptionFilter(state.previous_filter);
    if (active_filter != LastChanceExceptionFilter) {
      SetUnhandledExceptionFilter(active_filter);
    }
    if (std::get_terminate() == TerminateBridge) {
      std::set_terminate(state.previous_terminate);
    }
    if (state.previous_abort != SIG_ERR) {
      const auto active_abort = std::signal(SIGABRT, state.previous_abort);
      if (active_abort != AbortBridge && active_abort != SIG_ERR) {
        std::signal(SIGABRT, active_abort);
      }
    }
  }
  if (state.stop) {
    SetEvent(state.stop);
  }
  if (state.helper_process) {
    WaitForSingleObject(state.helper_process, 3000);
  }
  // Crash callbacks may already have passed their atomic started check. Keep
  // their mapping and event handles valid until process teardown instead of
  // racing reclamation on the shutdown path. The OS closes this bounded set.
}

[[noreturn]] void TriggerCrashProbe() {
  RaiseException(0xE0434D44, EXCEPTION_NONCONTINUABLE, 0, nullptr);
  TerminateProcess(GetCurrentProcess(), 0xE0434D44);
  std::abort();
}

} // namespace mcla::diagnostics::native
