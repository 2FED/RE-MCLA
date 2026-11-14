// Actual isolated SDK consumer, not a game/runtime/fixture or generic C++ ABI.
// Parent recipe MUST pin exact paired DLL/EXE/dependency bytes before launch.
// Path/owner checks below are NOT hashes or an arbitrary-PE security sandbox.
// One fresh process per mode; no OS input, desktop capture or file writes.
#include "../src/mcla_clock_commit_history_abi.h"
#include "../src/mcla_clock_read_witness_abi_v2.h"
#if !defined(WIN32_LEAN_AND_MEAN)
#define WIN32_LEAN_AND_MEAN
#endif
#if !defined(NOMINMAX)
#define NOMINMAX
#endif
#include <Windows.h>
#include <xmmintrin.h>
#include <array>
#include <cstring>
#include <cwchar>
#include <iostream>
#include <limits>
#include <memory>
#include <numeric>
#include <utility>

namespace {
constexpr uint64_t high = 0x123456789ABCDEF0ull, low = 0xFEDCBA9876543210ull;
unsigned checks{}, failures{}, controls{};
void Check(bool ok, const char* label) {
  ++checks; if (!ok) { ++failures; std::cerr << "FAIL " << label << '\n'; }
}
struct Native {
  uint32_t error{}, mxcsr{};
  static Native Capture() noexcept { const uint32_t error = GetLastError(); return {error, uint32_t(_mm_getcsr())}; }
  void Restore() const noexcept { SetLastError(error); _mm_setcsr(mxcsr); }
  bool operator==(const Native&) const = default;
};
template<class Operation> uint32_t Control(Operation&& operation) {
  const unsigned ordinal = controls++;
  const Native before{0x13572000u + ordinal, 0x1F80u + (ordinal % 4) * 0x2000u};
  before.Restore(); const uint32_t result = std::forward<Operation>(operation)();
  const Native after = Native::Capture();
  Check(before == after, "control export changed incoming LastError or MXCSR");
  return result;
}
using ClockQuery = uint64_t (__cdecl*)();
using SetFrequency = void (__cdecl*)(uint64_t);
using SetScalar = void (__cdecl*)(double);
struct Api {
  MclaClockHistoryInfoFnV1 hi; MclaClockHistoryArmFnV1 ha;
  MclaClockHistoryStopFnV1 hs; MclaClockHistorySnapshotFnV1 hp;
  MclaClockWitnessInfoFnV2 wi; MclaClockWitnessArmFnV2 wa;
  MclaClockWitnessQueryFnV2 wq; MclaClockWitnessStopFnV2 ws; MclaClockWitnessSnapshotFnV2 wp;
  ClockQuery query, frequency; SetFrequency set_frequency; SetScalar set_scalar;
};
template<class T> T Resolve(HMODULE module, const char* name) {
  FARPROC address = GetProcAddress(module, name); Check(address != nullptr, name);
  if (!address) return nullptr;
  HMODULE owner{}; MEMORY_BASIC_INFORMATION memory{};
  Check(GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
      reinterpret_cast<LPCWSTR>(address), &owner) && owner == module, "resolved function belongs to exact DLL not a forwarder or executable");
  const bool queried = VirtualQuery(reinterpret_cast<const void*>(address), &memory, sizeof(memory)) != 0;
  const DWORD protection = memory.Protect & 0xFFu;
  Check(queried && memory.AllocationBase == module && !(memory.Protect & (PAGE_GUARD | PAGE_NOACCESS)) &&
      (protection == PAGE_EXECUTE || protection == PAGE_EXECUTE_READ || protection == PAGE_EXECUTE_READWRITE ||
       protection == PAGE_EXECUTE_WRITECOPY), "resolved function is executable storage of exact DLL");
  return reinterpret_cast<T>(address);
}
__declspec(noinline) void ConsumerAnchor() {}
bool ExportNames(HMODULE module) {
  // Trusted already-loaded pinned target only. Bounds guard accidental format
  // drift; this is not an adversarial-memory reader or portable PE verifier.
  const auto* image = reinterpret_cast<const unsigned char*>(module);
  const auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(image);
  if (dos->e_magic != IMAGE_DOS_SIGNATURE || dos->e_lfanew <= 0 || dos->e_lfanew > 1048576) return false;
  const auto* nt = reinterpret_cast<const IMAGE_NT_HEADERS64*>(image + dos->e_lfanew);
  if (nt->Signature != IMAGE_NT_SIGNATURE || nt->OptionalHeader.Magic != IMAGE_NT_OPTIONAL_HDR64_MAGIC) return false;
  const uint64_t size = nt->OptionalHeader.SizeOfImage;
  const auto span = [&](uint64_t rva, uint64_t bytes) { return rva < size && bytes <= size - rva; };
  const auto directory = nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT];
  if (!span(directory.VirtualAddress, sizeof(IMAGE_EXPORT_DIRECTORY))) return false;
  const auto* exports = reinterpret_cast<const IMAGE_EXPORT_DIRECTORY*>(image + directory.VirtualAddress);
  if (exports->NumberOfNames > 65536 || !span(exports->AddressOfNames, uint64_t(exports->NumberOfNames) * 4)) return false;
  const auto* names = reinterpret_cast<const DWORD*>(image + exports->AddressOfNames);
  constexpr const char* allowed[]{"MclaClockHistoryInfoV1Export", "MclaClockHistoryArmV1Export",
      "MclaClockHistoryStopV1Export", "MclaClockHistorySnapshotV1Export"};
  unsigned found{};
  for (DWORD index = 0; index < exports->NumberOfNames; ++index) {
    if (!span(names[index], 1)) return false;
    const char* name = reinterpret_cast<const char*>(image + names[index]);
    const uint64_t available = size - names[index];
    // Existing SDK exports include long decorated template names; do not reject
    // unrelated legitimate names at an arbitrary 512-character cutoff.
    const size_t bound = size_t(available < 65536 ? available : 65536);
    if (!std::memchr(name, 0, bound)) return false;
    if (std::strncmp(name, "MclaClockWitnessFixture", 23) == 0) return false;
    if (std::strncmp(name, "MclaClockHistory", 16) != 0) continue;
    bool recognized = false;
    for (const char* candidate : allowed) recognized = recognized || std::strcmp(name, candidate) == 0;
    if (!recognized) return false; // Includes history Fixture, Query/reset extras.
    ++found;
  }
  return found == 4;
}
bool Load(const wchar_t* path, Api& api) {
  const size_t count = std::wcslen(path);
  if (count < 4 || count >= 32768 || path[1] != L':' || (path[2] != L'/' && path[2] != L'\\')) return false;
  std::unique_ptr<wchar_t[]> full(new wchar_t[32768]), actual(new wchar_t[32768]);
  const DWORD full_length = GetFullPathNameW(path, 32768, full.get(), nullptr);
  Check(full_length && full_length < 32768, "bounded absolute DLL path"); if (failures) return false;
  HMODULE module = LoadLibraryExW(full.get(), nullptr, LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_SYSTEM32);
  Check(module != nullptr, "restricted exact absolute SDK DLL load"); if (!module) return false;
  const DWORD length = GetModuleFileNameW(module, actual.get(), 32768);
  Check(length && length < 32768 && CompareStringOrdinal(full.get(), -1, actual.get(), -1, TRUE) == CSTR_EQUAL,
      "loaded module path equals exact supplied SDK path");
  HMODULE pinned{}, executable_owner{};
  Check(GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_PIN,
      reinterpret_cast<LPCWSTR>(module), &pinned) && pinned == module, "SDK process-lifetime pin no unload claim");
  Check(GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
      reinterpret_cast<LPCWSTR>(&ConsumerAnchor), &executable_owner) && executable_owner == GetModuleHandleW(nullptr) &&
      executable_owner != module, "consumer executable owner is separate from exact SDK");
  Check(ExportNames(module), "only four history exports and no synthetic fixture or Query reset extras");
  api = {Resolve<MclaClockHistoryInfoFnV1>(module, "MclaClockHistoryInfoV1Export"),
      Resolve<MclaClockHistoryArmFnV1>(module, "MclaClockHistoryArmV1Export"),
      Resolve<MclaClockHistoryStopFnV1>(module, "MclaClockHistoryStopV1Export"),
      Resolve<MclaClockHistorySnapshotFnV1>(module, "MclaClockHistorySnapshotV1Export"),
      Resolve<MclaClockWitnessInfoFnV2>(module, "MclaClockWitnessInfoV2Export"),
      Resolve<MclaClockWitnessArmFnV2>(module, "MclaClockWitnessArmV2Export"),
      Resolve<MclaClockWitnessQueryFnV2>(module, "MclaClockWitnessQueryBoundV2Export"),
      Resolve<MclaClockWitnessStopFnV2>(module, "MclaClockWitnessStopV2Export"),
      Resolve<MclaClockWitnessSnapshotFnV2>(module, "MclaClockWitnessSnapshotV2Export"),
      Resolve<ClockQuery>(module, "?QueryGuestTickCount@Clock@chrono@rex@@SA_KXZ"),
      Resolve<ClockQuery>(module, "?QueryHostTickFrequency@Clock@chrono@rex@@SA_KXZ"),
      Resolve<SetFrequency>(module, "?set_guest_tick_frequency@Clock@chrono@rex@@SAX_K@Z"),
      Resolve<SetScalar>(module, "?set_guest_time_scalar@Clock@chrono@rex@@SAXN@Z")};
  return !failures; // Deliberately never FreeLibrary, including failures.
}

