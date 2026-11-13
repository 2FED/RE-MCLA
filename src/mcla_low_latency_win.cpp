// l95 (M11): host side of the SDK low-latency layer. See mcla_low_latency.h.

#include "mcla_low_latency.h"

#include "mcla_hitch_host.h"
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

namespace {

// l99: the SDK hitch / video-memory entry points.
using HitchSequenceFn = uint64_t (*)();
using HitchLastFn = int (*)(uint64_t *, uint64_t *, uint64_t *, uint64_t *);
using VramSampleFn = void (*)(uint64_t, uint64_t, uint64_t);

struct HitchEntry {
  HitchSequenceFn sequence = nullptr;
  HitchLastFn last = nullptr;
  VramSampleFn vram = nullptr;
};

HitchEntry ResolveHitch() {
  HitchEntry entry;
#if defined(_WIN32)
  HMODULE runtime = GetModuleHandleW(L"rexruntime.dll");
  if (runtime) {
    entry.sequence = reinterpret_cast<HitchSequenceFn>(
        reinterpret_cast<void *>(GetProcAddress(runtime, "rex_hitch_logged_sequence")));
    entry.last = reinterpret_cast<HitchLastFn>(
        reinterpret_cast<void *>(GetProcAddress(runtime, "rex_hitch_last")));
    entry.vram = reinterpret_cast<VramSampleFn>(
        reinterpret_cast<void *>(GetProcAddress(runtime, "rex_vram_host_sample")));
    if (!entry.sequence || !entry.last || !entry.vram) {
      entry = {};
    }
  }
#endif
  MCLA_APP_INFO("{}", mcla::hitch::EntryLine(entry.sequence != nullptr));
  return entry;
}

const HitchEntry &Hitch() {
  static const HitchEntry entry = ResolveHitch();
  return entry;
}

}  // namespace

uint64_t MclaSdkHitchLoggedSequence() {
  const HitchEntry &entry = Hitch();
  return entry.sequence ? entry.sequence() : 0;
}

bool MclaSdkHitchLast(uint64_t &seq, uint64_t &logged_seq, uint64_t &frame,
                      uint64_t &interval_us) {
  const HitchEntry &entry = Hitch();
  return entry.last && entry.last(&seq, &logged_seq, &frame, &interval_us) != 0;
}

void MclaSdkVramHostSample(uint64_t budget_mb, uint64_t usage_mb, uint64_t nonlocal_mb) {
  const HitchEntry &entry = Hitch();
  if (entry.vram) {
    entry.vram(budget_mb, usage_mb, nonlocal_mb);
  }
}
