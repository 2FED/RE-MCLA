#include "mcla_launcher_process_win.h"

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>

#include <array>
#include <fstream>
#include <iostream>
#include <iterator>

namespace {
using namespace mcla::launcher;
unsigned checks = 0;
unsigned failures = 0;
void Check(bool result, const char *label) {
  ++checks;
  if (!result) { ++failures; std::cerr << "FAIL " << label << '\n'; }
}

std::vector<std::wstring> DecodeEnvironment(const std::vector<wchar_t> &block) {
  std::vector<std::wstring> result;
  size_t offset = 0;
  while (offset < block.size() && block[offset]) {
    const std::wstring entry(block.data() + offset);
    result.push_back(entry);
    offset += entry.size() + 1;
  }
  return result;
}

std::wstring ParentVariable(const wchar_t *name) {
  const DWORD count = GetEnvironmentVariableW(name, nullptr, 0);
  if (!count) return {};
  std::wstring value(count, L'\0');
  const DWORD copied = GetEnvironmentVariableW(name, value.data(), count);
  if (copied >= count) return {};
  value.resize(copied);
  return value;
}

std::expected<ProcessStatus, std::wstring> Wait(OwnedProcess &process, DWORD timeout = 10000) {
  const ULONGLONG started = GetTickCount64();
  do {
    auto status = process.Poll();
    if (!status || (status->exit_code && status->active_job_processes == 0)) return status;
    if (GetTickCount64() - started >= timeout) return std::unexpected(L"Test child wait timed out");
    Sleep(5);
  } while (true);
}

bool WaitForReady(OwnedProcess &process, const std::filesystem::path &receipt) {
  const auto started = GetTickCount64();
  do {
    const auto status = process.Poll();
    if (!status || status->exit_code) return false;
    std::ifstream input(receipt);
    std::string line;
    if (std::getline(input, line) && line == "OWNED_FAKE_CHILD_READY") return true;
    Sleep(5);
  } while (GetTickCount64() - started < 10000);
  return false;
}

const std::vector<std::wstring> kArguments{
    L"", L"plain", L"two words", L"\u0422\u0435\u0441\u0442 \u0441\u0435\u0439\u0432\u0443",
    L"C:\\trailing\\", L"embedded\"quote", L"before\\\"quote", L"\\\\", L"a\tb"};

int Child(int argc, wchar_t **argv) {
  if (argc < 3) return 91;
  const std::wstring_view mode = argv[1];
  const std::filesystem::path receipt = argv[2];
  if (mode == L"--sleep") {
    { std::ofstream marker(receipt); marker << "OWNED_FAKE_CHILD_READY\n"; }
    Sleep(30000);
    return 0;
  }
  if (mode == L"--exit259") return 259;
  if (mode != L"--receipt" || static_cast<size_t>(argc) != kArguments.size() + 3) return 92;
  for (size_t i = 0; i < kArguments.size(); ++i)
    if (argv[i + 3] != kArguments[i]) return 93;
  const std::array<std::wstring_view, 11> allowed{L"APPDATA", L"HOMEDRIVE", L"HOMEPATH",
      L"LOCALAPPDATA", L"PATH", L"ProgramData", L"SystemRoot", L"TEMP", L"TMP",
      L"USERPROFILE", L"WINDIR"};
  wchar_t *environment = GetEnvironmentStringsW();
  if (!environment) return 94;
  bool safe = true;
  for (const wchar_t *entry = environment; *entry; entry += wcslen(entry) + 1) {
    const std::wstring_view text(entry);
    const auto separator = text.find(L'=');
    const auto name = text.substr(0, separator);
    bool matched = false;
    for (const auto expected : allowed)
      matched |= name == expected;
    safe &= matched;
  }
  FreeEnvironmentStringsW(environment);
  if (!safe) return 95;
  const auto cwd = std::filesystem::current_path();
  if (cwd != receipt.parent_path()) return 96;
  std::ofstream marker(receipt, std::ios::binary);
  marker << "ARGV_ENV_CWD_PASS\n";
  return marker ? 0 : 97;
}

} // namespace