// Global POD storage only; no automatic >640KiB aggregate or temporary.
MclaClockHistorySnapshotV1 history{}, history_repeat{}, history_before{};
MclaClockWitnessSnapshotV2 witness{}, witness_repeat{}, witness_before{};
void PrefixHistory(MclaClockHistorySnapshotV1& out) {
  std::memset(&out, 0, sizeof(out)); out.abi_version = 1; out.struct_size = sizeof(out);
}
void PrefixWitness(MclaClockWitnessSnapshotV2& out) {
  std::memset(&out, 0, sizeof(out)); out.abi_version = 2; out.struct_size = sizeof(out);
}
bool Descriptor(const Api& api) {
  MclaClockHistoryInfoV1 h{}; h.abi_version = 1; h.struct_size = sizeof(h);
  // FIRST history call/guard touch in this fresh process.
  Check(Control([&] { return api.hi(&h); }) == MCLA_CH_OK && h.abi_version == 1 && h.struct_size == 72 &&
      h.binding_version == 1 && h.binding_size == 64 && h.metadata_version == 1 && h.metadata_size == 88 &&
      h.record_version == 1 && h.record_size == 184 && h.snapshot_version == 1 && h.snapshot_size == 753784 &&
      h.rows_offset == 120 && h.capacity == 4096 && !h.proof_flags && !h.reserved &&
      !h.reserved_words[0] && !h.reserved_words[1], "cold history descriptor exact versions sizes alignment capacity and zero proofs");
  MclaClockWitnessInfoV2 w{}; w.abi_version = 2; w.struct_size = sizeof(w);
  Check(Control([&] { return api.wi(&w); }) == MCLA_CW_OK && w.abi_version == 2 && w.struct_size == 72 &&
      w.binding_version == 1 && w.status_version == 1 && w.record_version == 1 && w.snapshot_version == 2 &&
      w.binding_size == 72 && w.status_size == 24 && w.record_size == 160 && w.snapshot_size == 655480 &&
      w.snapshot_rows_offset == 120 && w.capacity == 4096 && !w.proof_flags && !w.reserved &&
      !w.reserved_words[0] && !w.reserved_words[1], "paired old V2 descriptor remains exact and independent");
  return !failures;
}
void BadAndPending(const Api& api) {
  MclaClockHistoryInfoV1 info{}; info.abi_version = 99; info.struct_size = sizeof(info); const auto old = info;
  Check(Control([&] { return api.hi(&info); }) == MCLA_CH_BAD_ABI && !std::memcmp(&info, &old, sizeof(info)), "history bad Info prefix unchanged");
  Check(Control([&] { return api.hi(nullptr); }) == MCLA_CH_BAD_ABI &&
      Control([&] { return api.hp(nullptr); }) == MCLA_CH_BAD_ABI, "history null control pointers reject");
  struct alignas(8) Small { uint32_t version, size; uint64_t guard; } small{1, 16, 0xAABBCCDDEEFF1122ull};
  const auto small_before = small;
  Check(Control([&] { return api.hp(reinterpret_cast<MclaClockHistorySnapshotV1*>(&small)); }) == MCLA_CH_BAD_ABI &&
      !std::memcmp(&small, &small_before, sizeof(small)), "history small old buffer rejects without oversized write");
  alignas(8) unsigned char unaligned[88]{}; unsigned char saved[88];
  const uint32_t prefix[]{1, 72}; std::memcpy(unaligned + 1, prefix, sizeof(prefix)); std::memcpy(saved, unaligned, sizeof(saved));
  Check(Control([&] { return api.hi(reinterpret_cast<MclaClockHistoryInfoV1*>(unaligned + 1)); }) == MCLA_CH_BAD_ABI &&
      !std::memcmp(unaligned, saved, sizeof(saved)), "history unaligned Info rejects unchanged");
  PrefixHistory(history); history.abi_version = 2; std::memcpy(&history_before, &history, sizeof(history));
  Check(Control([&] { return api.hp(&history); }) == MCLA_CH_BAD_ABI && !std::memcmp(&history, &history_before, sizeof(history)),
      "history wrong Snapshot version unchanged");
  PrefixHistory(history); std::memcpy(&history_before, &history, sizeof(history));
  Check(Control([&] { return api.hp(&history); }) == MCLA_CH_PENDING && !std::memcmp(&history, &history_before, sizeof(history)),
      "default OFF history pending output completely unchanged");
  PrefixWitness(witness); std::memcpy(&witness_before, &witness, sizeof(witness));
  Check(Control([&] { return api.wp(&witness); }) == MCLA_CW_PENDING && !std::memcmp(&witness, &witness_before, sizeof(witness)),
      "default OFF V2 pending uses its own distinct result code");
}
struct Outcome {
  MclaClockWitnessBindingV1 binding{}; MclaClockWitnessStatusV1 status{};
  Native incoming{}, produced{}; uint64_t value{}; bool bound{}, returned{}, threw{};
};
std::array<Outcome, 5> calls{};
Outcome before_arm{};
struct Setting { Native incoming{}, produced{}; uint64_t argument_bits{}; bool returned{}, threw{}; } settings[2];
MclaClockWitnessBindingV1 Binding(uint64_t timer, uint64_t read) {
  return {1, sizeof(MclaClockWitnessBindingV1), high, low, timer, read, 0, 0, 0, 0, 0, 0};
}
void Query(const Api& api, Outcome& out, unsigned index, bool bound = false, uint64_t timer = 0, uint64_t read = 0) {
  out.bound = bound; if (bound) out.binding = Binding(timer, read);
  out.incoming = {0x24681000u + index, 0x1F80u + (index % 4) * 0x2000u}; out.incoming.Restore();
  try { out.value = bound ? api.wq(&out.binding, &out.status) : api.query(); out.produced = Native::Capture(); out.returned = true; }
  catch (...) { out.produced = Native::Capture(); out.threw = true; }
  Check(out.returned && !out.threw, "actual SDK query returned without exception");
  if (bound) Check(out.status.abi_version == 1 && out.status.struct_size == 24 && out.status.admission == MCLA_CW_ACCEPTED &&
      out.status.completion == MCLA_CW_PUBLISHED && !out.status.issues && !out.status.reserved, "actual V2 query published exact raw status");
  // Original QPC/mutex code owns produced native effects. Record BOTH values;
  // no invented unchanged-LastError or unchanged-MXCSR assertion for queries.
}
bool EmptyBinding(const MclaClockHistoryBindingV1& b) {
  return !b.run_high && !b.run_low && !b.timer_invocation && !b.read_ordinal && !b.phase_call_id &&
      !b.phase_thread_epoch && !b.phase_native_thread_id && !b.phase_present && !b.present && !b.reserved;
}
bool JoinedBinding(const MclaClockHistoryBindingV1& h, const MclaClockWitnessBindingV1& w) {
  return h.present == 1 && h.run_high == w.run_high && h.run_low == w.run_low && h.timer_invocation == w.timer_invocation &&
      h.read_ordinal == w.read_ordinal && h.phase_call_id == w.phase_call_id && h.phase_thread_epoch == w.phase_thread_epoch &&
      h.phase_native_thread_id == w.phase_native_thread_id && h.phase_present == w.phase_present && !h.reserved &&
      w.abi_version == 1 && w.struct_size == 72 && !w.flags && !w.reserved;
}
bool HistoryEnvelope(uint32_t rows, bool armed) {
  return history.abi_version == 1 && history.struct_size == 753784 && history.capacity == 4096 && history.row_count == rows &&
    history.run_high == (armed ? high : 0) && history.run_low == (armed ? low : 0) && history.observed_attempts == rows &&
    history.mutex_ordinals == rows && !history.unordered_attempts && history.stored_entries == rows && history.published_entries == rows &&
    !history.aborted_entries && !history.overflow_entries && !history.diagnostic_entries && !history.saturated_rejections &&
    !history.counter_saturated && !history.counts_reserved && !history.proof_flags && !history.reserved;
}
bool WitnessEnvelope(uint32_t rows) {
  return witness.abi_version == 2 && witness.struct_size == 655480 && witness.capacity == 4096 && witness.row_count == rows &&
    witness.run_high == high && witness.run_low == low && witness.observed_bound_reads == rows && witness.stored_entries == rows &&
    witness.published_entries == rows && !witness.aborted_entries && !witness.overflow_entries && !witness.invalid_bindings &&
    !witness.diagnostic_entries && !witness.saturated_rejections && !witness.missing_witnesses && !witness.duplicate_witnesses &&
    !witness.counter_saturated && !witness.proof_flags;
}
void Unused() {
  const MclaClockHistoryRecordV1 hz{}; const MclaClockWitnessRecordV1 wz{};
  bool zero = history.row_count <= 4096 && witness.row_count <= 4096;
  for (uint32_t i = history.row_count < 4096 ? history.row_count : 4096; i < 4096; ++i)
    zero = zero && !std::memcmp(&history.rows[i], &hz, sizeof(hz));
  for (uint32_t i = witness.row_count < 4096 ? witness.row_count : 4096; i < 4096; ++i)
    zero = zero && !std::memcmp(&witness.rows[i], &wz, sizeof(wz));
  Check(zero, "all unused history and V2 wire rows zero");
}
void StopAndRepeat(const Api& api) {
  Control([&] { api.hs(); return MCLA_CH_OK; }); Control([&] { api.ws(); return MCLA_CW_OK; });
  Check(Control([&] { return api.hp(&history); }) == MCLA_CH_OK, "actual history quiescent Stop drains");
  Check(Control([&] { return api.wp(&witness); }) == MCLA_CW_OK, "actual V2 quiescent Stop drains independently");
  PrefixHistory(history_repeat); PrefixWitness(witness_repeat);
  Check(Control([&] { return api.hp(&history_repeat); }) == MCLA_CH_OK && !std::memcmp(&history, &history_repeat, sizeof(history)),
      "actual history repeated full Snapshot byte stable");
  Check(Control([&] { return api.wp(&witness_repeat); }) == MCLA_CW_OK && !std::memcmp(&witness, &witness_repeat, sizeof(witness)),
      "actual V2 repeated full Snapshot byte stable");
  Check(Control([&] { return api.ha(high, low); }) == MCLA_CH_REJECTED &&
      Control([&] { return api.wa(high, low); }) == MCLA_CW_REJECTED, "both stopped families reject rearm with their distinct codes");
  Unused();
}
void CheckQueryRow(const MclaClockHistoryRecordV1& row, const Outcome& call) {
  const auto& m = row.metadata;
  bool math = m.pre_ratio_denominator != 0;
  if (row.kind == MCLA_CH_KIND_ADVANCE && math) {
    const uint64_t delta = m.host_sample > m.pre_host ? m.host_sample - m.pre_host : 0;
    math = m.post_host == m.host_sample && m.post_guest == m.pre_guest + (delta * m.pre_ratio_numerator) / m.pre_ratio_denominator;
  } else if (row.kind == MCLA_CH_KIND_CACHED)
    math = m.pre_host == m.post_host && m.pre_guest == m.post_guest;
  else math = false;
  Check(math && m.returned_guest == m.post_guest && m.returned_guest == call.value && m.presence == MCLA_CH_QUERY_FIELDS &&
      m.pre_ratio_numerator == m.post_ratio_numerator && m.pre_ratio_denominator == m.post_ratio_denominator && !m.reserved,
      "actual query row literal branch arithmetic and exact caller return agree");
  Check(call.bound ? JoinedBinding(row.binding, call.binding) : EmptyBinding(row.binding), "actual query carries only its explicit bound identity or canonical unbound");
}
void NormalChecks(uint64_t frequency) {
  Check(HistoryEnvelope(7, true) && WitnessEnvelope(3), "exact seven history commits and three bound V2 reads without extra rows or diagnostics");
  const uint64_t n1 = 50000000ull * 10, d1 = frequency * 10, g1 = std::gcd(n1, d1);
  const uint64_t n2 = 50000000ull * 10, d2 = frequency * 20, g2 = std::gcd(n2, d2);
  const uint64_t numerators[]{1, 1, n1/g1, n1/g1, n2/g2, n2/g2, n2/g2};
  const uint64_t denominators[]{1, 1, d1/g1, d1/g1, d2/g2, d2/g2, d2/g2};
  constexpr int call_slots[]{0, 1, -1, 2, -1, 3, 4};
  for (uint32_t i = 0; i < 7; ++i) {
    const auto& row = history.rows[i]; const auto& m = row.metadata;
    Check(row.storage_ordinal == i+1 && row.mutex_ordinal == i+1 && row.ordering == MCLA_CH_ORDERING_MUTEX &&
        row.disposition == MCLA_CH_PUBLISHED && !row.issues, "source mutex order and complete raw disposition retained");
    Check(m.post_ratio_numerator == numerators[i] && m.post_ratio_denominator == denominators[i], "effective ratio equals exact frozen source integer reduction");
    if (i) {
      const auto& before = history.rows[i-1].metadata;
      Check(m.pre_host == before.post_host && m.pre_guest == before.post_guest && m.pre_ratio_numerator == before.post_ratio_numerator &&
          m.pre_ratio_denominator == before.post_ratio_denominator, "whole serial source state and ratio chain has no invented interior gap");
    } else Check(m.pre_ratio_numerator == 1 && m.pre_ratio_denominator == 1, "SDK initial effective ratio is literal D72 default one to one");
    if (call_slots[i] >= 0) CheckQueryRow(row, calls[size_t(call_slots[i])]);
    else Check(row.kind == MCLA_CH_KIND_RATIO && EmptyBinding(row.binding) && m.presence == MCLA_CH_RATIO_FIELDS &&
        m.pre_host == m.post_host && m.pre_guest == m.post_guest && !m.host_sample && !m.returned_guest && !m.reserved,
        "both setter effective ratio commits are unbound and preserve clock state");
  }
  constexpr size_t history_slots[]{1, 3, 6}, query_slots[]{1, 2, 4};
  for (size_t i = 0; i < 3; ++i) {
    const auto& v = witness.rows[i]; const auto& h = history.rows[history_slots[i]]; const auto& m = h.metadata;
    Check(v.admission_ordinal == i+1 && JoinedBinding(h.binding, v.binding) &&
        !std::memcmp(&v.binding, &calls[query_slots[i]].binding, sizeof(v.binding)) && v.host_sample == m.host_sample &&
        v.returned_guest == m.returned_guest && v.ratio_numerator == m.pre_ratio_numerator && v.ratio_denominator == m.pre_ratio_denominator &&
        v.pre_last_host == m.pre_host && v.pre_last_guest == m.pre_guest && v.post_last_host == m.post_host &&
        v.post_last_guest == m.post_guest && v.presence == 15 && v.branch == h.kind && v.disposition == MCLA_CW_PUBLISHED &&
        !v.issues, "all V2 raw fields join exact history request sample branch ratio states and return");
  }
}

