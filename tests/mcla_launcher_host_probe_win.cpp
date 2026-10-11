#include "mcla_launcher_controller_win.h"
#include "mcla_launcher_build_win.h"
#include "mcla_launcher_game_win.h"
#include "mcla_managed_config_policy.h"

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <bcrypt.h>

#include <array>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>

#if !defined(MCLA_LAUNCHER_HOST_LIFECYCLE_TEST)
#error This guest-free host probe requires the nonshipping lifecycle-only controller switch.
#endif
#if defined(MCLA_LAUNCHER_CONTROLLER_TESTING) || defined(MCLA_LAUNCHER_GAME_TESTING)
#error Real-host admission must never use synthetic game metadata or GPU admission.
#endif

namespace {
using namespace mcla::launcher;
unsigned checks = 0, failures = 0;
void Check(bool pass, const char *label) {
  ++checks;
  if (!pass) { ++failures; std::cerr << "FAIL " << label << '\n'; }
}
std::expected<std::string, std::wstring> Read(const std::filesystem::path &file,
                                           uint64_t maximum = 65536) {
  std::error_code error;
  const auto size = std::filesystem::file_size(file, error);
  if (error || size > maximum) return std::unexpected(L"Missing or oversized probe output: " + file.wstring());
  std::ifstream stream(file, std::ios::binary);
  std::string bytes(static_cast<size_t>(size), '\0');
  if (!stream || !stream.read(bytes.data(), static_cast<std::streamsize>(bytes.size())) ||
      stream.peek() != std::char_traits<char>::eof())
    return std::unexpected(L"Cannot read exact probe output: " + file.wstring());
  return bytes;
}
bool Empty(const std::filesystem::path &path) {
  std::error_code error;
  const bool result = std::filesystem::is_empty(path, error);
  return !error && result;
}
std::expected<std::filesystem::path, std::wstring> SoleSession(const std::filesystem::path &profile) {
  std::filesystem::path result;
  std::error_code error;
  std::filesystem::directory_iterator entries(profile / L"sessions", error), end;
  if (error) return std::unexpected(L"Cannot inspect owned session directory");
  for (; entries != end; entries.increment(error)) {
    if (error || !result.empty()) return std::unexpected(L"Expected exactly one owned session");
    result = entries->path();
  }
  if (error || result.empty()) return std::unexpected(L"Missing owned session");
  return result;
}
int Probe(int argc, wchar_t **argv) {
  std::map<std::wstring, std::wstring> options;
  bool verify_only = false;
  for (int i = 1; i < argc; ++i) {
    const std::wstring_view arg(argv[i]);
    if (arg == L"--verify-only" && !verify_only) { verify_only = true; continue; }
    const auto equal = arg.find(L'=');
    if (!arg.starts_with(L"--") || equal == arg.npos || equal + 1 == arg.size()) return 2;
    const auto key = arg.substr(2, equal - 2);
    if ((key != L"build" && key != L"game" && key != L"gpu") ||
        !options.emplace(key, arg.substr(equal + 1)).second) return 2;
  }
  if (!options.contains(L"build") || !options.contains(L"game") ||
      (!verify_only && !options.contains(L"gpu"))) {
    std::cerr << "Required: --build=<exact staged root> --game=<exact supported root> "
                 "[--verify-only | --gpu=<exact adapter name>]\n";
    return 2;
  }
  auto build_root = fs::OpenDirectory(options.at(L"build"));
  auto game_root = fs::OpenDirectory(options.at(L"game"));
  if (!build_root || !game_root) {
    std::wcerr << (!build_root ? build_root.error() : game_root.error()) << L'\n'; return 1;
  }
  auto disjoint = fs::CompareDisjoint(*build_root, *game_root);
  if (!disjoint) { std::wcerr << disjoint.error() << L'\n'; return 1; }
  auto build = VerifyBuild(*build_root);
  if (!build) { std::wcerr << build.error() << L'\n'; return 1; }
  auto game = VerifyGameRoot(*game_root);
  if (!game) { std::wcerr << game.error() << L'\n'; return 1; }
  std::cout << "BUILD_INTEGRITY_VERIFIED " << build->fingerprint() << '\n'
            << "GAME_REVISION_VERIFIED " << game->fingerprint() << '\n';
  // Keep both verified inventories pinned throughout the owned child lifetime.
  // Verification is not gameplay compatibility or a trust/signature claim.
  if (verify_only) { std::cout << "VERIFY_ONLY_COMPLETE_NO_HOST_LAUNCH\n"; return 0; }

  std::array<unsigned char, 16> random{};
  if (BCryptGenRandom(nullptr, random.data(), static_cast<ULONG>(random.size()),
                      BCRYPT_USE_SYSTEM_PREFERRED_RNG) < 0) return 2;
  std::wstring name = L"mcla-host-probe-";
  for (const auto b : random) { name += L"0123456789abcdef"[b >> 4]; name += L"0123456789abcdef"[b & 15]; }
  const auto root = std::filesystem::temp_directory_path() / name;
  if (!CreateDirectoryW(root.c_str(), nullptr)) return 2;
  std::wcout << L"Fresh guest-free host probe; preserved evidence: " << root.wstring() << L'\n';
  for (const auto &path : {root / L"store", root / L"profiles"})
    if (!CreateDirectoryW(path.c_str(), nullptr)) return 2;
  auto store = OpenPreferenceStore((root / L"store").wstring());
  if (!store) { std::wcerr << store.error() << L'\n'; return 1; }
  Preferences p;
  p.build_root = std::filesystem::path(build->path()).u16string();
  p.game_root = std::filesystem::path(game->path()).u16string();
  p.gpu_name = std::u16string(options.at(L"gpu").begin(), options.at(L"gpu").end());
  p.resolution_scale = 1; p.vsync = true; p.fullscreen = false;
  auto profile = CreateManagedProfile(p, (root / L"profiles").wstring(), u"Guest-free host probe", store->directory());
  if (!profile) { std::wcerr << profile.error() << L'\n'; return 1; }
  p.library.entries.push_back(*profile); p.library.selected_id = profile->id;
  const auto saved = store->Save(p);
  if (!saved) { std::wcerr << saved.error() << L'\n'; return 1; }
  const std::filesystem::path personal(profile->root);
  Check(Empty(personal / L"user") && Empty(personal / L"sessions"), "fresh empty profile has no imported owner content");
  if (failures) return 1;
  LauncherController controller(std::move(*store));
  UiRequest request;
  request.build_root = build->path(); request.game_root = game->path();
  request.gpu_name = options.at(L"gpu"); request.resolution_scale = 1;
  request.vsync = true; request.fullscreen = false;
  const auto start = controller.Dispatch(UiAction::Play, request);
  Check(start.running && controller.running(), "production controller admits guest-free real host");
  if (!controller.running()) { std::wcerr << start.status << L'\n'; return 1; }
  auto session = SoleSession(personal);
  if (!session) { std::wcerr << session.error() << L'\n'; return 1; }
  const auto initial_config = Read(*session / L"managed.toml");
  Check(initial_config && mcla::managed_config::IsSentinel(*initial_config), "session begins with exact comment-only sentinel");
  const auto began = GetTickCount64();
  UiState latest = start;
  while (controller.running() && GetTickCount64() - began < 30000) {
    latest = controller.Dispatch(UiAction::Refresh, {});
    if (controller.running()) Sleep(10);
  }
  Check(!controller.running(), "owned host and all job descendants finish within 30 seconds");
  if (controller.running()) {
    std::wcerr << L"Probe timed out; only its owned job is cleaned up, all evidence preserved.\n" << latest.status << L'\n';
    return 1;
  }
  Check(!controller.completion_failed(), "controller reports successful session finalization");
  const auto state = Read(*session / L"state.txt");
  Check(state && *state == "complete\nexit 0\n", "normal zero exit has an exact complete session receipt");
  const auto config = Read(*session / L"managed.toml");
  Check(config && initial_config && *config == *initial_config && mcla::managed_config::IsSentinel(*config),
        "host never rewrites immutable managed sentinel");
  const auto log = Read(*session / L"mcla.log", 16ull * 1024 * 1024);
  Check(log && log->find("MCLA lifecycle: probe requested; guest runtime skipped") != log->npos,
        "host log explicitly confirms guest runtime was skipped");
  Check(log && log->find("MCLA lifecycle: probe complete") != log->npos,
        "host log explicitly confirms lifecycle probe completion");
  const auto before = Read(*session / L"before-snapshot.txt");
  const auto after = Read(*session / L"after-snapshot.txt");
  Check(before && after && *before == "MCLA-SAVE-SNAPSHOT-V1\n" && *before == *after,
        "complete before/after snapshot inventories prove no user-tree changes");
  Check(Empty(personal / L"user") && Empty(*session / L"before-user") && Empty(*session / L"after-user"),
        "guest-free host leaves personal user and both snapshots empty");
  const auto receipt = Read(*session / L"launch-receipt.txt");
  Check(receipt && receipt->find("profile " + profile->id + "\n") != receipt->npos &&
        receipt->find("\nbuild " + build->fingerprint() + "\n") != receipt->npos &&
        receipt->find("\ngame " + game->fingerprint() + "\n") != receipt->npos,
        "session provenance binds exact fresh profile build and game");
  auto unchanged_build = VerifyBuild(*build_root);
  Check(unchanged_build && unchanged_build->fingerprint() == build->fingerprint(),
        "build inventory remains exact with no generated config/log files");
  auto profile_guard = fs::OpenDirectory(personal.wstring());
  if (profile_guard) {
    auto writer = fs::AcquireWriterLock(*profile_guard);
    Check(writer.has_value(), "profile writer lock releases after owned-job completion");
  } else Check(false, "owned profile remains accessible");
  std::cout << "Guest-free managed host: " << checks << " checks, " << failures << " failures\n";
  std::cout << (failures ? "GUEST_FREE_HOST_PROBE_FAILED\n" : "GUEST_FREE_HOST_PROBE_PASS_NOT_GAMEPLAY\n");
  return failures ? 1 : 0;
}
} // namespace

int wmain(int argc, wchar_t **argv) {
  try { return Probe(argc, argv); }
  catch (const std::exception &error) {
    std::cerr << "Host probe failed; owned temporary evidence is preserved: " << error.what() << '\n';
    return 1;
  }
}
