#include "mcla_launcher_store_win.h"
#include "mcla_launcher_writable_root_win.h"

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <bcrypt.h>
#include <shlobj.h>

#include <array>
#include <iostream>
#include <optional>
#include <stdexcept>

namespace {
using namespace mcla::launcher;
unsigned checks = 0, failures = 0, skips = 0;
void Check(bool value, const char *label) {
  ++checks;
  if (!value) { ++failures; std::cerr << "FAIL " << label << '\n'; }
}
std::wstring Wide(std::u16string_view value) { return {value.begin(), value.end()}; }
std::u16string Utf16(std::wstring_view value) { return {value.begin(), value.end()}; }

std::wstring NewFixture() {
  wchar_t temporary[32768]{};
  const DWORD length = GetTempPathW(32768, temporary);
  if (!length || length >= 32768) throw std::runtime_error("temporary directory unavailable");
  std::array<uint8_t, 16> random{};
  if (BCryptGenRandom(nullptr, random.data(), static_cast<ULONG>(random.size()), BCRYPT_USE_SYSTEM_PREFERRED_RNG) < 0)
    throw std::runtime_error("fixture random identity unavailable");
  std::wstring root(temporary, length);
  root += L"MclaStoreTest-";
  constexpr wchar_t hex[] = L"0123456789abcdef";
  for (auto value : random) { root += hex[value >> 4]; root += hex[value & 15]; }
  if (!CreateDirectoryW(root.c_str(), nullptr)) throw std::runtime_error("cannot create exclusive fixture");
  return root;
}

std::wstring Directory(const std::wstring &root, const wchar_t *name) {
  const auto result = root + L"\\" + name;
  if (!CreateDirectoryW(result.c_str(), nullptr)) throw std::runtime_error("cannot create fixture subdirectory");
  return result;
}

// Fixture-owned only. Existing writes intentionally simulate independent file
// corruption; production storage never calls this helper or follows a fallback.
void FixtureWrite(const std::wstring &path, std::span<const uint8_t> bytes, bool replace = false) {
  HANDLE file = CreateFileW(path.c_str(), GENERIC_WRITE, 0, nullptr,
      replace ? TRUNCATE_EXISTING : CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr);
  if (file == INVALID_HANDLE_VALUE) throw std::runtime_error("cannot create/change fixture file");
  DWORD written = 0;
  const bool ok = WriteFile(file, bytes.data(), static_cast<DWORD>(bytes.size()), &written, nullptr) && written == bytes.size();
  CloseHandle(file);
  if (!ok) throw std::runtime_error("fixture write failed");
}
void FixtureText(const std::wstring &path, std::string_view text, bool replace = false) {
  FixtureWrite(path, std::span(reinterpret_cast<const uint8_t *>(text.data()), text.size()), replace);
}

std::vector<uint8_t> Read(const fs::DirectoryGuard &directory, std::wstring_view name) {
  const auto bytes = fs::ReadFile(directory, name, kMaximumPreferencesBytes);
  if (!bytes) throw std::runtime_error("fixture bounded read failed");
  return *bytes;
}
size_t ChildCount(const std::wstring &root) {
  WIN32_FIND_DATAW data{};
  HANDLE search = FindFirstFileW((root + L"\\*").c_str(), &data);
  if (search == INVALID_HANDLE_VALUE) throw std::runtime_error("fixture enumeration failed");
  size_t count = 0;
  do {
    if (std::wstring_view(data.cFileName) != L"." && std::wstring_view(data.cFileName) != L"..") ++count;
  } while (FindNextFileW(search, &data));
  FindClose(search);
  return count;
}

void TestStore(const std::wstring &root) {
  const auto store_path = Directory(root, L"store");
  Preferences first;
  first.gpu_name = u"Synthetic GPU";
  first.render_target = 1440;
  first.fullscreen = true;
  {
    auto store = OpenPreferenceStore(store_path);
    Check(store.has_value(), "empty directory opens first run");
    if (!store) return;
    Check(store->preferences() == Preferences{}, "first run is empty preferences without fallback");
    Check(!OpenPreferenceStore(store_path), "concurrent store writer rejected");
    Check(store->Save(first).has_value(), "first atomic preferences save");
    Check(store->preferences() == first, "successful save updates memory");
    const auto first_decoded = DecodePreferences(Read(store->directory(), L"preferences.dat"));
    Check(first_decoded && *first_decoded == first, "first persisted preferences decode exactly");
    auto changed = first;
    changed.vsync = false;
    changed.render_target = 720;
    Check(store->Save(changed).has_value(), "second atomic save");
    const auto backup = DecodePreferences(Read(store->directory(), L"preferences.previous.dat"));
    Check(backup && *backup == first, "previous-valid backup precedes second publication");
    auto invalid = changed;
    invalid.render_target = 1081;
    const auto before = Read(store->directory(), L"preferences.dat");
    Check(!store->Save(invalid), "invalid save rejected");
    Check(store->preferences() == changed && Read(store->directory(), L"preferences.dat") == before,
          "invalid save changes neither memory nor primary bytes");
  }
  {
    auto store = OpenPreferenceStore(store_path);
    Check(store && !store->preferences().vsync && store->preferences().render_target == 720,
          "store reopens exact last choices");
    if (!store) return;
    const auto accepted = store->preferences();
    const auto primary_before = Read(store->directory(), L"preferences.dat");
    // Deny rename/delete only on this fixture's current primary. Reads and
    // backup remain possible; final atomic publication must fail safely.
    HANDLE deny_replace = CreateFileW((store_path + L"\\preferences.dat").c_str(),
        GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    Check(deny_replace != INVALID_HANDLE_VALUE, "fixture holds replacement-denying reader");
    if (deny_replace != INVALID_HANDLE_VALUE) {
      Check(!store->Save(first), "atomic publication failure reported");
      CloseHandle(deny_replace);
      Check(store->preferences() == accepted && Read(store->directory(), L"preferences.dat") == primary_before,
            "publication failure preserves prior memory and primary bytes");
      const auto saved_previous = DecodePreferences(Read(store->directory(), L"preferences.previous.dat"));
      Check(saved_previous && *saved_previous == accepted, "failed publication still retains previous-valid backup");
    }
    const auto backup = Read(store->directory(), L"preferences.previous.dat");
    FixtureText(store_path + L"\\preferences.dat", "corrupt", true);
    Check(!store->Save(first), "live corruption detected before backup/publication");
    Check(store->preferences() == accepted, "failed corrupted save preserves accepted memory");
    Check(Read(store->directory(), L"preferences.dat") == std::vector<uint8_t>({'c','o','r','r','u','p','t'}),
          "corrupted primary not overwritten");
    Check(Read(store->directory(), L"preferences.previous.dat") == backup, "previous-valid backup untouched on corruption");
  }
  Check(!OpenPreferenceStore(store_path), "corrupt primary never opens backup automatically");
  const auto nonempty = Directory(root, L"missing-primary");
  FixtureText(nonempty + L"\\preferences.previous.dat", "retained backup");
  Check(!OpenPreferenceStore(nonempty), "missing primary with backup is not first-run reset");
  Check(GetFileAttributesW((nonempty + L"\\.mcla-writer.lock").c_str()) == INVALID_FILE_ATTRIBUTES,
        "wrong nonempty store rejected without lock-file mutation");
  Check(!OpenPreferenceStore(root + L"\\does-not-exist"), "store never creates arbitrary root");
}

void TestRenderTargetMigration(const std::wstring &root) {
  const auto store_path = Directory(root, L"render-target-migration");
  Preferences legacy;
  legacy.gpu_name = u"Migration fixture GPU";
  legacy.render_target = 1440;
  legacy.fullscreen = true;
  const auto legacy_bytes = EncodePreferences(legacy);
  Check(legacy_bytes && (*legacy_bytes)[7] == '1', "1440p remains legacy V1 scale2 bytes");
  if (!legacy_bytes) return;
  FixtureWrite(store_path + L"\\preferences.dat", *legacy_bytes);
  // Preexisting persistent lock makes read-only opening observable without
  // confusing a newly created empty coordination file with migration writes.
  FixtureText(store_path + L"\\.mcla-writer.lock", "");
  const auto original_count = ChildCount(store_path);
  {
    auto store = OpenPreferenceStore(store_path);
    Check(store && store->preferences() == legacy, "V1 scale2 opens as 1440p without changing other settings");
    if (!store) return;
    Check(Read(store->directory(), L"preferences.dat") == *legacy_bytes && ChildCount(store_path) == original_count,
          "V1 load does not rewrite bytes or create a migration backup");
    auto next = legacy;
    next.render_target = 1080;
    Check(store->Save(next).has_value(), "explicit 1080p save upgrades preferences to V2");
    const auto upgraded = Read(store->directory(), L"preferences.dat");
    Check(upgraded[7] == '2', "1080p is never published as a misleading V1 scale");
    Check(Read(store->directory(), L"preferences.previous.dat") == *legacy_bytes,
          "V2 publication retains exact previous V1 bytes");
    Check(store->preferences() == next, "explicit upgrade preserves all non-target settings");
  }
  {
    auto store = OpenPreferenceStore(store_path);
    auto expected = legacy; expected.render_target = 1080;
    Check(store && store->preferences() == expected, "V2 1080p reopens exactly");
    if (!store) return;
    const auto before = Read(store->directory(), L"preferences.dat");
    const auto backup = Read(store->directory(), L"preferences.previous.dat");
    auto invalid = expected; invalid.render_target = 1081;
    Check(!store->Save(invalid), "unknown explicit target is rejected before save");
    Check(store->preferences() == expected && Read(store->directory(), L"preferences.dat") == before &&
              Read(store->directory(), L"preferences.previous.dat") == backup,
          "invalid target preserves memory, V2 primary and V1 backup");
    expected.render_target = 2160;
    Check(store->Save(expected).has_value(), "2160p publishes a supported V2 target");
    const auto second = Read(store->directory(), L"preferences.dat");
    const auto decoded = DecodePreferences(second);
    Check(second[7] == '2' && decoded && *decoded == expected,
          "2160p persisted bytes retain the exact explicit target");
    Check(Read(store->directory(), L"preferences.previous.dat") == before,
          "2160p save retains exact previous 1080p V2 backup");
    Check(store->Save(legacy).has_value(), "return to an old target explicitly writes old-compatible V1");
    Check(Read(store->directory(), L"preferences.dat") == *legacy_bytes &&
              Read(store->directory(), L"preferences.previous.dat") == second,
          "return to 1440p restores exact V1 encoding while preserving V2 backup");
  }
  {
    auto store = OpenPreferenceStore(store_path);
    Check(store && store->preferences() == legacy, "V1 rollback target survives restart with unchanged settings");
  }
  const auto invalid_path = Directory(root, L"invalid-v1-scale");
  auto invalid_bytes = *legacy_bytes;
  // The final three LE u32 fields are scale/target, vsync, fullscreen.
  const auto offset = invalid_bytes.size() - 12;
  invalid_bytes[offset] = 3;
  invalid_bytes[offset+1] = invalid_bytes[offset+2] = invalid_bytes[offset+3] = 0;
  FixtureWrite(invalid_path + L"\\preferences.dat", invalid_bytes);
  Check(!OpenPreferenceStore(invalid_path), "legacy scale3 remains invalid, never silently migrates to 2160p");
  auto invalid_guard = fs::OpenDirectory(invalid_path);
  Check(invalid_guard && Read(*invalid_guard, L"preferences.dat") == invalid_bytes && ChildCount(invalid_path) == 1,
        "invalid V1 admission leaves bytes and inventory unchanged without a lock");
}

void TestProfiles(const std::wstring &root) {
  const auto store_path = Directory(root, L"profile-library");
  const auto parent = Directory(root, L"personal-profiles");
  const auto build = Directory(root, L"build");
  const auto game = Directory(root, L"game");
  FixtureText(parent + L"\\owner-note.txt", "parent sentinel");
  auto store = OpenPreferenceStore(store_path);
  Check(store.has_value(), "profile-test store opens");
  if (!store) return;
  Preferences preferences;
  preferences.build_root = Utf16(build);
  preferences.game_root = Utf16(game);
  const auto before = ChildCount(parent);
  auto created = CreateManagedProfile(preferences, parent, u"MAIN", store->directory());
  Check(created.has_value(), "new explicit child created");
  if (!created) return;
  Check(created->label == u"MAIN" && ValidOpaqueId(created->id), "new entry has explicit label and opaque identity");
  Check(Wide(created->root).starts_with(parent + L"\\MCLA-"), "only unique child, not parent adopted");
  Check(ChildCount(parent) == before + 1, "creation adds exactly one child to parent");
  auto parent_guard = fs::OpenDirectory(parent);
  Check(parent_guard.has_value(), "parent remains guardable");
  if (!parent_guard) return;
  Check(Read(*parent_guard, L"owner-note.txt") == std::vector<uint8_t>({'p','a','r','e','n','t',' ','s','e','n','t','i','n','e','l'}),
        "parent unrelated content unchanged");
  auto import_profile = CreateImportProfile(preferences, parent, u"Pending import", store->directory());
  Check(import_profile.has_value(), "safe import child creation succeeds");
  if (import_profile) {
    auto importing = fs::OpenDirectory(Wide(import_profile->root));
    Check(importing.has_value(), "import child guardable");
    if (importing) {
      Check(!fs::ReadFile(*importing, kManagedProfileMarker, 128), "new import child never publishes completed managed marker");
      const auto pending = Read(*importing, L"import-in-progress.dat");
      Check(std::string(pending.begin(), pending.end()) == "MCLA managed profile v1\n" + import_profile->id + "\n",
            "import-pending marker has exact eventual managed marker contents");
      Check(!RegisterManagedProfile(preferences, Wide(import_profile->root), u"Pending import", store->directory()),
            "new import child cannot register before import owner completes it");
      Check(!Read(*importing, kProfileInitializationMarker).empty(), "import initialization provenance preserved");
    }
  }
  auto child = fs::OpenDirectory(Wide(created->root));
  Check(child.has_value(), "new child guardable");
  if (!child) return;
  const auto marker = Read(*child, kManagedProfileMarker);
  const std::string marker_text(marker.begin(), marker.end());
  Check(marker_text == "MCLA managed profile v1\n" + created->id + "\n", "completed marker exact schema and ID");
  Check(!Read(*child, kProfileInitializationMarker).empty(), "initialization provenance retained");
  Check(GetFileAttributesW((Wide(created->root) + L"\\managed-profile.pending.dat").c_str()) == INVALID_FILE_ATTRIBUTES,
        "pending marker atomically renamed only at completion");
  for (const wchar_t *name : {L"user", L"cache", L"logs", L"diagnostics", L"sessions", L"backups", L"metadata", L"metadata\\update"}) {
    const DWORD attributes = GetFileAttributesW((Wide(created->root) + L"\\" + name).c_str());
    Check(attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_DIRECTORY), "managed child layout complete");
  }
  auto registered = RegisterManagedProfile(preferences, Wide(created->root), u"MAIN", store->directory());
  Check(registered.has_value(), "completed managed profile can register");
  if (!registered) return;
  Check(registered->library.entries.size() == 1 && registered->library.selected_id.empty(),
        "registration appends but never auto-selects");
  Check(preferences.library.entries.empty(), "registration does not mutate caller input");
  Check(!RegisterManagedProfile(*registered, Wide(created->root), u"Alias", store->directory()),
        "same physical root rejected");
  auto alias = Wide(created->root);
  for (auto &c : alias) if (c >= L'a' && c <= L'z') c -= L'a' - L'A';
  Check(!RegisterManagedProfile(*registered, alias, u"Case alias", store->directory()),
        "case alias of same identity rejected");
  auto second = CreateManagedProfile(*registered, parent, u"Clean", store->directory());
  Check(second.has_value(), "sibling profile allowed under same personal parent");
  if (!second) return;
  auto both = RegisterManagedProfile(*registered, Wide(second->root), u"Clean", store->directory());
  Check(both.has_value(), "second managed profile registers");
  if (!both) return;
  for (const auto &id : {created->id, second->id}) {
    both->library.selected_id = id;
    Check(store->Save(*both).has_value(), "explicit selector saved without moving profile contents");
    auto decoded = DecodePreferences(Read(store->directory(), L"preferences.dat"));
    Check(decoded && decoded->library.selected_id == id && decoded->library.entries.size() == 2,
          "both selectors roundtrip independently");
  }
  auto third = CreateManagedProfile(*both, parent, u"Third", store->directory());
  Check(third.has_value(), "third sibling created without selecting");
  if (third) {
    auto registered_third = RegisterManagedProfile(*both, Wide(third->root), u"Third", store->directory());
    Check(registered_third && registered_third->library.selected_id == second->id,
          "registration preserves previous explicit selection");
    FixtureText(Wide(third->root) + L"\\import-in-progress.dat", "synthetic incomplete import");
    const auto importing = RegisterManagedProfile(*both, Wide(third->root), u"Importing", store->directory());
    Check(!importing && importing.error().find(L"import is incomplete") != std::wstring::npos,
          "import-in-progress rejects registration even with valid completed marker");
  }
  const size_t parent_count = ChildCount(parent);
  Check(!CreateManagedProfile(*both, parent, u" invalid ", store->directory()), "invalid label fails before creation");
  Check(ChildCount(parent) == parent_count, "invalid creation leaves parent unchanged");
  for (const auto &protected_root : {Wide(created->root), build, game, store_path}) {
    const auto count = ChildCount(protected_root);
    Check(!CreateManagedProfile(*both, protected_root, u"Forbidden nested", store->directory()),
          "creation inside protected root rejected");
    Check(ChildCount(protected_root) == count, "protected root untouched after rejected creation");
  }
  const auto unknown = Directory(root, L"unknown-profile");
  FixtureText(unknown + L"\\managed-profile.dat", "MCLA managed profile v99\nnot-known\n");
  Check(!RegisterManagedProfile(*both, unknown, u"Unknown", store->directory()), "unknown marker rejected");
  const auto incomplete = Directory(root, L"incomplete-profile");
  FixtureText(incomplete + L"\\initialization-started.dat", "started but not complete");
  Check(!RegisterManagedProfile(*both, incomplete, u"Incomplete", store->directory()), "incomplete child never adopted");
  const auto duplicated = Directory(root, L"copied-identity");
  FixtureText(duplicated + L"\\managed-profile.dat", marker_text);
  Check(!RegisterManagedProfile(*both, duplicated, u"Copied identity", store->directory()), "copied marker ID rejected across distinct roots");
  const auto link = root + L"\\profile-link";
  if (CreateSymbolicLinkW(link.c_str(), Wide(created->root).c_str(),
                         SYMBOLIC_LINK_FLAG_DIRECTORY | 0x2 /* unprivileged flag */)) {
    Check(!RegisterManagedProfile(*both, link, u"Reparse", store->directory()), "reparse registration rejected");
  } else {
    ++skips;
    std::cout << "SKIP directory symlink fixture unavailable (Win32 " << GetLastError() << ")\n";
  }
}

void TestWritablePolicy(const std::wstring &root) {
  auto safe = fs::OpenDirectory(root);
  Check(safe && ValidateWritableRoot(*safe), "dedicated personal fixture directory allowed");
  // These are read-only policy probes. No creation is attempted in real known
  // folders: OpenPreferenceStore must refuse them before its first lock write.
  for (const auto *id : {&FOLDERID_Profile, &FOLDERID_Documents, &FOLDERID_LocalAppData,
                         &FOLDERID_RoamingAppData, &FOLDERID_ProgramFiles,
                         &FOLDERID_ProgramFilesX86, &FOLDERID_ProgramData}) {
    PWSTR path = nullptr;
    const HRESULT resolved = SHGetKnownFolderPath(*id, KF_FLAG_DONT_VERIFY, nullptr, &path);
    Check(SUCCEEDED(resolved) && path, "protected known folder resolved");
    if (FAILED(resolved) || !path) { CoTaskMemFree(path); continue; }
    auto guard = fs::OpenDirectory(path);
    Check(!guard || !ValidateWritableRoot(*guard), "exact known-folder container blocked");
    const auto lock = std::wstring(path) + L"\\.mcla-writer.lock";
    const auto before = GetFileAttributesW(lock.c_str());
    Check(!OpenPreferenceStore(path), "protected store rejected before acquiring lock");
    Check(GetFileAttributesW(lock.c_str()) == before, "protected store rejection creates no lock file");
    CoTaskMemFree(path);
  }
  wchar_t windows[32768]{};
  const UINT count = GetWindowsDirectoryW(windows, 32768);
  Check(count && count < 32768, "Windows directory resolved for readonly policy probe");
  if (count && count < 32768) {
    auto guard = fs::OpenDirectory(windows);
    Check(!guard || !ValidateWritableRoot(*guard), "Windows root blocked");
    auto system = fs::OpenDirectory(std::wstring(windows) + L"\\System32");
    Check(!system || !ValidateWritableRoot(*system), "Windows subtree blocked");
  }
}
} // namespace

int main() {
  try {
    const auto root = NewFixture();
    std::wcout << L"Preserved isolated fixture: " << root << L'\n';
    TestStore(root);
    TestRenderTargetMigration(root);
    TestProfiles(root);
    TestWritablePolicy(root);
  } catch (const std::exception &error) {
    ++failures;
    std::cerr << "fixture error: " << error.what() << '\n';
  }
  std::cout << checks << " checks, " << failures << " failures, " << skips << " skips\n";
  return failures ? 1 : 0;
}
