#pragma once

// KI-026 race-transition camera softlock: an always-on read-only F10 state
// block, default-off transition logging (--mcla_ki026_probe) and a
// default-off watchdog (--mcla_ki026_watchdog). See
// docs/evidence/M9-006-ki-026-camera-analysis-20261029.md.
//
// KI-035 adds the warp camera (warper+296) to the block, default-off warp
// state logging (--mcla_warp_probe) and a default-off state-20 watchdog
// (--mcla_warp_watchdog_seconds); see mcla_warp_probe.h and
// docs/evidence/M10-ki035-warp-probe-20261104.md.

#include <cstdint>
#include <string>

namespace rex::memory {
class Memory;
}

// Returns a JSON object describing the guest warp/camera-pause/pause-manager
// state, or an empty string when the guest is not available. Every guest
// pointer is validated against the guest heaps before it is dereferenced;
// "complete": false means at least one read was refused.
std::string MclaKi026StateJson(rex::memory::Memory *memory, uint8_t *base);
