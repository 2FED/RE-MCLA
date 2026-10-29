// mcla - privacy-bounded live diagnostics and crash package orchestration

#pragma once

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <memory>
#include <string>
#include <vector>

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
  uint64_t graphics_command_ring_idle_us = 0;
  uint64_t graphics_command_wait_reg_mem_us = 0;
  uint64_t graphics_memory_non_mmio_write_fault_count = 0;
  uint64_t graphics_memory_non_mmio_write_fault_us = 0;
  uint64_t graphics_command_primary_buffer_count = 0;
  uint64_t graphics_command_primary_buffer_us = 0;
  uint64_t graphics_command_draw_packet_count = 0;
  uint64_t graphics_command_draw_packet_us = 0;
  uint64_t graphics_command_swap_packet_count = 0;
  uint64_t graphics_command_swap_packet_us = 0;
  uint64_t graphics_command_interrupt_packet_count = 0;
  uint64_t graphics_command_interrupt_packet_us = 0;
  uint64_t graphics_vulkan_fence_call_count = 0;
  uint64_t graphics_vulkan_fence_call_us = 0;
  uint64_t graphics_memory_write_fault_lock_wait_us = 0;
  uint64_t graphics_memory_write_fault_query_us = 0;
  uint64_t graphics_memory_write_fault_callback_us = 0;
  uint64_t graphics_memory_write_fault_already_cleared_count = 0;
  uint64_t graphics_memory_watch_arm_calls = 0;
  uint64_t graphics_memory_watch_arm_us = 0;
  uint64_t graphics_memory_watch_arm_lock_wait_us = 0;
  uint64_t graphics_memory_watch_disarm_calls = 0;
  uint64_t graphics_memory_watch_disarm_us = 0;
  uint64_t graphics_memory_watch_protect_failures = 0;
  uint64_t graphics_memory_watch_arm_gap_splits = 0;
  uint64_t graphics_memory_watch_disarm_safe_gap_splits = 0;
  uint64_t graphics_memory_watch_disarm_gap_lock_misses = 0;
  uint64_t graphics_vulkan_resolve_count = 0;
  uint64_t graphics_vulkan_resolve_us = 0;
  uint64_t graphics_vulkan_resolve_readback_copy_count = 0;
  uint64_t graphics_vulkan_resolve_readback_bytes = 0;
  uint64_t graphics_vulkan_resolve_readback_copy_us = 0;
  uint64_t graphics_vulkan_resolve_readback_wait_count = 0;
  uint64_t graphics_vulkan_resolve_readback_wait_us = 0;
  uint64_t graphics_vulkan_binding_update_count = 0;
  uint64_t graphics_vulkan_binding_update_us = 0;
  uint64_t graphics_vulkan_texture_descriptor_reuse_count = 0;
  uint64_t graphics_vulkan_texture_descriptor_write_count = 0;
  uint64_t graphics_vulkan_descriptor_update_count = 0;
  uint64_t graphics_vulkan_descriptor_update_us = 0;
  uint64_t race_back_sequence = 0;
  uint64_t race_back_handler_calls = 0;
  uint64_t race_back_apply_calls = 0;
  // KI-026 guest warp/camera-pause snapshot (JSON object; empty if unavailable).
  std::string guest_ki026_json;
};

using CaptureProvider = std::function<RuntimeState()>;

// The game's last presented output image (R8 G8 B8 X8 rows), for F10 frames
// (KI-042: a window BitBlt of the flip-model swapchain can return a stale
// image). The provider may wait on the GPU; the package worker bounds that wait.
struct GuestFrame {
  uint32_t width = 0;
  uint32_t height = 0;
  size_t stride = 0;
  std::vector<uint8_t> rgbx;
};
using GuestFrameProvider = std::function<bool(GuestFrame &)>;

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
  // Optional; without it F10 keeps the window capture with staleness checks.
  void SetGuestFrameProvider(GuestFrameProvider provider);
  SnapshotStatus snapshot_status() const;
  bool WaitForIdle(std::chrono::milliseconds timeout);
  void Stop();

private:
  class Impl;
  std::unique_ptr<Impl> impl_;
};

} // namespace mcla::diagnostics
