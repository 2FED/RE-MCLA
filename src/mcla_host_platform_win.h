#pragma once

// Shared compatibility-layer detection (launcher, host, crash handler).
// Wine/Proton exports wine_get_version from its ntdll; real Windows does not.

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include <atomic>

namespace mcla::platform {

namespace detail {
// -1: detect; 0: forced real Windows; 1: forced Wine. Test seam only.
inline std::atomic<int> wine_override{-1};
} // namespace detail

inline bool DetectWine() {
  const HMODULE ntdll = GetModuleHandleW(L"ntdll.dll");
  return ntdll && GetProcAddress(ntdll, "wine_get_version") != nullptr;
}

inline bool RunningUnderWine() {
  const int forced = detail::wine_override.load(std::memory_order_relaxed);
  if (forced >= 0) return forced == 1;
  static const bool detected = DetectWine();
  return detected;
}

// Tests only: force the Wine (1) or Windows (0) branch, or restore detection (-1).
// Production code never calls this; there is no environment or file trigger.
inline void SetWineOverrideForTesting(int mode) {
  detail::wine_override.store(mode, std::memory_order_relaxed);
}

} // namespace mcla::platform
