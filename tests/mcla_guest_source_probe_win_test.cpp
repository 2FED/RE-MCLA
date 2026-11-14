// No game, window, input, GPU, Presenter instance or capture call is created by
// this test. Including the implementation exposes its private serialization and
// exclusive-file seams without adding a public production testing API.
// Compile this TU (not the module again) with MCLA_GUEST_SOURCE_PROBE=1 and the
// provenance SDK headers. Link launcher filesystem, rex core/UI (the included
// but uncalled Run references Presenter::GetGuestOutputSequence), their normal
// dependencies, bcrypt, and nlohmann_json headers. C++23 required.
// Runtime creates ONE uniquely owned temp directory and preserves it; no delete,
// overwrite, shell child, environment/config loading or existing save access.
#include "../src/mcla_guest_source_probe_win.cpp"

#include <bcrypt.h>
#include <nlohmann/json.hpp>

#include <iostream>
#include <span>

REXCVAR_DEFINE_BOOL(mcla_source_test_peer_probe, false, "Test", "Synthetic peer");
REXCVAR_DEFINE_BOOL(mcla_source_test_nonprobe, false, "Test", "Not a probe");

namespace {
namespace probe = mcla::guest_source_probe;
namespace fs = mcla::launcher::fs;
using JsonValue = nlohmann::json;
unsigned checks = 0, failures = 0;
constexpr auto token = "0123456789abcdef0123456789abcdef";

void Check(bool ok, const char* name) {
  ++checks;
  if (!ok) { ++failures; std::cerr << "FAIL " << name << '\n'; }
}
template <typename F> void Fails(F&& f, const char* reason, const char* name) {
  bool expected = false;
  try { f(); }
  catch (const probe::Failure& e) { expected = std::string_view(e.reason) == reason; }
  catch (...) {}
  Check(expected, name);
}
void TokenAndRegistry() {
  Check(!probe::Enabled(), "probe is disabled at static default");
  probe::ValidateExclusiveConfig("not-a-backend");  // Disabled is a no-op.
  Check(probe::ValidToken(token), "32 lowercase hexadecimal characters");
  Check(probe::ValidToken(std::string(32, '0')), "token is not a nonce-strength claim");
  for (const auto& value : {std::string(), std::string(31, 'a'), std::string(33, 'a'),
       std::string(32, 'A'), std::string(32, 'g'), std::string(32, ' '),
       std::string(31, 'a') + '\0', std::string(31, 'a') + '"',
       std::string(31, 'a') + '\n', std::string(31, 'a') + char(0xff)})
    Check(!probe::ValidToken(value), "invalid token rejected");
  Check(!rex::cvar::IsFinalized(), "test registry remains initialization-only");
  Check(rex::cvar::SetFlagByName("mcla_source_capture_token", token), "set token through real registry");
  Check(rex::cvar::SetFlagByName("mcla_source_capture_probe", "true"), "enable through real registry");
  Check(probe::Enabled(), "registry enabled probe");
  probe::ValidateExclusiveConfig("d3d12");
  Fails([] { probe::ValidateExclusiveConfig("vulkan"); }, "D3D12_REQUIRED", "other backend rejected");
  REXCVAR_SET(mcla_source_capture_token, "bad");
  Fails([] { probe::ValidateExclusiveConfig("d3d12"); }, "INVALID_CAPTURE_TOKEN", "bad enabled token rejected");
  REXCVAR_SET(mcla_source_capture_token, token);
  REXCVAR_SET(mcla_source_test_peer_probe, true);
  Fails([] { probe::ValidateExclusiveConfig("d3d12"); }, "OTHER_PROBE_ENABLED_OR_INVALID", "peer probe rejected");
  REXCVAR_SET(mcla_source_test_peer_probe, false);
  REXCVAR_SET(mcla_source_test_nonprobe, true);
  probe::ValidateExclusiveConfig("d3d12");  // Unrelated settings are not probes.
  auto& registry = rex::cvar::GetRegistry();
  const auto it = std::find_if(registry.begin(), registry.end(), [](const auto& e) {
    return e.name == "mcla_source_test_peer_probe";
  });
  Check(it != registry.end(), "synthetic peer has registry entry");
  if (it != registry.end()) {
    const auto original = *it;
    it->type = rex::cvar::FlagType::String;
    Fails([] { probe::ValidateExclusiveConfig("d3d12"); }, "UNRECOGNIZED_PROBE_FLAG", "nonboolean probe rejected");
    *it = original;
    it->getter = {};
    Fails([] { probe::ValidateExclusiveConfig("d3d12"); }, "UNRECOGNIZED_PROBE_FLAG", "missing getter rejected");
    *it = original;
    it->getter = [] { return std::string("0"); };
    Fails([] { probe::ValidateExclusiveConfig("d3d12"); }, "OTHER_PROBE_ENABLED_OR_INVALID", "noncanonical false rejected");
    *it = original;
  }
  REXCVAR_SET(mcla_source_test_nonprobe, false);
  REXCVAR_SET(mcla_source_capture_probe, false);
  REXCVAR_SET(mcla_source_capture_token, "");
  Check(!probe::Enabled(), "test restores disabled state");
}

void ReceiptChecks() {
  probe::Receipt receipt;
  receipt.token = token;
  const auto absent = JsonValue::parse(probe::Json(receipt));
  Check(absent.at("schema") == "mcla-guest-source-capture-v1", "receipt schema");
  Check(absent.at("status") == "FAILED" && absent.at("capture_calls") == 0,
        "uncompleted receipt fails with zero attempts");
  Check(absent.at("capture").is_null() && absent.at("image_state") == "absent",
        "no capture metadata for absent image");
  for (const auto* key : {"display_proven", "simulation_proven", "fps_proven", "scene_admitted",
       "source_binding_independently_reviewed", "keyboard_or_controller_injected", "desktop_capture"})
    Check(absent.at(key).is_boolean() && !absent.at(key).get<bool>(), "receipt denies unproven admission");
  rex::ui::GuestOutputCapture capture;
  capture.image.width = 2; capture.image.height = 2; capture.image.stride = 8;
  capture.image.data.resize(16);
  constexpr uint64_t large = 9007199254741001ull;  // Greater than double exact-integer range.
  capture.provenance = {17, 61, 10000000, large, large + 1, large - 1,
                        large + 2, large + 3, large + 4, large + 5};
  Check(rex::ui::ValidGuestCaptureProvenance(capture.provenance, 2, 2, 8, 16),
        "synthetic paired metadata is structurally valid");
  receipt.accepted = &capture;
  receipt.capture_calls = 1;
  receipt.readiness_sequence = 60;
  receipt.readiness_wait_ms = 4321;
  receipt.image_state = "complete";
  receipt.image_bytes = 70;
  receipt.status = "SOURCE_CAPTURE_COLLECTED_REVIEW_PENDING";
  receipt.reason = "ONE_PAIRED_GUEST_SOURCE_RESULT_NO_DISPLAY_OR_TIMING_CLAIM";
  const auto full = JsonValue::parse(probe::Json(receipt));
  Check(full.at("token") == token && full.at("image_file") == "frame.bmp", "receipt correlation and fixed filename");
  Check(full.at("readiness_sequence_not_pixel_identity") == 60 &&
        full.at("capture").at("acquired_sequence") == 61, "readiness does not replace paired identity");
  Check(full.at("readiness_wait_ms") == 4321 && full.at("capture_calls") == 1 &&
        full.at("image_bytes_written") == 70, "outer diagnostic values preserved");
  const JsonValue expected_capture = {
      {"clock", "win32-query-performance-counter"}, {"pixel_format", "R8G8B8X8"},
      {"file_format", "24bit-BGR-bottom-up-BMP"}, {"width", 2}, {"height", 2},
      {"stride", 8}, {"raw_bytes", 16}, {"presenter_epoch", 17}, {"acquired_sequence", 61},
      {"qpc_frequency", 10000000}, {"refresh_cpu_begin_qpc", large},
      {"refresh_cpu_end_qpc", large + 1}, {"acquire_before_qpc", large - 1},
      {"acquire_after_qpc", large + 2}, {"copy_submit_before_qpc", large + 3},
      {"copy_completion_observed_qpc", large + 4}, {"pixels_ready_qpc", large + 5}};
  Check(full.at("capture") == expected_capture, "all paired fields exact, raw ticks not converted or rounded");
  for (const auto* key : {"display_proven", "simulation_proven", "fps_proven", "scene_admitted",
       "source_binding_independently_reviewed", "keyboard_or_controller_injected", "desktop_capture"})
    Check(full.at(key) == false, "collected receipt still grants no admission");
  receipt.status = "FAILED"; receipt.reason = "STOPPED_DURING_IMAGE_WRITE";
  receipt.image_state = "partial"; receipt.image_bytes = 54;
  const auto partial = JsonValue::parse(probe::Json(receipt));
  Check(partial.at("status") == "FAILED" && partial.at("image_state") == "partial" &&
        partial.at("image_bytes_written") == 54 && partial.at("capture") == expected_capture,
        "failed image can retain paired metadata without acceptance");
}

std::vector<uint8_t> ExpectedBmp(const rex::ui::RawImage& image) {
  const auto row_bytes = ((image.width * 3 + 3) / 4) * 4;
  std::vector<uint8_t> bytes(54 + row_bytes * image.height, 0);
  auto u32 = [&](size_t at, uint32_t value) {
    for (unsigned i = 0; i != 4; ++i) bytes.at(at + i) = uint8_t(value >> (i * 8));
  };
  bytes[0] = 'B'; bytes[1] = 'M'; u32(2, static_cast<uint32_t>(bytes.size()));
  u32(10, 54); u32(14, 40); u32(18, image.width); u32(22, image.height);
  bytes[26] = 1; bytes[28] = 24; u32(34, row_bytes * image.height);
  for (uint32_t y = 0; y < image.height; ++y)
    for (uint32_t x = 0; x < image.width; ++x)
      for (unsigned c = 0; c < 3; ++c)
        bytes.at(54 + (image.height - y - 1) * row_bytes + 3 * x + c) =
            image.data.at(y * image.stride + x * 4 + 2 - c);
  return bytes;
}

void FileChecks(const fs::DirectoryGuard& directory) {
  for (uint32_t width = 1; width <= 4; ++width) {
    rex::ui::RawImage image;
    image.width = width; image.height = 2; image.stride = width * 4;
    image.data.resize(image.stride * image.height);
    for (size_t i = 0; i < image.data.size(); ++i) image.data[i] = uint8_t(i * 7 + 3);
    const auto name = L"width-" + std::to_wstring(width) + L".bmp";
    probe::ExclusiveFile output;
    output.Open(directory, name.c_str());
    Check(output.WasCreated() && output.IsOpen() && output.bytes() == 0, "new file bookkeeping");
    probe::WriteBmp(output, image, {});
    const auto expected = ExpectedBmp(image);
    Check(!output.IsOpen() && output.bytes() == expected.size(), "BMP flush/close and exact byte count");
    auto actual = fs::ReadFile(directory, name, 1024);
    Check(actual && *actual == expected, "entire BMP: bottom-up RGBX-to-BGR plus every padding byte");
    probe::ExclusiveFile duplicate;
    Fails([&] { duplicate.Open(directory, name.c_str()); }, "EXCLUSIVE_FILE_CREATE_FAILED", "finished file cannot be overwritten");
    Check(!duplicate.WasCreated() && !duplicate.IsOpen() && duplicate.bytes() == 0,
          "rejected existing file not recorded as newly created");
    actual = fs::ReadFile(directory, name, 1024);
    Check(actual && *actual == expected, "rejected overwrite preserves every existing byte");
    Fails([&] { output.Write("x", 1); }, "FILE_NOT_OPEN", "write after finish rejected");
    Fails([&] { output.Finish(); }, "FILE_NOT_OPEN", "finish after finish rejected");
  }
  {
    probe::ExclusiveFile partial;
    partial.Open(directory, L"partial.bmp");
    Fails([&] { partial.Open(directory, L"unused.bmp"); }, "FILE_ALREADY_OPEN", "cannot switch open output");
    std::stop_source cancelled;
    cancelled.request_stop();
    rex::ui::RawImage image;
    image.width = 1; image.height = 1; image.stride = 4; image.data = {1, 2, 3, 99};
    Fails([&] { probe::WriteBmp(partial, image, cancelled.get_token()); },
          "STOPPED_DURING_IMAGE_WRITE", "stop rejects image before first row");
    Check(partial.IsOpen() && partial.WasCreated() && partial.bytes() == 54,
          "partial header retained/accounted, not falsely marked finished");
  }  // Closing by destruction must not delete or rewrite partial evidence.
  auto partial_bytes = fs::ReadFile(directory, L"partial.bmp", 1024);
  Check(partial_bytes && partial_bytes->size() == 54, "partial file survives close");
  Check(GetFileAttributesW((directory.path() + L"\\unused.bmp").c_str()) == INVALID_FILE_ATTRIBUTES,
        "rejected second open creates nothing");
  {
    probe::ExclusiveFile too_large;
    too_large.Open(directory, L"oversize-rejected.bmp");
    rex::ui::RawImage image;
    image.width = image.height = 16384;  // Size rejected before any pixel dereference.
    Fails([&] { probe::WriteBmp(too_large, image, {}); }, "BMP_SIZE_FAILED", "oversize rejected before serialization");
    Check(too_large.WasCreated() && too_large.bytes() == 0, "empty failed file bookkeeping");
  }
  auto empty = fs::ReadFile(directory, L"oversize-rejected.bmp", 1024);
  Check(empty && empty->empty(), "empty rejected artifact preserved");
}
}  // namespace

