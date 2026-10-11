#include "mcla_launcher_build_win.h"

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <bcrypt.h>

#include <array>
#include <functional>
#include <iostream>
#include <map>
#include <optional>
#include <span>

namespace {
using namespace mcla::launcher;
unsigned checks = 0;
unsigned failures = 0;
unsigned cases = 0;
void Check(bool value, const char *label) {
  ++checks;
  if (!value) { ++failures; std::cerr << "FAIL " << label << '\n'; }
}
std::string Hash(std::string_view bytes) {
  // Independent one-shot API checks the production streaming implementation.
  BCRYPT_ALG_HANDLE algorithm = nullptr;
  if (BCryptOpenAlgorithmProvider(&algorithm, BCRYPT_SHA256_ALGORITHM, nullptr, 0) < 0) return {};
  std::array<uint8_t, 32> digest{};
  const auto status = BCryptHash(algorithm, nullptr, 0,
      reinterpret_cast<PUCHAR>(const_cast<char *>(bytes.data())), static_cast<ULONG>(bytes.size()),
      digest.data(), static_cast<ULONG>(digest.size()));
  BCryptCloseAlgorithmProvider(algorithm, 0);
  if (status < 0) return {};
  constexpr char hex[] = "0123456789ABCDEF";
  std::string result;
  for (const auto byte : digest) { result += hex[byte >> 4]; result += hex[byte & 15]; }
  return result;
}
bool WriteNew(const std::wstring &path, std::string_view bytes) {
  const auto file = CreateFileW(path.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr);
  if (file == INVALID_HANDLE_VALUE) return false;
  DWORD written = 0;
  const bool okay = ::WriteFile(file, bytes.data(), static_cast<DWORD>(bytes.size()), &written, nullptr) && written == bytes.size();
  CloseHandle(file);
  return okay;
}
using Files = std::map<std::string, std::string>;
Files Minimal() { return {{"mcla.exe", "MZsynthetic-host-not-executable"}, {"mcla_crash_handler.exe", "MZsynthetic-crash-not-executable"}}; }
std::string Manifest(const Files &files) {
  std::string text = "MCLA-DEV-BUILD-V1\n";
  for (const auto &[name, bytes] : files)
    text += Hash(bytes) + " " + std::to_string(bytes.size()) + " " + name + "\n";
  return text;
}
std::wstring NewCase(const std::wstring &root) {
  const auto path = root + L"\\case-" + std::to_wstring(++cases);
  if (!CreateDirectoryW(path.c_str(), nullptr)) return {};
  return path;
}
bool Populate(const std::wstring &path, const Files &files, std::string_view manifest) {
  if (!WriteNew(path + L"\\" + std::wstring(kDeveloperBuildManifest), manifest)) return false;
  for (const auto &[name, bytes] : files) {
    if (!WriteNew(path + L"\\" + std::wstring(name.begin(), name.end()), bytes)) return false;
  }
  return true;
}
void RejectCase(const std::wstring &root, const Files &files, std::string_view manifest, const char *label) {
  const auto path = NewCase(root);
  Check(!path.empty() && Populate(path, files, manifest), "prepare owned negative fixture");
  auto directory = fs::OpenDirectory(path);
  Check(directory.has_value(), "open negative fixture guard");
  if (directory) Check(!VerifyBuild(*directory), label);
}
} // namespace

