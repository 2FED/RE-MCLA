#pragma once

// M10-003 step 1: local union of the SDK shader stores (launcher seeding).
// See docs/evidence/M10-prewarm-union-store-20261101.md.
//
// The SDK keeps, per cache root, <cache_root>/shaders/shareable/:
// - 545407F8.xsh: header "XESH" + version 0x20201219 (big-endian), then per
//   guest shader a 12-byte record header (XXH3-64 of the ucode, 31-bit dword
//   count, 1-bit type) followed by the raw Xenos ucode dwords;
// - 545407F8.{rtv,rov}.d3d12.xpso: header "XEPS" + "DXRT"/"DXRO" + version
//   max(PipelineDescription 0x20210425, DXBC Modification 0x20260226)
//   (big-endian), then 72-byte records (XXH3-64 of the 64-byte description,
//   then the description);
// - 545407F8.{fbo,fsi}.vk.xpso: header "XEPS" + 0/1 + version
//   max(0x20260228, SPIR-V Modification 12), then 68-byte records.
// The SDK reads each file up to the first record whose hash does not match and
// discards a file whose header differs from its own constants.
//
// This header is pure (no filesystem): it parses and validates those files
// exactly and merges the records of several stores of one family into one
// file, deduplicated (shaders by ucode hash, descriptions by their full
// bytes). The constants must follow the SDK; a mismatch is fail-safe (the
// SDK discards a seeded file it does not recognise and starts cold).
// The merged .xsh holds game data (ucode) and stays on this machine.

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#ifndef XXH_INLINE_ALL
#define XXH_INLINE_ALL
#endif
#include "xxhash.h"

