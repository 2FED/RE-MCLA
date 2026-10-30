#include "mcla_diagnostic_image_client_win.h"
#include "mcla_host_platform_win.h"
#include <chrono>
#include <cstdio>
#include <cstring>
#include <string>
#include <thread>
#include <vector>
using namespace mcla::diagnostics::image;
namespace mcla::diagnostics::image {
CaptureResult RunImageHelperForTest(const Request &, const std::filesystem::path &, std::string_view, std::stop_token);
}
namespace {
// KI-049: unprivileged NTFS junction (documented mount-point reparse layout).
bool MakeJunction(const std::wstring &link, const std::wstring &target) {
  if (!CreateDirectoryW(link.c_str(), nullptr)) return false;
  const std::wstring substitute = L"\\??\\" + target;
  const auto sub_bytes = static_cast<WORD>(substitute.size() * sizeof(wchar_t));
  const auto print_bytes = static_cast<WORD>(target.size() * sizeof(wchar_t));
  const WORD data_length = static_cast<WORD>(4 * sizeof(WORD) + sub_bytes + print_bytes + 2 * sizeof(wchar_t));
  std::vector<uint8_t> buffer(8 + data_length, 0);
  const DWORD tag = IO_REPARSE_TAG_MOUNT_POINT;
  const WORD fields[] = {data_length, 0, 0, sub_bytes,
                         static_cast<WORD>(sub_bytes + sizeof(wchar_t)), print_bytes};
  std::memcpy(buffer.data(), &tag, sizeof(tag));
  std::memcpy(buffer.data() + 4, fields, sizeof(fields));
  std::memcpy(buffer.data() + 16, substitute.data(), sub_bytes);
  std::memcpy(buffer.data() + 16 + sub_bytes + sizeof(wchar_t), target.data(), print_bytes);
  const HANDLE handle = CreateFileW(link.c_str(), GENERIC_WRITE, 0, nullptr, OPEN_EXISTING,
      FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OPEN_REPARSE_POINT, nullptr);
  if (handle == INVALID_HANDLE_VALUE) return false;
  DWORD returned = 0;
  const bool ok = DeviceIoControl(handle, FSCTL_SET_REPARSE_POINT, buffer.data(),
      static_cast<DWORD>(buffer.size()), nullptr, 0, &returned, nullptr) != FALSE;
  CloseHandle(handle);
  return ok;
}
} // namespace

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
  // KI-049: helper reached through a junction ancestor. Real Windows refuses
  // it; only the forced Wine branch accepts the ancestor. The helper file
  // itself must never be a reparse point, Wine included.
  wchar_t temp[MAX_PATH + 1]{};
  const DWORD temp_length = GetTempPathW(MAX_PATH + 1, temp);
  const std::wstring fixture = std::wstring(temp, temp_length) + L"mcla-image-client-junction-" +
      std::to_wstring(GetCurrentProcessId()) + L"-" + std::to_wstring(GetTickCount64());
  const auto real = fixture + L"\\real";
  const auto link = fixture + L"\\link";
  const bool fixture_ready = temp_length && temp_length <= MAX_PATH &&
      CreateDirectoryW(fixture.c_str(), nullptr) && CreateDirectoryW(real.c_str(), nullptr) &&
      CopyFileW(helper.c_str(), (real + L"\\helper.exe").c_str(), TRUE) && MakeJunction(link, real);
  check(fixture_ready, "create owned junction fixture (preserved)");
  if (fixture_ready) {
    std::printf("Owned junction fixture (preserved): %ls\n", fixture.c_str());
    request.request_id = 1;
    const std::filesystem::path via = link + L"\\helper.exe";
    check(RunImageHelperForTest(request, via, expected, {}).failure == ClientFailure::kInvalidHelper,
          "Windows: junction ancestor of helper refused before launch");
    mcla::platform::SetWineOverrideForTesting(1);
    const auto accepted = RunImageHelperForTest(request, via, expected, {});
    check(accepted.captured && accepted.failure == ClientFailure::kNone && accepted.cleanup_complete,
          "forced Wine: helper through junction ancestor admitted");
    bool recorded = false;
    for (const auto &path : mcla::platform::WineAcceptedPaths()) recorded = recorded || path == link;
    check(recorded, "forced Wine: acceptance recorded");
    const auto helper_link = fixture + L"\\helper-link.exe";
    if (MakeJunction(helper_link, real)) {
      check(RunImageHelperForTest(request, helper_link, expected, {}).failure == ClientFailure::kInvalidHelper,
            "forced Wine: reparse helper path itself refused");
    } else {
      check(false, "create helper-path junction fixture");
    }
    mcla::platform::SetWineOverrideForTesting(-1);
    check(RunImageHelperForTest(request, via, expected, {}).failure == ClientFailure::kInvalidHelper,
          "restored detection: junction ancestor refused again");
  }
  std::printf("%d checks, %d failures (fake child only, no capture)\n", checks, failures);
  std::fflush(stdout);
  return failures ? 1 : 0;
}