int wmain(int argc, wchar_t **) {
  if (argc != 1) return 2;
  wchar_t temp[MAX_PATH + 1]{};
  const auto length = GetTempPathW(MAX_PATH + 1, temp);
  if (!length || length > MAX_PATH) return 2;
  std::array<uint8_t, 16> random{};
  if (BCryptGenRandom(nullptr, random.data(), static_cast<ULONG>(random.size()), BCRYPT_USE_SYSTEM_PREFERRED_RNG) < 0) return 2;
  std::wstring root = std::wstring(temp, length) + L"mcla-build-verifier-test-";
  constexpr wchar_t hex[] = L"0123456789abcdef";
  for (const auto byte : random) { root += hex[byte >> 4]; root += hex[byte & 15]; }
  if (!CreateDirectoryW(root.c_str(), nullptr)) return 2;
  std::wcout << L"Owned synthetic fixture (preserved, never executed): " << root << L'\n';
  Check(Hash("abc") == "BA7816BF8F01CFEA414140DE5DAE2223B00361A396177A9CB410FF61F20015AD", "independent SHA256 known answer");

  auto files = Minimal();
  // Cross at least one streaming chunk boundary without requiring large files.
  files["rexruntime.dll"] = "MZ" + std::string(1024 * 1024 + 19, 'x');
  files["mcla_diagnostic_image_helper.exe"] = "MZsynthetic-helper";
  files["TracyClient.dll"] = "MZsynthetic-tracy";
  const auto manifest = Manifest(files);
  const auto path = NewCase(root);
  Check(Populate(path, files, manifest), "prepare positive synthetic build");
  auto directory = fs::OpenDirectory(path);
  Check(directory.has_value(), "open build guard");
  if (!directory) return 1;
  {
    auto verified = VerifyBuild(*directory);
    if (!verified) std::wcerr << L"Verify diagnostic: " << verified.error() << L'\n';
    Check(verified.has_value(), "verify synthetic build");
    if (verified) {
      Check(verified->fingerprint() == Hash(manifest), "fingerprint hashes exact manifest bytes");
      Check(verified->files().size() == files.size(), "all listed artifacts exposed");
      for (const auto &file : verified->files()) {
        const std::string name(file.name.begin(), file.name.end());
        Check(file.sha256 == Hash(files.at(name)) && file.bytes == files.at(name).size(), "artifact identities match");
      }
      for (const auto *name : {L"mcla.exe", L"rexruntime.dll", L"mcla-build.manifest"}) {
        const auto held = CreateFileW((path + L"\\" + name).c_str(), GENERIC_WRITE,
            FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
        Check(held == INVALID_HANDLE_VALUE, "verified handle blocks writes");
        if (held != INVALID_HANDLE_VALUE) CloseHandle(held);
        Check(!MoveFileExW((path + L"\\" + name).c_str(), (path + L"\\renamed.bin").c_str(), 0), "verified handle blocks rename/delete");
      }
    }
  }
  const auto writable = CreateFileW((path + L"\\mcla.exe").c_str(), GENERIC_WRITE,
      FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
  Check(writable != INVALID_HANDLE_VALUE, "destroying verified object releases file pins");
  if (writable != INVALID_HANDLE_VALUE) CloseHandle(writable);

  const auto minimal = Minimal();
  const auto base = Manifest(minimal);
  auto changed = minimal; changed.erase("mcla.exe");
  RejectCase(root, changed, base, "missing host file");
  changed = minimal; changed["mcla.exe"].back() = 'X';
  RejectCase(root, changed, base, "same-size hash mismatch");
  changed = minimal; changed["mcla.exe"] += 'X';
  RejectCase(root, changed, base, "file size mismatch");
  changed = minimal; changed["mcla.exe"].clear();
  RejectCase(root, changed, base, "zero-byte artifact");
  for (const auto *name : {"evil.exe", "EVIL.DLL", "MCLA.TOML", "README.txt", "mcla_wgc_helper.exe"}) {
    changed = minimal; changed[name] = "unlisted";
    RejectCase(root, changed, base, "unexpected case-insensitive inventory addition");
  }
  for (const auto *name : {"evil.dll", "mcla_wgc_helper.exe", "mcla.toml"}) {
    changed = minimal; changed[name] = "listed but forbidden";
    RejectCase(root, changed, Manifest(changed), "manifest cannot allow arbitrary/config/invented helper names");
  }
  changed = minimal; changed.erase("mcla_crash_handler.exe");
  RejectCase(root, changed, Manifest(changed), "required crash handler missing from manifest");
  RejectCase(root, minimal, "", "empty manifest");
  RejectCase(root, minimal, base.substr(0, base.size() - 1), "missing final LF");
  RejectCase(root, minimal, base + "\n", "blank extra entry");
  RejectCase(root, minimal, base + base.substr(base.find('\n') + 1), "duplicate entries");
  const auto first_row = base.find('\n') + 1;
  const auto second_row = base.find('\n', first_row) + 1;
  RejectCase(root, minimal, base.substr(0, first_row) + base.substr(second_row) +
      base.substr(first_row, second_row - first_row), "unsorted rows rejected");
  auto malformed = base; malformed[16] = '\r';
  RejectCase(root, minimal, malformed, "noncanonical CR/header");
  malformed = base; malformed[0] = 'X';
  RejectCase(root, minimal, malformed, "unsupported manifest header");
  malformed = base; malformed[base.find('\n') + 1] = 'g';
  RejectCase(root, minimal, malformed, "malformed hash");
  malformed = base; malformed.insert(malformed.find(' ') + 1, "0");
  RejectCase(root, minimal, malformed, "noncanonical leading-zero byte count");
  malformed = base;
  const auto size_begin = malformed.find(' ') + 1;
  malformed.replace(size_begin, malformed.find(' ', size_begin) - size_begin, "2147483649");
  RejectCase(root, minimal, malformed, "artifact over 2 GiB rejected before hash");
  malformed = base;
  malformed.replace(malformed.find("mcla.exe"), 8, "../mcla.exe");
  RejectCase(root, minimal, malformed, "manifest path traversal");
  RejectCase(root, minimal, std::string(64 * 1024 + 1, 'x'), "manifest byte bound");
  changed = minimal; changed["rexruntime.dll"] = "MZthird";
  malformed = "MCLA-DEV-BUILD-V1\n";
  for (const auto &[name, payload] : changed)
    malformed += Hash(payload) + " 2147483648 " + name + "\n";
  RejectCase(root, changed, malformed, "aggregate over 4 GiB rejected before hash");

  // Preserve hardlink/symlink fixtures; no files are deleted or executed.
  const auto hard_case = NewCase(root);
  auto missing_host = minimal; missing_host.erase("mcla.exe");
  Check(Populate(hard_case, missing_host, base), "prepare hardlink fixture");
  const auto host_source = root + L"\\hardlink-host-source.bin";
  Check(WriteNew(host_source, minimal.at("mcla.exe")), "create owned hardlink source");
  Check(CreateHardLinkW((hard_case + L"\\mcla.exe").c_str(), host_source.c_str(), nullptr) != FALSE, "create hardlinked artifact");
  auto hard_directory = fs::OpenDirectory(hard_case);
  Check(hard_directory && !VerifyBuild(*hard_directory), "hardlinked artifact rejected");
  const auto manifest_link_case = NewCase(root);
  for (const auto &[name, payload] : minimal)
    Check(WriteNew(manifest_link_case + L"\\" + std::wstring(name.begin(), name.end()), payload), "manifest-hardlink artifact preparation");
  const auto manifest_source = root + L"\\manifest-hardlink-source.txt";
  Check(WriteNew(manifest_source, base), "create owned manifest hardlink source");
  Check(CreateHardLinkW((manifest_link_case + L"\\" + std::wstring(kDeveloperBuildManifest)).c_str(), manifest_source.c_str(), nullptr) != FALSE, "create hardlinked manifest");
  auto manifest_link_directory = fs::OpenDirectory(manifest_link_case);
  Check(manifest_link_directory && !VerifyBuild(*manifest_link_directory), "hardlinked manifest rejected");
  const auto link_case = NewCase(root);
  Check(Populate(link_case, missing_host, base), "prepare reparse fixture");
  if (CreateSymbolicLinkW((link_case + L"\\mcla.exe").c_str(), host_source.c_str(), 2)) {
    auto link_directory = fs::OpenDirectory(link_case);
    Check(link_directory && !VerifyBuild(*link_directory), "reparse artifact rejected");
  } else {
    std::cout << "SKIP symlink unavailable (Win32 " << GetLastError() << ")\n";
  }
  const auto nested_case = NewCase(root);
  Check(Populate(nested_case, minimal, base), "prepare nested inventory fixture");
  Check(CreateDirectoryW((nested_case + L"\\nested").c_str(), nullptr) != FALSE, "create unexpected subdirectory");
  auto nested_directory = fs::OpenDirectory(nested_case);
  Check(nested_directory && !VerifyBuild(*nested_directory), "nested payload directories rejected");
  std::cout << "Launcher build verifier: " << checks << " checks, " << failures << " failures\n";
  return failures ? 1 : 0;
}