void NativeJson(const Native& n) { std::cout << "{\"last_error\":" << n.error << ",\"mxcsr\":" << n.mxcsr << '}'; }
void BindingJson(const MclaClockWitnessBindingV1& b) {
  std::cout << "{\"abi_version\":" << b.abi_version << ",\"struct_size\":" << b.struct_size << ",\"run_high\":" << b.run_high
    << ",\"run_low\":" << b.run_low << ",\"timer_invocation\":" << b.timer_invocation << ",\"read_ordinal\":" << b.read_ordinal
    << ",\"phase_call_id\":" << b.phase_call_id << ",\"phase_thread_epoch\":" << b.phase_thread_epoch
    << ",\"phase_native_thread_id\":" << b.phase_native_thread_id << ",\"phase_present\":" << b.phase_present
    << ",\"flags\":" << b.flags << ",\"reserved\":" << b.reserved << '}';
}
void OutcomeJson(const Outcome& c) {
  std::cout << "{\"bound\":" << (c.bound ? "true" : "false") << ",\"returned\":" << (c.returned ? "true" : "false")
    << ",\"threw\":" << (c.threw ? "true" : "false") << ",\"value\":" << c.value << ",\"incoming\":";
  NativeJson(c.incoming); std::cout << ",\"produced\":"; NativeJson(c.produced);
  std::cout << ",\"binding\":"; BindingJson(c.binding);
  const auto& s = c.status;
  std::cout << ",\"status\":{\"abi_version\":" << s.abi_version << ",\"struct_size\":" << s.struct_size << ",\"admission\":" << s.admission
    << ",\"completion\":" << s.completion << ",\"issues\":" << s.issues << ",\"reserved\":" << s.reserved << "}}";
}
void Dump(const char* mode, uint64_t frequency, size_t call_count) {
  std::cout << "RAW_SDK_HISTORY {\"mode\":\"" << mode << "\",\"host_frequency\":" << frequency << ",\"before_arm\":";
  OutcomeJson(before_arm); std::cout << ",\"calls\":[";
  for (size_t i = 0; i < call_count; ++i) { if (i) std::cout << ','; OutcomeJson(calls[i]); }
  std::cout << "],\"settings\":[";
  const size_t setting_count = std::strcmp(mode, "normal") == 0 ? 2 : 0;
  for (size_t i = 0; i < setting_count; ++i) {
    if (i) std::cout << ',';
    std::cout << "{\"kind\":\"" << (i == 0 ? "set_guest_tick_frequency" : "set_guest_time_scalar")
      << "\",\"argument_bits\":" << settings[i].argument_bits << ",\"returned\":" << (settings[i].returned ? "true" : "false")
      << ",\"threw\":" << (settings[i].threw ? "true" : "false") << ",\"incoming\":";
    NativeJson(settings[i].incoming); std::cout << ",\"produced\":"; NativeJson(settings[i].produced); std::cout << '}';
  }
  const auto& e = history;
  std::cout << "],\"history\":{\"abi_version\":" << e.abi_version << ",\"struct_size\":" << e.struct_size << ",\"capacity\":" << e.capacity
    << ",\"row_count\":" << e.row_count << ",\"run_high\":" << e.run_high << ",\"run_low\":" << e.run_low
    << ",\"observed_attempts\":" << e.observed_attempts << ",\"mutex_ordinals\":" << e.mutex_ordinals << ",\"unordered_attempts\":" << e.unordered_attempts
    << ",\"stored_entries\":" << e.stored_entries << ",\"published_entries\":" << e.published_entries << ",\"aborted_entries\":" << e.aborted_entries
    << ",\"overflow_entries\":" << e.overflow_entries << ",\"diagnostic_entries\":" << e.diagnostic_entries << ",\"saturated_rejections\":" << e.saturated_rejections
    << ",\"counter_saturated\":" << e.counter_saturated << ",\"counts_reserved\":" << e.counts_reserved << ",\"proof_flags\":" << e.proof_flags
    << ",\"reserved\":" << e.reserved << ",\"rows\":[";
  for (uint32_t i = 0; i < e.row_count && i < 4096; ++i) {
    if (i) std::cout << ','; const auto& r = e.rows[i]; const auto& b = r.binding; const auto& m = r.metadata;
    std::cout << "{\"storage_ordinal\":" << r.storage_ordinal << ",\"mutex_ordinal\":" << r.mutex_ordinal << ",\"kind\":" << r.kind
      << ",\"ordering\":" << r.ordering << ",\"disposition\":" << r.disposition << ",\"issues\":" << r.issues
      << ",\"binding\":{\"run_high\":" << b.run_high << ",\"run_low\":" << b.run_low << ",\"timer_invocation\":" << b.timer_invocation
      << ",\"read_ordinal\":" << b.read_ordinal << ",\"phase_call_id\":" << b.phase_call_id << ",\"phase_thread_epoch\":" << b.phase_thread_epoch
      << ",\"phase_native_thread_id\":" << b.phase_native_thread_id << ",\"phase_present\":" << b.phase_present << ",\"present\":" << b.present
      << ",\"reserved\":" << b.reserved << "},\"metadata\":{\"host_sample\":" << m.host_sample << ",\"returned_guest\":" << m.returned_guest
      << ",\"pre_host\":" << m.pre_host << ",\"pre_guest\":" << m.pre_guest << ",\"post_host\":" << m.post_host << ",\"post_guest\":" << m.post_guest
      << ",\"pre_ratio_numerator\":" << m.pre_ratio_numerator << ",\"pre_ratio_denominator\":" << m.pre_ratio_denominator
      << ",\"post_ratio_numerator\":" << m.post_ratio_numerator << ",\"post_ratio_denominator\":" << m.post_ratio_denominator
      << ",\"presence\":" << m.presence << ",\"reserved\":" << m.reserved << "}}";
  }
  const auto& v = witness;
  std::cout << "]},\"v2\":{\"abi_version\":" << v.abi_version << ",\"struct_size\":" << v.struct_size << ",\"capacity\":" << v.capacity
    << ",\"row_count\":" << v.row_count << ",\"run_high\":" << v.run_high << ",\"run_low\":" << v.run_low
    << ",\"observed_bound_reads\":" << v.observed_bound_reads << ",\"stored_entries\":" << v.stored_entries << ",\"published_entries\":" << v.published_entries
    << ",\"aborted_entries\":" << v.aborted_entries << ",\"overflow_entries\":" << v.overflow_entries << ",\"invalid_bindings\":" << v.invalid_bindings
    << ",\"diagnostic_entries\":" << v.diagnostic_entries << ",\"saturated_rejections\":" << v.saturated_rejections
    << ",\"missing_witnesses\":" << v.missing_witnesses << ",\"duplicate_witnesses\":" << v.duplicate_witnesses
    << ",\"counter_saturated\":" << v.counter_saturated << ",\"proof_flags\":" << v.proof_flags << ",\"rows\":[";
  for (uint32_t i = 0; i < v.row_count && i < 4096; ++i) {
    if (i) std::cout << ','; const auto& r = v.rows[i];
    std::cout << "{\"admission_ordinal\":" << r.admission_ordinal << ",\"binding\":"; BindingJson(r.binding);
    std::cout << ",\"host_sample\":" << r.host_sample << ",\"returned_guest\":" << r.returned_guest << ",\"ratio_numerator\":" << r.ratio_numerator
      << ",\"ratio_denominator\":" << r.ratio_denominator << ",\"pre_last_host\":" << r.pre_last_host << ",\"pre_last_guest\":" << r.pre_last_guest
      << ",\"post_last_host\":" << r.post_last_host << ",\"post_last_guest\":" << r.post_last_guest << ",\"presence\":" << r.presence
      << ",\"branch\":" << r.branch << ",\"disposition\":" << r.disposition << ",\"issues\":" << r.issues << '}';
  }
  std::cout << "]},\"engineering_proof\":{\"sdk_build_ready\":false,\"ffi_general\":false,\"source_routed_coverage\":false,"
    "\"complete_clock_history\":false,\"mutex_caller_discipline\":false,\"native_state_preservation\":false,"
    "\"original_semantics\":false,\"actual_clock_cause\":false,\"simulation\":false,\"fps\":false}}\n";
}
} // namespace

