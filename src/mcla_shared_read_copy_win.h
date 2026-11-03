#pragma once

// KI-065 (M10-008): the F10 snapshot copies the live save tree on its worker
// thread while the game keeps running, so the game may save at the same time.
// std::filesystem::copy_file (CopyFile2 -> CopyFileExW) keeps the source open
// for the whole copy with sharing the platform chooses. On Windows it grants
// write and delete sharing (mcla_shared_read_copy_win_test prints it); Wine's
// CopyFileExW is believed to omit FILE_SHARE_DELETE (unverified; the same test
// run under Proton prints it). Without delete sharing the game's content
// transaction (SDK content_manager.cpp: backup copy, remove_all of the
// package, marker create/remove, rename) fails with a sharing violation while
// the copy runs, the SDK returns a silent X_ERROR_ACCESS_DENIED and the game
// shows "storage device was removed" (Steam Deck, l66, F10 during a save).
//
// This copier opens the source with read, write and delete sharing, reads it
// into memory, closes it and only then writes the destination, so it never
// blocks the game and holds each file only for the read. A copy torn by a
// concurrent write is caught by the caller's size/time stability check and
// retried. The content-transaction scratch entries (".rexbackup" copies and
// ".rexpending" markers of the SDK content manager) are left out of the F10
// copy: they exist only while a save is in flight and are removed or replaced
// by it.

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <string_view>
#include <vector>

namespace mcla::diagnostics {

// Sharing of the source handle: never blocks the game's writers, renames or
// deletes.
inline constexpr DWORD kSharedReadShareMode =
    FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE;

// Suffixes the SDK content manager (system/xam/content_manager.cpp) gives to
// the backup copy of a save package/header and to the transaction marker.
inline constexpr std::wstring_view kTransactionBackupSuffix = L".rexbackup";
inline constexpr std::wstring_view kTransactionMarkerSuffix = L".rexpending";

inline bool IsTransactionScratchName(std::wstring_view name) {
  return name.ends_with(kTransactionBackupSuffix) ||
         name.ends_with(kTransactionMarkerSuffix);
}

struct SharedReadCopyResult {
  bool copied = false;
  DWORD error = ERROR_SUCCESS;  // Win32 error of the failing step
  uint64_t bytes = 0;
};

class OwnedHandle {
 public:
  explicit OwnedHandle(HANDLE handle) : handle_(handle) {}
  OwnedHandle(const OwnedHandle &) = delete;
  OwnedHandle &operator=(const OwnedHandle &) = delete;
  ~OwnedHandle() { Close(); }
  bool valid() const { return handle_ != INVALID_HANDLE_VALUE && handle_ != nullptr; }
  HANDLE get() const { return handle_; }
  bool Close() {
    if (!valid()) return true;
    const BOOL closed = CloseHandle(handle_);
    handle_ = INVALID_HANDLE_VALUE;
    return closed != FALSE;
  }

 private:
  HANDLE handle_;
};

// The source open used by CopyFileSharedRead (exposed for tests).
inline HANDLE OpenSharedRead(const std::filesystem::path &source) {
  return CreateFileW(source.c_str(), GENERIC_READ, kSharedReadShareMode, nullptr,
                     OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_SEQUENTIAL_SCAN,
                     nullptr);
}

// Reads all of source (at most max_bytes) through a fully shared handle,
// closes it, then creates or replaces destination with those bytes. A source
// larger than max_bytes, also one that grows past it while being read, fails
// with ERROR_FILE_TOO_LARGE and writes nothing. A failed write removes the
// partial destination.
inline SharedReadCopyResult CopyFileSharedRead(const std::filesystem::path &source,
                                               const std::filesystem::path &destination,
                                               uint64_t max_bytes) {
  SharedReadCopyResult result;
  std::vector<uint8_t> data;
  {
    OwnedHandle in(OpenSharedRead(source));
    if (!in.valid()) {
      result.error = GetLastError();
      return result;
    }
    std::vector<uint8_t> chunk(64 * 1024);
    for (;;) {
      DWORD got = 0;
      if (!ReadFile(in.get(), chunk.data(), static_cast<DWORD>(chunk.size()), &got,
                    nullptr)) {
        result.error = GetLastError();
        return result;
      }
      if (got == 0) break;
      if (data.size() + got > max_bytes) {
        result.error = ERROR_FILE_TOO_LARGE;
        return result;
      }
      data.insert(data.end(), chunk.begin(), chunk.begin() + got);
    }
  }
  OwnedHandle out(CreateFileW(destination.c_str(), GENERIC_WRITE, 0, nullptr,
                              CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr));
  if (!out.valid()) {
    result.error = GetLastError();
    return result;
  }
  size_t written = 0;
  while (written < data.size()) {
    const size_t left = data.size() - written;
    const DWORD want = static_cast<DWORD>(left < (1u << 20) ? left : (1u << 20));
    DWORD wrote = 0;
    if (!WriteFile(out.get(), data.data() + written, want, &wrote, nullptr) || wrote == 0) {
      result.error = wrote == 0 && GetLastError() == ERROR_SUCCESS ? ERROR_WRITE_FAULT
                                                                    : GetLastError();
      out.Close();
      DeleteFileW(destination.c_str());
      return result;
    }
    written += wrote;
  }
  if (!out.Close()) {
    result.error = GetLastError();
    DeleteFileW(destination.c_str());
    return result;
  }
  result.copied = true;
  result.bytes = data.size();
  return result;
}

}  // namespace mcla::diagnostics
