// KI-082 traffic motion probe (MCLA_TRAFMOT v=1). See mcla_traffic_motion.h
// for the guest layout, its generated-code citations and the classification,
// and docs/evidence/M11-l107-guest-tiling-and-msaa-20261117.md.
//
// One strong override, active only with the init-only switch
// mcla_traffic_motion_probe (default off; with it off the hook is one static
// bool test and the original call). It calls the ORIGINAL and only reads
// guest words the original itself reads at the same point:
//   sub_826FA0F8 (mcAmbientCollider vfunc +56, main thread, once per
//     simulated ambient car per step): at entry, the collider's physics
//     instance and the matrix the original is about to use (same branch);
//     the position (row 3) and the orientation check go to the tracker.
// MclaTrafficMotionOnTimerUpdate (called by the raw timer after each
// main-timer tick) closes the update, writes the per-minute summary and,
// for kBurstNs after an accepted live snapshot (MclaTrafficMotionMark), one
// line per update. Guest memory and registers are never written.

#include "mcla_traffic_motion.h"

#include "mcla_init.h"
#include "mcla_logging.h"

#include <rex/cvar.h>

#include <atomic>
#include <chrono>
#include <cstring>
#include <mutex>
#include <string>
#include <vector>

REXCVAR_DEFINE_BOOL(mcla_traffic_motion_probe, false, "MCLA",
    "Log the per-update motion of nearby simulated traffic cars against the "
    "game step and the host frame interval (KI-082; MCLA_TRAFMOT lines, a "
    "5 s per-update burst after each F10; diagnostic, read-only)")
    .lifecycle(rex::cvar::Lifecycle::kInitOnly);

namespace {

namespace tmo = mcla::traffic_motion;

bool ProbeOn() {
  static const bool on = REXCVAR_GET(mcla_traffic_motion_probe);
  return on;
}

float AsFloat(uint32_t bits) {
  float value = 0.0f;
  std::memcpy(&value, &bits, sizeof(value));
  return value;
}

int64_t NowNs() {
  return std::chrono::duration_cast<std::chrono::nanoseconds>(
             std::chrono::steady_clock::now().time_since_epoch())
      .count();
}

struct Shared {
  std::mutex mutex;
  tmo::Tracker tracker;  // under the mutex
  bool configured = false;
  int64_t burst_until_ns = 0;
  uint64_t minutes = 0;
  uint64_t bursts = 0;
  uint64_t frame_lines = 0;
  std::atomic<bool> burst_request{false};
};

Shared &SharedState() {
  static Shared shared;
  return shared;
}

tmo::Vec3 Row(uint8_t *base, uint32_t at) {
  return {AsFloat(REX_LOAD_U32(at)), AsFloat(REX_LOAD_U32(at + 4)),
          AsFloat(REX_LOAD_U32(at + 8))};
}

void Observe(PPCContext &ctx, uint8_t *base) {
  const uint32_t collider = ctx.r3.u32;
  if (!tmo::PointerPlausible(collider)) {
    return;
  }
  const uint32_t inst = REX_LOAD_U32(collider + tmo::kInstOffset);
  if (!tmo::PointerPlausible(inst)) {
    return;
  }
  // The branch the original takes, reading only what it reads.
  uint8_t tls_byte = 0;
  const uint32_t tls = REX_LOAD_U32(ctx.r13.u32);
  if (tmo::PointerPlausible(tls)) {
    tls_byte = REX_LOAD_U8(tls + tmo::kTlsByteOffset);
  }
  uint8_t flags = 0;
  uint32_t inner = 0;
  if (tls_byte == 0) {
    flags = REX_LOAD_U8(inst + tmo::kInstFlagsOffset);
    if ((flags & tmo::kInstLinkedBit) != 0) {
      const uint32_t link = REX_LOAD_U32(inst + tmo::kInstLinkOffset);
      if (tmo::PointerPlausible(link)) {
        inner = REX_LOAD_U32(link + tmo::kLinkInnerOffset);
      }
    }
  }
  const tmo::Path path = tmo::SelectPath(tls_byte, flags, inner);
  uint32_t inner_base = 0;
  uint32_t buffer_index = 0;
  if (path == tmo::Path::kLinked) {
    if (!tmo::PointerPlausible(inner)) {
      return;
    }
    inner_base = REX_LOAD_U32(inner + tmo::kInnerBaseOffset);
  } else if (path == tmo::Path::kBuffered) {
    buffer_index = REX_LOAD_U32(tmo::kBufferIndex);
    if (buffer_index > 16) {
      return;
    }
  }
  const uint32_t m = tmo::MatrixAddress(path, inst, inner_base, buffer_index);
  if (!tmo::PointerPlausible(m)) {
    return;
  }
  const tmo::Vec3 r0 = Row(base, m);
  const tmo::Vec3 r1 = Row(base, m + tmo::kRowBytes);
  const tmo::Vec3 r2 = Row(base, m + 2 * tmo::kRowBytes);
  const tmo::Vec3 pos = Row(base, m + tmo::kPositionOffset);
  auto &s = SharedState();
  std::lock_guard guard(s.mutex);
  s.tracker.OnCar(collider, pos, tmo::MatrixOk(r0, r1, r2), path);
}

}  // namespace

