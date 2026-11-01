// M10-003: pure store parser and merger (src/mcla_shader_store_seed.h).
// Synthetic stores only: the "ucode" is generated counter data, not game data.

#include "mcla_shader_store_log.h"
#include "mcla_shader_store_seed.h"

#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

namespace {
namespace store = mcla::shader_store;
using store::Family;
using store::HeaderStatus;
using Bytes = std::vector<uint8_t>;

int checks = 0;
void Check(bool value, const char *message) {
  if (!value) {
    std::fprintf(stderr, "FAIL: %s\n", message);
    std::exit(1);
  }
  ++checks;
}

void PutLe32(Bytes &out, uint32_t v) {
  for (int i = 0; i < 4; ++i) out.push_back(uint8_t(v >> (8 * i)));
}
void PutLe64(Bytes &out, uint64_t v) {
  for (int i = 0; i < 8; ++i) out.push_back(uint8_t(v >> (8 * i)));
}

// One .xsh record: 12-byte header (hash, count | type << 31) + ucode.
Bytes ShaderRecord(uint32_t seed, uint32_t dwords, bool pixel = false) {
  Bytes ucode;
  for (uint32_t i = 0; i < dwords; ++i) PutLe32(ucode, seed * 2654435761u + i);
  Bytes out;
  PutLe64(out, store::Hash(ucode));
  PutLe32(out, dwords | (pixel ? 0x80000000u : 0u));
  out.insert(out.end(), ucode.begin(), ucode.end());
  return out;
}

// One .xpso record of the family's size: hash + description.
Bytes PipelineRecord(Family family, uint32_t seed) {
  Bytes description;
  const size_t size = store::Info(family).record_bytes - 8;
  for (size_t i = 0; i < size; ++i) description.push_back(uint8_t(seed * 31 + i * 7));
  Bytes out;
  PutLe64(out, store::Hash(description));
  out.insert(out.end(), description.begin(), description.end());
  return out;
}

Bytes File(Family family, const std::vector<Bytes> &records) {
  Bytes out = store::Header(family);
  for (const auto &record : records) out.insert(out.end(), record.begin(), record.end());
  return out;
}

void TestHeaders() {
  const Bytes xsh = store::Header(Family::kShaders);
  Check(xsh == Bytes{'X', 'E', 'S', 'H', 0x20, 0x20, 0x12, 0x19}, "xsh header bytes");
  Check(store::Header(Family::kD3d12Rtv) ==
            Bytes{'X', 'E', 'P', 'S', 'D', 'X', 'R', 'T', 0x20, 0x26, 0x02, 0x26},
        "rtv.d3d12 header bytes");
  Check(store::Header(Family::kD3d12Rov) ==
            Bytes{'X', 'E', 'P', 'S', 'D', 'X', 'R', 'O', 0x20, 0x26, 0x02, 0x26},
        "rov.d3d12 header bytes");
  Check(store::Header(Family::kVulkanFbo) == Bytes{'X', 'E', 'P', 'S', 0, 0, 0, 0, 0x20, 0x26, 0x02, 0x28},
        "fbo.vk header bytes");
  Check(store::Header(Family::kVulkanFsi) == Bytes{'X', 'E', 'P', 'S', 1, 0, 0, 0, 0x20, 0x26, 0x02, 0x28},
        "fsi.vk header bytes");
  Check(store::Info(Family::kD3d12Rtv).record_bytes == 72 && store::Info(Family::kVulkanFbo).record_bytes == 68,
        "record sizes (SDK sizeof PipelineStoredDescription)");
  for (const auto &info : store::kFamilies)
    Check(info.file.starts_with(L"545407F8."), "every store file is the title's");
  Check(store::kTitleId == 0x545407F8, "title id");
  Check(store::Info(Family::kD3d12Rtv).file == L"545407F8.rtv.d3d12.xpso" &&
            store::Info(Family::kVulkanFsi).file == L"545407F8.fsi.vk.xpso",
        "SDK file names");
}

void TestParseValid() {
  const auto file = File(Family::kShaders, {ShaderRecord(1, 5), ShaderRecord(2, 0), ShaderRecord(3, 9, true)});
  const auto parsed = store::Parse(Family::kShaders, file);
  Check(parsed.header == HeaderStatus::kValid && parsed.records.size() == 3, "three shaders");
  Check(parsed.valid_bytes == file.size() && !parsed.stopped, "whole file valid");
  Check(parsed.records[2].size == 12 + 36, "pixel-type record size uses the 31-bit count");
  const auto pipelines =
      File(Family::kD3d12Rtv, {PipelineRecord(Family::kD3d12Rtv, 1), PipelineRecord(Family::kD3d12Rtv, 2)});
  const auto p = store::Parse(Family::kD3d12Rtv, pipelines);
  Check(p.header == HeaderStatus::kValid && p.records.size() == 2 && !p.stopped, "two descriptions");
  Check(store::Parse(Family::kShaders, store::Header(Family::kShaders)).records.empty(), "header only");
}

void TestParseInvalidHeaders() {
  Check(store::Parse(Family::kShaders, {}).header == HeaderStatus::kEmpty, "empty");
  const Bytes shortfile{'X', 'E', 'S'};
  Check(store::Parse(Family::kShaders, shortfile).header == HeaderStatus::kShort, "short");
  auto bad_magic = File(Family::kShaders, {ShaderRecord(1, 4)});
  bad_magic[0] = 'Y';
  Check(store::Parse(Family::kShaders, bad_magic).header == HeaderStatus::kBadMagic, "bad magic");
  auto bad_version = File(Family::kShaders, {ShaderRecord(1, 4)});
  bad_version[7] = 0x20;
  Check(store::Parse(Family::kShaders, bad_version).header == HeaderStatus::kBadVersion, "xsh version mismatch");
  auto pso_version = File(Family::kD3d12Rtv, {PipelineRecord(Family::kD3d12Rtv, 1)});
  pso_version[11] = 0x25;  // e.g. the 0x20210425 of an older SDK
  Check(store::Parse(Family::kD3d12Rtv, pso_version).header == HeaderStatus::kBadVersion, "xpso version mismatch");
  // Families never mix.
  const auto rtv = File(Family::kD3d12Rtv, {PipelineRecord(Family::kD3d12Rtv, 1)});
  Check(store::Parse(Family::kD3d12Rov, rtv).header == HeaderStatus::kBadMagic, "rtv is not rov");
  Check(store::Parse(Family::kVulkanFbo, rtv).header == HeaderStatus::kBadMagic, "d3d12 is not vk");
  const auto fbo = File(Family::kVulkanFbo, {PipelineRecord(Family::kVulkanFbo, 1)});
  Check(store::Parse(Family::kVulkanFsi, fbo).header == HeaderStatus::kBadMagic, "fbo is not fsi");
  Check(store::Parse(Family::kD3d12Rtv, fbo).header == HeaderStatus::kBadMagic, "vk is not d3d12");
  const auto xsh = File(Family::kShaders, {ShaderRecord(1, 4)});
  Check(store::Parse(Family::kD3d12Rtv, xsh).header != HeaderStatus::kValid, "xsh is not xpso");
  Check(store::Parse(Family::kShaders, rtv).header == HeaderStatus::kBadMagic, "xpso is not xsh");
  Check(store::HeaderStatusName(HeaderStatus::kBadVersion) == "bad-version", "status name");
}

void TestTruncatedRecords() {
  auto file = File(Family::kShaders, {ShaderRecord(1, 5), ShaderRecord(2, 6)});
  const size_t full = file.size();
  file.resize(full - 3);  // last ucode cut
  auto parsed = store::Parse(Family::kShaders, file);
  Check(parsed.records.size() == 1 && parsed.stopped, "partial last shader dropped");
  Check(parsed.valid_bytes == 8 + 12 + 20, "valid prefix bytes");
  file = File(Family::kShaders, {ShaderRecord(1, 5)});
  file.push_back(0x11);  // partial record header
  parsed = store::Parse(Family::kShaders, file);
  Check(parsed.records.size() == 1 && parsed.stopped, "partial record header dropped");
  // A corrupted record in the middle ends the valid prefix (SDK rule).
  auto middle = ShaderRecord(2, 6);
  middle[20] ^= 0xFF;
  file = File(Family::kShaders, {ShaderRecord(1, 5), middle, ShaderRecord(3, 2)});
  parsed = store::Parse(Family::kShaders, file);
  Check(parsed.records.size() == 1 && parsed.stopped, "bad ucode hash stops");
  // Oversized count.
  Bytes huge;
  PutLe64(huge, 0);
  PutLe32(huge, store::kMaxUcodeDwords + 1);
  file = File(Family::kShaders, {ShaderRecord(1, 5), huge});
  parsed = store::Parse(Family::kShaders, file);
  Check(parsed.records.size() == 1 && parsed.stopped, "oversized ucode count stops");
  // Pipelines: partial trailing record and bad description hash.
  auto pso = File(Family::kD3d12Rtv, {PipelineRecord(Family::kD3d12Rtv, 1), PipelineRecord(Family::kD3d12Rtv, 2)});
  pso.resize(pso.size() - 1);
  parsed = store::Parse(Family::kD3d12Rtv, pso);
  Check(parsed.records.size() == 1 && parsed.stopped, "partial description dropped");
  auto bad = PipelineRecord(Family::kVulkanFsi, 2);
  bad[30] ^= 1;
  pso = File(Family::kVulkanFsi, {PipelineRecord(Family::kVulkanFsi, 1), bad, PipelineRecord(Family::kVulkanFsi, 3)});
  parsed = store::Parse(Family::kVulkanFsi, pso);
  Check(parsed.records.size() == 1 && parsed.stopped, "bad description hash stops");
}

void TestMergeDedup() {
  const auto a = ShaderRecord(1, 4), b = ShaderRecord(2, 5), c = ShaderRecord(3, 6), d = ShaderRecord(4, 7);
  store::Merger merger(Family::kShaders);
  auto target = merger.SetTarget(File(Family::kShaders, {a, b}));
  Check(target.added == 2 && merger.target_records() == 2 && !merger.foreign(), "target kept");
  auto first = merger.AddSource(File(Family::kShaders, {b, c}));
  Check(first.added == 1 && first.duplicates == 1, "source adds only the new shader");
  auto second = merger.AddSource(File(Family::kShaders, {c, d, a}));
  Check(second.added == 1 && second.duplicates == 2, "dedup across sources");
  Check(merger.records() == 4 && merger.added() == 2 && merger.NeedsWrite(), "union of four");
  Check(merger.bytes() == File(Family::kShaders, {a, b, c, d}), "target first, then additions in order");
  // Same ucode hash in one source twice: kept once.
  store::Merger twice(Family::kShaders);
  twice.SetTarget({});
  Check(twice.AddSource(File(Family::kShaders, {a, a})).added == 1, "in-file duplicate shader once");
  // Descriptions dedupe by full bytes.
  const auto p1 = PipelineRecord(Family::kD3d12Rtv, 1), p2 = PipelineRecord(Family::kD3d12Rtv, 2);
  store::Merger pso(Family::kD3d12Rtv);
  pso.SetTarget({});
  Check(pso.AddSource(File(Family::kD3d12Rtv, {p1, p2, p1})).added == 2, "description dedup");
  Check(pso.AddSource(File(Family::kD3d12Rtv, {p2})).added == 0, "description already known");
  Check(pso.records() == 2 && pso.target_records() == 0, "two descriptions");
}

void TestMergeRejects() {
  const auto a = ShaderRecord(1, 4);
  store::Merger merger(Family::kShaders);
  merger.SetTarget({});
  auto version = File(Family::kShaders, {ShaderRecord(5, 4)});
  version[6] = 0x13;
  const auto rejected = merger.AddSource(version);
  Check(rejected.header == HeaderStatus::kBadVersion && rejected.added == 0, "version mismatch adds nothing");
  Check(!merger.NeedsWrite() && merger.records() == 0, "nothing to write");
  // rov records offered to an rtv merger.
  store::Merger rtv(Family::kD3d12Rtv);
  rtv.SetTarget({});
  Check(rtv.AddSource(File(Family::kD3d12Rov, {PipelineRecord(Family::kD3d12Rov, 1)})).added == 0,
        "rov never merged into rtv");
  Check(rtv.AddSource(File(Family::kVulkanFbo, {PipelineRecord(Family::kVulkanFbo, 1)})).added == 0,
        "vk never merged into d3d12");
  // A foreign target (other format version) is never replaced.
  store::Merger foreign(Family::kShaders);
  auto newer = File(Family::kShaders, {a});
  newer[7] = 0x20;
  foreign.SetTarget(newer);
  Check(foreign.foreign() && foreign.target_header() == HeaderStatus::kBadVersion, "foreign target");
  Check(foreign.AddSource(File(Family::kShaders, {ShaderRecord(9, 3)})).added == 0, "no merge into foreign");
  Check(!foreign.NeedsWrite() && foreign.records() == 0, "foreign not written");
  store::Merger unreadable(Family::kShaders);
  unreadable.SetTargetUnreadable();
  Check(unreadable.AddSource(File(Family::kShaders, {a})).added == 0 && !unreadable.NeedsWrite(),
        "unreadable target left alone");
  // Truncated source: its valid prefix is used.
  auto cut = File(Family::kShaders, {ShaderRecord(6, 4), ShaderRecord(7, 4)});
  cut.resize(cut.size() - 2);
  const auto partial = merger.AddSource(cut);
  Check(partial.added == 1 && partial.stopped, "truncated source: valid prefix merged");
}

void TestTargetTail() {
  const auto a = ShaderRecord(1, 4), b = ShaderRecord(2, 4);
  auto target = File(Family::kShaders, {a});
  target.push_back(0x55);
  store::Merger only(Family::kShaders);
  only.SetTarget(target);
  Check(only.target_records() == 1 && !only.NeedsWrite(), "bad tail alone: no write (SDK truncates)");
  store::Merger merged(Family::kShaders);
  merged.SetTarget(target);
  merged.AddSource(File(Family::kShaders, {b}));
  Check(merged.NeedsWrite() && merged.bytes() == File(Family::kShaders, {a, b}), "bad tail dropped on write");
}

void TestSizeBound() {
  const auto a = ShaderRecord(1, 10), b = ShaderRecord(2, 10), c = ShaderRecord(3, 2);
  const size_t bound = 8 + a.size() + c.size();
  store::Merger merger(Family::kShaders, bound);
  merger.SetTarget({});
  const auto result = merger.AddSource(File(Family::kShaders, {a, b, c}));
  Check(result.added == 2 && result.over_bound == 1, "record over the bound left out");
  Check(merger.bytes().size() <= bound, "output within the bound");
  Check(store::Parse(Family::kShaders, merger.bytes()).records.size() == 2, "bounded output stays valid");
  Check(store::kMaxFileBytes == 16u * 1024 * 1024, "16 MiB per file");
}

void TestIdempotence() {
  const std::vector<Bytes> sources = {File(Family::kD3d12Rtv, {PipelineRecord(Family::kD3d12Rtv, 1),
                                                               PipelineRecord(Family::kD3d12Rtv, 2)}),
                                      File(Family::kD3d12Rtv, {PipelineRecord(Family::kD3d12Rtv, 3)})};
  store::Merger first(Family::kD3d12Rtv);
  first.SetTarget({});
  for (const auto &s : sources) first.AddSource(s);
  Check(first.NeedsWrite() && first.records() == 3, "first pass seeds");
  store::Merger again(Family::kD3d12Rtv);
  again.SetTarget(first.bytes());
  for (const auto &s : sources) again.AddSource(s);
  Check(!again.NeedsWrite() && again.added() == 0 && again.records() == 3, "second pass adds nothing");
  Check(again.bytes() == first.bytes(), "same bytes");
}

void TestSummary() {
  store::Summary s;
  Check(store::Classify(s) == store::Source::kCold, "cold");
  s.target_shaders = 3;
  Check(store::Classify(s) == store::Source::kExisting, "existing");
  s.added_descriptions = 1;
  Check(store::Classify(s) == store::Source::kSeeded, "seeded");
  s.error = true;
  Check(store::Classify(s) == store::Source::kError, "error");
  store::Summary line;
  line.shaders = 758;
  line.descriptions = 6822;
  line.added_shaders = 700;
  line.added_descriptions = 6000;
  line.from_dirs = 51;
  line.ms = 42;
  line.bytes = 900000;
  const auto text = store::SummaryLine(line);
  Check(text.starts_with("MCLA store seed: source=seeded shaders=758 descriptions=6822 from_dirs=51 ms=42 "
                         "bytes=900000 "),
        "summary line fields in the agreed order");
  Check(text.ends_with(" switch=on"), "switch on");
  store::Summary off;
  off.enabled = false;
  Check(store::SummaryLine(off).starts_with("MCLA store seed: source=cold ") &&
            store::SummaryLine(off).ends_with(" switch=off"),
        "switch off is cold");
  for (const char c : text) Check(static_cast<unsigned char>(c) < 0x80, "ASCII line");
}

void TestNamesAndLog() {
  Check(store::FingerprintName(std::wstring(64, L'A')), "upper hex fingerprint");
  Check(store::FingerprintName(std::wstring(64, L'f')), "lower hex fingerprint");
  Check(!store::FingerprintName(std::wstring(63, L'A')), "short name");
  Check(!store::FingerprintName(std::wstring(64, L'G')), "non-hex");
  Check(!store::FingerprintName(L""), "empty");
  const int64_t records[store::kFamilyCount] = {758, 6822, -1, -2, 0};
  Check(store::StartupLine(records, 123) ==
            "MCLA shader store at startup: xsh=758 rtv.d3d12=6822 rov.d3d12=absent fbo.vk=unreadable "
            "fsi.vk=0 bytes=123",
        "host startup line");
}
}  // namespace

int main() {
  TestHeaders();
  TestParseValid();
  TestParseInvalidHeaders();
  TestTruncatedRecords();
  TestMergeDedup();
  TestMergeRejects();
  TestTargetTail();
  TestSizeBound();
  TestIdempotence();
  TestSummary();
  TestNamesAndLog();
  std::printf("mcla_shader_store_seed_test: %d checks passed\n", checks);
  return 0;
}
