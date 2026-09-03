// mcla - portable fallback when native minidumps are unavailable

#include "mcla_native_crash.h"

#include <cstdlib>

namespace mcla::diagnostics::native {

bool Start(const std::filesystem::path &, const std::filesystem::path &,
           const std::filesystem::path &, std::string_view, bool,
           std::string &error) {
  error = "native minidumps are not implemented on this platform";
  return false;
}

bool WriteLiveMiniDump(const std::filesystem::path &, std::chrono::milliseconds,
                       std::string &error) {
  error = "native minidumps are not implemented on this platform";
  return false;
}

void RefreshCrashHandlers() {}

void Stop() {}

[[noreturn]] void TriggerCrashProbe() { std::abort(); }

} // namespace mcla::diagnostics::native