int wmain(int argc, wchar_t **argv) {
  if (argc > 1 && std::wstring_view(argv[1]).starts_with(L"--")) return Child(argc, argv);
  using namespace mcla::launcher;
  Check(QuoteWindowsArgument(L"") == L"\"\"", "quote empty");
  Check(QuoteWindowsArgument(L"a b") == L"\"a b\"", "quote spaces");
  Check(QuoteWindowsArgument(L"x\\") == L"\"x\\\\\"", "double trailing slash");
  Check(QuoteWindowsArgument(L"x\"y") == L"\"x\\\"y\"", "escape quote");

  const std::vector<EnvironmentEntry> source{
      {L"TEMP", L"D:\\Temp"}, {L"appdata", L"D:\\Data"},
      {L"REX_USER_DATA_ROOT", L"SENTINEL_FORBIDDEN_ROOT"},
      {L"REX_MCLA_MANAGED_LAUNCH_CONFIG", L"SENTINEL_FORBIDDEN_CONFIG"},
      {L"LITELLM_API_KEY", L"SENTINEL_NOT_A_REAL_SECRET"},
      {L"OPENAI_API_KEY", L"SENTINEL_NOT_A_REAL_SECRET"},
      {L"PATH", L"SENTINEL_POISONED_PATH"}, {L"SystemRoot", L"SENTINEL_POISONED_WINDOWS"},
      {L"OTHER", L"SENTINEL_UNAPPROVED"}};
  const auto block = MakeOwnedEnvironment(source, L"C:\\Windows", L"C:\\Windows\\System32");
  Check(block && block->size() > 1 && block->back() == L'\0' && (*block)[block->size() - 2] == L'\0',
        "double NUL environment");
  if (block) {
    const auto entries = DecodeEnvironment(*block);
    Check(entries.size() == 5, "whitelist only");
    const std::wstring all(block->begin(), block->end());
    Check(all.find(L"SENTINEL") == std::wstring::npos && all.find(L"REX_") == std::wstring::npos,
          "secrets overrides and poisoned paths removed");
    Check(all.find(L"PATH=C:\\Windows\\System32;C:\\Windows") != std::wstring::npos,
          "system PATH only");
    Check(all.find(L"APPDATA=D:\\Data") != std::wstring::npos, "canonical allowed variable names");
  }
  auto duplicate = source; duplicate.emplace_back(L"temp", L"D:\\Other");
  Check(!MakeOwnedEnvironment(duplicate, L"C:\\Windows", L"C:\\Windows\\System32"), "duplicate allowed rejected");
  auto nul = source; nul.front().second = std::wstring(L"D:\\x\0y", 6);
  Check(!MakeOwnedEnvironment(nul, L"C:\\Windows", L"C:\\Windows\\System32"), "embedded environment NUL rejected");
  auto huge = source; huge.front().second.assign(32767, L'x');
  Check(!MakeOwnedEnvironment(huge, L"C:\\Windows", L"C:\\Windows\\System32"), "bounded environment");
  Check(!MakeOwnedEnvironment(source, L"relative", L"C:\\Windows\\System32"), "trusted paths absolute");
  Check(!MakeOwnedEnvironment(source, L"C:\\Windows;D:\\Poison", L"C:\\Windows\\System32"), "no PATH injection");
  const auto before_path = ParentVariable(L"PATH");
  const auto before_rex = ParentVariable(L"REX_USER_DATA_ROOT");
  Check(BuildOwnedEnvironment().has_value(), "real child environment built");
  Check(before_path == ParentVariable(L"PATH") && before_rex == ParentVariable(L"REX_USER_DATA_ROOT"),
        "parent environment unchanged");
  OwnedProcess empty;
  Check(empty.ProcessId() == 0 && !empty.Poll() && !empty.TerminateAndWait(), "empty ownership rejects actions");

  std::array<wchar_t, 32768> module{};
  const DWORD count = GetModuleFileNameW(nullptr, module.data(), static_cast<DWORD>(module.size()));
  if (!count || count >= module.size()) return 98;
  const std::filesystem::path executable(std::wstring(module.data(), count));
  const auto temporary = std::filesystem::temp_directory_path();
  std::filesystem::path root;
  for (unsigned attempt = 0; attempt != 100; ++attempt) {
    const auto candidate = temporary / (L"mcla-process-fixture-" + std::to_wstring(GetCurrentProcessId()) +
        L"-" + std::to_wstring(GetTickCount64()) + L"-" + std::to_wstring(attempt));
    if (CreateDirectoryW(candidate.c_str(), nullptr)) { root = candidate; break; }
    if (GetLastError() != ERROR_ALREADY_EXISTS) break;
  }
  if (root.empty()) return 99;
  // Only this newly/exclusively-created fixture directory receives test writes.
  const auto receipt = root / L"receipt.txt";
  auto args = std::vector<std::wstring>{L"--receipt", receipt.native()};
  args.insert(args.end(), kArguments.begin(), kArguments.end());
  {
    auto child = StartOwnedProcess(executable, args, root);
    Check(child.has_value(), "owned fake child launch");
    if (child) {
      Check(child->ProcessId() != 0, "owned PID");
      auto moved = std::move(*child);
      Check(child->ProcessId() == 0 && !child->Poll(), "moved ownership inert");
      const auto status = Wait(moved);
      Check(status && status->exit_code == 0 && status->active_job_processes == 0,
            "actual argv environment cwd and normal job exit");
      std::ifstream input(receipt);
      std::string line; std::getline(input, line);
      Check(line == "ARGV_ENV_CWD_PASS", "fake receipt");
    } else { std::wcerr << child.error() << L'\n'; }
  }
  {
    auto child = StartOwnedProcess(executable, {L"--exit259", receipt.native()}, root);
    Check(child.has_value(), "exit259 child launch");
    if (child) { const auto status = Wait(*child); Check(status && status->exit_code == 259, "real259 is exited"); }
  }
  Check(!StartOwnedProcess(L"missing.exe", {}, root), "relative executable rejected");
  Check(!StartOwnedProcess(root / L"missing.exe", {}, root), "missing executable rejected");
  Check(!StartOwnedProcess(executable, {}, root / L"missing"), "missing working directory rejected");
  Check(!StartOwnedProcess(executable, {std::wstring(L"x\0y", 3)}, root), "NUL argument rejected");
  Check(!StartOwnedProcess(executable, {std::wstring(32767, L'x')}, root), "long command rejected");
  {
    auto one = StartOwnedProcess(executable, {L"--sleep", (root / L"one.txt").native()}, root);
    auto two = StartOwnedProcess(executable, {L"--sleep", (root / L"two.txt").native()}, root);
    Check(one && two && one->ProcessId() != two->ProcessId(), "two independent owned fake children");
    if (one && two) {
      Check(WaitForReady(*one, root / L"one.txt") && WaitForReady(*two, root / L"two.txt"),
            "both independent children reached their owned wait before cleanup");
      Check(one->TerminateAndWait().has_value(), "terminate first owned job");
      const auto survivor = two->Poll();
      if (!survivor) std::wcerr << L"Second-child poll failed: " << survivor.error() << L'\n';
      else if (survivor->exit_code || survivor->active_job_processes < 1)
        std::cerr << "Second-child state: exited=" << survivor->exit_code.has_value()
                  << " code=" << survivor->exit_code.value_or(0)
                  << " active=" << survivor->active_job_processes << '\n';
#if defined(MCLA_LAUNCHER_PROCESS_TEST_SEAM)
      const auto members = two->JobProcessIdsForTest();
      bool has_root = false, contains_first = false, approved_members = members.has_value();
      std::array<wchar_t, 32768> system_directory{};
      const UINT system_length = GetSystemDirectoryW(system_directory.data(), static_cast<UINT>(system_directory.size()));
      const std::wstring console_image = system_length && system_length < system_directory.size()
          ? (std::filesystem::path(std::wstring(system_directory.data(), system_length)) / L"conhost.exe").wstring()
          : std::wstring{};
      if (!members) std::wcerr << L"Owned-job census: " << members.error() << L'\n';
      else for (const auto pid : *members) {
        has_root |= pid == two->ProcessId();
        contains_first |= pid == one->ProcessId();
        std::array<wchar_t, 32768> image{};
        DWORD size = static_cast<DWORD>(image.size());
        const HANDLE member = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
        if (member && QueryFullProcessImageNameW(member, 0, image.data(), &size)) {
          std::wcout << L"Owned-job member PID=" << pid << L" root=" << (pid == two->ProcessId())
                     << L" image=" << std::wstring_view(image.data(), size) << L'\n';
          // Observed Windows conhost joins this CREATE_NO_WINDOW job. Only that
          // exact OS-system-directory helper is accepted besides our root.
          approved_members &= pid == two->ProcessId() || (!console_image.empty() &&
              CompareStringOrdinal(image.data(), static_cast<int>(size), console_image.data(),
                  static_cast<int>(console_image.size()), TRUE) == CSTR_EQUAL);
        } else {
          std::wcout << L"Owned-job member PID=" << pid << L" image unavailable\n";
          approved_members = false;
        }
        if (member) CloseHandle(member);
      }
      Check(has_root && !contains_first && approved_members,
            "other job contains its own root and only the proven Windows console helper");
#endif
      Check(survivor && !survivor->exit_code && survivor->active_job_processes >= 1,
            "cleanup does not touch other owned job");
      Check(!two->TerminateAndWait(30001), "overlong wait rejected without termination");
      Check(two->TerminateAndWait().has_value(), "terminate second owned job");
    }
  }
  HANDLE destruction_witness = nullptr;
  {
    auto child = StartOwnedProcess(executable, {L"--sleep", (root / L"raii.txt").native()}, root);
    Check(child.has_value(), "RAII child launched");
    if (child) destruction_witness = OpenProcess(SYNCHRONIZE, FALSE, child->ProcessId());
    Check(destruction_witness != nullptr, "observe exact owned child before destruction");
  }
  if (destruction_witness) {
    Check(WaitForSingleObject(destruction_witness, 5000) == WAIT_OBJECT_0,
          "RAII cleanup leaves no running owned root");
    CloseHandle(destruction_witness);
  }
  Check(before_path == ParentVariable(L"PATH") && before_rex == ParentVariable(L"REX_USER_DATA_ROOT"),
        "parent environment unchanged after actual children");
  // No broad recursive deletion: remove only exact fixture-owned known files.
  for (const auto *name : {L"receipt.txt", L"one.txt", L"two.txt", L"raii.txt"})
    DeleteFileW((root / name).c_str());
  Check(RemoveDirectoryW(root.c_str()) != FALSE, "only owned fixture removed");
  std::cout << "Owned process: " << checks << " checks, " << failures << " failures\n";
  return failures ? 1 : 0;
}
