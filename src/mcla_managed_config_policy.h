#pragma once

#include "mcla_launcher_profile_policy.h"

#include <array>
#include <optional>
#include <string>
#include <string_view>

namespace mcla::managed_config {

// This file is deliberately not a settings store. The launcher passes reviewed
// settings on CLI and removes inherited REX_* overrides from the child only.
inline constexpr std::string_view kSentinel =
    "# MCLA managed session v1; settings are passed on CLI.\n";

enum class Issue { InvalidSelector, InvalidRequiredPath, GameRootOverlap };

struct RequiredPaths {
  std::u16string game;
  std::u16string user;
  std::u16string cache;
  std::u16string update;
  std::u16string metadata;
  std::u16string log_file;
  std::u16string diagnostics;
};

inline bool IsSentinel(std::string_view bytes) { return bytes == kSentinel; }

// Lexical defense in depth, NOT filesystem admission. The launcher must hold
// canonical root/ancestor handles and writer locks and validate the full layout.
// All paths are explicit absolute local Windows paths, including output files.
inline std::optional<Issue> Validate(std::u16string_view selector,
                                     const RequiredPaths &paths) {
  const auto config = launcher::NormalizeRoot(selector);
  if (!config) return Issue::InvalidSelector;
  const std::array<std::u16string_view, 7> values{
      paths.game, paths.user, paths.cache, paths.update, paths.metadata,
      paths.log_file, paths.diagnostics};
  std::array<std::u16string, 7> normalized;
  for (size_t i = 0; i < values.size(); ++i) {
    auto root = launcher::NormalizeRoot(values[i]);
    if (!root) return Issue::InvalidRequiredPath;
    normalized[i] = std::move(*root);
  }
  if (launcher::RootsOverlap(normalized[0], *config))
    return Issue::GameRootOverlap;
  for (size_t i = 1; i < normalized.size(); ++i) {
    if (launcher::RootsOverlap(normalized[0], normalized[i]))
      return Issue::GameRootOverlap;
  }
  return std::nullopt;
}

} // namespace mcla::managed_config
