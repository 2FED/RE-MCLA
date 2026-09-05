// Host-only, address-free samples for the opt-in readback-cost experiment.
#pragma once

#include "mcla_diagnostics.h"

#include <cstdint>
#include <locale>
#include <ostream>
#include <sstream>
#include <string>
#include <string_view>

namespace mcla::diagnostics {

// These are overlapping cumulative counters, not an exclusive time budget.
// Invalid input never writes to the destination. A failed destination write
// returns false, but a generic ostream cannot roll back partially written bytes.
inline bool WriteReadbackCostSample(std::ostream &output, std::string_view edge,
                                   std::string_view mode, uint64_t monotonic_us,
                                   const RuntimeState &state) {
  if ((edge != "begin" && edge != "end") ||
      (mode != "fast" && mode != "some") || monotonic_us == 0 ||
      !state.runtime_available || !output.good()) {
    return false;
  }

  // Do not inherit the destination's locale, base, width, or boolean formatting.
  // Whitelisted strings need no JSON escaping; all other values are unsigned.
  std::ostringstream record;
  record.imbue(std::locale::classic());
  record << "{\"schema\":\"mcla-readback-cost-sample-v2\","
            "\"diagnostic_only\":true,\"audio_counters_available\":false,"
            "\"edge\":\""
         << edge << "\",\"mode\":\"" << mode
         << "\",\"monotonic_us\":" << monotonic_us
         << ",\"title_id\":" << state.title_id
         << ",\"backend\":" << state.graphics_backend
         << ",\"guest_output_sequence\":" << state.guest_output_sequence
         << ",\"guest_vblank_sequence\":" << state.guest_vblank_sequence
         << ",\"frames_closed\":" << state.graphics_guest_frames_closed
         << ",\"draw_packet_count\":" << state.graphics_command_draw_packet_count
         << ",\"draw_packet_us\":" << state.graphics_command_draw_packet_us
         << ",\"primary_buffer_count\":"
         << state.graphics_command_primary_buffer_count
         << ",\"primary_buffer_us\":" << state.graphics_command_primary_buffer_us
         << ",\"resolve_count\":" << state.graphics_vulkan_resolve_count
         << ",\"resolve_us\":" << state.graphics_vulkan_resolve_us
         << ",\"resolve_readback_copy_count\":"
         << state.graphics_vulkan_resolve_readback_copy_count
         << ",\"resolve_readback_bytes\":"
         << state.graphics_vulkan_resolve_readback_bytes
         << ",\"resolve_readback_copy_us\":"
         << state.graphics_vulkan_resolve_readback_copy_us
         << ",\"resolve_readback_wait_count\":"
         << state.graphics_vulkan_resolve_readback_wait_count
         << ",\"resolve_readback_wait_us\":"
         << state.graphics_vulkan_resolve_readback_wait_us
         << ",\"watch_arm_calls\":" << state.graphics_memory_watch_arm_calls
         << ",\"watch_arm_us\":" << state.graphics_memory_watch_arm_us
         << ",\"watch_disarm_calls\":" << state.graphics_memory_watch_disarm_calls
         << ",\"watch_disarm_us\":" << state.graphics_memory_watch_disarm_us
         << ",\"watch_arm_gap_splits\":" << state.graphics_memory_watch_arm_gap_splits
         << ",\"watch_disarm_safe_gap_splits\":"
         << state.graphics_memory_watch_disarm_safe_gap_splits
         << ",\"watch_disarm_gap_lock_misses\":"
         << state.graphics_memory_watch_disarm_gap_lock_misses
         << ",\"non_mmio_write_fault_count\":"
         << state.graphics_memory_non_mmio_write_fault_count
         << ",\"non_mmio_write_fault_us\":"
         << state.graphics_memory_non_mmio_write_fault_us
         << ",\"watch_protect_failures\":"
         << state.graphics_memory_watch_protect_failures
         << ",\"binding_update_count\":"
         << state.graphics_vulkan_binding_update_count
         << ",\"binding_update_us\":" << state.graphics_vulkan_binding_update_us
         << "}\n";
  if (!record.good()) {
    return false;
  }
  const std::string line = record.str();
  try {
    output.write(line.data(), static_cast<std::streamsize>(line.size()));
  } catch (const std::ios_base::failure &) {
    return false;
  }
  return output.good();
}

} // namespace mcla::diagnostics
