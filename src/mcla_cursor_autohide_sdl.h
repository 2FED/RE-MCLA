#pragma once

// M9-009 host adapter: observes SDL gamepad, joystick (wheel) and mouse
// events and drives the game window's cursor visibility through the SDK
// rex::ui::Window API. Decision logic lives in mcla_cursor_autohide.h.

#include <memory>

namespace rex::ui {
class Window;
}

namespace mcla::cursor {

class SdlAutoHide;

// Creates and registers the adapter on the UI thread. Returns null if the
// SDL event watch cannot be installed. The caller must call Shutdown on the
// UI thread before the window is destroyed.
std::shared_ptr<SdlAutoHide> InstallSdlAutoHide(rex::ui::Window *window);

// UI thread only.
void NotifyFocusChanged(SdlAutoHide &autohide, bool focused);

// UI thread only. Removes the event watch and restores a cursor this adapter
// hid. Idempotent.
void Shutdown(SdlAutoHide &autohide);

} // namespace mcla::cursor
