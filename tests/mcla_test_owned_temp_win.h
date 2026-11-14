#pragma once

// Test-only cleanup for a synthetic fixture that a test created itself under
// %TEMP%. The guard removes the fixture when it goes out of scope, on every
// return and exception path of the owning scope. It never follows a reparse
// point: junctions and symbolic links inside the fixture are removed as link
// objects (RemoveDirectoryW / DeleteFileW on the link), and their targets are
// never enumerated or touched. Paths use the \\?\ form so names such as
// "trailing." can be removed. Read-only attributes are cleared on the entry
// itself only (opened with FILE_FLAG_OPEN_REPARSE_POINT).
//
// Set MCLA_KEEP_TEST_FIXTURES=1 to keep the fixture for inspection.
// Production code never includes this header.

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include <iostream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace mcla::test {

inline std::wstring ExtendedLengthPath(const std::wstring &path) {
  if (path.rfind(L"\\\\?\\", 0) == 0) return path;
  return L"\\\\?\\" + path;
}

inline void ClearReadOnlyNoFollow(const std::wstring &path, DWORD attributes) {
  if ((attributes & FILE_ATTRIBUTE_READONLY) == 0) return;
  const HANDLE handle = CreateFileW(path.c_str(), FILE_WRITE_ATTRIBUTES,
      FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING,
      FILE_FLAG_OPEN_REPARSE_POINT | FILE_FLAG_BACKUP_SEMANTICS, nullptr);
  if (handle == INVALID_HANDLE_VALUE) return;
  FILE_BASIC_INFO info{};  // Zero timestamps mean "leave unchanged".
  info.FileAttributes = attributes & ~static_cast<DWORD>(FILE_ATTRIBUTE_READONLY);
  if (info.FileAttributes == 0) info.FileAttributes = FILE_ATTRIBUTE_NORMAL;
  SetFileInformationByHandle(handle, FileBasicInfo, &info, sizeof(info));
  CloseHandle(handle);
}

// Removes one entry (extended-length path) and counts what could not be removed.
inline void RemoveEntryNoFollow(const std::wstring &path, DWORD attributes, unsigned &left) {
  ClearReadOnlyNoFollow(path, attributes);
  const bool directory = (attributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
  if ((attributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0) {
    // The link object only; the target is never opened or enumerated.
    if (!(directory ? RemoveDirectoryW(path.c_str()) : DeleteFileW(path.c_str()))) ++left;
    return;
  }
  if (!directory) {
    if (!DeleteFileW(path.c_str())) ++left;
    return;
  }
  std::vector<std::pair<std::wstring, DWORD>> children;
  WIN32_FIND_DATAW data{};
  const HANDLE find = FindFirstFileExW((path + L"\\*").c_str(), FindExInfoBasic, &data,
      FindExSearchNameMatch, nullptr, 0);
  if (find == INVALID_HANDLE_VALUE) {
    ++left;
    return;
  }
  do {
    const std::wstring_view name(data.cFileName);
    if (name != L"." && name != L"..") children.emplace_back(path + L"\\" + data.cFileName, data.dwFileAttributes);
  } while (FindNextFileW(find, &data));
  FindClose(find);
  for (const auto &[child, child_attributes] : children) RemoveEntryNoFollow(child, child_attributes, left);
  if (!RemoveDirectoryW(path.c_str())) ++left;
}

class OwnedTempFixture {
 public:
  OwnedTempFixture() = default;
  OwnedTempFixture(const std::wstring &root, std::wstring_view prefix) { Adopt(root, prefix); }
  OwnedTempFixture(const OwnedTempFixture &) = delete;
  OwnedTempFixture &operator=(const OwnedTempFixture &) = delete;
  ~OwnedTempFixture() { Remove(); }

  // Accepts only a direct child of %TEMP% whose name starts with `prefix`
  // (the test's own unique fixture). Anything else is never removed.
  bool Adopt(const std::wstring &root, std::wstring_view prefix) {
    wchar_t temp[MAX_PATH + 1]{};
    const DWORD length = GetTempPathW(MAX_PATH + 1, temp);
    if (!length || length > MAX_PATH || root.size() <= length + prefix.size()) return Refuse(root);
    if (CompareStringOrdinal(root.c_str(), static_cast<int>(length), temp, static_cast<int>(length), TRUE) != CSTR_EQUAL)
      return Refuse(root);
    const std::wstring_view name = std::wstring_view(root).substr(length);
    if (name.find_first_of(L"\\/:") != std::wstring_view::npos || name.rfind(prefix, 0) != 0) return Refuse(root);
    root_ = root;
    return true;
  }

  void Remove() {
    if (root_.empty()) return;
    const std::wstring root = root_;
    root_.clear();
    wchar_t keep[8]{};
    if (GetEnvironmentVariableW(L"MCLA_KEEP_TEST_FIXTURES", keep, 8) == 1 && keep[0] == L'1') {
      std::wcout << L"Owned fixture kept (MCLA_KEEP_TEST_FIXTURES=1): " << root << L'\n';
      return;
    }
    const auto path = ExtendedLengthPath(root);
    unsigned left = 0;
    for (int attempt = 0; attempt < 5; ++attempt) {
      const DWORD attributes = GetFileAttributesW(path.c_str());
      if (attributes == INVALID_FILE_ATTRIBUTES) {
        left = 0;
        break;
      }
      left = 0;
      RemoveEntryNoFollow(path, attributes, left);
      if (!left) break;
      Sleep(200);  // A just-exited child or a scanner may still hold a handle.
    }
    if (left) std::wcerr << L"WARN owned fixture cleanup incomplete (" << left << L" entries): " << root << L'\n';
    else std::wcout << L"Owned fixture removed: " << root << L'\n';
  }

 private:
  bool Refuse(const std::wstring &root) {
    std::wcerr << L"WARN fixture cleanup refused for a path outside the owned TEMP prefix: " << root << L'\n';
    return false;
  }
  std::wstring root_;
};

}  // namespace mcla::test