// Ambient collider update: r3 = the collider.
REX_EXTERN(sub_826FA0F8) {
  if (ProbeOn()) {
    Observe(ctx, base);
  }
  __imp__sub_826FA0F8(ctx, base);
}

void MclaTrafficMotionOnTimerUpdate(uint8_t *base) {
  if (!ProbeOn()) {
    return;
  }
  const int64_t now = NowNs();
  const double dt = AsFloat(REX_LOAD_U32(tmo::kMainTimer + tmo::kDeltaOffset));
  std::vector<std::string> lines;
  auto &s = SharedState();
  {
    std::lock_guard guard(s.mutex);
    if (!s.configured) {
      s.configured = true;
      char text[160];
      std::snprintf(text, sizeof(text),
                    "MCLA_TRAFMOT v=1 kind=config probe=1 cars=%zu burst_s=%lld minute_s=%lld",
                    tmo::kMaxCars, static_cast<long long>(tmo::kBurstNs / 1'000'000'000),
                    static_cast<long long>(tmo::kMinuteNs / 1'000'000'000));
      lines.emplace_back(text);
    }
    if (s.burst_request.exchange(false, std::memory_order_acq_rel)) {
      s.burst_until_ns = now + tmo::kBurstNs;
      ++s.bursts;
      lines.emplace_back("MCLA_TRAFMOT v=1 kind=burst seq=" + std::to_string(s.bursts) +
                         " upd=" + std::to_string(s.tracker.update()));
    }
    tmo::Frame frame;
    const bool finished = s.tracker.OnUpdate(now, dt, &frame);
    const bool burst = now <= s.burst_until_ns;
    if (finished && burst) {
      lines.push_back(tmo::FrameLine(frame));
      ++s.frame_lines;
    }
    if (s.tracker.MinuteDue(now)) {
      lines.push_back(tmo::MinuteLine(s.tracker.TakeMinute(now), burst));
      ++s.minutes;
    }
  }
  for (const auto &line : lines) {
    MCLA_APP_INFO("{}", line);
  }
}

void MclaTrafficMotionMark() {
  if (!ProbeOn()) {
    return;
  }
  SharedState().burst_request.store(true, std::memory_order_release);
}

std::string MclaTrafficMotionStateJson() {
  if (!ProbeOn()) {
    return {};
  }
  auto &s = SharedState();
  std::lock_guard guard(s.mutex);
  return tmo::StateJson(true, s.tracker.update(), s.minutes, s.bursts, s.frame_lines,
                       s.tracker.minute());
}
