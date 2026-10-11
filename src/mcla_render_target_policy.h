#pragma once

#include <array>
#include <cstdint>

namespace mcla::launcher {

struct RenderTargetPlan {
  uint32_t id, width, height, guest_width, guest_height, scale;
};

// Requested guest-output target, not window/client dimensions. These bounded
// integer-scale tuples have startup-output evidence on the pinned Windows host;
// this table alone grants neither host capability nor gameplay acceptance.
inline constexpr std::array<RenderTargetPlan, 4> kRenderTargets{{
    {720, 1280, 720, 1280, 720, 1},
    {1080, 1920, 1080, 960, 540, 2},
    {1440, 2560, 1440, 1280, 720, 2},
    {2160, 3840, 2160, 1280, 720, 3},
}};

inline constexpr const RenderTargetPlan *FindRenderTarget(uint32_t id) {
  for (const auto &target : kRenderTargets)
    if (target.id == id) return &target;
  return nullptr;
}

} // namespace mcla::launcher
