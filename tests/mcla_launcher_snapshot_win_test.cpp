#include "mcla_launcher_snapshot_win.h"

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <bcrypt.h>

#include <array>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <vector>

namespace {
namespace fs = mcla::launcher::fs;
unsigned checks = 0, failures = 0;
void Check(bool good, const char *label) {
  ++checks;
  if (!good) { ++failures; std::cerr << "FAIL " << label << '\n'; }
}
bool NewFile(const std::filesystem::path &path, std::string_view contents) {
  const HANDLE file = CreateFileW(path.c_str(), GENERIC_WRITE, 0, nullptr,
      CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr);
  if (file == INVALID_HANDLE_VALUE) return false;
  DWORD count = 0;
  const bool written = WriteFile(file, contents.data(), static_cast<DWORD>(contents.size()), &count, nullptr) &&
      count == contents.size() && FlushFileBuffers(file);
  CloseHandle(file);
  return written;
}
std::string Read(const std::filesystem::path &path) {
  std::ifstream file(path, std::ios::binary);
  return {std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
}
bool Directory(const std::filesystem::path &path) { return CreateDirectoryW(path.c_str(), nullptr) != FALSE; }
std::string NameHex(const std::filesystem::path &path) {
  std::string result;
  for (const char16_t c : path.generic_u16string())
    for (const int shift : {12, 8, 4, 0}) result += "0123456789ABCDEF"[(c >> shift) & 15];
  return result;
}
std::expected<std::string, std::wstring> Copy(const std::filesystem::path &a, const std::filesystem::path &b) {
  auto source = fs::OpenDirectory(a.wstring());
  auto dest = fs::OpenDirectory(b.wstring());
  if (!source) return std::unexpected(source.error());
  if (!dest) return std::unexpected(dest.error());
  return mcla::launcher::CopySaveTree(*source, *dest);
}

#if defined(MCLA_LAUNCHER_SNAPSHOT_TEST_SEAM)
struct Drift { std::filesystem::path source, destination; bool source_added = false, destination_write_denied = false; };
void AddFileDuringCopy(void *context) {
  auto &state = *static_cast<Drift *>(context);
  state.source_added = NewFile(state.source / L"late.txt", "late cooperative-writer violation");
  HANDLE opened = CreateFileW((state.destination / L"original.txt").c_str(), GENERIC_WRITE,
      FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, 0, nullptr);
  state.destination_write_denied = opened == INVALID_HANDLE_VALUE;
  if (opened != INVALID_HANDLE_VALUE) CloseHandle(opened);
}
#endif
} // namespace

int main() {
  std::array<unsigned char, 16> random{};
  if (BCryptGenRandom(nullptr, random.data(), static_cast<ULONG>(random.size()), BCRYPT_USE_SYSTEM_PREFERRED_RNG) < 0)
    return 2;
  std::wstring name = L"mcla-snapshot-test-";
  for (const auto b : random) { name += L"0123456789abcdef"[b >> 4]; name += L"0123456789abcdef"[b & 15]; }
  const auto root = std::filesystem::temp_directory_path() / name;
  if (!Directory(root)) return 2;
  std::wcout << L"Owned synthetic snapshot fixture (preserved): " << root.wstring() << L'\n';
  auto owner = fs::OpenDirectory(root.wstring());
  if (!owner) return 2;
  // Writer coordination belongs outside copied source/destination trees.
  auto lock = fs::AcquireWriterLock(*owner);
  if (!lock) return 2;
  const auto source = root / L"source", first = root / L"first", second = root / L"second";
  for (const auto &path : {source, first, second, source / L"z-empty", source / L"a", source / L"a/nested"})
    if (!Directory(path)) return 2;
  const auto unicode = std::filesystem::path(L"a/\u0421\u0435\u0439\u0432 \u0437 \u043f\u0440\u043e\u0431\u0456\u043b\u043e\u043c.bin");
  if (!NewFile(source / L"hello.txt", "hello") || !NewFile(source / unicode, std::string("a\0b", 3)) ||
      !NewFile(source / L"a/nested/zero.bin", "")) return 2;
  const auto inspect = [&](const std::filesystem::path &tree) {
    auto guard = fs::OpenDirectory(tree.wstring());
    if (!guard) return false;
    const auto result = mcla::launcher::ValidateSaveTreeForSnapshot(*guard, (root / L"never-created").wstring());
    if (!result && tree == source) std::wcerr << result.error() << L'\n';
    return result.has_value();
  };
  Check(inspect(source), "read-only snapshot preflight accepts complete nested Unicode source");
  Check(!std::filesystem::exists(root / L"never-created"), "snapshot preflight never creates destination");
  const auto copied = Copy(source, first);
  Check(copied.has_value(), "complete nested/empty/Unicode tree copied");
  if (!copied) std::wcerr << copied.error() << L'\n';
  Check(Read(first / L"hello.txt") == "hello" && Read(first / unicode) == std::string("a\0b", 3),
        "destination exact bytes read independently");
  Check(std::filesystem::is_directory(first / L"z-empty") && std::filesystem::is_empty(first / L"z-empty"),
        "empty directory preserved");
  Check(std::filesystem::is_regular_file(first / L"a/nested/zero.bin") &&
      std::filesystem::file_size(first / L"a/nested/zero.bin") == 0, "empty file preserved");
  if (copied) {
    const std::string hello = "F 2CF24DBA5FB0A30E26E83B2AC5B9E29E1B161E5C1FA7425E73043362938B9824 5 " +
        NameHex(L"hello.txt") + "\n";
    Check(copied->find(hello) != std::string::npos, "known SHA256 manifest digest");
    Check(copied->find("D " + NameHex(L"a") + "\n") <
          copied->find("D " + NameHex(L"z-empty") + "\n"), "deterministic sorted directories");
  }
  const auto again = Copy(source, second);
  Check(copied && again && *copied == *again, "repeat immutable source gives identical full manifest");
  Check(Read(source / L"hello.txt") == "hello" && Read(source / unicode) == std::string("a\0b", 3),
        "ordinary copy leaves source bytes unchanged");
  Check(!Copy(source, first), "nonempty destination never overwritten");
  Check(Read(first / L"hello.txt") == "hello", "existing destination remains unchanged");
  Check(!Copy(source, source) && !Copy(source, source / L"z-empty"), "equal/nested destination rejected");
  const auto empty = root / L"empty", empty_dest = root / L"empty-dest";
  if (!Directory(empty) || !Directory(empty_dest)) return 2;
  const auto empty_copy = Copy(empty, empty_dest);
  Check(empty_copy && *empty_copy == "MCLA-SAVE-SNAPSHOT-V1\n", "empty tree explicit complete manifest");

  const auto hard = root / L"hard", hard_dest = root / L"hard-dest";
  if (!Directory(hard) || !Directory(hard_dest) || !NewFile(root / L"hard-outside.bin", "preserve")) return 2;
  if (CreateHardLinkW((hard / L"linked.bin").c_str(), (root / L"hard-outside.bin").c_str(), nullptr)) {
    Check(!Copy(hard, hard_dest), "hardlinked source rejected");
    Check(!inspect(hard), "read-only snapshot preflight rejects hardlinks");
    Check(Read(root / L"hard-outside.bin") == "preserve" && std::filesystem::is_empty(hard_dest),
          "hardlink outside target preserved and destination untouched");
  } else { Check(false, "hardlink fixture must be supported on admitted filesystem"); }

  const auto streams = root / L"streams", stream_dest = root / L"streams-dest";
  if (!Directory(streams) || !Directory(stream_dest) || !NewFile(streams / L"data.bin", "main")) return 2;
  Check(NewFile(streams / L"data.bin:extra", "must-not-be-omitted"), "named-stream fixture created");
  Check(!Copy(streams, stream_dest) && Read(streams / L"data.bin:extra") == "must-not-be-omitted",
        "named stream rejected without source mutation");
  Check(!inspect(streams), "read-only snapshot preflight rejects named streams");

  const auto redirected = root / L"redirected", redirect_dest = root / L"redirect-dest";
  if (!Directory(redirected) || !Directory(redirect_dest)) return 2;
  if (CreateSymbolicLinkW((redirected / L"link").c_str(), source.c_str(), SYMBOLIC_LINK_FLAG_DIRECTORY | 2)) {
    Check(!Copy(redirected, redirect_dest) && std::filesystem::is_empty(redirect_dest), "directory reparse rejected");
  } else { std::cout << "SKIP reparse creation unavailable; Win32 " << GetLastError() << '\n'; }

  const auto malformed = root / L"malformed", malformed_dest = root / L"malformed-dest";
  if (!Directory(malformed) || !Directory(malformed_dest)) return 2;
  const auto trailing = std::filesystem::path(L"\\\\?\\" + (malformed / L"trailing.").wstring());
  if (NewFile(trailing, "unsafe")) {
    Check(!Copy(malformed, malformed_dest), "extended-path trailing-dot component rejected");
    Check(!inspect(malformed), "read-only snapshot preflight rejects unsafe components");
    Check(Read(trailing) == "unsafe", "unsafe source name remains untouched");
  } else { std::cout << "SKIP trailing-dot fixture unavailable; Win32 " << GetLastError() << '\n'; }

#if defined(MCLA_LAUNCHER_SNAPSHOT_TEST_SEAM)
  const auto drift_source = root / L"drift", drift_dest = root / L"drift-dest";
  if (!Directory(drift_source) || !Directory(drift_dest) || !NewFile(drift_source / L"original.txt", "original")) return 2;
  auto drift_from = fs::OpenDirectory(drift_source.wstring());
  auto drift_to = fs::OpenDirectory(drift_dest.wstring());
  if (!drift_from || !drift_to) return 2;
  Drift state{drift_source, drift_dest};
  const auto drift = mcla::launcher::CopySaveTreeBeforeVerifyForTest(*drift_from, *drift_to, AddFileDuringCopy, &state);
  Check(state.source_added && !drift, "final whole-source inventory detects a newly added file");
  Check(state.destination_write_denied, "output handles deny writes through final verification");
  Check(Read(drift_dest / L"original.txt") == "original" && Read(drift_source / L"original.txt") == "original",
        "failure preserves incomplete destination and original source");
#else
  std::cout << "SKIP deterministic mutation fixture: build with MCLA_LAUNCHER_SNAPSHOT_TEST_SEAM\n";
#endif
  std::cout << "Launcher snapshot: " << checks << " checks, " << failures << " failures\n";
  return failures ? 1 : 0;
}
