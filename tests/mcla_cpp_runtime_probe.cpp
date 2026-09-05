// Nonshipping /MD compatibility probe. Run the same PE with builtin and
// app-local CRTs; inspect loaded paths as well as the synchronization result.
#include <chrono>
#include <condition_variable>
#include <cstdio>
#include <cwchar>
#include <initializer_list>
#include <mutex>
#include <thread>

#if defined(_WIN32)
#include <windows.h>
#endif

int main() {
#if defined(_WIN32)
  for (const auto* name : {L"msvcp140.dll", L"vcruntime140.dll",
                           L"vcruntime140_1.dll"}) {
    wchar_t path[32768]{};
    const auto module = GetModuleHandleW(name);
    if (!module) {
      std::fwprintf(stdout, L"CRT %ls=NOT_LOADED\n", name);
      if (std::wcscmp(name, L"msvcp140.dll") == 0) return 2;
      continue;
    }
    const auto length = GetModuleFileNameW(module, path, 32768);
    if (length == 0 || length >= 32768) {
      std::fwprintf(stderr, L"CRT module unavailable: %ls\n", name);
      return 2;
    }
    std::fwprintf(stdout, L"CRT %ls=%ls\n", name, path);
  }
#endif
  for (int attempt = 0; attempt < 3; ++attempt) {
    std::mutex mutex;
    std::condition_variable condition;
    bool observed = false;
    long long before_lock_ms = -1;
    long long acquired_ms = -1;
    const auto start = std::chrono::steady_clock::now();
    const auto elapsed = [&] {
      return static_cast<long long>(
          std::chrono::duration_cast<std::chrono::milliseconds>(
              std::chrono::steady_clock::now() - start).count());
    };
    std::unique_lock lock(mutex);
    std::thread contender([&] {
      std::this_thread::sleep_for(std::chrono::milliseconds(20));
      before_lock_ms = elapsed();
      std::lock_guard guard(mutex);
      acquired_ms = elapsed();
      observed = true;
      condition.notify_all();
    });
    const bool success = condition.wait_for(
        lock, std::chrono::milliseconds(500), [&] { return observed; });
    const auto completion_ms = elapsed();
    lock.unlock();
    contender.join();
    // Every worker-written diagnostic is read after join. A timeout is a
    // failure even if the contender acquires the mutex immediately afterward.
    std::fwprintf(stdout, L"attempt=%d observed=%d elapsed_ms=%lld "
                L"contender_before_lock_ms=%lld acquired_ms=%lld\n",
                attempt, int(success), completion_ms, before_lock_ms,
                acquired_ms);
    if (!success || completion_ms >= 400) {
      return 1;
    }
  }
  return 0;
}
