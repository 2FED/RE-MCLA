#include "../src/mcla_step_timer_memory_win.h"

#include <algorithm>
#include <cstring>
#include <iostream>
#include <type_traits>
#include <utility>

namespace memory = mcla::step_timer_memory;
static_assert(sizeof(uintptr_t) == 8);
static_assert(memory::kBytes == 108);
static_assert(std::is_same_v<decltype(memory::State::timer), mcla::timer_journal::State>);
static_assert(std::is_same_v<decltype(memory::State::reciprocal_bits), uint32_t>);
static_assert(std::is_same_v<decltype(memory::State::count), uint32_t>);
static_assert(std::is_same_v<decltype(memory::State::saved_unsplit_bits), uint32_t>);
static_assert(std::is_same_v<decltype(memory::State::split_state), uint8_t>);
static_assert(std::is_trivially_copyable_v<memory::State>);

namespace {
unsigned checks = 0, failures = 0;
void Check(bool yes, const char* name) {
  ++checks;
  if (!yes) { ++failures; std::cerr << "FAIL " << name << '\n'; }
}
struct InjectedFailure {};
struct Injection {
  unsigned calls = 0;
  BOOL success = TRUE;
  SIZE_T count = memory::kBytes;
  bool throws = false;
  uintptr_t expected_source = 0;
  std::array<uint8_t, memory::kBytes> bytes{};
} injected;
BOOL WINAPI CopyInjected(HANDLE process, LPCVOID source, LPVOID target, SIZE_T requested,
                         SIZE_T* count) {
  ++injected.calls;
  Check(process == GetCurrentProcess(), "test seam always receives self handle");
  Check(reinterpret_cast<uintptr_t>(source) == injected.expected_source, "test seam receives exact translated source");
  Check(target != nullptr && count != nullptr && requested == memory::kBytes,
        "test seam exact bounded destination/count");
  if (!target || !count || requested != memory::kBytes) return FALSE;
  std::memcpy(target, injected.bytes.data(), injected.bytes.size());
  *count = injected.count;
  SetLastError(0xBAADF00Du);
  if (injected.throws) throw InjectedFailure{};
  return injected.success;
}
void StoreWord(std::array<uint8_t, memory::kBytes>& bytes, uint32_t at, uint32_t value) {
  for (unsigned i = 0; i < 4; ++i) bytes[at+i] = uint8_t(value >> ((3-i)*8));
}
}

