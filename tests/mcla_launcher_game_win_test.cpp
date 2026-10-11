#include "mcla_launcher_game_win.h"

#ifndef MCLA_LAUNCHER_GAME_TESTING
#error This synthetic target requires MCLA_LAUNCHER_GAME_TESTING; never define it for the launcher.
#endif

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <bcrypt.h>

#include <array>
#include <filesystem>
#include <iostream>
#include <map>
#include <span>

namespace {
using namespace mcla::launcher;
unsigned checks = 0, failures = 0, cases = 0;
void Check(bool value, const char *label) {
  ++checks;
  if (!value) { ++failures; std::cerr << "FAIL " << label << '\n'; }
}
std::string Hash(std::string_view bytes) {
  BCRYPT_ALG_HANDLE algorithm = nullptr;
  if (BCryptOpenAlgorithmProvider(&algorithm, BCRYPT_SHA256_ALGORITHM, nullptr, 0) < 0) return {};
  std::array<uint8_t, 32> digest{};
  const auto status = BCryptHash(algorithm, nullptr, 0,
      reinterpret_cast<PUCHAR>(const_cast<char *>(bytes.data())), static_cast<ULONG>(bytes.size()), digest.data(), static_cast<ULONG>(digest.size()));
  BCryptCloseAlgorithmProvider(algorithm, 0);
  if (status < 0) return {};
  constexpr char hex[] = "0123456789ABCDEF";
  std::string result;
  for (const auto byte : digest) { result += hex[byte >> 4]; result += hex[byte & 15]; }
  return result;
}
bool WriteNew(const std::wstring &path, std::string_view payload) {
  const auto file = CreateFileW(path.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr);
  if (file == INVALID_HANDLE_VALUE) return false;
  DWORD written = 0;
  const bool okay = ::WriteFile(file, payload.data(), static_cast<DWORD>(payload.size()), &written, nullptr) && written == payload.size();
  CloseHandle(file);
  return okay;
}
using Files = std::map<std::wstring, std::string>;
Files Payloads() {
  return {{L"$SystemUpdate/system.manifest", "synthetic-system"}, {L"default.xex", "synthetic-XEX-not-executable"},
          {L"xarchive_cache.rpf", "synthetic-RPF" + std::string(1024 * 1024 + 17, 'x')}};
}
std::vector<VerifiedGameFile> Expected(const Files &files) {
  std::vector<VerifiedGameFile> expected;
  for (const auto &[name, payload] : files) expected.push_back({name, payload.size(), Hash(payload)});
  return expected;
}
std::wstring NewCase(const std::wstring &root, const Files &files) {
  const auto path = root + L"\\case-" + std::to_wstring(++cases);
  if (!CreateDirectoryW(path.c_str(), nullptr)) return {};
  for (const auto &[name, payload] : files) {
    const auto destination = std::filesystem::path(path) / name;
    std::error_code error;
    std::filesystem::create_directories(destination.parent_path(), error);
    if (error || !WriteNew(destination.wstring(), payload)) return {};
  }
  return path;
}
void Reject(const std::wstring &root, const Files &files, const std::vector<VerifiedGameFile> &expected, const char *label) {
  const auto path = NewCase(root, files);
  Check(!path.empty(), "create owned negative fixture");
  auto directory = fs::OpenDirectory(path);
  Check(directory.has_value(), "guard negative fixture");
  if (directory) Check(!VerifyGameRootForTest(*directory, expected), label);
}
} // namespace

