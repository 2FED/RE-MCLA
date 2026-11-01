#include "mcla_dev_overrides.h"

#include <cstdio>
#include <cstdlib>
#include <string>

namespace {
int checks = 0;
void Check(bool value, const char *message) {
  if (!value) {
    std::fprintf(stderr, "FAIL: %s\n", message);
    std::exit(1);
  }
  ++checks;
}

using mcla::dev_overrides::ApplyAction;
using mcla::dev_overrides::DecideApply;
using mcla::dev_overrides::FileStatus;
using mcla::dev_overrides::FindKey;
using mcla::dev_overrides::Key;
using mcla::dev_overrides::LineIssue;
using mcla::dev_overrides::Parse;
using mcla::dev_overrides::Parsed;
using mcla::dev_overrides::Phase;

bool NoValues(const Parsed &parsed) {
  return parsed.ValueCount() == 0;
}

void TestWhitelist() {
  Check(mcla::dev_overrides::kKeyCount == 8, "eight whitelisted names");
  Check(FindKey("mcla_readback_sync_composite") == Key::kReadbackSyncComposite, "readback key");
  Check(FindKey("mcla_native_d3d_refcount") == Key::kNativeD3dRefcount, "refcount key");
  Check(FindKey("mcla_native_xapi_interlocked") == Key::kNativeXapiInterlocked, "xapi key");
  Check(FindKey("d3d12_wrm_event_wake") == Key::kWrmEventWake, "wrm key");
  Check(FindKey("d3d12_replay_batched_wake") == Key::kReplayBatchedWake, "replay key");
  Check(FindKey("d3d12_readback_elide_unread") == Key::kReadbackElideUnread, "elide key");
  Check(FindKey("mcla_gpu_backend_vulkan") == Key::kGpuBackendVulkan, "vulkan backend key");
  Check(FindKey("prewarm_seed_store") == Key::kPrewarmSeedStore, "store seeding key");
  Check(!FindKey("prewarm_seed"), "store seeding prefix");
  for (size_t i = 0; i < mcla::dev_overrides::kKeyCount; ++i) {
    Check(FindKey(mcla::dev_overrides::kKeys[i].name) == static_cast<Key>(i),
          "kKeys is indexed by Key");
  }
  // Names that must stay out of the whitelist (other cvars, near misses).
  Check(!FindKey("d3d12_readback_sync_composite"), "SDK readback cvar not whitelisted");
  Check(!FindKey("readback_resolve"), "readback_resolve not whitelisted");
  Check(!FindKey("d3d12_perf_split"), "perf split not whitelisted");
  Check(!FindKey("d3d12_readback_gpu_timing"), "read-back GPU timing not whitelisted");
  Check(!FindKey("d3d12_readback_elide"), "elide prefix");
  Check(!FindKey("vsync"), "vsync not whitelisted");
  Check(!FindKey("gpu_backend"), "SDK gpu_backend string cvar not whitelisted");
  Check(!FindKey("mcla_gpu_backend"), "vulkan key prefix");
  Check(!FindKey("vulkan_device"), "vulkan_device not whitelisted");
  Check(!FindKey("mcla_native_d3d_refcountX"), "suffix");
  Check(!FindKey("MCLA_NATIVE_D3D_REFCOUNT"), "case sensitive");
  Check(!FindKey(""), "empty name");
  Check(mcla::dev_overrides::Info(Key::kReadbackSyncComposite).phase == Phase::kStartup,
        "readback host switch at startup");
  Check(mcla::dev_overrides::Info(Key::kNativeD3dRefcount).phase == Phase::kStartup,
        "refcount at startup (init-only, before the guest)");
  Check(mcla::dev_overrides::Info(Key::kNativeXapiInterlocked).phase == Phase::kStartup,
        "xapi at startup");
  Check(mcla::dev_overrides::Info(Key::kWrmEventWake).phase == Phase::kGraphicsPlugin,
        "wrm after the plugin registered it");
  Check(mcla::dev_overrides::Info(Key::kReplayBatchedWake).phase == Phase::kGraphicsPlugin,
        "replay after the plugin registered it");
  Check(mcla::dev_overrides::Info(Key::kReadbackElideUnread).phase == Phase::kGraphicsPlugin,
        "elide after the plugin registered it");
  Check(mcla::dev_overrides::Info(Key::kGpuBackendVulkan).phase == Phase::kStartup,
        "vulkan backend at startup (OnPostInitLogging precedes OnPreSetup)");
  Check(mcla::dev_overrides::Info(Key::kPrewarmSeedStore).phase == Phase::kLauncher,
        "store seeding is a launcher key (no host cvar)");
  {
    const Parsed p = Parse("prewarm_seed_store=false\n");
    Check(p.status == FileStatus::kParsed && p.Value(Key::kPrewarmSeedStore) == false,
          "store seeding off parses");
  }
  Check(mcla::dev_overrides::kFile == "mcla-dev-overrides.txt", "file name (same as l55)");
  Check(mcla::dev_overrides::kDirectory == "MCLA-R-Launcher-v1", "launcher per-user directory");
  Check(mcla::dev_overrides::kMaxBytes == 1024, "1 KiB limit");
}

void TestValidFiles() {
  {
    const Parsed p = Parse("");
    Check(p.status == FileStatus::kParsed && NoValues(p) && p.ignored_lines == 0, "empty file");
  }
  {
    // The l55 single-line format keeps working.
    const Parsed p = Parse("mcla_readback_sync_composite=false\r\n");
    Check(p.status == FileStatus::kParsed, "l55 line parsed");
    Check(p.Value(Key::kReadbackSyncComposite) == false, "l55 line value");
    Check(p.ValueCount() == 1 && p.ignored_lines == 0, "l55 line only");
  }
  {
    const Parsed p = Parse("mcla_readback_sync_composite=true");
    Check(p.Value(Key::kReadbackSyncComposite) == true, "no final line end");
  }
  {
    const Parsed p = Parse(
        "mcla_readback_sync_composite=true\r\n"
        "mcla_native_d3d_refcount=false\r\n"
        "mcla_native_xapi_interlocked=false\r\n"
        "d3d12_wrm_event_wake=true\r\n"
        "d3d12_replay_batched_wake=false\r\n"
        "d3d12_readback_elide_unread=true\r\n");
    Check(p.status == FileStatus::kParsed && p.ValueCount() == 6 && p.ignored_lines == 0,
          "six keys");
    Check(p.Value(Key::kReadbackSyncComposite) == true, "all six: readback");
    Check(p.Value(Key::kNativeD3dRefcount) == false, "all six: refcount");
    Check(p.Value(Key::kNativeXapiInterlocked) == false, "all six: xapi");
    Check(p.Value(Key::kWrmEventWake) == true, "all six: wrm");
    Check(p.Value(Key::kReplayBatchedWake) == false, "all six: replay");
    Check(p.Value(Key::kReadbackElideUnread) == true, "all six: elide");
  }
  {
    // The l58 arm files: six lines of at most 37 bytes fit the 1 KiB limit.
    const Parsed p = Parse(
        "mcla_readback_sync_composite=true\r\n"
        "mcla_native_d3d_refcount=false\r\n"
        "mcla_native_xapi_interlocked=false\r\n"
        "d3d12_wrm_event_wake=false\r\n"
        "d3d12_replay_batched_wake=false\r\n"
        "d3d12_readback_elide_unread=false\r\n");
    Check(p.ValueCount() == 6 && p.Value(Key::kReadbackElideUnread) == false &&
              p.Value(Key::kReadbackSyncComposite) == true,
          "l58 baseline arm");
  }
  {
    // The l59 Deck arm files (private/l59-deck-arms/<arm>/mcla-dev-overrides.txt,
    // ASCII LF): all seven keys, read-back sync on, Vulkan on or off.
    constexpr const char *kVulkanArm =
        "mcla_readback_sync_composite=true\n"
        "mcla_native_d3d_refcount=false\n"
        "mcla_native_xapi_interlocked=false\n"
        "d3d12_wrm_event_wake=false\n"
        "d3d12_replay_batched_wake=false\n"
        "d3d12_readback_elide_unread=false\n"
        "mcla_gpu_backend_vulkan=true\n";
    const Parsed v = Parse(kVulkanArm);
    Check(v.status == FileStatus::kParsed && v.ValueCount() == 7 && v.ignored_lines == 0,
          "l59 vulkan arm: seven keys");
    Check(v.Value(Key::kGpuBackendVulkan) == true &&
              v.Value(Key::kReadbackSyncComposite) == true &&
              v.Value(Key::kReadbackElideUnread) == false &&
              v.Value(Key::kNativeD3dRefcount) == false,
          "l59 vulkan arm values");
    const Parsed d = Parse(
        "mcla_readback_sync_composite=true\n"
        "mcla_native_d3d_refcount=false\n"
        "mcla_native_xapi_interlocked=false\n"
        "d3d12_wrm_event_wake=false\n"
        "d3d12_replay_batched_wake=false\n"
        "d3d12_readback_elide_unread=false\n"
        "mcla_gpu_backend_vulkan=false\n");
    Check(d.ValueCount() == 7 && d.ignored_lines == 0 &&
              d.Value(Key::kGpuBackendVulkan) == false &&
              d.Value(Key::kReadbackSyncComposite) == true,
          "l59 d3d12 arm");
    const Parsed bad = Parse("mcla_gpu_backend_vulkan=vulkan\ngpu_backend=true\n");
    Check(bad.ValueCount() == 0 && bad.ignored_lines == 2 &&
              bad.issues[0].issue == LineIssue::kMalformed &&
              bad.issues[1].issue == LineIssue::kUnknownName,
          "no string backend values, gpu_backend unknown");
  }
  {
    const Parsed p = Parse("d3d12_wrm_event_wake=true\nd3d12_replay_batched_wake=true\n");
    Check(p.ValueCount() == 2 && p.Value(Key::kWrmEventWake) == true &&
              p.Value(Key::kReplayBatchedWake) == true && !p.Value(Key::kNativeD3dRefcount),
          "LF line ends, absent keys stay unset");
  }
  {
    const Parsed p = Parse("# l57 arm: wrm\r\n\r\n\nd3d12_wrm_event_wake=true\r\n\r\n");
    Check(p.ValueCount() == 1 && p.ignored_lines == 0 && p.Value(Key::kWrmEventWake) == true,
          "comments and empty lines are skipped silently");
  }
}

void TestIgnoredLines() {
  {
    const Parsed p = Parse(
        "d3d12_wrm_event_wake=true\n"
        "d3d12_wrm_event_wake = false\n"  // spaces
        "bogus_name=true\n"               // unknown
        "d3d12_replay_batched_wake=1\n"   // value
        "d3d12_wrm_event_wake=false\n");  // duplicate
    Check(p.status == FileStatus::kParsed, "mixed file parsed");
    Check(p.ValueCount() == 1 && p.Value(Key::kWrmEventWake) == true,
          "only the valid first line applies");
    Check(p.ignored_lines == 4 && p.issue_count == 4, "four lines ignored");
    Check(p.issues[0].line == 2 && p.issues[0].issue == LineIssue::kMalformed, "line 2 spaces");
    Check(p.issues[1].line == 3 && p.issues[1].issue == LineIssue::kUnknownName, "line 3 name");
    Check(p.issues[2].line == 4 && p.issues[2].issue == LineIssue::kMalformed, "line 4 value");
    Check(p.issues[3].line == 5 && p.issues[3].issue == LineIssue::kDuplicate, "line 5 repeat");
  }
  const char *malformed[] = {
      "d3d12_wrm_event_wake",           "d3d12_wrm_event_wake=",
      "=true",                          "d3d12_wrm_event_wake=True",
      "d3d12_wrm_event_wake=TRUE",      "d3d12_wrm_event_wake=yes",
      "d3d12_wrm_event_wake=0",         "d3d12_wrm_event_wake= true",
      "d3d12_wrm_event_wake=true ",     " d3d12_wrm_event_wake=true",
      "\td3d12_wrm_event_wake=true",    "d3d12_wrm_event_wake=true=true",
      "d3d12_wrm_event_wake==true",     "d3d12_wrm_event_wake=true\r\r",
      "d3d12_wrm_event_wake=true;",     "d3d12_wrm_event_wake:true",
  };
  for (const char *line : malformed) {
    const Parsed p = Parse(line);
    Check(p.status == FileStatus::kParsed && NoValues(p) && p.ignored_lines == 1 &&
              p.issues[0].issue == LineIssue::kMalformed,
          line);
  }
  {
    const Parsed p = Parse(std::string("d3d12_wrm_event_wake=true\0", 26));
    Check(NoValues(p) && p.issues[0].issue == LineIssue::kMalformed, "NUL in a line");
  }
  {
    const Parsed p = Parse("d3d12_readback_sync_composite=true\nreadback_resolve=true\n");
    Check(NoValues(p) && p.ignored_lines == 2 && p.issues[0].issue == LineIssue::kUnknownName &&
              p.issues[1].issue == LineIssue::kUnknownName,
          "cvars outside the whitelist are not applied");
  }
  {
    std::string many;
    for (int i = 0; i < 40; ++i) {
      many += "x=true\n";
    }
    const Parsed p = Parse(many);
    Check(p.ignored_lines == 40 && p.issue_count == mcla::dev_overrides::kMaxIssues,
          "issue list is bounded, count is not");
    Check(p.issues[15].line == 16, "issue line numbers");
  }
}

void TestRejectedFiles() {
  {
    const Parsed p = Parse("\xEF\xBB\xBFmcla_readback_sync_composite=false");
    Check(p.status == FileStatus::kRejected && NoValues(p), "BOM rejects the file");
  }
  {
    const Parsed p = Parse("d3d12_wrm_event_wake=true\n# caf\xC3\xA9\n");
    Check(p.status == FileStatus::kRejected && NoValues(p),
          "non-ASCII anywhere (also in a comment) rejects the whole file");
  }
  {
    std::string padded = "d3d12_wrm_event_wake=true\n";
    padded += std::string(mcla::dev_overrides::kMaxBytes - padded.size(), '\n');
    Check(padded.size() == 1024, "padded size");
    const Parsed ok = Parse(padded);
    Check(ok.status == FileStatus::kParsed && ok.Value(Key::kWrmEventWake) == true,
          "exactly 1 KiB accepted");
    padded += '\n';
    const Parsed big = Parse(padded);
    Check(big.status == FileStatus::kRejected && NoValues(big), "1025 bytes rejected");
  }
}

void TestApplyDecision() {
  Check(DecideApply(false, false) == ApplyAction::kUnavailable, "cvar missing");
  Check(DecideApply(false, true) == ApplyAction::kUnavailable, "cvar missing wins");
  Check(DecideApply(true, true) == ApplyAction::kUserSet, "command line wins over the file");
  Check(DecideApply(true, false) == ApplyAction::kApply, "default value: apply");
  static_assert(DecideApply(true, false) == ApplyAction::kApply);
}

void TestConstexpr() {
  constexpr Parsed p = Parse("mcla_native_d3d_refcount=false\nd3d12_wrm_event_wake=true\n");
  static_assert(p.status == FileStatus::kParsed);
  static_assert(p.ValueCount() == 2);
  static_assert(p.Value(Key::kNativeD3dRefcount) == false);
  static_assert(p.Value(Key::kWrmEventWake) == true);
  Check(std::string(mcla::dev_overrides::LineIssueName(LineIssue::kUnknownName)) ==
            "unknown name",
        "issue names");
}
}  // namespace

int main() {
  TestWhitelist();
  TestValidFiles();
  TestIgnoredLines();
  TestRejectedFiles();
  TestApplyDecision();
  TestConstexpr();
  std::printf("mcla_dev_overrides_test: %d checks passed\n", checks);
  return 0;
}
