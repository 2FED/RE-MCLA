#pragma once

// M10-003 step 1: seed a build's shader store from the other stores of this
// installation (docs/evidence/M10-prewarm-union-store-20261101.md).
//
// Before a session the launcher merges, into
// <profile>/cache/<fingerprint>/shaders/shareable/, the store files of every
// other <managed profile>/cache/<64-hex fingerprint>/shaders/shareable/ of the
// profiles in the launcher library (the selected profile included). Only the
// five SDK store file names are read (mcla_shader_store_seed.h); nothing else
// in a profile (saves, sessions, diagnostics) is opened. Source files are never
// modified. Directories are opened through fs::OpenDirectory (no reparse
// ancestors on Windows; KI-049 rules under Wine) and files through
// fs::ReadFile (regular, single-link, non-reparse, bounded). Each changed
// target file is written with fs::AtomicWrite (same-directory temporary file,
// then rename) under the profile writer lock the caller holds. Nothing is
// sent anywhere.
//
// Seeding never blocks a launch: failures are counted in the summary line and
// the session starts with whatever store the build already had.

#include "mcla_dev_overrides.h"
#include "mcla_launcher_fs_win.h"
#include "mcla_shader_store_seed.h"

#include <string>
#include <string_view>
#include <vector>

namespace mcla::launcher {

// The file in the session directory that records the summary line.
inline constexpr std::wstring_view kStoreSeedFile = L"store-seed.txt";

// build_cache is the held <profile>/cache/<fingerprint> directory of the
// session. profile_roots are the managed profile roots of the launcher
// library. With enabled=false nothing is read from other profiles and nothing
// is written; the summary still counts the build's own store.
shader_store::Summary SeedShaderStore(const fs::DirectoryGuard &build_cache,
                                      const std::vector<std::wstring> &profile_roots, bool enabled);

// The switch value from a parsed override file: prewarm_seed_store when the
// file was read and holds a valid line for it, else the launcher default.
constexpr bool StoreSeedSwitch(const dev_overrides::Parsed &parsed, bool launcher_default) {
  if (parsed.status != dev_overrides::FileStatus::kParsed) return launcher_default;
  const auto value = parsed.Value(dev_overrides::Key::kPrewarmSeedStore);
  return value ? *value : launcher_default;
}

// Whether seeding is on for this launch: the launcher default unless the
// owner-internal override file (MCLA_DEV_OVERRIDES launcher builds only) in
// the launcher directory holds prewarm_seed_store=true|false.
bool StoreSeedEnabled(const fs::DirectoryGuard &launcher_directory);

}  // namespace mcla::launcher
