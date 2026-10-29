#include "mcla_diagnostic_trigger.h"

#include <nlohmann/json.hpp>

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

using namespace mcla::diagnostics;
using Button = HoldComboTrigger::Button;

void TestWheelButtonParsing() {
  Check(ParseWheelButtons(kDefaultWheelButtons) == WheelButtons{8, 9},
        "T300 default L2,R2 = 8,9");
  Check(ParseWheelButtons(" 10 , 11 ") == WheelButtons{10, 11},
        "whitespace around indices is accepted");
  Check(ParseWheelButtons("-1,9") == WheelButtons{-1, 9}, "L2 can be disabled");
  Check(ParseWheelButtons("8,-1") == WheelButtons{8, -1}, "R2 can be disabled");
  Check(ParseWheelButtons("-1,-1") == WheelButtons{-1, -1},
        "both disabled is valid");
  Check(ParseWheelButtons("0,255") == WheelButtons{0, 255}, "range bounds");
  for (const char *bad : {"", "8", "8,", ",9", "8,9,10", "256,9", "8,1000",
                          "a,9", "8,9x", "+8,9", "08,9", "8,8", "-2,9",
                          "8;9", "8 9"}) {
    Check(!ParseWheelButtons(bad).has_value(), "malformed index list rejected");
  }
}

void TestEdgeTrigger() {
  ButtonEdgeTrigger trigger;
  Check(trigger.OnDown(5), "first press fires");
  Check(!trigger.OnDown(5), "repeated down while held does not fire");
  Check(!trigger.OnDown(6), "another device while held does not fire");
  trigger.OnUp(6);
  Check(trigger.held(), "release by another device does not re-arm");
  Check(!trigger.OnDown(5), "still one package for the held press");
  trigger.OnUp(5);
  Check(!trigger.held(), "release re-arms");
  Check(trigger.OnDown(6), "next press fires again");
  trigger.OnDeviceRemoved(6);
  Check(trigger.OnDown(5), "removed device cannot leave the trigger held");
}

void TestHoldCombo() {
  HoldComboTrigger combo(1000);
  Check(combo.hold_ms() == 1000, "hold time");
  // Short tap: both buttons, released before the hold time.
  Check(!combo.OnButton(1, Button::kStart, true, 0), "start alone does not arm");
  Check(combo.OnButton(1, Button::kBack, true, 10), "start+back arms");
  Check(combo.deadline_ms() == 1010, "deadline is press time + hold");
  Check(!combo.Poll(500), "not before the hold time");
  combo.OnButton(1, Button::kBack, false, 600);
  Check(!combo.armed() && !combo.Poll(2000), "short tap never fires");

  // Full hold fires exactly once.
  Check(combo.OnButton(1, Button::kBack, true, 3000), "re-press arms again");
  Check(!combo.Poll(3999), "one millisecond early");
  Check(combo.Poll(4000), "fires at the hold time");
  Check(!combo.Poll(9000), "fires once per hold");
  Check(!combo.OnButton(1, Button::kStart, true, 9100),
        "repeated down while held does not re-arm");
  Check(!combo.Poll(20000), "still once");
  combo.OnButton(1, Button::kStart, false, 20100);
  Check(!combo.OnButton(1, Button::kBack, true, 20200),
        "back alone after releasing start does not arm");
  Check(combo.OnButton(1, Button::kStart, true, 20300),
        "release re-arms the next hold");
  Check(combo.Poll(21300), "second hold fires");

  // Separate devices do not combine, and a second pad cannot steal the combo.
  HoldComboTrigger split(1000);
  Check(!split.OnButton(1, Button::kStart, true, 0) &&
            !split.OnButton(2, Button::kBack, true, 0),
        "start on one pad and back on another do not arm");
  Check(!split.Poll(5000), "split buttons never fire");
  Check(split.OnButton(2, Button::kStart, true, 5000), "pad 2 arms");
  Check(!split.OnButton(1, Button::kBack, true, 5100),
        "pad 1 cannot re-arm while pad 2 owns the combo");
  split.OnButton(1, Button::kBack, false, 5200);
  Check(split.armed(), "other pad release does not disarm the owner");
  Check(split.Poll(6000), "owner fires");
  split.OnDeviceRemoved(2);
  Check(!split.armed(), "removed owner leaves nothing armed");
  Check(split.OnButton(1, Button::kBack, true, 7000) && split.Poll(8000),
        "remaining pad can fire after the owner is removed");
}

