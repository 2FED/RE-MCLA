// M10-001 host side of the command-processor cost split. See
// mcla_perf_split.h. Everything here is a no-op with an SDK that has no
// rex/perf/perf_split.h (the pinned volunteer SDK).

#include "mcla_perf_split.h"

#include "mcla_init.h"
#include "mcla_logging.h"

#include <rex/cvar.h>

#include <chrono>
#include <mutex>
#include <stop_token>
#include <thread>

#if __has_include(<rex/perf/perf_split.h>)
#include <rex/perf/perf_split.h>
#define MCLA_HAVE_PERF_SPLIT 1
#else
#define MCLA_HAVE_PERF_SPLIT 0
#endif

#if defined(_WIN32)
#include <windows.h>
#include <tlhelp32.h>
#endif

#ifndef MCLA_PERF_SPLIT_DEFAULT
#define MCLA_PERF_SPLIT_DEFAULT 0
#endif

namespace mcla::perf_split {
namespace {

std::mutex worker_mutex;
std::jthread worker;

#if MCLA_HAVE_PERF_SPLIT && defined(_WIN32)
std::string ThreadName(HANDLE thread) {
  using GetThreadDescriptionFunction = HRESULT(WINAPI *)(HANDLE, PWSTR *);
  static const auto function = reinterpret_cast<GetThreadDescriptionFunction>(
      GetProcAddress(GetModuleHandleW(L"kernel32.dll"), "GetThreadDescription"));
  if (!function) {
    return {};
  }
  PWSTR description = nullptr;
  if (FAILED(function(thread, &description)) || !description) {
    return {};
  }
  std::string result;
  const int bytes = WideCharToMultiByte(CP_UTF8, 0, description, -1, nullptr, 0,
                                        nullptr, nullptr);
  if (bytes > 1) {
    result.resize(static_cast<size_t>(bytes));
    WideCharToMultiByte(CP_UTF8, 0, description, -1, result.data(), bytes, nullptr,
                        nullptr);
    result.pop_back();
  }
  LocalFree(description);
  return result;
}

uint64_t FileTime100ns(const FILETIME &value) {
  return (uint64_t(value.dwHighDateTime) << 32) | value.dwLowDateTime;
}

std::vector<ThreadCpuSample> SampleThreads() {
  std::vector<ThreadCpuSample> samples;
  HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
  if (snapshot == INVALID_HANDLE_VALUE) {
    return samples;
  }
  THREADENTRY32 entry{};
  entry.dwSize = sizeof(entry);
  const DWORD pid = GetCurrentProcessId();
  if (Thread32First(snapshot, &entry)) {
    do {
      if (entry.th32OwnerProcessID != pid) {
        continue;
      }
      HANDLE thread =
          OpenThread(THREAD_QUERY_LIMITED_INFORMATION, FALSE, entry.th32ThreadID);
      if (!thread) {
        continue;
      }
      FILETIME creation{}, exit{}, kernel{}, user{};
      if (GetThreadTimes(thread, &creation, &exit, &kernel, &user)) {
        ThreadCpuSample sample;
        sample.id = entry.th32ThreadID;
        sample.user_100ns = FileTime100ns(user);
        sample.kernel_100ns = FileTime100ns(kernel);
        sample.name = ThreadName(thread);
        samples.push_back(std::move(sample));
      }
      CloseHandle(thread);
    } while (Thread32Next(snapshot, &entry));
  }
  CloseHandle(snapshot);
  return samples;
}

// Off the CP thread: one Toolhelp walk per emitted split window.
void RunThreadCpu(std::stop_token stop) {
  std::vector<ThreadCpuSample> previous = SampleThreads();
  uint64_t seen_seq = rex::perf::GetPerfSplitSnapshot().last_window.seq;
  while (!stop.stop_requested()) {
    std::this_thread::sleep_for(std::chrono::milliseconds(250));
    const auto snapshot = rex::perf::GetPerfSplitSnapshot();
    if (!snapshot.available || snapshot.last_window.seq == seen_seq) {
      continue;
    }
    seen_seq = snapshot.last_window.seq;
    auto current = SampleThreads();
    MCLA_GPU_INFO("{}", FormatThreadCpuLine(snapshot.last_window.reason, seen_seq,
                                            snapshot.last_window.wall_ms, previous, current));
    previous = std::move(current);
  }
}
#endif

}  // namespace

void OnGraphicsPluginLoaded() {
#if MCLA_HAVE_PERF_SPLIT
  const std::string cvar(kPerfSplitCvar);
  const bool sdk_has_cvar = rex::cvar::GetFlagInfo(cvar) != nullptr;
  const bool user_set = sdk_has_cvar && rex::cvar::HasNonDefaultValue(cvar);
  switch (DecideDefault(MCLA_PERF_SPLIT_DEFAULT != 0, sdk_has_cvar, user_set)) {
    case DefaultAction::kDisabledByOption:
      break;
    case DefaultAction::kUnavailable:
      MCLA_GPU_INFO("MCLA perf split: {} unavailable in this SDK", cvar);
      break;
    case DefaultAction::kUserSet:
      MCLA_GPU_INFO("MCLA perf split: {} set by the user ({}), left alone", cvar,
                    rex::cvar::Query<int32_t>(cvar));
      break;
    case DefaultAction::kEnable:
      if (rex::cvar::SetFlagByName(cvar, "1")) {
        MCLA_GPU_INFO("MCLA perf split: build default {}=1 (one draw in 16)", cvar);
      } else {
        MCLA_GPU_ERROR("MCLA perf split: could not set {}=1", cvar);
      }
      break;
  }
#if defined(_WIN32)
  // The worker only logs once the CP has emitted a window, so it is harmless
  // while the split stays off.
  if (sdk_has_cvar) {
    std::lock_guard lock(worker_mutex);
    if (!worker.joinable()) {
      worker = std::jthread([](std::stop_token stop) { RunThreadCpu(stop); });
    }
  }
#endif
#endif
}

void Mark(std::string_view source) {
#if MCLA_HAVE_PERF_SPLIT
  const std::string tag(source);
  rex::graphics::MarkPerfSplit(tag.c_str());
#else
  (void)source;
#endif
}

std::string StateJson() {
#if MCLA_HAVE_PERF_SPLIT
  const auto snapshot = rex::perf::GetPerfSplitSnapshot();
  if (!snapshot.available) {
    return {};
  }
  const auto &t = snapshot.totals;
  std::vector<std::pair<std::string, std::string>> fields;
  fields.emplace_back("version", std::to_string(rex::perf::kPerfSplitVersion));
  fields.emplace_back("active_mode", std::to_string(rex::perf::PerfSplitActiveMode()));
  fields.emplace_back("seq", std::to_string(t.seq));
  fields.emplace_back("last_reason",
                      "\"" + rex::perf::SanitizePerfSplitReason(t.reason) + "\"");
  fields.emplace_back("tsc_mhz", JsonMs(t.tsc_mhz));
  fields.emplace_back("render_tid", std::to_string(t.render_tid));
  fields.emplace_back("render_handle", std::to_string(t.render_handle));
  fields.emplace_back("wall_ms", JsonMs(t.wall_ms));
#define MCLA_PERF_SPLIT_JSON_Count(name) fields.emplace_back(#name, std::to_string(t.name));
#define MCLA_PERF_SPLIT_JSON_Ms(name) fields.emplace_back(#name "_ms", JsonMs(t.name##_ms));
#define MCLA_PERF_SPLIT_JSON_Kb(name) fields.emplace_back(#name "_kb", std::to_string(t.name##_kb));
#define MCLA_PERF_SPLIT_JSON(name, kind) MCLA_PERF_SPLIT_JSON_##kind(name)
  REX_PERF_SPLIT_FIELDS(MCLA_PERF_SPLIT_JSON)
#undef MCLA_PERF_SPLIT_JSON
#undef MCLA_PERF_SPLIT_JSON_Kb
#undef MCLA_PERF_SPLIT_JSON_Ms
#undef MCLA_PERF_SPLIT_JSON_Count
  return FormatJsonObject(fields);
#else
  return {};
#endif
}

void Shutdown() {
  std::lock_guard lock(worker_mutex);
  if (worker.joinable()) {
    worker.request_stop();
    worker.join();
  }
}

}  // namespace mcla::perf_split

