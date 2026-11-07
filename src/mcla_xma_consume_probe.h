#pragma once

// KI-071 step 2: guest-side XMA consume ledger (MCLA_XMA_CONSUME v=1). Pure
// parts: the guest layout the hook reads and the per-context ledger with its
// line format. The hook is in mcla_xma_consume_probe.cpp; everything here is
// unit-tested (tests/mcla_xma_consume_probe_test.cpp, which also checks the
// layout against the generated code). Design:
// docs/evidence/M11-ki071-xma-consume-probe-20261107.md; inputs:
// docs/evidence/M11-ki071-sibling-contexts-re-20261107.md (section 2) and
// docs/evidence/M11-ki071-xma-sibling-trace-sdk-20261107.md (consume hook).
//
// Static analysis (generated/default at the M11 codegen):
// - Consume sub_823ED700 (mcla_recomp.25.cpp:40311): r3 = the voice's XMA
//   object, r4 = stream index i, r5 = samples consumed from that stream (per
//   channel), r6 = pointer to a u32 that receives the next read address.
//   Its only caller is the voice pump sub_823FC9F8 (mcla_recomp.26.cpp:5692:
//   `mr r5,r3` after the resampler `bl 0x823ff5f8`, `lwz r3,184(r31)`,
//   `mr r4,r27`, `bl 0x823ed700`).
// - Stream record rec = [r3+8] + i*96: `rlwinm r11,r4,1,0,30`,
//   `add r10,r4,r11` (3i), `lwz r11,8(r3)`, `rlwinm r31,r10,5,0,26` (96i),
//   `add r11,r31,r11`.
// - Stereo = bit 29 of the dword at rec+4 (`lwz r7,4(r11)`,
//   `rlwinm r10,r7,3,31,31`); bytes consumed = r5 << (stereo + 1)
//   (`addi r6,r10,1`, `slw r10,r5,r6`).
// - rec+80 (u16) is the hardware context id, written once at create by
//   sub_823EDCD8 (mcla_recomp.25.cpp:41146): XMACreateContext,
//   MmGetPhysicalAddress, `subf r11,r11,r3` (phys - array base),
//   `srawi r10,r11,6`, `clrlwi r9,r10,16`, `sth r9,80(r30)`. It equals the
//   SDK's context index (64-byte contexts, XmaDecoder::kContextCount = 320),
//   so the ids match the SDK `[xma] group:` / first-decode lines.
//
// The ledger sums r5 per context id with relaxed atomics (hot path: three
// checked reads and two atomic adds per call). Once per second one caller
// collects and formats a line of the ids consumed in that interval; once per
// minute a summary line follows.

#include <array>
#include <atomic>
#include <cstdint>
#include <mutex>
#include <string>

namespace mcla::xma_consume {

inline constexpr uint32_t kConsumeFunction = 0x823ED700u;
inline constexpr uint32_t kCreateFunction = 0x823EDCD8u;
inline constexpr uint32_t kPumpFunction = 0x823FC9F8u;

// Layout (see above).
inline constexpr uint32_t kStreamsOffset = 8;       // [xma+8] = stream records
inline constexpr uint32_t kStreamRecordSize = 96;
inline constexpr uint32_t kRecordDword1 = 4;        // shadow context dword1
inline constexpr uint32_t kStereoBit = 29;          // in dword1
inline constexpr uint32_t kRecordContextId = 80;    // u16 hardware context id
inline constexpr uint32_t kContextCount = 320;
// Plausibility bound for r4 (a voice has a handful of streams); a larger
// index is counted as bad and not read.
inline constexpr uint32_t kMaxStreamIndex = 64;

inline constexpr size_t kMaxLineEntries = 32;
inline constexpr uint64_t kSecondMs = 1000;
inline constexpr uint64_t kMinuteMs = 60000;

constexpr uint32_t RecordAddress(uint32_t streams, uint32_t stream) {
  return streams + stream * kStreamRecordSize;
}

constexpr bool Stereo(uint32_t dword1) {
  return ((dword1 >> kStereoBit) & 1u) != 0;
}

// One context id in a second line.
struct Entry {
  uint32_t id = 0;
  uint64_t samples = 0;  // per channel, in this interval
  uint64_t run = 0;      // per channel, since the id was last idle for an interval
  bool stereo = false;
};

// "[id:value,...]" for the entries (in order).
template <typename Value>
std::string List(const Entry *entries, size_t count, Value value) {
  std::string text = "[";
  for (size_t i = 0; i < count; ++i) {
    text += i ? "," : "";
    text += std::to_string(entries[i].id) + ":" + std::to_string(value(entries[i]));
  }
  return text + "]";
}

inline std::string FormatSecondLine(uint64_t interval_ms, size_t ids, const Entry *entries,
                                    size_t count, uint64_t bad) {
  std::string stereo = "[";
  bool first = true;
  for (size_t i = 0; i < count; ++i) {
    if (entries[i].stereo) {
      stereo += first ? "" : ",";
      stereo += std::to_string(entries[i].id);
      first = false;
    }
  }
  stereo += "]";
  return "MCLA_XMA_CONSUME v=1 kind=second ms=" + std::to_string(interval_ms) +
         " ids=" + std::to_string(ids) +
         " ctxs=" + List(entries, count, [](const Entry &e) { return e.samples; }) +
         " run=" + List(entries, count, [](const Entry &e) { return e.run; }) +
         " stereo=" + stereo + " more=" + std::to_string(ids - count) +
         " bad=" + std::to_string(bad);
}

inline std::string FormatMinuteLine(uint64_t interval_ms, uint32_t ids, uint64_t samples,
                                    uint64_t calls, uint64_t bad, uint32_t lines) {
  return "MCLA_XMA_CONSUME v=1 kind=minute ms=" + std::to_string(interval_ms) +
         " ids=" + std::to_string(ids) + " samples=" + std::to_string(samples) +
         " calls=" + std::to_string(calls) + " bad=" + std::to_string(bad) +
         " lines=" + std::to_string(lines);
}

// What one Collect call produced (empty strings: no line).
struct Lines {
  std::string second;
  std::string minute;
};

class Ledger {
 public:
  // Hot path (any guest thread).
  void Add(uint32_t id, uint32_t samples, bool stereo) {
    if (id >= kContextCount) {
      AddBad();
      return;
    }
    sum_[id].fetch_add(samples, std::memory_order_relaxed);
    if (stereo) {
      stereo_[id].store(true, std::memory_order_relaxed);
    }
    calls_.fetch_add(1, std::memory_order_relaxed);
  }
  void AddBad() {
    calls_.fetch_add(1, std::memory_order_relaxed);
    bad_.fetch_add(1, std::memory_order_relaxed);
  }

