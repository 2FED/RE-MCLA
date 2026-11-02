#include "mcla_m10_defaults.h"

#include <cstdio>
#include <cstdlib>
#include <string_view>

namespace {
int checks = 0;
void Check(bool value, const char *message) {
  if (!value) {
    std::fprintf(stderr, "FAIL: %s\n", message);
    std::exit(1);
  }
  ++checks;
}

using mcla::m10_defaults::Action;
using mcla::m10_defaults::Decide;
using mcla::m10_defaults::Default;
using mcla::m10_defaults::Inputs;
using mcla::m10_defaults::kDefaults;
using mcla::m10_defaults::kNeverWritten;
using mcla::m10_defaults::Phase;
using mcla::m10_defaults::Tally;
namespace dev = mcla::dev_overrides;

const Default *Find(std::string_view cvar) {
  for (const auto &entry : kDefaults) {
    if (entry.cvar == cvar) {
      return &entry;
    }
  }
  return nullptr;
}

// A volunteer build at a fresh start: option on, switch on, cvar registered
// and at its default, no override file.
constexpr Inputs Fresh() {
  Inputs in;
  in.option_on = true;
  in.switch_on = true;
  in.measurement_build = false;
  in.registered = true;
  in.non_default = false;
  in.dev_override_applied = false;
  in.holds_value = false;
  return in;
}

void TestTable() {
  struct Expected {
    std::string_view cvar;
    bool value;
    Phase phase;
    bool diagnostic;
  };
  constexpr Expected expected[] = {
      {"memory_watch_census", false, Phase::kStartup, true},
      {"d3d12_readback_sync_targeted", true, Phase::kGraphicsPlugin, false},
      {"d3d12_readback_elide_unread", true, Phase::kGraphicsPlugin, false},
      {"d3d12_wrm_event_wake", true, Phase::kGraphicsPlugin, false},
      {"d3d12_replay_batched_wake", true, Phase::kGraphicsPlugin, false},
      {"d3d12_readback_gpu_timing", false, Phase::kGraphicsPlugin, true},
  };
  Check(std::size(expected) == kDefaults.size(), "six defaults");
  for (const auto &e : expected) {
    const Default *entry = Find(e.cvar);
    Check(entry != nullptr, "every expected cvar is in the table");
    Check(entry->value == e.value, "value");
    Check(entry->phase == e.phase, "phase");
    Check(entry->diagnostic == e.diagnostic, "diagnostic flag");
  }
  // Every override key must name the same cvar and the same phase as the dev
  // override whitelist, so its applied record really is this cvar's arm.
  for (const auto &entry : kDefaults) {
    if (!entry.override_key) {
      continue;
    }
    const auto &info = dev::Info(*entry.override_key);
    Check(dev::CvarName(info) == entry.cvar, "override key writes the same cvar");
    const bool same_phase =
        (info.phase == dev::Phase::kStartup && entry.phase == Phase::kStartup) ||
        (info.phase == dev::Phase::kGraphicsPlugin && entry.phase == Phase::kGraphicsPlugin);
    Check(same_phase, "override key is applied in the same phase");
  }
  Check(!Find("d3d12_readback_gpu_timing")->override_key, "gpu timing has no override key");
  // Every dev-override key that writes one of these cvars is linked.
  for (size_t i = 0; i < dev::kKeyCount; ++i) {
    const Default *entry = Find(dev::CvarName(dev::kKeys[i]));
    if (entry) {
      Check(entry->override_key && *entry->override_key == static_cast<dev::Key>(i),
            "every override key of a default cvar is linked");
    }
  }
  for (const auto never : kNeverWritten) {
    Check(Find(never) == nullptr, "net-negative switches are never written");
  }
  Check(Find("memory_watch_probes") == nullptr, "probes not written");
  Check(Find("d3d12_readback_sync_composite") == nullptr,
        "composite stays with mcla_readback_sync");
  Check(Find("d3d12_readback_sync_kick_draws") == nullptr, "kick draws keeps its SDK default");
  Check(Find("mcla_native_d3d_refcount") == nullptr && Find("mcla_native_xapi_interlocked") == nullptr,
        "host native switches already default true");
  for (size_t i = 0; i < kDefaults.size(); ++i) {
    for (size_t j = i + 1; j < kDefaults.size(); ++j) {
      Check(kDefaults[i].cvar != kDefaults[j].cvar, "no cvar twice");
    }
  }
}

void TestDecision() {
  for (const auto &entry : kDefaults) {
    Check(Decide(entry, Fresh()) == Action::kApply, "fresh volunteer start applies");

    Inputs in = Fresh();
    in.option_on = false;
    in.switch_on = false;
    in.dev_override_applied = true;
    Check(Decide(entry, in) == Action::kDisabledByOption, "option off wins over everything");

    in = Fresh();
    in.switch_on = false;
    Check(Decide(entry, in) == Action::kOffBySwitch, "host switch off");

    in = Fresh();
    in.registered = false;
    Check(Decide(entry, in) == Action::kUnavailable, "older SDK without the cvar");

    in = Fresh();
    in.non_default = true;
    Check(Decide(entry, in) == Action::kUserSet, "command line or config value kept");

    in = Fresh();
    in.holds_value = true;
    Check(Decide(entry, in) == Action::kAlready, "SDK default already equal");

    // Owner A/B arm: the override file wrote the cvar, maybe to its SDK
    // default (non_default false). It must win.
    in = Fresh();
    in.dev_override_applied = true;
    in.measurement_build = true;
    const Action owner = Decide(entry, in);
    if (entry.diagnostic) {
      Check(owner == Action::kMeasurementBuild, "diagnostics untouched in owner builds");
    } else {
      Check(owner == Action::kDevOverride, "dev override arm wins");
    }
    in.non_default = true;
    Check(Decide(entry, in) != Action::kApply, "never overwrite an arm set to a non-default");

    // Measurement build without a file value.
    in = Fresh();
    in.measurement_build = true;
    Check(Decide(entry, in) ==
              (entry.diagnostic ? Action::kMeasurementBuild : Action::kApply),
          "owner build without the file: speed-ups on, diagnostics at SDK default");
  }
  // A key with no override key ignores the applied flag (it cannot be set).
  const Default *timing = Find("d3d12_readback_gpu_timing");
  Inputs in = Fresh();
  in.dev_override_applied = true;
  Check(Decide(*timing, in) == Action::kApply, "applied flag without a key is ignored");
}

void TestExhaustive() {
  // Over all input combinations: kApply only for option on, switch on,
  // registered, default value, no applied file value (for keyed entries), and
  // not a diagnostic switch in a measurement build.
  size_t applies = 0;
  for (const auto &entry : kDefaults) {
    for (unsigned bits = 0; bits < 128; ++bits) {
      Inputs in;
      in.option_on = bits & 1;
      in.switch_on = bits & 2;
      in.measurement_build = bits & 4;
      in.registered = bits & 8;
      in.non_default = bits & 16;
      in.dev_override_applied = bits & 32;
      in.holds_value = bits & 64;
      const bool expect_apply = in.option_on && in.switch_on && in.registered &&
                                !in.non_default && !in.holds_value &&
                                !(entry.override_key && in.dev_override_applied) &&
                                !(entry.diagnostic && in.measurement_build);
      const Action action = Decide(entry, in);
      Check((action == Action::kApply) == expect_apply, "apply only in the exact case");
      applies += action == Action::kApply ? 1 : 0;
    }
  }
  // Free inputs in the apply case: measurement_build and dev_override_applied.
  // Four keyed speed-ups: 2 each (applied must be false); census (keyed,
  // diagnostic): 1; GPU timing (no key, diagnostic): 2.
  Check(applies == 4 * 2 + 1 + 2, "apply case count");
}

void TestTally() {
  Tally tally;
  tally.Add(Action::kApply);
  tally.Add(Action::kApply);
  tally.Add(Action::kUserSet);
  Check(tally.Count(Action::kApply) == 2, "apply count");
  Check(tally.Count(Action::kUserSet) == 1, "user set count");
  Check(tally.Count(Action::kDevOverride) == 0, "dev override count");
  Check(std::string_view(mcla::m10_defaults::ActionName(Action::kApply)) == "applied",
        "action name");
  Check(mcla::m10_defaults::ValueText(false) == "false", "value text");
}
}  // namespace

int main() {
  TestTable();
  TestDecision();
  TestExhaustive();
  TestTally();
  std::printf("mcla_m10_defaults_test: %d checks passed\n", checks);
  return 0;
}
