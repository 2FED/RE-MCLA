// SYNTHETIC DLL ONLY. No real SDK clock, game, file I/O, clocks or GUI execute.
// Recipe supplies the exact original/transformed clock bodies. Runtime and TLS
// are owned here, never instantiated in the EXE. Keep this DLL pinned for life.
#include "../src/mcla_clock_read_witness_bridge_core.h"
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>
#include <xmmintrin.h>
#include <string_view>
#include <utility>

#ifndef MCLA_CLOCK_SOURCE_SHA256
#error Exact source identity must be supplied by the independently checked recipe.
#endif
static_assert(std::string_view(MCLA_CLOCK_SOURCE_SHA256) ==
    "D72A7093E0029F85FFB6859B05DE440416EB2F66157E2C5BC994B538A29F7DE4");
namespace bridge = mcla::clock_read_witness::bridge;
namespace fixture {
constinit bridge::Runtime runtime;
constinit bridge::Runtime pending_runtime;  // Test-only separate lifetime, NEVER reset.
constinit MclaClockWitnessSnapshotV1 pending_output{};  // Single owner of pending test.
struct State {
  MclaClockWitnessFixtureConfigV1 config;
  MclaClockWitnessFixtureStatsV1 stats;
  MclaClockWitnessStatusV1 nested_status;
  bridge::Runtime* stop_target;
  uint32_t depth, owned_locks;
  bool held, last_observed;
};
constinit thread_local State state{};
uint64_t Original(bool observed);
uint64_t PlainObserved();
uint64_t PlainBaseline();
uint64_t Sample() {
  auto& s = state;
  const uint64_t index = s.stats.host_samples++;
  if (index < 4) {
    s.stats.incoming_error[index] = GetLastError();
    s.stats.incoming_mxcsr[index] = _mm_getcsr();
  } else ++s.stats.fixture_errors;
  SetLastError(uint32_t(0xABCD1000u + index));
  _mm_setcsr(index % 2 ? 0x5F80u : 0x3F80u);
  if (s.config.scenario == MCLA_CW_FIXTURE_THROW) throw uint32_t(0x10203040u);
  if (s.config.scenario == MCLA_CW_FIXTURE_PLAIN_REENTRANT && !index) {
    s.stats.nested_return = s.last_observed ? PlainObserved() : PlainBaseline();
  }
  return 100 + index * 5;
}
}  // namespace fixture

// These are ONLY the original body's predetermined mutex semantics. The
// observer adds no mutex. A cached case represents another owner's release.
namespace fixture_std {
struct defer_lock_t {};
inline constexpr defer_lock_t defer_lock{};
struct mutex {
  bool try_lock() noexcept {
    auto& s = fixture::state; ++s.stats.tries;
    if (s.held) return false;
    s.held = true; ++s.owned_locks; return true;
  }
  void lock() noexcept {
    auto& s = fixture::state; ++s.stats.waits;
    if (s.owned_locks) ++s.stats.fixture_errors;
    s.held = true; ++s.owned_locks;
  }
  void unlock() noexcept {
    auto& s = fixture::state;
    if (!s.held || s.owned_locks != 1) ++s.stats.fixture_errors;
    s.held = false; if (s.owned_locks) --s.owned_locks; ++s.stats.unlocks;
  }
};
template<class Mutex> class unique_lock {
 public:
  unique_lock(Mutex& value, defer_lock_t) noexcept : mutex_(value) {}
  ~unique_lock() noexcept { if (owned_) mutex_.unlock(); }
  bool try_lock() noexcept { owned_ = mutex_.try_lock(); return owned_; }
  void lock() noexcept { mutex_.lock(); owned_ = true; }
 private:
  Mutex& mutex_; bool owned_{};
};
}  // namespace fixture_std

namespace baseline {
thread_local bool clock_no_scaling{};
thread_local std::pair<uint64_t,uint64_t> guest_tick_ratio_{5,1};
thread_local uint64_t last_host_tick_count_{95}, last_guest_tick_count_{475};
thread_local fixture_std::mutex tick_mutex_;
struct Clock { static uint64_t QueryHostTickCount() { return fixture::Sample(); } };
#define REXCVAR_GET(name) (name)
#define std fixture_std
#include "mcla_clock_update_body.inc"
#undef std
#undef REXCVAR_GET
}  // namespace baseline

