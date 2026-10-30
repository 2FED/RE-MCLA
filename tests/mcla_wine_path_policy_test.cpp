// KI-049 (M9-006): pure Wine/Proton ancestor policy. No filesystem access.
#include "mcla_wine_path_policy.h"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <initializer_list>

namespace {
int checks = 0;
void Check(bool value, const char *message) {
  if (!value) {
    std::fprintf(stderr, "FAIL: %s\n", message);
    std::exit(1);
  }
  ++checks;
}

using mcla::platform::Accepted;
using mcla::platform::AcceptedThroughWineLink;
using mcla::platform::AncestorFacts;
using mcla::platform::AncestorVerdict;
using mcla::platform::DecideAncestor;
using mcla::platform::DriveTypeAllowed;
using mcla::platform::IsWineUnixLinkTag;
using mcla::platform::RegularFileAttributes;
namespace p = mcla::platform;

constexpr uint32_t kDir = p::kAttributeDirectory;
constexpr uint32_t kReparse = p::kAttributeReparsePoint;

// The decision is constexpr: the Windows branch is fixed at compile time too.
static_assert(DecideAncestor({false, kDir | kReparse, true, p::kTagMountPoint, true, true}) ==
              AncestorVerdict::kRejectReparse);
static_assert(DecideAncestor({true, kDir | kReparse, false, 0, true, true}) ==
              AncestorVerdict::kWineLinkSameDrive);

void TestOrdinaryDirectories() {
  for (const bool wine : {false, true}) {
    Check(DecideAncestor({wine, kDir}) == AncestorVerdict::kNormalDirectory, "plain directory accepted");
    Check(DecideAncestor({wine, kDir | 0x20 /*archive*/ | 0x01 /*read-only*/}) ==
              AncestorVerdict::kNormalDirectory, "directory with ordinary attributes accepted");
    Check(DecideAncestor({wine, 0x20}) == AncestorVerdict::kRejectNotDirectory, "file ancestor refused");
    Check(DecideAncestor({wine, 0}) == AncestorVerdict::kRejectNotDirectory, "attribute-less ancestor refused");
  }
}

void TestWindowsNeverRelaxes() {
  // Every combination of the Wine facts is irrelevant on real Windows.
  for (const uint32_t attributes : {kReparse, kDir | kReparse}) {
    for (const bool tag_known : {false, true}) {
      for (const uint32_t tag : {0u, p::kTagMountPoint, p::kTagSymlink, p::kTagLxSymlink, 0x80000017u}) {
        for (const bool target_dir : {false, true}) {
          for (const bool same : {false, true}) {
            const auto verdict = DecideAncestor({false, attributes, tag_known, tag, target_dir, same});
            Check(verdict == AncestorVerdict::kRejectReparse, "Windows reparse ancestor refused");
            Check(!Accepted(verdict), "Windows reparse ancestor not accepted");
          }
        }
      }
    }
  }
}

void TestWineBranch() {
  // Z:\ root and SteamOS mount points: no tag, followed target on Z:.
  auto verdict = DecideAncestor({true, kDir | kReparse, false, 0, true, true});
  Check(verdict == AncestorVerdict::kWineLinkSameDrive, "Wine root/mount same drive accepted");
  Check(Accepted(verdict) && AcceptedThroughWineLink(verdict), "same-drive acceptance flagged as Wine link");
  // Symlink reported without the directory bit itself: target decides.
  verdict = DecideAncestor({true, kReparse, true, p::kTagLxSymlink, true, true});
  Check(verdict == AncestorVerdict::kWineLinkSameDrive, "Wine symlink without own directory bit accepted");
  // Foreign drive letter (removable media given its own letter).
  for (const uint32_t tag : {p::kTagLxSymlink, p::kTagSymlink, p::kTagMountPoint}) {
    verdict = DecideAncestor({true, kDir | kReparse, true, tag, true, false});
    Check(verdict == AncestorVerdict::kWineLinkKnownTag, "Wine foreign drive with Unix tag accepted");
    Check(AcceptedThroughWineLink(verdict), "known-tag acceptance flagged as Wine link");
  }
  // Foreign drive without a tag, or with an unrelated tag, is refused.
  Check(DecideAncestor({true, kDir | kReparse, false, 0, true, false}) == AncestorVerdict::kRejectTarget,
        "Wine foreign drive without tag refused");
  Check(DecideAncestor({true, kDir | kReparse, true, 0x80000017u, true, false}) == AncestorVerdict::kRejectTarget,
        "Wine foreign drive with unrelated tag refused");
  Check(DecideAncestor({true, kDir | kReparse, false, p::kTagLxSymlink, true, false}) == AncestorVerdict::kRejectTarget,
        "unread tag value is ignored");
  // Target that is not a directory (dangling link, link to a file).
  for (const bool same : {false, true}) {
    Check(DecideAncestor({true, kDir | kReparse, true, p::kTagLxSymlink, false, same}) ==
              AncestorVerdict::kRejectTarget, "Wine link to non-directory refused");
    Check(DecideAncestor({true, kReparse, false, 0, false, same}) == AncestorVerdict::kRejectTarget,
          "Wine dangling link refused");
  }
  Check(!Accepted(AncestorVerdict::kRejectTarget) && !Accepted(AncestorVerdict::kRejectNotDirectory) &&
            !Accepted(AncestorVerdict::kRejectReparse), "reject verdicts are not accepted");
  Check(Accepted(AncestorVerdict::kNormalDirectory) && !AcceptedThroughWineLink(AncestorVerdict::kNormalDirectory),
        "normal directory is not a Wine link acceptance");
}

void TestTags() {
  Check(IsWineUnixLinkTag(0xA000001Du), "LX symlink tag value");
  Check(IsWineUnixLinkTag(0xA000000Cu), "symlink tag value");
  Check(IsWineUnixLinkTag(0xA0000003u), "mount point tag value");
  Check(!IsWineUnixLinkTag(0u) && !IsWineUnixLinkTag(0x80000017u) && !IsWineUnixLinkTag(0x9000001Au),
        "other tags are not Unix links");
}

void TestDrives() {
  Check(DriveTypeAllowed(false, p::kDriveFixed) && DriveTypeAllowed(true, p::kDriveFixed), "fixed drive allowed");
  Check(!DriveTypeAllowed(false, p::kDriveRemovable), "Windows removable drive refused");
  Check(DriveTypeAllowed(true, p::kDriveRemovable), "Wine removable drive allowed");
  for (const bool wine : {false, true}) {
    for (const unsigned type : {0u, 1u, 4u, 5u, 6u})  // unknown, no root, remote, cdrom, ramdisk
      Check(!DriveTypeAllowed(wine, type), "other drive types refused");
  }
}

void TestFilesNeverRelaxed() {
  Check(RegularFileAttributes(0x20, 1) && RegularFileAttributes(0x80, 1), "regular file accepted");
  Check(!RegularFileAttributes(0x20 | kReparse, 1), "reparse file refused");
  Check(!RegularFileAttributes(kDir, 1), "directory refused as file");
  Check(!RegularFileAttributes(p::kAttributeDevice, 1), "device refused as file");
  Check(!RegularFileAttributes(0x20, 2) && !RegularFileAttributes(0x20, 0), "link count must be one");
}
} // namespace

int main() {
  TestOrdinaryDirectories();
  TestWindowsNeverRelaxes();
  TestWineBranch();
  TestTags();
  TestDrives();
  TestFilesNeverRelaxed();
  std::printf("Wine path policy: %d checks, 0 failures\n", checks);
  return 0;
}
