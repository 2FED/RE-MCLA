#include "mcla_launcher_store_win.h"
#include "mcla_launcher_writable_root_win.h"

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <bcrypt.h>

#include <array>
#include <optional>
#include <utility>

namespace mcla::launcher {
namespace {
constexpr std::wstring_view kPreferencesName = L"preferences.dat";
constexpr std::wstring_view kBackupName = L"preferences.previous.dat";
constexpr std::wstring_view kLockName = L".mcla-writer.lock";
constexpr std::string_view kManagedPrefix = "MCLA managed profile v1\n";

std::wstring Native(std::wstring_view path) { return L"\\\\?\\" + std::wstring(path); }
std::wstring Wide(std::u16string_view value) { return {value.begin(), value.end()}; }
std::u16string Utf16(std::wstring_view value) { return {value.begin(), value.end()}; }
std::wstring Error(std::wstring_view text, DWORD code = GetLastError()) {
  return std::wstring(text) + L" (Win32 " + std::to_wstring(code) + L")";
}
bool Equal(std::wstring_view a, std::wstring_view b) {
  return a.size() == b.size() && CompareStringOrdinal(a.data(), static_cast<int>(a.size()),
      b.data(), static_cast<int>(b.size()), TRUE) == CSTR_EQUAL;
}
bool Ancestor(std::wstring_view a, std::wstring_view b) {
  return a.size() < b.size() && b[a.size()] == L'\\' && Equal(a, b.substr(0, a.size()));
}
bool Overlap(std::wstring_view a, std::wstring_view b) {
  return Equal(a, b) || Ancestor(a, b) || Ancestor(b, a);
}

std::expected<bool, std::wstring> MissingPreferences(const fs::DirectoryGuard &directory) {
  const auto path = Native(directory.path() + L"\\" + std::wstring(kPreferencesName));
  if (GetFileAttributesW(path.c_str()) != INVALID_FILE_ATTRIBUTES) return false;
  const auto error = GetLastError();
  if (error == ERROR_FILE_NOT_FOUND) return true;
  return std::unexpected(Error(L"cannot inspect preferences file", error));
}

std::expected<void, std::wstring> EmptyFirstRunDirectory(const fs::DirectoryGuard &directory) {
  WIN32_FIND_DATAW entry{};
  HANDLE search = FindFirstFileW(Native(directory.path() + L"\\*").c_str(), &entry);
  if (search == INVALID_HANDLE_VALUE) {
    const DWORD error = GetLastError();
    if (error == ERROR_FILE_NOT_FOUND) return {};
    return std::unexpected(Error(L"cannot inspect first-run store", error));
  }
  bool valid = true;
  DWORD enumeration_error = ERROR_NO_MORE_FILES;
  do {
    const std::wstring_view name(entry.cFileName);
    if (name == L"." || name == L".." || Equal(name, kLockName)) continue;
    valid = false;
    break;
  } while (FindNextFileW(search, &entry));
  if (valid) enumeration_error = GetLastError();
  FindClose(search);
  if (!valid) return std::unexpected(L"missing preferences in nonempty store; preserve it and recover explicitly");
  if (enumeration_error != ERROR_NO_MORE_FILES)
    return std::unexpected(Error(L"first-run enumeration failed", enumeration_error));
  return {};
}

std::expected<std::vector<fs::DirectoryGuard>, std::wstring> ExistingRoots(const Preferences &preferences) {
  if (!EncodePreferences(preferences)) return std::unexpected(L"preferences model is invalid");
  std::vector<fs::DirectoryGuard> roots;
  for (const auto &entry : preferences.library.entries) {
    auto root = fs::OpenDirectory(Wide(entry.root));
    if (!root) return std::unexpected(L"registered profile is unavailable: " + root.error());
    roots.push_back(std::move(*root));
  }
  for (const auto &path : {preferences.build_root, preferences.game_root}) {
    if (path.empty()) continue;
    auto root = fs::OpenDirectory(Wide(path));
    if (!root) return std::unexpected(L"configured build/game root is unavailable: " + root.error());
    roots.push_back(std::move(*root));
  }
  return roots;
}

std::expected<std::string, std::wstring> RandomId() {
  std::array<uint8_t, 16> bytes{};
  if (BCryptGenRandom(nullptr, bytes.data(), static_cast<ULONG>(bytes.size()),
                      BCRYPT_USE_SYSTEM_PREFERRED_RNG) < 0)
    return std::unexpected(L"cannot generate profile identity");
  constexpr char hex[] = "0123456789abcdef";
  std::string id;
  for (const auto value : bytes) { id += hex[value >> 4]; id += hex[value & 15]; }
  if (!ValidOpaqueId(id)) return std::unexpected(L"generated profile identity was invalid");
  return id;
}

// Only used inside a freshly created, guarded child. CREATE_NEW never replaces
// an existing file, and interrupted output remains for explicit investigation.
std::expected<void, std::wstring> WriteNew(
    const fs::DirectoryGuard &directory, std::wstring_view name, std::string_view contents) {
  const auto path = Native(directory.path() + L"\\" + std::wstring(name));
  HANDLE file = CreateFileW(path.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_NEW,
      FILE_FLAG_OPEN_REPARSE_POINT | FILE_FLAG_WRITE_THROUGH, nullptr);
  if (file == INVALID_HANDLE_VALUE) return std::unexpected(Error(L"cannot create new profile marker"));
  DWORD written = 0;
  const bool wrote = WriteFile(file, contents.data(), static_cast<DWORD>(contents.size()), &written, nullptr) != FALSE;
  DWORD error = wrote ? ERROR_SUCCESS : GetLastError();
  const bool complete = wrote && written == contents.size();
  if (!complete && error == ERROR_SUCCESS) error = ERROR_WRITE_FAULT;
  const bool flushed = complete && FlushFileBuffers(file);
  if (complete && !flushed) error = GetLastError();
  CloseHandle(file);
  if (!flushed) return std::unexpected(Error(L"profile marker write/flush failed; incomplete child preserved", error));
  return {};
}
} // namespace

struct PreferenceStore::State {
  fs::DirectoryGuard directory;
  fs::WriterLock lock;
  Preferences preferences;
  std::optional<std::vector<uint8_t>> previous_bytes;
};
PreferenceStore::PreferenceStore(std::unique_ptr<State> state) : state_(std::move(state)) {}
PreferenceStore::PreferenceStore(PreferenceStore &&) noexcept = default;
PreferenceStore &PreferenceStore::operator=(PreferenceStore &&) noexcept = default;
PreferenceStore::~PreferenceStore() = default;
const Preferences &PreferenceStore::preferences() const { return state_->preferences; }
const fs::DirectoryGuard &PreferenceStore::directory() const { return state_->directory; }

std::expected<PreferenceStore, std::wstring> OpenPreferenceStore(std::wstring_view existing_directory) {
  auto directory = fs::OpenDirectory(existing_directory);
  if (!directory) return std::unexpected(directory.error());
  auto writable = ValidateWritableRoot(*directory);
  if (!writable) return std::unexpected(writable.error());
  // Do not leave even a lock file in an accidentally chosen non-store folder.
  // This preflight is read-only; repeat the authoritative read under the lock.
  auto preflight_missing = MissingPreferences(*directory);
  if (!preflight_missing) return std::unexpected(preflight_missing.error());
  if (*preflight_missing) {
    auto empty = EmptyFirstRunDirectory(*directory);
    if (!empty) return std::unexpected(empty.error());
  } else {
    auto bytes = fs::ReadFile(*directory, kPreferencesName, kMaximumPreferencesBytes);
    if (!bytes || !DecodePreferences(*bytes))
      return std::unexpected(L"invalid or unreadable preference store; no lock or fallback created");
  }
  auto lock = fs::AcquireWriterLock(*directory);
  if (!lock) return std::unexpected(lock.error());
  auto missing = MissingPreferences(*directory);
  if (!missing) return std::unexpected(missing.error());
  Preferences preferences;
  std::optional<std::vector<uint8_t>> previous;
  if (*missing) {
    auto empty = EmptyFirstRunDirectory(*directory);
    if (!empty) return std::unexpected(empty.error());
  } else {
    auto bytes = fs::ReadFile(*directory, kPreferencesName, kMaximumPreferencesBytes);
    if (!bytes) return std::unexpected(bytes.error());
    auto decoded = DecodePreferences(*bytes);
    if (!decoded) return std::unexpected(L"preferences file is invalid; no fallback or overwrite attempted");
    preferences = std::move(*decoded);
    previous = std::move(*bytes);
  }
  return PreferenceStore(std::make_unique<PreferenceStore::State>(
      std::move(*directory), std::move(*lock), std::move(preferences), std::move(previous)));
}

std::expected<void, std::wstring> PreferenceStore::Save(const Preferences &preferences) {
  if (!state_) return std::unexpected(L"preference store was moved from");
  auto encoded = EncodePreferences(preferences);
  if (!encoded) return std::unexpected(L"refusing invalid preferences model");
  Preferences accepted = preferences; // Allocate before publishing any bytes.
  if (state_->previous_bytes) {
    auto current = fs::ReadFile(state_->directory, kPreferencesName, kMaximumPreferencesBytes);
    if (!current) return std::unexpected(current.error());
    if (*current != *state_->previous_bytes || !DecodePreferences(*current))
      return std::unexpected(L"preferences changed or became invalid; no overwrite attempted");
    auto backup = fs::AtomicWrite(state_->directory, kBackupName, *current);
    if (!backup) return std::unexpected(backup.error());
  } else {
    auto empty = EmptyFirstRunDirectory(state_->directory);
    if (!empty) return std::unexpected(empty.error());
  }
  auto published = fs::AtomicWrite(state_->directory, kPreferencesName, *encoded);
  if (!published) return std::unexpected(published.error());
  state_->preferences = std::move(accepted);
  state_->previous_bytes = std::move(*encoded);
  return {};
}

std::expected<Preferences, std::wstring> RegisterManagedProfile(
    const Preferences &preferences, std::wstring_view root,
    std::u16string_view label, const fs::DirectoryGuard &local_store) {
  if (!ValidLabel(label)) return std::unexpected(L"invalid profile library label");
  auto candidate = fs::OpenDirectory(root);
  if (!candidate) return std::unexpected(candidate.error());
  auto writable = ValidateWritableRoot(*candidate);
  if (!writable) return std::unexpected(writable.error());
  const auto import_path = Native(candidate->path() + L"\\import-in-progress.dat");
  if (GetFileAttributesW(import_path.c_str()) != INVALID_FILE_ATTRIBUTES)
    return std::unexpected(L"profile import is incomplete; registration refused");
  if (GetLastError() != ERROR_FILE_NOT_FOUND)
    return std::unexpected(Error(L"cannot inspect profile import status"));
  auto separate = fs::CompareDisjoint(*candidate, local_store);
  if (!separate) return std::unexpected(L"profile overlaps local preferences store");
  auto roots = ExistingRoots(preferences);
  if (!roots) return std::unexpected(roots.error());
  for (const auto &existing : *roots) {
    if (!fs::CompareDisjoint(*candidate, existing))
      return std::unexpected(L"profile overlaps an existing profile/build/game directory identity");
  }
  auto marker = fs::ReadFile(*candidate, kManagedProfileMarker, 128);
  if (!marker) return std::unexpected(L"not a completed managed profile: " + marker.error());
  const std::string contents(marker->begin(), marker->end());
  if (!contents.starts_with(kManagedPrefix) || contents.size() != kManagedPrefix.size() + 33 || contents.back() != '\n')
    return std::unexpected(L"unknown or incomplete managed profile marker");
  const auto id = contents.substr(kManagedPrefix.size(), 32);
  if (!ValidOpaqueId(id)) return std::unexpected(L"invalid managed profile identity");
  auto result = preferences;
  result.library.entries.push_back({id, std::u16string(label), Utf16(candidate->path()), Layout::ManagedExternalV1});
  if (!EncodePreferences(result)) return std::unexpected(L"managed registration would create an invalid or duplicate library");
  return result;
}

namespace {
std::expected<ProfileEntry, std::wstring> CreateProfileImpl(
    const Preferences &preferences, std::wstring_view parent,
    std::u16string_view label, const fs::DirectoryGuard &local_store, bool import_pending) {
  if (!ValidLabel(label)) return std::unexpected(L"invalid profile library label");
  if (preferences.library.entries.size() >= kMaximumProfiles)
    return std::unexpected(L"profile library capacity reached");
  auto container = fs::OpenDirectory(parent);
  if (!container) return std::unexpected(container.error());
  auto writable = ValidateWritableRoot(*container);
  if (!writable) return std::unexpected(writable.error());
  auto roots = ExistingRoots(preferences);
  if (!roots) return std::unexpected(roots.error());
  auto id = RandomId();
  if (!id) return std::unexpected(id.error());
  for (const auto &entry : preferences.library.entries)
    if (entry.id == *id) return std::unexpected(L"generated profile identity collision; no write attempted");
  const auto child_path = container->path() + L"\\MCLA-" + std::wstring(id->begin(), id->end());
  if (!NormalizeRoot(Utf16(child_path))) return std::unexpected(L"new profile path exceeds path policy");
  const auto safe_child = [&](const fs::DirectoryGuard &protected_root) {
    if (Overlap(child_path, protected_root.path())) return false;
    // A parent may contain existing sibling profiles. Other physical overlap
    // (including an alias of/inside a protected root) must fail closed.
    if (!fs::CompareDisjoint(*container, protected_root) &&
        !Ancestor(container->path(), protected_root.path())) return false;
    return true;
  };
  if (!safe_child(local_store)) return std::unexpected(L"new profile would overlap local preferences store");
  for (const auto &existing : *roots)
    if (!safe_child(existing)) return std::unexpected(L"new profile would overlap a profile/build/game root");
  if (!CreateDirectoryW(Native(child_path).c_str(), nullptr))
    return std::unexpected(Error(L"cannot create exclusive new profile child"));
  const auto preserve = [&](std::wstring error) {
    return std::unexpected(std::move(error) + L"; incomplete child preserved: " + child_path);
  };
  auto child = fs::OpenDirectory(child_path);
  if (!child) return preserve(child.error());
  auto lock = fs::AcquireWriterLock(*child);
  if (!lock) return preserve(lock.error());
  auto started = WriteNew(*child, kProfileInitializationMarker, "MCLA profile initialization started v1\n" + *id + "\n");
  if (!started) return preserve(started.error());
  for (const wchar_t *name : {L"user", L"cache", L"logs", L"diagnostics", L"sessions", L"backups", L"metadata"}) {
    if (!CreateDirectoryW(Native(child_path + L"\\" + name).c_str(), nullptr))
      return preserve(Error(L"cannot create managed profile directory"));
  }
  auto metadata = fs::OpenDirectory(child_path + L"\\metadata");
  if (!metadata) return preserve(metadata.error());
  if (!CreateDirectoryW(Native(metadata->path() + L"\\update").c_str(), nullptr))
    return preserve(Error(L"cannot create profile update metadata directory"));
  constexpr std::wstring_view pending_marker = L"managed-profile.pending.dat";
  auto completed = WriteNew(*child, pending_marker, std::string(kManagedPrefix) + *id + "\n");
  if (!completed) return preserve(completed.error());
  // No replace flag: the final completion name must not already exist. A
  // failed write/flush leaves only the pending record, never a valid marker.
  const std::wstring final_name(import_pending ? std::wstring_view(L"import-in-progress.dat") : kManagedProfileMarker);
  if (!MoveFileExW(Native(child_path + L"\\" + std::wstring(pending_marker)).c_str(),
                   Native(child_path + L"\\" + final_name).c_str(),
                   MOVEFILE_WRITE_THROUGH))
    return preserve(Error(L"cannot publish profile creation marker"));
  return ProfileEntry{*id, std::u16string(label), Utf16(child->path()), Layout::ManagedExternalV1};
}
} // namespace

std::expected<ProfileEntry, std::wstring> CreateManagedProfile(
    const Preferences &preferences, std::wstring_view parent,
    std::u16string_view label, const fs::DirectoryGuard &local_store) {
  return CreateProfileImpl(preferences, parent, label, local_store, false);
}

std::expected<ProfileEntry, std::wstring> CreateImportProfile(
    const Preferences &preferences, std::wstring_view parent,
    std::u16string_view label, const fs::DirectoryGuard &local_store) {
  return CreateProfileImpl(preferences, parent, label, local_store, true);
}

} // namespace mcla::launcher