using MclaClockWitnessCall = bridge::Witness;
namespace instrumented {
thread_local bool clock_no_scaling{};
thread_local std::pair<uint64_t,uint64_t> guest_tick_ratio_{5,1};
thread_local uint64_t last_host_tick_count_{95}, last_guest_tick_count_{475};
thread_local fixture_std::mutex tick_mutex_;
struct Clock { static uint64_t QueryHostTickCount() { return fixture::Sample(); } };
#define REXCVAR_GET(name) (name)
#define std fixture_std
#include "mcla_clock_update_witness_body.inc"
#undef std
#undef REXCVAR_GET
}  // namespace instrumented

namespace fixture {
uint64_t PlainObserved() { return instrumented::UpdateGuestClock(); }
uint64_t PlainBaseline() { return baseline::UpdateGuestClock(); }
uint64_t Original(bool observed) {
  auto& s = state; ++s.stats.original_queries; s.last_observed = observed;
  const auto mode = s.config.scenario;
  if (mode == MCLA_CW_FIXTURE_STOP_PENDING && s.stop_target) {
    // Stop in the wrapper-to-witness gap: the read lease already exists, but
    // neither the source helper nor the original host query has begun.
    s.stop_target->Stop();
    const uint32_t copied = s.stop_target->TrySnapshot(&pending_output);
    s.stats.pending_seen = copied == MCLA_CW_PENDING;
    if (copied != MCLA_CW_PENDING) ++s.stats.fixture_errors;
  }
  if (!s.depth && mode >= MCLA_CW_FIXTURE_NESTED_BOUND && mode <= MCLA_CW_FIXTURE_NESTED_SUPPRESSED) {
    struct Depth { uint32_t& value; explicit Depth(uint32_t& v) : value(v) { ++value; } ~Depth() { --value; } } depth(s.depth);
    auto nested = s.config.nested;
    if (mode == MCLA_CW_FIXTURE_NESTED_UNBOUND) nested.timer_invocation = 0;
    if (mode == MCLA_CW_FIXTURE_NESTED_INVALID) nested.run_low ^= 1;
    if (mode == MCLA_CW_FIXTURE_NESTED_SUPPRESSED) nested.flags = MCLA_CW_SUPPRESS;
    if (observed) s.stats.nested_return = runtime.QueryBound(&nested, &s.nested_status, [] { return Original(true); });
    else s.stats.nested_return = Original(false);
    s.stats.nested_admission = s.nested_status.admission;
    s.stats.nested_completion = s.nested_status.completion;
  }
  uint64_t result = !observed || mode == MCLA_CW_FIXTURE_MISSING
      ? baseline::UpdateGuestClock() : instrumented::UpdateGuestClock();
  if (mode == MCLA_CW_FIXTURE_DUPLICATE)
    result = observed ? instrumented::UpdateGuestClock() : baseline::UpdateGuestClock();
  if (mode == MCLA_CW_FIXTURE_THROW_AFTER_SOURCE) throw uint32_t(0x10203041u);
  return result;
}
bool Header(uint32_t version, uint32_t size, uint32_t expected) noexcept {
  return version == MCLA_CW_ABI_V1 && size == expected;
}
void Configure(const MclaClockWitnessFixtureConfigV1& config) noexcept {
  state = {}; state.config = config;
  state.stats.abi_version = MCLA_CW_ABI_V1; state.stats.struct_size = sizeof(state.stats);
  state.held = config.scenario == MCLA_CW_FIXTURE_CACHED;
  baseline::clock_no_scaling = instrumented::clock_no_scaling = config.scenario == MCLA_CW_FIXTURE_UNSCALED;
  baseline::guest_tick_ratio_ = instrumented::guest_tick_ratio_ = {5,1};
  baseline::last_host_tick_count_ = instrumented::last_host_tick_count_ = 95;
  baseline::last_guest_tick_count_ = instrumented::last_guest_tick_count_ = 475;
}
}  // namespace fixture