int wmain(int argc, wchar_t** argv) {
  if (argc != 3 || (std::wcscmp(argv[2], L"normal") && std::wcscmp(argv[2], L"stop-before-arm"))) {
    std::cerr << "Exact absolute SDK DLL path and normal|stop-before-arm required\n"; return 2;
  }
  const Native initial = Native::Capture();
  struct Restore { Native value; ~Restore() { value.Restore(); } } restore{initial};
  Api api{}; if (!Load(argv[1], api) || !Descriptor(api)) return 1;
  BadAndPending(api); if (failures) return 1;
  Query(api, before_arm, 99); // Actual original call while BOTH observers are OFF.
  const uint64_t frequency = api.frequency();
  Check(frequency && frequency <= std::numeric_limits<uint64_t>::max()/20, "host frequency permits nonzero exact source ratio denominators");
  if (failures) return 1;
  if (!std::wcscmp(argv[2], L"stop-before-arm")) {
    Control([&] { api.hs(); return MCLA_CH_OK; });
    Check(Control([&] { return api.ha(high, low); }) == MCLA_CH_REJECTED &&
        Control([&] { return api.hp(&history); }) == MCLA_CH_OK && HistoryEnvelope(0, false), "cold Stop-before-Arm yields empty irreversible history");
    Check(Control([&] { return api.wa(high, low); }) == MCLA_CW_OK, "stopped history does not stop independent V2 Arm");
    if (failures) return 1; // Never execute a Clock call after failed lifecycle admission.
    Query(api, calls[0], 0, true, 99, 0);
    StopAndRepeat(api);
    Check(HistoryEnvelope(0, false) && WitnessEnvelope(1) && witness.rows[0].returned_guest == calls[0].value &&
        !std::memcmp(&witness.rows[0].binding, &calls[0].binding, sizeof(calls[0].binding)), "V2 work cannot resurrect or contaminate stopped history");
    Check(witness.rows[0].admission_ordinal == 1 && witness.rows[0].presence == 15 &&
        (witness.rows[0].branch == 1 || witness.rows[0].branch == 2) && witness.rows[0].disposition == MCLA_CW_PUBLISHED &&
        witness.rows[0].ratio_numerator == 1 && witness.rows[0].ratio_denominator == 1 && !witness.rows[0].issues,
        "cold independent V2 retains the actual default scaled query tuple");
    Dump("stop-before-arm", frequency, 1);
  } else {
    Check(Control([&] { return api.ha(0, 0); }) == MCLA_CH_REJECTED &&
        Control([&] { return api.ha(high, low); }) == MCLA_CH_OK &&
        Control([&] { return api.wa(high, low); }) == MCLA_CW_OK &&
        Control([&] { return api.ha(high, low); }) == MCLA_CH_REJECTED &&
        Control([&] { return api.wa(high, low); }) == MCLA_CW_REJECTED, "both families once-arm same full run token with distinct result codes");
    std::memcpy(&history_before, &history, sizeof(history));
    Check(Control([&] { return api.hp(&history); }) == MCLA_CH_PENDING && !std::memcmp(&history, &history_before, sizeof(history)),
        "armed pre-Stop history remains pending unchanged");
    if (failures) return 1; // Keep setters/queries behind successful exact paired Arm.
    Query(api, calls[0], 0); Query(api, calls[1], 1, true, 7, 0);
    settings[0].argument_bits = 50000000; settings[0].incoming = {0x35791000u, 0x3F80u}; settings[0].incoming.Restore();
    try { api.set_frequency(50000000); settings[0].produced = Native::Capture(); settings[0].returned = true; }
    catch (...) { settings[0].produced = Native::Capture(); settings[0].threw = true; }
    Query(api, calls[2], 2, true, 7, 1);
    const double scalar = 0.5; std::memcpy(&settings[1].argument_bits, &scalar, sizeof(scalar));
    settings[1].incoming = {0x35792000u, 0x5F80u}; settings[1].incoming.Restore();
    try { api.set_scalar(scalar); settings[1].produced = Native::Capture(); settings[1].returned = true; }
    catch (...) { settings[1].produced = Native::Capture(); settings[1].threw = true; }
    Query(api, calls[3], 3); Query(api, calls[4], 4, true, 8, 0);
    Check(settings[0].returned && !settings[0].threw && settings[1].returned && !settings[1].threw, "both actual SDK setters returned serially without exception");
    StopAndRepeat(api); NormalChecks(frequency); Dump("normal", frequency, 5);
  }
  std::cout << "checks=" << checks << " failures=" << failures << " controls=" << controls
    << " actual_private_sdk_prerequisite_only no_Runtime_game_input_history_health_simulation_FPS_cause_claim\n";
  return failures ? 1 : 0;
}
