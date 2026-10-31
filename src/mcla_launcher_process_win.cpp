#include "mcla_launcher_process_win.h"
#include "mcla_launcher_profile_policy.h"
#include "mcla_host_platform_win.h"

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>

#include <algorithm>
#include <array>
#include <climits>
#include <limits>

namespace mcla::launcher {
namespace {

constexpr uint32_t kOwnedTerminationCode = 0x4d434c41;
constexpr std::array<std::wstring_view, 8> kInheritedDirectories{
    L"LOCALAPPDATA", L"APPDATA", L"USERPROFILE", L"TEMP", L"TMP",
    L"ProgramData", L"HOMEDRIVE", L"HOMEPATH"};
// KI-050 (M9-006): Steam sets these for the launched game; Wine imports them
// into the launcher's environment. Without SDL_GAMECONTROLLER_ALLOW_STEAM_
// VIRTUAL_GAMEPAD SDL ignores the Steam Virtual Gamepad (28DE:11FF), the only
// controller Steam Input exposes. Passed only under Wine; never on Windows.
constexpr std::array<std::wstring_view, 10> kProtonPassThrough{
    L"SDL_GAMECONTROLLER_ALLOW_STEAM_VIRTUAL_GAMEPAD", L"SDL_GAMECONTROLLERCONFIG",
    L"SDL_GAMECONTROLLER_IGNORE_DEVICES", L"SDL_GAMECONTROLLER_IGNORE_DEVICES_EXCEPT",
    L"SteamVirtualGamepadInfo", L"SteamAppId", L"SteamGameId", L"SteamDeck",
    L"STEAM_COMPAT_DATA_PATH", L"WINEPREFIX"};

bool EqualName(std::wstring_view a, std::wstring_view b) {
  return a.size() == b.size() && a.size() <= INT_MAX &&
      CompareStringOrdinal(a.data(), static_cast<int>(a.size()), b.data(),
                           static_cast<int>(b.size()), TRUE) == CSTR_EQUAL;
}

bool HasNul(std::wstring_view value) {
  return value.find(L'\0') != std::wstring_view::npos;
}

std::wstring WinError(std::wstring_view action, DWORD error = GetLastError()) {
  return std::wstring(action) + L" (Win32 " + std::to_wstring(error) + L")";
}

std::expected<std::wstring, std::wstring> SystemDirectory(bool windows) {
  std::array<wchar_t, 32768> buffer{};
  const UINT count = windows ? GetWindowsDirectoryW(buffer.data(), static_cast<UINT>(buffer.size()))
                             : GetSystemDirectoryW(buffer.data(), static_cast<UINT>(buffer.size()));
  if (count == 0 || count >= buffer.size())
    return std::unexpected(WinError(L"Cannot resolve Windows system directories"));
  return std::wstring(buffer.data(), count);
}

} // namespace

std::wstring QuoteWindowsArgument(std::wstring_view argument) {
  std::wstring result = L"\"";
  size_t backslashes = 0;
  for (const wchar_t c : argument) {
    if (c == L'\\') {
      ++backslashes;
    } else {
      result.append(c == L'"' ? backslashes * 2 + 1 : backslashes, L'\\');
      result += c;
      backslashes = 0;
    }
  }
  result.append(backslashes * 2, L'\\');
  result += L'"';
  return result;
}

std::span<const std::wstring_view> ProtonPassThroughNames() { return kProtonPassThrough; }

std::expected<std::vector<wchar_t>, std::wstring> MakeOwnedEnvironment(
    std::span<const EnvironmentEntry> source, std::wstring_view windows_directory,
    std::wstring_view system_directory, bool under_wine, size_t *passed_through) {
  if (passed_through) *passed_through = 0;
  if (windows_directory.empty() || system_directory.empty() ||
      HasNul(windows_directory) || HasNul(system_directory) ||
      windows_directory.find(L';') != std::wstring_view::npos ||
      system_directory.find(L';') != std::wstring_view::npos ||
      !NormalizeRoot(std::filesystem::path(windows_directory).u16string()) ||
      !NormalizeRoot(std::filesystem::path(system_directory).u16string()))
    return std::unexpected(L"Invalid trusted Windows environment directories");
  std::vector<EnvironmentEntry> values;
  size_t passed = 0;
  const auto keep = [&](std::wstring_view allowed) -> bool {
    const EnvironmentEntry *chosen = nullptr;
    for (const auto &entry : source) {
      if (!EqualName(entry.first, allowed)) continue;
      if (chosen || HasNul(entry.second)) return false;
      chosen = &entry;
    }
    if (chosen && !chosen->second.empty()) values.emplace_back(allowed, chosen->second);
    return true;
  };
  for (const auto allowed : kInheritedDirectories)
    if (!keep(allowed)) return std::unexpected(L"Invalid or duplicate allowed child environment entry");
  if (under_wine) {
    const size_t before = values.size();
    for (const auto allowed : kProtonPassThrough)
      if (!keep(allowed)) return std::unexpected(L"Invalid or duplicate allowed child environment entry");
    passed = values.size() - before;
  }
  values.emplace_back(L"SystemRoot", windows_directory);
  values.emplace_back(L"WINDIR", windows_directory);
  values.emplace_back(L"PATH", std::wstring(system_directory) + L";" + std::wstring(windows_directory));
  std::sort(values.begin(), values.end(), [](const auto &a, const auto &b) {
    return CompareStringOrdinal(a.first.data(), static_cast<int>(a.first.size()),
        b.first.data(), static_cast<int>(b.first.size()), TRUE) == CSTR_LESS_THAN;
  });
  std::vector<wchar_t> block;
  for (const auto &[name, value] : values) {
    // Bound the complete double-NUL-terminated environment, not each value only.
    if (name.size() + value.size() + 3 > 32767 - block.size())
      return std::unexpected(L"Child environment exceeds the bounded size");
    block.insert(block.end(), name.begin(), name.end());
    block.push_back(L'=');
    block.insert(block.end(), value.begin(), value.end());
    block.push_back(L'\0');
  }
  block.push_back(L'\0');
  if (passed_through) *passed_through = passed;
  return block;
}

std::expected<std::vector<wchar_t>, std::wstring> BuildOwnedEnvironment(size_t *passed_through) {
  const auto windows = SystemDirectory(true);
  const auto system = SystemDirectory(false);
  if (!windows) return std::unexpected(windows.error());
  if (!system) return std::unexpected(system.error());
  const bool under_wine = platform::RunningUnderWine();
  std::vector<std::wstring_view> names(kInheritedDirectories.begin(), kInheritedDirectories.end());
  if (under_wine) names.insert(names.end(), kProtonPassThrough.begin(), kProtonPassThrough.end());
  std::vector<EnvironmentEntry> source;
  for (const auto name : names) {
    SetLastError(ERROR_SUCCESS);
    const DWORD required = GetEnvironmentVariableW(name.data(), nullptr, 0);
    if (!required) {
      if (GetLastError() == ERROR_ENVVAR_NOT_FOUND || GetLastError() == ERROR_SUCCESS) continue;
      return std::unexpected(WinError(L"Cannot read allowed child environment"));
    }
    if (required > 32767) return std::unexpected(L"Allowed environment value exceeds bound");
    std::wstring value(required, L'\0');
    const DWORD copied = GetEnvironmentVariableW(name.data(), value.data(), required);
    if (!copied || copied >= required)
      return std::unexpected(L"Allowed environment value changed while reading");
    value.resize(copied);
    source.emplace_back(name, std::move(value));
  }
  return MakeOwnedEnvironment(source, *windows, *system, under_wine, passed_through);
}

struct OwnedProcess::State {
  HANDLE process = nullptr;
  HANDLE job = nullptr;
  HANDLE executable_pin = INVALID_HANDLE_VALUE;
  DWORD pid = 0;
  size_t proton_pass_through = 0;
  bool assigned = false;

