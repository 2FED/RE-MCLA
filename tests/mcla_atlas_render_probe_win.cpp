// Separate diagnostic identity and one opt-in atlas metadata flag only.
// Closed-log validation and actual draw/resource-name linkage remain external
// gates; this helper's process/capture success does not assert either.
#define MCLA_ATLAS_RENDER_PROBE
#include "mcla_renderdoc_render_probe_win.cpp"
