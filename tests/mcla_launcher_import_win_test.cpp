#include "mcla_launcher_controller_win.h"
#include "mcla_launcher_import_policy.h"

#include <windows.h>
#include <bcrypt.h>
#include <array>
#include <exception>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <map>
#include <set>
#include <string>
#include <string_view>

// Integration of the production import action only. Never invokes Play, creates
// a window, launches a child, or reads an existing owner's bundle/save.
namespace {
using namespace mcla::launcher;
namespace path = std::filesystem;
using Json = nlohmann::json;
unsigned checks = 0, failures = 0;
const std::string bundle_id = "mcla-0.18.0.0-win64-AAAAAAAAAAAAAAAA-BBBBBBBBBBBBBBBB-CCCCCCCCCCCCCCCC";
const path::path save_relative = L"B13EBABEBABEBABE/545407F8/00000001/mc4.sav/mc4.sav";
const path::path header_relative = L"B13EBABEBABEBABE/545407F8/Headers/00000001/mc4.sav.header";
void Check(bool pass, const char *label) {
  ++checks;
  if (!pass) { ++failures; std::cerr << "FAIL " << label << '\n'; }
}
std::wstring Native(const path::path &p) {
  // Extended-length Win32 paths do not translate '/' separators. Fixtures
  // intentionally use ordinary filesystem path literals with either spelling.
  auto preferred = p;
  preferred.make_preferred();
  return L"\\\\?\\" + preferred.wstring();
}
bool NewFile(const path::path &p, std::string_view bytes) {
  HANDLE file = CreateFileW(Native(p).c_str(), GENERIC_WRITE, 0, nullptr, CREATE_NEW,
      FILE_ATTRIBUTE_NORMAL, nullptr);
  if (file == INVALID_HANDLE_VALUE) {
    const auto error = GetLastError();
    std::wcerr << L"Fixture CREATE_NEW failed: " << p.wstring()
               << L" (Win32 " << error << L")\n";
    return false;
  }
  DWORD written = 0;
  const bool result = WriteFile(file, bytes.data(), static_cast<DWORD>(bytes.size()), &written, nullptr) &&
      written == bytes.size() && FlushFileBuffers(file);
  const auto error = result ? ERROR_SUCCESS : GetLastError();
  CloseHandle(file);
  if (!result) std::wcerr << L"Fixture write/flush failed: " << p.wstring()
                         << L" (Win32 " << error << L")\n";
  return result;
}
std::string Read(const path::path &p) {
  std::ifstream file(p, std::ios::binary);
  return {std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
}
std::set<path::path> Children(const path::path &p) {
  std::set<path::path> result;
  for (const auto &entry : path::directory_iterator(p)) result.insert(entry.path());
  return result;
}
struct Tree {
  std::map<path::path, std::string> files;
  std::set<path::path> directories;
  bool operator==(const Tree &) const = default;
};
Tree Contents(const path::path &p) {
  Tree result;
  for (const auto &entry : path::recursive_directory_iterator(p)) {
    const auto relative = entry.path().lexically_relative(p);
    if (entry.is_directory()) result.directories.insert(relative);
    else result.files.emplace(relative, Read(entry.path()));
  }
  return result;
}
Json Manifest() {
  const std::string a(64, 'A'), b(64, 'B'), c(64, 'C'), d(64, 'D');
  return {
    {"schema", "mcla-portable-campaign-bundle-v2"}, {"task", "M7-016"}, {"bundle_id", bundle_id},
    {"project_version", "0.18.0.0"}, {"sdk_version", "0.19.0.0"}, {"sdk_commit", std::string(40, 'a')},
    {"created_utc", "2026-10-10T12:09:04.5528351Z"}, {"platform", "windows-proton"}, {"architecture", "x86_64"},
    {"launcher", "Launch-MCLA.exe"}, {"cpp_runtime", {{"policy", "msvc-14.44-app-local-v1"}, {"version", "14.44.35211.0"}}},
    {"window_mode", {{"startup_fullscreen", true}, {"alt_enter_toggle", true}, {"left_double_click_toggle", true}}},
    {"immutable_file_count", 33}, {"game", {{"manifest_sha256", a}, {"file_count", 15}, {"payload_bytes", 6569586392ULL}}},
    {"selected_save", {{"source_snapshot", "private/save-archive/M7-009/" + d},
      {"save_relative", "B13EBABEBABEBABE/545407F8/00000001/mc4.sav/mc4.sav"},
      {"header_relative", "B13EBABEBABEBABE/545407F8/Headers/00000001/mc4.sav.header"},
      {"save_sha256", c}, {"header_sha256", a}, {"profile_sha256", d}, {"ancestor_profile_sha256", Json::array()}}},
    {"writable_roots", {"user", "cache", "logs", "diagnostics", "results", "update", "metadata"}},
    {"retention", {{"completed_sessions", 32}, {"save_snapshots_per_session", 32}}},
    {"private_content", {{"game_and_save_private", true}, {"source_iso_included", false}}},
    {"image_capture", {{"mode", "window-wgc-v1"}, {"helper_path", "bin/mcla_diagnostic_image_helper.exe"},
      {"helper_sha256", b}, {"helper_bytes", 61440}, {"host_sha256", a}}}
  };
}
bool Bundle(const path::path &root, bool header = true, bool recognized = true) {
  path::create_directories(root / L"user" / save_relative.parent_path());
  path::create_directories(root / L"user" / header_relative.parent_path());
  path::create_directories(root / L"user/cache/nested");
  path::create_directories(root / L"user/empty-directory");
  path::create_directories(root / L"cache");
  auto manifest = Manifest();
  if (!recognized) manifest["schema"] = "unrecognized-fixture-only";
  return NewFile(root / L"bundle-id.txt", bundle_id + "\r\n") &&
      NewFile(root / L"bundle-manifest.json", manifest.dump()) &&
      NewFile(root / L"user" / save_relative, "SYNTHETIC_LIVE_SAVE_NOT_SEED_HASH\n") &&
      (!header || NewFile(root / L"user" / header_relative, "SYNTHETIC_HEADER\n")) &&
      NewFile(root / L"user/B13EBABEBABEBABE/local-display-name.txt", "ImportFixture") &&
      NewFile(root / L"user/cache/nested/fixture.bin", std::string("A\0B", 3)) &&
      NewFile(root / L"cache/not-user.txt", "DO_NOT_IMPORT_BUNDLE_CACHE\n");
}
UiRequest Request(const path::path &bundle, const path::path &destination) {
  UiRequest r;
  r.legacy_source_root = (bundle / L"user").wstring();
  r.profile_root = destination.wstring();
  r.profile_label = L"Imported fixture";
  return r;
}
} // namespace

int main() {
  try {
    std::array<wchar_t, 32768> temp{};
    const auto length = GetTempPathW(static_cast<DWORD>(temp.size()), temp.data());
    if (!length || length >= temp.size()) return 2;
    std::array<unsigned char, 16> bytes{};
    if (BCryptGenRandom(nullptr, bytes.data(), static_cast<ULONG>(bytes.size()), BCRYPT_USE_SYSTEM_PREFERRED_RNG) < 0) return 2;
    std::wstring suffix;
    for (auto b : bytes) { suffix += L"0123456789abcdef"[b >> 4]; suffix += L"0123456789abcdef"[b & 15]; }
    const auto root = path::path(temp.data()) / (L"mcli-import-" + suffix);
    if (!CreateDirectoryW(Native(root).c_str(), nullptr)) return 2;
    std::wcout << L"Synthetic fixture preserved: " << root.wstring() << L'\n';
    path::create_directory(root / L"store");
    path::create_directory(root / L"destinations");
    auto store = OpenPreferenceStore((root / L"store").wstring());
    if (!store) { std::wcerr << store.error() << L'\n'; return 2; }
    LauncherController controller(std::move(*store));
    UiRequest seed; seed.profile_root = (root / L"destinations").wstring(); seed.profile_label = L"Existing selected fixture";
    auto state = controller.Dispatch(UiAction::CreateProfile, seed);
    Check(state.library.entries.size() == 1, "create independent selected-profile fixture");
    if (state.library.entries.size() != 1) { std::wcerr << state.status << L'\n'; return 1; }
    const auto selected_id = state.library.entries.front().id;
    UiRequest select; select.selected_profile_id = selected_id;
    state = controller.Dispatch(UiAction::SelectProfile, select);
    Check(state.library.selected_id == selected_id, "explicitly select preexisting managed profile");

    const auto old = root / L"old";
    Check(Bundle(old), "create recognized old bundle with complete synthetic user tree");
    const auto original = Contents(old / L"user");
    const auto old_manifest = Read(old / L"bundle-manifest.json"), old_id = Read(old / L"bundle-id.txt");
    const auto before = Children(root / L"destinations");
    const auto hint = root / L"store/collection-parent.dat";
    if (!MoveFileExW(Native(hint).c_str(), Native(root / L"preserved-create-hint.dat").c_str(), MOVEFILE_WRITE_THROUGH)) return 2;
    state = controller.Dispatch(UiAction::ImportLegacy, Request(old, root / L"destinations"));
    Check(state.library.entries.size() == 2, "production ImportLegacy registers complete new profile");
    const auto encoded_parent = (root / L"destinations").u8string();
    const auto imported_hint = Read(hint);
    Check(state.collection_parent == (root / L"destinations").wstring() &&
        imported_hint == "MCLA-COLLECTION-PARENT-V1\n" + std::string(encoded_parent.begin(), encoded_parent.end()),
        "successful production import persists canonical collection parent in optional versioned sidecar");
    Check(state.library.selected_id == selected_id && !controller.running(), "import preserves selection and never launches a child");
    if (state.library.entries.size() != 2) { std::wcerr << state.status << L'\n'; return 1; }
    const auto &imported = state.library.entries.back();
    const path::path imported_root(imported.root);
    Check(imported.id != selected_id && ValidOpaqueId(imported.id), "import receives independent opaque managed identity");
    Check(Contents(imported_root / L"user") == original, "all user files binary bytes and empty directories copied exactly");
    Check(path::is_empty(imported_root / L"cache"), "old bundle cache outside user is not imported");
    Check(Read(imported_root / L"user/B13EBABEBABEBABE/local-display-name.txt") == "ImportFixture", "in-game name preserved without renaming source");
    Check(Read(imported_root / L"import-source-manifest.json") == old_manifest, "exact original bundle manifest retained as provenance");
    const auto provenance = Read(imported_root / L"import-source.txt");
    Check(provenance.starts_with("MCLA-LEGACY-IMPORT-V1\nbundle " + bundle_id + "\nsource "), "receipt binds normalized original bundle identity and source path");
    Check(Read(imported_root / L"import-receipt.txt").starts_with("MCLA-SAVE-SNAPSHOT-V1\n") &&
        path::exists(imported_root / L"managed-profile.dat") && !path::exists(imported_root / L"import-in-progress.dat"),
        "complete inventory and final marker published only after import");
    Check(Children(root / L"destinations").size() == before.size() + 1, "successful import creates exactly one new managed root");
    Check(Contents(old / L"user") == original && Read(old / L"bundle-manifest.json") == old_manifest &&
        Read(old / L"bundle-id.txt") == old_id && Read(old / L"cache/not-user.txt") == "DO_NOT_IMPORT_BUNDLE_CACHE\n",
        "original complete user tree bundle metadata and external cache unchanged");

    const auto reject_before_creation = [&](const path::path &source, const char *label) {
      const auto previous = Children(root / L"destinations");
      const auto source_before = Contents(source / L"user");
      const auto rejected = controller.Dispatch(UiAction::ImportLegacy, Request(source, root / L"destinations"));
      Check(rejected.library.entries.size() == 2 && rejected.library.selected_id == selected_id &&
          Children(root / L"destinations") == previous && Contents(source / L"user") == source_before, label);
    };
    Check(Bundle(root / L"missing-header", false), "create missing-header fixture");
    reject_before_creation(root / L"missing-header", "missing header rejected before destination creation with source and selection intact");
    Check(Bundle(root / L"unknown", true, false), "create unrecognized metadata fixture");
    reject_before_creation(root / L"unknown", "unknown metadata rejected before destination creation");
    Check(Bundle(root / L"busy"), "create busy legacy fixture");
    HANDLE busy = CreateFileW(Native(root / L"busy/bundle.lock").c_str(), GENERIC_READ | GENERIC_WRITE, 0,
        nullptr, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr);
    Check(busy != INVALID_HANDLE_VALUE, "hold exact legacy bundle lock without a game");
    if (busy == INVALID_HANDLE_VALUE) return 1;
    reject_before_creation(root / L"busy", "held old bundle lock rejects import before destination creation");
    CloseHandle(busy);

    const auto inside = old / L"user/empty-directory";
    const auto inside_before = Children(inside), destinations_before = Children(root / L"destinations");
    state = controller.Dispatch(UiAction::ImportLegacy, Request(old, inside));
    Check(state.library.entries.size() == 2 && state.library.selected_id == selected_id &&
        Children(inside) == inside_before && Children(root / L"destinations") == destinations_before &&
        Contents(old / L"user") == original, "destination inside original bundle rejected before new child creation");

    const auto unsafe = root / L"hardlink";
    Check(Bundle(unsafe) && NewFile(unsafe / L"owned-link-target.bin", "SYNTHETIC_EXTRA\n"), "create separate unsafe-copy fixture");
    const bool linked = CreateHardLinkW(Native(unsafe / L"user/extra-hardlink.bin").c_str(),
        Native(unsafe / L"owned-link-target.bin").c_str(), nullptr) != FALSE;
    Check(linked, "create fixture-owned extra user hardlink (never owner data)");
    if (!linked) return 1;
    const auto unsafe_original = Contents(unsafe / L"user");
    const auto unsafe_before = Children(root / L"destinations");
    state = controller.Dispatch(UiAction::ImportLegacy, Request(unsafe, root / L"destinations"));
    auto incomplete = Children(root / L"destinations");
    for (const auto &existing : unsafe_before) incomplete.erase(existing);
    Check(state.library.entries.size() == 2 && state.library.selected_id == selected_id && incomplete.size() == 1,
        "unsafe extra file fails full-tree copy leaving one unregistered incomplete root");
    if (incomplete.size() == 1) {
      Check(path::exists(*incomplete.begin() / L"import-in-progress.dat") &&
          !path::exists(*incomplete.begin() / L"managed-profile.dat"), "failed copy cannot publish a completed profile marker");
      UiRequest register_failed; register_failed.profile_root = incomplete.begin()->wstring(); register_failed.profile_label = L"Must not register";
      state = controller.Dispatch(UiAction::RegisterManaged, register_failed);
      Check(state.library.entries.size() == 2 && state.library.selected_id == selected_id,
          "explicit registration also refuses incomplete import");
    }
    Check(Contents(unsafe / L"user") == unsafe_original && Read(unsafe / L"owned-link-target.bin") == "SYNTHETIC_EXTRA\n",
        "failed copy preserves all original source bytes and hardlink target");
    Check(!controller.running() && !state.running, "all integration actions finish without game or helper process launch");
    Check(Read(hint) == imported_hint, "failed production imports and registration never replace collection hint");
    std::cout << "Launcher production legacy import: " << checks << " checks, " << failures << " failures\n";
    return failures ? 1 : 0;
  } catch (const std::exception &error) {
    std::cerr << "Synthetic import fixture exception: " << error.what() << '\n';
    return 2;
  }
}
