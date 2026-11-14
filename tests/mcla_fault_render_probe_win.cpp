// Separate natural-write-fault diagnostic. Producer and upload observation stay
// OFF; closed-log and breadcrumb review are external acceptance gates.
#define MCLA_FAULT_RENDER_PROBE
#include "mcla_producer_render_probe_win.cpp"
