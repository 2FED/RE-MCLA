// One-shot diagnostic image IPC. This is NOT the M8 timing protocol.
#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <type_traits>

namespace mcla::diagnostics::image {

inline constexpr uint64_t kMagic = 0x31474D49414C434DULL; // MCLAIMG1
inline constexpr uint64_t kVersion = 1;
inline constexpr uint64_t kMaxDimension = 8192;
inline constexpr uint64_t kMaxPayloadBytes = 64ULL * 1024 * 1024;
inline constexpr uint64_t kRequestBytes = 144;
inline constexpr uint64_t kResultBytes = 288;

// Same-host, same-endian fixed-width layout in an unnamed inherited mapping.
// No pointers, handles owned by the recipient, strings, or producer clocks.
// owner_birth is the process creation FILETIME value, not an image timestamp.
struct Request {
  uint64_t magic = kMagic;
  uint64_t version = kVersion;
  uint64_t header_bytes = kRequestBytes;
  uint64_t payload_capacity = 0;
  uint64_t owner_pid = 0;
  uint64_t owner_birth = 0;
  uint64_t hwnd = 0;
  uint64_t owner_thread = 0;
  int64_t client_x = 0;
  int64_t client_y = 0;
  uint64_t client_width = 0;
  uint64_t client_height = 0;
  uint64_t dpi = 0;
  uint64_t request_id = 0;
  std::array<uint64_t, 4> reserved{};
  bool operator==(const Request &) const = default;
};

enum class Status : uint64_t { kPending = 0, kSuccess = 1, kFailure = 2 };
enum class Failure : uint64_t {
  kNone = 0,
  kOwnerChanged = 1,
  kUnsupported = 2,
  kCaptureFailed = 3,
  kInvalidImage = 4,
  kTimeout = 5,
  kInternal = 6,
};
enum class Layout : uint64_t { kNone = 0, kTopDownTightBgra8 = 1 };

struct Result {
  Request request{}; // Immutable copy of the entire request, including reserved.
  uint64_t header_bytes = kResultBytes;
  Status status = Status::kPending;
  Failure failure = Failure::kNone;
  // Physical screen origin of the WINDOW capture surface, never the desktop.
  int64_t content_x = 0;
  int64_t content_y = 0;
  uint64_t content_width = 0;
  uint64_t content_height = 0;
  uint64_t source_row_pitch = 0;
  uint64_t source_buffer_bytes = 0;
  uint64_t crop_x = 0;
  uint64_t crop_y = 0;
  Layout layout = Layout::kNone;
  uint64_t output_row_pitch = 0;
  uint64_t payload_bytes = 0;
  std::array<uint64_t, 4> reserved{};
};

static_assert(sizeof(Request) == kRequestBytes);
static_assert(sizeof(Result) == kResultBytes);
static_assert(offsetof(Result, request) == 0);
static_assert(offsetof(Result, header_bytes) == kRequestBytes);
static_assert(std::is_standard_layout_v<Request> &&
              std::is_trivially_copyable_v<Request>);
static_assert(std::is_standard_layout_v<Result> &&
              std::is_trivially_copyable_v<Result>);

// The payload begins immediately after Result and has exactly the request's
// capacity. Mapping size is sizeof(Result) + capacity, always checked beforehand.
inline constexpr bool CheckedMultiply(uint64_t a, uint64_t b, uint64_t &out) {
  if (b != 0 && a > std::numeric_limits<uint64_t>::max() / b) {
    return false;
  }
  out = a * b;
  return true;
}

inline constexpr bool ImageShape(uint64_t width, uint64_t height,
                                 uint64_t &row_bytes, uint64_t &bytes) {
  return width > 0 && width <= kMaxDimension && height > 0 &&
         height <= kMaxDimension && CheckedMultiply(width, 4, row_bytes) &&
         CheckedMultiply(row_bytes, height, bytes) && bytes <= kMaxPayloadBytes;
}

inline constexpr bool ZeroReserved(const std::array<uint64_t, 4> &reserved) {
  return reserved[0] == 0 && reserved[1] == 0 && reserved[2] == 0 &&
         reserved[3] == 0;
}

// Win32 physical RECT coordinates are signed 32-bit. Bound both endpoints so
// later subtraction/addition stays defined even for hostile shared memory.
inline constexpr bool PhysicalAxis(int64_t origin, uint64_t extent) {
  constexpr int64_t low = std::numeric_limits<int32_t>::min();
  constexpr int64_t high = std::numeric_limits<int32_t>::max();
  return origin >= low && origin <= high && extent <= kMaxDimension &&
         extent <= static_cast<uint64_t>(high - origin);
}

inline constexpr bool ValidateRequest(const Request &request) {
  uint64_t row_bytes = 0;
  uint64_t bytes = 0;
  return request.magic == kMagic && request.version == kVersion &&
         request.header_bytes == kRequestBytes &&
         request.owner_pid > 0 && request.owner_pid <= UINT32_MAX &&
         request.owner_birth != 0 && request.hwnd != 0 &&
         request.owner_thread > 0 && request.owner_thread <= UINT32_MAX &&
         request.request_id != 0 && request.dpi > 0 && request.dpi <= UINT32_MAX &&
         ZeroReserved(request.reserved) &&
         ImageShape(request.client_width, request.client_height, row_bytes, bytes) &&
         PhysicalAxis(request.client_x, request.client_width) &&
         PhysicalAxis(request.client_y, request.client_height) &&
         request.payload_capacity == bytes;
}

inline constexpr bool KnownFailure(Failure failure) {
  switch (failure) {
  case Failure::kOwnerChanged:
  case Failure::kUnsupported:
  case Failure::kCaptureFailed:
  case Failure::kInvalidImage:
  case Failure::kTimeout:
  case Failure::kInternal:
    return true;
  default:
    return false;
  }
}

// Pure structural validation ONLY. Caller must first observe normal helper exit
// and bounded Job drain, then copy the stable header into local memory. Never
// admit bytes while a mapping writer may still exist. An abnormal/timeout exit
// rejects even a success-looking header. The caller/helper must independently
// verify the live PID/birth/HWND/thread and physical geometry/DPI around capture.
// These tests cannot prove window ownership, visibility, freshness, or cleanup.
// A valid failure is well formed but contains NO usable image; test status too.
// A success is an image-only observation: no exact timestamp, FPS, frame identity,
// presentation ordering, or producer/observer clock-bracket claim is made.
inline constexpr bool ValidateResult(const Request &expected, const Result &result,
                                     uint64_t available_payload_bytes) {
  if (!ValidateRequest(expected) || result.request != expected ||
      result.header_bytes != kResultBytes || !ZeroReserved(result.reserved) ||
      available_payload_bytes != expected.payload_capacity) {
    return false;
  }
  if (result.status == Status::kFailure) {
    return KnownFailure(result.failure) && result.content_x == 0 &&
           result.content_y == 0 && result.content_width == 0 &&
           result.content_height == 0 && result.source_row_pitch == 0 &&
           result.source_buffer_bytes == 0 && result.crop_x == 0 &&
           result.crop_y == 0 && result.layout == Layout::kNone &&
           result.output_row_pitch == 0 && result.payload_bytes == 0;
  }
  if (result.status != Status::kSuccess || result.failure != Failure::kNone ||
      result.layout != Layout::kTopDownTightBgra8) {
    return false;
  }
  uint64_t content_row_bytes = 0;
  uint64_t content_bytes = 0;
  uint64_t source_bytes = 0;
  uint64_t output_row_bytes = 0;
  uint64_t output_bytes = 0;
  if (!ImageShape(result.content_width, result.content_height,
                  content_row_bytes, content_bytes) ||
      !PhysicalAxis(result.content_x, result.content_width) ||
      !PhysicalAxis(result.content_y, result.content_height) ||
      result.source_row_pitch < content_row_bytes ||
      result.source_row_pitch % 4 != 0 ||
      !CheckedMultiply(result.source_row_pitch, result.content_height,
                       source_bytes) ||
      source_bytes > kMaxPayloadBytes ||
      result.source_buffer_bytes != source_bytes ||
      !ImageShape(expected.client_width, expected.client_height,
                  output_row_bytes, output_bytes)) {
    return false;
  }
  // Subtract rather than add untrusted crop values to avoid unsigned overflow.
  if (result.crop_x > result.content_width ||
      result.crop_y > result.content_height ||
      expected.client_width > result.content_width - result.crop_x ||
      expected.client_height > result.content_height - result.crop_y ||
      expected.client_x < result.content_x ||
      expected.client_y < result.content_y) {
    return false;
  }
  return result.crop_x == static_cast<uint64_t>(expected.client_x - result.content_x) &&
         result.crop_y == static_cast<uint64_t>(expected.client_y - result.content_y) &&
         result.output_row_pitch == output_row_bytes &&
         result.payload_bytes == output_bytes;
}

// Bind the structurally valid result to independently observed surface bounds.
// A shifted surface plus compensating crop can be self-consistent but wrong.
inline constexpr bool MatchesSurface(const Result &result, int64_t x, int64_t y,
                                     int64_t width, int64_t height) {
  return width > 0 && height > 0 && result.content_x == x && result.content_y == y &&
         result.content_width == static_cast<uint64_t>(width) &&
         result.content_height == static_cast<uint64_t>(height);
}

} // namespace mcla::diagnostics::image
