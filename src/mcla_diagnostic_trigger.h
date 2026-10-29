#pragma once

// M9-010: diagnostic snapshot triggers without a keyboard.
//
// Pure policy only (no SDL, SDK, window, clock or thread dependency) so that
// it can be unit tested:
//  - trigger identity (category/source) recorded in state.json/manifest.json;
//  - the raw wheel L2/R2 button-index cvar format;
//  - one-package-per-press edge trigger with release re-arm;
//  - the gamepad Start+Back hold combo;
//  - a small ring buffer of host-observed active-device transitions;
//  - the "wheel" section written into state.json for wheel snapshots.
// The SDL adapter is mcla_diagnostic_triggers_sdl.cpp.

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace mcla::diagnostics {

inline constexpr std::string_view kCategoryGeneral = "general";
inline constexpr std::string_view kCategoryWheel = "wheel";
inline constexpr std::string_view kSourceF10 = "f10";
inline constexpr std::string_view kSourceWheelR2 = "wheel-r2";
inline constexpr std::string_view kSourceWheelL2 = "wheel-l2";
inline constexpr std::string_view kSourcePad = "pad";

// ---------------------------------------------------------------------------
// Raw wheel button configuration: "L2,R2" raw SDL joystick button indices of
// the active wheel, each -1 (disabled) or 0..255. Whitespace around values is
// allowed. T300 defaults are 8,9 (unverified, see the M9-010 evidence).

struct WheelButtons {
  int l2 = -1;
  int r2 = -1;
  bool operator==(const WheelButtons &) const = default;
};

inline constexpr std::string_view kDefaultWheelButtons = "8,9";

inline std::optional<int> ParseButtonIndex(std::string_view text) {
  while (!text.empty() && (text.front() == ' ' || text.front() == '\t')) {
    text.remove_prefix(1);
  }
  while (!text.empty() && (text.back() == ' ' || text.back() == '\t')) {
    text.remove_suffix(1);
  }
  if (text == "-1") {
    return -1;
  }
  if (text.empty() || text.size() > 3) {
    return std::nullopt;
  }
  int value = 0;
  for (const char c : text) {
    if (c < '0' || c > '9') {
      return std::nullopt;
    }
    value = value * 10 + (c - '0');
  }
  if (value > 255 || (text.size() > 1 && text.front() == '0')) {
    return std::nullopt;
  }
  return value;
}

inline std::optional<WheelButtons> ParseWheelButtons(std::string_view text) {
  const auto comma = text.find(',');
  if (comma == std::string_view::npos ||
      text.find(',', comma + 1) != std::string_view::npos) {
    return std::nullopt;
  }
  const auto l2 = ParseButtonIndex(text.substr(0, comma));
  const auto r2 = ParseButtonIndex(text.substr(comma + 1));
  if (!l2 || !r2 || (*l2 >= 0 && *l2 == *r2)) {
    return std::nullopt;
  }
  return WheelButtons{*l2, *r2};
}

// ---------------------------------------------------------------------------
// One package per press: fires on the first press, ignores repeated downs
// (and other devices) until the pressing device releases the button.

class ButtonEdgeTrigger {
public:
  bool OnDown(uint32_t device) {
    if (held_) {
      return false;
    }
    held_ = true;
    device_ = device;
    return true;
  }
  void OnUp(uint32_t device) {
    if (held_ && device == device_) {
      held_ = false;
    }
  }
  // A removed device can never send its release.
  void OnDeviceRemoved(uint32_t device) { OnUp(device); }
  bool held() const { return held_; }

private:
  bool held_ = false;
  uint32_t device_ = 0;
};

// ---------------------------------------------------------------------------
// Gamepad Start+Back held for hold_ms fires once; a short tap never fires.
// Releasing either button re-arms. Only one gamepad is tracked at a time:
// the first one to hold both buttons owns the combo until it releases.

class HoldComboTrigger {
public:
  enum class Button : uint8_t { kStart, kBack };

  explicit HoldComboTrigger(uint32_t hold_ms = 1000) : hold_ms_(hold_ms) {}

  uint32_t hold_ms() const { return hold_ms_; }

  // Returns true when this event arms the combo (both buttons now held); the
  // caller then schedules Poll at deadline_ms().
  bool OnButton(uint32_t device, Button button, bool down, uint64_t now_ms) {
    Pad *pad = Find(device, down);
    if (!pad) {
      return false;
    }
    (button == Button::kStart ? pad->start : pad->back) = down;
    if (!down) {
      if (armed_ && owner_ == device) {
        armed_ = false;
      }
      if (fired_ && owner_ == device) {
        fired_ = false;
      }
      if (!pad->start && !pad->back) {
        *pad = Pad{};
      }
      return false;
    }
    if (pad->start && pad->back && !armed_ && !fired_) {
      armed_ = true;
      owner_ = device;
      deadline_ms_ = now_ms + hold_ms_;
      return true;
    }
    return false;
  }

  // Fires once when the owner has held both buttons for hold_ms.
  bool Poll(uint64_t now_ms) {
    if (!armed_ || now_ms < deadline_ms_) {
      return false;
    }
    armed_ = false;
    fired_ = true;
    return true;
  }

  void OnDeviceRemoved(uint32_t device) {
    for (auto &pad : pads_) {
      if (pad.used && pad.device == device) {
        pad = Pad{};
      }
    }
    if ((armed_ || fired_) && owner_ == device) {
      armed_ = false;
      fired_ = false;
    }
  }

  bool armed() const { return armed_; }
  uint64_t deadline_ms() const { return deadline_ms_; }

private:
  struct Pad {
    bool used = false;
    uint32_t device = 0;
    bool start = false;
    bool back = false;
  };

  Pad *Find(uint32_t device, bool create) {
    for (auto &pad : pads_) {
      if (pad.used && pad.device == device) {
        return &pad;
      }
    }
    if (!create) {
      return nullptr;
    }
    for (auto &pad : pads_) {
      if (!pad.used) {
        pad = Pad{true, device, false, false};
        return &pad;
      }
    }
    return nullptr;
  }

  uint32_t hold_ms_;
  std::array<Pad, 8> pads_{};
  bool armed_ = false;
  bool fired_ = false;
  uint32_t owner_ = 0;
  uint64_t deadline_ms_ = 0;
};

// ---------------------------------------------------------------------------
// Host-observed latest meaningful input device and its recent transitions.
// This is the host's own observation of SDL input, not the SDK's routing
// decision (that is recorded from the SDK's SDL_ACTIVE_CONTROLLER log lines).

enum class InputSource : uint8_t { kJoystick, kGamepad };

inline std::string_view InputSourceName(InputSource source) {
  return source == InputSource::kGamepad ? "gamepad" : "joystick";
}

struct ActiveTransition {
  uint64_t timestamp_ms = 0;
  int64_t from = -1; // -1: none
  uint32_t to = 0;
  InputSource source = InputSource::kJoystick;
};

class ActiveDeviceTracker {
public:
  static constexpr size_t kCapacity = 16;

  // Returns true when the latest device changed (a transition is recorded).
  bool OnActivity(uint32_t device, InputSource source, uint64_t timestamp_ms) {
    if (has_latest_ && latest_ == device) {
      latest_timestamp_ms_ = timestamp_ms;
      return false;
    }
    ActiveTransition transition{timestamp_ms,
                                has_latest_ ? int64_t{latest_} : int64_t{-1},
                                device, source};
    ring_[next_] = transition;
    next_ = (next_ + 1) % kCapacity;
    if (count_ < kCapacity) {
      ++count_;
    }
    ++total_;
    has_latest_ = true;
    latest_ = device;
    latest_source_ = source;
    latest_timestamp_ms_ = timestamp_ms;
    return true;
  }

  bool has_latest() const { return has_latest_; }
  uint32_t latest() const { return latest_; }
  InputSource latest_source() const { return latest_source_; }
  uint64_t latest_timestamp_ms() const { return latest_timestamp_ms_; }
  uint64_t total_transitions() const { return total_; }

  // Oldest first.
  std::vector<ActiveTransition> Transitions() const {
    std::vector<ActiveTransition> result;
    result.reserve(count_);
    const size_t first = (next_ + kCapacity - count_) % kCapacity;
    for (size_t i = 0; i < count_; ++i) {
      result.push_back(ring_[(first + i) % kCapacity]);
    }
    return result;
  }

private:
  std::array<ActiveTransition, kCapacity> ring_{};
  size_t next_ = 0;
  size_t count_ = 0;
  uint64_t total_ = 0;
  bool has_latest_ = false;
  uint32_t latest_ = 0;
  InputSource latest_source_ = InputSource::kJoystick;
  uint64_t latest_timestamp_ms_ = 0;
};

// ---------------------------------------------------------------------------
// Snapshot request identity and the wheel-state section.

struct JoystickState {
  uint32_t instance_id = 0;
  std::string name;
  std::string guid;
  uint16_t vendor_id = 0;
  uint16_t product_id = 0;
  int joystick_type = 0;
  bool is_gamepad = false;
  bool is_wheel = false;
  bool opened = false;
  bool haptic = false;
  std::vector<int16_t> axes;
  std::vector<uint8_t> buttons;
  std::vector<uint8_t> hats;
};

struct WheelState {
  WheelButtons buttons;
  std::optional<uint32_t> trigger_device;
  std::vector<JoystickState> devices;
  bool host_latest_known = false;
  uint32_t host_latest = 0;
  InputSource host_latest_source = InputSource::kJoystick;
  uint64_t host_latest_timestamp_ms = 0;
  uint64_t host_total_transitions = 0;
  std::vector<ActiveTransition> host_transitions;
  // Filled by the package worker from the captured SDK log.
  std::vector<std::string> sdk_active_controller_log;
  std::vector<std::string> sdk_force_feedback_log;
  std::vector<std::string> sdk_device_log;
};

// An empty source means the snapshot reason (for example "probe" or
// "unexpected-guest-exit") is also the source.
struct SnapshotTrigger {
  std::string category{kCategoryGeneral};
  std::string source;
  std::optional<WheelState> wheel;
};

inline constexpr size_t kMaxSdkLogLinesPerKind = 32;
inline constexpr size_t kMaxSdkLogLineBytes = 512;

enum class SdkInputLogKind : uint8_t { kNone, kActiveController, kForceFeedback, kDevice };

// Classifies a captured SDK log line (ReXGlue SDL input driver markers).
inline SdkInputLogKind ClassifySdkInputLogLine(std::string_view line) {
  if (line.find("SDL_ACTIVE_CONTROLLER") != std::string_view::npos) {
    return SdkInputLogKind::kActiveController;
  }
  if (line.find("SDL_WHEEL_FFB") != std::string_view::npos ||
      line.find("SDL wheel FFB") != std::string_view::npos ||
      line.find("SDL wheel does not support") != std::string_view::npos ||
      line.find("SDL wheel rumble") != std::string_view::npos) {
    return SdkInputLogKind::kForceFeedback;
  }
  if (line.find("SDL_WHEEL_CONNECTED") != std::string_view::npos ||
      line.find("SDL wheel ") != std::string_view::npos ||
      line.find("SDL OnControllerDeviceAdded") != std::string_view::npos ||
      line.find("removed from physical slot") != std::string_view::npos ||
      line.find("SDL_HOTPLUG") != std::string_view::npos ||
      line.find("SDL_XINPUT") != std::string_view::npos) {
    return SdkInputLogKind::kDevice;
  }
  return SdkInputLogKind::kNone;
}

// Keeps the newest kMaxSdkLogLinesPerKind lines of each kind, oldest first.
inline void AddSdkInputLogLine(WheelState &wheel, std::string_view line) {
  std::vector<std::string> *target = nullptr;
  switch (ClassifySdkInputLogLine(line)) {
  case SdkInputLogKind::kActiveController:
    target = &wheel.sdk_active_controller_log;
    break;
  case SdkInputLogKind::kForceFeedback:
    target = &wheel.sdk_force_feedback_log;
    break;
  case SdkInputLogKind::kDevice:
    target = &wheel.sdk_device_log;
    break;
  case SdkInputLogKind::kNone:
    return;
  }
  if (line.size() > kMaxSdkLogLineBytes) {
    line = line.substr(0, kMaxSdkLogLineBytes);
  }
  if (target->size() == kMaxSdkLogLinesPerKind) {
    target->erase(target->begin());
  }
  target->emplace_back(line);
}

inline std::string TriggerJsonEscape(std::string_view value) {
  std::string result;
  result.reserve(value.size() + 8);
  for (const unsigned char c : value) {
    if (c == '\\') {
      result += "\\\\";
    } else if (c == '"') {
      result += "\\\"";
    } else if (c < 0x20 || c == 0x7f) {
      char escaped[7]{};
      std::snprintf(escaped, sizeof(escaped), "\\u%04x", c);
      result += escaped;
    } else {
      result.push_back(static_cast<char>(c));
    }
  }
  return result;
}

// "trigger" object shared by state.json and manifest.json.
inline std::string BuildTriggerJson(const SnapshotTrigger &trigger) {
  return "{\"category\": \"" + TriggerJsonEscape(trigger.category) +
         "\", \"source\": \"" + TriggerJsonEscape(trigger.source) + "\"}";
}

namespace detail {
template <typename T> std::string JsonNumberArray(const std::vector<T> &values) {
  std::string out = "[";
  for (size_t i = 0; i < values.size(); ++i) {
    if (i) {
      out += ", ";
    }
    out += std::to_string(static_cast<int64_t>(values[i]));
  }
  return out + "]";
}
inline std::string JsonStringArray(const std::vector<std::string> &values,
                                   std::string_view indent) {
  if (values.empty()) {
    return "[]";
  }
  std::string out = "[\n";
  for (size_t i = 0; i < values.size(); ++i) {
    out += std::string(indent) + "  \"" + TriggerJsonEscape(values[i]) + "\"";
    out += i + 1 == values.size() ? "\n" : ",\n";
  }
  return out + std::string(indent) + "]";
}
inline std::string Hex16(uint16_t value) {
  char text[7]{};
  std::snprintf(text, sizeof(text), "0x%04X", static_cast<unsigned>(value));
  return text;
}
} // namespace detail

// The "wheel" object of state.json (schema mcla-wheel-state-v1). Indented for
// a two-space nesting level inside state.json.
inline std::string BuildWheelStateJson(const WheelState &wheel) {
  std::string out = "{\n";
  out += "    \"schema\": \"mcla-wheel-state-v1\",\n";
  out += "    \"config\": {\"l2_button\": " + std::to_string(wheel.buttons.l2) +
         ", \"r2_button\": " + std::to_string(wheel.buttons.r2) + "},\n";
  out += "    \"trigger_device\": " +
         (wheel.trigger_device ? std::to_string(*wheel.trigger_device)
                               : std::string("null")) +
         ",\n";
  out += "    \"devices\": [";
  for (size_t i = 0; i < wheel.devices.size(); ++i) {
    const auto &d = wheel.devices[i];
    out += i ? ",\n" : "\n";
    out += "      {\"instance_id\": " + std::to_string(d.instance_id) +
           ", \"name\": \"" + TriggerJsonEscape(d.name) + "\", \"guid\": \"" +
           TriggerJsonEscape(d.guid) + "\", \"vendor_id\": \"" +
           detail::Hex16(d.vendor_id) + "\", \"product_id\": \"" +
           detail::Hex16(d.product_id) +
           "\", \"joystick_type\": " + std::to_string(d.joystick_type) +
           ", \"gamepad\": " + (d.is_gamepad ? "true" : "false") +
           ", \"wheel\": " + (d.is_wheel ? "true" : "false") +
           ", \"opened\": " + (d.opened ? "true" : "false") +
           ", \"haptic\": " + (d.haptic ? "true" : "false") +
           ", \"axes\": " + detail::JsonNumberArray(d.axes) +
           ", \"buttons\": " + detail::JsonNumberArray(d.buttons) +
           ", \"hats\": " + detail::JsonNumberArray(d.hats) + "}";
  }
  out += wheel.devices.empty() ? "],\n" : "\n    ],\n";
  out += "    \"host_latest_active\": ";
  if (wheel.host_latest_known) {
    out += "{\"instance_id\": " + std::to_string(wheel.host_latest) +
           ", \"source\": \"" +
           std::string(InputSourceName(wheel.host_latest_source)) +
           "\", \"timestamp_ms\": " +
           std::to_string(wheel.host_latest_timestamp_ms) + "}";
  } else {
    out += "null";
  }
  out += ",\n    \"host_total_transitions\": " +
         std::to_string(wheel.host_total_transitions) + ",\n";
  out += "    \"host_transitions\": [";
  for (size_t i = 0; i < wheel.host_transitions.size(); ++i) {
    const auto &t = wheel.host_transitions[i];
    out += i ? ",\n" : "\n";
    out += "      {\"timestamp_ms\": " + std::to_string(t.timestamp_ms) +
           ", \"from\": " + std::to_string(t.from) +
           ", \"to\": " + std::to_string(t.to) + ", \"source\": \"" +
           std::string(InputSourceName(t.source)) + "\"}";
  }
  out += wheel.host_transitions.empty() ? "],\n" : "\n    ],\n";
  out += "    \"sdk_active_controller_log\": " +
         detail::JsonStringArray(wheel.sdk_active_controller_log, "    ") + ",\n";
  out += "    \"force_feedback\": {\"source\": \"sdk-log\", \"effect_table\": "
         "\"not-exposed-by-sdk\", \"log\": " +
         detail::JsonStringArray(wheel.sdk_force_feedback_log, "    ") + "},\n";
  out += "    \"sdk_device_log\": " +
         detail::JsonStringArray(wheel.sdk_device_log, "    ") + "\n";
  out += "  }";
  return out;
}

} // namespace mcla::diagnostics
