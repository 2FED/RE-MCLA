#include "mcla_diagnostic_image_protocol.h"

#include <iostream>
#include <limits>
#include <string_view>

namespace {
using namespace mcla::diagnostics::image;
unsigned checks = 0;
unsigned failures = 0;

void Check(std::string_view label, bool actual, bool expected) {
  ++checks;
  if (actual != expected) {
    ++failures;
    std::cerr << "FAIL " << label << '\n';
  }
}

Request GoodRequest() {
  Request request;
  request.owner_pid = 100;
  request.owner_birth = 123456789;
  request.hwnd = 0x10203;
  request.owner_thread = 104;
  request.client_x = -1920;
  request.client_y = 120;
  request.client_width = 1280;
  request.client_height = 720;
  request.dpi = 144;
  request.request_id = 1;
  request.payload_capacity = 1280 * 720 * 4;
  return request;
}

Result GoodResult(const Request &request) {
  Result result;
  result.request = request;
  result.status = Status::kSuccess;
  result.content_x = request.client_x - 8;
  result.content_y = request.client_y - 32;
  result.content_width = request.client_width + 16;
  result.content_height = request.client_height + 40;
  result.source_row_pitch = (result.content_width * 4 + 255) & ~uint64_t(255);
  result.source_buffer_bytes = result.source_row_pitch * result.content_height;
  result.crop_x = 8;
  result.crop_y = 32;
  result.layout = Layout::kTopDownTightBgra8;
  result.output_row_pitch = request.client_width * 4;
  result.payload_bytes = request.payload_capacity;
  return result;
}
} // namespace

