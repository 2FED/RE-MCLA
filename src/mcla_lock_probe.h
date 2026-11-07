#pragma once

// M11-007d / KI-047: guest texture lock probe and read-back completion at the
// lock (MCLA_LOCK v=1). Pure parts: the lock core's register and stack layout,
// the per-lock decision, the line pacer, the per-minute counters and the line
// formats. The hook is in mcla_lock_probe.cpp; everything here is unit-tested
// (tests/mcla_lock_probe_test.cpp, which also checks the layout against the
// generated code). Inputs: docs/evidence/M11-ki047-lock-fence-re-20261107.md
// (the lock path and the early fence) and
// docs/evidence/M11-007c-steady-bounded-wait-and-range-api-20261107.md (the
// SDK range API rex::gpu::ReadbackSyncQueryRange / ReadbackSyncEnsureRange).
//
// Static analysis (generated/default at the M11 codegen):
// - Every D3D texture lock (LockRect sub_82410518, LockBox sub_824105A8 ->
//   sub_82410440) and the vertex/index buffer locks (sub_82422320 /
//   sub_82422430) reach the XDK lock core sub_82421CA0
//   (mcla_recomp.27.cpp:44257). At its entry: r3 = resource (`mr r24,r3`),
//   r4 = a 64-bit argument (`cmpldi cr6,r4,0`, `rldicl r11,r4,32,32`; non-zero
//   selects a fence from a device table and sets flag 2), r7 = texture base
//   (`mr r23,r7`; sub_82410440 passes `(tex+32) & ~0xFFF`), r8 = mip base
//   (`mr r26,r8`), r9 = the locked start as a guest virtual address of
//   physical memory (`mr r28,r9`), r10 = the locked length in bytes
//   (`mr r27,r10`). The lock flags are the first stack argument: after
//   `stwu r1,-176(r1)` the core reads `lwz r10,260(r1)`, i.e. the caller's
//   r1 + 84 (sub_82410440 stores them with `stw r27,84(r1)` before
//   `bl 0x82421ca0`). Flags 0x1010 select the ReadFence, 0x12 skip the
//   flush.
// - The core first waits for the resource fence (`bl 0x82411e98`,
//   BlockOnFence), then converts r9 to the physical range it flushes:
//   `clrlwi r10,r28,3` (r9 & 0x1FFFFFFF), `rlwinm r11,r28,12,20,31`,
//   `addi r11,r11,512`, `rlwinm r11,r11,0,19,19` (+0x1000 for the
//   0xE0000000 view), `add r30,r11,r10` (physical start), `add r29,r30,r27`
//   (physical end = start + r10). So the start and length are known at entry;
//   the caller sub_82410440 need not be hooked.
//
// Placement of the read-back completion: the guest's own synchronisation is
// the fence wait inside the core, and on the host the fence word is written
// when the command processor parses EVENT_WRITE_SHD. Before the core has run,
// the command processor may not even have reached the resolve, so a pending
// copy of this frame may not exist yet. The hook therefore queries the range
// at entry (logged as the entry state), calls the ORIGINAL UNCHANGED (the
// fence wait included), then queries again and, with mcla_lock_readback_ensure,
// completes a pending copy with ReadbackSyncEnsureRange before the hook
// returns to the guest caller. The guest reads the locked bytes only after
// the lock returns, so this is "before the lock returns" for the guest.
// Guest memory is never written by the hook (only by the SDK delivery, which
// is the same memcpy as the command processor's own read-back delivery).

#include <array>
#include <cstdint>
#include <cstdio>
#include <string>

namespace mcla::lock_probe {

inline constexpr uint32_t kLockCore = 0x82421CA0u;
inline constexpr uint32_t kLockCaller = 0x82410440u;
inline constexpr uint32_t kBlockOnFence = 0x82411E98u;
// The core's frame size and its flags read (see above).
inline constexpr uint32_t kCoreFrame = 176;
inline constexpr uint32_t kCoreFlagsRead = 260;
inline constexpr uint32_t kFlagsStackOffset = kCoreFlagsRead - kCoreFrame;  // caller r1 + 84
inline constexpr uint32_t kEnsureTimeoutMs = 20;

inline constexpr uint64_t kMinuteMs = 60000;
inline constexpr size_t kMaxKeys = 16;

// The lock core's physical translation of r9 (see above).
constexpr uint32_t GuestToPhysical(uint32_t address) {
  return (address & 0x1FFFFFFFu) + ((((address >> 20) & 0xFFFu) + 512u) & 0x1000u);
}

// What the hook does for one lock.
struct Plan {
  bool query = false;   // query the range at entry and after the original
  bool log = false;     // a MCLA_LOCK fn=lock line (paced) and the key count
  bool ensure = false;  // ReadbackSyncEnsureRange after the original
};

// Before the original: whether to query at all (either switch on, a non-empty
// range).
constexpr bool ShouldQuery(bool probe, bool ensure_on, uint32_t length) {
  return (probe || ensure_on) && length != 0;
}

// After the original: overlap at entry or after it; ensure only for a
// pending copy after the guest's own fence wait.
constexpr Plan Decide(bool probe, bool ensure_on, uint32_t length, bool overlap_entry,
                      bool overlap_after, bool pending_after) {
  Plan plan;
  plan.query = ShouldQuery(probe, ensure_on, length);
  if (!plan.query) {
    return plan;
  }
  const bool overlap = overlap_entry || overlap_after;
  plan.log = probe && overlap;
  plan.ensure = ensure_on && overlap_after && pending_after;
  return plan;
}

// Token bucket: kBurst lines, then one line per kRefillMs (2/s); `suppressed`
// receives the calls suppressed since the previous line.
class LinePacer {
 public:
  static constexpr uint32_t kBurst = 32;
  static constexpr uint64_t kRefillMs = 500;
  static constexpr uint32_t kMaxLines = 4000;

