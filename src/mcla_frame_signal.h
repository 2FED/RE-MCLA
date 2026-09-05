// Pure RGB-content classification for diagnostic frames, not gameplay health.

#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>

namespace mcla::diagnostics {

enum class FrameRgbClass : uint8_t {
  kUnavailable,
  kAllBlack,
  kUniformNonblack,
  kNonuniform,
};

inline std::string_view FrameRgbClassName(FrameRgbClass value) {
  switch (value) {
  case FrameRgbClass::kAllBlack:
    return "all-black";
  case FrameRgbClass::kUniformNonblack:
    return "uniform-nonblack";
  case FrameRgbClass::kNonuniform:
    return "nonuniform";
  default:
    return "unavailable";
  }
}

struct FrameSignal {
  FrameRgbClass classification = FrameRgbClass::kUnavailable;
  uint32_t width = 0;
  uint32_t height = 0;
};

// Inspect every BGR pixel until a difference is found. Ignore alpha/X and row
// padding: a GDI BI_RGB bitmap does not promise meaningful alpha. Validate the
// complete view before any access, including when the last row lacks padding.
// Nonuniform only describes these bytes; it proves neither freshness nor a
// healthy renderer, visible presentation, or gameplay.
inline FrameSignal MeasureBgraFrame(std::span<const uint8_t> pixels,
                                    uint32_t width, uint32_t height,
                                    size_t stride) {
  const uint64_t row_bytes_wide = uint64_t(width) * 4;
  if (!width || !height || row_bytes_wide > pixels.size() ||
      row_bytes_wide > stride) {
    return {};
  }
  const size_t row_bytes = static_cast<size_t>(row_bytes_wide);
  // Division avoids overflow in stride * (height - 1) + row_bytes.
  if (uint64_t(height - 1) > (pixels.size() - row_bytes) / stride) {
    return {};
  }

  const uint8_t blue = pixels[0];
  const uint8_t green = pixels[1];
  const uint8_t red = pixels[2];
  for (uint32_t y = 0; y < height; ++y) {
    const size_t row_start = size_t(y) * stride;
    for (size_t x = 0; x < row_bytes; x += 4) {
      const size_t pixel = row_start + x;
      if (pixels[pixel] != blue || pixels[pixel + 1] != green ||
          pixels[pixel + 2] != red) {
        return {FrameRgbClass::kNonuniform, width, height};
      }
    }
  }
  return {blue || green || red ? FrameRgbClass::kUniformNonblack
                              : FrameRgbClass::kAllBlack,
          width, height};
}

} // namespace mcla::diagnostics
