#include "mcla_diagnostic_image_client_win.h"
#include <chrono>
#include <cstdio>
#include <thread>
using namespace mcla::diagnostics::image;
namespace mcla::diagnostics::image {
CaptureResult RunImageHelperForTest(const Request &, const std::filesystem::path &, std::string_view, std::stop_token);
}
int main(int argc, char **argv) {
  if (argc != 3) return 2;
  const auto helper = std::filesystem::absolute(argv[1]);
  const std::string_view expected = argv[2];
  int checks = 0, failures = 0;
  auto check = [&](bool ok, const char *label) {
    ++checks;
    if (!ok) { ++failures; std::fprintf(stderr, "FAIL %s\n", label); }
  };
  Request request;
  request.owner_pid = 1; request.owner_thread = 1; request.owner_birth = 1;
  request.hwnd = 1; request.client_x = 10; request.client_y = 20;
  request.client_width = 4; request.client_height = 4;
  request.payload_capacity = 64; request.dpi = 96;
  for (uint64_t mode : {1, 2, 3, 4, 5, 6, 8, 9, 10, 11, 12}) {
    request.request_id = mode;
    std::stop_source stop;
    std::jthread cancel;
    if (mode == 6) cancel = std::jthread([&] {
      std::this_thread::sleep_for(std::chrono::milliseconds(100)); stop.request_stop();
    });
    const auto begin = std::chrono::steady_clock::now();
    const auto result = RunImageHelperForTest(request, helper, expected, stop.get_token());
    const auto elapsed = std::chrono::steady_clock::now() - begin;
    std::printf("mode %llu: captured=%d cleanup=%d failure=%d exit=%u\n",
                static_cast<unsigned long long>(mode), result.captured,
                result.cleanup_complete, static_cast<int>(result.failure), result.helper_exit);
    check(result.cleanup_complete, "owned helper drained");
    check(elapsed < std::chrono::seconds(6), "bounded observed return");
    if (mode == 1) {
      check(result.captured && result.failure == ClientFailure::kNone, "fake success admitted");
      check(result.pixels.size() == 64 && result.pixels.front() == std::byte{0x5a}, "exact fake payload");
    } else {
      check(!result.captured && result.pixels.empty(), "rejected pixels never returned");
      const auto expected = mode == 4 || mode == 12 ? ClientFailure::kHelperExit :
          mode == 5 ? ClientFailure::kTimeout : mode == 6 ? ClientFailure::kCancelled :
          mode == 11 ? ClientFailure::kInvalidOwner : ClientFailure::kProtocol;
      check(result.failure == expected, "exact rejection reason");
      if (mode == 12) check(result.metadata.status == Status::kFailure &&
          result.metadata.failure == Failure::kCaptureFailed, "validated failure header only");
      if (mode == 4) check(result.metadata.status == Status::kPending,
          "abnormal success-looking header not retained");
    }
  }
  std::stop_source stopped; stopped.request_stop();
  check(RunImageHelperForTest(request, helper, expected, stopped.get_token()).failure == ClientFailure::kCancelled,
        "pre-cancel never launches");
  check(CaptureOwnedWindow(0, helper, expected).failure == ClientFailure::kInvalidOwner, "null HWND rejected");
  check(RunImageHelperForTest(request, "relative.exe", expected, {}).failure == ClientFailure::kInvalidHelper,
        "relative executable rejected");
  check(RunImageHelperForTest(request, helper, std::string(64, '0'), {}).failure == ClientFailure::kInvalidHelper,
        "wrong executable hash rejected before launch");
  check(RunImageHelperForTest(request, helper, "", {}).failure == ClientFailure::kInvalidHelper,
        "missing executable hash rejected before launch");
  std::printf("%d checks, %d failures (fake child only, no capture)\n", checks, failures);
  std::fflush(stdout);
  return failures ? 1 : 0;
}