  // Called after Add/AddBad. At most one caller per second gets the second
  // line (and, once a minute has passed, the minute line); everyone else
  // returns at once with empty lines.
  Lines Collect(uint64_t now_ms) {
    Lines lines;
    uint64_t next = next_ms_.load(std::memory_order_relaxed);
    if (next == 0) {
      // First call: start the clocks.
      if (next_ms_.compare_exchange_strong(next, now_ms + kSecondMs,
                                           std::memory_order_relaxed)) {
        std::lock_guard lock(mutex_);
        second_start_ms_ = now_ms;
        minute_start_ms_ = now_ms;
      }
      return lines;
    }
    if (now_ms < next ||
        !next_ms_.compare_exchange_strong(next, now_ms + kSecondMs, std::memory_order_relaxed)) {
      return lines;
    }
    std::unique_lock lock(mutex_, std::try_to_lock);
    if (!lock.owns_lock()) {
      return lines;
    }
    std::array<Entry, kMaxLineEntries> entries{};
    size_t count = 0;
    size_t ids = 0;
    for (uint32_t id = 0; id < kContextCount; ++id) {
      const uint64_t samples = sum_[id].exchange(0, std::memory_order_relaxed);
      if (!samples) {
        run_[id] = 0;  // idle for a whole interval: the next run starts at 0
        continue;
      }
      run_[id] += samples;
      ++ids;
      minute_seen_[id] = true;
      minute_samples_ += samples;
      if (count < kMaxLineEntries) {
        entries[count++] =
            Entry{id, samples, run_[id], stereo_[id].load(std::memory_order_relaxed)};
      }
    }
    const uint64_t bad = bad_.exchange(0, std::memory_order_relaxed);
    minute_bad_ += bad;
    if (ids || bad) {
      lines.second = FormatSecondLine(now_ms - second_start_ms_, ids, entries.data(), count, bad);
      ++minute_lines_;
    }
    second_start_ms_ = now_ms;
    if (now_ms - minute_start_ms_ >= kMinuteMs) {
      uint32_t distinct = 0;
      for (bool &seen : minute_seen_) {
        distinct += seen ? 1 : 0;
        seen = false;
      }
      lines.minute = FormatMinuteLine(now_ms - minute_start_ms_, distinct, minute_samples_,
                                      calls_.exchange(0, std::memory_order_relaxed), minute_bad_,
                                      minute_lines_);
      minute_start_ms_ = now_ms;
      minute_samples_ = 0;
      minute_bad_ = 0;
      minute_lines_ = 0;
    }
    return lines;
  }

 private:
  std::array<std::atomic<uint64_t>, kContextCount> sum_{};
  std::array<std::atomic<bool>, kContextCount> stereo_{};
  std::atomic<uint64_t> calls_{0};
  std::atomic<uint64_t> bad_{0};
  std::atomic<uint64_t> next_ms_{0};
  // Collector state (under mutex_).
  std::mutex mutex_;
  std::array<uint64_t, kContextCount> run_{};
  std::array<bool, kContextCount> minute_seen_{};
  uint64_t second_start_ms_ = 0;
  uint64_t minute_start_ms_ = 0;
  uint64_t minute_samples_ = 0;
  uint64_t minute_bad_ = 0;
  uint32_t minute_lines_ = 0;
};

}  // namespace mcla::xma_consume
