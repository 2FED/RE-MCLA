#pragma once

// KI-049 (M9-006): pure ancestor/file decision used by the launcher filesystem
// guard. Real Windows semantics never change: any reparse ancestor is refused.
// Under Wine/Proton the Linux root (drive Z:) and every mount point or Unix
// symlink to a directory is reported as a reparse point, so the ordinary
// policy made every path outside the C: prefix unreachable. Only under Wine
// an ancestor reparse point is accepted when its followed target is a
// directory and either stays on the requested drive letter or carries a
// link/mount tag that Wine uses for Unix objects. Files themselves are never
// relaxed: they must be regular, non-reparse, non-device and single-link.

#include <cstdint>

namespace mcla::platform {

// Win32 values, duplicated so this header stays free of <windows.h>.
inline constexpr uint32_t kAttributeDirectory = 0x00000010;
inline constexpr uint32_t kAttributeDevice = 0x00000040;
inline constexpr uint32_t kAttributeReparsePoint = 0x00000400;
inline constexpr uint32_t kTagMountPoint = 0xA0000003;  // IO_REPARSE_TAG_MOUNT_POINT
inline constexpr uint32_t kTagSymlink = 0xA000000C;     // IO_REPARSE_TAG_SYMLINK
inline constexpr uint32_t kTagLxSymlink = 0xA000001D;   // IO_REPARSE_TAG_LX_SYMLINK
inline constexpr unsigned kDriveRemovable = 2;          // DRIVE_REMOVABLE
inline constexpr unsigned kDriveFixed = 3;              // DRIVE_FIXED

enum class AncestorVerdict {
  kNormalDirectory,     // not a reparse point and a directory (all platforms)
  kWineLinkSameDrive,   // Wine only: followed target is a directory on the same drive
  kWineLinkKnownTag,    // Wine only: followed target is a directory, Unix link/mount tag
  kRejectNotDirectory,  // not a directory
  kRejectReparse,       // reparse point outside Wine (the ordinary Windows policy)
  kRejectTarget,        // Wine: target is not a directory, or foreign drive without a known tag
};

struct AncestorFacts {
  bool under_wine = false;
  uint32_t attributes = 0;          // as reported for the ancestor itself (no follow)
  bool tag_known = false;           // reparse tag could be read
  uint32_t tag = 0;
  bool target_is_directory = false; // the followed object is a directory
  bool target_same_drive = false;   // the followed final path keeps the drive letter
};

constexpr bool IsWineUnixLinkTag(uint32_t tag) {
  return tag == kTagLxSymlink || tag == kTagSymlink || tag == kTagMountPoint;
}

constexpr AncestorVerdict DecideAncestor(const AncestorFacts &facts) {
  if (!(facts.attributes & kAttributeReparsePoint))
    return (facts.attributes & kAttributeDirectory) ? AncestorVerdict::kNormalDirectory
                                                    : AncestorVerdict::kRejectNotDirectory;
  if (!facts.under_wine) return AncestorVerdict::kRejectReparse;
  if (!facts.target_is_directory) return AncestorVerdict::kRejectTarget;
  if (facts.target_same_drive) return AncestorVerdict::kWineLinkSameDrive;
  if (facts.tag_known && IsWineUnixLinkTag(facts.tag)) return AncestorVerdict::kWineLinkKnownTag;
  return AncestorVerdict::kRejectTarget;
}

constexpr bool Accepted(AncestorVerdict verdict) {
  return verdict == AncestorVerdict::kNormalDirectory ||
         verdict == AncestorVerdict::kWineLinkSameDrive ||
         verdict == AncestorVerdict::kWineLinkKnownTag;
}

constexpr bool AcceptedThroughWineLink(AncestorVerdict verdict) {
  return verdict == AncestorVerdict::kWineLinkSameDrive ||
         verdict == AncestorVerdict::kWineLinkKnownTag;
}

// Local fixed drives always; under Wine a removable drive letter is also
// accepted because Wine's mount manager may give an SD card its own letter.
constexpr bool DriveTypeAllowed(bool under_wine, unsigned drive_type) {
  return drive_type == kDriveFixed || (under_wine && drive_type == kDriveRemovable);
}

// Never relaxed, under Wine included: the file itself must be a regular,
// non-reparse, non-device, single-link object.
constexpr bool RegularFileAttributes(uint32_t attributes, uint32_t link_count) {
  return !(attributes & (kAttributeReparsePoint | kAttributeDirectory | kAttributeDevice)) &&
         link_count == 1;
}

} // namespace mcla::platform
