// mcla - private protocol shared by the title and crash helper

#pragma once

#if defined(_WIN32)

#include <Windows.h>

#include <cstdint>

namespace mcla::diagnostics::native {

constexpr uint32_t kCrashIpcMagic = 0x4D434C41; // MCLA
constexpr uint32_t kCrashIpcVersion = 2;
constexpr size_t kCrashIpcPathCapacity = 1024;
constexpr size_t kCrashIpcVersionCapacity = 32;

struct CrashIpcState {
  uint32_t magic;
  uint32_t version;
  DWORD parent_process_id;
  volatile LONG live_sequence;
  volatile LONG live_completed_sequence;
  volatile LONG live_result;
  wchar_t live_dump_path[kCrashIpcPathCapacity];
  volatile LONG crash_started;
  volatile LONG crash_result;
  volatile LONG show_reporter_dialog;
  DWORD crash_thread_id;
  DWORD exception_code;
  uint64_t exception_address;
  uint64_t exception_pointers;
  wchar_t diagnostics_root[kCrashIpcPathCapacity];
  wchar_t user_data_root[kCrashIpcPathCapacity];
  wchar_t journal_path[kCrashIpcPathCapacity];
  wchar_t app_version[kCrashIpcVersionCapacity];
};

} // namespace mcla::diagnostics::native

#endif
