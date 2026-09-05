#include "mcla_frame_signal.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <limits>
#include <span>
#include <string_view>
#include <utility>

namespace {
using mcla::diagnostics::FrameRgbClass;
using mcla::diagnostics::FrameRgbClassName;
using mcla::diagnostics::FrameSignal;
using mcla::diagnostics::MeasureBgraFrame;

unsigned checks = 0;
unsigned failures = 0;

void Check(std::string_view label, FrameSignal actual, FrameRgbClass expected,
           uint32_t width = 0, uint32_t height = 0) {
  ++checks;
  if (actual.classification != expected || actual.width != width ||
      actual.height != height) {
    ++failures;
    std::cerr << "FAIL " << label << ": "
              << FrameRgbClassName(actual.classification) << ' ' << actual.width
              << 'x' << actual.height << '\n';
  }
}
} // namespace

int main() {
  const std::array<uint8_t, 16> black{};
  const std::array<uint8_t, 16> black_opaque{
      0, 0, 0, 255, 0, 0, 0, 255, 0, 0, 0, 255, 0, 0, 0, 255};
  const std::array<uint8_t, 16> black_mixed_alpha{
      0, 0, 0, 0, 0, 0, 0, 255, 0, 0, 0, 71, 0, 0, 0, 1};
  const std::array<uint8_t, 16> uniform{
      7, 12, 19, 0, 7, 12, 19, 255, 7, 12, 19, 71, 7, 12, 19, 1};
  auto nonuniform = uniform;
  nonuniform[14] = 20;

  Check("unavailable capture", {}, FrameRgbClass::kUnavailable);
  Check("empty buffer", MeasureBgraFrame({}, 2, 2, 8),
        FrameRgbClass::kUnavailable);
  Check("zero width", MeasureBgraFrame(black, 0, 2, 8),
        FrameRgbClass::kUnavailable);
  Check("zero height", MeasureBgraFrame(black, 2, 0, 8),
        FrameRgbClass::kUnavailable);
  Check("short stride", MeasureBgraFrame(black, 2, 2, 7),
        FrameRgbClass::kUnavailable);
  Check("zero stride", MeasureBgraFrame(black, 2, 2, 0),
        FrameRgbClass::kUnavailable);
  Check("truncated final pixel",
        MeasureBgraFrame(std::span(black).first(15), 2, 2, 8),
        FrameRgbClass::kUnavailable);
  Check("wide row overflow", MeasureBgraFrame(black, UINT32_MAX, 2, 8),
        FrameRgbClass::kUnavailable);
  Check("stride arithmetic overflow",
        MeasureBgraFrame(black, 2, 3, std::numeric_limits<size_t>::max()),
        FrameRgbClass::kUnavailable);
  Check("transparent black", MeasureBgraFrame(black, 2, 2, 8),
        FrameRgbClass::kAllBlack, 2, 2);
  Check("opaque black", MeasureBgraFrame(black_opaque, 2, 2, 8),
        FrameRgbClass::kAllBlack, 2, 2);
  Check("mixed alpha is ignored", MeasureBgraFrame(black_mixed_alpha, 2, 2, 8),
        FrameRgbClass::kAllBlack, 2, 2);
  Check("uniform nonblack", MeasureBgraFrame(uniform, 2, 2, 8),
        FrameRgbClass::kUniformNonblack, 2, 2);
  Check("one differing RGB component", MeasureBgraFrame(nonuniform, 2, 2, 8),
        FrameRgbClass::kNonuniform, 2, 2);
  Check("single pixel", MeasureBgraFrame(uniform, 1, 1, 4),
        FrameRgbClass::kUniformNonblack, 1, 1);
  auto early_difference = nonuniform;
  early_difference[4] = 99;
  Check("validate entire buffer before early difference",
        MeasureBgraFrame(std::span(early_difference).first(15), 2, 2, 8),
        FrameRgbClass::kUnavailable);

  // Nonzero padding and alpha must not turn an all-black image nonuniform.
  std::array<uint8_t, 20> padded{
      0, 0, 0, 255, 0, 0, 0, 1, 9, 8, 7, 6,
      0, 0, 0, 2, 0, 0, 0, 3};
  Check("ignore row padding and accept unpadded last row",
        MeasureBgraFrame(padded, 2, 2, 12), FrameRgbClass::kAllBlack, 2, 2);
  for (const size_t pixel : {size_t(0), size_t(4), size_t(12), size_t(16)}) {
    padded[pixel] = 31;
  }
  Check("padded uniform nonblack", MeasureBgraFrame(padded, 2, 2, 12),
        FrameRgbClass::kUniformNonblack, 2, 2);
  padded[18] = 1;
  Check("padded final row difference", MeasureBgraFrame(padded, 2, 2, 12),
        FrameRgbClass::kNonuniform, 2, 2);
  Check("padded final row truncated",
        MeasureBgraFrame(std::span(padded).first(19), 2, 2, 12),
        FrameRgbClass::kUnavailable);

  const std::array names = {
      std::pair{FrameRgbClass::kUnavailable, "unavailable"},
      std::pair{FrameRgbClass::kAllBlack, "all-black"},
      std::pair{FrameRgbClass::kUniformNonblack, "uniform-nonblack"},
      std::pair{FrameRgbClass::kNonuniform, "nonuniform"}};
  for (const auto &[value, name] : names) {
    ++checks;
    if (FrameRgbClassName(value) != name) {
      ++failures;
      std::cerr << "FAIL serialized class name " << name << '\n';
    }
  }
  std::cout << "Frame signal: " << checks << " checks, " << failures
            << " failures\n";
  return failures ? 1 : 0;
}
