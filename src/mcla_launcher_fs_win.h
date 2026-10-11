#pragma once

#include <cstddef>
#include <cstdint>
#include <expected>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace mcla::launcher::fs {

// Local fixed-drive directories only. Holds every ancestor without DELETE
// sharing; guards must outlive the operation/child protected by those handles.
class DirectoryGuard {
 public:
  struct State;
  DirectoryGuard(DirectoryGuard &&) noexcept;
  DirectoryGuard &operator=(DirectoryGuard &&) noexcept;
  ~DirectoryGuard();
  DirectoryGuard(const DirectoryGuard &) = delete;
  DirectoryGuard &operator=(const DirectoryGuard &) = delete;
  const std::wstring &path() const;
 private:
  explicit DirectoryGuard(std::unique_ptr<State> state);
  std::unique_ptr<State> state_;
  friend std::expected<DirectoryGuard, std::wstring> OpenDirectory(std::wstring_view);
  friend std::expected<void, std::wstring> CompareDisjoint(const DirectoryGuard &, const DirectoryGuard &);
};

class WriterLock {
 public:
  struct State;
  WriterLock(WriterLock &&) noexcept;
  WriterLock &operator=(WriterLock &&) noexcept;
  ~WriterLock();
  WriterLock(const WriterLock &) = delete;
  WriterLock &operator=(const WriterLock &) = delete;
 private:
  explicit WriterLock(std::unique_ptr<State> state);
  std::unique_ptr<State> state_;
  friend std::expected<WriterLock, std::wstring> AcquireWriterLock(const DirectoryGuard &, bool);
};

std::expected<DirectoryGuard, std::wstring> OpenDirectory(std::wstring_view path);
std::expected<void, std::wstring> CompareDisjoint(const DirectoryGuard &a, const DirectoryGuard &b);
// Persistent zero-byte lock file: closing the exclusive handle releases the
// lock. Never delete a lock to infer that an owner is stale.
// existing_only probes availability without creating a missing lock file.
std::expected<WriterLock, std::wstring> AcquireWriterLock(const DirectoryGuard &directory,
                                                        bool existing_only = false);
std::expected<std::vector<uint8_t>, std::wstring> ReadFile(
    const DirectoryGuard &directory, std::wstring_view simple_name, size_t maximum_bytes);
// Requires a caller-held WriterLock for application-level writer coordination.
// Flushes a new same-directory file, then atomically replaces the destination.
// Failed temp files are preserved. No deletion/adoption/migration is performed.
std::expected<void, std::wstring> AtomicWrite(
    const DirectoryGuard &directory, std::wstring_view simple_name, std::span<const uint8_t> bytes);

} // namespace mcla::launcher::fs
