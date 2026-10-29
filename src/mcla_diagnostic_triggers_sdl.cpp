// M9-010 host adapter, see mcla_diagnostic_triggers_sdl.h.

#include "mcla_diagnostic_triggers_sdl.h"

#include "mcla_cursor_autohide.h"
#include "mcla_logging.h"

#include <rex/logging.h>
#include <rex/ui/sdl_event_observer.h>
#include <rex/ui/window.h>
#include <rex/ui/windowed_app_context.h>

#include <chrono>
#include <condition_variable>
#include <mutex>
#include <stop_token>
#include <thread>
#include <unordered_map>
#include <utility>

namespace mcla::diagnostics {

namespace {

uint64_t SteadyMs() {
  return static_cast<uint64_t>(
      std::chrono::duration_cast<std::chrono::milliseconds>(
          std::chrono::steady_clock::now().time_since_epoch())
          .count());
}

JoystickState ToJoystickState(const rex::ui::SdlJoystickSnapshot &in) {
  JoystickState out;
  out.instance_id = in.instance_id;
  out.name = in.name;
  out.guid = in.guid;
  out.vendor_id = in.vendor_id;
  out.product_id = in.product_id;
  out.joystick_type = in.joystick_type;
  out.is_gamepad = in.is_gamepad;
  out.is_wheel = in.is_wheel;
  out.opened = in.opened;
  out.haptic = in.haptic;
  out.axes = in.axes;
  out.buttons = in.buttons;
  out.hats = in.hats;
  return out;
}

} // namespace

class SdlDiagnosticTriggers final
    : public std::enable_shared_from_this<SdlDiagnosticTriggers> {
public:
  SdlDiagnosticTriggers(rex::ui::Window *window, const TriggerConfig &config,
                        TriggerCallback callback)
      : window_(window), config_(config), callback_(std::move(callback)),
        combo_(config.pad_hold_ms) {}

  ~SdlDiagnosticTriggers() {
    // Safety net only; Stop() is the normal path.
    if (observer_token_ != 0) {
      rex::ui::RemoveSdlEventObserver(observer_token_);
    }
    if (timer_.joinable()) {
      timer_.request_stop();
      timer_cv_.notify_all();
      // The timer thread may briefly hold the last reference.
      if (timer_.get_id() == std::this_thread::get_id()) {
        timer_.detach();
      }
    }
  }

  SdlDiagnosticTriggers(const SdlDiagnosticTriggers &) = delete;
  SdlDiagnosticTriggers &operator=(const SdlDiagnosticTriggers &) = delete;

  bool Install() {
    observer_token_ = rex::ui::AddSdlEventObserver(
        [this](const SDL_Event &event) { Observe(event); });
    if (observer_token_ == 0) {
      return false;
    }
    std::weak_ptr<SdlDiagnosticTriggers> weak = weak_from_this();
    timer_ = std::jthread(
        [weak, this](std::stop_token token) { TimerMain(token, weak); });
    return true;
  }

  void Stop() {
    if (observer_token_ != 0) {
      // No observer call is in flight once removal returns.
      rex::ui::RemoveSdlEventObserver(observer_token_);
      observer_token_ = 0;
    }
    if (timer_.joinable()) {
      timer_.request_stop();
      timer_cv_.notify_all();
      timer_.join();
    }
    stopped_ = true;
  }

  WheelState CaptureWheel(std::optional<uint32_t> trigger_device) {
    WheelState wheel;
    wheel.buttons = config_.wheel_buttons;
    wheel.trigger_device = trigger_device;
    for (const auto &device : rex::ui::CaptureSdlJoysticks(true)) {
      wheel.devices.push_back(ToJoystickState(device));
    }
    std::lock_guard lock(activity_mutex_);
    wheel.host_latest_known = tracker_.has_latest();
    wheel.host_latest = tracker_.latest();
    wheel.host_latest_source = tracker_.latest_source();
    wheel.host_latest_timestamp_ms = tracker_.latest_timestamp_ms();
    wheel.host_total_transitions = tracker_.total_transitions();
    wheel.host_transitions = tracker_.Transitions();
    return wheel;
  }

private:
  struct DeviceInfo {
    bool wheel = false;
    std::string name;
  };

  // Any thread that pushes SDL events. Read-only and non-blocking: it never
  // touches SDL or the window, only host state and a deferred UI post.
  void Observe(const SDL_Event &event) {
    // Joystick, gamepad and sensor events only (the SDK driver's range).
    if (event.type < SDL_EVENT_JOYSTICK_AXIS_MOTION ||
        event.type >= SDL_EVENT_FINGER_DOWN) {
      return;
    }
    TrackActivity(event);
    switch (event.type) {
    case SDL_EVENT_JOYSTICK_BUTTON_DOWN:
    case SDL_EVENT_JOYSTICK_BUTTON_UP: {
      const int button = event.jbutton.button;
      if (config_.wheel_button_probe || button == config_.wheel_buttons.l2 ||
          button == config_.wheel_buttons.r2) {
        const uint32_t device = event.jbutton.which;
        const bool down = event.type == SDL_EVENT_JOYSTICK_BUTTON_DOWN;
        Post([device, button, down](SdlDiagnosticTriggers &self) {
          self.OnJoystickButton(device, button, down);
        });
      }
      break;
    }
    case SDL_EVENT_GAMEPAD_BUTTON_DOWN:
    case SDL_EVENT_GAMEPAD_BUTTON_UP: {
      const uint8_t button = event.gbutton.button;
      if (button == SDL_GAMEPAD_BUTTON_START ||
          button == SDL_GAMEPAD_BUTTON_BACK) {
        const uint32_t device = event.gbutton.which;
        const bool down = event.type == SDL_EVENT_GAMEPAD_BUTTON_DOWN;
        const auto which = button == SDL_GAMEPAD_BUTTON_START
                               ? HoldComboTrigger::Button::kStart
                               : HoldComboTrigger::Button::kBack;
        Post([device, which, down](SdlDiagnosticTriggers &self) {
          self.OnPadButton(device, which, down);
        });
      }
      break;
    }
    case SDL_EVENT_JOYSTICK_REMOVED:
    case SDL_EVENT_GAMEPAD_REMOVED: {
      const uint32_t device = event.type == SDL_EVENT_JOYSTICK_REMOVED
                                  ? event.jdevice.which
                                  : event.gdevice.which;
      Post([device](SdlDiagnosticTriggers &self) { self.OnRemoved(device); });
      break;
    }
    default:
      break;
    }
  }

  // Host-observed latest meaningful input device (same activity rule as the
  // M9-009 cursor autohide).
  void TrackActivity(const SDL_Event &event) {
    using cursor::Activity;
    using cursor::PadAxisSource;
    const uint64_t timestamp_ms = event.common.timestamp / 1000000u;
    std::lock_guard lock(activity_mutex_);
    Activity activity = Activity::kNone;
    uint32_t device = 0;
    InputSource source = InputSource::kJoystick;
    switch (event.type) {
    case SDL_EVENT_JOYSTICK_BUTTON_DOWN:
      activity = filter_.OnButtonDown();
      device = event.jbutton.which;
      break;
    case SDL_EVENT_GAMEPAD_BUTTON_DOWN:
      activity = filter_.OnButtonDown();
      device = event.gbutton.which;
      source = InputSource::kGamepad;
      break;
    case SDL_EVENT_JOYSTICK_HAT_MOTION:
      activity = filter_.OnHat(event.jhat.value);
      device = event.jhat.which;
      break;
    case SDL_EVENT_JOYSTICK_AXIS_MOTION:
      activity = filter_.OnAxis(PadAxisSource::kJoystick, event.jaxis.which,
                                event.jaxis.axis, event.jaxis.value);
      device = event.jaxis.which;
      break;
    case SDL_EVENT_GAMEPAD_AXIS_MOTION:
      activity = filter_.OnAxis(PadAxisSource::kGamepad, event.gaxis.which,
                                event.gaxis.axis, event.gaxis.value);
      device = event.gaxis.which;
      source = InputSource::kGamepad;
      break;
    case SDL_EVENT_JOYSTICK_REMOVED:
      filter_.OnDeviceRemoved(event.jdevice.which);
      break;
    case SDL_EVENT_GAMEPAD_REMOVED:
      filter_.OnDeviceRemoved(event.gdevice.which);
      break;
    default:
      break;
    }
    if (activity == Activity::kPad) {
      tracker_.OnActivity(device, source, timestamp_ms);
    }
  }

  template <typename Function> void Post(Function function) {
    std::weak_ptr<SdlDiagnosticTriggers> weak = weak_from_this();
    window_->app_context().CallInUIThreadDeferred(
        [weak, function = std::move(function)]() {
          if (auto self = weak.lock(); self && !self->stopped_) {
            function(*self);
          }
        });
  }

  // UI thread.
  const DeviceInfo &Info(uint32_t device) {
    auto it = devices_.find(device);
    if (it == devices_.end()) {
      DeviceInfo info;
      if (const auto snapshot = rex::ui::QuerySdlJoystick(device, false)) {
        info.wheel = snapshot->is_wheel;
        info.name = snapshot->name;
      }
      it = devices_.emplace(device, std::move(info)).first;
    }
    return it->second;
  }

  // UI thread.
  void OnJoystickButton(uint32_t device, int button, bool down) {
    const DeviceInfo &info = Info(device);
    if (config_.wheel_button_probe && down) {
      MCLA_INPUT_INFO("MCLA_WHEEL_BUTTON_PROBE v=1 instance={} wheel={} "
                      "button={} name=\"{}\"",
                      device, info.wheel ? 1 : 0, button, info.name);
    }
    if (!info.wheel) {
      return;
    }
    const struct {
      int index;
      ButtonEdgeTrigger *trigger;
      TriggerKind kind;
    } roles[] = {{config_.wheel_buttons.r2, &r2_, TriggerKind::kWheelR2},
                 {config_.wheel_buttons.l2, &l2_, TriggerKind::kWheelL2}};
    for (const auto &role : roles) {
      if (role.index < 0 || role.index != button) {
        continue;
      }
      if (!down) {
        role.trigger->OnUp(device);
      } else if (role.trigger->OnDown(device) && callback_) {
        callback_(TriggerEvent{role.kind, device});
      }
    }
  }

  // UI thread.
  void OnPadButton(uint32_t device, HoldComboTrigger::Button button,
                   bool down) {
    // A wheel that SDL also maps as a gamepad must not trigger "pad".
    if (Info(device).wheel) {
      return;
    }
    if (combo_.OnButton(device, button, down, SteadyMs())) {
      combo_owner_ = device;
      {
        std::lock_guard lock(timer_mutex_);
        timer_deadline_ms_ = combo_.deadline_ms();
      }
      timer_cv_.notify_all();
    }
  }

  // UI thread.
  void Poll() {
    if (combo_.Poll(SteadyMs()) && callback_) {
      callback_(TriggerEvent{TriggerKind::kPad, combo_owner_});
    }
  }

  // UI thread.
  void OnRemoved(uint32_t device) {
    devices_.erase(device);
    l2_.OnDeviceRemoved(device);
    r2_.OnDeviceRemoved(device);
    combo_.OnDeviceRemoved(device);
  }

  // Sleeps until the armed hold deadline, then asks the UI thread to poll.
  void TimerMain(std::stop_token token,
                 std::weak_ptr<SdlDiagnosticTriggers> weak) {
    std::unique_lock lock(timer_mutex_);
    while (!token.stop_requested()) {
      if (timer_deadline_ms_ == 0) {
        timer_cv_.wait(lock, token, [this] { return timer_deadline_ms_ != 0; });
        continue;
      }
      const uint64_t now = SteadyMs();
      if (now < timer_deadline_ms_) {
        timer_cv_.wait_for(lock, token,
                           std::chrono::milliseconds(timer_deadline_ms_ - now),
                           [] { return false; });
        continue;
      }
      timer_deadline_ms_ = 0;
      lock.unlock();
      if (auto self = weak.lock()) {
        self->Post([](SdlDiagnosticTriggers &triggers) { triggers.Poll(); });
      }
      lock.lock();
    }
  }

  rex::ui::Window *const window_;
  const TriggerConfig config_;
  const TriggerCallback callback_;
  rex::ui::SdlEventObserverToken observer_token_ = 0; // UI thread only.
  bool stopped_ = false;                               // UI thread only.

  // Observer threads.
  std::mutex activity_mutex_;
  cursor::PadActivityFilter filter_;
  ActiveDeviceTracker tracker_;

  // UI thread only.
  std::unordered_map<uint32_t, DeviceInfo> devices_;
  ButtonEdgeTrigger l2_;
  ButtonEdgeTrigger r2_;
  HoldComboTrigger combo_;
  uint32_t combo_owner_ = 0;

  // Hold timer.
  std::mutex timer_mutex_;
  std::condition_variable_any timer_cv_;
  uint64_t timer_deadline_ms_ = 0; // Guarded by timer_mutex_; 0 = idle.
  std::jthread timer_;
};

std::shared_ptr<SdlDiagnosticTriggers>
InstallSdlDiagnosticTriggers(rex::ui::Window *window, const TriggerConfig &config,
                             TriggerCallback callback) {
  if (!window) {
    return nullptr;
  }
  auto triggers =
      std::make_shared<SdlDiagnosticTriggers>(window, config, std::move(callback));
  if (!triggers->Install()) {
    return nullptr;
  }
  return triggers;
}

WheelState CaptureWheelState(SdlDiagnosticTriggers &triggers,
                             std::optional<uint32_t> trigger_device) {
  return triggers.CaptureWheel(trigger_device);
}

void Shutdown(SdlDiagnosticTriggers &triggers) { triggers.Stop(); }

} // namespace mcla::diagnostics
