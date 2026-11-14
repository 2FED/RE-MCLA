// Private rd7 32bpp downscale candidate. Reuse the exact rd6 route/admission;
// the supervisor requires fault/upload/producer OFF and atlas metadata ON.
#define MCLA_DOWNSCALE_RENDER_PROBE
#include "mcla_fault_render_probe_win.cpp"