#if MCLA_HAVE_PERF_SPLIT
namespace {
// Depth of sub_82411E98 on this guest thread: sub_82412F98 polls inside it
// are already part of its wait.
thread_local uint32_t serial_wait_depth = 0;
}  // namespace

// Strong override of the generated weak GPU-serial wait: loops on
// sub_82412F98 until the GPU read position passes the requested fence. Two
// TSC reads per call while the split is on; otherwise one relaxed load.
REX_EXTERN(sub_82411E98) {
  if (!rex::perf::PerfSplitActive()) {
    __imp__sub_82411E98(ctx, base);
    return;
  }
  ++serial_wait_depth;
  const uint64_t begin = rex::perf::ReadTsc();
  __imp__sub_82411E98(ctx, base);
  rex::perf::AddRenderSerialWaitTicks(rex::perf::ReadTsc() - begin);
  --serial_wait_depth;
}

// Strong override of the generated weak GPU-serial poll step, timed only
// when called outside sub_82411E98 (five other call sites).
REX_EXTERN(sub_82412F98) {
  if (serial_wait_depth || !rex::perf::PerfSplitActive()) {
    __imp__sub_82412F98(ctx, base);
    return;
  }
  const uint64_t begin = rex::perf::ReadTsc();
  __imp__sub_82412F98(ctx, base);
  rex::perf::AddRenderSerialPollTicks(rex::perf::ReadTsc() - begin);
}
#endif
