// Fresh private SDK-DLL consumer only: no game/runtime creation, configuration
// setters, fixture exports, OS input, clock-based performance test or file I/O.
// A successful run checks THESE bound reads, not complete SDK clock history,
// guest timing correctness, frame joins, simulation, FPS or the real miss cause.
// The source route calls Clock::QueryGuestTickCount once; this consumer has no
// original-call counter and cannot dynamically prove that implementation fact.
#include "../src/mcla_clock_read_witness_abi_v2.h"
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>
#include <xmmintrin.h>
#include <array>
#include <cstring>
#include <cwchar>
#include <iostream>
#include <latch>
#include <thread>
#include <utility>

namespace {
constexpr uint64_t kHigh = 0x2C1307FEDCBA9876ull, kLow = 0xA5671234890BCDEFull;
constexpr size_t kCalls = 64;
unsigned checks{}, failures{}, controls{};
void Check(bool ok, const char* label) {
  ++checks; if (!ok) { ++failures; std::cerr << "FAIL " << label << '\n'; }
}
struct Native {
  uint32_t error, mxcsr;
  static Native Capture() noexcept { const auto error = GetLastError(); return {error,_mm_getcsr()}; }
  void Restore() const noexcept { SetLastError(error); _mm_setcsr(mxcsr); }
  bool operator==(const Native&) const = default;
};
struct Api {
  MclaClockWitnessInfoFnV2 info;
  MclaClockWitnessArmFnV2 arm;
  MclaClockWitnessQueryFnV2 query;
  MclaClockWitnessStopFnV2 stop;
  MclaClockWitnessSnapshotFnV2 snapshot;
};
template<class T> T Resolve(HMODULE module, const char* name) {
  const auto result = reinterpret_cast<T>(GetProcAddress(module,name));
  Check(result != nullptr,name); return result;
}
template<class Operation> uint32_t Control(Operation&& operation, const char* label) {
  const unsigned index = controls++;
  const Native incoming{0x24681000u+index,0x1F80u+uint32_t(index%4)*0x2000u};
  incoming.Restore();
  const uint32_t result = std::forward<Operation>(operation)();
  const auto produced = Native::Capture();
  Check(produced == incoming,label); return result;
}
MclaClockWitnessBindingV1 Binding(uint64_t timer, uint64_t read) noexcept {
  // No phase census runs here: absent optional phase is exactly canonical zero.
  return {MCLA_CW_ABI_V1,sizeof(MclaClockWitnessBindingV1),kHigh,kLow,timer,read,0,0,0,0,0,0};
}
struct Call {
  MclaClockWitnessBindingV1 binding{};
  MclaClockWitnessStatusV1 status{};
  Native incoming{}, produced{};
  uint64_t value{};
  uint32_t returned{}, threw{};
};
Call Query(const Api& api, MclaClockWitnessBindingV1 binding, unsigned index) {
  Call output; output.binding = binding;
  output.incoming = {0x35790000u+index,0x1F80u+uint32_t(index%4)*0x2000u};
  output.incoming.Restore();
  try { output.value = api.query(&output.binding,&output.status); output.returned = 1; }
  catch (...) { output.threw = 1; }
  output.produced = Native::Capture();  // Before assertions, printing or exports.
  return output;
}
// All >640KiB buffers have static storage and exactly one main-thread owner.
MclaClockWitnessSnapshotV2 snapshot{}, repeat{}, unchanged{};
std::array<Call,kCalls> calls{};
bool ValidInfo(const MclaClockWitnessInfoV2& info) noexcept {
  return info.abi_version == 2 && info.struct_size == 72 &&
      info.binding_version == 1 && info.status_version == 1 && info.record_version == 1 && info.snapshot_version == 2 &&
      info.binding_size == 72 && info.status_size == 24 && info.record_size == 160 && info.snapshot_size == 655480 &&
      info.snapshot_rows_offset == 120 && info.capacity == 4096 && !info.proof_flags && !info.reserved &&
      !info.reserved_words[0] && !info.reserved_words[1];
}
void CheckCall(const Call& call) {
  Check(call.returned && !call.threw && call.status.abi_version == 1 && call.status.struct_size == 24 &&
      call.status.admission == MCLA_CW_ACCEPTED && call.status.completion == MCLA_CW_PUBLISHED &&
      !call.status.issues && !call.status.reserved,"SDK bound query returned explicit published status without diagnostic");
  Check(call.incoming.mxcsr == call.produced.mxcsr,"SDK query preserves tested incoming MXCSR rounding/status bits");
  // QueryPerformanceCounter/original mutex code owns its observable LastError
  // effects. Record those effects below; no invented unchanged-error assertion.
}
bool Arithmetic(const MclaClockWitnessRecordV1& row) noexcept {
  constexpr uint32_t host = 1, returned = 2, ratio = 4, locked = 8;
  if ((row.presence & (host|returned|ratio)) != (host|returned|ratio) ||
      (row.presence & ~(host|returned|ratio|locked)) || !row.ratio_numerator || !row.ratio_denominator) return false;
  if (row.branch == 1) {
    if (!(row.presence & locked)) return false;
    const uint64_t delta = row.host_sample > row.pre_last_host ? row.host_sample-row.pre_last_host : 0;
    // EXACT source uint64 multiplication/addition, including defined unsigned
    // wrap BEFORE division. No widened arithmetic/tolerance/reordered history.
    const uint64_t guest_delta = (delta*row.ratio_numerator)/row.ratio_denominator;
    return row.post_last_host == row.host_sample && row.post_last_guest == row.pre_last_guest+guest_delta &&
        row.returned_guest == row.post_last_guest;
  }
  if (row.branch == 2) return (row.presence & locked) && row.pre_last_host == row.post_last_host &&
      row.pre_last_guest == row.post_last_guest && row.returned_guest == row.post_last_guest;
  if (row.branch == 3) return !(row.presence & locked) && !row.pre_last_host && !row.pre_last_guest &&
      !row.post_last_host && !row.post_last_guest &&
      row.returned_guest == (row.host_sample*row.ratio_numerator)/row.ratio_denominator;
  return false;
}
}  // namespace