  bool Allow(uint64_t now_ms, uint32_t &suppressed) {
    if (!started_) {
      started_ = true;
      tokens_ = kBurst;
      last_ms_ = now_ms;
    }
    if (now_ms > last_ms_) {
      const uint64_t refill = (now_ms - last_ms_) / kRefillMs;
      if (refill) {
        const uint64_t tokens = tokens_ + refill;
        tokens_ = static_cast<uint32_t>(tokens < kBurst ? tokens : kBurst);
        last_ms_ += refill * kRefillMs;
      }
    }
    if (lines_ >= kMaxLines || tokens_ == 0) {
      ++suppressed_;
      return false;
    }
    --tokens_;
    ++lines_;
    suppressed = suppressed_;
    suppressed_ = 0;
    return true;
  }

 private:
  bool started_ = false;
  uint32_t tokens_ = 0;
  uint64_t last_ms_ = 0;
  uint32_t lines_ = 0;
  uint32_t suppressed_ = 0;
};

// The SDK's ReadbackRangeInfo / ReadbackSyncEnsureResult, mirrored so the
// pure parts do not depend on the SDK header.
struct KeyState {
  bool overlap = false;
  uint32_t key_base = 0;
  uint32_t key_length = 0;
  uint32_t slot_size = 0;
  uint32_t width = 0;
  uint32_t height = 0;
  uint32_t format = 0;
  bool exempt = false;
  bool pending = false;
  bool unsubmitted = false;
  uint32_t copy_age_frames = UINT32_MAX;
  uint32_t resolve_age_frames = 0;
};

struct EnsureState {
  bool called = false;
  uint32_t waited_ms = 0;
  uint32_t waited_us = 0;
  bool pending = false;
  bool delivered = false;
  bool timed_out = false;
};

// One lock as seen by the hook.
struct Lock {
  uint32_t resource = 0;
  uint64_t r4 = 0;
  uint32_t flags = 0;
  bool flags_read = false;
  uint32_t texture_base = 0;
  uint32_t start = 0;  // r9 (guest virtual)
  uint32_t length = 0;
  uint32_t lr = 0;
  uint32_t tid = 0;
};

inline std::string Hex8(uint32_t value) {
  char text[16];
  std::snprintf(text, sizeof(text), "%08X", value);
  return text;
}

inline std::string AgeText(uint32_t frames) {
  return frames == UINT32_MAX ? std::string("none") : std::to_string(frames);
}

// The key fields of a query ("key=none" without an overlap).
inline std::string KeyText(const KeyState &k) {
  if (!k.overlap) {
    return "key=none";
  }
  char text[256];
  std::snprintf(text, sizeof(text),
                "key=%08X key_len=%u size=%ux%u fmt=%u slot=%u exempt=%d pending=%d "
                "unsubmitted=%d copy_age=%s resolve_age=%u",
                k.key_base, k.key_length, k.width, k.height, k.format, k.slot_size,
                k.exempt ? 1 : 0, k.pending ? 1 : 0, k.unsubmitted ? 1 : 0,
                AgeText(k.copy_age_frames).c_str(), k.resolve_age_frames);
  return text;
}

// MCLA_LOCK v=1 fn=lock ...: the lock, the entry query, the query after the
// original (prefixed after_) and the ensure result.
inline std::string LockLine(const Lock &lock, const KeyState &entry, const KeyState &after,
                            const EnsureState &ensure, uint32_t suppressed) {
  char head[256];
  std::snprintf(head, sizeof(head),
                "MCLA_LOCK v=1 fn=lock res=%08X r4=%016llX flags=%s base=%08X start=%08X "
                "phys=%08X len=%u lr=%08X tid=%u ",
                lock.resource, static_cast<unsigned long long>(lock.r4),
                lock.flags_read ? Hex8(lock.flags).c_str() : "n/a", lock.texture_base,
                lock.start, GuestToPhysical(lock.start), lock.length, lock.lr, lock.tid);
  std::string text = head;
  text += KeyText(entry);
  text += " after_" + KeyText(after);
  if (ensure.called) {
    char tail[160];
    std::snprintf(tail, sizeof(tail),
                  " ensure=1 ensure_pending=%d waited_ms=%u waited_us=%u delivered=%d "
                  "timed_out=%d",
                  ensure.pending ? 1 : 0, ensure.waited_ms, ensure.waited_us,
                  ensure.delivered ? 1 : 0, ensure.timed_out ? 1 : 0);
    text += tail;
  } else {
    text += " ensure=0";
  }
  text += " suppressed=" + std::to_string(suppressed);
  return text;
}

// Per-minute counters.
struct Minute {
  uint64_t locks = 0;
  uint64_t overlapping = 0;
  uint64_t pending_entry = 0;
  uint64_t pending_after = 0;
  uint64_t ensure_calls = 0;
  uint64_t ensure_pending = 0;
  uint64_t ensure_delivered = 0;
  uint64_t ensure_timeouts = 0;
  uint64_t ensure_wait_ms_total = 0;
  uint32_t ensure_wait_ms_max = 0;
  struct KeyCount {
    uint32_t base = 0;
    uint64_t count = 0;
  };
  std::array<KeyCount, kMaxKeys> keys{};
  size_t key_count = 0;
  uint64_t other_keys = 0;  // overlaps of keys beyond kMaxKeys

