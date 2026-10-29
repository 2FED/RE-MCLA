#pragma once

// M9-009: hide the mouse cursor while the focused game window receives
// gamepad or wheel input, and show it again on mouse activity.
//
// Pure decision logic only. It has no SDL, window or threading dependency so
// that it can be unit tested; the host adapter in mcla_app.cpp translates SDL
// events into the plain inputs below and applies the resulting actions to the
// SDK window on the UI thread.

#include <array>
#include <cstdint>
#include <cstdlib>

namespace mcla::cursor {

// Classification of one input event for the autohide policy.
enum class Activity : uint8_t {
  kNone,
  kPad,   // Non-trivial gamepad or wheel input.
  kMouse, // Mouse motion, button or wheel.
};

enum class PadAxisSource : uint8_t {
  kGamepad,
  kJoystick,
};

// Decides whether pad/wheel axis and button events are deliberate input.
//
// Axis rule: the first value seen for an axis only seeds its baseline. Later
// values count as activity only when they move at least kAxisActivityDelta
// away from the baseline, which then follows the new value. This rejects idle
// noise and resting drift around any rest position (centred sticks, triggers
// and pedals resting at either end of the range) without knowing the device.
//
// Not thread safe; the adapter serialises calls.
class PadActivityFilter {
public:
  // About 6% of the full signed 16-bit axis range (12.5% of a stick's half
  // range, roughly 56 degrees on a 900-degree wheel).
  static constexpr int32_t kAxisActivityDelta = 4096;
  static constexpr size_t kCapacity = 64;

  Activity OnButtonDown() const { return Activity::kPad; }
  Activity OnButtonUp() const { return Activity::kNone; }
  // SDL_HAT_CENTERED is zero; any other hat value is a pressed direction.
  Activity OnHat(uint8_t value) const {
    return value != 0 ? Activity::kPad : Activity::kNone;
  }

  Activity OnAxis(PadAxisSource source, uint32_t device, uint8_t axis,
                  int16_t value) {
    Entry *entry = Find(source, device, axis);
    if (!entry) {
      Entry &slot = entries_[next_slot_];
      next_slot_ = (next_slot_ + 1) % kCapacity;
      slot = Entry{true, source, axis, device, value};
      return Activity::kNone;
    }
    const int32_t delta = std::abs(int32_t{value} - int32_t{entry->baseline});
    if (delta < kAxisActivityDelta) {
      return Activity::kNone;
    }
    entry->baseline = value;
    return Activity::kPad;
  }

  // Drops the baselines of a removed device. SDL instance IDs are not reused,
  // so this only frees slots.
  void OnDeviceRemoved(uint32_t device) {
    for (auto &entry : entries_) {
      if (entry.used && entry.device == device) {
        entry = Entry{};
      }
    }
  }

private:
  struct Entry {
    bool used = false;
    PadAxisSource source = PadAxisSource::kGamepad;
    uint8_t axis = 0;
    uint32_t device = 0;
    int16_t baseline = 0;
  };

  Entry *Find(PadAxisSource source, uint32_t device, uint8_t axis) {
    for (auto &entry : entries_) {
      if (entry.used && entry.source == source && entry.device == device &&
          entry.axis == axis) {
        return &entry;
      }
    }
    return nullptr;
  }

  std::array<Entry, kCapacity> entries_{};
  size_t next_slot_ = 0;
};

// Cursor ownership state machine. It only reverses a hide it performed
// itself, never forces a hide over another owner (for example the SDK
// keyboard/mouse driver's mouse capture), and never acts while unfocused.
//
// Not thread safe; the adapter calls it on the UI thread.
class AutoHideController {
public:
  enum class Action : uint8_t {
    kNone,
    kHide, // Set the window cursor to hidden.
    kShow, // Restore the window cursor to visible.
  };

  explicit AutoHideController(bool focused = false) : focused_(focused) {}

  bool focused() const { return focused_; }
  bool hidden() const { return hidden_; }

  // cursor_free: the window cursor is currently visible and no mouse capture
  // is requested, so hiding it does not override another owner.
  Action OnPadActivity(bool cursor_free) {
    if (!focused_ || hidden_ || !cursor_free) {
      return Action::kNone;
    }
    hidden_ = true;
    return Action::kHide;
  }

  // capture_active: another owner holds the mouse; its motion (including
  // re-centring) must not reveal the cursor. The hide stays owned so the
  // first mouse activity after the capture ends restores visibility.
  Action OnMouseActivity(bool capture_active) {
    if (!hidden_ || capture_active) {
      return Action::kNone;
    }
    hidden_ = false;
    return Action::kShow;
  }

  Action OnFocusChanged(bool focused) {
    focused_ = focused;
    if (focused || !hidden_) {
      return Action::kNone;
    }
    hidden_ = false;
    return Action::kShow;
  }

  // Restores the cursor before the host releases the window.
  Action OnShutdown() {
    focused_ = false;
    if (!hidden_) {
      return Action::kNone;
    }
    hidden_ = false;
    return Action::kShow;
  }

private:
  bool focused_ = false;
  bool hidden_ = false;
};

} // namespace mcla::cursor
