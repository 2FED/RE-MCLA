// KI-035 F10 guest-thread sampler. See mcla_guest_sampler.h.
//
// Lifetime: ObjectTable::GetObjectsByType returns retained object_refs, and
// they are held until the window ends, so no XThread is destroyed while it is
// sampled. XThread::Exit/Terminate free the guest stack and release the
// handle but never reset thread_state_ (it is destroyed only with the XThread
// object), so the PPCContext storage stays valid while the ref is held. The
// context pointer is snapshotted only for threads that are running at the
// start: running_ is set after thread_state_ is assigned in Create(), so a
// thread still being created is never read. A thread that exits during the
// window is skipped from then on (is_running() turns false) instead of
// repeating its last breadcrumbs.

#include "mcla_guest_sampler.h"

#include "mcla_logging.h"

#include <rex/ppc/context.h>
#include <rex/system/kernel_state.h>
#include <rex/system/thread_state.h>
#include <rex/system/xthread.h>

#include <chrono>
#include <mutex>
#include <stop_token>
#include <thread>

namespace mcla::guest_sampler {
namespace {

std::mutex sampler_mutex;
std::jthread sampler_thread;

struct Target {
  rex::system::object_ref<rex::system::XThread> thread;
  const PPCContext *context = nullptr;
  Histogram histogram;
};

void Run(std::stop_token stop, rex::system::KernelState *kernel) {
  std::vector<Target> targets;
  for (auto &thread : kernel->object_table()->GetObjectsByType<rex::system::XThread>()) {
    if (!thread || !thread->is_guest_thread() || !thread->is_running()) {
      continue;
    }
    auto *state = thread->thread_state();
    const PPCContext *context = state ? state->context() : nullptr;
    if (!context) {
      continue;
    }
    Target target;
    target.histogram.name = thread->name();
    // M10-001: the host thread id and its "XThreadNNNN (handle)" name.
    if (auto *host_thread = thread->thread()) {
      target.histogram.tid = host_thread->system_id();
      if (target.histogram.name.empty()) {
        target.histogram.name = host_thread->name();
      }
    }
    target.histogram.handle = thread->handle();
    target.context = context;
    target.thread = std::move(thread);
    targets.push_back(std::move(target));
  }
  MCLA_APP_INFO("MCLA_GUEST_SAMPLE v=1 start threads={} seconds={} interval_ms={}",
                targets.size(), kSampleSeconds, kSampleIntervalMs);
  const auto end = std::chrono::steady_clock::now() + std::chrono::seconds(kSampleSeconds);
  while (!stop.stop_requested() && std::chrono::steady_clock::now() < end) {
    for (auto &target : targets) {
      if (!target.thread->is_running()) {
        continue;
      }
      // Racy diagnostic reads of aligned scalars written by the guest thread
      // (GuestFunctionScope / RecordGuestImport). The import pointer always
      // names a static string.
      const uint32_t function =
          *static_cast<const volatile uint32_t *>(&target.context->current_function);
      const char *import =
          *static_cast<const char *const volatile *>(&target.context->last_import);
      target.histogram.Add(function, import);
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(kSampleIntervalMs));
  }
  for (const auto &target : targets) {
    const auto line = FormatLine(target.histogram);
    if (!line.empty()) {
      MCLA_APP_INFO("{}", line);
    }
  }
  MCLA_APP_INFO("MCLA_GUEST_SAMPLE v=1 end stopped={}", stop.stop_requested() ? 1 : 0);
}

void JoinLocked() {
  if (sampler_thread.joinable()) {
    sampler_thread.request_stop();
    sampler_thread.join();
  }
}

}  // namespace

void Start(rex::system::KernelState *kernel) {
  if (!kernel) {
    return;
  }
  std::lock_guard lock(sampler_mutex);
  JoinLocked();
  sampler_thread =
      std::jthread([kernel](std::stop_token stop) { Run(stop, kernel); });
}

void Stop() {
  std::lock_guard lock(sampler_mutex);
  JoinLocked();
}

}  // namespace mcla::guest_sampler
