#pragma once

#include "mcla_diagnostic_image_protocol.h"
#include <cstddef>
#include <filesystem>
#include <stop_token>
#include <string_view>
#include <vector>

namespace mcla::diagnostics::image {

enum class ClientFailure {
  kNone, kInvalidOwner, kInvalidHelper, kSetup, kCancelled, kTimeout,
  kHelperExit, kCleanup, kProtocol,
};
struct CaptureResult {
  bool captured = false;
  bool cleanup_complete = true;
  ClientFailure failure = ClientFailure::kSetup;
  uint32_t helper_exit = 0;
  Result metadata{};
  std::vector<std::byte> pixels;
};

// Captures only a visible foreground HWND owned by THIS process. No game launch,
// desktop fallback, activation, resizing, persistence or exact timing claim.
// helper must be a reviewed, absolute local EXE. Its required SHA256 is checked
// using the held deny-write/delete executable handle before child creation.
// A dedicated kill-on-close Job contains only the helper. Parent deadline is
// four seconds from suspended creation/assignment plus <=500ms termination wait
// or normal-exit Job-drain grace;
// failed cleanup never admits pixels. Process creation itself is a Win32 call.
// Ordinary Win32 calls are not a hard real-time guarantee against an OS failure.
CaptureResult CaptureOwnedWindow(uint64_t hwnd, const std::filesystem::path &helper,
                                 std::string_view expected_sha256,
                                 std::stop_token stop = {});

} // namespace mcla::diagnostics::image