void TestTracker() {
  ActiveDeviceTracker tracker;
  Check(!tracker.has_latest() && tracker.Transitions().empty(), "empty tracker");
  Check(tracker.OnActivity(3, InputSource::kJoystick, 100), "first device");
  Check(!tracker.OnActivity(3, InputSource::kJoystick, 150),
        "same device is not a transition");
  Check(tracker.latest_timestamp_ms() == 150, "latest timestamp follows activity");
  Check(tracker.OnActivity(7, InputSource::kGamepad, 200), "switch to pad");
  const auto transitions = tracker.Transitions();
  Check(transitions.size() == 2 && transitions[0].from == -1 &&
            transitions[0].to == 3 && transitions[1].from == 3 &&
            transitions[1].to == 7 &&
            transitions[1].source == InputSource::kGamepad,
        "transitions are recorded oldest first");
  for (uint32_t i = 0; i < 40; ++i) {
    tracker.OnActivity(i % 2 ? 3 : 7, InputSource::kJoystick, 300 + i);
  }
  const auto ring = tracker.Transitions();
  Check(ring.size() == ActiveDeviceTracker::kCapacity, "ring is bounded");
  Check(ring.back().timestamp_ms == 339 && ring.front().timestamp_ms == 324,
        "ring keeps the newest transitions");
  Check(tracker.total_transitions() == 41, "total transition count is kept");
}

void TestSdkLogClassification() {
  Check(ClassifySdkInputLogLine("SDL_ACTIVE_CONTROLLER v=1 guest_slot=0 "
                                "previous_physical_slot=1 physical_slot=0") ==
            SdkInputLogKind::kActiveController,
        "active controller marker");
  Check(ClassifySdkInputLogLine("SDL_WHEEL_FFB_EFFECT v=2 action=start slot=3 "
                                "type=9") == SdkInputLogKind::kForceFeedback,
        "FFB effect marker");
  Check(ClassifySdkInputLogLine("SDL wheel FFB start failed for slot 1") ==
            SdkInputLogKind::kForceFeedback,
        "FFB failure line");
  Check(ClassifySdkInputLogLine("SDL_WHEEL_CONNECTED v=1 physical_slot=0") ==
            SdkInputLogKind::kDevice,
        "wheel connect marker");
  Check(ClassifySdkInputLogLine("SDL wheel removed from physical slot 0.") ==
            SdkInputLogKind::kDevice,
        "device removal line");
  Check(ClassifySdkInputLogLine("MCLA diagnostics: snapshot queued") ==
            SdkInputLogKind::kNone,
        "unrelated line ignored");
  WheelState wheel;
  for (int i = 0; i < 40; ++i) {
    AddSdkInputLogLine(wheel, "SDL_WHEEL_FFB_EFFECT v=2 action=start slot=" +
                                  std::to_string(i));
  }
  AddSdkInputLogLine(wheel, "unrelated");
  AddSdkInputLogLine(wheel, std::string(2000, 'x') + "SDL_ACTIVE_CONTROLLER");
  Check(wheel.sdk_force_feedback_log.size() == kMaxSdkLogLinesPerKind &&
            wheel.sdk_force_feedback_log.back().ends_with("slot=39") &&
            wheel.sdk_force_feedback_log.front().ends_with("slot=8"),
        "newest SDK lines kept per kind");
  Check(wheel.sdk_active_controller_log.size() == 1 &&
            wheel.sdk_active_controller_log[0].size() == kMaxSdkLogLineBytes,
        "SDK lines are length-bounded");
  Check(wheel.sdk_device_log.empty(), "unrelated lines are not recorded");
}

