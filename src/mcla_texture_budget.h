#pragma once

namespace mcla {

// Raises the SDK texture cache limits to match the GPU's video-memory budget,
// unless the user set them, and starts a monitor that lowers the hard limit if
// the process approaches its budget. No-op where the budget cannot be queried.
#if defined(_WIN32)
void ApplyTextureCacheBudget();
void StopTextureCacheBudgetMonitor();
#else
inline void ApplyTextureCacheBudget() {}
inline void StopTextureCacheBudgetMonitor() {}
#endif

}  // namespace mcla
