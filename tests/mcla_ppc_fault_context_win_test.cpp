// Synthetic owned PPC contexts only: no game, guest heap, windows or files.
// Include-source access exposes only the diagnostic reader seam to this test.
#include "../src/mcla_ppc_fault_context_win.cpp"

#include <array>
#include <atomic>
#include <cstring>
#include <iostream>
#include <thread>
#include <type_traits>

namespace {
namespace f = mcla::ppc_fault;
namespace d = f::detail;
std::atomic<unsigned> checks{}, failures{};
void Check(bool value, const char* label) {
  ++checks;
  if (!value) { ++failures; std::cerr << "FAIL " << label << '\n'; }
}
static_assert(!std::is_copy_constructible_v<f::Scope> && !std::is_move_constructible_v<f::Scope>);
static_assert(!std::is_copy_assignable_v<f::Scope> && !std::is_move_assignable_v<f::Scope>);
constexpr std::array<size_t, 11> offsets = {
  offsetof(PPCContext, r1), offsetof(PPCContext, r3), offsetof(PPCContext, r4),
  offsetof(PPCContext, r5), offsetof(PPCContext, r26), offsetof(PPCContext, r27),
  offsetof(PPCContext, r28), offsetof(PPCContext, r31), offsetof(PPCContext, lr),
  offsetof(PPCContext, guest_pc), offsetof(PPCContext, current_function)};
struct Injection {
  const PPCContext* context = nullptr;
  unsigned calls = 0;
  int bad_field = -1;
  int mode = 0; // false, short, throw, overreported
} ;
thread_local Injection injection;
BOOL WINAPI Reader(HANDLE process, LPCVOID source, LPVOID target, SIZE_T bytes, SIZE_T* copied) {
  const unsigned i = injection.calls++;
  Check(i < offsets.size(), "at most eleven reads, no retries");
  if (i >= offsets.size()) return FALSE;
  Check(process == GetCurrentProcess(), "only self-process handle");
  Check(source == reinterpret_cast<const unsigned char*>(injection.context) + offsets[i],
        "exact fixed field address, no pointer chasing");
  Check(bytes == (i < 9 ? 8u : 4u), "exact field width");
  std::memcpy(target, source, bytes);
  *copied = bytes;
  SetLastError(0xDEAD);
  _mm_setcsr(0x3F80);
  if (int(i) == injection.bad_field || injection.bad_field == -2) {
    if (injection.mode == 1) { *copied = bytes - 1; return TRUE; }
    if (injection.mode == 2) throw 7;
    if (injection.mode == 3) { *copied = bytes + 1; return TRUE; }
    return FALSE;
  }
  return TRUE;
}
PPCContext Context(uint64_t tag) {
  PPCContext c{};
  c.r1.u64 = tag + 1; c.r3.u64 = tag + 3; c.r4.u64 = tag + 4; c.r5.u64 = tag + 5;
  c.r26.u64 = tag + 26; c.r27.u64 = tag + 27; c.r28.u64 = tag + 28; c.r31.u64 = tag + 31;
  c.lr = tag + 99; c.guest_pc = 0x827607E8; c.current_function = 0x827600A0;
  return c;
}
void Setup(const PPCContext& c) { injection = {}; injection.context = &c; }
f::PpcFaultRecord Read(const PPCContext& c) {
  Setup(c);
  f::PpcFaultRecord r{};
  d::CaptureUsing(r, Reader);
  return r;
}
void EqualFields(const f::PpcFaultRecord& r, const PPCContext& c) {
  for (size_t i = 0; i < offsets.size(); ++i) {
    uint64_t expected = 0;
    std::memcpy(&expected, reinterpret_cast<const unsigned char*>(&c) + offsets[i], i < 9 ? 8 : 4);
    Check(r.observed_fields[i] == expected, "observed field is raw host memory value");
  }
}
bool IsStatus(const f::PpcFaultRecord& r, f::Status status) {
  return r.status == static_cast<uint32_t>(status);
}
} // namespace

