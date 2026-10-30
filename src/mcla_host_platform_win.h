#pragma once

// Shared compatibility-layer detection (launcher, host, crash handler).
// Wine/Proton exports wine_get_version from its ntdll; real Windows does not.
// KI-049 (M9-006): also the Win32 side of the Wine reparse-ancestor policy
// (mcla_wine_path_policy.h): fact gathering and the once-per-path record.

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <winioctl.h>

#include "mcla_wine_path_policy.h"

#include <atomic>
#include <cstdint>
#include <cstring>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace mcla::platform {

namespace detail {
// -1: detect; 0: forced real Windows; 1: forced Wine. Test seam only.
inline std::atomic<int> wine_override{-1};
} // namespace detail

inline bool DetectWine() {
  const HMODULE ntdll = GetModuleHandleW(L"ntdll.dll");
  return ntdll && GetProcAddress(ntdll, "wine_get_version") != nullptr;
}

inline bool RunningUnderWine() {
  const int forced = detail::wine_override.load(std::memory_order_relaxed);
  if (forced >= 0) return forced == 1;
  static const bool detected = DetectWine();
  return detected;
}

// Tests only: force the Wine (1) or Windows (0) branch, or restore detection (-1).
// Production code never calls this; there is no environment or file trigger.
inline void SetWineOverrideForTesting(int mode) {
  detail::wine_override.store(mode, std::memory_order_relaxed);
}

// Reparse tag of an object opened with FILE_FLAG_OPEN_REPARSE_POINT. Wine may
// report 0 through FileAttributeTagInfo; FSCTL_GET_REPARSE_POINT is the
// fallback. Absence of a tag is a fact for the policy, never an error.
inline std::optional<uint32_t> ReadReparseTag(HANDLE handle) {
  FILE_ATTRIBUTE_TAG_INFO tag_info{};
  if (GetFileInformationByHandleEx(handle, FileAttributeTagInfo, &tag_info, sizeof(tag_info)) &&
      tag_info.ReparseTag != 0)
    return tag_info.ReparseTag;
  std::vector<uint8_t> buffer(MAXIMUM_REPARSE_DATA_BUFFER_SIZE);
  DWORD returned = 0;
  if (DeviceIoControl(handle, FSCTL_GET_REPARSE_POINT, nullptr, 0, buffer.data(),
                      static_cast<DWORD>(buffer.size()), &returned, nullptr) &&
      returned >= sizeof(uint32_t)) {
    uint32_t tag = 0;
    std::memcpy(&tag, buffer.data(), sizeof(tag));
    if (tag != 0) return tag;
  }
  return std::nullopt;
}

// Drive letter of the handle's normalized DOS final path ("\\?\X:\..."), or 0.
inline wchar_t FinalDriveLetter(HANDLE handle) {
  std::wstring value(32768, L'\0');
  const DWORD written = GetFinalPathNameByHandleW(handle, value.data(), static_cast<DWORD>(value.size()),
                                                  FILE_NAME_NORMALIZED | VOLUME_NAME_DOS);
  if (!written || written >= value.size()) return 0;
  value.resize(written);
  if (!value.starts_with(L"\\\\?\\") || value.size() < 7 || value[5] != L':' || value[6] != L'\\') return 0;
  return value[4];
}

inline bool SameDriveLetter(wchar_t a, std::wstring_view path) {
  if (!a || path.size() < 2 || path[1] != L':') return false;
  const auto upper = [](wchar_t c) { return (c >= L'a' && c <= L'z') ? static_cast<wchar_t>(c - 32) : c; };
  return upper(a) == upper(path[0]);
}

// Facts for a path-based check of an ancestor directory that reported the
// reparse attribute. Outside Wine nothing is opened and the decision refuses.
// `path` is an absolute drive path without the \\?\ prefix.
inline AncestorFacts InspectReparseAncestor(const std::wstring &path, uint32_t attributes, bool under_wine) {
  AncestorFacts facts{under_wine, attributes};
  if (!under_wine) return facts;
  const std::wstring native = L"\\\\?\\" + path;
  const HANDLE link = CreateFileW(native.c_str(), FILE_READ_ATTRIBUTES,
      FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING,
      FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OPEN_REPARSE_POINT, nullptr);
  if (link != INVALID_HANDLE_VALUE) {
    if (const auto tag = ReadReparseTag(link)) {
      facts.tag_known = true;
      facts.tag = *tag;
    }
    CloseHandle(link);
  }
  const HANDLE followed = CreateFileW(native.c_str(), FILE_READ_ATTRIBUTES,
      FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING,
      FILE_FLAG_BACKUP_SEMANTICS, nullptr);
  if (followed != INVALID_HANDLE_VALUE) {
    BY_HANDLE_FILE_INFORMATION target{};
    if (GetFileType(followed) == FILE_TYPE_DISK && GetFileInformationByHandle(followed, &target)) {
      facts.target_is_directory = (target.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
      facts.target_same_drive = SameDriveLetter(FinalDriveLetter(followed), path);
    }
    CloseHandle(followed);
  }
  return facts;
}

namespace detail {
inline constexpr size_t kMaximumRecordedAcceptances = 256;
inline std::mutex acceptance_mutex;
inline std::vector<std::wstring> acceptances;
inline bool OrdinalEqualIgnoreCase(std::wstring_view a, std::wstring_view b) {
  return a.size() == b.size() &&
         CompareStringOrdinal(a.data(), static_cast<int>(a.size()), b.data(),
                              static_cast<int>(b.size()), TRUE) == CSTR_EQUAL;
}
} // namespace detail

// Once per path per process: every Wine acceptance goes to the debugger/Proton
// log and to WineAcceptedPaths() (the launcher shows it in its status line).
inline void RecordWineAcceptance(std::wstring_view component, const std::wstring &path,
                                 AncestorVerdict verdict, const AncestorFacts &facts) {
  std::lock_guard lock(detail::acceptance_mutex);
  for (const auto &existing : detail::acceptances) {
    if (detail::OrdinalEqualIgnoreCase(existing, path)) return;
  }
  if (detail::acceptances.size() >= detail::kMaximumRecordedAcceptances) return;
  detail::acceptances.push_back(path);
  constexpr wchar_t hex[] = L"0123456789ABCDEF";
  std::wstring tag = L"none";
  if (facts.tag_known) {
    tag = L"0x";
    for (int shift = 28; shift >= 0; shift -= 4) tag += hex[(facts.tag >> shift) & 15];
  }
  const std::wstring line = L"MCLA " + std::wstring(component) + L": Proton: accepted symlink ancestor " +
      path + (verdict == AncestorVerdict::kWineLinkSameDrive ? L" (same drive" : L" (Unix link tag") +
      L", tag " + tag + L")\n";
  OutputDebugStringW(line.c_str());
}

inline std::vector<std::wstring> WineAcceptedPaths() {
  std::lock_guard lock(detail::acceptance_mutex);
  return detail::acceptances;
}

// Path-based ancestor check (GetFileAttributesW callers): true when the
// directory `path` with `attributes` may be traversed. Never relaxes a
// non-directory or anything on real Windows; records Wine acceptances.
inline bool AncestorDirectoryAllowed(std::wstring_view component, const std::wstring &path,
                                     uint32_t attributes) {
  if (!(attributes & kAttributeReparsePoint)) return (attributes & kAttributeDirectory) != 0;
  const auto facts = InspectReparseAncestor(path, attributes, RunningUnderWine());
  const auto verdict = DecideAncestor(facts);
  if (!Accepted(verdict)) return false;
  RecordWineAcceptance(component, path, verdict, facts);
  return true;
}

} // namespace mcla::platform
