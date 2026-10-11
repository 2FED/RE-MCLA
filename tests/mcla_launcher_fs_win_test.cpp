#include "mcla_launcher_fs_win.h"

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <bcrypt.h>

#include <array>
#include <filesystem>
#include <iostream>
#include <string>
#include <vector>

namespace {
namespace fs = mcla::launcher::fs;
unsigned checks = 0;
unsigned failures = 0;
void Check(bool value, const char *label) {
  ++checks;
  if (!value) { ++failures; std::cerr << "FAIL " << label << '\n'; }
}
void CheckWrite(const std::expected<void, std::wstring> &result, const char *label) {
  if (!result) std::wcerr << L"Atomic write diagnostic: " << result.error() << L'\n';
  Check(result.has_value(), label);
}
bool MakeFile(const std::wstring &path, std::span<const uint8_t> bytes) {
  const auto handle = CreateFileW(path.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr);
  if (handle == INVALID_HANDLE_VALUE) return false;
  DWORD written = 0;
  const bool ok = ::WriteFile(handle, bytes.data(), static_cast<DWORD>(bytes.size()), &written, nullptr) && written == bytes.size();
  CloseHandle(handle);
  return ok;
}
int ProbeChild(const std::wstring &root) {
  std::wstring executable(32768, L'\0');
  const auto count = GetModuleFileNameW(nullptr, executable.data(), static_cast<DWORD>(executable.size()));
  if (!count || count >= executable.size()) return -1;
  executable.resize(count);
  auto command = L"\"" + executable + L"\" --lock-probe \"" + root + L"\"";
  STARTUPINFOW startup{}; startup.cb = sizeof(startup);
  PROCESS_INFORMATION process{};
  if (!CreateProcessW(executable.c_str(), command.data(), nullptr, nullptr, FALSE, CREATE_NO_WINDOW,
                      nullptr, nullptr, &startup, &process)) return -2;
  CloseHandle(process.hThread);
  const auto wait = WaitForSingleObject(process.hProcess, 15000);
  if (wait != WAIT_OBJECT_0) {
    // Only the exact synthetic child handle created by this test is terminated.
    TerminateProcess(process.hProcess, 90);
    WaitForSingleObject(process.hProcess, 5000);
    CloseHandle(process.hProcess);
    return -3;
  }
  DWORD exit = 0;
  const bool ok = GetExitCodeProcess(process.hProcess, &exit) != FALSE;
  CloseHandle(process.hProcess);
  return ok ? static_cast<int>(exit) : -4;
}
} // namespace