namespace mcla::shader_store {

inline constexpr uint32_t kTitleId = 0x545407F8;
inline constexpr uint32_t kShaderMagic = 0x48534558;    // "XESH"
inline constexpr uint32_t kPipelineMagic = 0x53504558;  // "XEPS"
inline constexpr uint32_t kShaderVersion = 0x20201219;
inline constexpr uint32_t kD3d12PipelineVersion = 0x20260226;
inline constexpr uint32_t kVulkanPipelineVersion = 0x20260228;
inline constexpr size_t kShaderRecordHeaderBytes = 12;
// Largest ucode accepted per shader (the SDK reserves 0xFFFF dwords; the
// owner's largest is under 700).
inline constexpr uint32_t kMaxUcodeDwords = 0x10000;
// Bound of one store file, read or written.
inline constexpr size_t kMaxFileBytes = 16 * 1024 * 1024;

enum class Family : uint8_t { kShaders, kD3d12Rtv, kD3d12Rov, kVulkanFbo, kVulkanFsi };
inline constexpr size_t kFamilyCount = 5;

struct FamilyInfo {
  std::wstring_view file;  // name in shaders/shareable
  std::string_view label;
  size_t header_bytes;
  uint32_t api;            // the .xpso magic_api word (unused for shaders)
  uint32_t version;        // stored big-endian
  size_t record_bytes;     // fixed record size; 0 for shaders (variable)
};

inline constexpr std::array<FamilyInfo, kFamilyCount> kFamilies = {{
    {L"545407F8.xsh", "xsh", 8, 0, kShaderVersion, 0},
    {L"545407F8.rtv.d3d12.xpso", "rtv.d3d12", 12, 0x54525844, kD3d12PipelineVersion, 72},
    {L"545407F8.rov.d3d12.xpso", "rov.d3d12", 12, 0x4F525844, kD3d12PipelineVersion, 72},
    {L"545407F8.fbo.vk.xpso", "fbo.vk", 12, 0, kVulkanPipelineVersion, 68},
    {L"545407F8.fsi.vk.xpso", "fsi.vk", 12, 1, kVulkanPipelineVersion, 68},
}};

constexpr const FamilyInfo &Info(Family family) { return kFamilies[static_cast<size_t>(family)]; }

namespace detail {
inline uint32_t LoadLe32(const uint8_t *p) {
  return uint32_t(p[0]) | uint32_t(p[1]) << 8 | uint32_t(p[2]) << 16 | uint32_t(p[3]) << 24;
}
inline uint64_t LoadLe64(const uint8_t *p) {
  return uint64_t(LoadLe32(p)) | uint64_t(LoadLe32(p + 4)) << 32;
}
inline uint32_t LoadBe32(const uint8_t *p) {
  return uint32_t(p[3]) | uint32_t(p[2]) << 8 | uint32_t(p[1]) << 16 | uint32_t(p[0]) << 24;
}
inline void StoreLe32(std::vector<uint8_t> &out, uint32_t v) {
  for (int i = 0; i < 4; ++i) out.push_back(uint8_t(v >> (8 * i)));
}
inline void StoreBe32(std::vector<uint8_t> &out, uint32_t v) {
  for (int i = 3; i >= 0; --i) out.push_back(uint8_t(v >> (8 * i)));
}
}  // namespace detail

inline uint64_t Hash(std::span<const uint8_t> bytes) { return XXH3_64bits(bytes.data(), bytes.size()); }

// The header the SDK writes for the family.
inline std::vector<uint8_t> Header(Family family) {
  const auto &info = Info(family);
  std::vector<uint8_t> out;
  detail::StoreLe32(out, family == Family::kShaders ? kShaderMagic : kPipelineMagic);
  if (family != Family::kShaders) detail::StoreLe32(out, info.api);
  detail::StoreBe32(out, info.version);
  return out;
}

enum class HeaderStatus : uint8_t {
  kValid,
  kEmpty,       // zero bytes (the SDK treats it as a new file)
  kShort,       // shorter than the header
  kBadMagic,    // not this format, or another family (rtv/rov, d3d12/vk)
  kBadVersion,  // the same family written by another format version
};

constexpr std::string_view HeaderStatusName(HeaderStatus status) {
  switch (status) {
    case HeaderStatus::kValid: return "valid";
    case HeaderStatus::kEmpty: return "empty";
    case HeaderStatus::kShort: return "short";
    case HeaderStatus::kBadMagic: return "bad-magic";
    case HeaderStatus::kBadVersion: return "bad-version";
  }
  return "unknown";
}

struct Record {
  size_t offset = 0;  // in the parsed bytes
  size_t size = 0;
  uint64_t hash = 0;  // ucode hash (shaders) or description hash
};

struct Parsed {
  HeaderStatus header = HeaderStatus::kEmpty;
  std::vector<Record> records;  // the valid prefix, in file order
  size_t valid_bytes = 0;       // header + valid records
  bool stopped = false;         // bytes after the valid prefix (bad or partial record)
};

// Exact SDK reading rules: the header must equal Header(family); records are
// read until the end or the first truncated, oversized or hash-mismatching
// record (that record and everything after it are dropped).
inline Parsed Parse(Family family, std::span<const uint8_t> bytes) {
  Parsed parsed;
  const auto &info = Info(family);
  if (bytes.empty()) return parsed;
  if (bytes.size() < info.header_bytes) {
    parsed.header = HeaderStatus::kShort;
    return parsed;
  }
  const auto expected = Header(family);
  // Magic and api words first: a different family is never "another version".
  if (!std::equal(expected.begin(), expected.end() - 4, bytes.begin())) {
    parsed.header = HeaderStatus::kBadMagic;
    return parsed;
  }
  if (detail::LoadBe32(bytes.data() + info.header_bytes - 4) != info.version) {
    parsed.header = HeaderStatus::kBadVersion;
    return parsed;
  }
  parsed.header = HeaderStatus::kValid;
  size_t offset = info.header_bytes;
  while (offset < bytes.size()) {
    const size_t left = bytes.size() - offset;
    Record record{offset, 0, 0};
    if (family == Family::kShaders) {
      if (left < kShaderRecordHeaderBytes) break;
      const uint32_t dwords = detail::LoadLe32(bytes.data() + offset + 8) & 0x7FFFFFFFu;
      if (dwords > kMaxUcodeDwords) break;
      const size_t ucode = size_t(dwords) * 4;
      if (left - kShaderRecordHeaderBytes < ucode) break;
      record.size = kShaderRecordHeaderBytes + ucode;
      record.hash = detail::LoadLe64(bytes.data() + offset);
      if (Hash(bytes.subspan(offset + kShaderRecordHeaderBytes, ucode)) != record.hash) break;
    } else {
      if (left < info.record_bytes) break;
      record.size = info.record_bytes;
      record.hash = detail::LoadLe64(bytes.data() + offset);
      if (Hash(bytes.subspan(offset + 8, info.record_bytes - 8)) != record.hash) break;
    }
    parsed.records.push_back(record);
    offset += record.size;
  }
  parsed.valid_bytes = offset;
  parsed.stopped = offset != bytes.size();
  return parsed;
}

struct AddResult {
  HeaderStatus header = HeaderStatus::kEmpty;
  size_t records = 0;     // valid records in the input
  size_t added = 0;
  size_t duplicates = 0;
  size_t over_bound = 0;  // valid new records left out by the byte bound
  bool stopped = false;
};

// Merges stores of one family. The target (the build's own store) comes first
// and is kept; sources only add records it does not have.
class Merger {
 public:
  explicit Merger(Family family, size_t max_bytes = kMaxFileBytes)
      : family_(family), max_bytes_(max_bytes), bytes_(Header(family)) {}

