#include "mcla_launcher_snapshot_win.h"
#include "mcla_launcher_profile_policy.h"
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <bcrypt.h>
#include <algorithm>
#include <array>
#include <filesystem>
#include <climits>
#include <set>
#include <utility>

namespace mcla::launcher {
namespace {
struct File {
  HANDLE h = INVALID_HANDLE_VALUE;
  File() = default;
  explicit File(HANDLE value) : h(value) {}
  File(File &&f) noexcept : h(std::exchange(f.h, INVALID_HANDLE_VALUE)) {}
  File &operator=(File &&f) noexcept {
    if (this != &f) {
      if (h != INVALID_HANDLE_VALUE) CloseHandle(h);
      h = std::exchange(f.h, INVALID_HANDLE_VALUE);
    }
    return *this;
  }
  File(const File &) = delete;
  ~File() { if (h != INVALID_HANDLE_VALUE) CloseHandle(h); }
};
struct Entry { std::filesystem::path relative; uint64_t size; File file; };
std::wstring Native(const std::filesystem::path &p) { return L"\\\\?\\" + p.wstring(); }
std::filesystem::path ChildPath(const std::filesystem::path &root, const std::filesystem::path &relative) {
  return relative.empty() ? root : root / relative;
}
std::wstring Error(std::wstring_view what) {
  return std::wstring(what) + L" (Win32 " + std::to_wstring(GetLastError()) + L")";
}
struct Hash {
  BCRYPT_ALG_HANDLE algorithm = nullptr;
  BCRYPT_HASH_HANDLE hash = nullptr;
  ~Hash() { if (hash) BCryptDestroyHash(hash); if (algorithm) BCryptCloseAlgorithmProvider(algorithm, 0); }
  bool Begin() {
    return BCryptOpenAlgorithmProvider(&algorithm, BCRYPT_SHA256_ALGORITHM, nullptr, 0) >= 0 &&
           BCryptCreateHash(algorithm, &hash, nullptr, 0, nullptr, 0, 0) >= 0;
  }
  bool Add(const void *bytes, size_t n) {
    return n <= ULONG_MAX && BCryptHashData(hash, static_cast<PUCHAR>(const_cast<void *>(bytes)), static_cast<ULONG>(n), 0) >= 0;
  }
  std::expected<std::string, std::wstring> Finish() {
    std::array<unsigned char, 32> digest{};
    if (BCryptFinishHash(hash, digest.data(), static_cast<ULONG>(digest.size()), 0) < 0)
      return std::unexpected(L"snapshot SHA256 finalization failed");
    std::string result;
    for (auto b : digest) { result += "0123456789ABCDEF"[b >> 4]; result += "0123456789ABCDEF"[b & 15]; }
    return result;
  }
};

struct Record {
  std::u16string name;
  bool directory;
  uint64_t size;
  DWORD volume, index_high, index_low, write_high, write_low;
  bool operator==(const Record &) const = default;
};
struct InsensitiveLess {
  bool operator()(const std::wstring &a, const std::wstring &b) const {
    return CompareStringOrdinal(a.data(), static_cast<int>(a.size()), b.data(),
                                 static_cast<int>(b.size()), TRUE) == CSTR_LESS_THAN;
  }
};
bool PlainDataStream(const std::filesystem::path &path) {
  WIN32_FIND_STREAM_DATA data{};
  const HANDLE search = FindFirstStreamW(Native(path).c_str(), FindStreamInfoStandard, &data, 0);
  if (search == INVALID_HANDLE_VALUE) return GetLastError() == ERROR_HANDLE_EOF;
  bool only_default = true;
  do {
    only_default &= std::wstring_view(data.cStreamName) == L"::$DATA";
  } while (FindNextStreamW(search, &data));
  const DWORD error = GetLastError();
  FindClose(search);
  return only_default && error == ERROR_HANDLE_EOF;
}

// Re-enumerate every directory, not merely every initially observed file.
// Source file handles and DirectoryGuards remain held across the final scan.
std::expected<std::vector<Record>, std::wstring> Inventory(const std::filesystem::path &root) {
  std::vector<Record> records;
  std::vector<std::filesystem::path> pending{std::filesystem::path{}};
  size_t file_count = 0;
  uint64_t total = 0;
  for (size_t index = 0; index < pending.size(); ++index) {
    if (pending.size() > 513) return std::unexpected(L"save tree exceeds 512 descendant directories");
    const auto directory = pending[index];
    auto held = fs::OpenDirectory(ChildPath(root, directory).wstring());
    if (!held) return std::unexpected(held.error());
    std::vector<std::filesystem::path> children;
    std::set<std::wstring, InsensitiveLess> names;
    for (const auto &item : std::filesystem::directory_iterator(Native(ChildPath(root, directory)))) {
      if (children.size() >= 4096 + 512)
        return std::unexpected(L"save directory exceeds bounded entry count");
      const auto name = item.path().filename();
      if (!NormalizeRoot((root / directory / name).u16string()) ||
          !names.insert(name.wstring()).second)
        return std::unexpected(L"save entry has an unsafe or case-colliding path component");
      children.push_back(directory / name);
    }
    // Include root and directory identities/timestamps to detect additions then
    // removals during a cooperative copy as well as final differing inventories.
    if (index == 0) children.push_back({});
    for (const auto &relative : children) {
      const auto path = ChildPath(root, relative);
      File entry(CreateFileW(Native(path).c_str(), FILE_READ_ATTRIBUTES,
          FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING,
          FILE_FLAG_OPEN_REPARSE_POINT | FILE_FLAG_BACKUP_SEMANTICS, nullptr));
      BY_HANDLE_FILE_INFORMATION info{};
      if (entry.h == INVALID_HANDLE_VALUE || GetFileType(entry.h) != FILE_TYPE_DISK ||
          !GetFileInformationByHandle(entry.h, &info) ||
          (info.dwFileAttributes & (FILE_ATTRIBUTE_REPARSE_POINT | FILE_ATTRIBUTE_DEVICE)))
        return std::unexpected(L"save inventory contains an unreadable or redirected entry");
      const bool is_directory = (info.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
      const uint64_t size = is_directory ? 0 : (uint64_t(info.nFileSizeHigh) << 32) | info.nFileSizeLow;
      if (!PlainDataStream(path))
        return std::unexpected(L"named or unreadable NTFS streams cannot be silently omitted");
      if (!is_directory) {
        if (info.nNumberOfLinks != 1 || ++file_count > 4096 || size > 64ull * 1024 * 1024 ||
            total + size > 512ull * 1024 * 1024)
          return std::unexpected(L"save file is linked or exceeds snapshot bounds");
        total += size;
      } else if (!relative.empty()) {
        if (pending.size() >= 513)
          return std::unexpected(L"save tree exceeds 512 descendant directories");
        pending.push_back(relative);
      }
      records.push_back({relative.generic_u16string(), is_directory, size,
          info.dwVolumeSerialNumber, info.nFileIndexHigh, info.nFileIndexLow,
          info.ftLastWriteTime.dwHighDateTime, info.ftLastWriteTime.dwLowDateTime});
    }
  }
  std::sort(records.begin(), records.end(), [](const auto &a, const auto &b) { return a.name < b.name; });
  return records;
}

std::expected<std::string, std::wstring> ReadbackHash(HANDLE file, uint64_t expected_size) {
  LARGE_INTEGER zero{};
  if (!SetFilePointerEx(file, zero, nullptr, FILE_BEGIN))
    return std::unexpected(Error(L"cannot rewind snapshot for verification"));
  Hash hash;
  if (!hash.Begin()) return std::unexpected(L"cannot initialize destination verification hash");
  std::array<unsigned char, 65536> bytes{};
  uint64_t total = 0;
  for (;;) {
    DWORD read = 0;
    if (!::ReadFile(file, bytes.data(), static_cast<DWORD>(bytes.size()), &read, nullptr))
      return std::unexpected(Error(L"snapshot destination readback failed"));
    if (!read) break;
    total += read;
    if (total > expected_size || !hash.Add(bytes.data(), read))
      return std::unexpected(L"snapshot destination readback size/hash failed");
  }
  if (total != expected_size) return std::unexpected(L"snapshot destination readback size mismatch");
  return hash.Finish();
}
}
std::expected<void, std::wstring> ValidateSaveTreeForSnapshot(
    const fs::DirectoryGuard &source, std::wstring_view destination) {
  try {
    const std::filesystem::path src(source.path()), dst(destination);
    if (!NormalizeRoot(dst.u16string())) return std::unexpected(L"Invalid snapshot destination path");
    const auto before = Inventory(src);
    if (!before) return std::unexpected(before.error());
    std::vector<fs::DirectoryGuard> directories;
    std::vector<File> files;
    for (const auto &record : *before) {
      auto relative = std::filesystem::path(record.name);
      relative.make_preferred(); // Inventory names are portable '/'-separated UTF-16.
      if (!NormalizeRoot(ChildPath(dst, relative).u16string()))
        return std::unexpected(L"Destination snapshot path violates component or length policy");
      const auto path = ChildPath(src, relative);
      if (record.directory) {
        auto held = fs::OpenDirectory(path.wstring());
        if (!held) return std::unexpected(held.error());
        directories.push_back(std::move(*held));
        continue;
      }
      File file(CreateFileW(Native(path).c_str(), GENERIC_READ, FILE_SHARE_READ,
          nullptr, OPEN_EXISTING, FILE_FLAG_OPEN_REPARSE_POINT | FILE_FLAG_SEQUENTIAL_SCAN, nullptr));
      BY_HANDLE_FILE_INFORMATION info{};
      if (file.h == INVALID_HANDLE_VALUE || GetFileType(file.h) != FILE_TYPE_DISK ||
          !GetFileInformationByHandle(file.h, &info) || info.nNumberOfLinks != 1 ||
          (info.dwFileAttributes & (FILE_ATTRIBUTE_DIRECTORY | FILE_ATTRIBUTE_REPARSE_POINT)) ||
          info.dwVolumeSerialNumber != record.volume || info.nFileIndexHigh != record.index_high ||
          info.nFileIndexLow != record.index_low)
        return std::unexpected(L"Snapshot source is unreadable, linked or changed during inspection");
      auto readable = ReadbackHash(file.h, record.size);
      if (!readable) return std::unexpected(readable.error());
      files.push_back(std::move(file));
    }
    const auto after = Inventory(src);
    if (!after || *after != *before) return std::unexpected(L"Save tree changed during read-only inspection");
    return {};
  } catch (const std::exception &) {
    return std::unexpected(L"Read-only save tree inspection failed");
  }
}

static std::expected<std::string, std::wstring> CopySaveTreeImpl(
    const fs::DirectoryGuard &source, const fs::DirectoryGuard &destination
#if defined(MCLA_LAUNCHER_SNAPSHOT_TEST_SEAM)
    , void (*before_verify)(void *) = nullptr, void *context = nullptr
#endif
    ) {
  const auto distinct = fs::CompareDisjoint(source, destination);
  if (!distinct) return std::unexpected(distinct.error());
  try {
    const std::filesystem::path src(source.path()), dst(destination.path());
    if (!std::filesystem::is_empty(Native(dst))) return std::unexpected(L"snapshot destination must be empty");
    const auto before = Inventory(src);
    if (!before) return std::unexpected(before.error());
    std::vector<fs::DirectoryGuard> guards;
    std::vector<std::filesystem::path> directories;
    std::vector<Entry> files;
    uint64_t total = 0;
    // No recursive iterator: inspect and guard a directory before descending.
    std::vector<std::filesystem::path> pending{std::filesystem::path{}};
    for (size_t index = 0; index < pending.size(); ++index) {
      if (pending.size() > 513) return std::unexpected(L"save tree exceeds 512 descendant directories");
      const auto relative = pending[index];
      auto held = fs::OpenDirectory(ChildPath(src, relative).wstring());
      if (!held) return std::unexpected(held.error());
      guards.push_back(std::move(*held));
      for (const auto &item : std::filesystem::directory_iterator(Native(ChildPath(src, relative)))) {
        auto child = relative / item.path().filename();
        const auto path = src / child;
        if (!NormalizeRoot(path.u16string()) || !NormalizeRoot((dst / child).u16string()))
          return std::unexpected(L"source/destination snapshot path violates component or length policy");
        const DWORD attributes = GetFileAttributesW(Native(path).c_str());
        if (attributes == INVALID_FILE_ATTRIBUTES || (attributes & (FILE_ATTRIBUTE_REPARSE_POINT | FILE_ATTRIBUTE_DEVICE)))
          return std::unexpected(L"save tree contains an unreadable or reparse entry");
        if (attributes & FILE_ATTRIBUTE_DIRECTORY) {
          if (pending.size() >= 513)
            return std::unexpected(L"save tree exceeds 512 descendant directories");
          pending.push_back(child); directories.push_back(child); continue;
        }
        if (files.size() >= 4096) return std::unexpected(L"save tree exceeds 4096 files");
        File file(CreateFileW(Native(path).c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr,
            OPEN_EXISTING, FILE_FLAG_OPEN_REPARSE_POINT | FILE_FLAG_SEQUENTIAL_SCAN, nullptr));
        if (file.h == INVALID_HANDLE_VALUE) return std::unexpected(Error(L"cannot exclusively read save source"));
        BY_HANDLE_FILE_INFORMATION info{};
        if (GetFileType(file.h) != FILE_TYPE_DISK || !GetFileInformationByHandle(file.h, &info) ||
            info.nNumberOfLinks != 1 || (info.dwFileAttributes & (FILE_ATTRIBUTE_DIRECTORY | FILE_ATTRIBUTE_REPARSE_POINT)))
          return std::unexpected(L"save source must be regular, non-reparse and single-link");
        const uint64_t size = (uint64_t(info.nFileSizeHigh) << 32) | info.nFileSizeLow;
        if (size > 64 * 1024 * 1024 || total + size > 512 * 1024 * 1024)
          return std::unexpected(L"save tree exceeds 64 MiB per file or 512 MiB total");
        total += size;
        files.push_back({std::move(child), size, std::move(file)});
      }
    }
    // Stable manifest includes names as UTF-16 hex, so no delimiter ambiguity.
    const auto path_less = [](const auto &a, const auto &b) { return a.generic_u16string() < b.generic_u16string(); };
    std::sort(directories.begin(), directories.end(), path_less);
    std::sort(files.begin(), files.end(), [&](const Entry &a, const Entry &b) { return path_less(a.relative, b.relative); });
    std::string manifest = "MCLA-SAVE-SNAPSHOT-V1\n";
    const auto name_hex = [](const std::filesystem::path &path) {
      std::string hex;
      for (char16_t c : path.generic_u16string())
        for (int shift : {12, 8, 4, 0}) hex += "0123456789ABCDEF"[(c >> shift) & 15];
      return hex;
    };
    for (const auto &dir : directories) {
      if (!CreateDirectoryW(Native(dst / dir).c_str(), nullptr))
        return std::unexpected(Error(L"cannot create snapshot directory; incomplete copy preserved"));
      auto held = fs::OpenDirectory((dst / dir).wstring());
      if (!held) return std::unexpected(held.error());
      guards.push_back(std::move(*held));
      manifest += "D " + name_hex(dir) + "\n";
    }
    std::array<unsigned char, 65536> buffer{};
    std::vector<File> output_handles;
    for (const auto &entry : files) {
      File output(CreateFileW(Native(dst / entry.relative).c_str(), GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ, nullptr,
          CREATE_NEW, FILE_FLAG_OPEN_REPARSE_POINT | FILE_FLAG_WRITE_THROUGH, nullptr));
      if (output.h == INVALID_HANDLE_VALUE) return std::unexpected(Error(L"cannot create snapshot file; incomplete copy preserved"));
      Hash hash;
      if (!hash.Begin()) return std::unexpected(L"cannot initialize snapshot SHA256");
      uint64_t copied = 0;
      for (;;) {
        DWORD read = 0;
        if (!::ReadFile(entry.file.h, buffer.data(), static_cast<DWORD>(buffer.size()), &read, nullptr))
          return std::unexpected(Error(L"snapshot source read failed"));
        if (!read) break;
        copied += read;
        if (copied > entry.size || !hash.Add(buffer.data(), read)) return std::unexpected(L"snapshot size/hash mismatch");
        DWORD written = 0;
        if (!::WriteFile(output.h, buffer.data(), read, &written, nullptr) || written != read)
          return std::unexpected(Error(L"snapshot write failed; incomplete copy preserved"));
      }
      if (copied != entry.size || !FlushFileBuffers(output.h)) return std::unexpected(L"snapshot size/flush failed");
      auto digest = hash.Finish();
      if (!digest) return std::unexpected(digest.error());
      const auto verified = ReadbackHash(output.h, entry.size);
      if (!verified || *verified != *digest)
        return std::unexpected(L"snapshot destination SHA256 readback does not match source");
      output_handles.push_back(std::move(output));
      manifest += "F " + *digest + " " + std::to_string(entry.size) + " " + name_hex(entry.relative) + "\n";
    }
#if defined(MCLA_LAUNCHER_SNAPSHOT_TEST_SEAM)
    if (before_verify) before_verify(context);
#endif
    const auto after = Inventory(src);
    if (!after || *after != *before)
      return std::unexpected(L"source inventory changed; incomplete snapshot preserved");
    const auto copied = Inventory(dst);
    if (!copied || copied->size() != before->size())
      return std::unexpected(L"destination tree inventory differs; incomplete snapshot preserved");
    for (size_t i = 0; i < before->size(); ++i) {
      const auto &a = (*before)[i], &b = (*copied)[i];
      if (a.name != b.name || a.directory != b.directory || a.size != b.size)
        return std::unexpected(L"destination names/types/sizes differ; incomplete snapshot preserved");
    }
    return manifest;
  } catch (const std::filesystem::filesystem_error &) {
    return std::unexpected(L"save tree filesystem error; source untouched and incomplete copy preserved");
  }
}

std::expected<std::string, std::wstring> CopySaveTree(
    const fs::DirectoryGuard &source, const fs::DirectoryGuard &destination) {
  return CopySaveTreeImpl(source, destination);
}
#if defined(MCLA_LAUNCHER_SNAPSHOT_TEST_SEAM)
std::expected<std::string, std::wstring> CopySaveTreeBeforeVerifyForTest(
    const fs::DirectoryGuard &source, const fs::DirectoryGuard &destination,
    void (*before_verify)(void *), void *context) {
  return CopySaveTreeImpl(source, destination, before_verify, context);
}
#endif
}
