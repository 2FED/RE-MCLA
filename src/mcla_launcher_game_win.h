#pragma once

#include "mcla_launcher_fs_win.h"

#include <cstdint>
#include <expected>
#include <memory>
#include <span>
#include <string>
#include <vector>

namespace mcla::launcher {

struct VerifiedGameFile {
  std::wstring relative_path;
  uint64_t bytes = 0;
  std::string sha256;
};

class VerifiedGame {
 public:
  struct State;
  VerifiedGame(VerifiedGame &&) noexcept;
  VerifiedGame &operator=(VerifiedGame &&) noexcept;
  ~VerifiedGame();
  VerifiedGame(const VerifiedGame &) = delete;
  VerifiedGame &operator=(const VerifiedGame &) = delete;
  const std::wstring &path() const;
  const std::vector<VerifiedGameFile> &files() const;
  const std::string &fingerprint() const;
 private:
  explicit VerifiedGame(std::unique_ptr<State> state);
  std::unique_ptr<State> state_;
  friend struct GameVerifier;
};

// Embedded supported-revision metadata only, never expected hashes generated
// from the selected directory. Exact inventory + all 15 file hashes are checked.
// Retains all file and ancestor-directory handles throughout the child lifetime.
// No ownership/DRM/redistribution claim, import, cache-trust shortcut or writes.
std::expected<VerifiedGame, std::wstring> VerifyGameRoot(const fs::DirectoryGuard &directory);
std::span<const VerifiedGameFile> SupportedGameFiles();

#ifdef MCLA_LAUNCHER_GAME_TESTING
// Compiled into the standalone synthetic test target ONLY. Do not define this
// macro for the launcher: its expected metadata cannot be selected by users.
std::expected<VerifiedGame, std::wstring> VerifyGameRootForTest(
    const fs::DirectoryGuard &directory, std::span<const VerifiedGameFile> expected);
#endif

} // namespace mcla::launcher