  ~State() {
    if (process) {
      if (assigned && job) {
        // Includes descendants even if the root already exited. No breakaway
        // flag is enabled, so cleanup never targets a process outside this job.
        TerminateJobObject(job, kOwnedTerminationCode);
      } else if (WaitForSingleObject(process, 0) == WAIT_TIMEOUT) {
        TerminateProcess(process, kOwnedTerminationCode);
      }
    }
    if (job) CloseHandle(job); // KILL_ON_JOB_CLOSE is an additional cleanup guard.
    if (process) {
      WaitForSingleObject(process, 5000);
      CloseHandle(process);
    }
    if (executable_pin != INVALID_HANDLE_VALUE) CloseHandle(executable_pin);
  }
};

OwnedProcess::OwnedProcess() noexcept = default;
OwnedProcess::~OwnedProcess() = default;
OwnedProcess::OwnedProcess(OwnedProcess &&) noexcept = default;
OwnedProcess &OwnedProcess::operator=(OwnedProcess &&) noexcept = default;
uint32_t OwnedProcess::ProcessId() const noexcept { return state_ ? state_->pid : 0; }
size_t OwnedProcess::ProtonPassThroughCount() const noexcept {
  return state_ ? state_->proton_pass_through : 0;
}

std::expected<ProcessStatus, std::wstring> OwnedProcess::Poll() const {
  if (!state_ || !state_->process || !state_->job)
    return std::unexpected(L"No owned process");
  ProcessStatus status;
  const DWORD waited = WaitForSingleObject(state_->process, 0);
  if (waited == WAIT_OBJECT_0) {
    DWORD code = 0;
    if (!GetExitCodeProcess(state_->process, &code))
      return std::unexpected(WinError(L"Cannot read owned process exit"));
    status.exit_code = code; // Actual exit code 259 is not mistaken for STILL_ACTIVE.
  } else if (waited != WAIT_TIMEOUT) {
    return std::unexpected(WinError(L"Cannot poll owned process"));
  }
  JOBOBJECT_BASIC_ACCOUNTING_INFORMATION accounting{};
  if (!QueryInformationJobObject(state_->job, JobObjectBasicAccountingInformation,
                                 &accounting, sizeof(accounting), nullptr))
    return std::unexpected(WinError(L"Cannot query owned job"));
  status.active_job_processes = accounting.ActiveProcesses;
  return status;
}

std::expected<void, std::wstring> OwnedProcess::TerminateAndWait(uint32_t timeout_ms) {
  if (!state_ || !state_->process || !state_->job)
    return std::unexpected(L"No owned process");
  if (timeout_ms > 30000) return std::unexpected(L"Owned cleanup wait exceeds 30-second bound");
  if (!TerminateJobObject(state_->job, kOwnedTerminationCode))
    return std::unexpected(WinError(L"Cannot terminate owned job"));
  const ULONGLONG started = GetTickCount64();
  do {
    const auto status = Poll();
    if (!status) return std::unexpected(status.error());
    if (status->exit_code && status->active_job_processes == 0) return {};
    if (GetTickCount64() - started >= timeout_ms) break;
    Sleep(1);
  } while (true);
  return std::unexpected(L"Owned job cleanup did not finish within the timeout");
}

#if defined(MCLA_LAUNCHER_PROCESS_TEST_SEAM)
std::expected<std::vector<uint32_t>, std::wstring> OwnedProcess::JobProcessIdsForTest() const {
  if (!state_ || !state_->job) return std::unexpected(L"No owned job");
  std::vector<ULONG_PTR> storage(4098);
  auto *list = reinterpret_cast<JOBOBJECT_BASIC_PROCESS_ID_LIST *>(storage.data());
  if (!QueryInformationJobObject(state_->job, JobObjectBasicProcessIdList, list,
                                 static_cast<DWORD>(storage.size() * sizeof(ULONG_PTR)), nullptr))
    return std::unexpected(WinError(L"Cannot enumerate test-owned job"));
  if (list->NumberOfProcessIdsInList > 4096 ||
      list->NumberOfAssignedProcesses != list->NumberOfProcessIdsInList)
    return std::unexpected(L"Test job census exceeds bound or changed");
  std::vector<uint32_t> result;
  for (DWORD index = 0; index < list->NumberOfProcessIdsInList; ++index) {
    if (list->ProcessIdList[index] > UINT32_MAX)
      return std::unexpected(L"Test job PID exceeds Windows PID bound");
    result.push_back(static_cast<uint32_t>(list->ProcessIdList[index]));
  }
  return result;
}
#endif

std::expected<OwnedProcess, std::wstring> StartOwnedProcess(
    const std::filesystem::path &executable, const std::vector<std::wstring> &arguments,
    const std::filesystem::path &working_directory) {
  if (!NormalizeRoot(executable.u16string()) || !NormalizeRoot(working_directory.u16string()))
    return std::unexpected(L"Owned child needs explicit safe absolute local paths");
  const DWORD cwd_attributes = GetFileAttributesW(working_directory.c_str());
  if (cwd_attributes == INVALID_FILE_ATTRIBUTES ||
      !(cwd_attributes & FILE_ATTRIBUTE_DIRECTORY) || (cwd_attributes & FILE_ATTRIBUTE_REPARSE_POINT))
    return std::unexpected(L"Owned child working directory is missing or redirected");

  std::wstring command = QuoteWindowsArgument(executable.native());
  for (const auto &argument : arguments) {
    if (HasNul(argument) || argument.size() > 32766)
      return std::unexpected(L"Owned child argument contains NUL or exceeds bound");
    command += L' ';
    command += QuoteWindowsArgument(argument);
    if (command.size() > 32766)
      return std::unexpected(L"Owned child command exceeds Windows length bound");
  }
  size_t proton_pass_through = 0;
  auto environment = BuildOwnedEnvironment(&proton_pass_through);
  if (!environment) return std::unexpected(environment.error());
  OwnedProcess result;
  result.state_ = std::make_unique<OwnedProcess::State>();
  auto &state = *result.state_;
  state.proton_pass_through = proton_pass_through;
  state.executable_pin = CreateFileW(executable.c_str(), GENERIC_READ, FILE_SHARE_READ,
      nullptr, OPEN_EXISTING, FILE_FLAG_OPEN_REPARSE_POINT, nullptr);
  if (state.executable_pin == INVALID_HANDLE_VALUE)
    return std::unexpected(WinError(L"Cannot pin owned child executable"));
  BY_HANDLE_FILE_INFORMATION executable_info{};
  if (GetFileType(state.executable_pin) != FILE_TYPE_DISK ||
      !GetFileInformationByHandle(state.executable_pin, &executable_info) ||
      (executable_info.dwFileAttributes & (FILE_ATTRIBUTE_DIRECTORY | FILE_ATTRIBUTE_REPARSE_POINT)))
    return std::unexpected(L"Owned child executable is not a regular nonredirected file");

  state.job = CreateJobObjectW(nullptr, nullptr);
  if (!state.job) return std::unexpected(WinError(L"Cannot create owned job"));
  JOBOBJECT_EXTENDED_LIMIT_INFORMATION limits{};
  limits.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
  if (!SetInformationJobObject(state.job, JobObjectExtendedLimitInformation, &limits, sizeof(limits)))
    return std::unexpected(WinError(L"Cannot configure owned job"));
  STARTUPINFOW startup{};
  startup.cb = sizeof(startup);
  PROCESS_INFORMATION created{};
  if (!CreateProcessW(executable.c_str(), command.data(), nullptr, nullptr, FALSE,
      CREATE_SUSPENDED | CREATE_UNICODE_ENVIRONMENT | CREATE_NO_WINDOW,
      environment->data(), working_directory.c_str(), &startup, &created))
    return std::unexpected(WinError(L"Cannot create suspended owned child"));
  state.process = created.hProcess;
  state.pid = created.dwProcessId;
  const bool assigned = AssignProcessToJobObject(state.job, state.process) != FALSE;
  const DWORD assign_error = assigned ? ERROR_SUCCESS : GetLastError();
  if (!assigned) {
    CloseHandle(created.hThread);
    return std::unexpected(WinError(L"Cannot assign owned child to mandatory job", assign_error));
  }
  state.assigned = true;
  const DWORD resumed = ResumeThread(created.hThread);
  const DWORD resume_error = resumed == static_cast<DWORD>(-1) ? GetLastError() : ERROR_SUCCESS;
  CloseHandle(created.hThread);
  if (resumed == static_cast<DWORD>(-1))
    return std::unexpected(WinError(L"Cannot resume owned child", resume_error));
  return result;
}

} // namespace mcla::launcher
