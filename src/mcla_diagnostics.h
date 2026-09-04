// mcla - privacy-bounded live diagnostics and crash package orchestration

#pragma once

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <memory>
#include <string>

namespace mcla::diagnostics {

struct UiState {
  uint32_t logical_width = 0;
  uint32_t logical_height = 0;
  uint32_t physical_width = 0;
  uint32_t physical_height = 0;
  bool focused = false;
  bool fullscreen = false;
  uintptr_t native_window_handle = 0;
};

struct RuntimeState {
  bool runtime_available = false;
  uint32_t title_id = 0;
  uint64_t guest_output_sequence = 0;
  uint64_t guest_vblank_sequence = 0;
  uint32_t graphics_backend = 0;
  uint64_t graphics_guest_frames_opened = 0;
  uint64_t graphics_guest_frames_closed = 0;
  uint64_t graphics_issue_swap_calls = 0;
  uint64_t graphics_guest_output_refresh_successes = 0;
  uint64_t graphics_guest_output_refresh_failures = 0;
  uint64_t graphics_async_placeholder_skips = 0;
  uint64_t graphics_swap_source_missing = 0;
  uint64_t graphics_submission_count = 0;
  uint64_t graphics_frame_fence_wait_us = 0;
  uint64_t graphics_pipeline_end_submission_us = 0;
  uint64_t graphics_deferred_execute_us = 0;
  uint64_t graphics_queue_submit_us = 0;
  uint64_t graphics_guest_frame_wall_us = 0;
  uint64_t race_back_sequence = 0;
  uint64_t race_back_handler_calls = 0;
  uint64_t race_back_apply_calls = 0;
};

using CaptureProvider = std::function<RuntimeState()>;

enum class SnapshotStatus : uint8_t {
  kIdle,
  kCapturing,
  kComplete,
  kFailed,
};

class Manager {
public:
  Manager();
  ~Manager();
  Manager(const Manager &) = delete;
  Manager &operator=(const Manager &) = delete;

  bool Start(const std::filesystem::path &user_data_root,
             const std::filesystem::path &diagnostics_root,
             CaptureProvider provider, bool show_crash_reporter_dialog);
  bool RequestSnapshot(std::string reason, const UiState &ui_state);
  SnapshotStatus snapshot_status() const;
  bool WaitForIdle(std::chrono::milliseconds timeout);
  void Stop();

private:
  class Impl;
  std::unique_ptr<Impl> impl_;
};

} // namespace mcla::diagnostics
