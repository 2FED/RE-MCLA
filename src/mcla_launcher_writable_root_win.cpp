#include "mcla_launcher_writable_root_win.h"

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <shlobj.h>

#include <algorithm>
#include <utility>
#include <vector>

namespace mcla::launcher {
namespace {
struct Rule { std::wstring path; bool subtree; };
bool Equal(std::wstring_view a, std::wstring_view b) {
  return a.size() == b.size() && CompareStringOrdinal(a.data(), static_cast<int>(a.size()),
      b.data(), static_cast<int>(b.size()), TRUE) == CSTR_EQUAL;
}
bool Descendant(std::wstring_view parent, std::wstring_view path) {
  return parent.size() < path.size() && path[parent.size()] == L'\\' &&
      Equal(parent, path.substr(0, parent.size()));
}
std::wstring Clean(std::wstring path) {
  std::replace(path.begin(), path.end(), L'/', L'\\');
  if (path.starts_with(L"\\\\?\\")) path.erase(0, 4);
  while (path.size() > 3 && path.back() == L'\\') path.pop_back();
  return path;
}
void AddRule(std::vector<Rule> &rules, std::wstring path, bool subtree) {
  path = Clean(std::move(path));
  if (path.empty()) return;
  rules.push_back({path, subtree});
  // Follow only the trusted OS-supplied known-folder location to discover its
  // protected target, including redirected Documents. User roots themselves
  // are never opened through a reparse by this API.
  HANDLE handle = CreateFileW(path.c_str(), FILE_READ_ATTRIBUTES,
      FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr,
      OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, nullptr);
  if (handle == INVALID_HANDLE_VALUE) return; // Keep the exact OS path rule.
  const DWORD needed = GetFinalPathNameByHandleW(handle, nullptr, 0, FILE_NAME_NORMALIZED | VOLUME_NAME_DOS);
  if (needed && needed <= 32768) {
    std::wstring final(needed, L'\0');
    const DWORD written = GetFinalPathNameByHandleW(handle, final.data(), needed,
        FILE_NAME_NORMALIZED | VOLUME_NAME_DOS);
    if (written && written < needed) {
      final.resize(written);
      rules.push_back({Clean(std::move(final)), subtree});
    }
  }
  CloseHandle(handle);
}
std::expected<void, std::wstring> AddKnown(
    std::vector<Rule> &rules, REFKNOWNFOLDERID id, bool subtree) {
  PWSTR value = nullptr;
  const HRESULT result = SHGetKnownFolderPath(id, KF_FLAG_DONT_VERIFY, nullptr, &value);
  if (FAILED(result) || !value) {
    CoTaskMemFree(value);
    return std::unexpected(L"cannot resolve protected known-folder policy");
  }
  AddRule(rules, value, subtree);
  CoTaskMemFree(value);
  return {};
}
} // namespace

std::expected<void, std::wstring> ValidateWritableRoot(const fs::DirectoryGuard &directory) {
  const auto &path = directory.path();
  if (path.size() < 4 || path[1] != L':' || path[2] != L'\\')
    return std::unexpected(L"writable root must be a guarded local directory");
  const auto slash = path.find(L'\\', 3);
  const auto first = std::wstring_view(path).substr(3, slash == path.npos ? path.npos : slash - 3);
  for (const std::wstring_view reserved : {L"$Recycle.Bin", L"Recycler", L"Recycled", L"System Volume Information"})
    if (Equal(first, reserved)) return std::unexpected(L"volume system/recycle directory is not a personal data root");

  std::vector<Rule> rules;
  std::wstring windows(32768, L'\0');
  const UINT count = GetWindowsDirectoryW(windows.data(), static_cast<UINT>(windows.size()));
  if (!count || count >= windows.size()) return std::unexpected(L"cannot resolve protected Windows directory");
  windows.resize(count);
  AddRule(rules, std::move(windows), true);
  for (const auto *id : {&FOLDERID_ProgramFiles, &FOLDERID_ProgramFilesX86,
                         &FOLDERID_ProgramFilesX64, &FOLDERID_ProgramData}) {
    auto added = AddKnown(rules, *id, true);
    if (!added) return added;
  }
  for (const auto *id : {&FOLDERID_Profile, &FOLDERID_Documents, &FOLDERID_LocalAppData,
                         &FOLDERID_RoamingAppData, &FOLDERID_LocalAppDataLow}) {
    auto added = AddKnown(rules, *id, false);
    if (!added) return added;
  }
  PWSTR profile = nullptr;
  if (FAILED(SHGetKnownFolderPath(FOLDERID_Profile, KF_FLAG_DONT_VERIFY, nullptr, &profile)) || !profile) {
    CoTaskMemFree(profile);
    return std::unexpected(L"cannot resolve user AppData container");
  }
  AddRule(rules, std::wstring(profile) + L"\\AppData", false);
  CoTaskMemFree(profile);
  for (const auto &rule : rules) {
    if (Equal(rule.path, path) || (rule.subtree && Descendant(rule.path, path)))
      return std::unexpected(L"protected system or exact known-folder container is not a writable personal root");
  }
  return {};
}

} // namespace mcla::launcher
