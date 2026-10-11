#pragma once

#include "mcla_launcher_profile_policy.h"

#include <array>
#include <span>

namespace mcla::launcher {

// Deterministic binary v1, all integers unsigned little-endian:
// magic[8]="MCLALIB1", library_version:u32, entry_count:u32,
// selected_id:(u32 byte_count, ASCII bytes), then each entry in list order:
// id:(u32 byte_count, ASCII bytes), label:(u32 UTF-16 units, UTF-16LE),
// root:(u32 UTF-16 units, UTF-16LE), layout:u32 (1=managed, 2=legacy/read-only).
// No padding, terminators, unknown fields or trailing bytes are accepted.
// This is storage, not trust/launch authorization; runtime safety gates remain.
inline constexpr std::array<uint8_t, 8> kLibraryMagic = {'M', 'C', 'L', 'A', 'L', 'I', 'B', '1'};
inline constexpr size_t kMaximumLibraryBytes = 256 * 1024;

enum class CodecErrorCode {
  TooLarge,
  Truncated,
  InvalidMagic,
  UnsupportedVersion,
  InvalidLength,
  InvalidCount,
  InvalidEncoding,
  InvalidModel,
  TrailingBytes,
};
struct CodecError {
  CodecErrorCode code;
  size_t offset = 0;
  std::optional<Issue> issue;
};

enum class StoredSelectionState { EmptyLibrary, Unselected, Dangling, Selected };

// Call on a validated/decoded model. Missing or dangling selection is a UI
// repair state, never an instruction to select the first entry automatically.
inline StoredSelectionState GetStoredSelectionState(const ProfileLibrary &library) {
  if (library.selected_id.empty())
    return library.entries.empty() ? StoredSelectionState::EmptyLibrary : StoredSelectionState::Unselected;
  for (const auto &entry : library.entries) {
    if (entry.id == library.selected_id) return StoredSelectionState::Selected;
  }
  return StoredSelectionState::Dangling;
}

namespace codec_detail {
inline bool ValidUtf16(std::u16string_view text) {
  for (size_t i = 0; i < text.size(); ++i) {
    if (text[i] >= 0xd800 && text[i] <= 0xdbff) {
      if (++i == text.size() || text[i] < 0xdc00 || text[i] > 0xdfff) return false;
    } else if (text[i] >= 0xdc00 && text[i] <= 0xdfff) {
      return false;
    }
  }
  return true;
}

inline std::optional<CodecError> ValidateStorageModel(const ProfileLibrary &library) {
  if (library.version != kLibraryVersion)
    return CodecError{CodecErrorCode::UnsupportedVersion, 8, std::nullopt};
  if (library.entries.size() > kMaximumProfiles)
    return CodecError{CodecErrorCode::InvalidCount, 12, std::nullopt};
  if (!library.selected_id.empty() && !ValidOpaqueId(library.selected_id))
    return CodecError{CodecErrorCode::InvalidModel, 16, Issue{IssueCode::InvalidId}};
  for (size_t i = 0; i < library.entries.size(); ++i) {
    const auto &entry = library.entries[i];
    if (entry.id.size() > 32 || entry.label.size() > kMaximumLabelUnits || entry.root.size() > kMaximumPathUnits)
      return CodecError{CodecErrorCode::InvalidLength, 0, std::nullopt};
    if (!ValidUtf16(entry.label) || !ValidUtf16(entry.root))
      return CodecError{CodecErrorCode::InvalidEncoding, 0, std::nullopt};
  }
  if (library.entries.empty()) return std::nullopt;
  // Strict launch policy requires a valid selection. Validate only the entry
  // inventory using a private copy, preserving the original repair state.
  auto inventory = library;
  inventory.selected_id = inventory.entries.front().id;
  if (const auto issue = ValidateLibrary(inventory))
    return CodecError{CodecErrorCode::InvalidModel, 0, *issue};
  return std::nullopt;
}

inline void AppendU32(std::vector<uint8_t> &bytes, uint32_t value) {
  for (unsigned shift = 0; shift < 32; shift += 8)
    bytes.push_back(static_cast<uint8_t>(value >> shift));
}
inline void AppendAscii(std::vector<uint8_t> &bytes, std::string_view text) {
  AppendU32(bytes, static_cast<uint32_t>(text.size()));
  for (const auto c : text) bytes.push_back(static_cast<uint8_t>(c));
}
inline void AppendUtf16(std::vector<uint8_t> &bytes, std::u16string_view text) {
  AppendU32(bytes, static_cast<uint32_t>(text.size()));
  for (const auto c : text) {
    bytes.push_back(static_cast<uint8_t>(c));
    bytes.push_back(static_cast<uint8_t>(c >> 8));
  }
}

struct Reader {
  std::span<const uint8_t> bytes;
  size_t offset = 8;
  std::expected<uint32_t, CodecError> U32() {
    if (bytes.size() - offset < 4)
      return std::unexpected(CodecError{CodecErrorCode::Truncated, offset, std::nullopt});
    uint32_t value = 0;
    for (unsigned shift = 0; shift < 32; shift += 8)
      value |= static_cast<uint32_t>(bytes[offset++]) << shift;
    return value;
  }
  std::expected<std::string, CodecError> Ascii() {
    const auto start = offset;
    const auto count = U32();
    if (!count) return std::unexpected(count.error());
    if (*count > 32)
      return std::unexpected(CodecError{CodecErrorCode::InvalidLength, start, std::nullopt});
    if (*count > bytes.size() - offset)
      return std::unexpected(CodecError{CodecErrorCode::Truncated, offset, std::nullopt});
    std::string value;
    value.reserve(*count);
    for (uint32_t i = 0; i < *count; ++i) {
      const auto c = bytes[offset++];
      if (c > 0x7f)
        return std::unexpected(CodecError{CodecErrorCode::InvalidEncoding, offset - 1, std::nullopt});
      value.push_back(static_cast<char>(c));
    }
    return value;
  }
  std::expected<std::u16string, CodecError> Utf16(size_t maximum) {
    const auto start = offset;
    const auto count = U32();
    if (!count) return std::unexpected(count.error());
    if (*count > maximum)
      return std::unexpected(CodecError{CodecErrorCode::InvalidLength, start, std::nullopt});
    // Division avoids overflow even for hostile lengths.
    if (*count > (bytes.size() - offset) / 2)
      return std::unexpected(CodecError{CodecErrorCode::Truncated, offset, std::nullopt});
    std::u16string value;
    value.reserve(*count);
    for (uint32_t i = 0; i < *count; ++i) {
      const auto low = bytes[offset++];
      const auto high = bytes[offset++];
      value.push_back(static_cast<char16_t>(static_cast<uint16_t>(low) |
                                           (static_cast<uint16_t>(high) << 8)));
    }
    if (!ValidUtf16(value))
      return std::unexpected(CodecError{CodecErrorCode::InvalidEncoding, start + 4, std::nullopt});
    return value;
  }
};
} // namespace codec_detail

inline std::expected<std::vector<uint8_t>, CodecError> EncodeLibrary(const ProfileLibrary &library) {
  if (const auto error = codec_detail::ValidateStorageModel(library)) return std::unexpected(*error);
  std::vector<uint8_t> bytes(kLibraryMagic.begin(), kLibraryMagic.end());
  codec_detail::AppendU32(bytes, library.version);
  codec_detail::AppendU32(bytes, static_cast<uint32_t>(library.entries.size()));
  codec_detail::AppendAscii(bytes, library.selected_id);
  for (const auto &entry : library.entries) {
    codec_detail::AppendAscii(bytes, entry.id);
    codec_detail::AppendUtf16(bytes, entry.label);
    codec_detail::AppendUtf16(bytes, entry.root);
    codec_detail::AppendU32(bytes, entry.layout == Layout::ManagedExternalV1 ? 1 : 2);
  }
  if (bytes.size() > kMaximumLibraryBytes)
    return std::unexpected(CodecError{CodecErrorCode::TooLarge, 0, std::nullopt});
  return bytes;
}

inline std::expected<ProfileLibrary, CodecError> DecodeLibrary(std::span<const uint8_t> bytes) {
  if (bytes.size() > kMaximumLibraryBytes)
    return std::unexpected(CodecError{CodecErrorCode::TooLarge, 0, std::nullopt});
  if (bytes.size() < kLibraryMagic.size())
    return std::unexpected(CodecError{CodecErrorCode::Truncated, 0, std::nullopt});
  for (size_t i = 0; i < kLibraryMagic.size(); ++i) {
    if (bytes[i] != kLibraryMagic[i])
      return std::unexpected(CodecError{CodecErrorCode::InvalidMagic, i, std::nullopt});
  }
  codec_detail::Reader reader{bytes};
  const auto version = reader.U32();
  if (!version) return std::unexpected(version.error());
  if (*version != kLibraryVersion)
    return std::unexpected(CodecError{CodecErrorCode::UnsupportedVersion, 8, std::nullopt});
  const auto count = reader.U32();
  if (!count) return std::unexpected(count.error());
  if (*count > kMaximumProfiles)
    return std::unexpected(CodecError{CodecErrorCode::InvalidCount, 12, std::nullopt});
  auto selected = reader.Ascii();
  if (!selected) return std::unexpected(selected.error());
  ProfileLibrary library{*version, {}, std::move(*selected)};
  library.entries.reserve(*count);
  for (uint32_t i = 0; i < *count; ++i) {
    auto id = reader.Ascii();
    if (!id) return std::unexpected(id.error());
    auto label = reader.Utf16(kMaximumLabelUnits);
    if (!label) return std::unexpected(label.error());
    auto root = reader.Utf16(kMaximumPathUnits);
    if (!root) return std::unexpected(root.error());
    const auto layout_offset = reader.offset;
    auto layout = reader.U32();
    if (!layout) return std::unexpected(layout.error());
    if (*layout != 1 && *layout != 2)
      return std::unexpected(CodecError{CodecErrorCode::InvalidModel, layout_offset, Issue{IssueCode::InvalidLayout, i}});
    library.entries.push_back({std::move(*id), std::move(*label), std::move(*root),
                               *layout == 1 ? Layout::ManagedExternalV1 : Layout::LegacyBundleUserReadOnlyV1});
  }
  if (reader.offset != bytes.size())
    return std::unexpected(CodecError{CodecErrorCode::TrailingBytes, reader.offset, std::nullopt});
  if (const auto error = codec_detail::ValidateStorageModel(library)) return std::unexpected(*error);
  return library;
}

} // namespace mcla::launcher