int main(int argc, char**) {
  if (argc != 1) { std::cerr << "Unexpected arguments\n"; return 2; }
  try {
    TokenAndRegistry();
    ReceiptChecks();
    wchar_t temp[32768]{};
    const auto length = GetTempPathW(static_cast<DWORD>(std::size(temp)), temp);
    if (!length || length >= std::size(temp)) return 2;
    std::array<uint8_t, 16> random{};
    if (BCryptGenRandom(nullptr, random.data(), static_cast<ULONG>(random.size()),
                        BCRYPT_USE_SYSTEM_PREFERRED_RNG) < 0) return 2;
    std::wstring root(temp, length);
    root += L"mcla-guest-source-serialization-test-";
    constexpr wchar_t hex[] = L"0123456789abcdef";
    for (auto value : random) { root += hex[value >> 4]; root += hex[value & 15]; }
    if (!CreateDirectoryW(root.c_str(), nullptr)) return 2;
    std::wcout << L"Owned synthetic serialization fixture (preserved): " << root << L'\n';
    auto directory = fs::OpenDirectory(root);
    if (!directory) { std::wcerr << directory.error() << L'\n'; return 2; }
    FileChecks(*directory);
  } catch (const std::exception& error) {
    std::cerr << "Unhandled test exception: " << error.what() << '\n';
    return 2;
  }
  std::cout << "guest source serialization checks=" << checks << " failed=" << failures
            << " (no native capture, no timing/FPS admission)\n";
  return failures ? 1 : 0;
}
