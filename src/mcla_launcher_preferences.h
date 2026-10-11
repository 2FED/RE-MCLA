#pragma once

#include "mcla_launcher_library_codec.h"
#include <algorithm>

namespace mcla::launcher {
struct Preferences {
  ProfileLibrary library;
  std::u16string build_root, game_root, gpu_name;
  uint32_t resolution_scale = 1;
  bool vsync = true, fullscreen = false;
  bool operator==(const Preferences &) const = default;
};
inline constexpr size_t kMaximumPreferencesBytes = kMaximumLibraryBytes + 8192;
inline bool ValidPreferences(const Preferences &p) {
  if (!p.build_root.empty() && !NormalizeRoot(p.build_root)) return false;
  if (!p.game_root.empty() && !NormalizeRoot(p.game_root)) return false;
  if (p.gpu_name.size() > 200 || !detail::SafeUnicode(p.gpu_name)) return false;
  return p.resolution_scale == 1 || p.resolution_scale == 2;
}
inline std::expected<std::vector<uint8_t>, CodecError> EncodePreferences(const Preferences &p) {
  auto library = EncodeLibrary(p.library);
  if (!library) return std::unexpected(library.error());
  if (!ValidPreferences(p)) return std::unexpected(CodecError{CodecErrorCode::InvalidModel});
  std::vector<uint8_t> bytes{'M','C','L','A','P','R','F','1'};
  codec_detail::AppendU32(bytes, static_cast<uint32_t>(library->size()));
  bytes.insert(bytes.end(), library->begin(), library->end());
  codec_detail::AppendUtf16(bytes, p.build_root);
  codec_detail::AppendUtf16(bytes, p.game_root);
  codec_detail::AppendUtf16(bytes, p.gpu_name);
  codec_detail::AppendU32(bytes, p.resolution_scale);
  codec_detail::AppendU32(bytes, p.vsync ? 1 : 0);
  codec_detail::AppendU32(bytes, p.fullscreen ? 1 : 0);
  return bytes;
}
inline std::expected<Preferences, CodecError> DecodePreferences(std::span<const uint8_t> bytes) {
  constexpr std::array<uint8_t,8> magic{'M','C','L','A','P','R','F','1'};
  if (bytes.size() > kMaximumPreferencesBytes)
    return std::unexpected(CodecError{CodecErrorCode::TooLarge});
  if (bytes.size() < magic.size())
    return std::unexpected(CodecError{CodecErrorCode::Truncated});
  if (!std::equal(magic.begin(), magic.end(), bytes.begin()))
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
  auto scale = r.U32(), vsync = r.U32(), fullscreen = r.U32();
  if (!scale || !vsync || !fullscreen) return std::unexpected(CodecError{CodecErrorCode::Truncated});
  if (*vsync > 1 || *fullscreen > 1 || r.offset != bytes.size())
    return std::unexpected(CodecError{CodecErrorCode::InvalidModel});
  Preferences p{std::move(*library), std::move(*build), std::move(*game), std::move(*gpu),
                *scale, *vsync == 1, *fullscreen == 1};
  if (!ValidPreferences(p)) return std::unexpected(CodecError{CodecErrorCode::InvalidModel});
  return p;
}
} // namespace mcla::launcher