  // The target's current file (empty when absent). A target with a header
  // other than this family's (another SDK format version, another family or a
  // damaged header) is foreign: it is never replaced and nothing is merged.
  AddResult SetTarget(std::span<const uint8_t> file) {
    auto result = Add(file);
    target_header_ = result.header;
    foreign_ = result.header != HeaderStatus::kValid && result.header != HeaderStatus::kEmpty;
    target_records_ = records_;
    return result;
  }

  // A target file that exists but could not be read: never replaced.
  void SetTargetUnreadable() {
    target_header_ = HeaderStatus::kShort;
    foreign_ = true;
  }

  // One source store. Invalid or foreign files add nothing.
  AddResult AddSource(std::span<const uint8_t> file) {
    if (foreign_) return {};
    auto result = Add(file);
    added_ += result.added;
    return result;
  }

  Family family() const { return family_; }
  bool foreign() const { return foreign_; }
  HeaderStatus target_header() const { return target_header_; }
  size_t target_records() const { return target_records_; }
  size_t records() const { return foreign_ ? target_records_ : records_; }
  size_t added() const { return added_; }
  // True when the merged file differs from the target in content: something
  // was added. (A target whose only difference is a bad tail is left for the
  // SDK to truncate.)
  bool NeedsWrite() const { return !foreign_ && added_ != 0; }
  // Header + records; the content to write when NeedsWrite().
  const std::vector<uint8_t> &bytes() const { return bytes_; }

 private:
  AddResult Add(std::span<const uint8_t> file) {
    AddResult result;
    const auto parsed = Parse(family_, file);
    result.header = parsed.header;
    result.stopped = parsed.stopped;
    if (parsed.header != HeaderStatus::kValid) return result;
    result.records = parsed.records.size();
    for (const auto &record : parsed.records) {
      const auto bytes = file.subspan(record.offset, record.size);
      if (Known(record.hash, bytes)) {
        ++result.duplicates;
        continue;
      }
      if (bytes_.size() + record.size > max_bytes_) {
        ++result.over_bound;
        continue;
      }
      index_.emplace(record.hash, bytes_.size());
      bytes_.insert(bytes_.end(), bytes.begin(), bytes.end());
      ++records_;
      ++result.added;
    }
    return result;
  }
  bool Known(uint64_t hash, std::span<const uint8_t> bytes) const {
    const auto range = index_.equal_range(hash);
    if (family_ == Family::kShaders) return range.first != range.second;
    for (auto it = range.first; it != range.second; ++it)
      if (std::memcmp(bytes_.data() + it->second, bytes.data(), bytes.size()) == 0) return true;
    return false;
  }

