#include "mcla_launcher_controller_win.h"
#include "mcla_launcher_build_win.h"
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
#include <iterator>
#include <map>
#include <optional>
#include <set>

#if !defined(MCLA_LAUNCHER_CONTROLLER_TESTING)
#error This synthetic integration fixture requires the nonshipping controller test seam.
#endif

namespace {
using namespace mcla::launcher;
unsigned checks = 0, failures = 0;
void Check(bool pass, const char *label) {
  ++checks;
  if (!pass) { ++failures; std::cerr << "FAIL " << label << '\n'; }
}
std::u16string U16(const std::filesystem::path &p) { return p.u16string(); }
bool NewFile(const std::filesystem::path &path, std::string_view contents) {
  HANDLE file = CreateFileW(path.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_NEW,
      FILE_ATTRIBUTE_NORMAL, nullptr);
  if (file == INVALID_HANDLE_VALUE) return false;
  DWORD written = 0;
  const bool result = WriteFile(file, contents.data(), static_cast<DWORD>(contents.size()), &written, nullptr) &&
      written == contents.size() && FlushFileBuffers(file);
  CloseHandle(file);
  return result;
}
std::string Read(const std::filesystem::path &path) {
  std::ifstream file(path, std::ios::binary);
  return {std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
}
std::expected<std::string, std::wstring> HashFile(const std::filesystem::path &path) {
  BCRYPT_ALG_HANDLE algorithm = nullptr;
  BCRYPT_HASH_HANDLE hash = nullptr;
  if (BCryptOpenAlgorithmProvider(&algorithm, BCRYPT_SHA256_ALGORITHM, nullptr, 0) < 0)
    return std::unexpected(L"fixture hash algorithm failed");
  if (BCryptCreateHash(algorithm, &hash, nullptr, 0, nullptr, 0, 0) < 0) {
    BCryptCloseAlgorithmProvider(algorithm, 0);
    return std::unexpected(L"fixture hash allocation failed");
  }
  std::ifstream input(path, std::ios::binary);
  std::array<unsigned char, 65536> buffer{};
  bool good = input.is_open();
  uint64_t count = 0;
  while (good && input) {
    input.read(reinterpret_cast<char *>(buffer.data()), buffer.size());
    const auto read = input.gcount();
    count += static_cast<uint64_t>(read);
    if (count > 128ull * 1024 * 1024 || (read && BCryptHashData(hash, buffer.data(), static_cast<ULONG>(read), 0) < 0))
      good = false;
  }
  good &= input.eof();
  std::array<unsigned char, 32> digest{};
  good &= BCryptFinishHash(hash, digest.data(), static_cast<ULONG>(digest.size()), 0) >= 0;
  BCryptDestroyHash(hash);
  BCryptCloseAlgorithmProvider(algorithm, 0);
  if (!good) return std::unexpected(L"fixture executable hashing failed");
  std::string output;
  for (const auto byte : digest) { output += "0123456789ABCDEF"[byte >> 4]; output += "0123456789ABCDEF"[byte & 15]; }
  return output;
}

// The copied fixture executable is not an MCLA host. It accepts only the
// controller's nonshipping switch, validates routing, writes a harmless marker
// in its synthetic user tree, then waits for an explicit fixture-owned signal.
int FakeChild(int argc, wchar_t **argv) {
  std::map<std::wstring, std::wstring> args;
  for (int i = 1; i < argc; ++i) {
    std::wstring_view arg(argv[i]);
    const auto equals = arg.find(L'=');
    if (!arg.starts_with(L"--") || equals == arg.npos ||
        !args.emplace(arg.substr(2, equals - 2), arg.substr(equals + 1)).second) return 81;
  }
  if (args[L"mcla_launcher_fixture"] != L"true") return 82;
  const auto path = [&](const wchar_t *name) { return std::filesystem::path(args[name]); };
  const auto sentinel = path(L"mcla_managed_launch_config");
  const mcla::managed_config::RequiredPaths roots{U16(path(L"game_data_root")), U16(path(L"user_data_root")),
      U16(path(L"cache_root")), U16(path(L"update_data_root")), U16(path(L"metadata_root")),
      U16(path(L"log_file")), U16(path(L"mcla_diagnostics_root"))};
  if (mcla::managed_config::Validate(U16(sentinel), roots)) return 83;
  HANDLE pin = CreateFileW(sentinel.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING,
      FILE_FLAG_OPEN_REPARSE_POINT, nullptr);
  if (pin == INVALID_HANDLE_VALUE) return 84;
  std::array<char, mcla::managed_config::kSentinel.size() + 1> bytes{};
  DWORD count = 0;
  const bool valid_sentinel = ReadFile(pin, bytes.data(), static_cast<DWORD>(bytes.size()), &count, nullptr) &&
      mcla::managed_config::IsSentinel({bytes.data(), count});
  const auto finish = [&] (int code) { CloseHandle(pin); return code; };
  if (!valid_sentinel) return finish(85);
  const auto cwd = std::filesystem::current_path();
  if (cwd != sentinel.parent_path() || path(L"log_file").parent_path() != cwd) return finish(86);
  const auto profile = cwd.parent_path().parent_path();
  if (cwd.parent_path().filename() != L"sessions" ||
      path(L"user_data_root") != profile / L"user" ||
      path(L"cache_root").parent_path() != profile / L"cache" ||
      path(L"metadata_root") != profile / L"metadata" ||
      path(L"update_data_root") != profile / L"metadata/update" ||
      path(L"mcla_diagnostics_root").parent_path() != profile / L"diagnostics") return finish(92);
  const std::array<std::wstring_view, 11> allowed{L"APPDATA", L"HOMEDRIVE", L"HOMEPATH", L"LOCALAPPDATA",
      L"PATH", L"ProgramData", L"SystemRoot", L"TEMP", L"TMP", L"USERPROFILE", L"WINDIR"};
  wchar_t *environment = GetEnvironmentStringsW();
  if (!environment) return finish(87);
  bool safe = true;
  for (const wchar_t *entry = environment; *entry; entry += wcslen(entry) + 1) {
    const std::wstring_view text(entry);
    const auto key = text.substr(0, text.find(L'='));
    bool known = false;
    for (const auto name : allowed) known |= name == key;
    safe &= known;
  }
  FreeEnvironmentStringsW(environment);
  if (!safe) return finish(88);
  if (args[L"d3d12_adapter"] != L"0" || args[L"resolution_scale"] != L"2" ||
      args[L"vsync"] != L"true" || args[L"fullscreen"] != L"false" ||
      args[L"gpu_backend"] != L"d3d12") return finish(89);
  if (!NewFile(path(L"user_data_root") / L"fixture-save.bin", "FIXTURE_SAVE_ONLY\n") ||
      !NewFile(cwd / L"fixture-ready.txt", "ROOTS_ENV_SETTINGS_SENTINEL_PASS\n")) return finish(90);
  const auto started = GetTickCount64();
  do {
    if (Read(cwd / L"finish.signal") == "FINISH_FIXTURE\n") return finish(0);
    Sleep(5);
  } while (GetTickCount64() - started < 10000);
  return finish(91);
}

UiRequest Settings(const Preferences &p) {
  UiRequest r;
  r.build_root = std::filesystem::path(p.build_root).wstring();
  r.game_root = std::filesystem::path(p.game_root).wstring();
  r.gpu_name = L"Fixture"; r.resolution_scale = 2; r.vsync = true; r.fullscreen = false;
  r.selected_profile_id = p.library.selected_id;
  return r;
}
std::filesystem::path OnlySession(const std::filesystem::path &profile) {
  std::filesystem::path result;
  for (const auto &entry : std::filesystem::directory_iterator(profile / L"sessions")) {
    if (!result.empty()) return {};
    result = entry.path();
  }
  return result;
}
std::set<std::filesystem::path> Children(const std::filesystem::path &directory) {
  std::set<std::filesystem::path> result;
  for (const auto &entry : std::filesystem::directory_iterator(directory)) result.insert(entry.path());
  return result;
}

// A separate matrix avoids borrowing the earlier single-build controller case:
// both build inventories are independently valid and have different identities.
bool CrossBuildMatrix(const std::filesystem::path &root,
                      const std::filesystem::path &build_a,
                      const std::filesystem::path &game) {
  const auto build_b = root / L"build-b";
  for (const auto &path : {build_b, root / L"matrix-store-a",
                          root / L"matrix-store-b", root / L"matrix-profiles"})
    if (!CreateDirectoryW(path.c_str(), nullptr)) return false;
  if (!CopyFileW((build_a / L"mcla.exe").c_str(), (build_b / L"mcla.exe").c_str(), TRUE) ||
      !NewFile(build_b / L"mcla_crash_handler.exe", "second build fixture\n")) return false;
  const auto host_hash = HashFile(build_b / L"mcla.exe");
  const auto helper_hash = HashFile(build_b / L"mcla_crash_handler.exe");
  if (!host_hash || !helper_hash) return false;
  const std::string manifest = "MCLA-DEV-BUILD-V1\n" + *host_hash + " " +
      std::to_string(std::filesystem::file_size(build_b / L"mcla.exe")) + " mcla.exe\n" +
      *helper_hash + " " + std::to_string(std::filesystem::file_size(build_b / L"mcla_crash_handler.exe")) +
      " mcla_crash_handler.exe\n";
  if (!NewFile(build_b / L"mcla-build.manifest", manifest)) return false;
  const auto hash_a = HashFile(build_a / L"mcla-build.manifest");
  const auto hash_b = HashFile(build_b / L"mcla-build.manifest");
  if (!hash_a || !hash_b) return false;
  Check(*hash_a != *hash_b, "cross-build matrix uses distinct build fingerprints");

  auto store_a = OpenPreferenceStore((root / L"matrix-store-a").wstring());
  auto store_b = OpenPreferenceStore((root / L"matrix-store-b").wstring());
  if (!store_a || !store_b) return false;
  Preferences p;
  p.build_root = U16(build_a); p.game_root = U16(game); p.gpu_name = u"Fixture";
  p.resolution_scale = 2;
  auto profile_a = CreateManagedProfile(p, (root / L"matrix-profiles").wstring(), u"Matrix A", store_a->directory());
  if (!profile_a) return false;
  p.library.entries.push_back(*profile_a); p.library.selected_id = profile_a->id;
  auto profile_b = CreateManagedProfile(p, (root / L"matrix-profiles").wstring(), u"Matrix B", store_a->directory());
  if (!profile_b) return false;
  p.library.entries.push_back(*profile_b);
  auto second_preferences = p; second_preferences.build_root = U16(build_b);
  if (!store_a->Save(p) || !store_b->Save(second_preferences)) return false;
  const std::filesystem::path root_a(profile_a->root), root_b(profile_b->root);
  if (!NewFile(root_a / L"user/a-only.bin", "A_ONLY\n") ||
      !NewFile(root_b / L"user/b-only.bin", "B_ONLY\n")) return false;
  LauncherController a(std::move(*store_a)), b(std::move(*store_b));
  const auto request_a = Settings(p), request_b = Settings(second_preferences);
  const auto verified_a = a.Dispatch(UiAction::VerifyRoots, request_a);
  const auto verified_b = b.Dispatch(UiAction::VerifyRoots, request_b);
  Check(verified_a.status.find(std::wstring(hash_a->begin(), hash_a->end())) != std::wstring::npos &&
        verified_b.status.find(std::wstring(hash_b->begin(), hash_b->end())) != std::wstring::npos,
        "both distinct matrix build inventories pass production verification");
  Check(a.Dispatch(UiAction::Play, request_a).running, "matrix build A starts profile A");
  if (!a.running()) return false;
  const auto session_a = OnlySession(root_a);
  if (session_a.empty()) return false;
  const auto blocked = b.Dispatch(UiAction::Play, request_b);
  Check(!blocked.running && !b.running() && OnlySession(root_a) == session_a &&
        blocked.status.find(L"writer lock unavailable") != std::wstring::npos,
        "different valid build B cannot acquire profile A or create another session");
  UiRequest select_b; select_b.selected_profile_id = profile_b->id;
  const auto selected_b = b.Dispatch(UiAction::SelectProfile, select_b);
  Check(selected_b.library.selected_id == profile_b->id &&
        std::filesystem::is_empty(root_b / L"sessions") &&
        Read(root_b / L"user/b-only.bin") == "B_ONLY\n" &&
        !std::filesystem::exists(root_b / L"user/a-only.bin"),
        "matrix folder selection changes pointer only without moving saves");
  Check(b.Dispatch(UiAction::Play, request_b).running && a.running(),
        "different builds may run independent profiles simultaneously");
  if (!b.running()) return false;
  const auto session_b = OnlySession(root_b);
  if (session_b.empty()) return false;
  const auto started = GetTickCount64();
  bool ready = false;
  do {
    ready = Read(session_a / L"fixture-ready.txt") == "ROOTS_ENV_SETTINGS_SENTINEL_PASS\n" &&
            Read(session_b / L"fixture-ready.txt") == "ROOTS_ENV_SETTINGS_SENTINEL_PASS\n";
    if (ready) break;
    a.Dispatch(UiAction::Refresh, {}); b.Dispatch(UiAction::Refresh, {});
    if (!a.running() || !b.running()) break;
    Sleep(5);
  } while (GetTickCount64() - started < 5000);
  Check(ready, "both simultaneous children independently verify profile-local routing");
  if (!ready) return false;
  Check(Read(root_a / L"user/a-only.bin") == "A_ONLY\n" &&
        Read(root_b / L"user/b-only.bin") == "B_ONLY\n" &&
        !std::filesystem::exists(root_a / L"user/b-only.bin") &&
        !std::filesystem::exists(root_b / L"user/a-only.bin"),
        "simultaneous profiles retain distinct private save markers");
  const auto receipt_a = Read(session_a / L"launch-receipt.txt");
  const auto receipt_b = Read(session_b / L"launch-receipt.txt");
  Check(receipt_a.find("profile " + profile_a->id + "\nbuild " + *hash_a + "\n") != std::string::npos &&
        receipt_b.find("profile " + profile_b->id + "\nbuild " + *hash_b + "\n") != std::string::npos,
        "simultaneous receipts bind each exact profile and different build");
  Check(Children(root_a / L"diagnostics").size() == 1 && Children(root_b / L"diagnostics").size() == 1 &&
        std::filesystem::exists(root_a / L"diagnostics" / session_a.filename()) &&
        std::filesystem::exists(root_b / L"diagnostics" / session_b.filename()) &&
        std::filesystem::exists(root_a / L"cache" / std::wstring(hash_a->begin(), hash_a->end())) &&
        std::filesystem::exists(root_b / L"cache" / std::wstring(hash_b->begin(), hash_b->end())),
        "simultaneous diagnostic and cache roots remain profile/build scoped");
  if (!NewFile(session_a / L"finish.signal", "FINISH_FIXTURE\n") ||
      !NewFile(session_b / L"finish.signal", "FINISH_FIXTURE\n")) return false;
  const auto finishing = GetTickCount64();
  while ((a.running() || b.running()) && GetTickCount64() - finishing < 10000) {
    a.Dispatch(UiAction::Refresh, {}); b.Dispatch(UiAction::Refresh, {}); Sleep(5);
  }
  Check(!a.running() && !b.running() && !a.completion_failed() && !b.completion_failed(),
        "both independent profile sessions complete their owned jobs normally");
  Check(Read(session_a / L"state.txt") == "complete\nexit 0\n" &&
        Read(session_b / L"state.txt") == "complete\nexit 0\n" &&
        Read(session_a / L"after-user/a-only.bin") == "A_ONLY\n" &&
        Read(session_b / L"after-user/b-only.bin") == "B_ONLY\n" &&
        !std::filesystem::exists(session_a / L"after-user/b-only.bin") &&
        !std::filesystem::exists(session_b / L"after-user/a-only.bin"),
        "independent final snapshots preserve the correct profile lineage");
  return true;
}

std::optional<std::map<std::filesystem::path,std::string>> FixtureInventory(
    const std::filesystem::path &root) {
  std::map<std::filesystem::path,std::string> result;
  size_t count=0;
  for(const auto &entry:std::filesystem::recursive_directory_iterator(root)) {
    if(++count>4096 || entry.is_symlink()) return std::nullopt;
    const auto relative=entry.path().lexically_relative(root);
    if(entry.is_directory()) result.emplace(relative,"directory");
    else if(entry.is_regular_file()) {
      // Persistent lock handles are deliberately exclusive. Their pathname and
      // size still detect creation/removal without bypassing the held lock.
      if(entry.path().filename()==L".mcla-writer.lock")
        result.emplace(relative,"lock:"+std::to_string(entry.file_size()));
      else {
        auto hash=HashFile(entry.path()); if(!hash) return std::nullopt;
        result.emplace(relative,*hash);
      }
    } else return std::nullopt;
  }
  return result;
}

bool ReadOnlyPreflightMatrix(const std::filesystem::path &root,
                           const std::filesystem::path &build,
                           const std::filesystem::path &game,
                           const std::filesystem::path &bad_build) {
  const auto matrix=root/L"preflight";
  const auto build_before=FixtureInventory(build), game_before=FixtureInventory(game);
  if(!build_before || !game_before) return false;
  for(const auto &path:{matrix,matrix/L"store",matrix/L"profiles",matrix/L"bad-game"})
    if(!CreateDirectoryW(path.c_str(),nullptr)) return false;
  if(!NewFile(matrix/L"bad-game/default.xex","wrong")) return false;
  auto store=OpenPreferenceStore((matrix/L"store").wstring()); if(!store) return false;
  Preferences p; p.build_root=U16(build); p.game_root=U16(game); p.gpu_name=u"Fixture"; p.resolution_scale=2;
  auto profile=CreateManagedProfile(p,(matrix/L"profiles").wstring(),u"Preflight",store->directory());
  if(!profile) return false;
  p.library.entries.push_back(*profile); p.library.selected_id=profile->id;
  if(!store->Save(p)) return false;
  const std::filesystem::path profile_root(profile->root);
  if(!NewFile(profile_root/L"user/preserved.bin","PREFLIGHT_ORIGINAL\n")) return false;
  LauncherController controller(std::move(*store));
  auto request=Settings(p); request.fullscreen=true; // Unsaved draft must stay unsaved.
  const auto run_check=[&](const UiRequest &r,LaunchReadiness expected,const char *label) {
    auto before=FixtureInventory(matrix);
    const auto view=controller.Dispatch(UiAction::CheckLaunch,r);
    auto after=FixtureInventory(matrix);
    Check(view.readiness==expected && !view.readiness_reason.empty() &&
          !view.running && !controller.running(),label);
    Check(before && after && *before==*after,
          "preflight preserves every fixture file directory preference and save without new output");
    return view;
  };
  Check(controller.Dispatch(UiAction::Refresh,{}).readiness==LaunchReadiness::Unchecked,
        "ordinary refresh does not invent readiness");
  const auto ready=run_check(request,LaunchReadiness::Ready,"valid exact-profile preflight is Ready without a child");
  Check(!ready.fullscreen && request.fullscreen && std::filesystem::is_empty(profile_root/L"sessions"),
        "read-only preflight uses drafts without persisting them or creating a session");
  Check(controller.Dispatch(UiAction::Refresh,{}).readiness==LaunchReadiness::Unchecked,
        "readiness is not persisted across unrelated controller responses");
  auto invalid=request; invalid.selected_profile_id.clear();
  run_check(invalid,LaunchReadiness::Blocked,"missing acknowledged request profile is Blocked");
  invalid=request; invalid.selected_profile_id="11111111111111111111111111111111";
  run_check(invalid,LaunchReadiness::Blocked,"different request profile cannot borrow selected profile readiness");
  invalid=request; invalid.gpu_name=L"Unavailable fixture GPU";
  run_check(invalid,LaunchReadiness::Blocked,"unavailable exact GPU is Blocked");
  invalid=request; invalid.vsync=false;
  run_check(invalid,LaunchReadiness::Blocked,"unsupported stock VSync Off is Blocked");
  invalid=request; invalid.resolution_scale=3;
  run_check(invalid,LaunchReadiness::Blocked,"unsupported render scale is Blocked");
  invalid=request; invalid.build_root=bad_build.wstring();
  run_check(invalid,LaunchReadiness::Blocked,"malformed build inventory is Blocked");
  invalid=request; invalid.game_root=(matrix/L"bad-game").wstring();
  run_check(invalid,LaunchReadiness::Blocked,"wrong game inventory is Blocked");
  {
    auto guard=fs::OpenDirectory(profile_root.wstring()); if(!guard) return false;
    auto writer=fs::AcquireWriterLock(*guard); if(!writer) return false;
    const auto busy=run_check(request,LaunchReadiness::Blocked,"busy profile is Blocked without snapshot or session");
    Check(busy.readiness_reason.find(L"writer lock")!=std::wstring::npos,
          "busy profile failure identifies exclusive writer admission");
  }
  run_check(request,LaunchReadiness::Ready,"released profile lock permits a fresh Ready check");
  if(!MoveFileExW((profile_root/L".mcla-writer.lock").c_str(),(matrix/L"preserved-lock").c_str(),MOVEFILE_WRITE_THROUGH)) return false;
  run_check(request,LaunchReadiness::Blocked,"missing profile lock is Blocked and never recreated by Check");
  if(!MoveFileExW((matrix/L"preserved-lock").c_str(),(profile_root/L".mcla-writer.lock").c_str(),MOVEFILE_WRITE_THROUGH)) return false;
  if(!MoveFileExW((profile_root/L"logs").c_str(),(matrix/L"preserved-logs").c_str(),MOVEFILE_WRITE_THROUGH)) return false;
  run_check(request,LaunchReadiness::Blocked,"missing required profile child is Blocked without repair");
  if(!MoveFileExW((matrix/L"preserved-logs").c_str(),(profile_root/L"logs").c_str(),MOVEFILE_WRITE_THROUGH)) return false;
  if(!NewFile(profile_root/L"pending-operation.dat","fixture pending\n")) return false;
  run_check(request,LaunchReadiness::Blocked,"pending profile operation is Blocked without clearing marker");
  if(!MoveFileExW((profile_root/L"pending-operation.dat").c_str(),(matrix/L"preserved-pending.dat").c_str(),MOVEFILE_WRITE_THROUGH)) return false;
  const auto unfinished=profile_root/L"sessions/11111111111111111111111111111111";
  if(!CreateDirectoryW(unfinished.c_str(),nullptr) || !NewFile(unfinished/L"state.txt","running\n")) return false;
  run_check(request,LaunchReadiness::Blocked,"unfinished session is Blocked and retained");
  if(!MoveFileExW(unfinished.c_str(),(matrix/L"preserved-unfinished").c_str(),MOVEFILE_WRITE_THROUGH)) return false;
  const auto marker=profile_root/L"managed-profile.dat";
  if(!MoveFileExW(marker.c_str(),(matrix/L"preserved-marker.dat").c_str(),MOVEFILE_WRITE_THROUGH) ||
      !NewFile(marker,"MCLA managed profile v1\n11111111111111111111111111111111\n")) return false;
  run_check(request,LaunchReadiness::Blocked,"changed managed profile identity is Blocked without adoption");
  if(!MoveFileExW(marker.c_str(),(matrix/L"rejected-marker.dat").c_str(),MOVEFILE_WRITE_THROUGH) ||
      !MoveFileExW((matrix/L"preserved-marker.dat").c_str(),marker.c_str(),MOVEFILE_WRITE_THROUGH)) return false;
  const auto name_root=profile_root/L"user/B13EBABEBABEBABE";
  if(!CreateDirectoryW(name_root.c_str(),nullptr) ||
      !NewFile(name_root/L"local-display-name.txt","INVALID\nNAME")) return false;
  run_check(request,LaunchReadiness::Blocked,"invalid persisted in-game name is Blocked without rewriting");
  if(!MoveFileExW((name_root/L"local-display-name.txt").c_str(),
      (matrix/L"rejected-name.txt").c_str(),MOVEFILE_WRITE_THROUGH)) return false;
  const auto reject_start=[&](const char *label) {
    const auto before=FixtureInventory(profile_root/L"user");
    // Play may save settings, unlike Check. Use the original persisted settings;
    // this assertion covers no session preparation or save mutation on failure.
    const auto view=controller.Dispatch(UiAction::Play,Settings(p));
    const auto after=FixtureInventory(profile_root/L"user");
    Check(!view.running && !controller.running() &&
          std::filesystem::is_empty(profile_root/L"sessions") &&
          before && after && *before==*after,label);
  };
  const auto linked_source=matrix/L"hardlink-source.bin";
  const auto linked_user=profile_root/L"user/hardlink-fixture.bin";
  if(!NewFile(linked_source,"HARDLINK_FIXTURE\n") ||
      !CreateHardLinkW(linked_user.c_str(),linked_source.c_str(),nullptr)) return false;
  run_check(request,LaunchReadiness::Blocked,"hardlinked save is Blocked before claiming snapshot eligibility");
  reject_start("hardlinked save rejects actual Play before session preparation without changing saves");
  if(!MoveFileExW(linked_user.c_str(),(matrix/L"rejected-hardlink.bin").c_str(),MOVEFILE_WRITE_THROUGH)) return false;

  const auto ads_user=profile_root/L"user/ads-fixture.bin";
  const std::filesystem::path ads_stream(ads_user.wstring()+L":fixture");
  if(!NewFile(ads_user,"ADS_DEFAULT_FIXTURE\n") || !NewFile(ads_stream,"ADS_PRIVATE_FIXTURE\n")) return false;
  run_check(request,LaunchReadiness::Blocked,"alternate-stream save is Blocked before claiming snapshot eligibility");
  reject_start("alternate-stream save rejects actual Play before session preparation without changing saves");
  Check(Read(ads_stream)=="ADS_PRIVATE_FIXTURE\n","rejected save alternate stream remains byte-identical");
  if(!MoveFileExW(ads_user.c_str(),(matrix/L"rejected-ads.bin").c_str(),MOVEFILE_WRITE_THROUGH)) return false;

  const auto fingerprint=HashFile(build/L"mcla-build.manifest"); if(!fingerprint) return false;
  const auto obstructed_cache=profile_root/L"cache"/std::wstring(fingerprint->begin(),fingerprint->end());
  if(!NewFile(obstructed_cache,"NOT_A_CACHE_DIRECTORY\n")) return false;
  run_check(request,LaunchReadiness::Blocked,"regular file at build-specific cache is Blocked without replacement");
  reject_start("cache obstruction rejects actual Play before session preparation without changing saves");
  if(!MoveFileExW(obstructed_cache.c_str(),(matrix/L"rejected-cache-file.bin").c_str(),MOVEFILE_WRITE_THROUGH)) return false;
  run_check(request,LaunchReadiness::Ready,"fresh check after explicit fixture recovery is Ready");
  const auto build_after=FixtureInventory(build), game_after=FixtureInventory(game);
  Check(build_after && game_after && *build_before==*build_after && *game_before==*game_after,
        "all read-only checks preserve immutable build and game inventories");
  return true;
}
bool CollectionParentMatrix(const std::filesystem::path &root,
                            const std::filesystem::path &build,
                            const std::filesystem::path &game) {
  const auto matrix=root/L"collection-hint", local=matrix/L"store";
  const auto a=matrix/L"collection-a", b=matrix/L"collection-b", c=matrix/L"collection-c";
  for(const auto &path:{matrix,local,a,b,c}) if(!CreateDirectoryW(path.c_str(),nullptr)) return false;
  auto store=OpenPreferenceStore(local.wstring()); if(!store) return false;
  Preferences p; p.build_root=U16(build); p.game_root=U16(game); p.gpu_name=u"Fixture";
  auto original=CreateManagedProfile(p,a.wstring(),u"Original",store->directory()); if(!original) return false;
  p.library.entries.push_back(*original); p.library.selected_id=original->id;
  auto external=CreateManagedProfile(p,c.wstring(),u"Register later",store->directory()); if(!external) return false;
  if(!store->Save(p)) return false;
  const std::filesystem::path original_root(original->root);
  const auto hint=local/L"collection-parent.dat";
  auto controller=std::make_unique<LauncherController>(std::move(*store));
  const auto readonly_hint=[&](const std::wstring &expected,const char *label) {
    const auto before=FixtureInventory(matrix);
    const auto view=controller->Dispatch(UiAction::Refresh,{});
    controller->Dispatch(UiAction::CheckLaunch,Settings(p));
    const auto after=FixtureInventory(matrix);
    Check(view.collection_parent==expected,label);
    Check(before && after && *before==*after,"collection hint Refresh and Check never create repair or save files");
  };
  readonly_hint(a.wstring(),"old preferences without optional hint infer acknowledged profile collection not user folder");
  Check(!std::filesystem::exists(hint),"fallback does not migrate old preferences or auto-save a hint");
  UiRequest create; create.profile_root=b.wstring(); create.profile_label=L"Created in B";
  const auto created=controller->Dispatch(UiAction::CreateProfile,create);
  Check(created.library.entries.size()==2 && created.library.selected_id==original->id &&
      created.collection_parent==b.wstring() && std::filesystem::exists(hint),
      "explicit create remembers canonical collection without changing selected profile");
  if(created.library.entries.size()!=2 || !std::filesystem::exists(hint)) return false;
  const auto saved_hint=Read(hint), saved_preferences=Read(local/L"preferences.dat");
  controller.reset();
  auto reopened=OpenPreferenceStore(local.wstring()); if(!reopened) return false;
  Check(reopened->preferences().library.entries.size()==2 &&
      reopened->preferences().library.selected_id==original->id && Read(local/L"preferences.dat")==saved_preferences,
      "hint sidecar survives ordinary version-one preference reload without schema or byte changes");
  controller=std::make_unique<LauncherController>(std::move(*reopened));
  readonly_hint(b.wstring(),"remembered collection wins over selected profile fallback after reopen");
  UiRequest failed=create; failed.profile_root=c.wstring(); failed.profile_label=L"";
  controller->Dispatch(UiAction::CreateProfile,failed);
  controller->Dispatch(UiAction::DuplicateProfile,failed);
  failed.profile_label=L"Failed import"; failed.legacy_source_root=(matrix/L"missing/user").wstring();
  controller->Dispatch(UiAction::ImportLegacy,failed);
  failed.legacy_source_root=(matrix/L"missing-snapshot").wstring();
  controller->Dispatch(UiAction::RestoreSnapshot,failed);
  Check(Read(hint)==saved_hint,"failed create duplicate import and restore never remember requested parent");
  UiRequest registration; registration.profile_root=std::filesystem::path(external->root).wstring();
  registration.profile_label=L"External existing";
  const auto registered=controller->Dispatch(UiAction::RegisterManaged,registration);
  Check(registered.library.entries.size()==3 && registered.collection_parent==b.wstring() && Read(hint)==saved_hint,
      "registering arbitrary existing root does not change collection hint");
  if(!MoveFileExW(hint.c_str(),(matrix/L"preserved-valid-hint.dat").c_str(),MOVEFILE_WRITE_THROUGH)) return false;
  const auto utf8=[](const std::filesystem::path &path) {
    const auto encoded=path.u8string(); return std::string(encoded.begin(),encoded.end());
  };
  const std::string prefix="MCLA-COLLECTION-PARENT-V1\n";
  const std::vector<std::string> invalid_hints{
      "unknown version",prefix+std::string(1,static_cast<char>(0xff)),
      prefix+utf8(matrix/L"missing"),prefix+utf8(original_root/L"user"),
      prefix+utf8(build),prefix+utf8(local),prefix+"C:\\"};
  unsigned index=0;
  for(const auto &bytes:invalid_hints) {
    if(!NewFile(hint,bytes)) return false;
    readonly_hint(a.wstring(),"malformed unavailable nested or protected hint is ignored in favor of valid fallback");
    if(!MoveFileExW(hint.c_str(),(matrix/(L"rejected-hint-"+std::to_wstring(index++)+L".dat")).c_str(),MOVEFILE_WRITE_THROUGH)) return false;
  }
  const auto linked=matrix/L"linked-hint-source.dat";
  if(!NewFile(linked,saved_hint) || !CreateHardLinkW(hint.c_str(),linked.c_str(),nullptr)) return false;
  readonly_hint(a.wstring(),"unsafe hardlinked hint is ignored without modification");
  if(!MoveFileExW(hint.c_str(),(matrix/L"rejected-linked-hint.dat").c_str(),MOVEFILE_WRITE_THROUGH)) return false;
  const auto marker=original_root/L"managed-profile.dat";
  if(!MoveFileExW(marker.c_str(),(matrix/L"preserved-managed-marker.dat").c_str(),MOVEFILE_WRITE_THROUGH)) return false;
  readonly_hint(L"","missing managed marker prevents fallback from unacknowledged selected root");
  if(!MoveFileExW((matrix/L"preserved-managed-marker.dat").c_str(),marker.c_str(),MOVEFILE_WRITE_THROUGH)) return false;
  // A failed optional hint write must not misreport completed profile creation.
  if(!CreateDirectoryW(hint.c_str(),nullptr)) return false;
  const auto warned=controller->Dispatch(UiAction::CreateProfile,create);
  Check(warned.library.entries.size()==4 && warned.status.find(L"profile operation succeeded")!=std::wstring::npos &&
      std::filesystem::is_directory(hint) && warned.library.selected_id==original->id,
      "hint publication failure preserves successful creation and reports explicit warning");
  if(!MoveFileExW(hint.c_str(),(matrix/L"rejected-hint-directory").c_str(),MOVEFILE_WRITE_THROUGH)) return false;
  UiRequest copy; copy.profile_root=c.wstring(); copy.profile_label=L"Copy to C";
  const auto copied=controller->Dispatch(UiAction::DuplicateProfile,copy);
  Check(copied.library.entries.size()==5 && copied.collection_parent==c.wstring() &&
      copied.library.selected_id==original->id && std::filesystem::exists(hint),
      "successful duplicate remembers new collection while preserving selected source");
  return true;
}
} // namespace

int wmain(int argc, wchar_t **argv) {
  for (int i = 1; i < argc; ++i)
    if (std::wstring_view(argv[i]) == L"--mcla_launcher_fixture=true") return FakeChild(argc, argv);
  if (argc != 1) return 2;
  std::array<unsigned char, 16> random{};
  if (BCryptGenRandom(nullptr, random.data(), static_cast<ULONG>(random.size()), BCRYPT_USE_SYSTEM_PREFERRED_RNG) < 0) return 2;
  std::wstring name = L"mcla-controller-test-";
  for (const auto b : random) { name += L"0123456789abcdef"[b >> 4]; name += L"0123456789abcdef"[b & 15]; }
  const auto root = std::filesystem::temp_directory_path() / name;
  if (!CreateDirectoryW(root.c_str(), nullptr)) return 2;
  std::wcout << L"Owned synthetic controller fixture (preserved): " << root.wstring() << L'\n';
  const auto build = root / L"build", game = root / L"game", bad_build = root / L"bad-build";
  for (const auto &path : {build, game, bad_build, root / L"store-a", root / L"store-b", root / L"store-c", root / L"profiles"})
    if (!CreateDirectoryW(path.c_str(), nullptr)) return 2;
  std::array<wchar_t, 32768> executable{};
  const DWORD length = GetModuleFileNameW(nullptr, executable.data(), static_cast<DWORD>(executable.size()));
  if (!length || length >= executable.size() ||
      !CopyFileW(std::wstring(executable.data(), length).c_str(), (build / L"mcla.exe").c_str(), TRUE)) return 2;
  const auto hash = HashFile(build / L"mcla.exe");
  if (!hash) return 2;
  const std::string manifest = "MCLA-DEV-BUILD-V1\n" + *hash + " " +
      std::to_string(std::filesystem::file_size(build / L"mcla.exe")) + " mcla.exe\n" +
      "BA7816BF8F01CFEA414140DE5DAE2223B00361A396177A9CB410FF61F20015AD 3 mcla_crash_handler.exe\n";
  if (!NewFile(build / L"mcla_crash_handler.exe", "abc") ||
      !NewFile(build / L"mcla-build.manifest", manifest) ||
      !NewFile(game / L"default.xex", "abc") ||
      !NewFile(bad_build / L"mcla-build.manifest", "MALFORMED_FIXTURE_BUILD\n")) return 2;
  auto store_a = OpenPreferenceStore((root / L"store-a").wstring());
  auto store_b = OpenPreferenceStore((root / L"store-b").wstring());
  auto store_c = OpenPreferenceStore((root / L"store-c").wstring());
  if (!store_a || !store_b || !store_c) return 2;
  Preferences p;
  p.build_root = U16(build); p.game_root = U16(game); p.gpu_name = u"Fixture";
  p.resolution_scale = 2;
  auto main_profile = CreateManagedProfile(p, (root / L"profiles").wstring(), u"Main fixture", store_a->directory());
  if (!main_profile) { std::wcerr << main_profile.error() << L'\n'; return 2; }
  p.library.entries.push_back(*main_profile);
  p.library.selected_id = main_profile->id;
  auto other_profile = CreateManagedProfile(p, (root / L"profiles").wstring(), u"Other fixture", store_a->directory());
  if (!other_profile) return 2;
  p.library.entries.push_back(*other_profile);
  if (!store_a->Save(p) || !store_b->Save(p)) return 2;
  auto malformed = p;
  malformed.build_root = U16(bad_build);
  malformed.library.selected_id = other_profile->id;
  if (!store_c->Save(malformed)) return 2;
  const std::filesystem::path main_root(main_profile->root), other_root(other_profile->root);
  LauncherController first(std::move(*store_a)), contender(std::move(*store_b)), rejected(std::move(*store_c));
  const auto request = Settings(p);
  const auto prefs_before_verify = Read(root / L"store-a/preferences.dat");
  auto verify_request=request; verify_request.fullscreen=!request.fullscreen;
  const auto verified=first.Dispatch(UiAction::VerifyRoots,verify_request);
  Check(!verified.verification_failed,"successful explicit verification reports structured success");
  const auto manifest_hash=HashFile(build / L"mcla-build.manifest");
  Check(manifest_hash && verified.status.find(std::wstring(manifest_hash->begin(),manifest_hash->end()))!=std::wstring::npos &&
      !first.running() && std::filesystem::is_empty(main_root / L"sessions"), "read-only verify exposes exact build identity without child or session");
  Check(verified.fullscreen==verify_request.fullscreen && Read(root / L"store-a/preferences.dat")==prefs_before_verify,
      "verify retains unsaved form settings without persisting them");
  auto bad_verify=request; bad_verify.build_root=bad_build.wstring();
  const auto verify_failed=first.Dispatch(UiAction::VerifyRoots,bad_verify);
  Check(verify_failed.verification_failed,"failed explicit verification invalidates previous UI readiness");
  Check(verify_failed.status.find(L"Integrity verified")==std::wstring::npos && !first.running() &&
      Read(root / L"store-a/preferences.dat")==prefs_before_verify, "rejected verify does not claim integrity or mutate preferences");
  const auto initial = first.Dispatch(UiAction::Play, request);
  Check(initial.running && first.running(), "real controller starts only the synthetic self-child");
  Check(initial.readiness==LaunchReadiness::Running &&
        first.Dispatch(UiAction::CheckLaunch,request).readiness==LaunchReadiness::Running,
        "owned running session overrides readiness and refuses another preflight");
  if (!first.running()) { std::wcerr << initial.status << L'\n'; return 1; }
  const auto session = OnlySession(main_root);
  Check(!session.empty(), "one exact session created");
  if (session.empty()) return 1;
  bool ready = false;
  const auto began = GetTickCount64();
  do {
    if (Read(session / L"fixture-ready.txt") == "ROOTS_ENV_SETTINGS_SENTINEL_PASS\n") { ready = true; break; }
    const auto ui = first.Dispatch(UiAction::Refresh, {});
    if (!ui.running) { std::wcerr << ui.status << L'\n'; break; }
    Sleep(5);
  } while (GetTickCount64() - began < 5000);
  Check(ready, "child independently verifies exact roots environment settings and comment sentinel");
  if (!ready) return 1;
  const auto blocked = contender.Dispatch(UiAction::Play, request);
  Check(!blocked.running && !contender.running(), "second controller cannot write same profile");
  Check(OnlySession(main_root) == session, "lock rejection does not create another session");
  auto changed = request; changed.resolution_scale = 1; changed.vsync = false;
  const auto settings_blocked = first.Dispatch(UiAction::SaveSettings, changed);
  Check(settings_blocked.running && settings_blocked.resolution_scale == 2 && settings_blocked.vsync,
        "settings cannot change during owned session");
  UiRequest change_profile; change_profile.selected_profile_id = other_profile->id;
  const auto selection_blocked = first.Dispatch(UiAction::SelectProfile, change_profile);
  Check(selection_blocked.running && selection_blocked.library.selected_id == main_profile->id,
        "profile selection blocked while session owns save");
  UiRequest live_rename; live_rename.profile_label = L"Must not apply"; live_rename.in_game_name = L"MustNotApply";
  const auto live_label = first.Dispatch(UiAction::RenameLabel, live_rename);
  const auto live_name = first.Dispatch(UiAction::RenameInGameName, live_rename);
  Check(live_label.running && live_name.running && live_label.library.entries.front().label == main_profile->label &&
      live_name.in_game_name == L"User" && std::filesystem::is_empty(main_root / L"backups"),
      "both rename actions are blocked while owned child holds the profile");
  const auto bad = rejected.Dispatch(UiAction::Play, Settings(malformed));
  Check(!bad.running && !rejected.running() && std::filesystem::is_empty(other_root / L"sessions"),
        "malformed build rejected before child/session creation");
  Check(std::filesystem::is_empty(other_root / L"user"), "unselected other profile remains untouched");
  const HANDLE attempted = CreateFileW((build / L"mcla.exe").c_str(), GENERIC_WRITE,
      FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, 0, nullptr);
  Check(attempted == INVALID_HANDLE_VALUE, "verified build remains pinned through owned child");
  if (attempted != INVALID_HANDLE_VALUE) CloseHandle(attempted);
  const HANDLE config_write = CreateFileW((session / L"managed.toml").c_str(), GENERIC_WRITE,
      FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, 0, nullptr);
  Check(config_write == INVALID_HANDLE_VALUE, "fake child retains immutable sentinel like managed host");
  if (config_write != INVALID_HANDLE_VALUE) CloseHandle(config_write);
  Check(NewFile(session / L"finish.signal", "FINISH_FIXTURE\n"), "explicit owned fixture finish signal");
  const auto finishing = GetTickCount64();
  while (first.running() && GetTickCount64() - finishing < 10000) {
    first.Dispatch(UiAction::Refresh, {});
    Sleep(5);
  }
  Check(!first.running(), "controller waits for all job children then finishes");
  const auto final = first.Dispatch(UiAction::Refresh, {});
  if (first.running()) { std::wcerr << final.status << L'\n'; return 1; }
  Check(!first.completion_failed(), "successful fixture reports no finalization failure");
  Check(Read(session / L"state.txt") == "complete\nexit 0\n", "complete session exact normal-exit receipt");
  Check(Read(session / L"before-snapshot.txt") == "MCLA-SAVE-SNAPSHOT-V1\n" &&
      Read(session / L"after-snapshot.txt").find("F ") != std::string::npos,
      "before and after full-tree snapshot manifests retained");
  Check(std::filesystem::is_empty(session / L"before-user") &&
      Read(session / L"after-user/fixture-save.bin") == "FIXTURE_SAVE_ONLY\n" &&
      Read(main_root / L"user/fixture-save.bin") == "FIXTURE_SAVE_ONLY\n", "snapshots preserve actual fixture user change");
  const auto provenance = Read(session / L"launch-receipt.txt");
  Check(provenance.find("profile " + main_profile->id + "\n") != provenance.npos &&
      provenance.find("\ngame ") != provenance.npos && provenance.find("\nscale 2\n") != provenance.npos &&
      provenance.find("\nvsync 1\n") != provenance.npos, "receipt binds profile game and effective settings");
  Check(Read(build / L"mcla-build.manifest") == manifest && Read(game / L"default.xex") == "abc" &&
      HashFile(build / L"mcla.exe") == hash, "immutable synthetic build/game unchanged");
  Check(!std::filesystem::exists(build / L"mcla.toml") && !std::filesystem::exists(build / L"logs"),
        "nothing writes legacy config or build-local logs");
  auto final_profile = fs::OpenDirectory(main_root.wstring());
  if (final_profile) {
    auto released = fs::AcquireWriterLock(*final_profile);
    Check(released.has_value(), "profile writer lock released only after completed owned job");
  } else Check(false, "profile remains accessible");
  Check(std::filesystem::is_empty(other_root / L"user") && std::filesystem::is_empty(other_root / L"sessions"),
        "separate profile has no imported saves or sessions");
  const auto prefs_before_vsync = Read(root / L"store-a/preferences.dat");
  auto unsupported = request; unsupported.vsync = false;
  const auto vsync_rejected = first.Dispatch(UiAction::Play, unsupported);
  Check(!vsync_rejected.running && vsync_rejected.vsync && OnlySession(main_root) == session,
        "unvalidated VSync Off is rejected before another session or child");
  Check(!prefs_before_vsync.empty() && Read(root / L"store-a/preferences.dat") == prefs_before_vsync,
        "rejected VSync Off leaves authoritative preferences unchanged");

  UiRequest rename_label; rename_label.profile_label = L"Renamed fixture label";
  const auto labeled = first.Dispatch(UiAction::RenameLabel, rename_label);
  const auto unchanged_identity = [&](const UiState &state) {
    if (state.library.selected_id != main_profile->id || state.library.entries.size() != 2) return false;
    for (const auto &entry : state.library.entries)
      if (entry.id == main_profile->id)
        return entry.root == main_profile->root && entry.label == u"Renamed fixture label";
    return false;
  };
  Check(unchanged_identity(labeled), "library rename changes label only, retaining selected ID and root");
  Check(labeled.in_game_name == L"User" && Read(main_root / L"user/fixture-save.bin") == "FIXTURE_SAVE_ONLY\n" &&
      !std::filesystem::exists(main_root / L"user/B13EBABEBABEBABE") && std::filesystem::is_empty(main_root / L"backups"),
      "library rename does not change the in-game name or user content");
  UiRequest invalid_name; invalid_name.in_game_name = L"../not allowed";
  const auto invalid_rename = first.Dispatch(UiAction::RenameInGameName, invalid_name);
  Check(invalid_rename.in_game_name == L"User" && unchanged_identity(invalid_rename) &&
      std::filesystem::is_empty(main_root / L"backups") &&
      !std::filesystem::exists(main_root / L"user/B13EBABEBABEBABE"), "invalid in-game rename rejected before user/archive writes");
  UiRequest rename_name; rename_name.in_game_name = L"FixtureName";
  const auto renamed = first.Dispatch(UiAction::RenameInGameName, rename_name);
  Check(renamed.in_game_name == L"FixtureName" && unchanged_identity(renamed),
        "in-game rename updates displayed name without changing ID root or library label");
  Check(Read(main_root / L"user/B13EBABEBABEBABE/local-display-name.txt") == "FixtureName" &&
      Read(main_root / L"user/fixture-save.bin") == "FIXTURE_SAVE_ONLY\n", "rename writes stable XUID name while preserving save marker");
  std::filesystem::path archive;
  size_t archive_count = 0;
  for (const auto &entry : std::filesystem::directory_iterator(main_root / L"backups")) {
    ++archive_count; archive = entry.path();
  }
  Check(archive_count == 1 && archive.filename().wstring().starts_with(L"rename-"), "successful rename retains one explicit archive");
  if (archive_count == 1) {
    Check(Read(archive / L"before-user/fixture-save.bin") == "FIXTURE_SAVE_ONLY\n" &&
        !std::filesystem::exists(archive / L"before-user/B13EBABEBABEBABE") &&
        Read(archive / L"after-user/fixture-save.bin") == "FIXTURE_SAVE_ONLY\n" &&
        Read(archive / L"after-user/B13EBABEBABEBABE/local-display-name.txt") == "FixtureName",
        "rename archives retain complete before and after user content");
    const auto before_rename = Read(archive / L"before-snapshot.txt"), after_rename = Read(archive / L"after-snapshot.txt");
    Check(before_rename == Read(session / L"after-snapshot.txt") &&
        after_rename.starts_with("MCLA-SAVE-SNAPSHOT-V1\n") && after_rename != before_rename,
        "rename snapshots bind prior save inventory and the changed complete tree");
    Check(Read(archive / L"rename-receipt.txt") == "MCLA-PROFILE-NAME-V1\nprofile " + main_profile->id +
        "\nold User\nnew FixtureName\n" && Read(archive / L"completed.txt").starts_with("rename\n") &&
        !std::filesystem::exists(main_root / L"pending-operation.dat"), "successful rename publishes receipt and completes pending operation");
  }
  UiRequest branch; branch.profile_label = L"Duplicate fixture"; branch.profile_root = (root / L"profiles").wstring();
  const auto duplicated = first.Dispatch(UiAction::DuplicateProfile, branch);
  Check(duplicated.library.entries.size() == 3 && duplicated.library.selected_id == main_profile->id,
        "duplicate registers one new profile without changing selected source");
  if (duplicated.library.entries.size() != 3) { std::wcerr << duplicated.status << L'\n'; return 1; }
  const auto duplicate_entry = duplicated.library.entries.back();
  const std::filesystem::path duplicate_root(duplicate_entry.root);
  Check(ValidOpaqueId(duplicate_entry.id) && duplicate_entry.id != main_profile->id && duplicate_entry.root != main_profile->root,
        "duplicate has its own opaque identity and independent root");
  Check(Read(duplicate_root / L"user/fixture-save.bin") == "FIXTURE_SAVE_ONLY\n" &&
      Read(duplicate_root / L"user/B13EBABEBABEBABE/local-display-name.txt") == "FixtureName" &&
      std::filesystem::is_empty(duplicate_root / L"sessions") && std::filesystem::is_empty(duplicate_root / L"backups"),
      "duplicate copies full current user tree but not source session or rename history");
  Check(Read(duplicate_root / L"import-receipt.txt") == Read(archive / L"after-snapshot.txt") &&
      Read(duplicate_root / L"branch-receipt.txt").find("\nmode duplicate\nsource-profile " + main_profile->id +
          "\nnew-profile " + duplicate_entry.id + "\n") != std::string::npos,
      "duplicate binds complete copied inventory and both identities");
  branch.profile_label = L"Empty snapshot restore";
  branch.legacy_source_root = (session / L"before-user").wstring();
  const auto restored = first.Dispatch(UiAction::RestoreSnapshot, branch);
  Check(restored.collection_parent==(root/L"profiles").wstring() &&
      std::filesystem::exists(root/L"store-a/collection-parent.dat"),
      "successful snapshot restore remembers its destination collection");
  Check(restored.library.entries.size() == 4 && restored.library.selected_id == main_profile->id,
        "restore registers an independent branch while keeping original selected");
  if (restored.library.entries.size() != 4) { std::wcerr << restored.status << L'\n'; return 1; }
  const auto restore_entry = restored.library.entries.back();
  const std::filesystem::path restore_root(restore_entry.root);
  Check(ValidOpaqueId(restore_entry.id) && restore_entry.id != main_profile->id && restore_entry.id != duplicate_entry.id &&
      restore_entry.root != main_profile->root && std::filesystem::is_empty(restore_root / L"user") &&
      std::filesystem::is_empty(restore_root / L"sessions") && std::filesystem::is_empty(restore_root / L"backups"),
      "empty before-session snapshot restores into a new empty identity without history");
  Check(Read(main_root / L"user/fixture-save.bin") == "FIXTURE_SAVE_ONLY\n" &&
      Read(main_root / L"user/B13EBABEBABEBABE/local-display-name.txt") == "FixtureName" &&
      Read(restore_root / L"import-receipt.txt") == "MCLA-SAVE-SNAPSHOT-V1\n",
      "restoring an empty snapshot never overwrites current original user data");
  const auto before_outside = Children(root / L"profiles");
  branch.profile_label = L"Must reject outside snapshot"; branch.legacy_source_root = (other_root / L"user").wstring();
  const auto outside = first.Dispatch(UiAction::RestoreSnapshot, branch);
  Check(outside.library.entries.size() == 4 && outside.library.selected_id == main_profile->id &&
      Children(root / L"profiles") == before_outside, "arbitrary source outside selected profile is rejected before creating a branch");

  // Fresh fixture-only forged archive: the receipt describes the genuine
  // before-rename tree, but this copy has one changed byte. Original evidence
  // remains intact and all failed destinations are preserved.
  const auto forged = main_root / L"backups/rename-11111111111111111111111111111111";
  if (!CreateDirectoryW(forged.c_str(), nullptr) || !CreateDirectoryW((forged / L"before-user").c_str(), nullptr) ||
      !NewFile(forged / L"before-user/fixture-save.bin", "GIXTURE_SAVE_ONLY\n") ||
      !NewFile(forged / L"before-snapshot.txt", Read(archive / L"before-snapshot.txt"))) return 2;
  const auto before_forged = Children(root / L"profiles");
  branch.profile_label = L"Must reject forged snapshot"; branch.legacy_source_root = (forged / L"before-user").wstring();
  const auto mismatched = first.Dispatch(UiAction::RestoreSnapshot, branch);
  Check(mismatched.library.entries.size() == 4 && mismatched.library.selected_id == main_profile->id,
        "changed snapshot byte rejects registration and preserves selected library identity");
  const auto after_forged = Children(root / L"profiles");
  std::filesystem::path incomplete;
  size_t new_incomplete = 0;
  for (const auto &path : after_forged) if (!before_forged.contains(path)) { ++new_incomplete; incomplete = path; }
  Check(new_incomplete == 1, "failed snapshot verification preserves exactly one incomplete destination");
  if (new_incomplete == 1) {
    Check(!std::filesystem::exists(incomplete / L"managed-profile.dat") &&
        std::filesystem::exists(incomplete / L"import-in-progress.dat") &&
        Read(incomplete / L"user/fixture-save.bin") == "GIXTURE_SAVE_ONLY\n",
        "failed restore never publishes completed profile marker or deletes rejected copy");
  }
  Check(Read(forged / L"before-user/fixture-save.bin") == "GIXTURE_SAVE_ONLY\n" &&
      Read(main_root / L"user/fixture-save.bin") == "FIXTURE_SAVE_ONLY\n", "failed restore preserves source and original live data");
  Check(NewFile(main_root / L"pending-operation.dat", "fixture-owned-incomplete-operation\n"),
        "prepare explicit incomplete-operation blocker in fixture only");
  const auto pending_blocked = first.Dispatch(UiAction::Play, request);
  Check(!pending_blocked.running && !first.running() && OnlySession(main_root) == session &&
      Read(main_root / L"pending-operation.dat") == "fixture-owned-incomplete-operation\n",
      "pending operation rejects launch without child session deletion or automatic recovery");
  Check(Read(main_root / L"user/fixture-save.bin") == "FIXTURE_SAVE_ONLY\n" &&
      Read(main_root / L"user/B13EBABEBABEBABE/local-display-name.txt") == "FixtureName" &&
      std::filesystem::is_empty(other_root / L"user") && std::filesystem::is_empty(other_root / L"backups"),
      "blocked launch preserves renamed profile and does not affect separate profile");
  branch.profile_label = L"Must reject uncertain duplicate";
  const auto before_pending_duplicate = Children(root / L"profiles");
  const auto uncertain = first.Dispatch(UiAction::DuplicateProfile, branch);
  Check(uncertain.library.entries.size() == 4 && uncertain.library.selected_id == main_profile->id &&
      Children(root / L"profiles") == before_pending_duplicate, "pending operation prevents copying uncertain live user data");
  branch.profile_label = L"Verified recovery branch"; branch.legacy_source_root = (archive / L"after-user").wstring();
  const auto recovered = first.Dispatch(UiAction::RestoreSnapshot, branch);
  Check(recovered.library.entries.size() == 5 && recovered.library.selected_id == main_profile->id,
        "pending operation still permits restore from independently verified archived snapshot");
  if (recovered.library.entries.size() == 5) {
    const auto &recovery_entry = recovered.library.entries.back();
    const std::filesystem::path recovery_root(recovery_entry.root);
    Check(ValidOpaqueId(recovery_entry.id) && recovery_entry.id != main_profile->id &&
        Read(recovery_root / L"user/fixture-save.bin") == "FIXTURE_SAVE_ONLY\n" &&
        Read(recovery_root / L"user/B13EBABEBABEBABE/local-display-name.txt") == "FixtureName" &&
        Read(recovery_root / L"import-receipt.txt") == Read(archive / L"after-snapshot.txt"),
        "recovery branch contains complete verified snapshot with new opaque identity");
  }
  Check(Read(main_root / L"pending-operation.dat") == "fixture-owned-incomplete-operation\n" &&
      Read(main_root / L"user/B13EBABEBABEBABE/local-display-name.txt") == "FixtureName" &&
      OnlySession(main_root) == session, "recovery never clears original blocker overwrites source or starts a child");
  Check(CrossBuildMatrix(root, build, game), "cross-build matrix completed without fixture setup failure");
  Check(ReadOnlyPreflightMatrix(root, build, game, bad_build), "read-only launch preflight matrix completed");
  Check(CollectionParentMatrix(root,build,game),"optional collection parent hint matrix completed");
  std::cout << "Launcher controller: " << checks << " checks, " << failures << " failures\n";
  return failures ? 1 : 0;
}
