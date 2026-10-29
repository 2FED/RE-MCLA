// M9-009 host adapter, see mcla_cursor_autohide_sdl.h.

#include "mcla_cursor_autohide_sdl.h"

#include "mcla_cursor_autohide.h"

#include <rex/ui/sdl_event_observer.h>
#include <rex/ui/window.h>
#include <rex/ui/windowed_app_context.h>


#include <atomic>
#include <cstdint>
#include <mutex>

namespace mcla::cursor {

class SdlAutoHide final : public std::enable_shared_from_this<SdlAutoHide> {
public:
  explicit SdlAutoHide(rex::ui::Window *window)
      : window_(window), controller_(window->HasFocus()) {
    focused_.store(controller_.focused(), std::memory_order_relaxed);
  }

  ~SdlAutoHide() {
    // Safety net only; Stop() is the normal path and also restores the cursor.
    if (observer_token_ != 0) {
      rex::ui::RemoveSdlEventObserver(observer_token_);
    }
  }

  SdlAutoHide(const SdlAutoHide &) = delete;
  SdlAutoHide &operator=(const SdlAutoHide &) = delete;

  bool Install() {
    // The observer runs on the SDK's own SDL instance (inside rexruntime);
    // the host links no SDL code of its own.
    observer_token_ = rex::ui::AddSdlEventObserver(
        [this](const SDL_Event &event) { Observe(event); });
    watch_installed_ = observer_token_ != 0;
    return watch_installed_;
  }

  void FocusChanged(bool focused) {
    if (!watch_installed_) {
      return;
    }
    focused_.store(focused, std::memory_order_relaxed);
    if (controller_.OnFocusChanged(focused) ==
        AutoHideController::Action::kShow) {
      // Apply after the remaining focus listeners (for example the SDK
      // keyboard/mouse driver releasing its capture) have run.
      hidden_.store(false, std::memory_order_relaxed);
      std::weak_ptr<SdlAutoHide> weak = weak_from_this();
      window_->app_context().CallInUIThreadDeferred([weak]() {
        if (auto self = weak.lock()) {
          self->ShowIfStillHidden();
        }
      });
    }
  }

  void Stop() {
    if (!watch_installed_) {
      return;
    }
    // No observer call is in flight once removal returns.
    rex::ui::RemoveSdlEventObserver(observer_token_);
    observer_token_ = 0;
    watch_installed_ = false;
    if (controller_.OnShutdown() == AutoHideController::Action::kShow) {
      ShowIfStillHidden();
    }
    hidden_.store(false, std::memory_order_relaxed);
  }

private:
  using Visibility = rex::ui::Window::CursorVisibility;

  // Any thread that pushes SDL events (UI thread or an SDL joystick thread).
  void Observe(const SDL_Event &event) {
    const Activity activity = Classify(event);
    if (activity == Activity::kPad) {
      if (focused_.load(std::memory_order_relaxed) &&
          !hidden_.load(std::memory_order_relaxed)) {
        Request(activity);
      }
    } else if (activity == Activity::kMouse) {
      // Also supersede a queued pad hide so the latest input wins.
      if (hidden_.load(std::memory_order_relaxed) ||
          post_queued_.load(std::memory_order_acquire)) {
        Request(activity);
      }
    }
  }

