#pragma once
#include <cstddef>

namespace mcla::history_limits {
// Fixed diagnostic envelope, not an arbitrary-rate coverage guarantee.
// 180 seconds at 60 iterations/s, two operations and five worker events each:
// 21600 operations / 54000 events, with headroom. Overflow still fails closed.
inline constexpr std::size_t kOperations = 32768;
inline constexpr std::size_t kWorkerEvents = 65536;
inline constexpr std::size_t kSemaphoreJsonBytes = 32 * 1024 * 1024;
inline constexpr std::size_t kHandoffJsonBytes = 48 * 1024 * 1024;
}
