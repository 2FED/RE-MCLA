#include "mcla_launcher_library_codec.h"

#include <iostream>

namespace {
using namespace mcla::launcher;
unsigned checks = 0;
unsigned failures = 0;
void Check(bool value, const char *label) {
  ++checks;
  if (!value) { ++failures; std::cerr << "FAIL " << label << '\n'; }
}
constexpr std::string_view main_id = "1234567890abcdef1234567890abcdef";
constexpr std::string_view clean_id = "2234567890abcdef1234567890abcdef";
constexpr std::string_view stale_id = "3234567890abcdef1234567890abcdef";
ProfileLibrary Library() {
  return {1, {{std::string(main_id), u"MAIN \u041c\u0456\u0439 \U0001F3CE", u"D:\\Saves\\\u0413\u0440\u0430", Layout::ManagedExternalV1},
              {std::string(clean_id), u"Clean", u"E:/Tests/clean/user", Layout::LegacyBundleUserReadOnlyV1}},
          std::string(main_id)};
}
void PutU32(std::vector<uint8_t> &bytes, size_t offset, uint32_t value) {
  for (unsigned i = 0; i < 4; ++i) bytes.at(offset + i) = static_cast<uint8_t>(value >> (i * 8));
}
void ExpectDecode(const std::vector<uint8_t> &bytes, CodecErrorCode code) {
  const auto result = DecodeLibrary(bytes);
  Check(!result && result.error().code == code, "typed decode rejection");
}
void ExpectEncode(const ProfileLibrary &library, CodecErrorCode code) {
  const auto result = EncodeLibrary(library);
  Check(!result && result.error().code == code, "typed encode rejection");
}
void Roundtrip(const ProfileLibrary &library) {
  const auto before = library;
  const auto encoded = EncodeLibrary(library);
  Check(encoded.has_value(), "encode accepted model");
  if (!encoded) return;
  const auto decoded = DecodeLibrary(*encoded);
  Check(decoded && *decoded == before, "lossless model roundtrip");
  Check(library == before, "codec leaves input model untouched");
  Check(EncodeLibrary(library).value() == *encoded, "deterministic bytes");
  if (decoded) Check(EncodeLibrary(*decoded).value() == *encoded, "canonical re-encode");
}
} // namespace