  void CountKey(uint32_t base) {
    for (size_t i = 0; i < key_count; ++i) {
      if (keys[i].base == base) {
        ++keys[i].count;
        return;
      }
    }
    if (key_count < kMaxKeys) {
      keys[key_count++] = {base, 1};
      return;
    }
    ++other_keys;
  }

  // One lock: `key` is the overlapping key (after the original, else at
  // entry).
  void Add(const KeyState &entry, const KeyState &after, const EnsureState &ensure) {
    ++locks;
    if (!entry.overlap && !after.overlap) {
      return;
    }
    ++overlapping;
    CountKey(after.overlap ? after.key_base : entry.key_base);
    pending_entry += entry.overlap && entry.pending ? 1 : 0;
    pending_after += after.overlap && after.pending ? 1 : 0;
    if (ensure.called) {
      ++ensure_calls;
      ensure_pending += ensure.pending ? 1 : 0;
      ensure_delivered += ensure.delivered ? 1 : 0;
      ensure_timeouts += ensure.timed_out ? 1 : 0;
      ensure_wait_ms_total += ensure.waited_ms;
      ensure_wait_ms_max = ensure.waited_ms > ensure_wait_ms_max ? ensure.waited_ms
                                                                 : ensure_wait_ms_max;
    }
  }
};

// "[BASE:count,...]" in first-seen order, "+other:n" when keys overflowed.
inline std::string KeysText(const Minute &m) {
  std::string text = "[";
  for (size_t i = 0; i < m.key_count; ++i) {
    text += i ? "," : "";
    text += Hex8(m.keys[i].base) + ":" + std::to_string(m.keys[i].count);
  }
  if (m.other_keys) {
    text += m.key_count ? "," : "";
    text += "other:" + std::to_string(m.other_keys);
  }
  return text + "]";
}

inline std::string MinuteLine(const Minute &m, bool probe, bool ensure_on) {
  char text[384];
  std::snprintf(text, sizeof(text),
                "MCLA_LOCK v=1 kind=minute probe=%d ensure_on=%d locks=%llu overlapping=%llu "
                "pending_entry=%llu pending_after=%llu ensure_calls=%llu ensure_pending=%llu "
                "ensure_delivered=%llu ensure_timeouts=%llu ensure_wait_ms_total=%llu "
                "ensure_wait_ms_max=%u keys=",
                probe ? 1 : 0, ensure_on ? 1 : 0, static_cast<unsigned long long>(m.locks),
                static_cast<unsigned long long>(m.overlapping),
                static_cast<unsigned long long>(m.pending_entry),
                static_cast<unsigned long long>(m.pending_after),
                static_cast<unsigned long long>(m.ensure_calls),
                static_cast<unsigned long long>(m.ensure_pending),
                static_cast<unsigned long long>(m.ensure_delivered),
                static_cast<unsigned long long>(m.ensure_timeouts),
                static_cast<unsigned long long>(m.ensure_wait_ms_total), m.ensure_wait_ms_max);
  return text + KeysText(m);
}

// Minute window: true (and restarts the window) when a minute has passed.
class MinuteClock {
 public:
  bool Due(uint64_t now_ms) {
    if (!started_) {
      started_ = true;
      start_ms_ = now_ms;
      return false;
    }
    if (now_ms - start_ms_ < kMinuteMs) {
      return false;
    }
    start_ms_ = now_ms;
    return true;
  }

 private:
  bool started_ = false;
  uint64_t start_ms_ = 0;
};

}  // namespace mcla::lock_probe