int wmain(int argc, wchar_t **argv) {
  if (argc == 3 && std::wstring_view(argv[1]) == L"--lock-probe") {
    auto directory = fs::OpenDirectory(argv[2]);
    if (!directory) return 2;
    auto lock = fs::AcquireWriterLock(*directory);
    return lock ? 0 : 3;
  }
  if (argc != 1) { std::cerr << "Unexpected test arguments\n"; return 2; }
  wchar_t temp[MAX_PATH + 1]{};
  const auto temp_length = GetTempPathW(MAX_PATH + 1, temp);
  if (!temp_length || temp_length > MAX_PATH) return 2;
  std::array<uint8_t, 16> random{};
  if (BCryptGenRandom(nullptr, random.data(), static_cast<ULONG>(random.size()), BCRYPT_USE_SYSTEM_PREFERRED_RNG) < 0) return 2;
  constexpr wchar_t hex[] = L"0123456789abcdef";
  std::wstring root = std::wstring(temp, temp_length) + L"mcla-launcher-fs-test-";
  for (const auto byte : random) { root += hex[byte >> 4]; root += hex[byte & 15]; }
  // CREATE_NEW-equivalent directory creation proves ownership. Nothing is
  // recursively removed: preserve this unique synthetic fixture for inspection.
  if (!CreateDirectoryW(root.c_str(), nullptr)) return 2;
  const auto a_path = root + L"\\a";
  const auto b_path = root + L"\\b";
  const auto nested_path = a_path + L"\\nested";
  if (!CreateDirectoryW(a_path.c_str(), nullptr) || !CreateDirectoryW(b_path.c_str(), nullptr) ||
      !CreateDirectoryW(nested_path.c_str(), nullptr) ||
      !CreateDirectoryW((b_path + L"\\child").c_str(), nullptr)) return 2;
  std::wcout << L"Owned synthetic fixture (preserved): " << root << L'\n';
  auto a = fs::OpenDirectory(a_path);
  auto b = fs::OpenDirectory(b_path);
  Check(a.has_value() && b.has_value(), "open owned fixed-drive directories");
  if (!a || !b) {
    if (!a) std::wcerr << a.error() << L'\n';
    if (!b) std::wcerr << b.error() << L'\n';
    return 1;
  }
  Check(fs::CompareDisjoint(*a, *b).has_value(), "different roots are disjoint");
  auto nested = fs::OpenDirectory(nested_path);
  Check(nested.has_value(), "open nested fixture");
  if (nested) Check(!fs::CompareDisjoint(*a, *nested) && !fs::CompareDisjoint(*nested, *a), "ancestor identity overlaps both ways");
  auto case_path = a_path;
  case_path.back() = L'A';
  auto alias = fs::OpenDirectory(case_path);
  Check(alias.has_value(), "case alias can be inspected");
  if (alias) Check(!fs::CompareDisjoint(*a, *alias), "case alias is not disjoint");
  for (const auto *path : {L"", L"C:\\", L"relative", L"C:relative", L"\\\\server\\share", L"\\\\?\\C:\\folder", L"C:/a/../b"})
    Check(!fs::OpenDirectory(path), "malformed path rejected");
  Check(!fs::OpenDirectory(root + L"\\missing"), "missing directory never created");
  Check(GetFileAttributesW((root + L"\\missing").c_str()) == INVALID_FILE_ATTRIBUTES, "missing path remains absent");
  const auto renamed = root + L"-renamed";
  const bool rename_worked = MoveFileExW(root.c_str(), renamed.c_str(), 0) != FALSE;
  Check(!rename_worked, "every held ancestor prevents rename");
  if (rename_worked && !MoveFileExW(renamed.c_str(), root.c_str(), 0)) return 1;

  const std::vector<uint8_t> first{1, 2, 0, 4, 255};
  const std::vector<uint8_t> second{9, 8, 7};
  {
    auto lock = fs::AcquireWriterLock(*a);
    Check(lock.has_value(), "acquire root writer lock");
    Check(!fs::AcquireWriterLock(*a), "same process second lock rejected");
    Check(ProbeChild(a_path) == 3, "second process same root rejected");
    Check(ProbeChild(b_path) == 0, "second process different root independent");
    auto b_lock = fs::AcquireWriterLock(*b);
    Check(b_lock.has_value(), "different root concurrent lock");
    CheckWrite(fs::AtomicWrite(*a, L"library.bin", first), "atomic first write");
    auto read = fs::ReadFile(*a, L"library.bin", first.size());
    Check(read && *read == first, "exact binary read");
    Check(!fs::ReadFile(*a, L"library.bin", first.size() - 1), "oversize read rejected without truncating");
    CheckWrite(fs::AtomicWrite(*a, L"library.bin", second), "atomic replacement");
    read = fs::ReadFile(*a, L"library.bin", first.size());
    Check(read && *read == second, "replacement has no trailing old bytes");
    CheckWrite(fs::AtomicWrite(*a, L"empty.bin", {}), "empty atomic file");
    read = fs::ReadFile(*a, L"empty.bin", 0);
    Check(read && read->empty(), "zero-length exact read");
    Check(!fs::ReadFile(*a, L"absent.bin", 100), "missing read fails");
    Check(!fs::ReadFile(*a, L"library.bin", 64 * 1024 * 1024 + 1), "excessive caller read limit rejected");
    const auto concurrent_writer = CreateFileW((a_path + L"\\library.bin").c_str(), GENERIC_WRITE,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    Check(concurrent_writer != INVALID_HANDLE_VALUE, "synthetic concurrent writer opened");
    if (concurrent_writer != INVALID_HANDLE_VALUE) {
      Check(!fs::ReadFile(*a, L"library.bin", 100), "read refuses concurrent mutable writer");
      CloseHandle(concurrent_writer);
    }
    for (const auto *name : {L"", L"../escape", L"sub/file", L"sub\\file", L"CON", L"data:stream", L"file.", L"file ", L".mcla-writer.lock", L".MCLA-TMP-x"}) {
      Check(!fs::AtomicWrite(*a, name, first), "unsafe write name rejected");
      Check(!fs::ReadFile(*a, name, 100), "unsafe read name rejected");
    }
    Check(!fs::AtomicWrite(*a, L"nested", first), "directory replacement rejected");
    Check(!fs::ReadFile(*a, L"nested", 100), "directory read rejected");
    const auto held = CreateFileW((a_path + L"\\library.bin").c_str(), GENERIC_READ, FILE_SHARE_READ,
                                  nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    Check(held != INVALID_HANDLE_VALUE, "synthetic reader holds target");
    if (held != INVALID_HANDLE_VALUE) {
      Check(!fs::AtomicWrite(*a, L"library.bin", first), "failed publication preserves old file");
      CloseHandle(held);
      read = fs::ReadFile(*a, L"library.bin", 100);
      Check(read && *read == second, "old bytes intact after failed replace");
      size_t partials = 0;
      for (const auto &entry : std::filesystem::directory_iterator(a_path)) {
        if (entry.path().filename().wstring().starts_with(L".mcla-tmp-")) ++partials;
      }
      Check(partials == 1, "failed owned temporary preserved");
    }
  }
  Check(ProbeChild(a_path) == 0, "lock released by close despite persistent file");
  Check(GetFileAttributesW((a_path + L"\\.mcla-writer.lock").c_str()) != INVALID_FILE_ATTRIBUTES, "lock file not deleted");

  const auto bad_path = root + L"\\bad-lock";
  Check(CreateDirectoryW(bad_path.c_str(), nullptr) != FALSE, "create owned malformed-lock fixture");
  Check(MakeFile(bad_path + L"\\.mcla-writer.lock", first), "create nonempty lock fixture");
  auto bad_directory = fs::OpenDirectory(bad_path);
  if (bad_directory) Check(!fs::AcquireWriterLock(*bad_directory), "nonempty lock is not stale-deleted");
  else Check(false, "open malformed-lock fixture");
  const auto hard_source = b_path + L"\\hard-source.bin";
  Check(MakeFile(hard_source, first), "create hardlink source fixture");
  const bool hardlink = CreateHardLinkW((a_path + L"\\hard.bin").c_str(), hard_source.c_str(), nullptr) != FALSE;
  Check(hardlink, "create same-volume hardlink fixture");
  if (hardlink) {
    Check(!fs::ReadFile(*a, L"hard.bin", 100), "hardlinked read rejected");
    Check(!fs::AtomicWrite(*a, L"hard.bin", second), "hardlinked replacement rejected");
  }
  const auto unicode_path = root + L"\\\u0421\u0435\u0439\u0432\u0438";
  Check(CreateDirectoryW(unicode_path.c_str(), nullptr) != FALSE, "create Unicode fixture directory");
  auto unicode_directory = fs::OpenDirectory(unicode_path);
  Check(unicode_directory.has_value(), "open Unicode directory");
  if (unicode_directory) {
    auto unicode_lock = fs::AcquireWriterLock(*unicode_directory);
    Check(unicode_lock.has_value(), "Unicode root writer lock");
    CheckWrite(fs::AtomicWrite(*unicode_directory, L"\u043c\u0456\u0439.bin", first), "Unicode atomic filename");
    const auto read = fs::ReadFile(*unicode_directory, L"\u043c\u0456\u0439.bin", 100);
    Check(read && *read == first, "Unicode exact read");
  }
  const auto directory_link = root + L"\\directory-link";
  if (CreateSymbolicLinkW(directory_link.c_str(), b_path.c_str(), SYMBOLIC_LINK_FLAG_DIRECTORY | 2)) {
    Check(!fs::OpenDirectory(directory_link), "directory reparse root rejected");
    Check(!fs::OpenDirectory(directory_link + L"\\child"), "directory reparse ancestor rejected");
  } else {
    std::cout << "SKIP directory symlink creation unavailable (Win32 " << GetLastError() << ")\n";
  }
  const auto file_link = a_path + L"\\file-link.bin";
  if (CreateSymbolicLinkW(file_link.c_str(), hard_source.c_str(), 2)) {
    Check(!fs::ReadFile(*a, L"file-link.bin", 100), "file reparse read rejected");
    Check(!fs::AtomicWrite(*a, L"file-link.bin", second), "file reparse replacement rejected");
  } else {
    std::cout << "SKIP file symlink creation unavailable (Win32 " << GetLastError() << ")\n";
  }
  std::cout << "Launcher filesystem guard: " << checks << " checks, " << failures << " failures\n";
  return failures ? 1 : 0;
}
