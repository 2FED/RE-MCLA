// Actual paired dynamic-CRT synthetic DLL consumer. No recorder copy in EXE.
#include "../src/mcla_clock_read_witness_abi_v2.h"
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>
#include <xmmintrin.h>
#include <array>
#include <chrono>
#include <cstring>
#include <cwchar>
#include <iostream>
#include <thread>
namespace {
constexpr uint64_t high = 0x123456789ABCDEF0ull,low = 0xFEDCBA9876543210ull;
unsigned checks{},failures{};
void Check(bool ok,const char* what) { ++checks; if (!ok) { ++failures; std::cerr << "FAIL " << what << '\n'; } }
using Gate = uint32_t (MCLA_CW_CALL *)(uint32_t);
struct Api {
  MclaClockWitnessInfoFnV2 info;
  MclaClockWitnessArmFnV2 arm;
  MclaClockWitnessQueryFnV2 query;
  MclaClockWitnessStopFnV2 stop;
  MclaClockWitnessSnapshotFnV2 snapshot;
  MclaClockWitnessFixtureConfigureFnV1 configure;
  MclaClockWitnessFixtureStatsFnV1 stats;
  MclaClockWitnessFixtureOriginalFnV1 original;
  Gate gate;
};
template<class T> T Get(HMODULE module,const char* name) {
  auto f = reinterpret_cast<T>(GetProcAddress(module,name)); Check(f != nullptr,name); return f;
}
struct Native { uint32_t error,mxcsr; bool operator==(const Native&) const = default; };
Native Read() noexcept { const auto e = GetLastError(); return {e,_mm_getcsr()}; }
void Write(Native s) noexcept { SetLastError(s.error); _mm_setcsr(s.mxcsr); }
MclaClockWitnessBindingV1 Binding(uint64_t n) noexcept {
  return {1,sizeof(MclaClockWitnessBindingV1),high,low,n,n+100, n+1000,7,31,1,0,0};
}
struct Outcome {
  uint64_t value{}; uint32_t exception{},configured{},stats_ok{};
  Native produced{};
  MclaClockWitnessStatusV1 status{};
  MclaClockWitnessFixtureStatsV1 stats{};
};
Outcome Run(const Api& api,uint64_t n,uint32_t scenario,bool observed = true) {
  Outcome out;
  const MclaClockWitnessFixtureConfigV1 c{1,sizeof(c),scenario,0,Binding(n+100000)};
  out.configured = api.configure(&c);
  const auto request = Binding(n);
  Write({0x24681357u,0x1F80u});
  try { out.value = observed ? api.query(&request,&out.status) : api.original(); }
  catch (uint32_t e) { out.exception = e; }
  catch (...) { out.exception = 0xFFFFFFFFu; }
  out.produced = Read();
  out.stats.abi_version = 1; out.stats.struct_size = sizeof(out.stats);
  out.stats_ok = api.stats(&out.stats); return out;
}
void Compare(const Outcome& a,const Outcome& b) {
  Check(a.configured == 0 && b.configured == 0 && a.stats_ok == 0 && b.stats_ok == 0,"fixture configure/stats handshake");
  Check(a.value == b.value && a.exception == b.exception,"large bridge result/throw matches frozen original");
  Check(a.produced == b.produced &&
      std::memcmp(a.stats.incoming_error,b.stats.incoming_error,sizeof(a.stats.incoming_error)) == 0 &&
      std::memcmp(a.stats.incoming_mxcsr,b.stats.incoming_mxcsr,sizeof(a.stats.incoming_mxcsr)) == 0,
      "large bridge incoming and produced LastError/MXCSR match original");
  Check(a.stats.original_queries == b.stats.original_queries && a.stats.host_samples == b.stats.host_samples &&
      a.stats.tries == b.stats.tries && a.stats.waits == b.stats.waits && a.stats.unlocks == b.stats.unlocks &&
      a.stats.last_host == b.stats.last_host && a.stats.last_guest == b.stats.last_guest &&
      !a.stats.fixture_errors && !b.stats.fixture_errors,"frozen clock query and mutex semantics unchanged");
}
// No >640KiB automatic objects or aggregate reset temporaries.
MclaClockWitnessSnapshotV2 snapshot{},repeat{},pending_before{};
MclaClockWitnessSnapshotV1 old_small{},old_before{};
}
int wmain(int argc,wchar_t** argv) {
  if (argc != 2 || std::wcslen(argv[1]) < 4 || argv[1][1] != L':' ||
      (argv[1][2] != L'\\' && argv[1][2] != L'/')) return 2;
  const Native initial = Read();
  const auto module = LoadLibraryExW(argv[1],nullptr,LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_SYSTEM32);
  if (!module) { std::cerr << "FAIL owned V2 DLL load\n"; return 1; }
  wchar_t actual[32768]{};
  const auto length = GetModuleFileNameW(module,actual,32768);
  Check(length && length < 32768 && CompareStringOrdinal(actual,-1,argv[1],-1,TRUE) == CSTR_EQUAL,"exact supplied V2 DLL module");
  HMODULE pinned{};
  Check(GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_PIN,
      reinterpret_cast<LPCWSTR>(module),&pinned) && pinned == module,"V2 runtime process pinned, no unload claim");
  const Api api{
    Get<MclaClockWitnessInfoFnV2>(module,"MclaClockWitnessInfoV2Export"),
    Get<MclaClockWitnessArmFnV2>(module,"MclaClockWitnessArmV2Export"),
    Get<MclaClockWitnessQueryFnV2>(module,"MclaClockWitnessQueryBoundV2Export"),
    Get<MclaClockWitnessStopFnV2>(module,"MclaClockWitnessStopV2Export"),
    Get<MclaClockWitnessSnapshotFnV2>(module,"MclaClockWitnessSnapshotV2Export"),
    Get<MclaClockWitnessFixtureConfigureFnV1>(module,"MclaClockWitnessFixtureConfigureV2Export"),
    Get<MclaClockWitnessFixtureStatsFnV1>(module,"MclaClockWitnessFixtureStatsV2Export"),
    Get<MclaClockWitnessFixtureOriginalFnV1>(module,"MclaClockWitnessFixtureOriginalV2Export"),
    Get<Gate>(module,"MclaClockWitnessFixtureGateV2Export")};
  if (failures) return 1;
  Check(GetProcAddress(module,"MclaClockWitnessQueryBoundV1Export") == nullptr &&
      GetProcAddress(module,"MclaClockWitnessSnapshotV1Export") == nullptr,"V1 export family not silently routed to V2 runtime");
  MclaClockWitnessInfoV2 info{}; info.abi_version = 2; info.struct_size = sizeof(info);
  Check(api.info(&info) == 0 && info.binding_version == 1 && info.status_version == 1 && info.record_version == 1 &&
      info.snapshot_version == 2 && info.binding_size == 72 && info.status_size == 24 && info.record_size == 160 &&
      info.snapshot_size == 655480 && info.snapshot_rows_offset == 120 && info.capacity == 4096 &&
      !info.proof_flags && !info.reserved && !info.reserved_words[0] && !info.reserved_words[1],"explicit V2 versions sizes capacity offsets no-proof handshake");
  const auto info_before = info; info.abi_version = 1;
  const auto bad_info = info;
  Check(api.info(&info) == MCLA_CW_BAD_ABI && std::memcmp(&info,&bad_info,sizeof(info)) == 0 &&
      api.info(nullptr) == MCLA_CW_BAD_ABI,"wrong-version/null V2 descriptor unchanged");
  info = info_before; info.struct_size -= 8; const auto size_info = info;
  Check(api.info(&info) == MCLA_CW_BAD_ABI && std::memcmp(&info,&size_info,sizeof(info)) == 0,"wrong-size V2 descriptor unchanged");
  MclaClockWitnessInfoV1 legacy_info{1,sizeof(MclaClockWitnessInfoV1),0,0,0,0,0,0,{0,0}};
  const auto legacy_before = legacy_info;
  Check(api.info(reinterpret_cast<MclaClockWitnessInfoV2*>(&legacy_info)) == MCLA_CW_BAD_ABI &&
      std::memcmp(&legacy_info,&legacy_before,sizeof(legacy_info)) == 0,"small V1 descriptor rejected without oversized write");
  old_small.abi_version = 1; old_small.struct_size = sizeof(old_small); old_before = old_small;
  Check(api.snapshot(reinterpret_cast<MclaClockWitnessSnapshotV2*>(&old_small)) == MCLA_CW_BAD_ABI &&
      std::memcmp(&old_small,&old_before,sizeof(old_small)) == 0,"old small V1 buffer rejected without oversized write");
  old_small.abi_version = 2; old_before = old_small;
  Check(api.snapshot(reinterpret_cast<MclaClockWitnessSnapshotV2*>(&old_small)) == MCLA_CW_BAD_ABI &&
      std::memcmp(&old_small,&old_before,sizeof(old_small)) == 0 && api.snapshot(nullptr) == MCLA_CW_BAD_ABI,
      "small size and null V2 buffers rejected before copy");
  const auto disabled = Run(api,999,MCLA_CW_FIXTURE_NORMAL);
  Check(disabled.status.admission == MCLA_CW_DISABLED && disabled.value == 500,"V2 defaults off, original still executes");
  Check(api.arm(0,0) == MCLA_CW_REJECTED && api.arm(high,low) == 0 && api.arm(high,low) == MCLA_CW_REJECTED,"V2 one arm no reset");
  snapshot.abi_version = 2; snapshot.struct_size = sizeof(snapshot);
  std::memcpy(&pending_before,&snapshot,sizeof(snapshot));
  Check(api.snapshot(&snapshot) == MCLA_CW_PENDING && std::memcmp(&snapshot,&pending_before,sizeof(snapshot)) == 0,"V2 pre-Stop snapshot pending unchanged");
  for (uint32_t mode : {MCLA_CW_FIXTURE_NORMAL,MCLA_CW_FIXTURE_THROW,MCLA_CW_FIXTURE_NESTED_INVALID}) {
    const uint64_t n = mode == MCLA_CW_FIXTURE_NORMAL ? 1 : mode == MCLA_CW_FIXTURE_THROW ? 2 : 3;
    const auto base = Run(api,n,mode,false),observed = Run(api,n,mode);
    Compare(base,observed);
    Check(observed.status.admission == MCLA_CW_ACCEPTED && observed.status.completion ==
        (mode == MCLA_CW_FIXTURE_THROW ? MCLA_CW_ABORTED : MCLA_CW_PUBLISHED),"V2 exact success/abort status");
    if (mode == MCLA_CW_FIXTURE_NESTED_INVALID)
      Check(observed.stats.nested_admission == MCLA_CW_INVALID_ABI && observed.stats.nested_completion == MCLA_CW_NO_EVENT,
          "invalid inner V1-capacity runtime shadows outer V2 binding");
  }
  // Source-site negative control: primary may mutate copied 4093 bound to 4092.
  // The exact-4096 inventory assertion below must reject that missing row.
  for (uint64_t i = 3; i < 4093; ++i) {
    const auto out = Run(api,i+1,MCLA_CW_FIXTURE_NORMAL);
    Check(out.status.admission == MCLA_CW_ACCEPTED && out.status.completion == MCLA_CW_PUBLISHED && out.value == 500,
        "all rows beyond 64 and 1024 admitted before 4096 capacity");
    if (i == 287 || i == 1024) Check(out.stats.original_queries == 1 && out.stats.host_samples == 1,
        "288 and greater-than-1024 actual DLL reads retained");
  }
  std::array<Outcome,4> outcomes{};
  std::array<std::thread,4> threads;
  for (uint32_t i=0;i<4;++i) threads[i] = std::thread([&,i] { outcomes[i] = Run(api,10000+i,MCLA_CW_FIXTURE_STOP_PENDING); });
  const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
  while (api.gate(0) != 4 && std::chrono::steady_clock::now() < deadline) std::this_thread::yield();
  const bool all_entered = api.gate(0) == 4;
  Check(all_entered,"four concurrent last-slot original calls entered bounded rendezvous");
  const auto overflow = Run(api,20000,MCLA_CW_FIXTURE_NORMAL);
  Check(overflow.status.admission == MCLA_CW_OVERFLOW && overflow.value == 500,"overflow preserves original result without overwrite");
  api.stop();
  Check(api.snapshot(&snapshot) == MCLA_CW_PENDING && std::memcmp(&snapshot,&pending_before,sizeof(snapshot)) == 0,
      "Stop with final accepted leases pending cannot publish partial inventory");
  api.gate(1);
  for (auto& thread : threads) thread.join();
  uint32_t accepted{},rejected{};
  for (const auto& out : outcomes) {
    accepted += out.status.admission == MCLA_CW_ACCEPTED;
    rejected += out.status.admission == MCLA_CW_OVERFLOW;
    Check(out.value == 500 && out.stats.original_queries == 1 && out.stats.host_samples == 1 &&
        out.produced == Native{0xABCD1000u,0x3F80u},"concurrent accepted/overflow queries preserve result and native state");
  }
  Check(accepted == 3 && rejected == 1,"three final slots and one explicit concurrent overflow");
  Check(api.snapshot(&snapshot) == 0 && snapshot.row_count == 4096 && snapshot.capacity == 4096 &&
      snapshot.observed_bound_reads == 4098 && snapshot.stored_entries == 4096 && snapshot.overflow_entries == 2,
      "exact 4096-row complete stored inventory plus explicit overflow no overwrite");
  Check(snapshot.aborted_entries == 1 && snapshot.published_entries == 4095 && snapshot.diagnostic_entries == 1 &&
      !snapshot.missing_witnesses && !snapshot.duplicate_witnesses && !snapshot.invalid_bindings,
      "throw and cross-capacity masking inventory exact");
  for (uint32_t i=0;i<snapshot.row_count;++i) {
    const auto& row = snapshot.rows[i];
    Check(row.admission_ordinal == i+1 && row.binding.run_high == high && row.binding.run_low == low &&
        row.binding.abi_version == 1 && row.binding.struct_size == 72,"all 4096 row identities preserved admission order");
    if (i < 4093) Check(row.binding.timer_invocation == i+1,"pre-race slots retain exact identity without overwrite");
    else Check(row.binding.timer_invocation >= 10000 && row.binding.timer_invocation <= 10003,"last concurrent slots retained, not overflow identity");
  }
  repeat.abi_version = 2; repeat.struct_size = sizeof(repeat);
  Check(api.snapshot(&repeat) == 0 && std::memcmp(&snapshot,&repeat,sizeof(snapshot)) == 0,"large drained snapshot byte-stable repeat");
  Check(!snapshot.proof_flags && !snapshot.counter_saturated,"overflow remains INCONCLUSIVE, no clock/FPS/simulation proof");
  const auto stopped = Run(api,30000,MCLA_CW_FIXTURE_NORMAL);
  api.stop();
  Check(stopped.status.admission == MCLA_CW_STOPPED && stopped.value == 500 && api.arm(high,low) == MCLA_CW_REJECTED &&
      api.snapshot(&repeat) == 0 && std::memcmp(&snapshot,&repeat,sizeof(snapshot)) == 0,"post-Stop original executes and evidence remains frozen");
  Write(initial);
  std::cout << "checks=" << checks << " failures=" << failures << " synthetic_V2_DLL_only actual_SDK_and_FPS_unproven\n";
  return failures ? 1 : 0;
}
