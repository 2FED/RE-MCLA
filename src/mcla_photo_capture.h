#pragma once

// KI-037 photo-capture pipeline wait: pure window state machine, no SDK
// dependencies, so it can be unit-tested (tests/mcla_photo_capture_test.cpp).
// The guest hooks and the cvar access live in mcla_photo_capture.cpp.
//
// The window opens when the game's capture freeze takes effect and closes at
// the unfreeze. While it is open the GPU cvar d3d12_async_pipeline_wait is on,
// so the plugin waits for pipelines still being created instead of dropping
// their draws from the captured frame. A safety timeout (frames or wall time,
// whichever comes first) and host shutdown also close it. The controller never
// turns the cvar off when the user had already turned it on.

#include <cstdint>
#include <optional>

namespace mcla::photo_capture {

// The guest starts reading the capture after more than 3 frozen frames; this
// leaves ample room and still bounds a freeze that never gets its unfreeze.
inline constexpr uint32_t kTimeoutFrames = 120;
inline constexpr uint64_t kTimeoutMs = 5000;

// At most this many transitions are logged per process.
inline constexpr int kMaxLoggedTransitions = 50;

enum class Event { kOpen, kClose, kTimeout, kShutdown };

inline const char *EventName(Event event) {
  switch (event) {
    case Event::kOpen:
      return "window open";
    case Event::kClose:
      return "window closed";
    case Event::kTimeout:
      return "window timed out";
    case Event::kShutdown:
      return "shutdown";
  }
  return "?";
}

struct Decision {
  // True when the window state changed (log candidates).
  bool transition = false;
  // True when this transition should be logged (bounded count).
  bool log = false;
  Event event = Event::kOpen;
  // Value to write to d3d12_async_pipeline_wait, if any.
  std::optional<bool> write_wait;
  // Frames and milliseconds the window was open (close/timeout/shutdown).
  uint32_t frames = 0;
  uint64_t elapsed_ms = 0;
};

class Window {
 public:
  explicit Window(bool enabled) : enabled_(enabled) {}

  bool enabled() const { return enabled_; }
  bool open() const { return open_; }
  bool owning() const { return owning_; }
  bool stopped() const { return stopped_; }

  // The capture freeze took effect. `current_wait` is the cvar value now.
  Decision Open(uint64_t now_ms, bool current_wait) {
    Decision d;
    if (!enabled_ || stopped_ || open_) {
      return d;
    }
    open_ = true;
    frames_ = 0;
    opened_ms_ = now_ms;
    owning_ = !current_wait;
    if (owning_) {
      d.write_wait = true;
    }
    Transition(d, Event::kOpen);
    return d;
  }

  // The unfreeze ran.
  Decision Close(uint64_t now_ms) { return End(now_ms, Event::kClose); }

  // Called once per game frame; closes the window after the timeout.
  Decision Tick(uint64_t now_ms) {
    if (!open_) {
      return {};
    }
    ++frames_;
    if (frames_ >= kTimeoutFrames || now_ms - opened_ms_ >= kTimeoutMs) {
      return End(now_ms, Event::kTimeout);
    }
    return {};
  }

  // Host shutdown: close the window and ignore every later call.
  Decision Shutdown(uint64_t now_ms) {
    Decision d = End(now_ms, Event::kShutdown);
    stopped_ = true;
    return d;
  }

 private:
  Decision End(uint64_t now_ms, Event event) {
    Decision d;
    if (!open_) {
      return d;
    }
    open_ = false;
    d.frames = frames_;
    d.elapsed_ms = now_ms - opened_ms_;
    if (owning_) {
      d.write_wait = false;
      owning_ = false;
    }
    Transition(d, event);
    return d;
  }

  void Transition(Decision &d, Event event) {
    d.transition = true;
    d.event = event;
    if (logged_ < kMaxLoggedTransitions) {
      ++logged_;
      d.log = true;
    }
  }

  bool enabled_;
  bool open_ = false;
  bool owning_ = false;
  bool stopped_ = false;
  uint32_t frames_ = 0;
  uint64_t opened_ms_ = 0;
  int logged_ = 0;
};

// KI-047 always-on pipeline wait: at startup the host turns on the SDK's
// hot-reload cvar d3d12_async_pipeline_wait_always (bounded per wait and per
// minute in the SDK), so draws are not skipped while their pipelines are
// still being created, outside Photo Mode too. A value the user set is left
// alone (the registry reports only a non-default value, i.e. true; to turn
// the mode off use mcla_pipeline_wait_always=false); an owned value is turned
// off at shutdown so a settings save cannot persist it.
enum class AlwaysWaitStart {
  kDisabledByOption,  // mcla_pipeline_wait_always=false: nothing written
  kUnavailable,       // the GPU plugin has no always-on cvar (older SDK)
  kUserSet,           // the user set the cvar: left alone, not owned
  kEnable,            // write true and own it until shutdown
};

inline AlwaysWaitStart DecideAlwaysWaitStart(bool option_on, bool sdk_has_cvar,
                                             bool user_set) {
  if (!option_on) {
    return AlwaysWaitStart::kDisabledByOption;
  }
  if (!sdk_has_cvar) {
    return AlwaysWaitStart::kUnavailable;
  }
  return user_set ? AlwaysWaitStart::kUserSet : AlwaysWaitStart::kEnable;
}

// Value to write at shutdown: false only when the host owns the cvar.
inline std::optional<bool> AlwaysWaitShutdownWrite(bool owned) {
  if (owned) {
    return false;
  }
  return std::nullopt;
}

// Startup hook (defined in mcla_photo_capture.cpp), called once the GPU
// plugin and its cvars are loaded.
void StartAlwaysPipelineWait();

// Host shutdown hook (defined in mcla_photo_capture.cpp): turns an owned
// d3d12_async_pipeline_wait and an owned d3d12_async_pipeline_wait_always off
// so a later settings save cannot persist them.
void ShutdownPhotoCapture();

}  // namespace mcla::photo_capture
