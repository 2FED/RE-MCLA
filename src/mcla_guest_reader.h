#pragma once

// Checked guest-memory reads for host diagnostics that run inside guest hooks
// or on the F10 thread (KI-026/KI-035 probe, M11-002 album and pursuit
// probes). Moved unchanged from mcla_ki026_probe.cpp; Bytes() is new.
//
// A read is allowed only when the address lies inside a guest heap and the
// page protection includes read access. Spans are at most one guest page, so
// checking both ends covers every page they touch. A failed read returns 0
// and clears `ok`. The check and the read are not atomic: a page decommitted
// in between is a residual (diagnostic-only) risk.

#include "mcla_init.h"

#include <rex/system/kernel_state.h>
#include <rex/system/xmemory.h>

#include <cstdint>
#include <cstring>
#include <string>

namespace mcla::guest {

inline constexpr uint32_t kGuestPage = 4096;

class Reader {
 public:
  Reader(rex::memory::Memory *memory, uint8_t *base) : memory_(memory), base_(base) {}

  bool Readable(uint32_t address, uint32_t size) const {
    return Accessible(address, size, rex::memory::kMemoryProtectRead);
  }
  // Used only by the KI-035 watchdog before its three guest stores.
  bool Writable(uint32_t address, uint32_t size) const {
    return Accessible(address, size,
                      rex::memory::kMemoryProtectRead | rex::memory::kMemoryProtectWrite);
  }
  bool Accessible(uint32_t address, uint32_t size, uint32_t required) const {
    if (!memory_ || !base_ || !address || size == 0 || size > kGuestPage ||
        address > UINT32_MAX - (size - 1)) {
      return false;
    }
    for (const uint32_t probe : {address, address + size - 1}) {
      auto *heap = memory_->LookupHeap(probe);
      uint32_t protect = 0;
      // Bounds first: BaseHeap::QueryProtect accepts a page index equal to
      // the page-table size.
      if (!heap || probe < heap->heap_base() ||
          probe - heap->heap_base() >= heap->heap_size() ||
          !heap->QueryProtect(probe, &protect) || (protect & required) != required) {
        return false;
      }
    }
    return true;
  }
  uint32_t U32(uint32_t address, bool &ok) const {
    if (!Readable(address, 4)) {
      ok = false;
      return 0;
    }
    uint8_t *base = base_;
    return REX_LOAD_U32(address);
  }
  uint32_t U16(uint32_t address, bool &ok) const {
    if (!Readable(address, 2)) {
      ok = false;
      return 0;
    }
    uint8_t *base = base_;
    return REX_LOAD_U16(address);
  }
  uint32_t U8(uint32_t address, bool &ok) const {
    if (!Readable(address, 1)) {
      ok = false;
      return 0;
    }
    uint8_t *base = base_;
    return REX_LOAD_U8(address);
  }
  float F32(uint32_t address, bool &ok) const {
    const uint32_t bits = U32(address, ok);
    float value = 0.0f;
    std::memcpy(&value, &bits, sizeof(value));
    return value;
  }
  // Copies `size` raw guest bytes (at most one guest page, in guest byte
  // order) to `out`. False (out untouched) when the span is not readable.
  bool Bytes(uint32_t address, uint32_t size, uint8_t *out) const {
    if (!out || !Readable(address, size)) {
      return false;
    }
    uint8_t *base = base_;
    for (uint32_t i = 0; i < size; ++i) {
      out[i] = REX_LOAD_U8(address + i);
    }
    return true;
  }
  // Up to 96 printable characters of a NUL-terminated guest string. Each
  // page the string touches is checked before it is read.
  std::string Text(uint32_t address) const {
    std::string text;
    uint8_t *base = base_;
    for (uint32_t i = 0; i < 96; ++i) {
      const uint32_t at = address + i;
      if ((i == 0 || at % kGuestPage == 0) && !Readable(at, 1)) {
        break;
      }
      const char c = static_cast<char>(REX_LOAD_U8(at));
      if (c == 0) {
        break;
      }
      text.push_back(c >= 32 && c < 127 && c != '"' && c != '\\' ? c : '?');
    }
    return text;
  }

 private:
  rex::memory::Memory *memory_;
  uint8_t *base_;
};

// Reader for code running inside guest hooks.
inline Reader HookReader(uint8_t *base) {
  auto *kernel = rex::system::kernel_state();
  return Reader(kernel ? kernel->memory() : nullptr, base);
}

}  // namespace mcla::guest
