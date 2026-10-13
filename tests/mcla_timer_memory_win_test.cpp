#include "../src/mcla_timer_memory_win.h"
#include <iostream>

namespace {
unsigned checks = 0, failures = 0;
void Check(bool yes, const char* name) {
  ++checks;
  if (!yes) { ++failures; std::cerr << "FAIL " << name << '\n'; }
}
}
int main() {
  namespace memory = mcla::timer_memory;
  std::array<uint8_t, memory::kBytes> bytes{};
  for (size_t i = 0; i < bytes.size(); ++i) bytes[i] = uint8_t(i);
  const auto decoded = memory::Decode(bytes);
  Check(decoded.words[0] == 0x08090A0Bu, "word endian");
  Check(decoded.words.back() == 0x68696A6Bu, "last word endian");
  Check(decoded.bytes == std::array<uint8_t, 3>{56,58,72}, "byte fields");
  Check(decoded.qwords[0] == 0x4041424344454647ull, "qword endian");
  std::array<bool, memory::kBytes> selected{};
  for (size_t i = 0; i < mcla::timer_journal::kWordOffsets.size(); ++i) {
    const auto at = mcla::timer_journal::kWordOffsets[i];
    const uint32_t expected = (uint32_t(at) << 24) | ((at + 1) << 16) |
                              ((at + 2) << 8) | (at + 3);
    Check(decoded.words[i] == expected, "every word exact offset");
    for (unsigned b = 0; b < 4; ++b) selected[at + b] = true;
  }
  for (auto at : mcla::timer_journal::kByteOffsets) selected[at] = true;
  for (unsigned at = 64; at < 72; ++at) selected[at] = true;
  auto changed_gaps = bytes;
  for (size_t i = 0; i < bytes.size(); ++i) if (!selected[i]) changed_gaps[i] ^= 0xFF;
  Check(memory::Decode(changed_gaps) == decoded, "unselected prefix and gaps never exported");
  uintptr_t host = 99;
  Check(!memory::Address(0, 0x1000, host) && !host, "null base");
  Check(!memory::Address(0x1000, 0, host), "null guest");
  Check(!memory::Address(UINTPTR_MAX - 5, 0x1000, host), "host overflow");
  Check(!memory::Address(0x1000, UINT32_MAX - 3, host), "guest overflow");
  Check(!memory::Address(0x1000, 0xDFFFFFFFu, host), "physical offset seam");
  Check(memory::Address(0x100000000ull, 0xE0000000u, host) &&
        host == 0x1E0001000ull, "exact high-region offset");
  Check(memory::Address(0x100000000ull, 0xBCE70000u, host) &&
        host == 0x1BCE70000ull, "normal virtual offset");
  SYSTEM_INFO info{}; GetSystemInfo(&info);
  auto* pages = static_cast<uint8_t*>(VirtualAlloc(nullptr, size_t(info.dwPageSize) * 2,
                                                  MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
  Check(pages != nullptr, "owned test allocation");
  if (!pages) return 1;
  for (size_t i = 0; i < bytes.size(); ++i) pages[i] = bytes[i];
  const uintptr_t base = reinterpret_cast<uintptr_t>(pages) - 0x1000;
  mcla::timer_journal::State result;
  SetLastError(0x19700101);
  Check(memory::Read(base, 0x1000, result) && result == decoded, "actual process copy");
  Check(GetLastError() == 0x19700101, "success preserves last error");
  Check(reinterpret_cast<uintptr_t>(pages) > 0xE0001000ull, "high-region test base representable");
  if (reinterpret_cast<uintptr_t>(pages) > 0xE0001000ull)
    Check(memory::Read(reinterpret_cast<uintptr_t>(pages) - 0xE0001000ull,
                       0xE0000000u, result) && result == decoded, "actual high-region mapping");
  result = decoded; SetLastError(0x19700303);
  Check(!memory::Read(base, UINT32_MAX, result) && result == mcla::timer_journal::State{},
        "invalid address clears output");
  Check(GetLastError() == 0x19700303, "invalid-address path preserves last error");
  DWORD previous = 0;
  const bool protected_page = VirtualProtect(pages + info.dwPageSize,
                                             info.dwPageSize, PAGE_NOACCESS, &previous) != 0;
  Check(protected_page, "owned unreadable page");
  if (protected_page) {
    result = decoded; SetLastError(0x19700202);
    Check(!memory::Read(base, 0x1000 + info.dwPageSize - 50, result), "fault-contained boundary copy");
    Check(result == mcla::timer_journal::State{}, "failed copy never retains stale state");
    Check(GetLastError() == 0x19700202, "failure preserves last error");
    Check(!memory::Read(base, 0x1000 + info.dwPageSize, result), "unreadable page no crash");
  }
  Check(VirtualFree(pages, 0, MEM_RELEASE) != 0, "owned memory released");
  std::cout << "checks=" << checks << " failures=" << failures << '\n';
  return failures ? 1 : 0;
}
