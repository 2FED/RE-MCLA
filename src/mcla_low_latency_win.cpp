// l95 (M11): host side of the SDK low-latency layer. See mcla_low_latency.h.

#include "mcla_low_latency.h"

#include "mcla_logging.h"

#include <atomic>

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace {

using EnabledFn = int (*)();
using SimulationStartFn = void (*)();

struct Entry {
  EnabledFn enabled = nullptr;
  SimulationStartFn simulation_start = nullptr;
};

Entry Resolve() {
  Entry entry;
#if defined(_WIN32)
  HMODULE runtime = GetModuleHandleW(L"rexruntime.dll");
  if (runtime) {
    entry.enabled = reinterpret_cast<EnabledFn>(
        reinterpret_cast<void *>(GetProcAddress(runtime, "rex_low_latency_enabled")));
    entry.simulation_start = reinterpret_cast<SimulationStartFn>(
        reinterpret_cast<void *>(GetProcAddress(runtime, "rex_low_latency_simulation_start")));
    if (!entry.enabled || !entry.simulation_start) {
      entry = {};
    }
  }
#endif
  MCLA_APP_INFO("MCLA_LOWLAT host v=1 kind=entry sdk_entry={}",
                entry.simulation_start ? "found" : "absent");
  return entry;
}

}  // namespace

void MclaLowLatencySimulationStart() {
  static const Entry entry = Resolve();
  if (entry.simulation_start && entry.enabled()) {
    entry.simulation_start();
  }
}
