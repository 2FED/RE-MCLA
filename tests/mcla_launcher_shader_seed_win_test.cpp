// M10-003: the launcher seeding pass on real directories
// (src/mcla_launcher_shader_seed_win.cpp). Synthetic stores in an owned temp
// fixture only; no real store, profile or save is opened.

#include "mcla_launcher_shader_seed_win.h"

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <bcrypt.h>
#include <winioctl.h>
#include "mcla_test_owned_temp_win.h"

#include <array>
#include <cstring>
#include <filesystem>
#include <iostream>
#include <string>
#include <vector>

#ifndef MCLA_DEV_OVERRIDES
#define MCLA_DEV_OVERRIDES 0
#endif
#ifndef MCLA_PREWARM_SEED_DEFAULT
#define MCLA_PREWARM_SEED_DEFAULT 1
#endif

namespace {
namespace fs = mcla::launcher::fs;
namespace store = mcla::shader_store;
using store::Family;
using Bytes = std::vector<uint8_t>;

unsigned checks = 0;
unsigned failures = 0;
void Check(bool value, const char *label) {
  ++checks;
  if (!value) {
    ++failures;
    std::cerr << "FAIL " << label << '\n';
  }
}

void PutLe32(Bytes &out, uint32_t v) {
  for (int i = 0; i < 4; ++i) out.push_back(uint8_t(v >> (8 * i)));
}
void PutLe64(Bytes &out, uint64_t v) {
  for (int i = 0; i < 8; ++i) out.push_back(uint8_t(v >> (8 * i)));
}
// Synthetic counter "ucode" (not game data).
Bytes ShaderRecord(uint32_t seed, uint32_t dwords) {
  Bytes ucode;
  for (uint32_t i = 0; i < dwords; ++i) PutLe32(ucode, seed * 2654435761u + i);
  Bytes out;
  PutLe64(out, store::Hash(ucode));
  PutLe32(out, dwords);
  out.insert(out.end(), ucode.begin(), ucode.end());
  return out;
}
Bytes PipelineRecord(Family family, uint32_t seed) {
  Bytes description;
  for (size_t i = 0; i < store::Info(family).record_bytes - 8; ++i) description.push_back(uint8_t(seed * 31 + i * 7));
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
std::vector<Bytes> Shaders(uint32_t first, uint32_t count) {
  std::vector<Bytes> out;
  for (uint32_t i = 0; i < count; ++i) out.push_back(ShaderRecord(first + i, 3 + i % 5));
  return out;
}
std::vector<Bytes> Pipelines(Family family, uint32_t first, uint32_t count) {
  std::vector<Bytes> out;
  for (uint32_t i = 0; i < count; ++i) out.push_back(PipelineRecord(family, first + i));
  return out;
}

bool MakeDirs(const std::wstring &path) {
  std::error_code ec;
  std::filesystem::create_directories(path, ec);
  return !ec;
}
bool Write(const std::wstring &path, const Bytes &bytes) {
  const auto handle = CreateFileW(path.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr);
  if (handle == INVALID_HANDLE_VALUE) return false;
  DWORD written = 0;
  const bool ok = ::WriteFile(handle, bytes.data(), static_cast<DWORD>(bytes.size()), &written, nullptr) &&
                  written == bytes.size();
  CloseHandle(handle);
  return ok;
}
Bytes Read(const std::wstring &path) {
  Bytes bytes;
  const auto handle =
      CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
  if (handle == INVALID_HANDLE_VALUE) return bytes;
  LARGE_INTEGER size{};
  GetFileSizeEx(handle, &size);
  bytes.resize(static_cast<size_t>(size.QuadPart));
  DWORD read = 0;
  if (!bytes.empty()) ::ReadFile(handle, bytes.data(), static_cast<DWORD>(bytes.size()), &read, nullptr);
  CloseHandle(handle);
  bytes.resize(read);
  return bytes;
}
bool Exists(const std::wstring &path) { return GetFileAttributesW(path.c_str()) != INVALID_FILE_ATTRIBUTES; }
size_t Records(Family family, const std::wstring &path) {
  return store::Parse(family, Read(path)).records.size();
}
bool Contains(Family family, const std::wstring &path, const Bytes &record) {
  const auto bytes = Read(path);
  const auto parsed = store::Parse(family, bytes);
  for (const auto &r : parsed.records)
    if (r.size == record.size() && std::memcmp(bytes.data() + r.offset, record.data(), r.size) == 0) return true;
  return false;
}
size_t TempFiles(const std::wstring &directory) {
  size_t count = 0;
  std::error_code ec;
  for (std::filesystem::directory_iterator it(directory, ec), end; !ec && it != end; it.increment(ec))
    count += it->path().filename().wstring().starts_with(L".mcla-tmp-") ? 1 : 0;
  return count;
}

// NTFS junction (no privilege needed); the documented mount-point layout.
bool MakeJunction(const std::wstring &link, const std::wstring &target) {
  if (!CreateDirectoryW(link.c_str(), nullptr)) return false;
  const std::wstring substitute = L"\\??\\" + target;
  const auto sub_bytes = static_cast<WORD>(substitute.size() * sizeof(wchar_t));
  const auto print_bytes = static_cast<WORD>(target.size() * sizeof(wchar_t));
  const WORD path_bytes = static_cast<WORD>(sub_bytes + print_bytes + 2 * sizeof(wchar_t));
  const WORD data_length = static_cast<WORD>(4 * sizeof(WORD) + path_bytes);
  std::vector<uint8_t> buffer(8 + data_length, 0);
  const DWORD tag = IO_REPARSE_TAG_MOUNT_POINT;
  const WORD fields[] = {data_length, 0, 0, sub_bytes, static_cast<WORD>(sub_bytes + sizeof(wchar_t)), print_bytes};
  std::memcpy(buffer.data(), &tag, sizeof(tag));
  std::memcpy(buffer.data() + 4, fields, sizeof(fields));
  std::memcpy(buffer.data() + 16, substitute.data(), sub_bytes);
  std::memcpy(buffer.data() + 16 + sub_bytes + sizeof(wchar_t), target.data(), print_bytes);
  const HANDLE handle = CreateFileW(link.c_str(), GENERIC_WRITE, 0, nullptr, OPEN_EXISTING,
                                    FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OPEN_REPARSE_POINT, nullptr);
  if (handle == INVALID_HANDLE_VALUE) return false;
  DWORD returned = 0;
  const bool ok = DeviceIoControl(handle, FSCTL_SET_REPARSE_POINT, buffer.data(), static_cast<DWORD>(buffer.size()),
                                  nullptr, 0, &returned, nullptr) != FALSE;
  CloseHandle(handle);
  return ok;
}

std::wstring Fingerprint(wchar_t digit) { return std::wstring(64, digit); }
const std::wstring kXsh(store::Info(Family::kShaders).file);
const std::wstring kRtv(store::Info(Family::kD3d12Rtv).file);
const std::wstring kRov(store::Info(Family::kD3d12Rov).file);
const std::wstring kFbo(store::Info(Family::kVulkanFbo).file);

store::Summary SeedInto(const std::wstring &build_cache, const std::vector<std::wstring> &roots, bool enabled) {
  auto guard = fs::OpenDirectory(build_cache);
  if (!guard) {
    std::wcerr << L"cannot open " << build_cache << L": " << guard.error() << L'\n';
    store::Summary failed;
    failed.error = true;
    return failed;
  }
  return mcla::launcher::SeedShaderStore(*guard, roots, enabled);
}
}  // namespace

int main() {
  wchar_t temp[MAX_PATH + 1]{};
  const auto temp_length = GetTempPathW(MAX_PATH + 1, temp);
  if (!temp_length || temp_length > MAX_PATH) return 2;
  std::array<uint8_t, 16> random{};
  if (BCryptGenRandom(nullptr, random.data(), static_cast<ULONG>(random.size()), BCRYPT_USE_SYSTEM_PREFERRED_RNG) < 0)
    return 2;
  constexpr wchar_t hex[] = L"0123456789abcdef";
  std::wstring root = std::wstring(temp, temp_length) + L"mcla-store-seed-test-";
  for (const auto byte : random) {
    root += hex[byte >> 4];
    root += hex[byte & 15];
  }
  // Owned synthetic fixture, removed on exit; junctions are removed as links
  // and never followed.
  if (!CreateDirectoryW(root.c_str(), nullptr)) return 2;
  const mcla::test::OwnedTempFixture cleanup(root, L"mcla-store-seed-test-");
  {
    auto canonical = fs::OpenDirectory(root);
    if (!canonical) return 2;
    root = canonical->path();
  }
  std::wcout << L"Owned synthetic fixture (removed on exit): " << root << L'\n';

  const auto p1 = root + L"\\profile-main", p2 = root + L"\\profile-bench", hidden = root + L"\\outside";
  const auto target = p1 + L"\\cache\\" + Fingerprint(L'1');
  const auto target_store = target + L"\\shaders\\shareable";
  const auto a = p1 + L"\\cache\\" + Fingerprint(L'A') + L"\\shaders\\shareable";
  const auto b = p2 + L"\\cache\\" + Fingerprint(L'b') + L"\\shaders\\shareable";
  const auto old_format = p2 + L"\\cache\\" + Fingerprint(L'C') + L"\\shaders\\shareable";
  const auto not_build = p2 + L"\\cache\\not-a-build\\shaders\\shareable";
  const auto hidden_store = hidden + L"\\cache\\" + Fingerprint(L'D') + L"\\shaders\\shareable";
  bool ok = MakeDirs(target) && MakeDirs(a) && MakeDirs(b) && MakeDirs(old_format) && MakeDirs(not_build) &&
            MakeDirs(hidden_store) && MakeDirs(p2 + L"\\user");
  // Profile A (same profile, older build): shaders 0..9, rtv 0..19.
  ok &= Write(a + L"\\" + kXsh, File(Family::kShaders, Shaders(0, 10)));
  ok &= Write(a + L"\\" + kRtv, File(Family::kD3d12Rtv, Pipelines(Family::kD3d12Rtv, 0, 20)));
  // Profile B (a BENCH copy): shaders 5..14, rtv 10..29, rov 0..3, fbo.vk 0..2;
  // a different title's store next to them is never read.
  ok &= Write(b + L"\\" + kXsh, File(Family::kShaders, Shaders(5, 10)));
  ok &= Write(b + L"\\" + kRtv, File(Family::kD3d12Rtv, Pipelines(Family::kD3d12Rtv, 10, 20)));
  ok &= Write(b + L"\\" + kRov, File(Family::kD3d12Rov, Pipelines(Family::kD3d12Rov, 100, 4)));
  ok &= Write(b + L"\\" + kFbo, File(Family::kVulkanFbo, Pipelines(Family::kVulkanFbo, 200, 3)));
  ok &= Write(b + L"\\4D5307E6.xsh", File(Family::kShaders, Shaders(500, 3)));
  // An older SDK format: skipped.
  auto old_xsh = File(Family::kShaders, Shaders(600, 3));
  old_xsh[7] = 0x18;
  ok &= Write(old_format + L"\\" + kXsh, old_xsh);
  // Not a build fingerprint folder, and a store-like file among the saves.
  ok &= Write(not_build + L"\\" + kXsh, File(Family::kShaders, Shaders(700, 3)));
  ok &= Write(p2 + L"\\user\\" + kXsh, File(Family::kShaders, Shaders(800, 3)));
  // Outside the library; reachable only through a junction (refused).
  ok &= Write(hidden_store + L"\\" + kXsh, File(Family::kShaders, Shaders(900, 3)));
  if (!ok) {
    std::cerr << "fixture setup failed\n";
    return 2;
  }
  const bool junction = MakeDirs(p2 + L"\\cache") && MakeJunction(p2 + L"\\cache\\" + Fingerprint(L'E'),
                                                                   hidden + L"\\cache\\" + Fingerprint(L'D'));
  if (!junction) std::cout << "SKIP junction fixture (Win32 " << GetLastError() << ")\n";
  const auto source_a_xsh = Read(a + L"\\" + kXsh), source_b_rtv = Read(b + L"\\" + kRtv);
  const std::vector<std::wstring> roots{p1, p2};

  // 1. Disabled: nothing created, nothing read from other profiles.
  auto off = SeedInto(target, roots, false);
  Check(!off.error && store::Classify(off) == store::Source::kCold && !off.enabled, "switch off: cold");
  Check(off.scanned_dirs == 0 && off.bytes == 0, "switch off: nothing read or written");
  Check(!Exists(target + L"\\shaders"), "switch off: no folder created");
  Check(store::SummaryLine(off).ends_with(" switch=off"), "switch off in the line");

  // 2. Seeding: union by family, deduplicated.
  auto seeded = SeedInto(target, roots, true);
  Check(!seeded.error && store::Classify(seeded) == store::Source::kSeeded, "seeded");
  Check(seeded.shaders == 15 && seeded.added_shaders == 15, "15 distinct shaders (0..14)");
  Check(seeded.descriptions == 30 + 4 + 3, "30 rtv + 4 rov + 3 fbo descriptions");
  Check(seeded.from_dirs == 2, "two source folders contributed");
  Check(seeded.scanned_dirs == 3, "three fingerprint folders read (A, b, C); junction and non-build skipped");
  Check(seeded.skipped_files == 1, "older-format store skipped");
  Check(seeded.bytes > 0 && seeded.read_bytes > 0, "bytes counted");
  Check(Records(Family::kShaders, target_store + L"\\" + kXsh) == 15, "target xsh written");
  Check(Records(Family::kD3d12Rtv, target_store + L"\\" + kRtv) == 30, "target rtv written");
  Check(Records(Family::kD3d12Rov, target_store + L"\\" + kRov) == 4, "rov kept separate");
  Check(Records(Family::kVulkanFbo, target_store + L"\\" + kFbo) == 3, "vk kept separate");
  Check(!Exists(target_store + L"\\" + std::wstring(store::Info(Family::kVulkanFsi).file)), "no empty family file");
  Check(!Exists(target_store + L"\\4D5307E6.xsh"), "other title never copied");
  Check(!Contains(Family::kShaders, target_store + L"\\" + kXsh, ShaderRecord(500, 3)), "other title ignored");
  Check(!Contains(Family::kShaders, target_store + L"\\" + kXsh, ShaderRecord(600, 3)), "old format ignored");
  Check(!Contains(Family::kShaders, target_store + L"\\" + kXsh, ShaderRecord(700, 3)), "non-build folder ignored");
  Check(!Contains(Family::kShaders, target_store + L"\\" + kXsh, ShaderRecord(800, 3)), "saves never read");
  Check(!Contains(Family::kShaders, target_store + L"\\" + kXsh, ShaderRecord(900, 3)), "junction refused");
  Check(Read(a + L"\\" + kXsh) == source_a_xsh && Read(b + L"\\" + kRtv) == source_b_rtv, "sources unchanged");
  Check(TempFiles(target_store) == 0, "atomic writes leave no temporary file");

  // 3. Second pass: nothing new, nothing rewritten.
  const auto before = Read(target_store + L"\\" + kXsh);
  auto again = SeedInto(target, roots, true);
  Check(!again.error && store::Classify(again) == store::Source::kExisting, "second pass: existing");
  Check(again.added_shaders == 0 && again.added_descriptions == 0 && again.bytes == 0, "second pass adds nothing");
  Check(Read(target_store + L"\\" + kXsh) == before, "second pass leaves the file");

  // 4. A new source record later: merged into the existing store (target first).
  ok = Write(a + L"\\" + kFbo, File(Family::kVulkanFbo, Pipelines(Family::kVulkanFbo, 300, 2)));
  auto grown = SeedInto(target, roots, true);
  Check(ok && store::Classify(grown) == store::Source::kSeeded && grown.added_descriptions == 2, "grown store");
  Check(Records(Family::kVulkanFbo, target_store + L"\\" + kFbo) == 5, "fbo grew to 5");
  const auto grown_xsh = Read(target_store + L"\\" + kXsh);
  Check(grown_xsh == before, "unchanged family not rewritten");

  // 5. Foreign build store (another format version) is never replaced.
  const auto foreign = p1 + L"\\cache\\" + Fingerprint(L'2');
  const auto foreign_store = foreign + L"\\shaders\\shareable";
  auto newer = File(Family::kShaders, Shaders(1000, 2));
  newer[7] = 0x20;
  ok = MakeDirs(foreign_store) && Write(foreign_store + L"\\" + kXsh, newer);
  auto kept = SeedInto(foreign, roots, true);
  Check(ok && kept.foreign_files == 1, "foreign build store counted");
  Check(Read(foreign_store + L"\\" + kXsh) == newer, "foreign build store untouched");
  Check(Records(Family::kD3d12Rtv, foreign_store + L"\\" + kRtv) == 30, "other families still seeded");

  // 6. A hard-linked source file is refused (single-link rule).
  const auto linked = p2 + L"\\cache\\" + Fingerprint(L'F') + L"\\shaders\\shareable";
  ok = MakeDirs(linked) && Write(root + L"\\outside-file.xsh", File(Family::kShaders, Shaders(1100, 2)));
  const bool hardlink = ok && CreateHardLinkW((linked + L"\\" + kXsh).c_str(), (root + L"\\outside-file.xsh").c_str(),
                                              nullptr);
  const auto fresh = p1 + L"\\cache\\" + Fingerprint(L'3');
  ok = MakeDirs(fresh);
  auto refused = SeedInto(fresh, roots, true);
  if (hardlink) {
    Check(!Contains(Family::kShaders, fresh + L"\\shaders\\shareable\\" + kXsh, ShaderRecord(1100, 2)),
          "hard-linked source refused");
    // Skipped: the hard link, the older-format store and the foreign store of 5.
    Check(refused.skipped_files == 3, "hard link, old and newer format skipped");
  } else {
    std::cout << "SKIP hard-link fixture (Win32 " << GetLastError() << ")\n";
  }
  Check(refused.shaders == 15, "fresh build seeded from the valid stores");

  // 7. A profile whose cache folder is a junction is refused as a whole.
  const auto p3 = root + L"\\profile-linked";
  ok = MakeDirs(p3) && MakeJunction(p3 + L"\\cache", hidden + L"\\cache");
  if (ok) {
    const auto fresh2 = p1 + L"\\cache\\" + Fingerprint(L'4');
    MakeDirs(fresh2);
    auto linked_profile = SeedInto(fresh2, {p1, p3}, true);
    Check(!Contains(Family::kShaders, fresh2 + L"\\shaders\\shareable\\" + kXsh, ShaderRecord(900, 3)),
          "junction cache folder refused");
    Check(linked_profile.shaders == 15, "seeded from profile-main only");
  } else {
    std::cout << "SKIP junction profile fixture\n";
  }

  // 8. No library profiles: an empty store folder, cold.
  const auto lonely = root + L"\\lonely";
  MakeDirs(lonely);
  auto none = SeedInto(lonely, {}, true);
  Check(!none.error && store::Classify(none) == store::Source::kCold, "no sources: cold");

  // 9. The switch.
  {
    using mcla::dev_overrides::Parse;
    using mcla::launcher::StoreSeedSwitch;
    Check(!StoreSeedSwitch(Parse("prewarm_seed_store=false\n"), true), "file switch off");
    Check(StoreSeedSwitch(Parse("prewarm_seed_store=true\n"), false), "file switch on");
    Check(StoreSeedSwitch(Parse("# nothing\n"), true), "no line: default");
    Check(StoreSeedSwitch(Parse(std::string(5000, 'a')), true), "rejected file: default");
    // l81: the launcher parses the same file with the host parser, so the
    // 4 KiB limit is the same: exactly 4096 bytes are read, 4097 rejected.
    {
      std::string at_limit = "prewarm_seed_store=false\n";
      at_limit += std::string(mcla::dev_overrides::kMaxBytes - at_limit.size(), '\n');
      Check(at_limit.size() == 4096 && !StoreSeedSwitch(Parse(at_limit), true),
            "4096-byte file: the switch line applies");
      at_limit += '\n';
      Check(StoreSeedSwitch(Parse(at_limit), true) && !StoreSeedSwitch(Parse(at_limit), false),
            "4097-byte file: rejected, the default applies");
    }
    Check(StoreSeedSwitch(Parse("prewarm_seed_store=0\n"), true), "malformed: default");
    const auto launcher_dir = root + L"\\launcher";
    MakeDirs(launcher_dir);
    auto dir = fs::OpenDirectory(launcher_dir);
    Check(dir && mcla::launcher::StoreSeedEnabled(*dir) == (MCLA_PREWARM_SEED_DEFAULT != 0), "no file: default");
    Write(launcher_dir + L"\\mcla-dev-overrides.txt", Bytes{'p', 'r', 'e', 'w', 'a', 'r', 'm', '_', 's', 'e', 'e',
                                                            'd', '_', 's', 't', 'o', 'r', 'e', '=', 'f', 'a', 'l',
                                                            's', 'e', '\n'});
    Check(dir && mcla::launcher::StoreSeedEnabled(*dir) ==
                     (MCLA_DEV_OVERRIDES ? false : (MCLA_PREWARM_SEED_DEFAULT != 0)),
          "file read only in dev-override launcher builds");
    // l81: a 4096-byte file (beyond the former 2 KiB) is read through the
    // launcher's own file path; 4097 bytes are rejected (the default applies).
    for (const size_t size : {size_t{4096}, size_t{4097}}) {
      const auto sized_dir = root + L"\\launcher-" + std::to_wstring(size);
      MakeDirs(sized_dir);
      const std::string line = "prewarm_seed_store=false\n";
      Bytes bytes(line.begin(), line.end());
      bytes.resize(size, '\n');
      const bool written = Write(sized_dir + L"\\mcla-dev-overrides.txt", bytes);
      auto sized = fs::OpenDirectory(sized_dir);
      const bool expected =
          MCLA_DEV_OVERRIDES && size <= mcla::dev_overrides::kMaxBytes ? false
                                                                       : (MCLA_PREWARM_SEED_DEFAULT != 0);
      Check(written && sized && mcla::launcher::StoreSeedEnabled(*sized) == expected,
            size == 4096 ? "launcher reads a 4096-byte override file" : "launcher rejects 4097 bytes");
    }
  }

  std::cout << "Launcher shader store seeding: " << checks << " checks, " << failures << " failures\n";
  return failures ? 1 : 0;
}
