#pragma once
// Standalone adapter tests have synthetic parents and no real phase wrapper.
// They must never acquire positive boundary provenance from those fake parents.
// Real phase/full-chain tests deliberately do not include this stub.
#include "../src/mcla_phase_census_win.h"
namespace mcla::phase_census {
BeforeWindowUpdate ReadActiveUpdateBeforeWindow(const PPCContext&, const uint8_t*,
    uint64_t, uint64_t, uint64_t, const std::string&) noexcept { return {}; }
}
