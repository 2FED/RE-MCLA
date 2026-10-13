#pragma once

#if !defined(_WIN32)
#error Windows-only fault-contained timer observation.
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#include "mcla_timer_journal.h"

#include <array>
#include <limits>

namespace mcla::timer_memory {
static_assert(sizeof(uintptr_t) == 8, "Only the pinned Windows x64 guest map is supported");
constexpr size_t kBytes = 108;

// Match the pinned generated mcla_init.h REX_PHYS_HOST_OFFSET, not an
// invented guest-pointer cast. Translation does not establish readability or
// object lifetime. Reject overflow and ranges crossing the offset boundary.
inline bool Address(uintptr_t base, uint32_t guest, uintptr_t& host) noexcept {
  host = 0;
  if (!base || !guest || guest > UINT32_MAX - (kBytes - 1)) return false;
  const uint32_t last = guest + uint32_t(kBytes - 1);
  if ((guest < 0xE0000000u) != (last < 0xE0000000u)) return false;
  const uint64_t offset = uint64_t(guest) + (guest >= 0xE0000000u ? 0x1000 : 0);
  if (base > std::numeric_limits<uintptr_t>::max() - offset - kBytes) return false;
  host = base + uintptr_t(offset);
  return true;
}

inline timer_journal::State Decode(const std::array<uint8_t, kBytes>& bytes) noexcept {
  timer_journal::State out;
  auto big = [&](uint32_t at, unsigned count) {
    uint64_t value = 0;
    for (unsigned i = 0; i < count; ++i) value = (value << 8) | bytes[at + i];
    return value;
  };
  for (size_t i = 0; i < out.words.size(); ++i)
    out.words[i] = uint32_t(big(timer_journal::kWordOffsets[i], 4));
  for (size_t i = 0; i < out.bytes.size(); ++i)
    out.bytes[i] = bytes[timer_journal::kByteOffsets[i]];
  out.qwords[0] = big(64, 8);
  return out;
}

// Reads this process only, never writes guest memory. A failed/partial copy
// clears the result; no retry or direct volatile dereference fallback. The
// temporary contiguous object prefix includes unused gap bytes, but only the
// allowlisted fields leave this function. A successful copy is NOT a coherent
// multi-field snapshot or an independently proved timer-object lifetime.
inline bool Read(uintptr_t base, uint32_t guest, timer_journal::State& out) noexcept {
  const DWORD saved_error = GetLastError();
  out = {};
  uintptr_t host = 0;
  bool ok = false;
  if (Address(base, guest, host)) {
    std::array<uint8_t, kBytes> bytes{};
    SIZE_T count = 0;
    ok = ReadProcessMemory(GetCurrentProcess(), reinterpret_cast<const void*>(host),
                           bytes.data(), bytes.size(), &count) && count == bytes.size();
    if (ok) out = Decode(bytes);
  }
  SetLastError(saved_error);
  return ok;
}
}  // namespace mcla::timer_memory