int main() {
  const unsigned original_mxcsr = _mm_getcsr();
  PPCContext first = Context(100), second = Context(1000);
  f::PpcFaultRecord record{};
  Check(record.schema == 1 && IsStatus(record, f::Status::Disabled) && record.read_mask == 0,
        "default wire is safely disabled");
  Setup(first);
  d::CaptureUsing(record, Reader);
  Check(IsStatus(record, f::Status::Disabled) && injection.calls == 0, "default OFF performs no reads");
  {
    f::Scope disabled(9, first);
    d::CaptureUsing(record, Reader);
    Check(IsStatus(record, f::Status::Disabled), "disabled scope stays disabled");
    SetLastError(876); _mm_setcsr(0x9FC0);
    f::Enable();
    Check(GetLastError() == 876 && _mm_getcsr() == 0x9FC0, "Enable preserves error and FP");
    d::CaptureUsing(record, Reader);
    Check(IsStatus(record, f::Status::Absent) && record.requested_mask == 0 && injection.calls == 0,
          "enable never retroactively admits disabled scope");
  }
  d::CaptureUsing(record, Reader);
  Check(IsStatus(record, f::Status::Absent) && record.thread_id == 0, "enabled without scope is absent");
  f::Enable(); // idempotent, no reset
  {
    const auto first_before = first;
    SetLastError(112); _mm_setcsr(0x5F80);
    f::Scope outer(3, first);
    Check(GetLastError() == 112 && _mm_getcsr() == 0x5F80, "constructor preserves error and FP");
    record = Read(first);
    Check(IsStatus(record, f::Status::Complete) && record.schema == 1, "full capture complete");
    Check(record.thread_id == GetCurrentThreadId() && record.wrapper_entry.kind == 3, "thread and wrapper kind");
    Check(record.wrapper_entry.caller_lr == 199 && record.wrapper_entry.r3 == 103 &&
          record.wrapper_entry.r4 == 104 && record.wrapper_entry.r5 == 105, "exact enclosing entry fields");
    Check(record.requested_mask == f::kAllFields && record.read_mask == f::kAllFields &&
          record.requested_bytes == 80 && record.read_bytes == 80 && injection.calls == 11, "bounded exact counts");
    Check(GetLastError() == 112 && _mm_getcsr() == 0x5F80, "capture contains reader LastError and FP clobber");
    EqualFields(record, first);
    Check(std::memcmp(&first, &first_before, sizeof(first)) == 0, "full context unchanged by scope and capture");

    first.r3.u64 = UINT64_MAX; first.r28.u64 = 0x8000000000000000ull;
    record = Read(first);
    Check(record.wrapper_entry.r3 == 103 && record.observed_fields[1] == UINT64_MAX &&
          record.observed_fields[6] == 0x8000000000000000ull, "entry immutable, fault-memory values may change");
    {
      f::Scope inner(0, second); // zero kind retained, not interpreted as a phase ID
      record = Read(second);
      Check(record.wrapper_entry.kind == 0 && record.wrapper_entry.r3 == 1003, "inner distinct context masks outer");
      EqualFields(record, second);
    }
    record = Read(first);
    Check(record.wrapper_entry.kind == 3 && record.wrapper_entry.r3 == 103, "normal nested exit restores parent");
    try { f::Scope inner(7, second); throw 1; } catch (...) {}
    record = Read(first);
    Check(record.wrapper_entry.kind == 3, "exception unwinds lexical top");

    std::thread worker([&] {
      Setup(second);
      f::PpcFaultRecord other{};
      d::CaptureUsing(other, Reader);
      Check(IsStatus(other, f::Status::Absent) && injection.calls == 0, "other thread cannot see parent's TLS");
      f::Scope own(88, second);
      other = Read(second);
      Check(other.wrapper_entry.kind == 88 && other.thread_id == GetCurrentThreadId() &&
            other.thread_id != record.thread_id, "worker has independent TLS and native TID");
    });
    worker.join();
    record = Read(first);
    Check(record.wrapper_entry.kind == 3, "worker lifetime never modifies main TLS");

    for (int mode = 0; mode < 4; ++mode) {
      for (int failed : {0, 6, 10}) {
        Setup(first); injection.mode = mode; injection.bad_field = failed;
        SetLastError(444); _mm_setcsr(0x7F80);
        d::CaptureUsing(record, Reader);
        Check(IsStatus(record, f::Status::Partial) && record.read_mask == (f::kAllFields ^ (1u << failed)),
              "false/short/throw/overreported read has exact missing field");
        Check(record.observed_fields[failed] == 0 && record.read_bytes == (failed < 9 ? 72u : 76u),
              "failed field clears data and contributes no accepted bytes");
        Check(injection.calls == 11 && GetLastError() == 444 && _mm_getcsr() == 0x7F80,
              "failure keeps fixed work and restores error/FP");
      }
    }
    Setup(first); injection.bad_field = -2;
    d::CaptureUsing(record, Reader);
    Check(IsStatus(record, f::Status::Failed) && record.read_mask == 0 && record.read_bytes == 0,
          "all reads failed explicitly");
    for (auto value : record.observed_fields) Check(value == 0, "no stale failed data");
    d::CaptureUsing(record, nullptr);
    Check(IsStatus(record, f::Status::Failed) && record.requested_bytes == 80 && record.read_bytes == 0,
          "null diagnostic reader clears prior snapshot");
    // Exercise the public selection too, but only against our owned context.
    f::Capture(record);
    Check(IsStatus(record, f::Status::Complete), "public capture uses fixed self RPM");
    EqualFields(record, first);
    const auto before_end = first;
    record = Read(first);
    Check(std::memcmp(&first, &before_end, sizeof(first)) == 0, "all context fields remain untouched");
    SetLastError(555); _mm_setcsr(0x1F80);
  }
  Check(GetLastError() == 555 && _mm_getcsr() == 0x1F80, "destructor preserves error and FP");
  Setup(first); d::CaptureUsing(record, Reader);
  Check(IsStatus(record, f::Status::Absent) && injection.calls == 0 && record.wrapper_entry.kind == 0,
        "completed invocation leaves no stale record or address");
  uintptr_t address = 123;
  Check(!d::Address(0, {0, 8}, address) && address == 0, "null address rejected");
  Check(!d::Address(UINTPTR_MAX - 2, {4, 8}, address) && address == 0, "offset overflow rejected");
  Check(!d::Address(UINTPTR_MAX - 2, {0, 8}, address) && address == 0, "width overflow rejected");
  Check(!d::Address(100, {0, 0}, address) && address == 0, "zero-width rejected");
  Check(d::Address(UINTPTR_MAX - 7, {0, 8}, address) && address == UINTPTR_MAX - 7,
        "last nonwrapping bounded range accepted by arithmetic only");
  _mm_setcsr(original_mxcsr);
  std::cout << checks.load() << " checks, " << failures.load() << " failures\n";
  return failures ? 1 : 0;
}
