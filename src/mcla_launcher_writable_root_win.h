#pragma once

#include "mcla_launcher_fs_win.h"

namespace mcla::launcher {

// Writable personal roots only, not a general read/build-root restriction.
// Reject system/application subtrees and exact user/known-folder containers.
// DirectoryGuard already enforces local fixed-drive/non-reparse ancestry.
// This checks both OS-provided spelling and its resolved canonical spelling;
// it performs no filesystem mutation and does not grant launch authorization.
std::expected<void, std::wstring> ValidateWritableRoot(const fs::DirectoryGuard &directory);

} // namespace mcla::launcher
