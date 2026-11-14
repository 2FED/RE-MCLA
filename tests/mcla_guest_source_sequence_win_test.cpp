// Source-only no-GPU test. Include the module once; do not also compile it as a
// separate TU in this target. Define MCLA_GUEST_SOURCE_SEQUENCE_PROBE=1, use matching
// private provenance SDK headers/runtime, link normal rex core/UI dependencies
// plus mcla_launcher_fs_win.cpp and bcrypt, and expose nlohmann_json headers.
// No Run/Presenter/window/GPU/process/input operations or filesystem test writes.
#include "../src/mcla_guest_source_sequence_win.cpp"
#include <nlohmann/json.hpp>
#include <iostream>

REXCVAR_DEFINE_BOOL(mcla_sequence_test_peer_probe, false, "Test", "Synthetic peer");

namespace {
namespace probe = mcla::guest_source_sequence;
using JsonValue = nlohmann::json;
constexpr uint64_t begin = 100000, frequency = 10000;
unsigned checks = 0, failures = 0;
void Check(bool value, const char* label) {
  ++checks;
  if (!value) { ++failures; std::cerr << "FAIL " << label << '\n'; }
}
template<class F> void Throws(F&& action, const char* reason) {
  bool expected = false;
  try { action(); }
  catch (const probe::Failure& failure) { expected = std::string_view(failure.reason) == reason; }
  catch (...) {}
  Check(expected, reason);
}

struct Fake {
  uint64_t clock = begin;
  size_t captures = 0, waits = 0;
  std::stop_source cancellation;
  std::function<void(Fake&, uint64_t)> waiting;
  std::function<void(Fake&, rex::ui::GuestOutputCapture&)> modify;
  rex::ui::GuestCaptureStatus outcome = rex::ui::GuestCaptureStatus::kSuccess;
  bool duplicate = false;

