// Separate exact identity for the shader-only SDK with observer features absent.
// Compose only the plain RenderDoc helper: no atlas/upload/producer/fault flags.
// Build admission remains closed until the independently reviewed stage is pinned.
#define MCLA_CLEAN_DOWNSCALE_RENDER_PROBE
#include "mcla_renderdoc_render_probe_win.cpp"
