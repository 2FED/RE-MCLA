#pragma once

// KI-026 race-transition camera softlock: an always-on read-only F10 state
// block, default-off transition logging (--mcla_ki026_probe) and a
// default-off watchdog (--mcla_ki026_watchdog). See
// docs/evidence/M9-006-ki-026-camera-analysis-20261029.md.

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
