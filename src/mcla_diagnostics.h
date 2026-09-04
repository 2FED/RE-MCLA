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
  uint64_t race_back_sequence = 0;
  uint64_t race_back_handler_calls = 0;
  uint64_t race_back_apply_calls = 0;
};

using CaptureProvider = std::function<RuntimeState()>;

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
  bool WaitForIdle(std::chrono::milliseconds timeout);
  void Stop();

private:
  class Impl;
  std::unique_ptr<Impl> impl_;
};

} // namespace mcla::diagnostics