  Family family_;
  size_t max_bytes_;
  std::vector<uint8_t> bytes_;
  std::unordered_multimap<uint64_t, size_t> index_;
  size_t records_ = 0;
  size_t target_records_ = 0;
  size_t added_ = 0;
  bool foreign_ = false;
  HeaderStatus target_header_ = HeaderStatus::kEmpty;
};

// Source directories are build-fingerprint names (64 hexadecimal digits).
constexpr bool FingerprintName(std::wstring_view name) {
  if (name.size() != 64) return false;
  for (const wchar_t c : name)
    if (!((c >= L'0' && c <= L'9') || (c >= L'A' && c <= L'F') || (c >= L'a' && c <= L'f'))) return false;
  return true;
}

// Bounds of one seeding pass.
inline constexpr size_t kMaxSourceDirectories = 256;
inline constexpr size_t kMaxCacheEntriesPerProfile = 1024;
inline constexpr uint64_t kMaxReadBytes = 256ull * 1024 * 1024;

enum class Source : uint8_t {
  kCold,      // the build store had no records and nothing was added
  kSeeded,    // records were added from other stores
  kExisting,  // the build store had records and nothing was added
  kError,     // the target folder could not be prepared or a write failed
};

constexpr std::string_view SourceName(Source source) {
  switch (source) {
    case Source::kCold: return "cold";
    case Source::kSeeded: return "seeded";
    case Source::kExisting: return "existing";
    case Source::kError: return "error";
  }
  return "unknown";
}

struct Summary {
  bool enabled = true;
  bool error = false;
  size_t target_shaders = 0, target_descriptions = 0;  // before
  size_t shaders = 0, descriptions = 0;                // after
  size_t added_shaders = 0, added_descriptions = 0;
  size_t from_dirs = 0;      // source folders that added at least one record
  size_t scanned_dirs = 0;   // source folders read
  size_t skipped_files = 0;  // unreadable or invalid source files
  size_t foreign_files = 0;  // build store files left alone (foreign header)
  size_t over_bound = 0;     // records left out by the byte bound
  bool read_bound = false;   // the read budget stopped the scan
  uint64_t ms = 0, bytes = 0, read_bytes = 0;
};

constexpr Source Classify(const Summary &s) {
  if (s.error) return Source::kError;
  if (s.added_shaders + s.added_descriptions != 0) return Source::kSeeded;
  return s.target_shaders + s.target_descriptions != 0 ? Source::kExisting : Source::kCold;
}

// The per-session line (store-seed.txt and the launcher debug output).
inline std::string SummaryLine(const Summary &s) {
  const auto n = [](uint64_t v) { return std::to_string(v); };
  return "MCLA store seed: source=" + std::string(SourceName(Classify(s))) + " shaders=" + n(s.shaders) +
         " descriptions=" + n(s.descriptions) + " from_dirs=" + n(s.from_dirs) + " ms=" + n(s.ms) +
         " bytes=" + n(s.bytes) + " added_shaders=" + n(s.added_shaders) +
         " added_descriptions=" + n(s.added_descriptions) + " scanned_dirs=" + n(s.scanned_dirs) +
         " skipped_files=" + n(s.skipped_files) + " foreign_files=" + n(s.foreign_files) +
         " over_bound=" + n(s.over_bound) + " read_bytes=" + n(s.read_bytes) +
         (s.read_bound ? " read_bound=1" : "") + " switch=" + (s.enabled ? "on" : "off");
}

}  // namespace mcla::shader_store
