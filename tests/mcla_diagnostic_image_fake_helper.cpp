// Nonshipping adversarial child: no windows, capture, game, files or input.
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include "mcla_diagnostic_image_protocol.h"
#include <cstdlib>
#include <cstring>
using namespace mcla::diagnostics::image;
int wmain(int argc, wchar_t **argv) {
  if (argc != 3) return 80;
  const auto handle = reinterpret_cast<HANDLE>(std::wcstoull(argv[1], nullptr, 10));
  const auto bytes = std::wcstoull(argv[2], nullptr, 10);
  if (bytes != sizeof(Result) + 64) return 81;
  auto *view = static_cast<std::byte *>(MapViewOfFile(handle, FILE_MAP_ALL_ACCESS, 0, 0, bytes));
  if (!view) return 82;
  Result result;
  std::memcpy(&result, view, sizeof(result));
  if (!ValidateRequest(result.request)) return 83;
  const auto mode = result.request.request_id;
  if (mode == 2) return 0; // Exit zero, but never completed the result.
  if (mode == 6) Sleep(INFINITE);
  result.status = Status::kSuccess;
  result.content_x = result.request.client_x;
  result.content_y = result.request.client_y;
  result.content_width = 4;
  result.content_height = 4;
  result.source_row_pitch = 16;
  result.source_buffer_bytes = 64;
  result.layout = Layout::kTopDownTightBgra8;
  result.output_row_pitch = 16;
  result.payload_bytes = 64;
  if (mode == 3) ++result.header_bytes;
  if (mode == 8) ++result.request.owner_birth;
  if (mode == 9 || mode == 12) {
    const auto request = result.request;
    result = Result{}; result.request = request;
    result.status = Status::kFailure; result.failure = Failure::kCaptureFailed;
  }
  if (mode == 10) ++result.payload_bytes;
  if (mode == 11) {
    --result.content_x; ++result.crop_x; ++result.content_width;
    result.source_row_pitch = 20; result.source_buffer_bytes = 80;
  }
  std::memcpy(view, &result, sizeof(result));
  std::memset(view + sizeof(result), 0x5a, 64);
  if (mode == 5) Sleep(INFINITE); // Success-looking bytes from a live writer.
  UnmapViewOfFile(view);
  CloseHandle(handle);
  return mode == 4 ? 7 : mode == 12 ? 1 : 0;
}
