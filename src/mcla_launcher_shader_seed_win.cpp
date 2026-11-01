#include "mcla_launcher_shader_seed_win.h"

#include "mcla_dev_overrides.h"

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include <algorithm>
#include <exception>
#include <filesystem>
#include <optional>

#ifndef MCLA_PREWARM_SEED_DEFAULT
#define MCLA_PREWARM_SEED_DEFAULT 1
#endif
#ifndef MCLA_DEV_OVERRIDES
#define MCLA_DEV_OVERRIDES 0
#endif

namespace mcla::launcher {
namespace {
namespace store = mcla::shader_store;

std::wstring Native(std::wstring_view path) { return L"\\\\?\\" + std::wstring(path); }

bool SamePath(std::wstring_view a, std::wstring_view b) {
  return a.size() == b.size() &&
         CompareStringOrdinal(a.data(), static_cast<int>(a.size()), b.data(), static_cast<int>(b.size()), TRUE) ==
             CSTR_EQUAL;
}

uint64_t NowTicks() {
  LARGE_INTEGER value{};
  QueryPerformanceCounter(&value);
  return static_cast<uint64_t>(value.QuadPart);
}
uint64_t ElapsedMs(uint64_t start) {
  LARGE_INTEGER frequency{};
  QueryPerformanceFrequency(&frequency);
  if (frequency.QuadPart <= 0) return 0;
  return (NowTicks() - start) * 1000 / static_cast<uint64_t>(frequency.QuadPart);
}

enum class FileState { kAbsent, kRead, kError };
struct StoreFile {
  FileState state = FileState::kAbsent;
  std::vector<uint8_t> bytes;
};
// One store file of a held directory: absent, read (bounded, regular,
// single-link, non-reparse) or an error.
StoreFile ReadStoreFile(const fs::DirectoryGuard &directory, std::wstring_view name) {
  StoreFile file;
  const auto path = Native(directory.path() + L"\\" + std::wstring(name));
  if (GetFileAttributesW(path.c_str()) == INVALID_FILE_ATTRIBUTES) {
    const auto error = GetLastError();
    file.state = error == ERROR_FILE_NOT_FOUND || error == ERROR_PATH_NOT_FOUND ? FileState::kAbsent : FileState::kError;
    return file;
  }
  auto bytes = fs::ReadFile(directory, name, store::kMaxFileBytes);
  if (!bytes) {
    file.state = FileState::kError;
    return file;
  }
  file.state = FileState::kRead;
  file.bytes = std::move(*bytes);
  return file;
}

// An existing child directory of a held directory, or nothing. With create,
// a missing child is created first (never through a missing parent).
std::optional<fs::DirectoryGuard> Child(const fs::DirectoryGuard &parent, std::wstring_view name, bool create) {
  const auto path = parent.path() + L"\\" + std::wstring(name);
  if (create && !CreateDirectoryW(Native(path).c_str(), nullptr) && GetLastError() != ERROR_ALREADY_EXISTS)
    return std::nullopt;
  if (GetFileAttributesW(Native(path).c_str()) == INVALID_FILE_ATTRIBUTES) return std::nullopt;
  auto directory = fs::OpenDirectory(path);
  if (!directory) return std::nullopt;
  return std::optional<fs::DirectoryGuard>{std::move(*directory)};
}

struct Candidate {
  std::wstring path;  // <profile>\cache\<fingerprint>\shaders\shareable
  uint64_t written = 0;
};

// Source store folders of the library profiles, newest store first. Only
// fingerprint-named children of each profile's cache folder are considered.
std::vector<Candidate> Candidates(const std::vector<std::wstring> &profile_roots, const fs::DirectoryGuard &build_cache) {
  std::vector<Candidate> result;
  for (const auto &root : profile_roots) {
    auto profile = fs::OpenDirectory(root);
    if (!profile) continue;
    auto cache = Child(*profile, L"cache", false);
    if (!cache) continue;
    std::error_code ec;
    size_t entries = 0;
    for (std::filesystem::directory_iterator it(cache->path(), ec), end; !ec && it != end; it.increment(ec)) {
      if (++entries > store::kMaxCacheEntriesPerProfile) break;
      const auto name = it->path().filename().wstring();
      std::error_code entry_ec;
      if (!store::FingerprintName(name) || !it->is_directory(entry_ec) || it->is_symlink(entry_ec)) continue;
      const auto build = cache->path() + L"\\" + name;
      if (SamePath(build, build_cache.path())) continue;
      Candidate candidate{build + L"\\shaders\\shareable", 0};
      WIN32_FILE_ATTRIBUTE_DATA data{};
      const auto xsh = Native(candidate.path + L"\\" + std::wstring(store::Info(store::Family::kShaders).file));
      if (GetFileAttributesExW(xsh.c_str(), GetFileExInfoStandard, &data))
        candidate.written = (uint64_t{data.ftLastWriteTime.dwHighDateTime} << 32) | data.ftLastWriteTime.dwLowDateTime;
      result.push_back(std::move(candidate));
    }
  }
  std::sort(result.begin(), result.end(), [](const Candidate &a, const Candidate &b) {
    return a.written != b.written ? a.written > b.written : a.path < b.path;
  });
  if (result.size() > store::kMaxSourceDirectories) result.resize(store::kMaxSourceDirectories);
  return result;
}

store::Summary Seed(const fs::DirectoryGuard &build_cache, const std::vector<std::wstring> &profile_roots,
                    bool enabled) {
  store::Summary summary;
  summary.enabled = enabled;
  const auto started = NowTicks();
  // Without seeding nothing is created: only the build's own store is counted.
  auto shaders = Child(build_cache, L"shaders", enabled);
  auto shareable = shaders ? Child(*shaders, L"shareable", enabled) : std::nullopt;
  if (!shareable) {
    summary.error = enabled;
    summary.ms = ElapsedMs(started);
    return summary;
  }
  std::vector<store::Merger> mergers;
  for (size_t i = 0; i < store::kFamilyCount; ++i) {
    const auto family = static_cast<store::Family>(i);
    mergers.emplace_back(family);
    auto target = ReadStoreFile(*shareable, store::Info(family).file);
    // An unreadable build store file (too large, linked, locked) is left alone.
    if (target.state == FileState::kError) mergers.back().SetTargetUnreadable();
    else mergers.back().SetTarget(target.bytes);
    summary.foreign_files += mergers.back().foreign() ? 1 : 0;
  }
  if (enabled) {
    for (const auto &candidate : Candidates(profile_roots, build_cache)) {
      if (summary.read_bound) break;
      auto source = fs::OpenDirectory(candidate.path);
      if (!source || !fs::CompareDisjoint(*source, *shareable)) continue;
      ++summary.scanned_dirs;
      bool contributed = false;
      for (auto &merger : mergers) {
        if (merger.foreign()) continue;
        auto file = ReadStoreFile(*source, store::Info(merger.family()).file);
        if (file.state == FileState::kAbsent) continue;
        if (file.state == FileState::kError) {
          ++summary.skipped_files;
          continue;
        }
        summary.read_bytes += file.bytes.size();
        const auto added = merger.AddSource(file.bytes);
        if (added.header != store::HeaderStatus::kValid && added.header != store::HeaderStatus::kEmpty)
          ++summary.skipped_files;
        summary.over_bound += added.over_bound;
        contributed |= added.added != 0;
        if (summary.read_bytes >= store::kMaxReadBytes) {
          summary.read_bound = true;
          break;
        }
      }
      summary.from_dirs += contributed ? 1 : 0;
    }
    // Descriptions first, shaders last: either order is valid for the SDK.
    for (size_t i = store::kFamilyCount; i-- > 0;) {
      const auto &merger = mergers[i];
      if (!merger.NeedsWrite()) continue;
      const auto written = fs::AtomicWrite(*shareable, store::Info(merger.family()).file, merger.bytes());
      if (written) summary.bytes += merger.bytes().size();
      else summary.error = true;
    }
  }
  for (const auto &merger : mergers) {
    const bool shader = merger.family() == store::Family::kShaders;
    (shader ? summary.target_shaders : summary.target_descriptions) += merger.target_records();
    (shader ? summary.shaders : summary.descriptions) += merger.records();
    (shader ? summary.added_shaders : summary.added_descriptions) += merger.added();
  }
  summary.ms = ElapsedMs(started);
  return summary;
}
}  // namespace

store::Summary SeedShaderStore(const fs::DirectoryGuard &build_cache, const std::vector<std::wstring> &profile_roots,
                               bool enabled) {
  try {
    return Seed(build_cache, profile_roots, enabled);
  } catch (const std::exception &) {
    store::Summary summary;
    summary.enabled = enabled;
    summary.error = true;
    return summary;
  }
}

bool StoreSeedEnabled(const fs::DirectoryGuard &launcher_directory) {
  constexpr bool kDefault = MCLA_PREWARM_SEED_DEFAULT != 0;
#if MCLA_DEV_OVERRIDES
  const std::wstring name(dev_overrides::kFile.begin(), dev_overrides::kFile.end());
  auto file = ReadStoreFile(launcher_directory, name);
  if (file.state != FileState::kRead) return kDefault;
  const auto parsed = dev_overrides::Parse(
      std::string_view(reinterpret_cast<const char *>(file.bytes.data()), file.bytes.size()));
  return StoreSeedSwitch(parsed, kDefault);
#else
  (void)launcher_directory;
  return kDefault;
#endif
}

}  // namespace mcla::launcher