void TestJson() {
  SnapshotTrigger trigger;
  trigger.category = std::string(kCategoryWheel);
  trigger.source = std::string(kSourceWheelL2);
  WheelState wheel;
  wheel.buttons = {8, 9};
  wheel.trigger_device = 4;
  JoystickState device;
  device.instance_id = 4;
  device.name = "Thrustmaster \"T300\"\tRS";
  device.guid = "030000004f0400006eb6000000000000";
  device.vendor_id = 0x044F;
  device.product_id = 0xB66E;
  device.joystick_type = 2;
  device.is_wheel = true;
  device.opened = true;
  device.haptic = true;
  device.axes = {-32768, 0, 32767, 12};
  device.buttons = {0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0};
  device.hats = {0};
  wheel.devices.push_back(device);
  JoystickState pad;
  pad.instance_id = 9;
  pad.name = "Pad";
  pad.is_gamepad = true;
  wheel.devices.push_back(pad);
  ActiveDeviceTracker tracker;
  tracker.OnActivity(9, InputSource::kGamepad, 10);
  tracker.OnActivity(4, InputSource::kJoystick, 20);
  wheel.host_latest_known = true;
  wheel.host_latest = tracker.latest();
  wheel.host_latest_timestamp_ms = tracker.latest_timestamp_ms();
  wheel.host_total_transitions = tracker.total_transitions();
  wheel.host_transitions = tracker.Transitions();
  AddSdkInputLogLine(wheel, "SDL_ACTIVE_CONTROLLER v=1 physical_slot=0 reason=\"x\"");
  AddSdkInputLogLine(wheel, "SDL_WHEEL_FFB_EFFECT v=2 action=stop slot=1 type=8");
  trigger.wheel = wheel;

  Check(BuildTriggerJson(trigger) ==
            "{\"category\": \"wheel\", \"source\": \"wheel-l2\"}",
        "trigger object text");
  const std::string document = "{\n  \"trigger\": " + BuildTriggerJson(trigger) +
                               ",\n  \"wheel\": " +
                               BuildWheelStateJson(*trigger.wheel) + "\n}\n";
  nlohmann::json json;
  try {
    json = nlohmann::json::parse(document);
  } catch (const std::exception &) {
    Check(false, "wheel section is valid JSON");
  }
  Check(json["trigger"]["category"] == "wheel" &&
            json["trigger"]["source"] == "wheel-l2",
        "trigger fields parse");
  const auto &w = json["wheel"];
  Check(w["schema"] == "mcla-wheel-state-v1", "wheel schema");
  Check(w["config"]["l2_button"] == 8 && w["config"]["r2_button"] == 9,
        "configured indices recorded");
  Check(w["trigger_device"] == 4, "trigger device recorded");
  Check(w["devices"].size() == 2, "all devices recorded");
  const auto &d = w["devices"][0];
  Check(d["name"] == "Thrustmaster \"T300\"\tRS" &&
            d["guid"] == "030000004f0400006eb6000000000000" &&
            d["vendor_id"] == "0x044F" && d["product_id"] == "0xB66E",
        "device identity round-trips with escaping");
  Check(d["wheel"] == true && d["gamepad"] == false && d["opened"] == true &&
            d["haptic"] == true && d["joystick_type"] == 2,
        "device flags");
  Check(d["axes"] == nlohmann::json::array({-32768, 0, 32767, 12}) &&
            d["buttons"].size() == 13 && d["buttons"][8] == 1 &&
            d["hats"] == nlohmann::json::array({0}),
        "raw axes, buttons and hats");
  Check(w["devices"][1]["axes"].empty() && w["devices"][1]["gamepad"] == true,
        "unopened device has no values");
  Check(w["host_latest_active"]["instance_id"] == 4 &&
            w["host_latest_active"]["source"] == "joystick" &&
            w["host_latest_active"]["timestamp_ms"] == 20,
        "host latest active device");
  Check(w["host_total_transitions"] == 2 && w["host_transitions"].size() == 2 &&
            w["host_transitions"][0]["from"] == -1 &&
            w["host_transitions"][1]["from"] == 9 &&
            w["host_transitions"][1]["to"] == 4,
        "host transitions");
  Check(w["sdk_active_controller_log"].size() == 1 &&
            w["sdk_active_controller_log"][0] ==
                "SDL_ACTIVE_CONTROLLER v=1 physical_slot=0 reason=\"x\"",
        "SDK active-controller lines");
  Check(w["force_feedback"]["source"] == "sdk-log" &&
            w["force_feedback"]["effect_table"] == "not-exposed-by-sdk" &&
            w["force_feedback"]["log"].size() == 1,
        "force-feedback section states its limits");
  Check(w["sdk_device_log"].empty(), "empty SDK device log");

  WheelState empty;
  const std::string minimal = "{\"wheel\": " + BuildWheelStateJson(empty) + "}";
  try {
    const auto parsed = nlohmann::json::parse(minimal);
    Check(parsed["wheel"]["devices"].empty() &&
              parsed["wheel"]["host_latest_active"].is_null() &&
              parsed["wheel"]["trigger_device"].is_null() &&
              parsed["wheel"]["config"]["l2_button"] == -1,
          "empty wheel state is valid");
  } catch (const std::exception &) {
    Check(false, "empty wheel state is valid JSON");
  }
  SnapshotTrigger general;
  Check(general.category == "general" && general.source.empty() &&
            !general.wheel,
        "default trigger is general with the reason as source");
}

