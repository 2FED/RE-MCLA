#include "mcla_launcher_import_policy.h"

#include <iostream>

namespace {
using namespace mcla::launcher;
using Json = nlohmann::json;
unsigned checks = 0, failures = 0;
const std::string id = "mcla-0.18.0.0-win64-AAAAAAAAAAAAAAAA-BBBBBBBBBBBBBBBB-CCCCCCCCCCCCCCCC";
const std::string hash_a(64, 'A'), hash_b(64, 'B'), hash_c(64, 'C'), hash_d(64, 'D');
void Check(bool value, const char *label) {
  ++checks;
  if (!value) { ++failures; std::cerr << "FAIL " << label << '\n'; }
}
Json Manifest() {
  return {
    {"schema", "mcla-portable-campaign-bundle-v2"}, {"task", "M7-016"}, {"bundle_id", id},
    {"project_version", "0.18.0.0"}, {"sdk_version", "0.19.0.0"}, {"sdk_commit", std::string(40, 'a')},
    {"created_utc", "2026-10-10T12:09:04.5528351Z"}, {"platform", "windows-proton"}, {"architecture", "x86_64"},
    {"launcher", "Launch-MCLA.exe"}, {"cpp_runtime", {{"policy", "msvc-14.44-app-local-v1"}, {"version", "14.44.35211.0"}}},
    {"window_mode", {{"startup_fullscreen", true}, {"alt_enter_toggle", true}, {"left_double_click_toggle", true}}},
    {"immutable_file_count", 33}, {"game", {{"manifest_sha256", hash_a}, {"file_count", 15}, {"payload_bytes", 6569586392ULL}}},
    {"selected_save", {{"source_snapshot", "private/save-archive/M7-009/" + hash_d},
       {"save_relative", "B13EBABEBABEBABE/545407F8/00000001/mc4.sav/mc4.sav"},
       {"header_relative", "B13EBABEBABEBABE/545407F8/Headers/00000001/mc4.sav.header"},
       {"save_sha256", hash_c}, {"header_sha256", hash_a}, {"profile_sha256", hash_d}, {"ancestor_profile_sha256", Json::array()}}},
    {"writable_roots", {"user", "cache", "logs", "diagnostics", "results", "update", "metadata"}},
    {"retention", {{"completed_sessions", 32}, {"save_snapshots_per_session", 32}}},
    {"private_content", {{"game_and_save_private", true}, {"source_iso_included", false}}},
    {"image_capture", {{"mode", "window-wgc-v1"}, {"helper_path", "bin/mcla_diagnostic_image_helper.exe"},
       {"helper_sha256", hash_b}, {"helper_bytes", 61440}, {"host_sha256", hash_a}}}
  };
}
void Reject(const Json &manifest, const char *label) {
  Check(!ValidateLegacyMetadata(id, manifest.dump()), label);
}
} // namespace

