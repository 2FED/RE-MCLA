// Paired C++ dynamic-CRT consumer of the SYNTHETIC DLL, never the SDK/game.
// Uses ONLY the C-layout ABI header; no recorder/runtime/TLS copy lives here.
#include "../src/mcla_clock_read_witness_abi.h"
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>
#include <xmmintrin.h>
#include <array>
#include <cstring>
#include <cwchar>
#include <iostream>
#include <thread>

namespace {
constexpr uint64_t kHigh = 0x123456789ABCDEF0ull, kLow = 0xFEDCBA9876543210ull;
unsigned checks{}, failures{};
void Check(bool ok, const char* label) {
  ++checks;
  if (!ok) { ++failures; std::cerr << "FAIL " << label << '\n'; }
}
struct Api {
  MclaClockWitnessInfoFnV1 info;
  MclaClockWitnessArmFnV1 arm;
  MclaClockWitnessQueryFnV1 query;
  MclaClockWitnessStopFnV1 stop;
  MclaClockWitnessSnapshotFnV1 snapshot;
  MclaClockWitnessFixtureConfigureFnV1 configure;
  MclaClockWitnessFixtureStatsFnV1 stats;
  MclaClockWitnessFixtureOriginalFnV1 original;
  MclaClockWitnessFixturePendingFnV1 pending;
};
template<class Function> Function Resolve(HMODULE module, const char* name) {
  auto result = reinterpret_cast<Function>(GetProcAddress(module,name));
  Check(result != nullptr, name); return result;
}
struct NativeState {
  uint32_t error, mxcsr;
  bool operator==(const NativeState&) const = default;
};
NativeState ReadState() noexcept { const uint32_t error = GetLastError(); return {error,_mm_getcsr()}; }
void WriteState(NativeState value) noexcept { SetLastError(value.error); _mm_setcsr(value.mxcsr); }
MclaClockWitnessBindingV1 Binding(uint64_t invocation, uint64_t ordinal = 0) noexcept {
  return {MCLA_CW_ABI_V1,sizeof(MclaClockWitnessBindingV1),kHigh,kLow,invocation,ordinal,
      invocation + 10000,7,31,1,0,0};
}
MclaClockWitnessFixtureConfigV1 Config(uint32_t scenario, uint64_t nested = 900) noexcept {
  return {MCLA_CW_ABI_V1,sizeof(MclaClockWitnessFixtureConfigV1),scenario,0,Binding(nested)};
}
struct Outcome {
  uint64_t value{};
  uint32_t exception{}, config_result{}, stats_result{};
  NativeState produced{};
  MclaClockWitnessStatusV1 status{};
  MclaClockWitnessFixtureStatsV1 stats{};
};
Outcome Run(const Api& api, bool observed, uint32_t scenario,
    const MclaClockWitnessBindingV1* binding, bool status = true, uint64_t nested = 900) {
  Outcome out;
  const auto config = Config(scenario,nested);
  out.config_result = api.configure(&config);
  WriteState({0x24681357u,0x1F80u});
  try { out.value = observed ? api.query(binding,status ? &out.status : nullptr) : api.original(); }
  catch (uint32_t value) { out.exception = value; }
  catch (...) { out.exception = 0xFFFFFFFFu; }
  // Capture native results BEFORE any assertion/I/O/another export call.
  out.produced = ReadState();
  out.stats.abi_version = MCLA_CW_ABI_V1; out.stats.struct_size = sizeof(out.stats);
  out.stats_result = api.stats(&out.stats);
  return out;
}
void Compare(const Outcome& a, const Outcome& b) {
  Check(a.config_result == MCLA_CW_OK && b.config_result == MCLA_CW_OK &&
      a.stats_result == MCLA_CW_OK && b.stats_result == MCLA_CW_OK, "test-only configuration/stats ABI");
  Check(a.value == b.value && a.exception == b.exception, "original result/exception unchanged across DLL boundary");
  Check(a.produced == b.produced &&
      std::memcmp(a.stats.incoming_error,b.stats.incoming_error,sizeof(a.stats.incoming_error)) == 0 &&
      std::memcmp(a.stats.incoming_mxcsr,b.stats.incoming_mxcsr,sizeof(a.stats.incoming_mxcsr)) == 0,
      "first TLS touch and success/throw preserve incoming/produced LastError/MXCSR");
  Check(a.stats.original_queries == b.stats.original_queries && a.stats.host_samples == b.stats.host_samples &&
      a.stats.tries == b.stats.tries && a.stats.waits == b.stats.waits && a.stats.unlocks == b.stats.unlocks &&
      a.stats.last_host == b.stats.last_host && a.stats.last_guest == b.stats.last_guest &&
      a.stats.nested_return == b.stats.nested_return, "original-only query count clock sample and mutex semantics");
  Check(a.stats.fixture_errors == 0 && b.stats.fixture_errors == 0, "synthetic source schedule valid");
}
Outcome Pair(const Api& api, uint32_t scenario, const MclaClockWitnessBindingV1* binding,
    uint32_t admission, uint32_t completion, uint64_t nested = 900) {
  const auto control = Run(api,false,scenario,binding,true,nested);
  const auto observed = Run(api,true,scenario,binding,true,nested);
  Compare(control,observed);
  Check(observed.status.abi_version == MCLA_CW_ABI_V1 && observed.status.struct_size == sizeof(observed.status) &&
      observed.status.admission == admission && observed.status.completion == completion,
      "query admission/completion explicit, not inferred from result");
  return observed;
}
// Static outputs: not a large automatic Snapshot and no owner concurrency.
MclaClockWitnessSnapshotV1 snapshot{}, repeated{}, pending_before{};
uint32_t CountInvocation(const MclaClockWitnessSnapshotV1& data, uint64_t invocation) {
  uint32_t count = 0;
  for (uint32_t i = 0; i < data.row_count; ++i)
    if (data.rows[i].binding.timer_invocation == invocation) ++count;
  return count;
}
}  // namespace

