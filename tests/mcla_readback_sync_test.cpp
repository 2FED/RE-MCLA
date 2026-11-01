#include "mcla_readback_sync.h"

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

using mcla::readback_sync::DecideStart;
using mcla::readback_sync::EffectiveHostValue;
using mcla::readback_sync::OverrideStatus;
using mcla::readback_sync::ParseOverride;
using mcla::readback_sync::StartAction;

void TestParseOverride() {
  Check(ParseOverride("mcla_readback_sync_composite=false") == OverrideStatus::kFalse,
        "false without a line ending");
  Check(ParseOverride("mcla_readback_sync_composite=false\n") == OverrideStatus::kFalse,
        "false with LF");
  Check(ParseOverride("mcla_readback_sync_composite=false\r\n") == OverrideStatus::kFalse,
        "false with CRLF (cmd echo)");
  Check(ParseOverride("mcla_readback_sync_composite=true\r\n") == OverrideStatus::kTrue,
        "true with CRLF");
  Check(ParseOverride("") == OverrideStatus::kInvalid, "empty file");
  Check(ParseOverride("\r\n") == OverrideStatus::kInvalid, "blank line");
  Check(ParseOverride("mcla_readback_sync_composite=") == OverrideStatus::kInvalid,
        "missing value");
  Check(ParseOverride("mcla_readback_sync_composite") == OverrideStatus::kInvalid,
        "missing equals sign");
  Check(ParseOverride("mcla_readback_sync_composite=False") == OverrideStatus::kInvalid,
        "values are case sensitive");
  Check(ParseOverride("mcla_readback_sync_composite=0") == OverrideStatus::kInvalid,
        "numbers are not accepted");
  Check(ParseOverride("mcla_readback_sync_composite = false") == OverrideStatus::kInvalid,
        "no spaces");
  Check(ParseOverride("mcla_readback_sync_composite=false \r\n") == OverrideStatus::kInvalid,
        "no trailing space");
  Check(ParseOverride(" mcla_readback_sync_composite=false") == OverrideStatus::kInvalid,
        "no leading space");
  Check(ParseOverride("mcla_readback_sync_composite=false\n\n") == OverrideStatus::kInvalid,
        "exactly one line");
  Check(ParseOverride("mcla_readback_sync_composite=false\r\nother=1\r\n") ==
            OverrideStatus::kInvalid,
        "no second key");
  Check(ParseOverride("d3d12_readback_sync_composite=false") == OverrideStatus::kInvalid,
        "only the host switch");
  Check(ParseOverride("mcla_readback_sync_compositeX=false") == OverrideStatus::kInvalid,
        "the key must match exactly");
  Check(ParseOverride("\xEF\xBB\xBFmcla_readback_sync_composite=false") ==
            OverrideStatus::kInvalid,
        "a UTF-8 BOM is not ASCII");
  Check(ParseOverride(std::string("mcla_readback_sync_composite=false\0", 35)) ==
            OverrideStatus::kInvalid,
        "an embedded NUL is rejected");
  std::string long_line = "mcla_readback_sync_composite=false" + std::string(300, '\n');
  Check(ParseOverride(long_line) == OverrideStatus::kInvalid, "more than 256 bytes");
  static_assert(ParseOverride("mcla_readback_sync_composite=false") == OverrideStatus::kFalse);
}

void TestEffectiveValue() {
  Check(EffectiveHostValue(true, false, OverrideStatus::kAbsent), "build default on");
  Check(!EffectiveHostValue(true, false, OverrideStatus::kFalse), "file turns the fix off");
  Check(EffectiveHostValue(false, false, OverrideStatus::kTrue), "file turns the fix on");
  Check(EffectiveHostValue(true, false, OverrideStatus::kInvalid), "invalid file is ignored");
  Check(EffectiveHostValue(true, true, OverrideStatus::kFalse), "command line on beats the file");
  Check(!EffectiveHostValue(false, true, OverrideStatus::kTrue),
        "command line off beats the file");
}

void TestStartDecision() {
  Check(DecideStart(false, true, false, true) == StartAction::kDisabledByOption,
        "option off never writes the cvar");
  Check(DecideStart(false, false, false, true) == StartAction::kDisabledByOption,
        "option off wins over a missing cvar");
  Check(DecideStart(true, false, false, true) == StartAction::kUnavailable,
        "pinned SDK without the cvar");
  Check(DecideStart(true, true, true, true) == StartAction::kSdkUserSet,
        "a user value of the SDK cvar is left alone");
  Check(DecideStart(true, true, true, false) == StartAction::kSdkUserSet,
        "the SDK cvar wins over the host switch");
  Check(DecideStart(true, true, false, false) == StartAction::kOffByHost,
        "host switch off (A/B control)");
  Check(DecideStart(true, true, false, true) == StartAction::kEnable,
        "option on, SDK support, switch on: enable");
  static_assert(DecideStart(true, true, false, true) == StartAction::kEnable);
}

void TestNames() {
  Check(mcla::readback_sync::kSdkCvar == "d3d12_readback_sync_composite",
        "SDK cvar name matches the SDK");
  Check(mcla::readback_sync::kHostCvar == "mcla_readback_sync_composite", "host cvar name");
  Check(mcla::readback_sync::kOverrideFile == "mcla-dev-overrides.txt", "override file name");
  Check(mcla::readback_sync::kOverrideDirectory == "MCLA-R-Launcher-v1",
        "override directory is the launcher's per-user directory");
  Check(std::string(mcla::readback_sync::OverrideStatusName(OverrideStatus::kFalse)) == "false",
        "status names");
}
}  // namespace

int main() {
  TestParseOverride();
  TestEffectiveValue();
  TestStartDecision();
  TestNames();
  std::printf("mcla_readback_sync_test: %d checks passed\n", checks);
  return 0;
}
