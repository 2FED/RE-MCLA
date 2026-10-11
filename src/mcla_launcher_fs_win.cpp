#include "mcla_launcher_fs_win.h"
#include "mcla_launcher_profile_policy.h"

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <bcrypt.h>

#include <algorithm>
#include <array>
#include <utility>

namespace mcla::launcher::fs {
namespace {
constexpr size_t kMaximumIoBytes = 64 * 1024 * 1024;
constexpr std::wstring_view kLockName = L".mcla-writer.lock";
constexpr std::wstring_view kTempPrefix = L".mcla-tmp-";

struct Handle {
  HANDLE value = INVALID_HANDLE_VALUE;
  Handle() = default;
  explicit Handle(HANDLE handle) : value(handle) {}
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

struct Identity {
  DWORD volume;
  DWORD high;
  DWORD low;
  bool operator==(const Identity &) const = default;
};

std::wstring Error(std::wstring_view operation, DWORD code = GetLastError()) {
  return std::wstring(operation) + L" (Win32 " + std::to_wstring(code) + L")";
}
std::wstring NativePath(std::wstring_view path) { return L"\\\\?\\" + std::wstring(path); }
std::u16string Utf16(std::wstring_view path) {
  static_assert(sizeof(wchar_t) == sizeof(char16_t));
  return {path.begin(), path.end()};
}
bool OrdinalEqual(std::wstring_view a, std::wstring_view b) {
  if (a.size() != b.size()) return false;
  return CompareStringOrdinal(a.data(), static_cast<int>(a.size()), b.data(),
                              static_cast<int>(b.size()), TRUE) == CSTR_EQUAL;
}
bool OrdinalAncestor(std::wstring_view a, std::wstring_view b) {
  return a.size() < b.size() && b[a.size()] == L'\\' && OrdinalEqual(a, b.substr(0, a.size()));
}

std::expected<std::wstring, std::wstring> FinalPath(HANDLE handle) {
  const DWORD needed = GetFinalPathNameByHandleW(handle, nullptr, 0, FILE_NAME_NORMALIZED | VOLUME_NAME_DOS);
  if (!needed || needed > 32768) return std::unexpected(Error(L"cannot resolve directory handle path"));
  std::wstring value(needed, L'\0');
  const DWORD written = GetFinalPathNameByHandleW(handle, value.data(), needed, FILE_NAME_NORMALIZED | VOLUME_NAME_DOS);
  if (!written || written >= needed) return std::unexpected(Error(L"directory handle path changed"));
  value.resize(written);
  if (!value.starts_with(L"\\\\?\\") || value.size() < 7 || value[5] != L':' || value[6] != L'\\')
    return std::unexpected(L"canonical directory is not a local drive path");
  value.erase(0, 4);
  if (value.size() > 3 && value.back() == L'\\') value.pop_back();
  return value;
}

std::expected<BY_HANDLE_FILE_INFORMATION, std::wstring> RegularFileInfo(HANDLE handle, size_t maximum) {
  BY_HANDLE_FILE_INFORMATION info{};
  if (GetFileType(handle) != FILE_TYPE_DISK || !GetFileInformationByHandle(handle, &info))
    return std::unexpected(Error(L"cannot inspect regular file"));
  if ((info.dwFileAttributes & (FILE_ATTRIBUTE_REPARSE_POINT | FILE_ATTRIBUTE_DIRECTORY | FILE_ATTRIBUTE_DEVICE)) || info.nNumberOfLinks != 1)
    return std::unexpected(L"file must be regular, non-reparse and single-link");
  const auto size = (static_cast<uint64_t>(info.nFileSizeHigh) << 32) | info.nFileSizeLow;
  if (size > maximum) return std::unexpected(L"file exceeds byte bound");
  return info;
}

bool SimpleName(std::wstring_view name) {
  if (name.empty() || name.size() > 128 || name.find_first_of(L"/\\") != name.npos)
    return false;
  if (OrdinalEqual(name, kLockName) ||
      (name.size() >= kTempPrefix.size() && OrdinalEqual(name.substr(0, kTempPrefix.size()), kTempPrefix)))
    return false;
  return NormalizeRoot(u"C:/" + Utf16(name)).has_value();
}

std::expected<std::wstring, std::wstring> OperationPath(const DirectoryGuard &directory, std::wstring_view name) {
  if (directory.path().empty()) return std::unexpected(L"directory guard is empty");
  if (!SimpleName(name)) return std::unexpected(L"filename must be a safe single component, not an internal lock/temp name");
  return directory.path() + L"\\" + std::wstring(name);
}
} // namespace

struct DirectoryGuard::State {
  std::vector<Handle> handles;
  std::vector<Identity> ancestors;
  std::wstring canonical;
};
struct WriterLock::State { Handle handle; };

DirectoryGuard::DirectoryGuard(std::unique_ptr<State> state) : state_(std::move(state)) {}
DirectoryGuard::DirectoryGuard(DirectoryGuard &&) noexcept = default;
DirectoryGuard &DirectoryGuard::operator=(DirectoryGuard &&) noexcept = default;
DirectoryGuard::~DirectoryGuard() = default;
const std::wstring &DirectoryGuard::path() const {
  static const std::wstring empty;
  return state_ ? state_->canonical : empty;
}
WriterLock::WriterLock(std::unique_ptr<State> state) : state_(std::move(state)) {}
WriterLock::WriterLock(WriterLock &&) noexcept = default;
WriterLock &WriterLock::operator=(WriterLock &&) noexcept = default;
WriterLock::~WriterLock() = default;

std::expected<DirectoryGuard, std::wstring> OpenDirectory(std::wstring_view input) {
  const auto normalized = NormalizeRoot(Utf16(input));
  if (!normalized) return std::unexpected(L"directory path violates absolute local-drive lexical policy");
  // Validate lexically, but never use the ASCII-folded comparison key to open
  // a filesystem object: NTFS directories may be configured case-sensitive.
  std::wstring path(input);
  std::replace(path.begin(), path.end(), L'/', L'\\');
  if (GetDriveTypeW(path.substr(0, 3).c_str()) != DRIVE_FIXED)
    return std::unexpected(L"directory must be on a local fixed drive");
  auto state = std::make_unique<DirectoryGuard::State>();
  const auto hold_chain = [&](const std::wstring &chain) -> std::expected<void, std::wstring> {
  size_t length = 3;
  for (;;) {
    const auto prefix = chain.substr(0, length);
    Handle handle(CreateFileW(NativePath(prefix).c_str(), FILE_READ_ATTRIBUTES,
        FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING,
        FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OPEN_REPARSE_POINT, nullptr));
    if (!handle) return std::unexpected(Error(L"cannot hold directory ancestor"));
    BY_HANDLE_FILE_INFORMATION info{};
    if (GetFileType(handle.value) != FILE_TYPE_DISK || !GetFileInformationByHandle(handle.value, &info))
      return std::unexpected(Error(L"cannot inspect directory ancestor"));
    if (!(info.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) ||
        (info.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT))
      return std::unexpected(L"directory ancestor is not a normal non-reparse directory");
    state->ancestors.push_back({info.dwVolumeSerialNumber, info.nFileIndexHigh, info.nFileIndexLow});
    if (length == chain.size()) {
      auto canonical = FinalPath(handle.value);
      if (!canonical) return std::unexpected(canonical.error());
      if (!NormalizeRoot(Utf16(*canonical)) || GetDriveTypeW(canonical->substr(0, 3).c_str()) != DRIVE_FIXED)
        return std::unexpected(L"canonical directory violates local-drive path policy");
      state->canonical = std::move(*canonical);
    }
    state->handles.push_back(std::move(handle));
    if (length == chain.size()) break;
    const auto next = chain.find(L'\\', length == 3 ? 3 : length + 1);
    length = next == std::wstring::npos ? chain.size() : next;
  }
  return {};
  };
  const auto initial = hold_chain(path);
  if (!initial) return std::unexpected(initial.error());
  if (!OrdinalEqual(state->canonical, path)) {
    // Short names / local drive aliases can reveal ancestors that the supplied
    // spelling did not include. Hold that complete canonical chain as well.
    const auto canonical = state->canonical;
    const auto original_identity = state->ancestors.back();
    const auto resolved = hold_chain(canonical);
    if (!resolved) return std::unexpected(resolved.error());
    if (!OrdinalEqual(state->canonical, canonical) || state->ancestors.back() != original_identity)
      return std::unexpected(L"canonical directory identity changed during validation");
  }
  return DirectoryGuard(std::move(state));
}

std::expected<void, std::wstring> CompareDisjoint(const DirectoryGuard &a, const DirectoryGuard &b) {
  if (!a.state_ || !b.state_) return std::unexpected(L"cannot compare empty directory guards");
  const auto root_a = a.state_->ancestors.back();
  const auto root_b = b.state_->ancestors.back();
  for (const auto identity : a.state_->ancestors) {
    if (identity == root_b) return std::unexpected(L"directory identities overlap");
  }
  for (const auto identity : b.state_->ancestors) {
    if (identity == root_a) return std::unexpected(L"directory identities overlap");
  }
  if (OrdinalEqual(a.path(), b.path()) || OrdinalAncestor(a.path(), b.path()) || OrdinalAncestor(b.path(), a.path()))
    return std::unexpected(L"canonical directory paths overlap");
  return {};
}

std::expected<WriterLock, std::wstring> AcquireWriterLock(const DirectoryGuard &directory) {
  if (directory.path().empty()) return std::unexpected(L"directory guard is empty");
  const auto path = directory.path() + L"\\" + std::wstring(kLockName);
  Handle handle(CreateFileW(NativePath(path).c_str(), GENERIC_READ | GENERIC_WRITE, 0, nullptr,
      OPEN_ALWAYS, FILE_FLAG_OPEN_REPARSE_POINT | FILE_FLAG_WRITE_THROUGH, nullptr));
  if (!handle) return std::unexpected(Error(L"profile writer lock unavailable"));
  const auto info = RegularFileInfo(handle.value, 0);
  if (!info) return std::unexpected(info.error());
  auto state = std::make_unique<WriterLock::State>();
  state->handle = std::move(handle);
  return WriterLock(std::move(state));
}

std::expected<std::vector<uint8_t>, std::wstring> ReadFile(
    const DirectoryGuard &directory, std::wstring_view name, size_t maximum_bytes) {
  if (maximum_bytes > kMaximumIoBytes) return std::unexpected(L"requested read limit exceeds IO policy");
  const auto path = OperationPath(directory, name);
  if (!path) return std::unexpected(path.error());
  Handle handle(CreateFileW(NativePath(*path).c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr,
      OPEN_EXISTING, FILE_FLAG_OPEN_REPARSE_POINT | FILE_FLAG_SEQUENTIAL_SCAN, nullptr));
  if (!handle) return std::unexpected(Error(L"cannot open bounded file"));
  const auto info = RegularFileInfo(handle.value, maximum_bytes);
  if (!info) return std::unexpected(info.error());
  std::vector<uint8_t> bytes(info->nFileSizeLow);
  size_t offset = 0;
  while (offset < bytes.size()) {
    DWORD read = 0;
    const DWORD count = static_cast<DWORD>(std::min<size_t>(bytes.size() - offset, 1024 * 1024));
    if (!::ReadFile(handle.value, bytes.data() + offset, count, &read, nullptr) || !read)
      return std::unexpected(Error(L"bounded file read was truncated"));
    offset += read;
  }
  uint8_t extra = 0;
  DWORD read = 0;
  if (!::ReadFile(handle.value, &extra, 1, &read, nullptr) || read != 0)
    return std::unexpected(L"bounded file size changed during read");
  const auto after = RegularFileInfo(handle.value, maximum_bytes);
  if (!after || after->nFileSizeHigh != info->nFileSizeHigh || after->nFileSizeLow != info->nFileSizeLow)
    return std::unexpected(L"bounded file changed during read");
  return bytes;
}

std::expected<void, std::wstring> AtomicWrite(
    const DirectoryGuard &directory, std::wstring_view name, std::span<const uint8_t> bytes) {
  if (bytes.size() > kMaximumIoBytes) return std::unexpected(L"write exceeds IO policy");
  const auto target = OperationPath(directory, name);
  if (!target) return std::unexpected(target.error());
  // Hold the previous object against concurrent content modification. DELETE
  // sharing allows replacement of its name, never writing through a hardlink.
  Handle existing(CreateFileW(NativePath(*target).c_str(), FILE_READ_ATTRIBUTES,
      FILE_SHARE_READ | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING,
      FILE_FLAG_OPEN_REPARSE_POINT | FILE_FLAG_BACKUP_SEMANTICS, nullptr));
  if (existing) {
    const auto info = RegularFileInfo(existing.value, kMaximumIoBytes);
    if (!info) return std::unexpected(info.error());
  } else if (GetLastError() != ERROR_FILE_NOT_FOUND) {
    return std::unexpected(Error(L"cannot inspect atomic replacement target"));
  }
  std::array<uint8_t, 16> random{};
  if (BCryptGenRandom(nullptr, random.data(), static_cast<ULONG>(random.size()), BCRYPT_USE_SYSTEM_PREFERRED_RNG) < 0)
    return std::unexpected(L"cannot generate private temporary-file identity");
  constexpr wchar_t hex[] = L"0123456789abcdef";
  std::wstring temporary = directory.path() + L"\\" + std::wstring(kTempPrefix);
  for (const auto byte : random) { temporary += hex[byte >> 4]; temporary += hex[byte & 15]; }
  temporary += L".partial";
  Handle output(CreateFileW(NativePath(temporary).c_str(), GENERIC_WRITE | FILE_READ_ATTRIBUTES, 0,
      nullptr, CREATE_NEW, FILE_FLAG_OPEN_REPARSE_POINT | FILE_FLAG_WRITE_THROUGH, nullptr));
  if (!output) return std::unexpected(Error(L"cannot create exclusive atomic temporary file"));
  const auto preserve_error = [&](std::wstring message) {
    return std::unexpected(std::move(message) + L"; preserved temporary file: " + temporary);
  };
  const auto info = RegularFileInfo(output.value, 0);
  if (!info) return preserve_error(info.error());
  size_t offset = 0;
  while (offset < bytes.size()) {
    DWORD written = 0;
    const DWORD count = static_cast<DWORD>(std::min<size_t>(bytes.size() - offset, 1024 * 1024));
    if (!::WriteFile(output.value, bytes.data() + offset, count, &written, nullptr) || !written)
      return preserve_error(Error(L"atomic temporary write failed"));
    offset += written;
  }
  if (!FlushFileBuffers(output.value)) return preserve_error(Error(L"atomic temporary flush failed"));
  output = Handle{};
  // MoveFileEx may request access incompatible with our target-validation
  // handle. Release it before replacement; the caller's profile WriterLock
  // coordinates writers and the directory ancestor handles remain held.
  existing = Handle{};
  if (!MoveFileExW(NativePath(temporary).c_str(), NativePath(*target).c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
    return preserve_error(Error(L"atomic publication failed"));
  return {};
}

} // namespace mcla::launcher::fs