int main() {
  const Request request = GoodRequest();
  const Result result = GoodResult(request);
  Check("request", ValidateRequest(request), true);
  Check("negative desktop origin and padded source",
        ValidateResult(request, result, request.payload_capacity), true);
  Check("zero initialized request", ValidateRequest({}), false);
  Check("pending result", ValidateResult(request, {}, request.payload_capacity), false);

  const auto bad_request = [&](std::string_view label, auto mutate) {
    auto changed = request;
    mutate(changed);
    Check(label, ValidateRequest(changed), false);
  };
  bad_request("magic", [](auto &x) { ++x.magic; });
  bad_request("version", [](auto &x) { ++x.version; });
  bad_request("request size", [](auto &x) { --x.header_bytes; });
  bad_request("zero PID", [](auto &x) { x.owner_pid = 0; });
  bad_request("oversized PID", [](auto &x) { x.owner_pid = uint64_t(UINT32_MAX) + 1; });
  bad_request("zero birth", [](auto &x) { x.owner_birth = 0; });
  bad_request("zero HWND", [](auto &x) { x.hwnd = 0; });
  bad_request("zero thread", [](auto &x) { x.owner_thread = 0; });
  bad_request("oversized thread", [](auto &x) { x.owner_thread = uint64_t(UINT32_MAX) + 1; });
  bad_request("zero request id", [](auto &x) { x.request_id = 0; });
  bad_request("zero DPI", [](auto &x) { x.dpi = 0; });
  bad_request("oversized DPI", [](auto &x) { x.dpi = UINT64_MAX; });
  bad_request("zero width", [](auto &x) { x.client_width = 0; });
  bad_request("zero height", [](auto &x) { x.client_height = 0; });
  bad_request("large width", [](auto &x) { x.client_width = 8193; });
  bad_request("large height", [](auto &x) { x.client_height = UINT64_MAX; });
  bad_request("over 64 MiB", [](auto &x) { x.client_width = x.client_height = 8192; });
  bad_request("short capacity", [](auto &x) { --x.payload_capacity; });
  bad_request("long capacity", [](auto &x) { ++x.payload_capacity; });
  bad_request("huge capacity", [](auto &x) { x.payload_capacity = UINT64_MAX; });
  bad_request("origin underflow", [](auto &x) { x.client_x = INT64_MIN; });
  bad_request("origin overflow", [](auto &x) { x.client_y = INT64_MAX; });
  bad_request("right endpoint overflow", [](auto &x) { x.client_x = INT32_MAX; });
  bad_request("bottom endpoint overflow", [](auto &x) { x.client_y = INT32_MAX; });
  for (size_t i = 0; i < request.reserved.size(); ++i) {
    bad_request("each request reserved word", [i](auto &x) { x.reserved[i] = 1; });
  }

  const auto bad_result = [&](std::string_view label, auto mutate) {
    auto changed = result;
    mutate(changed);
    Check(label, ValidateResult(request, changed, request.payload_capacity), false);
  };
  bad_result("result size", [](auto &x) { --x.header_bytes; });
  bad_result("partial pending", [](auto &x) { x.status = Status::kPending; });
  bad_result("unknown status", [](auto &x) { x.status = static_cast<Status>(99); });
  bad_result("success with failure", [](auto &x) { x.failure = Failure::kTimeout; });
  bad_result("success unknown failure", [](auto &x) { x.failure = static_cast<Failure>(99); });
  bad_result("absent layout", [](auto &x) { x.layout = Layout::kNone; });
  bad_result("unknown layout", [](auto &x) { x.layout = static_cast<Layout>(99); });
  bad_result("zero surface width", [](auto &x) { x.content_width = 0; });
  bad_result("zero surface height", [](auto &x) { x.content_height = 0; });
  bad_result("large surface", [](auto &x) { x.content_width = 8193; });
  bad_result("surface origin overflow", [](auto &x) { x.content_x = INT64_MAX; });
  bad_result("surface origin underflow", [](auto &x) { x.content_y = INT64_MIN; });
  bad_result("source short stride", [](auto &x) { x.source_row_pitch = 1; });
  bad_result("source unaligned stride", [](auto &x) {
    ++x.source_row_pitch;
    x.source_buffer_bytes = x.source_row_pitch * x.content_height;
  });
  bad_result("source multiplication overflow", [](auto &x) { x.source_row_pitch = UINT64_MAX; });
  bad_result("source allocation bound", [](auto &x) {
    x.source_row_pitch = kMaxPayloadBytes;
    x.source_buffer_bytes = x.source_row_pitch * x.content_height;
  });
  bad_result("source short buffer", [](auto &x) { --x.source_buffer_bytes; });
  bad_result("source long buffer", [](auto &x) { ++x.source_buffer_bytes; });
  bad_result("crop addition overflow", [](auto &x) { x.crop_x = UINT64_MAX; });
  bad_result("crop y overflow", [](auto &x) { x.crop_y = UINT64_MAX; });
  bad_result("crop too narrow", [](auto &x) { x.crop_x = x.content_width; });
  bad_result("crop too short", [](auto &x) { x.crop_y = x.content_height; });
  bad_result("crop x drift", [](auto &x) { ++x.crop_x; });
  bad_result("crop y drift", [](auto &x) { ++x.crop_y; });
  bad_result("capture origin x drift", [](auto &x) { --x.content_x; });
  bad_result("capture origin y drift", [](auto &x) { --x.content_y; });
  bad_result("client left of surface", [&](auto &x) { x.content_x = request.client_x + 1; });
  bad_result("client above surface", [&](auto &x) { x.content_y = request.client_y + 1; });
  bad_result("padded output prohibited", [](auto &x) { ++x.output_row_pitch; });
  bad_result("short output stride", [](auto &x) { --x.output_row_pitch; });
  bad_result("short payload", [](auto &x) { --x.payload_bytes; });
  bad_result("oversized payload", [](auto &x) { x.payload_bytes = UINT64_MAX; });
  for (size_t i = 0; i < result.reserved.size(); ++i) {
    bad_result("each result reserved word", [i](auto &x) { x.reserved[i] = 1; });
    bad_result("each echoed reserved word", [i](auto &x) { x.request.reserved[i] = 1; });
  }
  // Every scalar in the echoed request is immutable, not merely PID/HWND.
  for (const auto member : {&Request::magic, &Request::version, &Request::header_bytes,
                           &Request::payload_capacity, &Request::owner_pid,
                           &Request::owner_birth, &Request::hwnd, &Request::owner_thread,
                           &Request::client_width, &Request::client_height,
                           &Request::dpi, &Request::request_id}) {
    bad_result("identity drift", [member](auto &x) { ++(x.request.*member); });
  }
  bad_result("client x identity drift", [](auto &x) { ++x.request.client_x; });
  bad_result("client y identity drift", [](auto &x) { ++x.request.client_y; });
  Check("short available bytes", ValidateResult(request, result, request.payload_capacity - 1), false);
  Check("long available bytes", ValidateResult(request, result, request.payload_capacity + 1), false);

  Result failed;
  failed.request = request;
  failed.status = Status::kFailure;
  for (const Failure reason : {Failure::kOwnerChanged, Failure::kUnsupported,
                               Failure::kCaptureFailed, Failure::kInvalidImage,
                               Failure::kTimeout, Failure::kInternal}) {
    failed.failure = reason;
    Check("well formed failure", ValidateResult(request, failed, request.payload_capacity), true);
  }
  failed.failure = Failure::kNone;
  Check("failure needs reason", ValidateResult(request, failed, request.payload_capacity), false);
  failed.failure = static_cast<Failure>(99);
  Check("unknown failure", ValidateResult(request, failed, request.payload_capacity), false);
  failed.failure = Failure::kCaptureFailed;
  for (const auto member : {&Result::content_width, &Result::content_height,
                           &Result::source_row_pitch, &Result::source_buffer_bytes,
                           &Result::crop_x, &Result::crop_y, &Result::output_row_pitch,
                           &Result::payload_bytes}) {
    auto changed = failed;
    changed.*member = 1;
    Check("failure rejects partial metadata", ValidateResult(request, changed, request.payload_capacity), false);
  }
  for (const auto member : {&Result::content_x, &Result::content_y}) {
    auto changed = failed;
    changed.*member = -1;
    Check("failure rejects origins", ValidateResult(request, changed, request.payload_capacity), false);
  }
  failed.layout = Layout::kTopDownTightBgra8;
  Check("failure rejects layout", ValidateResult(request, failed, request.payload_capacity), false);

  for (const uint64_t side : {uint64_t(1), uint64_t(4096)}) {
    auto edge = request;
    edge.client_width = edge.client_height = side;
    edge.payload_capacity = side * side * 4;
    auto image = GoodResult(edge);
    image.content_x = edge.client_x;
    image.content_y = edge.client_y;
    image.content_width = image.content_height = side;
    image.crop_x = image.crop_y = 0;
    image.source_row_pitch = image.output_row_pitch;
    image.source_buffer_bytes = image.payload_bytes;
    Check("one pixel and exact 64 MiB request", ValidateRequest(edge), true);
    Check("one pixel and exact 64 MiB result", ValidateResult(edge, image, edge.payload_capacity), true);
  }
  Check("bind observed surface", MatchesSurface(result, result.content_x, result.content_y,
         result.content_width, result.content_height), true);
  auto shifted = result;
  --shifted.content_x; ++shifted.crop_x; ++shifted.content_width;
  shifted.source_row_pitch += 4;
  shifted.source_buffer_bytes = shifted.source_row_pitch * shifted.content_height;
  Check("coordinated shift remains structurally valid",
        ValidateResult(request, shifted, request.payload_capacity), true);
  Check("external bounds reject coordinated shift", MatchesSurface(shifted,
        result.content_x, result.content_y, result.content_width, result.content_height), false);
  Check("negative external extent", MatchesSurface(result, result.content_x,
        result.content_y, -1, result.content_height), false);
  uint64_t product = 777;
  Check("checked multiply overflow", CheckedMultiply(UINT64_MAX, 2, product), false);
  Check("overflow leaves destination unchanged", product == 777, true);
  Check("checked multiply zero", CheckedMultiply(UINT64_MAX, 0, product), true);
  Check("zero product", product == 0, true);

  std::cout << checks << " image protocol checks; " << failures << " failures\n";
  return failures == 0 ? 0 : 1;
}