int wmain(int argc, wchar_t** argv) {
  if (argc != 2 || std::wcslen(argv[1]) < 4 || argv[1][1] != L':' ||
      (argv[1][2] != L'\\' && argv[1][2] != L'/')) {
    std::cerr << "One exact absolute private SDK DLL path required\n"; return 2;
  }
  const auto initial = Native::Capture();
  const HMODULE module = LoadLibraryExW(argv[1],nullptr,LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_SYSTEM32);
  if (!module) { std::cerr << "FAIL owned SDK DLL load Win32=" << GetLastError() << '\n'; return 1; }
  wchar_t actual[32768]{};
  const DWORD length = GetModuleFileNameW(module,actual,32768);
  Check(length && length < 32768 && CompareStringOrdinal(actual,-1,argv[1],-1,TRUE) == CSTR_EQUAL,
      "loaded exact supplied private SDK DLL path");
  HMODULE pinned{};
  Check(GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_PIN,
      reinterpret_cast<LPCWSTR>(module),&pinned) && pinned == module,"SDK DLL/runtime process-lifetime pinned, no unload claim");
  const Api api{
      Resolve<MclaClockWitnessInfoFnV2>(module,"MclaClockWitnessInfoV2Export"),
      Resolve<MclaClockWitnessArmFnV2>(module,"MclaClockWitnessArmV2Export"),
      Resolve<MclaClockWitnessQueryFnV2>(module,"MclaClockWitnessQueryBoundV2Export"),
      Resolve<MclaClockWitnessStopFnV2>(module,"MclaClockWitnessStopV2Export"),
      Resolve<MclaClockWitnessSnapshotFnV2>(module,"MclaClockWitnessSnapshotV2Export")};
  Check(!GetProcAddress(module,"MclaClockWitnessFixtureConfigureV2Export") &&
      !GetProcAddress(module,"MclaClockWitnessFixtureStatsV2Export") &&
      !GetProcAddress(module,"MclaClockWitnessFixtureOriginalV2Export") &&
      !GetProcAddress(module,"MclaClockWitnessFixtureGateV2Export"),"actual SDK target has no synthetic fixture exports");
  if (failures) return 1;

  MclaClockWitnessInfoV2 info{}; info.abi_version = 2; info.struct_size = sizeof(info);
  const bool compatible = Control([&] { return api.info(&info); },"Info preserves native sentinel") == MCLA_CW_OK && ValidInfo(info);
  Check(compatible,
      "strict V2 descriptor versions sizes row offset capacity and proof flags");
  if (!compatible || failures) return 1;  // Never query/arm an incompatible ABI.
  auto bad_info = info; bad_info.abi_version = 1; const auto bad_version_before = bad_info;
  Check(Control([&] { return api.info(&bad_info); },"bad Info version preserves native sentinel") == MCLA_CW_BAD_ABI &&
      std::memcmp(&bad_info,&bad_version_before,sizeof(bad_info)) == 0,"bad Info version fails closed unchanged");
  bad_info = info; --bad_info.struct_size; const auto bad_size_before = bad_info;
  Check(Control([&] { return api.info(&bad_info); },"bad Info size preserves native sentinel") == MCLA_CW_BAD_ABI &&
      std::memcmp(&bad_info,&bad_size_before,sizeof(bad_info)) == 0,"bad Info size fails closed unchanged");
  Check(Control([&] { return api.info(nullptr); },"null Info preserves native sentinel") == MCLA_CW_BAD_ABI,
      "null Info rejected");
  struct SmallEnvelope { uint32_t version, size; uint64_t canary; } small{2,sizeof(SmallEnvelope),0xAABBCCDDEEFF1122ull};
  const auto small_before = small;
  Check(Control([&] { return api.snapshot(reinterpret_cast<MclaClockWitnessSnapshotV2*>(&small)); },
      "small Snapshot envelope preserves native sentinel") == MCLA_CW_BAD_ABI &&
      std::memcmp(&small,&small_before,sizeof(small)) == 0,"undersized Snapshot rejected before oversized read/write");
  Check(Control([&] { return api.snapshot(nullptr); },"null Snapshot preserves native sentinel") == MCLA_CW_BAD_ABI,
      "null Snapshot rejected");
  snapshot.abi_version = 1; snapshot.struct_size = sizeof(snapshot);
  std::memcpy(&unchanged,&snapshot,sizeof(snapshot));
  Check(Control([&] { return api.snapshot(&snapshot); },"bad Snapshot version preserves native sentinel") == MCLA_CW_BAD_ABI &&
      std::memcmp(&snapshot,&unchanged,sizeof(snapshot)) == 0,"wrong Snapshot version fails closed unchanged");
  snapshot.abi_version = 2; snapshot.struct_size = sizeof(snapshot)-8;
  std::memcpy(&unchanged,&snapshot,sizeof(snapshot));
  Check(Control([&] { return api.snapshot(&snapshot); },"bad Snapshot size preserves native sentinel") == MCLA_CW_BAD_ABI &&
      std::memcmp(&snapshot,&unchanged,sizeof(snapshot)) == 0,"wrong Snapshot size fails closed unchanged");
  if (failures) return 1;  // Envelope/native-state failures are not runtime admission.

  const auto disabled = Query(api,Binding(9999,0),100);
  Check(disabled.returned && !disabled.threw && disabled.status.abi_version == 1 && disabled.status.struct_size == 24 &&
      disabled.status.admission == MCLA_CW_DISABLED && disabled.status.completion == MCLA_CW_NO_EVENT &&
      !disabled.status.issues && !disabled.status.reserved,"SDK observer defaults OFF; returned clock value is not row/count proof");
  Check(disabled.produced.mxcsr == disabled.incoming.mxcsr,"default-OFF query preserves incoming MXCSR");
  Check(Control([&] { return api.arm(0,0); },"zero Arm preserves native sentinel") == MCLA_CW_REJECTED &&
      Control([&] { return api.arm(kHigh,kLow); },"valid Arm preserves native sentinel") == MCLA_CW_OK &&
      Control([&] { return api.arm(kHigh,kLow); },"repeated Arm preserves native sentinel") == MCLA_CW_REJECTED,
      "SDK recorder accepts full128 run once without reset");
  snapshot.abi_version = 2; snapshot.struct_size = sizeof(snapshot);
  std::memcpy(&unchanged,&snapshot,sizeof(snapshot));
  Check(Control([&] { return api.snapshot(&snapshot); },"pending Snapshot preserves native sentinel") == MCLA_CW_PENDING &&
      std::memcmp(&snapshot,&unchanged,sizeof(snapshot)) == 0,"pre-Stop Snapshot pending without partial copy");

  // Exact caller identities exercise repeated invocations/read0+read1; these
  // are NOT forced low32 retries or proof that a guest timer executed here.
  for (unsigned i = 0; i < 32; ++i) calls[i] = Query(api,Binding(1+i/2,i%2),i);
  std::latch ready(2), release(1);
  std::array<std::thread,2> workers;
  bool started = true;
  try {
    for (unsigned lane = 0; lane < 2; ++lane) workers[lane] = std::thread([&,lane] {
      ready.count_down(); release.wait();
      for (unsigned i = 0; i < 16; ++i) {
        const unsigned slot = 32+lane*16+i;
        calls[slot] = Query(api,Binding(100+lane,i),slot);
      }
    });
  } catch (...) { started = false; }
  if (started) ready.wait();
  release.count_down();
  for (auto& worker : workers) if (worker.joinable()) worker.join();
  Check(started,"two ordinary SDK caller threads created and joined");
  for (const auto& call : calls) CheckCall(call);

  Control([&] { api.stop(); return MCLA_CW_OK; },"Stop preserves native sentinel");
  Check(Control([&] { return api.snapshot(&snapshot); },"drained Snapshot preserves native sentinel") == MCLA_CW_OK,
      "quiescent stopped SDK recorder drains nonblockingly");
  Check(snapshot.abi_version == 2 && snapshot.struct_size == 655480 && snapshot.capacity == 4096 &&
      snapshot.row_count == kCalls && snapshot.run_high == kHigh && snapshot.run_low == kLow &&
      snapshot.observed_bound_reads == kCalls && snapshot.stored_entries == kCalls && snapshot.published_entries == kCalls &&
      !snapshot.aborted_entries && !snapshot.overflow_entries && !snapshot.invalid_bindings && !snapshot.diagnostic_entries &&
      !snapshot.saturated_rejections && !snapshot.missing_witnesses && !snapshot.duplicate_witnesses &&
      !snapshot.counter_saturated && !snapshot.proof_flags,"exact64-row bound SDK inventory without overflow aborts or fabricated default-OFF row");

  std::array<bool,kCalls> seen{};
  unsigned advances{}, cached{}, unscaled{};
  const uint32_t bounded_rows = snapshot.row_count <= 4096 ? snapshot.row_count : 4096;
  for (uint32_t i = 0; i < bounded_rows; ++i) {
    const auto& row = snapshot.rows[i]; size_t match = kCalls; unsigned matches{};
    for (size_t c = 0; c < kCalls; ++c)
      if (std::memcmp(&row.binding,&calls[c].binding,sizeof(row.binding)) == 0) { match = c; ++matches; }
    Check(matches == 1 && match < kCalls && !seen[match],"each raw SDK identity matches exactly one original call, never time-nearest");
    if (matches == 1 && match < kCalls) {
      seen[match] = true;
      Check(row.returned_guest == calls[match].value && row.issues == calls[match].status.issues &&
          row.disposition == 2,"raw SDK return/disposition/issues match that exact query status and return");
    }
    Check(row.admission_ordinal == i+1 && row.binding.run_high == kHigh && row.binding.run_low == kLow &&
        row.binding.abi_version == 1 && row.binding.struct_size == 72 && !row.binding.flags && !row.binding.reserved,
        "raw admission ordinal and all128 run bits retained");
    Check(Arithmetic(row),"actual SDK branch raw coherent state satisfies exact source unsigned64 arithmetic");
    // Consumer invokes no setters or cvar initialization. Frozen SDK defaults:
    // scaling enabled, ratio1/1. A surprise is a failure, not a silent rebase.
    Check(row.ratio_numerator == 1 && row.ratio_denominator == 1 && (row.branch == 1 || row.branch == 2),
        "SDK clock configuration remains frozen default scaling and1to1 ratio");
    advances += row.branch == 1; cached += row.branch == 2; unscaled += row.branch == 3;
    std::cout << "SDK_ROW admission=" << row.admission_ordinal << " timer=" << row.binding.timer_invocation
        << " read=" << row.binding.read_ordinal << " branch=" << row.branch << " presence=" << row.presence
        << " host=" << row.host_sample << " guest=" << row.returned_guest << " numerator=" << row.ratio_numerator
        << " denominator=" << row.ratio_denominator << " preHost=" << row.pre_last_host << " preGuest=" << row.pre_last_guest
        << " postHost=" << row.post_last_host << " postGuest=" << row.post_last_guest << " issues=" << row.issues << '\n';
  }
  for (const bool present : seen) Check(present,"all64 exact call identities retained without filtering");
  bool unused_zero = snapshot.row_count <= 4096;
  const MclaClockWitnessRecordV1 zero{};
  for (uint32_t i = bounded_rows; i < 4096; ++i)
    unused_zero = unused_zero && std::memcmp(&snapshot.rows[i],&zero,sizeof(zero)) == 0;
  Check(unused_zero,"all unused wire rows explicitly zeroed");
  repeat.abi_version = 2; repeat.struct_size = sizeof(repeat);
  Check(Control([&] { return api.snapshot(&repeat); },"repeat Snapshot preserves native sentinel") == MCLA_CW_OK &&
      std::memcmp(&snapshot,&repeat,sizeof(snapshot)) == 0,"quiescent SDK Snapshot stable on repeated copy");
  const auto stopped = Query(api,Binding(5000,0),101);
  Check(stopped.returned && !stopped.threw && stopped.status.admission == MCLA_CW_STOPPED &&
      stopped.status.completion == MCLA_CW_NO_EVENT && stopped.produced.mxcsr == stopped.incoming.mxcsr,
      "post-Stop clock query returns without admitting a new row");
  Control([&] { api.stop(); return MCLA_CW_OK; },"repeat Stop preserves native sentinel");
  Check(Control([&] { return api.arm(kHigh,kLow); },"post-Stop Arm preserves native sentinel") == MCLA_CW_REJECTED &&
      Control([&] { return api.snapshot(&repeat); },"post-Stop Snapshot preserves native sentinel") == MCLA_CW_OK &&
      std::memcmp(&snapshot,&repeat,sizeof(snapshot)) == 0,"post-Stop operations do not mutate retained SDK evidence");
  unsigned last_error_changed{};
  for (size_t i = 0; i < kCalls; ++i) {
    const auto& call = calls[i]; last_error_changed += call.incoming.error != call.produced.error;
    std::cout << "SDK_CALL slot=" << i << " timer=" << call.binding.timer_invocation << " read=" << call.binding.read_ordinal
        << " value=" << call.value << " admission=" << call.status.admission << " completion=" << call.status.completion
        << " LastErrorIn=" << call.incoming.error << " LastErrorOut=" << call.produced.error
        << " MXCSRIn=" << call.incoming.mxcsr << " MXCSROut=" << call.produced.mxcsr << '\n';
  }
  std::cout << "SDK_UNBOUND disabledValue=" << disabled.value << " disabledLastErrorIn=" << disabled.incoming.error
      << " disabledLastErrorOut=" << disabled.produced.error << " stoppedValue=" << stopped.value
      << " stoppedLastErrorIn=" << stopped.incoming.error << " stoppedLastErrorOut=" << stopped.produced.error << '\n';
  initial.Restore();
  std::cout << "checks=" << checks << " failures=" << failures << " advance=" << advances << " cached=" << cached
      << " unscaled=" << unscaled << " boundLastErrorChanges=" << last_error_changed
      << " SDK_bound_read_prerequisite_only original_callcount_unmeasured complete_history_frame_simulation_FPS_cause_unproven\n";
  return failures ? 1 : 0;
}