  probe::detail::Sequence Collect(uint64_t freq = frequency, uint64_t start = begin) {
    return probe::detail::Collect(cancellation.get_token(), freq, start, 2, 1, {
        [&] { return clock; },
        [&](uint64_t due, std::stop_token) {
          ++waits; clock = std::max(clock, due);
          if (waiting) waiting(*this, due);
        },
        [&](rex::ui::GuestOutputCapture& output) {
          ++captures;
          const auto before = clock;
          const auto sequence = duplicate ? 1 : captures;
          const auto publication = duplicate ? begin - 50 : before - 50;
          output.image.width = 2; output.image.height = 1; output.image.stride = 8;
          output.image.data = {uint8_t(sequence), 2, 3, 0, 4, 5, 6, 255};
          output.provenance = {7, sequence, freq, publication, publication + 10,
                              before, before + 10, before + 20, before + 30, before + 40};
          clock += 50;
          if (modify) modify(*this, output);
          return outcome;
        }});
  }
};

void SchedulerTests() {
  {
    Fake fake;
    const auto result = fake.Collect();
    Check(fake.captures == 13 && fake.waits == 13, "exactly thirteen calls/waits");
    for (size_t i = 0; i < 13; ++i) {
      const auto& slot = result.slots[i];
      Check(slot.due == begin + i * 1000 && slot.before == slot.due && slot.after == slot.before + 50,
            "absolute schedule and external brackets");
      Check(slot.has_capture && std::string_view(slot.status) == "COLLECTED" && !slot.reason,
            "paired slot retained");
      Check(slot.paired.provenance.acquired_sequence == i + 1 && slot.paired.image.data[0] == i + 1,
            "pixel identity comes from paired result");
    }
  }
  {
    Fake fake; fake.duplicate = true;
    fake.modify = [](Fake& self, auto& output) { output.image.data[3] = uint8_t(self.captures); };
    const auto result = fake.Collect();
    probe::detail::ValidateBufferedRgb(result);
    Check(fake.captures == 13, "duplicates are not skipped or retried");
    for (const auto& slot : result.slots)
      Check(slot.has_capture && slot.paired.provenance.acquired_sequence == 1 &&
            slot.paired.provenance.refresh_cpu_begin_qpc == begin - 50,
            "duplicate keeps original publication despite X changes");
  }
  {
    Fake fake; fake.modify = [](Fake&, auto& output) { output.image.data[0] = 1; };
    const auto result = fake.Collect();
    Check(fake.captures == 13 && result.slots[12].has_capture, "distinct sequences with identical RGB allowed");
  }
  {
    Fake fake;
    fake.waiting = [](Fake& self, uint64_t due) { self.clock = due + 1000; };
    const auto result = fake.Collect();
    Check(fake.captures == 0 && fake.waits == 13, "all expired slots cause zero calls");
    for (const auto& slot : result.slots)
      Check(std::string_view(slot.status) == "MISSED_DEADLINE" && !slot.has_capture &&
            !slot.before && !slot.after && slot.paired.image.data.empty(), "miss has no stale capture");
  }
  {
    Fake fake;
    fake.modify = [](Fake& self, auto&) { if (self.captures == 1) self.clock = begin + 2150; };
    const auto result = fake.Collect();
    Check(fake.captures == 12 && std::string_view(result.slots[1].status) == "MISSED_DEADLINE",
          "slow call expires next slot without catch-up burst");
    Check(result.slots[2].before == begin + 2150, "next still-open slot is observed honestly late");
  }
  {
    Fake fake;
    fake.waiting = [](Fake& self, uint64_t due) { self.clock = due + ((self.waits % 2) ? 990 : 0); };
    fake.modify = [](Fake& self, auto& output) {
      self.clock = output.provenance.acquire_before_qpc + 1;
      auto& p = output.provenance;
      p.acquire_after_qpc = p.copy_submit_before_qpc = p.copy_completion_observed_qpc = p.pixels_ready_qpc = p.acquire_before_qpc;
    };
    const auto result = fake.Collect();
    Check(result.slots[1].before - result.slots[0].before == 10 &&
          result.slots[2].before - result.slots[1].before == 1990,
          "slot coverage does not imply uniform acquisition cadence");
  }
  for (const auto outcome : {rex::ui::GuestCaptureStatus::kFailed, rex::ui::GuestCaptureStatus::kUnsupported}) {
    Fake fake; fake.outcome = outcome;
    const auto result = fake.Collect();
    Check(fake.captures == 1 && std::string_view(result.slots[0].status) == "FAILED", "API failure is terminal");
    Check(!result.slots[0].has_capture && result.slots[0].paired.image.data.empty() &&
          result.slots[0].paired.provenance.acquired_sequence == 0, "failure clears malformed producer output too");
    for (size_t i = 1; i < 13; ++i)
      Check(std::string_view(result.slots[i].reason) == "PREVIOUS_TERMINAL_FAILURE" &&
            !result.slots[i].before, "failure suffix never retries");
  }
  {
    Fake fake; fake.cancellation.request_stop();
    const auto result = fake.Collect();
    Check(fake.captures == 0 && fake.waits == 0, "initial stop causes no wait or acquisition");
    Check(std::string_view(result.slots[0].reason) == "STOPPED" &&
          std::string_view(result.slots[12].reason) == "PREVIOUS_STOP", "stopped suffix explicit");
  }
  {
    Fake fake; fake.waiting = [](Fake& self, uint64_t) { if (self.waits == 4) self.cancellation.request_stop(); };
    const auto result = fake.Collect();
    Check(fake.captures == 3 && std::string_view(result.slots[3].reason) == "STOPPED",
          "stop during wait preserves earlier observations without further calls");
  }
  {
    Fake fake; fake.modify = [](Fake& self, auto&) { self.cancellation.request_stop(); };
    const auto result = fake.Collect();
    Check(fake.captures == 1 && std::string_view(result.slots[0].reason) == "STOPPED_AFTER_CAPTURE" &&
          result.slots[0].paired.image.data.empty(), "stop after API clears unaccepted image");
  }
  {
    Fake fake; fake.modify = [](Fake&, auto&) { throw std::runtime_error("synthetic"); };
    const auto result = fake.Collect();
    Check(fake.captures == 1 && std::string_view(result.slots[0].reason) == "API_FAILED" &&
          result.slots[0].paired.image.data.empty(), "throwing callback is retained terminal failure");
  }
  for (unsigned mode = 0; mode < 8; ++mode) {
    Fake fake; fake.duplicate = true;
    fake.modify = [mode](Fake& self, auto& output) {
      if (self.captures != 2) return;
      auto& p = output.provenance;
      switch (mode) {
        case 0: ++p.presenter_epoch; break;
        case 1: p.acquired_sequence = 0; break;
        case 2: ++p.qpc_frequency; break;
        case 3: ++p.refresh_cpu_begin_qpc; break;
        case 4: ++output.image.data[0]; break;
        case 5: output.image.width = 1; break;
        case 6: p.pixels_ready_qpc = self.clock + 1; break;
        case 7: output.image.data.pop_back(); break;
      }
    };
    const auto result = fake.Collect();
    if (mode == 4) {
      Check(fake.captures == 13 && result.slots[12].has_capture, "pixel mismatch never selects collection length");
      Throws([&] { probe::detail::ValidateBufferedRgb(result); }, "DUPLICATE_RGB_CHANGED");
    } else {
      Check(fake.captures == 2 && result.slots[0].has_capture && !result.slots[1].has_capture &&
            std::string_view(result.slots[1].reason) == "METADATA_INVALID", "malformed identity/provenance terminal rejection");
    }
  }
  {
    Fake fake;
    Throws([&] { fake.Collect(0); }, "INVALID_SCHEDULE");
    Throws([&] { fake.Collect(frequency, uint64_t(INT64_MAX)); }, "INVALID_SCHEDULE");
    fake.waiting = [](Fake& self, uint64_t due) { self.clock = due - 1; };
    Throws([&] { fake.Collect(); }, "CLOCK_REGRESSION_OR_INVALID");
  }
}

void SerializationAndConfigTests() {
  Check(!probe::Enabled(), "default sequence flag disabled");
  probe::ValidateExclusiveConfig("vulkan");
  Check(probe::ValidToken("0123456789abcdef0123456789abcdef") &&
        !probe::ValidToken("0123456789ABCDEF0123456789ABCDEF"), "token lowercase32hex");
  REXCVAR_SET(mcla_source_sequence_probe, true);
  REXCVAR_SET(mcla_source_sequence_token, "0123456789abcdef0123456789abcdef");
  probe::ValidateExclusiveConfig("d3d12");
  Throws([] { probe::ValidateExclusiveConfig("vulkan"); }, "D3D12_REQUIRED");
  REXCVAR_SET(mcla_sequence_test_peer_probe, true);
  Throws([] { probe::ValidateExclusiveConfig("d3d12"); }, "OTHER_PROBE_ENABLED_OR_INVALID");
  REXCVAR_SET(mcla_sequence_test_peer_probe, false);
  REXCVAR_SET(mcla_source_sequence_probe, false);
  REXCVAR_SET(mcla_source_sequence_token, "");
  Fake fake;
  auto sequence = fake.Collect();
  const auto value = JsonValue::parse(probe::Json("0123456789abcdef0123456789abcdef",
      "SOURCE_SEQUENCE_CENSUS_REVIEW_PENDING", "FIXED_SLOTS_NO_DISPLAY_SIMULATION_OR_TIMING_CLAIM", 60, 123, &sequence));
  Check(value.at("schema") == "mcla-guest-source-sequence-v1" && value.at("sequence").at("slots").size() == 13,
        "separate bounded native schema");
  for (const auto* key : {"native_admitted", "display_proven", "simulation_proven", "fps_proven",
                         "scene_admitted", "cycle_timing_proven", "legacy_cycle_transfer_eligible",
                         "input_injected", "desktop_capture", "source_binding_independently_reviewed"})
    Check(value.at(key) == false, "receipt does not admit capture or timing");
  for (size_t i = 0; i < 13; ++i) {
    const auto& row = value.at("sequence").at("slots").at(i);
    Check(row.at("slot") == i && row.at("due_qpc") == begin + i * 1000 &&
          row.at("capture").at("acquired_sequence") == i + 1 && row.at("reason").is_null(),
          "slot journal and paired metadata serialize independently");
    Check(row.at("image_state") == "absent" && row.at("image_bytes_written") == 0,
          "collection alone does not claim saved files");
  }
  Check(probe::ImageName(0) == "slot-00.bmp" && probe::ImageName(12) == "slot-12.bmp", "fixed unique image names");
  Throws([] { probe::ImageName(13); }, "INVALID_SLOT_INDEX");
  Fake failed; failed.outcome = rex::ui::GuestCaptureStatus::kFailed;
  const auto partial = failed.Collect();
  const auto failed_json = JsonValue::parse(probe::Json("0123456789abcdef0123456789abcdef",
      "SOURCE_SEQUENCE_CENSUS_INCOMPLETE_REVIEW_PENDING", "FIXED_SLOTS_NO_DISPLAY_SIMULATION_OR_TIMING_CLAIM", 60, 123, &partial));
  Check(failed_json.at("sequence").at("slots").at(0).at("capture").is_null() &&
        failed_json.at("sequence").at("slots").at(1).at("call_before_qpc").is_null(),
        "failure and unattempted suffix cannot serialize stale capture");
  const auto absent = JsonValue::parse(probe::Json("", "FAILED", "READINESS_TIMEOUT", 0, 0, nullptr));
  Check(absent.at("status") == "FAILED" && absent.at("sequence").is_null(), "precollection failure explicit");
}
}  // namespace

int main() {
  try { SchedulerTests(); SerializationAndConfigTests(); }
  catch (const std::exception& error) { std::cerr << "Unexpected test exception: " << error.what() << '\n'; return 2; }
  std::cout << "source sequence no-GPU checks=" << checks << " failed=" << failures << '\n';
  return failures ? 1 : 0;
}