int main() {
  std::array<uint8_t, memory::kBytes> bytes{};
  for (size_t i = 0; i < bytes.size(); ++i) bytes[i] = uint8_t(i);
  const auto original_bytes = bytes;
  const auto decoded = memory::Decode(bytes);
  Check(decoded.timer == mcla::timer_memory::Decode(bytes), "old allowlist decoded unchanged from same bytes");
  Check(decoded.reciprocal_bits == 0x0C0D0E0Fu, "raw reciprocal BE word at12");
  Check(decoded.count == 0x4C4D4E4Fu, "raw count BE word at76");
  Check(decoded.saved_unsplit_bits == 0x50515253u, "raw saved BE word at80");
  Check(decoded.split_state == 73, "raw state byte at73");
  Check(bytes == original_bytes, "Decode never changes source bytes");
  Check(!memory::kStateCoherenceProven && !memory::kObjectLifetimeProven &&
        !memory::kSimulationCorrectnessProven && !memory::kNativeAdmitted, "no correctness or lifetime admission");

  std::array<bool, memory::kBytes> selected{};
  for (auto at : mcla::timer_journal::kWordOffsets)
    for (unsigned i = 0; i < 4; ++i) selected[at+i] = true;
  for (auto at : mcla::timer_journal::kByteOffsets) selected[at] = true;
  for (auto at : mcla::timer_journal::kQwordOffsets)
    for (unsigned i = 0; i < 8; ++i) selected[at+i] = true;
  for (auto at : memory::kWordOffsets)
    for (unsigned i = 0; i < 4; ++i) selected[at+i] = true;
  selected[memory::kByteOffset] = true;
  auto gaps = bytes;
  for (size_t i = 0; i < gaps.size(); ++i) if (!selected[i]) gaps[i] ^= 0xFF;
  Check(memory::Decode(gaps) == decoded, "all unused prefix/gap bytes excluded");
  for (auto value : {0u, 0xFFFFFFFFu, 0x7F800001u, 0x80000000u}) {
    auto opaque = bytes;
    for (auto at : memory::kWordOffsets) StoreWord(opaque, at, value);
    opaque[memory::kByteOffset] = 0xFF;
    const auto result = memory::Decode(opaque);
    Check(result.reciprocal_bits == value && result.count == value && result.saved_unsplit_bits == value,
          "zero/max/NaN-like/negative-zero-like raw bits not interpreted");
    Check(result.split_state == 0xFF, "state byte not bool normalized");
    Check(result.timer == decoded.timer, "added fields do not alter old field projection");
  }

  // Deterministic partial/failure tests avoid relying on which count a specific
  // Windows version reports when an owned range crosses an unreadable page.
  constexpr uintptr_t fake_base = 0x1000;
  constexpr uint32_t fake_guest = 0x2000;
  injected = {}; injected.bytes = bytes; injected.expected_source = 0x3000;
  memory::State result = decoded;
  SetLastError(0x19701001u);
  Check(memory::detail::ReadUsing(fake_base, fake_guest, result, CopyInjected) && result == decoded,
        "successful injected exact full copy");
  Check(injected.calls == 1 && GetLastError() == 0x19701001u, "single copy and preserved last error");
  for (const auto count : {SIZE_T(0), SIZE_T(memory::kBytes-1), SIZE_T(memory::kBytes+1)}) {
    injected.calls = 0; injected.count = count; result = decoded; SetLastError(0x19701002u);
    Check(!memory::detail::ReadUsing(fake_base, fake_guest, result, CopyInjected) && result == memory::State{},
          "short/zero/overreported copy clears every field");
    Check(injected.calls == 1 && GetLastError() == 0x19701002u, "bad count no retry and preserves last error");
  }
  injected.calls = 0; injected.count = memory::kBytes; injected.success = FALSE;
  result = decoded; SetLastError(0x19701003u);
  Check(!memory::detail::ReadUsing(fake_base, fake_guest, result, CopyInjected) && result == memory::State{},
        "failed full-count copy clears every field");
  Check(injected.calls == 1 && GetLastError() == 0x19701003u, "failed reader no retry and preserves last error");
  injected.calls = 0; injected.success = TRUE; injected.throws = true;
  result = decoded; SetLastError(0x19701004u);
  Check(!memory::detail::ReadUsing(fake_base, fake_guest, result, CopyInjected) && result == memory::State{},
        "throwing injected diagnostic copy clears every field");
  Check(injected.calls == 1 && GetLastError() == 0x19701004u, "thrown reader no retry and preserves last error");
  injected.calls = 0; result = decoded; SetLastError(0x19701005u);
  Check(!memory::detail::ReadUsing(fake_base, fake_guest, result, nullptr) && result == memory::State{},
        "null reader rejected and cleared");
  Check(injected.calls == 0 && GetLastError() == 0x19701005u, "null reader no callback and preserves last error");
  for (const auto& pair : {std::pair<uintptr_t,uint32_t>{0, fake_guest}, {fake_base, 0},
       {UINTPTR_MAX-5, fake_guest}, {fake_base, UINT32_MAX}, {fake_base, 0xDFFFFFFFu}}) {
    result = decoded; SetLastError(0x19701006u);
    Check(!memory::detail::ReadUsing(pair.first, pair.second, result, CopyInjected) && result == memory::State{},
          "existing Address rejection clears all new and old fields");
    Check(injected.calls == 0 && GetLastError() == 0x19701006u, "invalid range never invokes reader");
  }

  SYSTEM_INFO info{}; GetSystemInfo(&info);
  auto* pages = static_cast<uint8_t*>(VirtualAlloc(nullptr, size_t(info.dwPageSize)*2,
      MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
  Check(pages != nullptr, "owned allocation");
  if (!pages) return 1;
  std::memcpy(pages, bytes.data(), bytes.size());
  const auto address = reinterpret_cast<uintptr_t>(pages);
  Check(address > 0x1000, "owned low-region base representable");
  if (address > 0x1000) {
    const auto base = address-0x1000; result = {}; SetLastError(0x19702001u);
    Check(memory::Read(base, 0x1000, result) && result == decoded, "actual one self RPM copy");
    Check(GetLastError() == 0x19702001u, "actual success preserves last error");
    Check(std::equal(bytes.begin(), bytes.end(), pages), "actual read never changes owned source memory");
    result = decoded; SetLastError(0x19702002u);
    Check(!memory::Read(base, UINT32_MAX, result) && result == memory::State{}, "actual invalid range clears whole state");
    Check(GetLastError() == 0x19702002u, "actual invalid range preserves last error");
    DWORD previous = 0;
    const bool protected_page = VirtualProtect(pages+info.dwPageSize, info.dwPageSize, PAGE_NOACCESS, &previous) != FALSE;
    Check(protected_page, "owned unreadable page");
    if (protected_page) {
      result = decoded; SetLastError(0x19702003u);
      Check(!memory::Read(base, 0x1000+info.dwPageSize-50, result) && result == memory::State{},
            "actual crossing failure fault contained and clears all fields");
      Check(GetLastError() == 0x19702003u, "actual crossing failure preserves last error");
      result = decoded; SetLastError(0x19702004u);
      Check(!memory::Read(base, 0x1000+info.dwPageSize, result) && result == memory::State{},
            "actual unreadable page failure without pointer dereference fallback");
      Check(GetLastError() == 0x19702004u, "actual unreadable failure preserves last error");
      Check(std::equal(bytes.begin(), bytes.end(), pages), "failed read never changes owned readable source");
    }
  }
  Check(address > 0xE0001000ull, "owned high-region base representable");
  if (address > 0xE0001000ull) {
    result = {}; SetLastError(0x19702005u);
    Check(memory::Read(address-0xE0001000ull, 0xE0000000u, result) && result == decoded,
          "actual existing high-region offset translation");
    Check(GetLastError() == 0x19702005u, "actual high-region success preserves last error");
  }
  Check(bytes == original_bytes, "all tests retain original source array");
  Check(VirtualFree(pages, 0, MEM_RELEASE) != FALSE, "owned allocation released");
  std::cout << "checks=" << checks << " failures=" << failures << '\n';
  return failures ? 1 : 0;
}