int wmain(int argc, wchar_t **) {
  if (argc != 1) return 2;
  wchar_t temp[MAX_PATH + 1]{};
  const auto length = GetTempPathW(MAX_PATH + 1, temp);
  if (!length || length > MAX_PATH) return 2;
  std::array<uint8_t, 16> random{};
  if (BCryptGenRandom(nullptr, random.data(), static_cast<ULONG>(random.size()), BCRYPT_USE_SYSTEM_PREFERRED_RNG) < 0) return 2;
  std::wstring root = std::wstring(temp, length) + L"mcla-game-verifier-test-";
  constexpr wchar_t hex[] = L"0123456789abcdef";
  for (const auto byte : random) { root += hex[byte >> 4]; root += hex[byte & 15]; }
  if (!CreateDirectoryW(root.c_str(), nullptr)) return 2;
  std::wcout << L"Owned synthetic game fixture (preserved; no game data read): " << root << L'\n';
  Check(Hash("abc") == "BA7816BF8F01CFEA414140DE5DAE2223B00361A396177A9CB410FF61F20015AD", "independent SHA256 known answer");
  const auto supported = SupportedGameFiles();
  uint64_t total = 0;
  bool known_xex = false;
  for (const auto &file : supported) {
    total += file.bytes;
    if (file.relative_path == L"default.xex") known_xex = file.bytes == 9252864 && file.sha256 == "C386F4001FA569E6AD4B982F441F67412F00B3F47C166134555CD4B59854A432";
  }
  Check(supported.size() == 15 && total == 6569586392, "embedded supported inventory cardinality/bytes");
  Check(known_xex, "embedded independently documented default.xex identity");
  const auto files = Payloads();
  const auto expected = Expected(files);
  const auto path = NewCase(root, files);
  auto directory = fs::OpenDirectory(path);
  Check(directory.has_value(), "open positive synthetic game root");
  if (!directory) return 1;
  {
    auto verified = VerifyGameRootForTest(*directory, expected);
    if (!verified) std::wcerr << L"Game verify diagnostic: " << verified.error() << L'\n';
    Check(verified.has_value(), "verify full synthetic inventory with multichunk file");
    if (verified) {
      Check(verified->files().size() == expected.size() && verified->fingerprint().size() == 64, "verified game metadata exposed");
      std::string metadata = "MCLA-VERIFIED-GAME-V1\nSYNTHETIC-TEST-ONLY\n";
      for (const auto &file : expected)
        metadata += file.sha256 + " " + std::to_string(file.bytes) + " " + std::string(file.relative_path.begin(), file.relative_path.end()) + "\n";
      Check(verified->fingerprint() == Hash(metadata), "game fingerprint exact metadata contract");
      for (const auto &file : expected) {
        const auto full = (std::filesystem::path(path) / file.relative_path).wstring();
        const auto writer = CreateFileW(full.c_str(), GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
            nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
        Check(writer == INVALID_HANDLE_VALUE, "game file pins deny writes");
        if (writer != INVALID_HANDLE_VALUE) CloseHandle(writer);
        Check(!MoveFileExW(full.c_str(), (path + L"\\moved.bin").c_str(), 0), "game file pins deny rename");
      }
      Check(!MoveFileExW((path + L"\\$SystemUpdate").c_str(), (path + L"\\moved-directory").c_str(), 0), "game internal directory pinned");
    }
  }
  const auto writer = CreateFileW((path + L"\\default.xex").c_str(), GENERIC_WRITE,
      FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
  Check(writer != INVALID_HANDLE_VALUE, "game pins release after verified object destruction");
  if (writer != INVALID_HANDLE_VALUE) CloseHandle(writer);
  Check(!VerifyGameRoot(*directory), "production expected metadata cannot accept synthetic fixture");

  auto changed = files; changed.erase(L"default.xex"); Reject(root, changed, expected, "missing file rejected");
  changed = files; changed[L"default.xex"].back() = 'X'; Reject(root, changed, expected, "same-size hash mismatch rejected");
  changed = files; changed[L"default.xex"] += 'X'; Reject(root, changed, expected, "size mismatch rejected");
  changed = files; changed[L"EXTRA.DLL"] = "extra"; Reject(root, changed, expected, "unexpected uppercase executable payload rejected");
  changed = files; changed[L"other/file"] = "extra"; Reject(root, changed, expected, "unexpected nested directory rejected");
  auto invalid = expected; invalid[0].relative_path = L"../escape"; Reject(root, files, invalid, "expected path traversal rejected");
  invalid = expected; invalid[0].relative_path = L"C:/absolute"; Reject(root, files, invalid, "expected absolute path rejected");
  invalid = expected; invalid[0].relative_path = L"x:stream"; Reject(root, files, invalid, "expected ADS rejected");
  invalid = expected; invalid[0].relative_path = L"CON"; Reject(root, files, invalid, "expected reserved device rejected");
  invalid = expected; invalid[0].relative_path = L"dir\\file"; Reject(root, files, invalid, "expected noncanonical separator rejected");
  invalid = expected; invalid[1].relative_path = invalid[0].relative_path; Reject(root, files, invalid, "expected duplicate path rejected");
  invalid = expected; invalid[1].relative_path = L"$SYSTEMUPDATE/SYSTEM.MANIFEST"; Reject(root, files, invalid, "expected case alias rejected");
  invalid = expected; invalid[0].sha256[0] = 'g'; Reject(root, files, invalid, "expected malformed hash rejected");
  invalid = expected; invalid[0].bytes = 0; Reject(root, files, invalid, "zero expected size rejected");
  invalid = expected; invalid[0].bytes = uint64_t{2} * 1024 * 1024 * 1024 + 1; Reject(root, files, invalid, "expected per-file byte bound");
  invalid.clear(); Reject(root, files, invalid, "empty expected inventory rejected");
  invalid.resize(65); Reject(root, files, invalid, "expected inventory count bound");

  changed = files; changed.erase(L"default.xex");
  const auto hard_case = NewCase(root, changed);
  const auto source = root + L"\\hardlink-source.bin";
  Check(WriteNew(source, files.at(L"default.xex")), "create owned hardlink source");
  Check(CreateHardLinkW((hard_case + L"\\default.xex").c_str(), source.c_str(), nullptr) != FALSE, "create hardlinked synthetic game file");
  auto hard_directory = fs::OpenDirectory(hard_case);
  Check(hard_directory && !VerifyGameRootForTest(*hard_directory, expected), "hardlinked game file rejected");
  const auto link_case = NewCase(root, changed);
  if (CreateSymbolicLinkW((link_case + L"\\default.xex").c_str(), source.c_str(), 2)) {
    auto link_directory = fs::OpenDirectory(link_case);
    Check(link_directory && !VerifyGameRootForTest(*link_directory, expected), "reparse game file rejected");
  } else {
    std::cout << "SKIP symlink unavailable (Win32 " << GetLastError() << ")\n";
  }
  std::cout << "Launcher game verifier: " << checks << " checks, " << failures << " failures\n";
  return failures ? 1 : 0;
}