  Activity Classify(const SDL_Event &event) {
    switch (event.type) {
    case SDL_EVENT_GAMEPAD_BUTTON_DOWN:
    case SDL_EVENT_JOYSTICK_BUTTON_DOWN:
      return filter_.OnButtonDown();
    case SDL_EVENT_JOYSTICK_HAT_MOTION:
      return filter_.OnHat(event.jhat.value);
    case SDL_EVENT_GAMEPAD_AXIS_MOTION: {
      std::lock_guard lock(filter_mutex_);
      return filter_.OnAxis(PadAxisSource::kGamepad, event.gaxis.which,
                            event.gaxis.axis, event.gaxis.value);
    }
    case SDL_EVENT_JOYSTICK_AXIS_MOTION: {
      std::lock_guard lock(filter_mutex_);
      return filter_.OnAxis(PadAxisSource::kJoystick, event.jaxis.which,
                            event.jaxis.axis, event.jaxis.value);
    }
    case SDL_EVENT_GAMEPAD_REMOVED: {
      std::lock_guard lock(filter_mutex_);
      filter_.OnDeviceRemoved(event.gdevice.which);
      return Activity::kNone;
    }
    case SDL_EVENT_JOYSTICK_REMOVED: {
      std::lock_guard lock(filter_mutex_);
      filter_.OnDeviceRemoved(event.jdevice.which);
      return Activity::kNone;
    }
    case SDL_EVENT_MOUSE_MOTION:
      // Ignore zero-delta motion so a cursor state change cannot re-show it.
      return (event.motion.xrel != 0.0f || event.motion.yrel != 0.0f)
                 ? Activity::kMouse
                 : Activity::kNone;
    case SDL_EVENT_MOUSE_BUTTON_DOWN:
    case SDL_EVENT_MOUSE_WHEEL:
      return Activity::kMouse;
    default:
      return Activity::kNone;
    }
  }

  // Coalesces bursts into one deferred UI-thread pass; the latest activity
  // wins. CallInUIThreadDeferred never runs inline, so no window state is
  // touched from inside SDL's event dispatch.
  void Request(Activity activity) {
    pending_.store(static_cast<uint8_t>(activity), std::memory_order_relaxed);
    if (post_queued_.exchange(true, std::memory_order_acq_rel)) {
      return;
    }
    std::weak_ptr<SdlAutoHide> weak = weak_from_this();
    if (!window_->app_context().CallInUIThreadDeferred([weak]() {
          if (auto self = weak.lock()) {
            self->Drain();
          }
        })) {
      post_queued_.store(false, std::memory_order_release);
    }
  }

  // UI thread.
  void Drain() {
    post_queued_.store(false, std::memory_order_release);
    const auto activity = static_cast<Activity>(
        pending_.exchange(static_cast<uint8_t>(Activity::kNone),
                          std::memory_order_relaxed));
    if (!watch_installed_) {
      return;
    }
    const bool captured = window_->IsMouseCaptureRequested();
    if (activity == Activity::kPad) {
      const bool cursor_free =
          !captured && window_->GetCursorVisibility() == Visibility::kVisible;
      if (controller_.OnPadActivity(cursor_free) ==
          AutoHideController::Action::kHide) {
        window_->SetCursorVisibility(Visibility::kHidden);
      }
    } else if (activity == Activity::kMouse) {
      if (controller_.OnMouseActivity(captured) ==
          AutoHideController::Action::kShow) {
        ShowIfStillHidden();
      }
    }
    hidden_.store(controller_.hidden(), std::memory_order_relaxed);
  }

  // UI thread. Only reverses a hide while nobody else owns the cursor.
  void ShowIfStillHidden() {
    if (window_->GetCursorVisibility() == Visibility::kHidden &&
        !window_->IsMouseCaptureRequested()) {
      window_->SetCursorVisibility(Visibility::kVisible);
    }
  }

  rex::ui::Window *const window_;
  AutoHideController controller_;       // UI thread only.
  bool watch_installed_ = false;        // UI thread only.
  rex::ui::SdlEventObserverToken observer_token_ = 0; // UI thread only.
  std::mutex filter_mutex_;
  PadActivityFilter filter_;            // Guarded by filter_mutex_ for axes.
  std::atomic<bool> focused_{false};    // Mirror of controller_ for Watch.
  std::atomic<bool> hidden_{false};     // Mirror of controller_ for Watch.
  std::atomic<bool> post_queued_{false};
  std::atomic<uint8_t> pending_{static_cast<uint8_t>(Activity::kNone)};
};

std::shared_ptr<SdlAutoHide> InstallSdlAutoHide(rex::ui::Window *window) {
  if (!window) {
    return nullptr;
  }
  auto autohide = std::make_shared<SdlAutoHide>(window);
  if (!autohide->Install()) {
    return nullptr;
  }
  return autohide;
}

void NotifyFocusChanged(SdlAutoHide &autohide, bool focused) {
  autohide.FocusChanged(focused);
}

void Shutdown(SdlAutoHide &autohide) { autohide.Stop(); }

} // namespace mcla::cursor
