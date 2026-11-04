#pragma once

#include <string_view>

namespace mcla {

// Raises the SDK texture cache limits to match the GPU's video-memory budget,
// unless the user set them, and starts a monitor that lowers the hard limit if
// the process approaches its budget. No-op where the budget cannot be queried.
// The monitor also writes the MCLA_VRAM census line (M10-007); the mark adds
// one line for an accepted F10 (no-op while the monitor is not running).
#if defined(_WIN32)
void ApplyTextureCacheBudget();
void MarkTextureCacheBudgetCensus(std::string_view reason);
void StopTextureCacheBudgetMonitor();
#else
inline void ApplyTextureCacheBudget() {}
inline void MarkTextureCacheBudgetCensus(std::string_view) {}
inline void StopTextureCacheBudgetMonitor() {}
#endif

}  // namespace mcla
