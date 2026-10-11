#include "mcla_launcher_build_win.h"

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <bcrypt.h>

#include <algorithm>
#include <array>
#include <limits>
#include <optional>
#include <span>
#include <utility>

namespace mcla::launcher {
namespace {
constexpr std::string_view kHeader = "MCLA-DEV-BUILD-V1\n";
constexpr uint64_t kMaximumFileBytes = uint64_t{2} * 1024 * 1024 * 1024;
constexpr uint64_t kMaximumTotalBytes = uint64_t{4} * 1024 * 1024 * 1024;
constexpr size_t kMaximumManifestBytes = 64 * 1024;
constexpr size_t kMaximumFiles = 16;
constexpr std::array<std::string_view, 11> kAllowedNames = {
    "TracyClient.dll", "mcla.exe", "mcla_crash_handler.exe",
    "mcla_diagnostic_image_helper.exe", "msvcp140.dll",
    "msvcp140_atomic_wait.dll", "rexgpu-xenos.dll", "rexruntime.dll",
    "vcruntime140.dll", "vcruntime140_1.dll", "mcla-test-build.ticket"};

struct Handle {
  HANDLE value = INVALID_HANDLE_VALUE;
  Handle() = default;
  explicit Handle(HANDLE h) : value(h) {}
  Handle(Handle &&other) noexcept : value(std::exchange(other.value, INVALID_HANDLE_VALUE)) {}
  Handle &operator=(Handle &&other) noexcept {
    if (this != &other) {
      if (value != INVALID_HANDLE_VALUE) CloseHandle(value);
      value = std::exchange(other.value, INVALID_HANDLE_VALUE);
    }
    return *this;
  }
  ~Handle() { if (value != INVALID_HANDLE_VALUE) CloseHandle(value); }
  Handle(const Handle &) = delete;
  Handle &operator=(const Handle &) = delete;
  explicit operator bool() const { return value != INVALID_HANDLE_VALUE; }
};
struct FindHandle {
  HANDLE value;
  ~FindHandle() { if (value != INVALID_HANDLE_VALUE) FindClose(value); }
};
std::wstring Error(std::wstring_view text, DWORD code = GetLastError()) {
  return std::wstring(text) + L" (Win32 " + std::to_wstring(code) + L")";
}
std::wstring Native(std::wstring_view root, std::wstring_view name) {
  return L"\\\\?\\" + std::wstring(root) + L"\\" + std::wstring(name);
}
bool OrdinalEqual(std::wstring_view a, std::wstring_view b) {
  return a.size() == b.size() && CompareStringOrdinal(a.data(), static_cast<int>(a.size()), b.data(),
      static_cast<int>(b.size()), TRUE) == CSTR_EQUAL;
}

std::expected<uint64_t, std::wstring> FileSize(HANDLE file, uint64_t maximum) {
  BY_HANDLE_FILE_INFORMATION info{};
  if (GetFileType(file) != FILE_TYPE_DISK || !GetFileInformationByHandle(file, &info))
    return std::unexpected(Error(L"cannot inspect build artifact"));
  if (info.dwFileAttributes & (FILE_ATTRIBUTE_REPARSE_POINT | FILE_ATTRIBUTE_DIRECTORY | FILE_ATTRIBUTE_DEVICE))
    return std::unexpected(L"build artifact must be a regular non-reparse file");
  if (info.nNumberOfLinks != 1) return std::unexpected(L"hardlinked build artifacts are not accepted");
  const uint64_t bytes = (static_cast<uint64_t>(info.nFileSizeHigh) << 32) | info.nFileSizeLow;
  if (!bytes || bytes > maximum) return std::unexpected(L"build artifact byte bound violated");
  return bytes;
}
std::expected<Handle, std::wstring> OpenPinned(std::wstring_view root, std::wstring_view name) {
  Handle file(CreateFileW(Native(root, name).c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING,
                          FILE_FLAG_OPEN_REPARSE_POINT | FILE_FLAG_SEQUENTIAL_SCAN, nullptr));
  if (!file) return std::unexpected(Error(L"cannot pin build artifact " + std::wstring(name)));
  return file;
}

struct Sha256 {
  BCRYPT_ALG_HANDLE algorithm = nullptr;
  BCRYPT_HASH_HANDLE hash = nullptr;
  std::vector<uint8_t> object;
  ~Sha256() {
    if (hash) BCryptDestroyHash(hash);
    if (algorithm) BCryptCloseAlgorithmProvider(algorithm, 0);
  }
  bool Start() {
    if (BCryptOpenAlgorithmProvider(&algorithm, BCRYPT_SHA256_ALGORITHM, nullptr, 0) < 0) return false;
    DWORD length = 0, result = 0;
    if (BCryptGetProperty(algorithm, BCRYPT_OBJECT_LENGTH, reinterpret_cast<PUCHAR>(&length), sizeof(length), &result, 0) < 0 ||
        result != sizeof(length) || !length || length > 1024 * 1024) return false;
    object.resize(length);
    return BCryptCreateHash(algorithm, &hash, object.data(), length, nullptr, 0, 0) >= 0;
  }
  bool Append(std::span<const uint8_t> bytes) {
    if (bytes.size() > (std::numeric_limits<ULONG>::max)()) return false;
    return BCryptHashData(hash, const_cast<PUCHAR>(bytes.data()), static_cast<ULONG>(bytes.size()), 0) >= 0;
  }
  std::expected<std::string, std::wstring> Finish() {
    std::array<uint8_t, 32> digest{};
    if (BCryptFinishHash(hash, digest.data(), static_cast<ULONG>(digest.size()), 0) < 0)
      return std::unexpected(L"cannot finalize build SHA256");
    constexpr char hex[] = "0123456789ABCDEF";
    std::string result;
    for (const auto byte : digest) { result += hex[byte >> 4]; result += hex[byte & 15]; }
    return result;
  }
};

std::expected<std::string, std::wstring> HashFile(HANDLE file, uint64_t size) {
  Sha256 hash;
  if (!hash.Start()) return std::unexpected(L"cannot initialize build SHA256");
  std::vector<uint8_t> buffer(1024 * 1024);
  uint64_t remaining = size;
  while (remaining) {
    const auto count = static_cast<DWORD>(std::min<uint64_t>(remaining, buffer.size()));
    DWORD read = 0;
    if (!::ReadFile(file, buffer.data(), count, &read, nullptr) || !read)
      return std::unexpected(Error(L"build artifact truncated while hashing"));
    if (!hash.Append(std::span<const uint8_t>(buffer.data(), read)))
      return std::unexpected(L"cannot hash build artifact chunk");
    remaining -= read;
  }
  uint8_t extra = 0; DWORD read = 0;
  if (!::ReadFile(file, &extra, 1, &read, nullptr) || read)
    return std::unexpected(L"build artifact grew while hashing");
  const auto after = FileSize(file, kMaximumFileBytes);
  if (!after || *after != size) return std::unexpected(L"build artifact changed while hashing");
  return hash.Finish();
}

std::expected<std::vector<VerifiedBuildFile>, std::wstring> Parse(std::string_view text) {
  if (text.size() > kMaximumManifestBytes || !text.starts_with(kHeader) || text.back() != '\n')
    return std::unexpected(L"developer build manifest header/size/final newline is invalid");
  for (const unsigned char c : text) {
    if (c != '\n' && (c < 0x20 || c > 0x7e)) return std::unexpected(L"build manifest must be canonical ASCII with LF newlines");
  }
  text.remove_prefix(kHeader.size());
  std::vector<VerifiedBuildFile> files;
  std::string previous;
  uint64_t total = 0;
  bool has_host = false, has_crash = false;
  while (!text.empty()) {
    const auto end = text.find('\n');
    const auto row = text.substr(0, end);
    text.remove_prefix(end + 1);
    if (files.size() >= kMaximumFiles || row.size() < 69 || row[64] != ' ')
      return std::unexpected(L"build manifest entry count/shape is invalid");
    const auto hash = row.substr(0, 64);
    for (const auto c : hash) {
      if (!((c >= '0' && c <= '9') || (c >= 'A' && c <= 'F')))
        return std::unexpected(L"build manifest SHA256 must be uppercase hex");
    }
    const auto separator = row.find(' ', 65);
    if (separator == row.npos) return std::unexpected(L"build manifest size/name separator missing");
    const auto size_text = row.substr(65, separator - 65);
    if (size_text.empty() || size_text.front() == '0') return std::unexpected(L"build size must be canonical positive decimal");
    uint64_t bytes = 0;
    for (const auto c : size_text) {
      if (c < '0' || c > '9' || bytes > (kMaximumFileBytes - static_cast<unsigned>(c - '0')) / 10)
        return std::unexpected(L"build artifact size is malformed or exceeds 2 GiB");
      bytes = bytes * 10 + static_cast<unsigned>(c - '0');
    }
    const auto name = row.substr(separator + 1);
    if (std::find(kAllowedNames.begin(), kAllowedNames.end(), name) == kAllowedNames.end() || name <= std::string_view(previous))
      return std::unexpected(L"build manifest basename is not allowlisted, sorted or unique");
    if (bytes > kMaximumTotalBytes - total) return std::unexpected(L"build exceeds 4 GiB aggregate bound");
    total += bytes;
    previous = name;
    has_host |= name == "mcla.exe";
    has_crash |= name == "mcla_crash_handler.exe";
    files.push_back({std::wstring(name.begin(), name.end()), bytes, std::string(hash)});
  }
  if (!has_host || !has_crash) return std::unexpected(L"build must include both host and crash handler");
  return files;
}

std::expected<void, std::wstring> VerifyInventory(std::wstring_view root, const std::vector<VerifiedBuildFile> &files) {
  WIN32_FIND_DATAW entry{};
  FindHandle search{FindFirstFileW(Native(root, L"*").c_str(), &entry)};
  if (search.value == INVALID_HANDLE_VALUE) return std::unexpected(Error(L"cannot enumerate developer build"));
  std::vector<std::wstring> seen;
  do {
    const std::wstring_view name(entry.cFileName);
    if (name == L"." || name == L"..") continue;
    if (seen.size() >= kMaximumFiles + 1 || (entry.dwFileAttributes & (FILE_ATTRIBUTE_DIRECTORY | FILE_ATTRIBUTE_REPARSE_POINT)))
      return std::unexpected(L"build inventory contains excessive, nested or redirected entries");
    for (const auto &prior : seen) {
      if (OrdinalEqual(prior, name)) return std::unexpected(L"build inventory contains case-aliased duplicates");
    }
    bool allowed = OrdinalEqual(name, kDeveloperBuildManifest);
    for (const auto &file : files) allowed |= OrdinalEqual(name, file.name);
    if (!allowed) return std::unexpected(L"unexpected build file (including executables, DLLs or configuration): " + std::wstring(name));
    seen.emplace_back(name);
  } while (FindNextFileW(search.value, &entry));
  if (GetLastError() != ERROR_NO_MORE_FILES) return std::unexpected(Error(L"build inventory enumeration failed"));
  if (seen.size() != files.size() + 1) return std::unexpected(L"build inventory is missing a listed artifact");
  return {};
}
} // namespace

struct VerifiedBuild::State {
  fs::DirectoryGuard directory;
  std::vector<Handle> handles;
  std::vector<VerifiedBuildFile> files;
  std::string fingerprint;
  explicit State(fs::DirectoryGuard guard) : directory(std::move(guard)) {}
};
VerifiedBuild::VerifiedBuild(std::unique_ptr<State> state) : state_(std::move(state)) {}
VerifiedBuild::VerifiedBuild(VerifiedBuild &&) noexcept = default;
VerifiedBuild &VerifiedBuild::operator=(VerifiedBuild &&) noexcept = default;
VerifiedBuild::~VerifiedBuild() = default;
const std::wstring &VerifiedBuild::path() const {
  static const std::wstring empty;
  return state_ ? state_->directory.path() : empty;
}
const std::vector<VerifiedBuildFile> &VerifiedBuild::files() const {
  static const std::vector<VerifiedBuildFile> empty;
  return state_ ? state_->files : empty;
}
const std::string &VerifiedBuild::fingerprint() const {
  static const std::string empty;
  return state_ ? state_->fingerprint : empty;
}

std::expected<VerifiedBuild, std::wstring> VerifyBuild(const fs::DirectoryGuard &directory) {
  auto held_directory = fs::OpenDirectory(directory.path());
  if (!held_directory) return std::unexpected(held_directory.error());
  auto state = std::make_unique<VerifiedBuild::State>(std::move(*held_directory));
  auto manifest = OpenPinned(state->directory.path(), kDeveloperBuildManifest);
  if (!manifest) return std::unexpected(manifest.error());
  const auto manifest_size = FileSize(manifest->value, kMaximumManifestBytes);
  if (!manifest_size) return std::unexpected(manifest_size.error());
  std::string text(static_cast<size_t>(*manifest_size), '\0');
  DWORD read = 0;
  if (!::ReadFile(manifest->value, text.data(), static_cast<DWORD>(text.size()), &read, nullptr) || read != text.size())
    return std::unexpected(Error(L"developer build manifest read failed"));
  const auto after = FileSize(manifest->value, kMaximumManifestBytes);
  if (!after || *after != *manifest_size) return std::unexpected(L"developer build manifest size changed");
  auto files = Parse(text);
  if (!files) return std::unexpected(files.error());
  const auto inventory = VerifyInventory(state->directory.path(), *files);
  if (!inventory) return std::unexpected(inventory.error());
  Sha256 hash;
  if (!hash.Start() || !hash.Append({reinterpret_cast<const uint8_t *>(text.data()), text.size()}))
    return std::unexpected(L"cannot hash developer build manifest");
  auto fingerprint = hash.Finish();
  if (!fingerprint) return std::unexpected(fingerprint.error());
  state->fingerprint = std::move(*fingerprint);
  state->files = std::move(*files);
  state->handles.push_back(std::move(*manifest));
  for (const auto &file : state->files) {
    auto held = OpenPinned(state->directory.path(), file.name);
    if (!held) return std::unexpected(held.error());
    const auto size = FileSize(held->value, kMaximumFileBytes);
    if (!size || *size != file.bytes) return std::unexpected(L"build artifact size differs from manifest: " + file.name);
    const auto digest = HashFile(held->value, *size);
    if (!digest) return std::unexpected(digest.error());
    if (*digest != file.sha256) return std::unexpected(L"build artifact hash differs from manifest: " + file.name);
    state->handles.push_back(std::move(*held));
  }
  // Recheck additions/removals after potentially lengthy streaming hashing.
  const auto final_inventory = VerifyInventory(state->directory.path(), state->files);
  if (!final_inventory) return std::unexpected(final_inventory.error());
  return VerifiedBuild(std::move(state));
}

} // namespace mcla::launcher
