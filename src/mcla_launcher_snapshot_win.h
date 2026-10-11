#pragma once
#include "mcla_launcher_fs_win.h"
#include <string>

namespace mcla::launcher {
// Copy a complete bounded tree to an already-created EMPTY destination.
// Caller holds the source writer lock. Every source file is held read-only
// throughout the copy; every directory ancestor is held against replacement.
// The lock belongs outside the copied subtree (no lock file is skipped).
// Caller also exclusively owns destination publication. Cooperative source
// membership drift is rejected by a final full-tree identity/size/time scan;
// this is not an adversarial snapshot against an uncooperative external writer.
// Destination bytes are flushed, read back and SHA256-verified while still held
// against writes/deletion. Named streams, links and ambiguous names fail closed.
// No merge, overwrite, deletion, filtering or save-format interpretation.
// On failure the new destination is preserved, never advertised as complete.
std::expected<std::string, std::wstring> CopySaveTree(
    const fs::DirectoryGuard &source, const fs::DirectoryGuard &destination);
#if defined(MCLA_LAUNCHER_SNAPSHOT_TEST_SEAM)
// Nonshipping deterministic mutation fixture. Never enabled in launcher builds.
std::expected<std::string, std::wstring> CopySaveTreeBeforeVerifyForTest(
    const fs::DirectoryGuard &source, const fs::DirectoryGuard &destination,
    void (*before_verify)(void *), void *context);
#endif
}