int main() {
  const auto original = Manifest();
  const auto before = original.dump();
  const auto good = ValidateLegacyMetadata(" \t" + id + "\r\n", before);
  if (!good) std::cerr << "Metadata diagnostic: " << good.error() << '\n';
  Check(good && *good == id, "valid metadata returns trimmed ID");
  Check(original.dump() == before, "validation leaves metadata unchanged");
  auto manifest = original;
  manifest["sdk_version"] = "0.20.0.0";
  Check(ValidateLegacyMetadata(id, manifest.dump()).has_value(), "legacy recognition accepts newer version-shaped SDK metadata");
  manifest = original; manifest["selected_save"]["save_sha256"] = hash_b;
  Check(ValidateLegacyMetadata(id, manifest.dump()).has_value(), "historical seed hash not treated as current live save");
  manifest = original; manifest.erase("image_capture"); manifest["immutable_file_count"] = 32;
  Check(ValidateLegacyMetadata(id, manifest.dump()).has_value(), "non-WGC legacy layout accepted");
  manifest = original; manifest["selected_save"]["source_snapshot"] = "private/save-archive/M6-014/final-hour-example/snapshot-1";
  manifest["selected_save"].erase("profile_sha256"); manifest["selected_save"].erase("ancestor_profile_sha256");
  Check(ValidateLegacyMetadata(id, manifest.dump()).has_value(), "legacy M6 source without full-profile metadata accepted");
  manifest = original; manifest["created_utc"] = "2024-02-29T23:59:59Z";
  Check(ValidateLegacyMetadata(id, manifest.dump()).has_value(), "valid leap date without fractions");

  for (const auto &bad_id : {std::string{}, std::string("../") + id, id + "\ninside", id + "/tail", id + ":stream",
                            std::string(129, 'a'), std::string(257, 'a'), std::string("\xEF\xBB\xBF") + id})
    Check(!ValidateLegacyMetadata(bad_id, before), "unsafe/missing/oversized ID rejected");
  Check(!ValidateLegacyMetadata(id + "-other", before), "ID file/manifest mismatch");
  Check(!ValidateLegacyMetadata(std::string(id + '\0'), before), "ID embedded NUL rejected");
  for (const auto *text : {"", "{}", "[]", "null", "false", "1", "{", "{\"schema\":", "{} {}"})
    Check(!ValidateLegacyMetadata(id, text), "malformed/wrong-root/trailing JSON rejected");
  Check(!ValidateLegacyMetadata(id, std::string(kMaximumLegacyManifestBytes + 1, ' ')), "JSON byte bound");
  Check(!ValidateLegacyMetadata(id, before + std::string("\0", 1)), "JSON trailing NUL rejected");
  manifest = original; manifest["schema"] = "mcla-portable-campaign-bundle-v1"; Reject(manifest, "old schema explicit rejection");
  manifest = original; manifest["schema"] = "mcla-portable-campaign-bundle-v3"; Reject(manifest, "unknown schema explicit rejection");
  manifest = original; manifest["unknown"] = true; Reject(manifest, "unknown top-level key rejected");
  manifest = original; manifest.erase("game"); Reject(manifest, "missing required field rejected");
  manifest = original; manifest["task"] = "M8-001"; Reject(manifest, "unknown task rejected");
  manifest = original; manifest["bundle_id"] = false; Reject(manifest, "ID type mismatch rejected");
  manifest = original; manifest["project_version"] = "0.18.1.0"; Reject(manifest, "project/ID version mismatch");
  manifest = original; manifest["sdk_version"] = "0.20.0"; Reject(manifest, "invalid SDK version");
  manifest = original; manifest["sdk_commit"] = std::string(40, 'A'); Reject(manifest, "commit case/shape rejected");
  manifest = original; manifest["platform"] = "linux"; Reject(manifest, "unknown platform rejected");
  manifest = original; manifest["architecture"] = "arm64"; Reject(manifest, "unknown architecture rejected");
  manifest = original; manifest["launcher"] = "../Launch-MCLA.exe"; Reject(manifest, "launcher path not exact");
  for (const auto *timestamp : {"2026-02-29T12:00:00Z", "2026-10-10T24:00:00Z", "2026-10-10T12:00:60Z",
                              "2026-10-10T12:00:00", "2026-10-10T12:00:00+03:00", "2026-10-10T12:00:00.Z"}) {
    manifest = original; manifest["created_utc"] = timestamp; Reject(manifest, "malformed/non-UTC timestamp");
  }
  manifest = original; manifest["cpp_runtime"]["policy"] = "unknown"; Reject(manifest, "unknown runtime policy");
  manifest = original; manifest["window_mode"]["startup_fullscreen"] = 1; Reject(manifest, "Boolean is not integer");
  manifest = original; manifest["game"]["file_count"] = 15.0; Reject(manifest, "file count float rejected");
  manifest = original; manifest["game"]["file_count"] = -15; Reject(manifest, "negative count rejected");
  manifest = original; manifest["game"]["payload_bytes"] = 1; Reject(manifest, "unsupported game size");
  manifest = original; manifest["game"]["unknown"] = 1; Reject(manifest, "unknown game metadata field");
  manifest = original; manifest["selected_save"]["save_relative"] = "B13EBABEBABEBABE/DEADBEEF/00000001/mc4.sav/mc4.sav"; Reject(manifest, "unsupported title ID");
  manifest = original; manifest["selected_save"]["header_relative"] = "../mc4.sav.header"; Reject(manifest, "unsafe header route");
  manifest = original; manifest["selected_save"]["save_sha256"] = "bad"; Reject(manifest, "invalid seed hash shape");
  manifest = original; manifest["selected_save"]["source_snapshot"] = "private/save-archive/M6-014/../escape"; Reject(manifest, "unsafe provenance route");
  manifest = original; manifest["selected_save"].erase("profile_sha256"); Reject(manifest, "M7 source requires historical full-profile identity");
  manifest = original; manifest["selected_save"]["ancestor_profile_sha256"] = {hash_a, hash_a}; Reject(manifest, "duplicate ancestors");
  manifest = original; manifest["selected_save"]["ancestor_profile_sha256"] = {hash_d}; Reject(manifest, "self ancestor");
  manifest = original; manifest["selected_save"]["ancestor_profile_sha256"] = std::vector<std::string>(257, hash_a); Reject(manifest, "ancestry count bound");
  manifest = original; manifest["writable_roots"].push_back("elsewhere"); Reject(manifest, "root layout drift");
  manifest = original; manifest["retention"]["completed_sessions"] = 33; Reject(manifest, "retention contract drift");
  manifest = original; manifest["private_content"]["source_iso_included"] = true; Reject(manifest, "ISO-included layout rejected");
  manifest = original; manifest["immutable_file_count"] = 32; Reject(manifest, "image inventory count mismatch");
  manifest = original; manifest["image_capture"]["helper_path"] = "bin/other.exe"; Reject(manifest, "unknown helper path");
  manifest = original; manifest["image_capture"]["helper_bytes"] = 67108865; Reject(manifest, "helper size bound");

  auto duplicate = before;
  duplicate.insert(1, "\"schema\":\"mcla-portable-campaign-bundle-v2\",");
  Check(!ValidateLegacyMetadata(id, duplicate), "duplicate root key rejected before overwrite");
  duplicate = before; duplicate.insert(1, "\"\\u0073chema\":\"mcla-portable-campaign-bundle-v2\",");
  Check(!ValidateLegacyMetadata(id, duplicate), "escaped duplicate key rejected");
  duplicate = before;
  const std::string marker = "\"game\":{";
  duplicate.insert(duplicate.find(marker) + marker.size(), "\"file_count\":15,");
  Check(!ValidateLegacyMetadata(id, duplicate), "duplicate nested key rejected");
  const auto deep = std::string(40, '[') + "0" + std::string(40, ']');
  Check(!ValidateLegacyMetadata(id, deep), "depth bound before schema validation");
  const auto many = Json(std::vector<int>(9000, 0)).dump();
  Check(!ValidateLegacyMetadata(id, many), "parser event bound");
  auto invalid_utf8 = before; invalid_utf8.insert(1, "\"\xFF\":1,");
  Check(!ValidateLegacyMetadata(id, invalid_utf8), "malformed UTF8 rejected");
  std::cout << "Launcher legacy metadata policy: " << checks << " checks, " << failures << " failures\n";
  return failures ? 1 : 0;
}