int main() {
  using namespace mcla::launcher;
  const auto library = Library();
  Roundtrip(library);
  auto repair = library; repair.selected_id.clear();
  Roundtrip(repair);
  Check(GetStoredSelectionState(repair) == StoredSelectionState::Unselected, "unselected repair state");
  Check(ValidateLibrary(repair).has_value(), "unselected cannot launch");
  repair.selected_id = stale_id;
  Roundtrip(repair);
  Check(GetStoredSelectionState(repair) == StoredSelectionState::Dangling, "dangling repair state");
  Check(ValidateLibrary(repair).has_value(), "dangling cannot launch");
  ProfileLibrary empty;
  Roundtrip(empty);
  Check(GetStoredSelectionState(empty) == StoredSelectionState::EmptyLibrary, "empty setup state");
  empty.selected_id = stale_id;
  Roundtrip(empty);
  Check(GetStoredSelectionState(empty) == StoredSelectionState::Dangling, "empty with stale selection retained");
  Check(GetStoredSelectionState(library) == StoredSelectionState::Selected, "selected state");
  auto legacy = library; legacy.selected_id = clean_id;
  Roundtrip(legacy);
  const auto legacy_plan = MakeLaunchPlan(legacy, {u"C:/Build/MCLA", u"F:/Game/MCLA"});
  Check(!legacy_plan && legacy_plan.error().code == IssueCode::LegacyReadOnly, "storage never promotes legacy");

  const auto encoded = EncodeLibrary(library);
  if (!encoded) return 1;
  const auto bytes = *encoded;
  for (size_t size = 0; size < bytes.size(); ++size)
    Check(!DecodeLibrary(std::span<const uint8_t>(bytes.data(), size)), "every truncation rejected");
  auto bad = bytes; bad.push_back(0); ExpectDecode(bad, CodecErrorCode::TrailingBytes);
  bad = bytes; bad[0] ^= 0xff; ExpectDecode(bad, CodecErrorCode::InvalidMagic);
  bad = bytes; PutU32(bad, 8, 2); ExpectDecode(bad, CodecErrorCode::UnsupportedVersion);
  bad = bytes; PutU32(bad, 12, 0xffffffff); ExpectDecode(bad, CodecErrorCode::InvalidCount);
  bad = bytes; PutU32(bad, 16, 0xffffffff); ExpectDecode(bad, CodecErrorCode::InvalidLength);
  bad = bytes; bad[20] = 0xff; ExpectDecode(bad, CodecErrorCode::InvalidEncoding);
  bad = bytes; bad[20] = 'g'; ExpectDecode(bad, CodecErrorCode::InvalidModel);
  ExpectDecode(std::vector<uint8_t>(kMaximumLibraryBytes + 1), CodecErrorCode::TooLarge);

  // Exact wire offsets for this fixture, independent of the encoder helpers.
  const size_t id_length = 20 + main_id.size();
  const size_t id_bytes = id_length + 4;
  const size_t label_length = id_bytes + main_id.size();
  const size_t label_bytes = label_length + 4;
  const size_t root_length = label_bytes + library.entries[0].label.size() * 2;
  const size_t root_bytes = root_length + 4;
  const size_t layout_offset = root_bytes + library.entries[0].root.size() * 2;
  const size_t second_id_length = layout_offset + 4;
  bad = bytes; PutU32(bad, id_length, 33); ExpectDecode(bad, CodecErrorCode::InvalidLength);
  bad = bytes; PutU32(bad, label_length, 81); ExpectDecode(bad, CodecErrorCode::InvalidLength);
  bad = bytes; PutU32(bad, root_length, 1025); ExpectDecode(bad, CodecErrorCode::InvalidLength);
  bad = bytes; PutU32(bad, layout_offset, 0); ExpectDecode(bad, CodecErrorCode::InvalidModel);
  bad = bytes; bad[label_bytes] = 0; bad[label_bytes + 1] = 0xd8;
  ExpectDecode(bad, CodecErrorCode::InvalidEncoding);
  bad = bytes; bad[root_bytes] = 0; bad[root_bytes + 1] = 0xdc;
  ExpectDecode(bad, CodecErrorCode::InvalidEncoding);
  bad = bytes;
  for (size_t i = 0; i < main_id.size(); ++i) bad[second_id_length + 4 + i] = static_cast<uint8_t>(main_id[i]);
  ExpectDecode(bad, CodecErrorCode::InvalidModel);
  auto same_lengths = library;
  same_lengths.entries[0].root = u"D:/Data/aaa";
  same_lengths.entries[1].root = u"E:/Data/bbb";
  auto duplicate_root = EncodeLibrary(same_lengths).value();
  const size_t first_root_offset = root_bytes;
  const size_t first_layout_offset = first_root_offset + same_lengths.entries[0].root.size() * 2;
  const size_t second_root_offset = first_layout_offset + 4 + 4 + clean_id.size() +
      4 + same_lengths.entries[1].label.size() * 2 + 4;
  for (size_t i = 0; i < same_lengths.entries[0].root.size() * 2; ++i)
    duplicate_root[second_root_offset + i] = duplicate_root[first_root_offset + i];
  ExpectDecode(duplicate_root, CodecErrorCode::InvalidModel);
  // A byte change can be another valid label/root; this codec is not an
  // authenticity checksum and must not claim all corruption is detectable.
  bad = bytes; bad[label_bytes] = 'N';
  Check(DecodeLibrary(bad).has_value(), "valid changed text remains data, not authenticated identity");

  auto invalid = library; invalid.version = 0; ExpectEncode(invalid, CodecErrorCode::UnsupportedVersion);
  invalid = library; invalid.entries.resize(kMaximumProfiles + 1); ExpectEncode(invalid, CodecErrorCode::InvalidCount);
  invalid = library; invalid.selected_id = "MAIN"; ExpectEncode(invalid, CodecErrorCode::InvalidModel);
  invalid = library; invalid.entries[0].id = std::string(33, 'a'); ExpectEncode(invalid, CodecErrorCode::InvalidLength);
  invalid = library; invalid.entries[0].label = std::u16string(81, u'a'); ExpectEncode(invalid, CodecErrorCode::InvalidLength);
  invalid = library; invalid.entries[0].root = std::u16string(1025, u'a'); ExpectEncode(invalid, CodecErrorCode::InvalidLength);
  invalid = library; invalid.entries[0].label = std::u16string(1, 0xd800); ExpectEncode(invalid, CodecErrorCode::InvalidEncoding);
  invalid = library; invalid.entries[0].root = std::u16string(1, 0xdc00); ExpectEncode(invalid, CodecErrorCode::InvalidEncoding);
  invalid = library; invalid.entries[0].layout = static_cast<Layout>(3); ExpectEncode(invalid, CodecErrorCode::InvalidModel);
  invalid = library; invalid.entries[1].id = invalid.entries[0].id; ExpectEncode(invalid, CodecErrorCode::InvalidModel);
  invalid = library; invalid.entries[1].root = invalid.entries[0].root; ExpectEncode(invalid, CodecErrorCode::InvalidModel);
  invalid = library; invalid.entries[1].root = invalid.entries[0].root + u"/nested"; ExpectEncode(invalid, CodecErrorCode::InvalidModel);

  ProfileLibrary maximum;
  for (size_t i = 1; i <= kMaximumProfiles; ++i) {
    const auto number = std::to_string(i);
    const auto id = std::string(32 - number.size(), '0') + number;
    std::u16string root = u"D:/Profiles/p";
    for (const auto c : number) root.push_back(static_cast<char16_t>(c));
    maximum.entries.push_back({id, std::u16string(kMaximumLabelUnits, u'a'), root, Layout::ManagedExternalV1});
  }
  Roundtrip(maximum);

  const auto empty_bytes = EncodeLibrary(ProfileLibrary{});
  Check(empty_bytes && *empty_bytes == std::vector<uint8_t>({'M','C','L','A','L','I','B','1',1,0,0,0,0,0,0,0,0,0,0,0}), "golden empty v1 bytes");
  std::cout << "Launcher library codec: " << checks << " checks, " << failures << " failures\n";
  return failures ? 1 : 0;
}
