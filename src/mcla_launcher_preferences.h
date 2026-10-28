#pragma once

#include "mcla_launcher_library_codec.h"
#include "mcla_render_target_policy.h"
#include <algorithm>

namespace mcla::launcher {
struct Preferences {
  ProfileLibrary library;
  std::u16string build_root, game_root, gpu_name;
  uint32_t render_target = 720;
  bool vsync = true, fullscreen = false;
  TimingMode timing = TimingMode::Stock;
  bool operator==(const Preferences &) const = default;
};
inline constexpr uint32_t kUnlockedTimingCode = 2;
inline constexpr size_t kMaximumPreferencesBytes = kMaximumLibraryBytes + 8192;
inline bool ValidPreferences(const Preferences &p) {
  if (!p.build_root.empty() && !NormalizeRoot(p.build_root)) return false;
  if (!p.game_root.empty() && !NormalizeRoot(p.game_root)) return false;
  if (p.gpu_name.size() > 200 || !detail::SafeUnicode(p.gpu_name)) return false;
  if (p.timing != TimingMode::Stock && p.timing != TimingMode::Unlocked) return false;
  return FindRenderTarget(p.render_target) != nullptr;
}
inline std::expected<std::vector<uint8_t>, CodecError> EncodePreferences(const Preferences &p) {
  auto library = EncodeLibrary(p.library);
  if (!library) return std::unexpected(library.error());
  if (!ValidPreferences(p)) return std::unexpected(CodecError{CodecErrorCode::InvalidModel});
  // Keep old representable choices byte-for-byte V1 compatible. New choices
  // require V2; old launchers reject that magic rather than misreading a scale.
  // Unlocked timing requires V3 (trailing timing code), so a launcher without
  // the mode refuses the file instead of silently starting stock.
  const bool unlocked = p.timing == TimingMode::Unlocked;
  const bool legacy = !unlocked && (p.render_target == 720 || p.render_target == 1440);
  std::vector<uint8_t> bytes{'M','C','L','A','P','R','F', static_cast<uint8_t>(unlocked ? '3' : legacy ? '1' : '2')};
  codec_detail::AppendU32(bytes, static_cast<uint32_t>(library->size()));
  bytes.insert(bytes.end(), library->begin(), library->end());
  codec_detail::AppendUtf16(bytes, p.build_root);
  codec_detail::AppendUtf16(bytes, p.game_root);
  codec_detail::AppendUtf16(bytes, p.gpu_name);
  codec_detail::AppendU32(bytes, legacy ? (p.render_target == 720 ? 1u : 2u) : p.render_target);
  codec_detail::AppendU32(bytes, p.vsync ? 1 : 0);
  codec_detail::AppendU32(bytes, p.fullscreen ? 1 : 0);
  if (unlocked) codec_detail::AppendU32(bytes, kUnlockedTimingCode);
  return bytes;
}
inline std::expected<Preferences, CodecError> DecodePreferences(std::span<const uint8_t> bytes) {
  constexpr std::array<uint8_t,8> magic{'M','C','L','A','P','R','F','1'};
  if (bytes.size() > kMaximumPreferencesBytes)
    return std::unexpected(CodecError{CodecErrorCode::TooLarge});
  if (bytes.size() < magic.size())
    return std::unexpected(CodecError{CodecErrorCode::Truncated});
  if (!std::equal(magic.begin(), magic.end() - 1, bytes.begin()) ||
      (bytes[7] != '1' && bytes[7] != '2' && bytes[7] != '3'))
    return std::unexpected(CodecError{CodecErrorCode::InvalidMagic});
  codec_detail::Reader r{bytes};
  auto n = r.U32();
  if (!n) return std::unexpected(n.error());
  if (*n > kMaximumLibraryBytes || *n > bytes.size() - r.offset)
    return std::unexpected(CodecError{CodecErrorCode::InvalidLength});
  auto library = DecodeLibrary(bytes.subspan(r.offset, *n));
  if (!library) return std::unexpected(library.error());
  r.offset += *n;
  auto build = r.Utf16(kMaximumPathUnits), game = r.Utf16(kMaximumPathUnits), gpu = r.Utf16(200);
  if (!build || !game || !gpu) return std::unexpected(CodecError{CodecErrorCode::InvalidLength});
  auto stored_target = r.U32(), vsync = r.U32(), fullscreen = r.U32();
  if (!stored_target || !vsync || !fullscreen) return std::unexpected(CodecError{CodecErrorCode::Truncated});
  TimingMode timing = TimingMode::Stock;
  if (bytes[7] == '3') {
    // Only unlocked is encoded as V3; a stock V3 would not be canonical.
    auto code = r.U32();
    if (!code) return std::unexpected(CodecError{CodecErrorCode::Truncated});
    if (*code != kUnlockedTimingCode) return std::unexpected(CodecError{CodecErrorCode::InvalidModel});
    timing = TimingMode::Unlocked;
  }
  if (*vsync > 1 || *fullscreen > 1 || r.offset != bytes.size())
    return std::unexpected(CodecError{CodecErrorCode::InvalidModel});
  uint32_t target = *stored_target;
  if (bytes[7] == '1') {
    if (*stored_target != 1 && *stored_target != 2)
      return std::unexpected(CodecError{CodecErrorCode::InvalidModel});
    target = *stored_target == 1 ? 720 : 1440;
  }
  Preferences p{std::move(*library), std::move(*build), std::move(*game), std::move(*gpu),
                target, *vsync == 1, *fullscreen == 1, timing};
  if (!ValidPreferences(p)) return std::unexpected(CodecError{CodecErrorCode::InvalidModel});
  return p;
}
} // namespace mcla::launcher
