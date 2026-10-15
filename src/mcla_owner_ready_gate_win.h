#pragma once
#include "mcla_owner_ready_gate.h"
#include <windows.h>
#include <algorithm>
#include <chrono>
#include <string>
#include <string_view>
#include <thread>

namespace mcla::owner_ready {
inline std::wstring EventName(std::string_view token, bool waiting) {
  Require(token.size() == 32 && std::all_of(token.begin(), token.end(), [](char c) {
    return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'); }), "OWNER_READY_INVALID_TOKEN");
  return std::wstring(waiting ? L"Local\\MCLA_OWNER_WAITING_V1_" : L"Local\\MCLA_OWNER_READY_V1_") +
         std::wstring(token.begin(), token.end());
}
struct EventHandle {
  HANDLE value{};
  explicit EventHandle(HANDLE h) : value(h) { Require(h != nullptr, "OWNER_READY_OPEN_FAILED"); }
  EventHandle(const EventHandle&) = delete;
  EventHandle& operator=(const EventHandle&) = delete;
  ~EventHandle() { CloseHandle(value); }
};
inline uint64_t Clock() {
  LARGE_INTEGER q{};
  Require(QueryPerformanceCounter(&q) && q.QuadPart > 0, "OWNER_READY_QPC_FAILED");
  return uint64_t(q.QuadPart);
}
inline Poll PollEvent(HANDLE event) {
  const auto result = WaitForSingleObject(event, 0);
  return result == WAIT_TIMEOUT ? Poll::kEmpty : result == WAIT_OBJECT_0 ? Poll::kSignaled : Poll::kFailed;
}
inline Evidence WaitNative(std::string_view token, Window window, std::stop_token stop) {
  LARGE_INTEGER frequency{};
  Require(QueryPerformanceFrequency(&frequency) && uint64_t(frequency.QuadPart) == window.frequency,
          "OWNER_READY_CLOCK_MISMATCH");
  // Supervisor alone creates objects. Never create, reset or adopt here.
  EventHandle ready(OpenEventW(SYNCHRONIZE, FALSE, EventName(token, false).c_str()));
  EventHandle waiting(OpenEventW(SYNCHRONIZE | EVENT_MODIFY_STATE, FALSE, EventName(token, true).c_str()));
  return Wait(window, stop, {Clock, [&] { return PollEvent(ready.value); },
      [&] { return PollEvent(waiting.value); }, [&] { return SetEvent(waiting.value) != 0; },
      [] { std::this_thread::sleep_for(std::chrono::milliseconds(25)); }});
}
} // namespace mcla::owner_ready
