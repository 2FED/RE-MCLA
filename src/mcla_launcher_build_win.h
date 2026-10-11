#pragma once

#include "mcla_launcher_fs_win.h"

#include <cstdint>
#include <expected>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace mcla::launcher {

inline constexpr std::wstring_view kDeveloperBuildManifest = L"mcla-build.manifest";
// ASCII, LF-only, mandatory final LF, strictly basename-sorted entries:
// MCLA-DEV-BUILD-V1\n
// <64 uppercase SHA256 hex> <canonical decimal bytes> <allowlisted basename>\n
// Required: mcla.exe, mcla_crash_handler.exe. All other entries optional.
// The directory contains exactly this manifest and its listed artifacts.
// This manifest is an integrity inventory supplied by the developer, not a
// signature/trust boundary. It proves neither compatibility nor safe PE code.
struct VerifiedBuildFile {
  std::wstring name;
  uint64_t bytes = 0;
  std::string sha256;
};

class VerifiedBuild {
 public:
  struct State;
  VerifiedBuild(VerifiedBuild &&) noexcept;
  VerifiedBuild &operator=(VerifiedBuild &&) noexcept;
  ~VerifiedBuild();
  VerifiedBuild(const VerifiedBuild &) = delete;
  VerifiedBuild &operator=(const VerifiedBuild &) = delete;
  const std::wstring &path() const;
  const std::vector<VerifiedBuildFile> &files() const;
  // SHA256 of exact canonical manifest bytes; ordering/encoding is strict.
  const std::string &fingerprint() const;
 private:
  explicit VerifiedBuild(std::unique_ptr<State> state);
  std::unique_ptr<State> state_;
  friend std::expected<VerifiedBuild, std::wstring> VerifyBuild(const fs::DirectoryGuard &);
};

// Retains its own directory guard and READ-only/no-WRITE/no-DELETE-sharing
// handles for the manifest and every listed artifact until destruction.
// Keep the result alive through the entire child lifetime. No file writes.
std::expected<VerifiedBuild, std::wstring> VerifyBuild(const fs::DirectoryGuard &directory);

} // namespace mcla::launcher
