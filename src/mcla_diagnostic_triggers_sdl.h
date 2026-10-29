#pragma once

// M9-010 host adapter: observes the SDK's SDL event stream (read-only, see
// rex/ui/sdl_event_observer.h) and turns wheel L2/R2 raw joystick buttons and
// a held gamepad Start+Back into diagnostic snapshot requests. Decision logic
// lives in mcla_diagnostic_trigger.h. Nothing here consumes or changes guest
// input: the SDK input driver receives every event unchanged.

#include "mcla_diagnostic_trigger.h"

#include <cstdint>
#include <functional>
#include <memory>
#include <optional>

namespace rex::ui {
class Window;
}

namespace mcla::diagnostics {

class SdlDiagnosticTriggers;

struct TriggerConfig {
  WheelButtons wheel_buttons;
  bool wheel_button_probe = false; // Log every raw joystick button press.
  uint32_t pad_hold_ms = 1000;
};

enum class TriggerKind : uint8_t { kWheelR2, kWheelL2, kPad };

struct TriggerEvent {
  TriggerKind kind = TriggerKind::kWheelR2;
  uint32_t device = 0;
};

// Called on the UI thread, once per accepted press or completed hold.
using TriggerCallback = std::function<void(const TriggerEvent &)>;

// UI thread. Returns null if the SDK event observer cannot be installed. The
// caller must call Shutdown on the UI thread before the window is destroyed.
std::shared_ptr<SdlDiagnosticTriggers>
InstallSdlDiagnosticTriggers(rex::ui::Window *window, const TriggerConfig &config,
                             TriggerCallback callback);

// UI thread. Wheel state for a wheel snapshot: SDL joystick snapshot from the
// SDK's SDL instance plus the host-observed active-device ring buffer. The
// SDK log sections are filled later by the package worker.
WheelState CaptureWheelState(SdlDiagnosticTriggers &triggers,
                             std::optional<uint32_t> trigger_device);

// UI thread. Removes the observer and stops the hold timer. Idempotent.
void Shutdown(SdlDiagnosticTriggers &triggers);

} // namespace mcla::diagnostics
