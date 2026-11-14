#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <winsock2.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <iphlpapi.h>
#include <bcrypt.h>

#include "mcla_renderdoc_probe_win.h"

#include <array>
#include <iostream>
#include <string>
#include <vector>

namespace probe = mcla::renderdoc_probe;
namespace policy = probe::policy;
namespace {
int checks = 0;
int failures = 0;
bool NoOwnedListeners(ULONG family) {
  ULONG bytes = 0;
  DWORD result = GetExtendedTcpTable(nullptr, &bytes, FALSE, family, TCP_TABLE_OWNER_PID_LISTENER, 0);
  if (result != ERROR_INSUFFICIENT_BUFFER || bytes == 0 || bytes > 16 * 1024 * 1024) return false;
  std::vector<unsigned char> buffer(bytes);
  result = GetExtendedTcpTable(buffer.data(), &bytes, FALSE, family, TCP_TABLE_OWNER_PID_LISTENER, 0);
  if (result != NO_ERROR) return false;
  const auto pid = GetCurrentProcessId();
  if (family == AF_INET) {
    const auto* table = reinterpret_cast<const MIB_TCPTABLE_OWNER_PID*>(buffer.data());
    if (buffer.size() < sizeof(DWORD) || table->dwNumEntries > (buffer.size() - sizeof(DWORD)) / sizeof(MIB_TCPROW_OWNER_PID)) return false;
    for (DWORD i = 0; i < table->dwNumEntries; ++i) if (table->table[i].dwOwningPid == pid) return false;
  } else {
    const auto* table = reinterpret_cast<const MIB_TCP6TABLE_OWNER_PID*>(buffer.data());
    if (buffer.size() < sizeof(DWORD) || table->dwNumEntries > (buffer.size() - sizeof(DWORD)) / sizeof(MIB_TCP6ROW_OWNER_PID)) return false;
    for (DWORD i = 0; i < table->dwNumEntries; ++i) if (table->table[i].dwOwningPid == pid) return false;
  }
  return true;
}
void Check(bool value, const char* description) {
  ++checks;
  if (!value) { ++failures; std::cerr << "FAIL: " << description << '\n'; }
}
int SelfTest() {
  for (const auto half : {std::string_view("false"), std::string_view("true")}) {
    for (const auto scale : {std::string_view("1"), std::string_view("2"), std::string_view("3")}) {
      const auto values = policy::ParseRuntimeValues(half, "fast", scale, scale);
      Check(values && values->half_pixel == (half == "true") &&
          values->scale_x == uint32_t(scale[0] - '0') && values->scale_y == values->scale_x,
          "actual runtime values parsed exactly");
      if (!values) continue;
      Check(policy::RuntimeUnchanged(*values, values), "unchanged runtime values admitted");
      Check(!policy::RuntimeUnchanged(*values, std::nullopt), "missing runtime re-read rejected");
      auto changed = *values; changed.half_pixel = !changed.half_pixel;
      Check(!policy::RuntimeUnchanged(*values, changed), "half-pixel runtime drift rejected");
      changed = *values; changed.scale_x = changed.scale_y = values->scale_x == 1 ? 2 : 1;
      Check(!policy::RuntimeUnchanged(*values, changed), "scale runtime drift rejected");
      for (const auto phase : {"ready", "outside", "editor"}) {
        const auto marker = policy::RuntimeMarker(phase, *values);
        const auto expected = std::string("MCLA_RENDERDOC_RUNTIME v=1 phase=") + phase +
            " half_pixel=" + (half == "true" ? "1" : "0") + " readback=fast scale_x=" +
            std::string(scale) + " scale_y=" + std::string(scale) + " status=PASS";
        Check(marker && *marker == expected, "exact canonical runtime PASS marker");
      }
    }
  }
  for (const auto bad : {"", "0", "1", "True", "FALSE", "false ", "true\n", "yes"})
    Check(!policy::ParseRuntimeValues(bad, "fast", "2", "2"), "noncanonical runtime bool rejected");
  for (const auto bad : {"", "none", "full", "FAST", "fast ", "fast\n"})
    Check(!policy::ParseRuntimeValues("false", bad, "2", "2"), "runtime readback must be fast");
  for (const auto bad : {"", "0", "4", "-1", "+1", "01", "2.0", " 2", "2\n"}) {
    Check(!policy::ParseRuntimeValues("false", "fast", bad, "2"), "invalid runtime X scale rejected");
    Check(!policy::ParseRuntimeValues("false", "fast", "2", bad), "invalid runtime Y scale rejected");
  }
  Check(!policy::ParseRuntimeValues(std::nullopt, "fast", "2", "2"), "absent half pixel rejected");
  Check(!policy::ParseRuntimeValues("false", std::nullopt, "2", "2"), "absent readback rejected");
  Check(!policy::ParseRuntimeValues("false", "fast", std::nullopt, "2"), "absent X scale rejected");
  Check(!policy::ParseRuntimeValues("false", "fast", "2", std::nullopt), "absent Y scale rejected");
  Check(!policy::ParseRuntimeValues("false", "fast", "1", "2"), "unequal runtime scale rejected");
  const policy::RuntimeValues valid{false, 2, 2};
  for (const auto invalid : {policy::RuntimeValues{}, policy::RuntimeValues{false, 4, 4},
                             policy::RuntimeValues{false, 1, 2}}) {
    Check(!policy::RuntimeUnchanged(invalid, invalid), "malformed identical snapshots rejected");
    Check(!policy::RuntimeMarker("ready", invalid), "malformed snapshot cannot emit PASS");
  }
  for (const auto phase : {"", "READY", "outside ", "editor\n", "unknown", "../outside"})
    Check(!policy::RuntimeMarker(phase, valid), "noncanonical runtime phase rejected");
  Check(policy::kRuntimeFailureMarker == "MCLA_RENDERDOC_RUNTIME v=1 status=FAIL",
        "failure marker is constant and contains no rejected values");
  using std::filesystem::path;
  const path temp(L"C:\\Users\\test\\Temp");
  const std::wstring hex = L"0123456789abcdef0123456789abcdef";
  const path parent = temp / (L"mcla-seeded-inspection-" + hex);
  const path root = parent / L"renderdoc";
  Check(policy::ValidCaptureDirectory(root, temp), "seeded directory admitted");
  Check(policy::ValidCaptureDirectory(root, path(temp.wstring() + L"\\")), "GetTempPath trailing separator admitted");
  Check(policy::ValidCaptureDirectory(temp / (L"mcla-renderdoc-bootstrap-" + hex) / L"renderdoc", temp), "bootstrap directory admitted");
  for (const auto& bad : {temp / L"renderdoc", parent / L"other", root / L"child", temp / L"unowned" / L"renderdoc",
      temp / L"mcla-seeded-inspection-0123456789ABCDEF0123456789ABCDEF" / L"renderdoc",
      temp / L"mcla-seeded-inspection-0123456789abcdef0123456789abcde" / L"renderdoc",
      parent / L".." / L"renderdoc", parent / L"renderdoc ", path(L"\\\\host\\share\\renderdoc"), path(L"C:renderdoc")})
    Check(!policy::ValidCaptureDirectory(bad, temp), "invalid capture directory rejected");
  Check(!policy::ValidCaptureDirectory(root, L"D:\\Temp"), "wrong TEMP rejected");
  Check(policy::ValidLabel("outside", 0), "first outside");
  Check(policy::ValidLabel("editor", 1), "second editor");
  Check(!policy::ValidLabel("editor", 0), "editor cannot precede outside");
  Check(!policy::ValidLabel("outside", 1), "outside cannot repeat");
  Check(!policy::ValidLabel("editor", 2), "third capture prohibited");
  Check(!policy::ValidLabel("../outside", 0), "arbitrary label prohibited");
  Check(policy::ValidCaptureFilename(L"mcla-outside_capture.rdc", "outside"), "outside fixed app-controlled basename");
  Check(policy::ValidCaptureFilename(L"mcla-editor_capture.rdc", "editor"), "editor fixed app-controlled basename");
  for (auto bad : {L"mcla-outside.rdc", L"mcla-outside_frame123.rdc", L"mcla-outside_frame0.rdc", L"mcla-outside_capture2.rdc",
      L"mcla-outside_capture.rdc.exe", L"mcla-outside_capture/2.rdc", L"mcla-outside_capture.rdc:ads", L"mcla-editor_capture.rdc",
      L"../mcla-outside_capture.rdc", L"mcla-outside_capture.RDC"})
    Check(!policy::ValidCaptureFilename(bad, "outside"), "unexpected filename rejected");
  Check(!policy::ValidCaptureFilename(L"mcla-editor_capture.rdc", "unknown"), "unknown filename label rejected");
  Check(!policy::ValidCaptureBytes(0), "empty rejected");
  Check(policy::ValidCaptureBytes(1), "nonempty admitted");
  Check(policy::ValidCaptureBytes(policy::kMaximumCaptureBytes), "size bound inclusive");
  Check(!policy::ValidCaptureBytes(policy::kMaximumCaptureBytes + 1), "oversize rejected");
  using enum policy::Progress;
  Check(policy::CheckProgress(0, 0, 0, false, 0) == Pending, "waiting for first capture");
  Check(policy::CheckProgress(0, 1, 59999, false, 0) == Complete, "one additional capture");
  Check(policy::CheckProgress(1, 2, 2, false, 0) == Complete, "second capture transition");
  Check(policy::CheckProgress(0, 2, 2, false, 0) == Reject, "extra capture rejected");
  Check(policy::CheckProgress(1, 0, 2, false, 0) == Reject, "decreased count rejected");
  Check(policy::CheckProgress(UINT32_MAX, 0, 2, false, 0) == Reject, "count overflow rejected");
  Check(policy::CheckProgress(0, 1, 60000, false, 0) == Reject, "deadline includes completed late capture");
  Check(policy::CheckProgress(0, 1, 1, true, 0) == Reject, "stop overrides completion");
  Check(policy::CheckProgress(0, 1, 1, false, 38920) == Reject, "listener identity rejects completion");
  Check(policy::CheckGuestProgress(10, 10, 10, 0, false, 0) == Pending, "guest initial sequence waits");
  Check(policy::CheckGuestProgress(10, 10, 11, 1, false, 0) == Pending, "one guest output insufficient");
  Check(policy::CheckGuestProgress(10, 11, 12, 29999, false, 0) == Complete, "two guest outputs required");
  Check(policy::CheckGuestProgress(10, 11, 15, 2, false, 0) == Complete, "poll may observe multiple outputs");
  Check(policy::CheckGuestProgress(10, 11, 10, 2, false, 0) == Reject, "guest sequence regression rejected");
  Check(policy::CheckGuestProgress(10, 9, 12, 2, false, 0) == Reject, "invalid previous guest state rejected");
  Check(policy::CheckGuestProgress(UINT64_MAX - 1, UINT64_MAX - 1, UINT64_MAX, 2, false, 0) == Reject, "guest sequence overflow rejected");
  Check(policy::CheckGuestProgress(10, 10, 12, 30000, false, 0) == Reject, "late guest output rejected");
  Check(policy::CheckGuestProgress(10, 10, 12, 2, true, 0) == Reject, "guest cancellation overrides completion");
  Check(policy::CheckGuestProgress(10, 10, 12, 2, false, 38920) == Reject, "guest isolation failure overrides completion");
  for (bool end_result : {false, true}) {
    for (bool remains_active : {false, true}) {
      int ends = 0, queries = 0, discards = 0;
      bool result = policy::FinishCapture([&] { ++ends; return end_result; },
          [&] { ++queries; return remains_active; }, [&] { ++discards; });
      Check(result == (end_result && !remains_active), "End and active-state joint verdict");
      Check(ends == 1 && queries == 1, "End and post-End active query occur exactly once");
      Check(discards == (remains_active ? 1 : 0), "only still-active capture discarded exactly once");
    }
  }
  {
    int ends = 0, discards = 0;
    Check(!policy::FinishCapture([&]() -> bool { ++ends; throw 1; },
        [] { return false; }, [&] { ++discards; }), "End exception fails closed");
    Check(ends == 1 && discards == 1, "unknown End state discarded once without retry");
  }
  {
    int discards = 0;
    Check(!policy::FinishCapture([] { return true; },
        []() -> bool { throw 1; }, [&] { ++discards; throw 2; }), "query/discard exception fails closed");
    Check(discards == 1, "throwing discard never retried");
  }
  Check(!probe::Capture("outside", [] { return false; }, [] { return uint64_t{0}; }), "uninitialized capture no DLL or file activity");
  Check(GetModuleHandleW(L"renderdoc.dll") == nullptr, "self-test never loads RenderDoc");
  std::cout << checks << " checks, " << failures << " failures; no DLL load or capture\n";
  return failures ? 1 : 0;
}
int Bootstrap() {
  // Explicit opt-in only. No graphics API/device, game, screenshots or capture.
  wchar_t temp[32768]{};
  DWORD n = GetTempPathW(static_cast<DWORD>(std::size(temp)), temp);
  if (!n || n >= std::size(temp)) return 2;
  std::array<unsigned char, 16> random{};
  if (BCryptGenRandom(nullptr, random.data(), static_cast<ULONG>(random.size()), BCRYPT_USE_SYSTEM_PREFERRED_RNG) < 0) return 2;
  std::wstring suffix;
  for (auto byte : random) { suffix += L"0123456789abcdef"[byte >> 4]; suffix += L"0123456789abcdef"[byte & 15]; }
  auto parent = std::filesystem::path(temp) / (L"mcla-renderdoc-bootstrap-" + suffix);
  if (!CreateDirectoryW(parent.c_str(), nullptr)) { std::cerr << "Fresh parent creation failed\n"; return 2; }
  std::wcout << L"Owned bootstrap directory: " << parent.wstring() << L'\n';
  auto result = probe::Init(parent / L"renderdoc");
  if (!result) { std::wcerr << result.error() << L'\n'; return 1; }
  if (!NoOwnedListeners(AF_INET) || !NoOwnedListeners(AF_INET6)) {
    std::cerr << "Bootstrap rejected: own-PID IPv4/IPv6 listener check failed\n";
    return 1;
  }
  std::cout << "BOOTSTRAP_PASS: pinned DLL/API 1.6; exclusive wildcard ports reserved; target control ident zero; own PID has zero IPv4/IPv6 LISTEN sockets. No GPU or capture/replay PASS.\n";
  // No cleanup: process lifetime owns hooks/reservations; preserved fresh empty
  // directory is intentional evidence. Never unload hooks or delete evidence.
  return 0;
}
}  // namespace
int wmain(int argc, wchar_t** argv) {
  try {
    if (argc == 1 || (argc == 2 && std::wstring_view(argv[1]) == L"--self-test")) return SelfTest();
    if (argc == 2 && std::wstring_view(argv[1]) == L"--bootstrap-load") return Bootstrap();
    std::cerr << "Usage: --self-test (default; no DLL load) | --bootstrap-load (explicit; no GPU)\n";
    return 2;
  } catch (...) { std::cerr << "Unexpected diagnostic test exception\n"; return 1; }
}
