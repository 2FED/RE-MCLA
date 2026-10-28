#include "mcla_launcher_preferences.h"
#include <iostream>

namespace {
using namespace mcla::launcher;
// Independent fixture of the pre-render-target preferences layout. The library
// codec is unchanged; this helper intentionally does not call EncodePreferences.
std::vector<uint8_t> Fixture(const Preferences &p, uint8_t version, uint32_t raw) {
  const auto library = EncodeLibrary(p.library);
  if (!library) return {};
  std::vector<uint8_t> bytes{'M','C','L','A','P','R','F',version};
  codec_detail::AppendU32(bytes, static_cast<uint32_t>(library->size()));
  bytes.insert(bytes.end(), library->begin(), library->end());
  codec_detail::AppendUtf16(bytes, p.build_root);
  codec_detail::AppendUtf16(bytes, p.game_root);
  codec_detail::AppendUtf16(bytes, p.gpu_name);
  codec_detail::AppendU32(bytes, raw);
  codec_detail::AppendU32(bytes, p.vsync ? 1 : 0);
  codec_detail::AppendU32(bytes, p.fullscreen ? 1 : 0);
  return bytes;
}
}

int main() {
  using namespace mcla::launcher;
  unsigned count = 0, failures = 0;
  auto check = [&](bool ok) { ++count; if (!ok) ++failures; };
  Preferences p;
  check(p.render_target == 720);
  auto bytes = EncodePreferences(p); check(bytes.has_value());
  if (!bytes) return 1;
  check(*bytes == Fixture(p, '1', 1));
  auto parsed = DecodePreferences(*bytes); check(parsed && *parsed == p);
  const std::array<RenderTargetPlan, 4> expected_targets{{
      {720,1280,720,1280,720,1}, {1080,1920,1080,960,540,2},
      {1440,2560,1440,1280,720,2}, {2160,3840,2160,1280,720,3}}};
  check(kRenderTargets.size() == expected_targets.size());
  for (const auto &expected : expected_targets) {
    const auto *actual = FindRenderTarget(expected.id);
    check(actual && actual->width == expected.width && actual->height == expected.height &&
          actual->guest_width == expected.guest_width && actual->guest_height == expected.guest_height &&
          actual->scale == expected.scale);
  }
  p.build_root = u"C:/Build with spaces"; p.game_root = u"D:/\u0413\u0440\u0430";
  p.gpu_name = u"NVIDIA GPU"; p.fullscreen = true;
  p.library.entries.push_back({"11111111111111111111111111111111", u"MAIN", u"E:/Saves/MAIN", Layout::ManagedExternalV1});
  p.library.selected_id = p.library.entries.front().id;
  for (const auto &target : kRenderTargets) {
    check(FindRenderTarget(target.id) == &target);
    check(target.guest_width * target.scale == target.width && target.guest_height * target.scale == target.height);
    p.render_target = target.id;
    bytes = EncodePreferences(p); check(bytes.has_value());
    if (!bytes) return 1;
    const bool legacy = target.id == 720 || target.id == 1440;
    check((*bytes)[7] == (legacy ? '1' : '2'));
    check(*bytes == Fixture(p, legacy ? '1' : '2', legacy ? (target.id == 720 ? 1 : 2) : target.id));
    parsed = DecodePreferences(*bytes); check(parsed && *parsed == p);
    // V2 decodes all four explicit IDs, including representable legacy choices.
    parsed = DecodePreferences(Fixture(p, '2', target.id)); check(parsed && *parsed == p);
    if (parsed) { const auto recoded = EncodePreferences(*parsed); check(recoded && *recoded == *bytes); }
    for (size_t i = 0; i < bytes->size(); ++i)
      check(!DecodePreferences(std::span(*bytes).first(i)));
    auto bad = *bytes; bad.push_back(0); check(!DecodePreferences(bad));
    bad = *bytes; bad.back() = 1; check(!DecodePreferences(bad));
    bad = *bytes; bad[8] = 0xff; bad[9] = 0xff; bad[10] = 0xff; bad[11] = 0xff;
    check(!DecodePreferences(bad));
    bad = *bytes; bad[0] = 'X'; check(!DecodePreferences(bad));
    bad = *bytes; bad[7] = '3'; check(!DecodePreferences(bad));
  }
  // M9-003: unlocked timing is V3 only (V2 layout plus one timing code).
  p.timing = TimingMode::Unlocked;
  for (const auto &target : kRenderTargets) {
    p.render_target = target.id;
    bytes = EncodePreferences(p); check(bytes.has_value());
    if (!bytes) return 1;
    auto expected = Fixture(p, '3', target.id);
    codec_detail::AppendU32(expected, kUnlockedTimingCode);
    check((*bytes)[7] == '3' && *bytes == expected);
    parsed = DecodePreferences(*bytes); check(parsed && *parsed == p && parsed->timing == TimingMode::Unlocked);
    for (size_t i = 0; i < bytes->size(); ++i)
      check(!DecodePreferences(std::span(*bytes).first(i)));
    auto bad = *bytes; bad.push_back(0); check(!DecodePreferences(bad));
    for (const uint32_t code : {0u, 1u, 3u, 0xffffffffu}) {
      bad = Fixture(p, '3', target.id); codec_detail::AppendU32(bad, code);
      check(!DecodePreferences(bad));
    }
    bad = *bytes; bad[7] = '2'; check(!DecodePreferences(bad));
  }
  p.render_target = 720;
  for (const auto invalid : {TimingMode::Fixed60, static_cast<TimingMode>(3), static_cast<TimingMode>(0xFF)}) {
    p.timing = invalid; check(!EncodePreferences(p));
  }
  // Switching back to 30 FPS restores the exact stock encoding.
  p.timing = TimingMode::Stock;
  bytes = EncodePreferences(p);
  check(bytes && *bytes == Fixture(p, '1', 1));
  for (const uint32_t legacy_scale : {1u, 2u}) {
    p.render_target = legacy_scale == 1 ? 720 : 1440;
    const auto legacy = Fixture(p, '1', legacy_scale);
    parsed = DecodePreferences(legacy);
    check(parsed && *parsed == p);
    if (parsed) { const auto recoded = EncodePreferences(*parsed); check(recoded && *recoded == legacy); }
  }
  for (const uint32_t invalid : {0u, 3u, 720u, 1080u, 1440u, 2160u, 0xffffffffu})
    check(!DecodePreferences(Fixture(p, '1', invalid)));
  for (const uint32_t invalid : {0u, 1u, 2u, 3u, 1081u, 4320u, 0xffffffffu}) {
    check(!FindRenderTarget(invalid));
    check(!DecodePreferences(Fixture(p, '2', invalid)));
    p.render_target = invalid; check(!EncodePreferences(p));
  }
  p.render_target = 720; p.build_root = u"relative"; check(!EncodePreferences(p));
  p.build_root.clear(); p.gpu_name = u"bad\nGPU"; check(!EncodePreferences(p));
  std::vector<uint8_t> too_large(kMaximumPreferencesBytes + 1);
  check(!DecodePreferences(too_large));
  std::cout << "Preferences: " << count << " checks, " << failures << " failures\n";
  return failures ? 1 : 0;
}