#define EXPORT extern "C" __declspec(dllexport)
EXPORT uint32_t MCLA_CW_CALL MclaClockWitnessInfoV1Export(MclaClockWitnessInfoV1* output) noexcept {
  if (!output || !fixture::Header(output->abi_version, output->struct_size, sizeof(*output))) return MCLA_CW_BAD_ABI;
  *output = {MCLA_CW_ABI_V1, sizeof(*output), sizeof(MclaClockWitnessBindingV1), sizeof(MclaClockWitnessStatusV1),
      sizeof(MclaClockWitnessRecordV1), sizeof(MclaClockWitnessSnapshotV1), MCLA_CW_CAPACITY_V1, 0, {0,0}};
  return MCLA_CW_OK;
}
EXPORT uint32_t MCLA_CW_CALL MclaClockWitnessArmV1Export(uint64_t high, uint64_t low) noexcept {
  return fixture::runtime.Arm(high,low) ? MCLA_CW_OK : MCLA_CW_REJECTED;
}
EXPORT uint64_t MCLA_CW_CALL MclaClockWitnessQueryBoundV1Export(
    const MclaClockWitnessBindingV1* input, MclaClockWitnessStatusV1* status) {
  return fixture::runtime.QueryBound(input,status,[] { return fixture::Original(true); });
}
EXPORT void MCLA_CW_CALL MclaClockWitnessStopV1Export() noexcept { fixture::runtime.Stop(); }
EXPORT uint32_t MCLA_CW_CALL MclaClockWitnessSnapshotV1Export(MclaClockWitnessSnapshotV1* output) noexcept {
  return fixture::runtime.TrySnapshot(output);
}
EXPORT uint32_t MCLA_CW_CALL MclaClockWitnessFixtureConfigureV1Export(const MclaClockWitnessFixtureConfigV1* input) noexcept {
  if (!input || !fixture::Header(input->abi_version,input->struct_size,sizeof(*input)) ||
      input->reserved || input->scenario > MCLA_CW_FIXTURE_THROW_AFTER_SOURCE) return MCLA_CW_BAD_ABI;
  fixture::Configure(*input); return MCLA_CW_OK;
}
EXPORT uint32_t MCLA_CW_CALL MclaClockWitnessFixtureStatsV1Export(MclaClockWitnessFixtureStatsV1* output) noexcept {
  if (!output || !fixture::Header(output->abi_version,output->struct_size,sizeof(*output))) return MCLA_CW_BAD_ABI;
  auto& s = fixture::state;
  s.stats.last_host = s.last_observed && s.config.scenario != MCLA_CW_FIXTURE_MISSING
      ? instrumented::last_host_tick_count_ : baseline::last_host_tick_count_;
  s.stats.last_guest = s.last_observed && s.config.scenario != MCLA_CW_FIXTURE_MISSING
      ? instrumented::last_guest_tick_count_ : baseline::last_guest_tick_count_;
  if (s.owned_locks) ++s.stats.fixture_errors;
  *output = s.stats; return MCLA_CW_OK;
}
EXPORT uint64_t MCLA_CW_CALL MclaClockWitnessFixtureOriginalV1Export() { return fixture::Original(false); }
EXPORT uint32_t MCLA_CW_CALL MclaClockWitnessFixturePendingV1Export(const MclaClockWitnessBindingV1* input) {
  if (!input || !fixture::pending_runtime.Arm(input->run_high,input->run_low)) return MCLA_CW_REJECTED;
  fixture::Configure({MCLA_CW_ABI_V1,sizeof(MclaClockWitnessFixtureConfigV1),MCLA_CW_FIXTURE_STOP_PENDING,0,{}});
  fixture::pending_output = {}; fixture::pending_output.abi_version = MCLA_CW_ABI_V1;
  fixture::pending_output.struct_size = sizeof(fixture::pending_output);
  fixture::state.stop_target = &fixture::pending_runtime;
  MclaClockWitnessStatusV1 status{};
  const uint64_t result = fixture::pending_runtime.QueryBound(input,&status,[] { return fixture::Original(true); });
  const uint32_t copied = fixture::pending_runtime.TrySnapshot(&fixture::pending_output);
  return result == 500 && fixture::state.stats.pending_seen && copied == MCLA_CW_OK &&
      status.completion == MCLA_CW_PUBLISHED && fixture::pending_output.row_count == 1 &&
      fixture::pending_output.published_entries == 1 ? MCLA_CW_OK : MCLA_CW_REJECTED;
}
#undef EXPORT