int wmain(int argc, wchar_t** argv) {
  if (argc != 2 || std::wcslen(argv[1]) < 4 || argv[1][1] != L':' ||
      (argv[1][2] != L'\\' && argv[1][2] != L'/')) {
    std::cerr << "One absolute owned DLL path required\n"; return 2;
  }
  const auto saved_native = ReadState();
  HMODULE module = LoadLibraryExW(argv[1],nullptr,LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_SYSTEM32);
  if (!module) { std::cerr << "FAIL owned DLL load\n"; return 1; }
  wchar_t actual[32768]{};
  const DWORD length = GetModuleFileNameW(module,actual,32768);
  Check(length && length < 32768 && CompareStringOrdinal(actual,-1,argv[1],-1,TRUE) == CSTR_EQUAL,
      "loaded exact supplied owned DLL path");
  HMODULE pinned{};
  Check(GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_PIN,
      reinterpret_cast<LPCWSTR>(module),&pinned) && pinned == module, "DLL/runtime pinned for process lifetime");
  const Api api{
      Resolve<MclaClockWitnessInfoFnV1>(module,"MclaClockWitnessInfoV1Export"),
      Resolve<MclaClockWitnessArmFnV1>(module,"MclaClockWitnessArmV1Export"),
      Resolve<MclaClockWitnessQueryFnV1>(module,"MclaClockWitnessQueryBoundV1Export"),
      Resolve<MclaClockWitnessStopFnV1>(module,"MclaClockWitnessStopV1Export"),
      Resolve<MclaClockWitnessSnapshotFnV1>(module,"MclaClockWitnessSnapshotV1Export"),
      Resolve<MclaClockWitnessFixtureConfigureFnV1>(module,"MclaClockWitnessFixtureConfigureV1Export"),
      Resolve<MclaClockWitnessFixtureStatsFnV1>(module,"MclaClockWitnessFixtureStatsV1Export"),
      Resolve<MclaClockWitnessFixtureOriginalFnV1>(module,"MclaClockWitnessFixtureOriginalV1Export"),
      Resolve<MclaClockWitnessFixturePendingFnV1>(module,"MclaClockWitnessFixturePendingV1Export")};
  if (failures) return 1;
  MclaClockWitnessInfoV1 info{MCLA_CW_ABI_V1,sizeof(info)};
  Check(api.info(&info) == MCLA_CW_OK && info.binding_size == sizeof(MclaClockWitnessBindingV1) &&
      info.status_size == sizeof(MclaClockWitnessStatusV1) && info.record_size == sizeof(MclaClockWitnessRecordV1) &&
      info.snapshot_size == sizeof(snapshot) && info.capacity == MCLA_CW_CAPACITY_V1 && !info.proof_flags,
      "frozen version size capacity and no-proof handshake");
  auto bad_info = info; bad_info.abi_version = 2;
  Check(api.info(&bad_info) == MCLA_CW_BAD_ABI && bad_info.abi_version == 2 &&
      api.info(nullptr) == MCLA_CW_BAD_ABI, "wrong ABI rejected without claiming support");

  auto binding = Binding(1);
  Pair(api,MCLA_CW_FIXTURE_NORMAL,&binding,MCLA_CW_DISABLED,MCLA_CW_NO_EVENT);
  Check(api.arm(0,0) == MCLA_CW_REJECTED && api.arm(kHigh,kLow) == MCLA_CW_OK &&
      api.arm(kHigh,kLow) == MCLA_CW_REJECTED, "one valid arm, no reset/rearm");
  snapshot.abi_version = MCLA_CW_ABI_V1; snapshot.struct_size = sizeof(snapshot);
  pending_before = snapshot;
  Check(api.snapshot(&snapshot) == MCLA_CW_PENDING && std::memcmp(&snapshot,&pending_before,sizeof(snapshot)) == 0,
      "snapshot pending and destination unchanged before Stop");

  Pair(api,MCLA_CW_FIXTURE_NORMAL,&binding,MCLA_CW_ACCEPTED,MCLA_CW_PUBLISHED);
  binding = Binding(2);
  Pair(api,MCLA_CW_FIXTURE_NORMAL,&binding,MCLA_CW_ACCEPTED,MCLA_CW_PUBLISHED);
  Pair(api,MCLA_CW_FIXTURE_NORMAL,&binding,MCLA_CW_ACCEPTED,MCLA_CW_PUBLISHED); // same caller ordinal retained
  binding = Binding(3);
  Pair(api,MCLA_CW_FIXTURE_CACHED,&binding,MCLA_CW_ACCEPTED,MCLA_CW_PUBLISHED);
  binding = Binding(4);
  Pair(api,MCLA_CW_FIXTURE_UNSCALED,&binding,MCLA_CW_ACCEPTED,MCLA_CW_PUBLISHED);
  binding = Binding(5);
  auto out = Pair(api,MCLA_CW_FIXTURE_NESTED_BOUND,&binding,MCLA_CW_ACCEPTED,MCLA_CW_PUBLISHED,105);
  Check(out.stats.nested_admission == MCLA_CW_ACCEPTED && out.stats.nested_completion == MCLA_CW_PUBLISHED,
      "nested bound call has its own explicit identity");
  const uint32_t modes[] = {MCLA_CW_FIXTURE_NESTED_UNBOUND,MCLA_CW_FIXTURE_NESTED_INVALID,MCLA_CW_FIXTURE_NESTED_SUPPRESSED};
  const uint32_t statuses[] = {MCLA_CW_UNBOUND,MCLA_CW_INVALID_BINDING,MCLA_CW_DISABLED};
  for (uint32_t i = 0; i < 3; ++i) {
    binding = Binding(6+i);
    out = Pair(api,modes[i],&binding,MCLA_CW_ACCEPTED,MCLA_CW_PUBLISHED,106+i);
    Check(out.stats.nested_admission == statuses[i] && out.stats.nested_completion == MCLA_CW_NO_EVENT,
        "rejected nested frame masks outer identity");
  }
  binding = Binding(9);
  Pair(api,MCLA_CW_FIXTURE_PLAIN_REENTRANT,&binding,MCLA_CW_ACCEPTED,MCLA_CW_PUBLISHED);
  binding = Binding(10);
  out = Pair(api,MCLA_CW_FIXTURE_THROW,&binding,MCLA_CW_ACCEPTED,MCLA_CW_ABORTED);
  Check(out.exception == 0x10203040u && (out.status.issues & (1u<<11)), "original C++ unwind retained aborted record");
  binding = Binding(11);
  Pair(api,MCLA_CW_FIXTURE_MISSING,&binding,MCLA_CW_ACCEPTED,MCLA_CW_MISSING_WITNESS);
  binding = Binding(12);
  Pair(api,MCLA_CW_FIXTURE_DUPLICATE,&binding,MCLA_CW_ACCEPTED,MCLA_CW_DUPLICATE_WITNESS);
  binding = Binding(13);
  Pair(api,MCLA_CW_FIXTURE_THROW_AFTER_SOURCE,&binding,MCLA_CW_ACCEPTED,MCLA_CW_ABORTED);
  binding = Binding(14);
  const auto no_status_control = Run(api,false,MCLA_CW_FIXTURE_NORMAL,&binding,false);
  Compare(no_status_control,Run(api,true,MCLA_CW_FIXTURE_NORMAL,&binding,false));
  Pair(api,MCLA_CW_FIXTURE_NORMAL,nullptr,MCLA_CW_UNBOUND,MCLA_CW_NO_EVENT);
  binding = Binding(15); binding.run_low ^= 1;
  Pair(api,MCLA_CW_FIXTURE_NORMAL,&binding,MCLA_CW_INVALID_BINDING,MCLA_CW_NO_EVENT);
  binding = Binding(16); binding.abi_version = 99;
  Pair(api,MCLA_CW_FIXTURE_NORMAL,&binding,MCLA_CW_INVALID_ABI,MCLA_CW_NO_EVENT);
  binding = Binding(17); binding.struct_size -= 8;
  Pair(api,MCLA_CW_FIXTURE_NORMAL,&binding,MCLA_CW_INVALID_ABI,MCLA_CW_NO_EVENT);
  binding = Binding(18); binding.reserved = 1;
  Pair(api,MCLA_CW_FIXTURE_NORMAL,&binding,MCLA_CW_INVALID_ABI,MCLA_CW_NO_EVENT);

  // Deliberate overlapping POD storage tests parse-before-status write. The
  // C++ object is a Binding; status is used only as raw output bytes afterward.
  auto alias = Binding(19);
  auto config = Config(MCLA_CW_FIXTURE_NORMAL);
  Check(api.configure(&config) == MCLA_CW_OK, "alias configuration");
  const uint64_t alias_value = api.query(&alias,reinterpret_cast<MclaClockWitnessStatusV1*>(&alias));
  MclaClockWitnessStatusV1 alias_status{};
  std::memcpy(&alias_status,&alias,sizeof(alias_status));
  Check(alias_value == 500 && alias_status.completion == MCLA_CW_PUBLISHED, "request copied before overlapping status write");

  std::array<Outcome,4> threaded{};
  std::array<std::thread,4> workers;
  for (uint32_t i = 0; i < 4; ++i) workers[i] = std::thread([&,i] {
    const auto request = Binding(1000+i,i);
    threaded[i] = Run(api,true,MCLA_CW_FIXTURE_NORMAL,&request);
  });
  for (auto& worker : workers) worker.join();
  for (const auto& result : threaded) {
    Check(result.value == 500 && result.status.completion == MCLA_CW_PUBLISHED &&
        result.stats.original_queries == 1 && result.stats.host_samples == 1 && result.stats.fixture_errors == 0,
        "DLL-owned TLS is isolated between joined consumer threads");
    Check(result.stats.incoming_error[0] == 0x24681357u && result.stats.incoming_mxcsr[0] == 0x1F80u &&
        result.produced == NativeState{0xABCD1000u,0x3F80u}, "worker first TLS touch preserves native state");
  }
  binding = Binding(2000);
  Check(api.pending(&binding) == MCLA_CW_OK && api.pending(&binding) == MCLA_CW_REJECTED,
      "separate once-only pending fixture: Stop waits for wrapper-admitted read");

  uint32_t overflow_seen = 0;
  for (uint32_t i = 0; i < 80; ++i) {
    binding = Binding(3000+i,i);
    const auto result = Run(api,true,MCLA_CW_FIXTURE_NORMAL,&binding);
    Check(result.value == 500 && result.stats.original_queries == 1 &&
        (result.status.admission == MCLA_CW_ACCEPTED || result.status.admission == MCLA_CW_OVERFLOW),
        "bounded overflow never skips or repeats original clock query");
    if (result.status.admission == MCLA_CW_OVERFLOW) ++overflow_seen;
  }
  api.stop(); api.stop();
  Check(api.snapshot(&snapshot) == MCLA_CW_OK && snapshot.row_count == MCLA_CW_CAPACITY_V1 &&
      snapshot.stored_entries == MCLA_CW_CAPACITY_V1 && snapshot.overflow_entries == overflow_seen &&
      overflow_seen > 0 && snapshot.observed_bound_reads == MCLA_CW_CAPACITY_V1 + overflow_seen,
      "fixed bounded inventory retains overflow without overwrite");
  Check(snapshot.missing_witnesses == 1 && snapshot.duplicate_witnesses == 1 &&
      snapshot.aborted_entries == 4 && snapshot.diagnostic_entries == 4 && snapshot.invalid_bindings == 2,
      "missing duplicate and thrown reads remain explicit diagnostic failures");
  Check(CountInvocation(snapshot,9) == 1, "nested read inherited an outer binding");
  Check(CountInvocation(snapshot,2) == 2 && CountInvocation(snapshot,5) == 1 && CountInvocation(snapshot,105) == 1 &&
      CountInvocation(snapshot,106) == 0 && CountInvocation(snapshot,107) == 0 && CountInvocation(snapshot,108) == 0 &&
      CountInvocation(snapshot,19) == 1, "exact caller identities retained without alias or guessed parent");
  for (uint32_t i = 0; i < snapshot.row_count; ++i) {
    const auto& row = snapshot.rows[i];
    Check(row.admission_ordinal == i+1 && row.binding.run_high == kHigh && row.binding.run_low == kLow,
        "admission order and all 128 run-token bits retained");
    if (row.binding.timer_invocation == 3)
      Check(row.branch == 2 && row.host_sample == 100 && row.pre_last_host == 95 && row.post_last_host == 95 &&
          row.returned_guest == 475, "cached branch raw sample distinct from coherent locked cache");
    if (row.binding.timer_invocation == 4)
      Check(row.branch == 3 && !(row.presence & 8u) && !row.pre_last_host && !row.post_last_host,
          "NoScaling has unavailable locked fields and no extra lock");
  }
  repeated.abi_version = MCLA_CW_ABI_V1; repeated.struct_size = sizeof(repeated);
  Check(api.snapshot(&repeated) == MCLA_CW_OK && std::memcmp(&snapshot,&repeated,sizeof(snapshot)) == 0,
      "stopped drained snapshot is byte-stable on repeated copy");
  binding = Binding(4000);
  Pair(api,MCLA_CW_FIXTURE_NORMAL,&binding,MCLA_CW_STOPPED,MCLA_CW_NO_EVENT);
  Check(api.arm(kHigh,kLow) == MCLA_CW_REJECTED && api.snapshot(&repeated) == MCLA_CW_OK &&
      std::memcmp(&snapshot,&repeated,sizeof(snapshot)) == 0, "post-Stop original executes without changing drained evidence");
  Check(!snapshot.proof_flags && !snapshot.counter_saturated && snapshot.published_entries + snapshot.aborted_entries == snapshot.row_count,
      "synthetic records only: no SDK history simulation frame FPS or runtime proof");
  WriteState(saved_native);
  std::cout << "checks=" << checks << " failures=" << failures
      << " synthetic_DLL_boundary_only SDK_integration_and_actual_clock_cause_unproven\n";
  return failures ? 1 : 0;
}
