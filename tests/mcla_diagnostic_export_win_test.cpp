#include "mcla_diagnostic_export_win.h"
#include "mcla_launcher_capabilities.h"

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
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

// Retained synthetic TEMP fixtures only. No real profiles, game, build, settings,
// credentials, UI or network are used. The primary runs this target separately.
namespace {
namespace disk = std::filesystem;
namespace fs = mcla::launcher::fs;
namespace ex = mcla::launcher::diagnostic_export;
using mcla::launcher::Preferences;
unsigned checks = 0, failures = 0, skips = 0, serial = 0;
constexpr std::string_view kProfile = "11111111111111111111111111111111";
constexpr std::string_view kOther = "22222222222222222222222222222222";
constexpr std::string_view kSession = "33333333333333333333333333333333";
constexpr std::wstring_view kPackage = L"live-20261011T120000Z-123-1";
constexpr std::string_view kPrivate = "SYNTHETIC-PRIVATE-SENTINEL-NOT-REAL-DATA";
const std::string kBuild(64, 'A'), kGame(64, 'B');

void Check(bool ok, const char *label) {
  ++checks;
  if (!ok) { ++failures; std::cerr << "FAIL " << label << '\n'; }
}
void Need(bool ok, const char *label) { if (!ok) throw std::runtime_error(label); }
bool NewFile(const disk::path &path, std::string_view bytes) {
  const auto h = CreateFileW(path.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_NEW,
      FILE_ATTRIBUTE_NORMAL, nullptr);
  if (h == INVALID_HANDLE_VALUE) return false;
  DWORD written = 0;
  const bool ok = WriteFile(h, bytes.data(), static_cast<DWORD>(bytes.size()), &written, nullptr) &&
      written == bytes.size() && FlushFileBuffers(h);
  CloseHandle(h);
  return ok;
}
std::string Read(const disk::path &path) {
  std::ifstream stream(path, std::ios::binary);
  Need(stream.good(), "cannot read synthetic fixture");
  return {std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>()};
}
std::string Sha256(std::string_view bytes) {
  BCRYPT_ALG_HANDLE algorithm = nullptr;
  BCRYPT_HASH_HANDLE hash = nullptr;
  Need(BCryptOpenAlgorithmProvider(&algorithm, BCRYPT_SHA256_ALGORITHM, nullptr, 0) >= 0,
      "fixture SHA256 provider failed");
  DWORD length = 0, actual = 0;
  Need(BCryptGetProperty(algorithm, BCRYPT_OBJECT_LENGTH, reinterpret_cast<PUCHAR>(&length),
      sizeof(length), &actual, 0) >= 0, "fixture SHA256 property failed");
  std::vector<unsigned char> storage(length);
  Need(BCryptCreateHash(algorithm, &hash, storage.data(), length, nullptr, 0, 0) >= 0,
      "fixture SHA256 creation failed");
  Need(BCryptHashData(hash, reinterpret_cast<PUCHAR>(const_cast<char *>(bytes.data())),
      static_cast<ULONG>(bytes.size()), 0) >= 0, "fixture SHA256 input failed");
  std::array<unsigned char, 32> digest{};
  Need(BCryptFinishHash(hash, digest.data(), static_cast<ULONG>(digest.size()), 0) >= 0,
      "fixture SHA256 completion failed");
  BCryptDestroyHash(hash); BCryptCloseAlgorithmProvider(algorithm, 0);
  std::string result;
  for (auto b : digest) { result += "0123456789ABCDEF"[b >> 4]; result += "0123456789ABCDEF"[b & 15]; }
  return result;
}
using Tree = std::map<std::wstring, std::string>;
Tree Inventory(const disk::path &root) {
  Tree result;
  for (disk::recursive_directory_iterator it(root), end; it != end; ++it) {
    const auto attributes = GetFileAttributesW(it->path().c_str());
    Need(attributes != INVALID_FILE_ATTRIBUTES, "fixture inventory failed");
    const auto key = it->path().lexically_relative(root).generic_wstring();
    if (attributes & FILE_ATTRIBUTE_REPARSE_POINT) {
      it.disable_recursion_pending(); result[key] = "REPARSE";
    } else if (it->is_directory()) result[key] = "DIRECTORY";
    // The exporter holds the existing zero-byte writer lock exclusively. Its
    // size proves its empty content without opening that intentional lock.
    else result[key] = disk::file_size(it->path()) == 0 ? std::string{} : Read(it->path());
  }
  return result;
}
bool CanWrite(const disk::path &path) {
  const auto h = CreateFileW(path.c_str(), GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE |
      FILE_SHARE_DELETE, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
  if (h == INVALID_HANDLE_VALUE) return false;
  CloseHandle(h); return true;
}
std::string StateJson() {
  return "{\"schema\":\"mcla-diagnostic-state-v1\",\"reason\":\"f10\","
      "\"process\":{\"pid\":123,\"creation_time_100ns\":42,\"working_set_bytes\":100,"
      "\"handle_count\":2,\"native_thread_count\":1},"
      "\"window\":{\"logical_width\":1280,\"logical_height\":720,\"physical_width\":1280,"
      "\"physical_height\":720,\"focused\":true,\"fullscreen\":false},"
      "\"runtime\":{\"available\":true,\"title_id\":1414793208,\"guest_output_sequence\":100,\"guest_vblank_sequence\":200},"
      "\"graphics\":{\"backend\":1},\"native_threads\":[{\"id\":456,\"base_priority\":8,"
      "\"kernel_time_100ns\":3,\"user_time_100ns\":4,\"name\":\"" + std::string(kPrivate) + "\"}]}\n";
}
std::string Manifest(std::string_view state, bool bad_hash = false) {
  return "{\"schema\":\"mcla-diagnostic-package-v1\",\"kind\":\"live\",\"reason\":\"f10\","
      "\"created_utc\":\"20261011T120000Z\",\"mcla_version\":\"0.19.0.0\",\"platform\":\"windows\","
      "\"privacy\":{\"automatic_upload\":false,\"package_safe_to_share\":false,"
      "\"private_artifacts\":[\"log-tail.txt\",\"save-private\"]},"
      "\"capture\":{\"frame\":false,\"minidump\":false,\"save_found\":false,\"save_files\":0,\"save_bytes\":0},"
      "\"artifacts\":[{\"name\":\"state.json\",\"bytes\":" + std::to_string(state.size()) +
      ",\"sha256\":\"" + (bad_hash ? std::string(64, '0') : Sha256(state)) + "\",\"safe_to_share\":true},"
      "{\"name\":\"log-tail.txt\"},{\"name\":\"save-metadata.json\"},{\"name\":\"save-files-private.json\"}]}\n";
}
std::string Receipt(std::string_view profile = kProfile) {
  // Deliberately predates capabilities.txt; current preferences below differ.
  return "MCLA-MANAGED-SESSION-V1\nprofile " + std::string(profile) + "\nbuild " + kBuild +
      "\ngame " + kGame + "\ntiming stock\ngpu NVIDIA GeForce RTX 5060 Laptop GPU\nadapter 1\nscale 2"
      "\nvsync 1\nfullscreen 0\n";
}
struct Fixture {
  disk::path root, settings, profile, other, build, game, destination, session, package;
  Preferences preferences;
  Fixture(const disk::path &parent, bool state_file = true, bool bad_hash = false,
      bool writer_lock = true, std::string_view session_state = "complete\nexit 0\n",
      std::string_view marker_id = kProfile, std::string_view receipt_id = kProfile,
      bool with_capabilities = false) {
    root = parent / (L"case-" + std::to_wstring(++serial));
    settings = root/L"settings"; profile = root/L"profile"; other = root/L"other-profile";
    build = root/L"build"; game = root/L"game"; destination = root/L"exports";
    session = profile/L"sessions"/std::string(kSession);
    package = profile/L"diagnostics"/std::string(kSession)/L"live"/kPackage;
    for (const auto &p : {settings, other, build, game, destination, session, package,
        profile/L"user", package/L"save-private"}) disk::create_directories(p);
    Need(NewFile(profile/L"managed-profile.dat", "MCLA managed profile v1\n" + std::string(marker_id) + "\n"), "fixture marker");
    Need(NewFile(other/L"managed-profile.dat", "MCLA managed profile v1\n" + std::string(kOther) + "\n"), "fixture other marker");
    if (writer_lock) Need(NewFile(profile/L".mcla-writer.lock", ""), "fixture writer lock");
    Need(NewFile(other/L".mcla-writer.lock", ""), "fixture other lock");
    auto receipt = Receipt(receipt_id);
    if (with_capabilities) {
      receipt += "render_target 2560x1440\nguest_mode 1280x720\ncapability_policy 1\ncapability_record capabilities.txt\n";
      const auto capability = mcla::launcher::ResolveCapabilities(
          {kBuild, kGame, mcla::launcher::TimingMode::Stock, 1440, true});
      Need(capability.has_value(), "fixture capability plan");
      const auto record = mcla::launcher::SerializeCapabilities(*capability);
      Need(record.has_value() && NewFile(session/L"capabilities.txt", *record), "fixture capability record");
    }
    Need(NewFile(session/L"launch-receipt.txt", receipt), "fixture receipt");
    Need(NewFile(session/L"state.txt", session_state), "fixture session state");
    const auto state = StateJson();
    if (state_file) Need(NewFile(package/L"state.json", state), "fixture diagnostic state");
    Need(NewFile(package/L"manifest.json", Manifest(state, bad_hash)), "fixture manifest");
    for (const auto &p : {profile/L"user"/L"owner-save.bin", other/L"owner-secret.env",
        package/L"log-tail.txt", package/L"save-private"/L"save.bin", settings/L"settings-private.txt"})
      Need(NewFile(p, kPrivate), "fixture private sentinel");
    preferences.library.entries = {{std::string(kProfile), u"Synthetic private label", profile.u16string()},
        {std::string(kOther), u"Other synthetic label", other.u16string()}};
    preferences.library.selected_id = kProfile;
    preferences.build_root = build.u16string(); preferences.game_root = game.u16string();
    preferences.gpu_name = u"Different current GPU"; preferences.render_target = 720;
    preferences.fullscreen = true;
    Need(mcla::launcher::EncodePreferences(preferences).has_value(), "fixture preferences must encode");
  }
  std::expected<ex::ExportPlan, std::wstring> Prepare(const fs::DirectoryGuard &guard,
      std::wstring_view name = kPackage, std::string_view id = kSession) const {
    return ex::PrepareLiveExport(preferences, guard, id, name, destination.wstring());
  }
};

#ifdef MCLA_DIAGNOSTIC_EXPORT_TESTING
ex::ExportTestPhase injected_phase = ex::ExportTestPhase::ChildCreated;
std::wstring expected_hook_destination;
bool hook_called = false, hook_wrote = false;
void PlantExtraOutput(ex::ExportTestPhase phase, std::wstring_view destination) {
  if (phase != injected_phase) return;
  hook_called = true;
  // Never write to a callback-supplied path unless it is the precise previewed
  // fresh child of this test's independently created synthetic fixture.
  if (destination != expected_hook_destination) return;
  hook_wrote = NewFile(disk::path(destination)/L"unexpected-private.txt", kPrivate);
}
struct HookScope {
  HookScope(ex::ExportTestPhase phase, std::wstring destination) {
    injected_phase = phase; expected_hook_destination = std::move(destination);
    hook_called = false; hook_wrote = false;
    ex::SetExportTestHook(PlantExtraOutput);
  }
  ~HookScope() { ex::SetExportTestHook(nullptr); expected_hook_destination.clear(); }
};
#endif

void RejectedWithoutWrites(Fixture &f, const char *label,
    std::wstring_view package = kPackage, std::string_view session = kSession) {
  auto settings = fs::OpenDirectory(f.settings.wstring()); Need(settings.has_value(), "fixture settings guard");
  const auto before = Inventory(f.root);
  { const auto result = f.Prepare(*settings, package, session); Check(!result, label); }
  Check(Inventory(f.root) == before, "rejected preparation preserves complete fixture inventory and bytes");
}
void Run(const disk::path &root) {
  Check(Sha256("abc") == "BA7816BF8F01CFEA414140DE5DAE2223B00361A396177A9CB410FF61F20015AD",
      "fixture SHA256 has independent standard-vector oracle");
  Fixture good(root);
  auto settings = fs::OpenDirectory(good.settings.wstring()); Need(settings.has_value(), "settings guard");
  const auto before = Inventory(good.root);
  disk::path preview_destination;
  {
    auto plan = good.Prepare(*settings);
    Check(plan.has_value(), "legacy completed live session plans without game assets or current lease");
    if (!plan) { std::wcerr << plan.error() << '\n'; return; }
    preview_destination = plan->destination();
    Check(Inventory(good.root) == before && !disk::exists(preview_destination), "preview creates no files or output directory");
    Check(!plan->confirmation_token().empty() && !plan->report_json().empty(), "plan provides exact report and confirmation token");
    Check(plan->report_json().find(kPrivate) == std::string::npos &&
        plan->report_json().find(kProfile) == std::string::npos &&
        plan->report_json().find("Synthetic private label") == std::string::npos &&
        plan->report_json().find("Different current GPU") == std::string::npos &&
        plan->report_json().find("save.bin") == std::string::npos &&
        plan->report_json().find("NVIDIA GeForce RTX 5060 Laptop GPU") == std::string::npos,
        "public report excludes synthetic private payload, profile identity and current GPU preference");
    Check(plan->report_json().find(kBuild) != std::string::npos &&
        plan->report_json().find(kGame) != std::string::npos &&
        plan->report_json().find("NVIDIA") != std::string::npos,
        "public report uses historical build/game identity and only an enum GPU vendor");
    const auto expected_inventory = "MCLA-METADATA-EXPORT-V1\n" + Sha256(plan->report_json()) + " " +
        std::to_string(plan->report_json().size()) + " report.json\n";
    Check(plan->inventory_text() == expected_inventory, "inventory preview independently binds exact report bytes");
    for (const auto &path : {good.profile/L"managed-profile.dat", good.session/L"launch-receipt.txt",
        good.session/L"state.txt", good.package/L"manifest.json", good.package/L"state.json"}) {
      Check(!CanWrite(path), "plan pins consumed source against writes");
      Check(!MoveFileW(path.c_str(), (path.wstring()+L".replacement-attempt").c_str()), "plan pins consumed source against replacement");
    }
    Check(!good.Prepare(*settings), "second export cannot acquire selected profile while first plan owns it");
    Check(!ex::ConfirmExport(*plan, "wrong-confirmation-token"), "wrong confirmation token rejected");
    Check(Inventory(good.root) == before && !disk::exists(preview_destination), "wrong confirmation and competing preview write nothing");
  }
  Check(Inventory(good.root) == before && !disk::exists(preview_destination), "Cancel by destruction is zero-write");
  Check(CanWrite(good.package/L"state.json") && CanWrite(good.session/L"launch-receipt.txt"), "Cancel releases input read-pins");
  {
    auto plan = good.Prepare(*settings); Need(plan.has_value(), "positive preparation after Cancel");
    const auto report = plan->report_json(), receipt = plan->inventory_text();
    const auto destination = plan->destination();
    const auto token = plan->confirmation_token();
    Check(!ex::ConfirmExport(*plan, "wrong-token"), "wrong token does not consume a valid plan");
    const auto published = ex::ConfirmExport(*plan, token);
    Check(published && *published == destination, "correct confirmation publishes exactly previewed destination");
    if (published) {
      const Tree expected{{L"EXPORT-COMPLETE.txt", receipt}, {L"report.json", report}};
      Check(Inventory(destination) == expected, "export contains exactly two previewed files and no private originals");
      const auto exported = Inventory(destination);
      Check(!ex::ConfirmExport(*plan, token), "successful plan cannot publish twice");
      Check(Inventory(destination) == exported, "second confirmation preserves completed export");
    }
  }
  // Compare all original entries, ignoring only the separately owned exports subtree.
  const auto after = Inventory(good.root);
  bool original_unchanged = true;
  for (const auto &[name, bytes] : before) {
    const auto found = after.find(name);
    original_unchanged &= found != after.end() && found->second == bytes;
  }
  Check(original_unchanged, "successful export leaves all source/settings/private sentinels untouched");

  Fixture collision(root);
  auto collision_settings = fs::OpenDirectory(collision.settings.wstring()); Need(collision_settings.has_value(), "collision settings");
  {
    auto plan = collision.Prepare(*collision_settings); Need(plan.has_value(), "collision plan");
    const disk::path destination(plan->destination());
    Need(CreateDirectoryW(destination.c_str(), nullptr) != FALSE && NewFile(destination/L"keep.txt", kPrivate), "collision sentinel");
    const auto original = Inventory(collision.root);
    const auto token = plan->confirmation_token();
    Check(!ex::ConfirmExport(*plan, token), "new child collision refuses overwrite/adoption");
    Check(Inventory(collision.root) == original, "collision preserves existing child and all sources");
    Check(!ex::ConfirmExport(*plan, token), "failed correct-token attempt is consumed once");
  }
  Fixture missing_state(root, false); RejectedWithoutWrites(missing_state, "missing diagnostic state rejected");
  Fixture bad_hash(root, true, true); RejectedWithoutWrites(bad_hash, "state manifest hash mismatch rejected");
  Fixture no_lock(root, true, false, false); RejectedWithoutWrites(no_lock, "missing existing profile lock rejected");
  Check(!disk::exists(no_lock.profile/L".mcla-writer.lock"), "export never creates missing profile lock");
  Fixture incomplete(root, true, false, true, "running\npid 123\n"); RejectedWithoutWrites(incomplete, "running session rejected");
  Fixture bad_marker(root, true, false, true, "complete\nexit 0\n", kOther); RejectedWithoutWrites(bad_marker, "managed marker/profile mismatch rejected");
  Fixture bad_receipt(root, true, false, true, "complete\nexit 0\n", kProfile, kOther); RejectedWithoutWrites(bad_receipt, "receipt/profile mismatch rejected");
  Fixture pending(root);
  Need(NewFile(pending.profile/L"pending-operation.dat", "synthetic pending operation\n"), "pending operation fixture");
  RejectedWithoutWrites(pending, "profile pending operation rejected without repairing/deleting marker");
  Fixture capability(root, true, false, true, "complete\nexit 0\n", kProfile, kProfile, true);
  {
    auto guard = fs::OpenDirectory(capability.settings.wstring()); Need(guard.has_value(), "capability settings guard");
    const auto before_capability = Inventory(capability.root);
    {
      auto plan = capability.Prepare(*guard);
      Check(plan.has_value(), "completed session with optional capability record admits read-only preview");
      if (plan) {
        Check(!CanWrite(capability.session/L"capabilities.txt"), "optional capability file held against writes");
        Check(!MoveFileW((capability.session/L"capabilities.txt").c_str(),
            (capability.session/L"capabilities.replacement-attempt").c_str()), "optional capability held against replacement");
      }
      Check(Inventory(capability.root) == before_capability, "optional capability preview writes nothing");
    }
    Check(CanWrite(capability.session/L"capabilities.txt"), "Cancel releases optional capability pin");
    Check(Inventory(capability.root) == before_capability, "optional capability Cancel preserves source bytes");
  }
#ifdef MCLA_DIAGNOSTIC_EXPORT_TESTING
  for (const auto phase : {ex::ExportTestPhase::ChildCreated, ex::ExportTestPhase::ReportWritten}) {
    Fixture injected(root);
    auto guard = fs::OpenDirectory(injected.settings.wstring()); Need(guard.has_value(), "injection settings guard");
    const auto before_injection = Inventory(injected.root);
    auto plan = injected.Prepare(*guard); Need(plan.has_value(), "injection export plan");
    const disk::path destination(plan->destination());
    const auto report = plan->report_json(), token = plan->confirmation_token();
    {
      HookScope hook(phase, destination.wstring());
      const auto published = ex::ConfirmExport(*plan, token);
      Check(hook_called && hook_wrote, "exact owned-output interference hook executed");
      Check(!published, "unexpected output entry rejects publication");
    }
    Tree expected{{L"unexpected-private.txt", std::string(kPrivate)}};
    if (phase == ex::ExportTestPhase::ReportWritten) expected[L"report.json"] = report;
    Check(Inventory(destination) == expected,
        "partial output retained exactly, without completion marker or unrequested cleanup");
    Check(!disk::exists(destination/L"EXPORT-COMPLETE.txt"), "failed output topology never receives completion marker");
    if (phase == ex::ExportTestPhase::ChildCreated)
      Check(!disk::exists(destination/L"report.json"), "child interference rejected before first report write");
    const auto after_injection = Inventory(injected.root);
    bool preserved = true;
    for (const auto &[name, bytes] : before_injection) {
      const auto found = after_injection.find(name);
      preserved &= found != after_injection.end() && found->second == bytes;
    }
    Check(preserved, "injected publication failure preserves every original input");
    Check(!ex::ConfirmExport(*plan, token), "injected failure consumes correct confirmation once");
    Check(Inventory(destination) == expected, "reconfirmation cannot modify retained incomplete export");
  }
#endif
  Fixture syntax(root);
  for (const auto name : {L"../live-20261011T120000Z-123-1", L"live-20261011T120000Z-123-1.partial",
      L"crash-20261011T120000Z-123", L"live-20261011T120000Z-123", L"live-20261011T120000Z-123-1:stream",
      L"C:/outside", L"live-20261011T120000Z-0-1"})
    RejectedWithoutWrites(syntax, "unsupported/traversing package name rejected", name);
  RejectedWithoutWrites(syntax, "invalid session identifier rejected", kPackage, "../escape");
  RejectedWithoutWrites(syntax, "missing different session rejected", kPackage, "44444444444444444444444444444444");
  {
    auto guard = fs::OpenDirectory(syntax.settings.wstring()); Need(guard.has_value(), "overlap settings");
    for (const auto &destination : {syntax.profile, syntax.other, syntax.build, syntax.game, syntax.settings, syntax.root}) {
      const auto inventory = Inventory(syntax.root);
      Check(!ex::PrepareLiveExport(syntax.preferences, *guard, kSession, kPackage, destination.wstring()),
          "destination overlapping any protected root in either direction rejected");
      Check(Inventory(syntax.root) == inventory, "overlap rejection creates no child or input changes");
    }
    auto invalid = syntax.preferences; invalid.render_target = 999;
    const auto inventory = Inventory(syntax.root);
    Check(!ex::PrepareLiveExport(invalid, *guard, kSession, kPackage, syntax.destination.wstring()), "invalid preferences rejected");
    Check(Inventory(syntax.root) == inventory, "invalid preferences cause no writes");
  }
  Fixture ads(root);
  Need(NewFile(ads.package/L"state.json:private-stream", kPrivate), "ADS fixture");
  RejectedWithoutWrites(ads, "consumed file with alternate data stream rejected");
  Check(Read(ads.package/L"state.json:private-stream") == kPrivate, "rejected ADS bytes preserved");
  Fixture hardlink(root, false);
  Need(NewFile(hardlink.root/L"outside-state.json", StateJson()), "hardlink target fixture");
  Need(CreateHardLinkW((hardlink.package/L"state.json").c_str(), (hardlink.root/L"outside-state.json").c_str(), nullptr) != FALSE,
      "hardlinks must be supported by admitted fixture filesystem");
  RejectedWithoutWrites(hardlink, "hardlinked consumed metadata rejected");
  Check(Read(hardlink.root/L"outside-state.json") == StateJson(), "hardlink target untouched");
  Fixture reparse(root, false);
  Need(NewFile(reparse.root/L"outside-state.json", StateJson()), "reparse target fixture");
  if (CreateSymbolicLinkW((reparse.package/L"state.json").c_str(), (reparse.root/L"outside-state.json").c_str(), 2)) {
    RejectedWithoutWrites(reparse, "reparse consumed metadata rejected");
    Check(Read(reparse.root/L"outside-state.json") == StateJson(), "reparse target untouched");
  } else {
    const auto error = GetLastError();
    if (error == ERROR_PRIVILEGE_NOT_HELD) { ++skips; std::cout << "SKIP symlink fixture: Windows 1314; no reparse PASS\n"; }
    else Check(false, "unexpected symlink fixture failure (not Windows 1314)");
  }
}
} // namespace

int main() {
  try {
    std::array<unsigned char, 16> random{};
    Need(BCryptGenRandom(nullptr, random.data(), static_cast<ULONG>(random.size()), BCRYPT_USE_SYSTEM_PREFERRED_RNG) >= 0,
        "fixture random identity failed");
    std::wstring name = L"mcla-diagnostic-export-test-";
    for (const auto b : random) { name += L"0123456789abcdef"[b >> 4]; name += L"0123456789abcdef"[b & 15]; }
    const auto root = disk::temp_directory_path()/name;
    Need(CreateDirectoryW(root.c_str(), nullptr) != FALSE, "exclusive synthetic root creation failed");
    std::wcout << L"Preserved synthetic diagnostic-export fixture: " << root.wstring() << L'\n';
    Run(root);
    std::cout << "Diagnostic export native: " << checks << " checks, " << failures << " failures, " << skips << " skips\n";
    return failures ? 1 : 0;
  } catch (const std::exception &error) {
    std::cerr << "Synthetic diagnostic-export setup failed; preserve fixture: " << error.what() << '\n';
    return 2;
  }
}
