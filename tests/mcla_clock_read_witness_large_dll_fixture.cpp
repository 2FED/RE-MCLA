// Synthetic source-faithful DLL prerequisite only; never SDK/game admission.
#include "../src/mcla_clock_read_witness_bridge_core.h"
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>
#include <xmmintrin.h>
#include <string_view>
#include <thread>
#include <utility>
#ifndef MCLA_CLOCK_SOURCE_SHA256
#error Pinned original source identity required
#endif
static_assert(std::string_view(MCLA_CLOCK_SOURCE_SHA256) ==
    "D72A7093E0029F85FFB6859B05DE440416EB2F66157E2C5BC994B538A29F7DE4");
namespace b = mcla::clock_read_witness::bridge;
namespace f {
// Both core and scratch are process-owned preallocated storage, not stack.
constinit b::RuntimeV2 runtime;
constinit b::Runtime masking_runtime; // Test-only cross-capacity masking.
constinit std::atomic<uint32_t> entered{0}, released{0};
struct State {
  MclaClockWitnessFixtureConfigV1 config;
  MclaClockWitnessFixtureStatsV1 stats;
  uint32_t owned{};
  bool observed{}, nested{};
};
constinit thread_local State state{};
uint64_t Original(bool observed);
uint64_t Sample() {
  auto& s = state;
  const auto n = s.stats.host_samples++;
  if (n < 4) { s.stats.incoming_error[n] = GetLastError(); s.stats.incoming_mxcsr[n] = _mm_getcsr(); }
  else ++s.stats.fixture_errors;
  SetLastError(0xABCD1000u + uint32_t(n)); _mm_setcsr(n % 2 ? 0x5F80u : 0x3F80u);
  if (s.config.scenario == MCLA_CW_FIXTURE_THROW) throw uint32_t(0x10203040u);
  return 100 + n * 5;
}
}
namespace fixture_std {
struct defer_lock_t {};
inline constexpr defer_lock_t defer_lock{};
struct mutex {
  bool try_lock() noexcept { ++f::state.stats.tries; ++f::state.owned; return true; }
  void lock() noexcept { ++f::state.stats.waits; ++f::state.owned; }
  void unlock() noexcept {
    if (f::state.owned != 1) ++f::state.stats.fixture_errors;
    if (f::state.owned) --f::state.owned; ++f::state.stats.unlocks;
  }
};
template<class M> class unique_lock {
 public:
  unique_lock(M& m, defer_lock_t) noexcept : m_(m) {}
  ~unique_lock() noexcept { if (held_) m_.unlock(); }
  bool try_lock() noexcept { held_ = m_.try_lock(); return held_; }
  void lock() noexcept { m_.lock(); held_ = true; }
 private: M& m_; bool held_{};
};
}
namespace baseline {
thread_local bool clock_no_scaling{};
thread_local std::pair<uint64_t,uint64_t> guest_tick_ratio_{5,1};
thread_local uint64_t last_host_tick_count_{95},last_guest_tick_count_{475};
thread_local fixture_std::mutex tick_mutex_;
struct Clock { static uint64_t QueryHostTickCount() { return f::Sample(); } };
#define REXCVAR_GET(name) (name)
#define std fixture_std
#include "mcla_clock_update_body.inc"
#undef std
#undef REXCVAR_GET
}
using MclaClockWitnessCall = b::Witness;
namespace instrumented {
thread_local bool clock_no_scaling{};
thread_local std::pair<uint64_t,uint64_t> guest_tick_ratio_{5,1};
thread_local uint64_t last_host_tick_count_{95},last_guest_tick_count_{475};
thread_local fixture_std::mutex tick_mutex_;
struct Clock { static uint64_t QueryHostTickCount() { return f::Sample(); } };
#define REXCVAR_GET(name) (name)
#define std fixture_std
#include "mcla_clock_update_witness_body.inc"
#undef std
#undef REXCVAR_GET
}
namespace f {
bool Prefix(const void* p,uint32_t version,uint32_t size) noexcept {
  if (!p) return false;
  uint32_t h[2]; std::memcpy(h,p,sizeof(h)); return h[0] == version && h[1] == size;
}
uint64_t Original(bool observed) {
  auto& s = state; ++s.stats.original_queries; s.observed = observed;
  if (!s.nested && s.config.scenario == MCLA_CW_FIXTURE_STOP_PENDING) {
    entered.fetch_add(1,std::memory_order_release);
    while (!released.load(std::memory_order_acquire)) std::this_thread::yield();
  }
  if (!s.nested && s.config.scenario == MCLA_CW_FIXTURE_NESTED_INVALID) {
    s.nested = true;
    auto invalid = s.config.nested; invalid.abi_version = 99;
    MclaClockWitnessStatusV1 status{};
    s.stats.nested_return = observed ? masking_runtime.QueryBound(&invalid,&status,[] { return Original(true); }) : Original(false);
    s.stats.nested_admission = status.admission; s.stats.nested_completion = status.completion;
    s.nested = false;
  }
  return observed ? instrumented::UpdateGuestClock() : baseline::UpdateGuestClock();
}
}
#define EXPORT extern "C" __declspec(dllexport)
EXPORT uint32_t MCLA_CW_CALL MclaClockWitnessInfoV2Export(MclaClockWitnessInfoV2* out) noexcept {
  if (!f::Prefix(out,MCLA_CW_ABI_V2,sizeof(*out))) return MCLA_CW_BAD_ABI;
  *out = {MCLA_CW_ABI_V2,sizeof(*out),1,1,1,2,
      sizeof(MclaClockWitnessBindingV1),sizeof(MclaClockWitnessStatusV1),sizeof(MclaClockWitnessRecordV1),
      sizeof(MclaClockWitnessSnapshotV2),120,MCLA_CW_CAPACITY_V2,0,0,{0,0}};
  return MCLA_CW_OK;
}
EXPORT uint32_t MCLA_CW_CALL MclaClockWitnessArmV2Export(uint64_t high,uint64_t low) noexcept {
  return f::runtime.Arm(high,low) ? MCLA_CW_OK : MCLA_CW_REJECTED;
}
EXPORT uint64_t MCLA_CW_CALL MclaClockWitnessQueryBoundV2Export(const MclaClockWitnessBindingV1* input,MclaClockWitnessStatusV1* status) {
  return f::runtime.QueryBound(input,status,[] { return f::Original(true); });
}
EXPORT void MCLA_CW_CALL MclaClockWitnessStopV2Export() noexcept { f::runtime.Stop(); }
EXPORT uint32_t MCLA_CW_CALL MclaClockWitnessSnapshotV2Export(MclaClockWitnessSnapshotV2* out) noexcept { return f::runtime.TrySnapshot(out); }
EXPORT uint32_t MCLA_CW_CALL MclaClockWitnessFixtureConfigureV2Export(const MclaClockWitnessFixtureConfigV1* input) noexcept {
  if (!f::Prefix(input,1,sizeof(*input))) return MCLA_CW_BAD_ABI;
  MclaClockWitnessFixtureConfigV1 copy; std::memcpy(&copy,input,sizeof(copy));
  if (copy.reserved || (copy.scenario != MCLA_CW_FIXTURE_NORMAL && copy.scenario != MCLA_CW_FIXTURE_THROW &&
      copy.scenario != MCLA_CW_FIXTURE_NESTED_INVALID && copy.scenario != MCLA_CW_FIXTURE_STOP_PENDING)) return MCLA_CW_BAD_ABI;
  f::state = {}; f::state.config = copy;
  f::state.stats.abi_version = 1; f::state.stats.struct_size = sizeof(f::state.stats);
  baseline::last_host_tick_count_ = instrumented::last_host_tick_count_ = 95;
  baseline::last_guest_tick_count_ = instrumented::last_guest_tick_count_ = 475;
  return MCLA_CW_OK;
}
EXPORT uint32_t MCLA_CW_CALL MclaClockWitnessFixtureStatsV2Export(MclaClockWitnessFixtureStatsV1* out) noexcept {
  if (!f::Prefix(out,1,sizeof(*out))) return MCLA_CW_BAD_ABI;
  f::state.stats.last_host = f::state.observed ? instrumented::last_host_tick_count_ : baseline::last_host_tick_count_;
  f::state.stats.last_guest = f::state.observed ? instrumented::last_guest_tick_count_ : baseline::last_guest_tick_count_;
  *out = f::state.stats; return MCLA_CW_OK;
}
EXPORT uint64_t MCLA_CW_CALL MclaClockWitnessFixtureOriginalV2Export() { return f::Original(false); }
// Test-only atomic rendezvous OUTSIDE original body/clock lock; never production.
EXPORT uint32_t MCLA_CW_CALL MclaClockWitnessFixtureGateV2Export(uint32_t action) noexcept {
  if (action == 1) f::released.store(1,std::memory_order_release);
  return f::entered.load(std::memory_order_acquire);
}
#undef EXPORT