// Prefilled tester report.txt written into each package folder.
void TestPackageReport() {
  PackageReportFacts facts;
  facts.package = "live-20261029T120000Z-4242-3";
  facts.kind = "live";
  facts.created_utc = "20261029T120001Z";
  facts.mcla_version = "0.9.0";
  facts.category = "wheel";
  facts.source = "wheel-l2";
  facts.window = "2560x1440 fullscreen";

  WheelState wheel;
  wheel.trigger_device = 7;
  JoystickState pad;
  pad.instance_id = 3;
  pad.name = "Xbox Controller";
  pad.guid = "0300aaaa";
  pad.vendor_id = 0x045E;
  pad.product_id = 0x028E;
  pad.is_gamepad = true;
  JoystickState t300;
  t300.instance_id = 7;
  t300.name = "Thrustmaster T300RS Racing wheel";
  t300.guid = "030000004f04000066b6000000000000";
  t300.vendor_id = 0x044F;
  t300.product_id = 0xB66E;
  t300.is_wheel = true;
  wheel.devices = {pad, t300};
  facts.wheel = WheelReportLine(wheel);
  Check(facts.wheel == "Thrustmaster T300RS Racing wheel | VID 0x044F PID "
                       "0xB66E | GUID 030000004f04000066b6000000000000",
        "wheel line names the trigger device model, VID/PID and GUID");

  const auto text = BuildPackageReportText(facts);
  Check(text.starts_with("MCLA-R TEST REPORT\n\n"), "report header");
  Check(text.find("\npackage live-20261029T120000Z-4242-3\nkind live\n"
                  "created_utc 20261029T120001Z\nmcla_version 0.9.0\n"
                  "category wheel\nsource wheel-l2\nwheel Thrustmaster T300RS") !=
            std::string::npos,
        "report carries package, time, version, category and source");
  Check(text.find("\nwindow 2560x1440 fullscreen\n" +
                  std::string(kPackageReportPendingLine) + "\n\n1. What were you doing?\n") !=
            std::string::npos,
        "pending launcher line precedes the questions");
  for (const char *question :
       {"1. What were you doing?", "2. Steps to make it happen again",
        "3. What went wrong?", "4. What did you expect instead?"}) {
    Check(text.find(question) != std::string::npos, "four questions present");
  }
  Check(text.find('\r') == std::string::npos, "LF only");

  WheelState no_trigger;
  no_trigger.devices = {pad, t300};
  Check(WheelReportLine(no_trigger).starts_with("Thrustmaster T300RS"),
        "without a trigger device the first SDL wheel is named");
  WheelState only_pad;
  only_pad.devices = {pad};
  Check(WheelReportLine(only_pad) == "unknown", "no wheel connected: unknown");
  Check(WheelReportLine(std::nullopt).starts_with("not-recorded"),
        "packages without wheel capture say not-recorded");
  JoystickState odd = t300;
  odd.name = std::string("Wheel\n\x01\xC3\xA9") + std::string(200, 'x');
  odd.guid.clear();
  WheelState odd_state;
  odd_state.devices = {odd};
  const auto odd_line = WheelReportLine(odd_state);
  Check(odd_line.starts_with("Wheel???? ") || odd_line.starts_with("Wheel????x"),
        "control and non-ASCII bytes are replaced");
  Check(odd_line.find('\n') == std::string::npos &&
            odd_line.ends_with("| GUID unknown") &&
            odd_line.size() < kMaxReportFieldBytes + 64,
        "fields are bounded and empty GUID is unknown");

  PackageReportFacts crash;
  crash.package = "crash-20261029T120500Z-4242";
  crash.kind = "crash";
  crash.category = "crash";
  crash.source = "crash";
  crash.wheel = "not-recorded (crash)";
  const auto crash_text = BuildPackageReportText(crash);
  Check(crash_text.find("\nkind crash\ncreated_utc unknown\nmcla_version unknown\n"
                        "category crash\nsource crash\nwheel not-recorded (crash)\n"
                        "window unknown\n") != std::string::npos,
        "crash report never guesses unknown facts");
}
} // namespace

int main() {
  TestWheelButtonParsing();
  TestEdgeTrigger();
  TestHoldCombo();
  TestTracker();
  TestSdkLogClassification();
  TestJson();
  TestPackageReport();
  std::printf("PASS: %d diagnostic trigger checks\n", checks);
}
