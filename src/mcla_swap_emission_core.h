#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <span>

// Standalone numerical host-word protocol. SDK integration owns byte ordering,
// packet/ring IO, clocks, snapshot reads, epoch uniqueness and synchronization.
// No allocation, exceptions, game algorithms, resource reads or native APIs.
namespace rex::diagnostics::swap_emission {
inline constexpr size_t kStockWords = 4;
inline constexpr size_t kFetchWords = 6;
inline constexpr size_t kExtendedWords = 11;
inline constexpr size_t kMaximumPayloadWords = 56;
inline constexpr uint32_t kMagic = 0x4D535750u;  // Numeric protocol tag, MSWP.
inline constexpr uint32_t kVersion = 1;
inline constexpr uint64_t kMaxQpc = uint64_t(std::numeric_limits<int64_t>::max());
inline constexpr bool kSimulationProven = false;
inline constexpr bool kSnapshotCoherenceProven = false;
inline constexpr bool kNativeAdmitted = false;

enum class Result {
  kLegacyDisabled, kLegacyPacket, kExtendedPacket, kTokenIssued, kTokenMatched,
  kInvalidStockWordCount, kInvalidFetchWordCount, kPayloadTooLong,
  kUnsupportedPayloadLength, kUnknownMagic, kUnknownVersion,
  kDeclaredLengthMismatch, kZeroToken, kInvalidEpoch,
  kCapacityExhausted, kUnissuedToken, kTokenEpochMismatch,
  kReplay, kStockMismatch, kFetchMismatch,
};
enum class SnapshotResult { kAvailable, kUnavailable, kInvalidQpc, kMissingIdentity };

struct Token {
  uint64_t epoch = 0;
  uint64_t serial = 0;
  constexpr bool operator==(const Token&) const = default;
};
struct Snapshot {
  uint64_t qpc_before = 0, qpc_after = 0;
  uint32_t native_thread_id = 0, guest_caller_lr = 0;
  uint32_t timer_address = 0, timer_counter = 0, timer_delta_bits = 0;
  bool snapshot_available = false;
  constexpr bool operator==(const Snapshot&) const = default;
};
struct Payload {
  std::array<uint32_t, kMaximumPayloadWords> words{};
  size_t length = 0;
  constexpr std::span<const uint32_t> view() const noexcept {
    return length <= words.size() ? std::span<const uint32_t>{words.data(), length}
                                 : std::span<const uint32_t>{};
  }
};
struct Decoded {
  std::array<uint32_t, kStockWords> stock{};
  Token token;
  bool extended = false;
};
struct Emission {
  Payload payload;
  Token token;
  Snapshot snapshot;
  SnapshotResult snapshot_result = SnapshotResult::kUnavailable;
};
struct Match {
  Token token;
  Snapshot snapshot;
  SnapshotResult snapshot_result = SnapshotResult::kUnavailable;
};

inline SnapshotResult NormalizeSnapshot(const Snapshot& input, Snapshot& output) noexcept {
  const Snapshot frozen = input;  // Also safe when caller normalizes in place.
  output = {};
  if (!frozen.snapshot_available) return SnapshotResult::kUnavailable;
  if (!frozen.qpc_before || frozen.qpc_before > frozen.qpc_after || frozen.qpc_after > kMaxQpc)
    return SnapshotResult::kInvalidQpc;
  if (!frozen.native_thread_id || !frozen.guest_caller_lr || !frozen.timer_address)
    return SnapshotResult::kMissingIdentity;
  // Counter/delta bits are opaque, including zero and IEEE special bit patterns.
  // Available means only structurally usable caller claims, never coherence.
  output = frozen;
  return SnapshotResult::kAvailable;
}

inline Result Encode(std::span<const uint32_t> stock, Token token, Payload& output,
                     bool enabled = false) noexcept {
  if (stock.size() != kStockWords) { output = {}; return Result::kInvalidStockWordCount; }
  std::array<uint32_t, kStockWords> frozen{};
  for (size_t i = 0; i < kStockWords; ++i) frozen[i] = stock[i];
  output = {};
  if (enabled && (!token.epoch || !token.serial)) return Result::kZeroToken;
  for (size_t i = 0; i < kStockWords; ++i) output.words[i] = frozen[i];
  output.length = enabled ? kExtendedWords : kStockWords;
  if (!enabled) return Result::kLegacyDisabled;
  output.words[4] = kMagic;
  output.words[5] = kVersion;
  output.words[6] = uint32_t(kExtendedWords);
  output.words[7] = uint32_t(token.epoch >> 32);
  output.words[8] = uint32_t(token.epoch);
  output.words[9] = uint32_t(token.serial >> 32);
  output.words[10] = uint32_t(token.serial);
  return Result::kTokenIssued;
}

inline Result Decode(std::span<const uint32_t> payload, Decoded& output) noexcept {
  if (payload.size() > kMaximumPayloadWords) { output = {}; return Result::kPayloadTooLong; }
  const size_t count = payload.size();
  if (count != kStockWords && count != kExtendedWords) {
    output = {}; return Result::kUnsupportedPayloadLength;
  }
  std::array<uint32_t, kExtendedWords> frozen{};
  for (size_t i = 0; i < count; ++i) frozen[i] = payload[i];
  output = {};
  if (count == kStockWords) {
    for (size_t i = 0; i < kStockWords; ++i) output.stock[i] = frozen[i];
    return Result::kLegacyPacket;
  }
  // Exact physical count is checked before any extension word is accessed.
  if (frozen[4] != kMagic) return Result::kUnknownMagic;
  if (frozen[5] != kVersion) return Result::kUnknownVersion;
  if (frozen[6] != kExtendedWords) return Result::kDeclaredLengthMismatch;
  Token token{(uint64_t(frozen[7]) << 32) | frozen[8],
              (uint64_t(frozen[9]) << 32) | frozen[10]};
  if (!token.epoch || !token.serial) return Result::kZeroToken;
  for (size_t i = 0; i < kStockWords; ++i) output.stock[i] = frozen[i];
  output.token = token;
  output.extended = true;
  return Result::kExtendedPacket;  // Syntax only, NOT evidence of issuance.
}

template<size_t Capacity = 1024>
class Ledger {
  static_assert(Capacity > 0 && Capacity <= 4096);
 public:
  explicit Ledger(uint64_t epoch = 0, bool enabled = false) noexcept
      : epoch_(epoch), enabled_(enabled) {}
  Ledger(const Ledger&) = delete;
  Ledger& operator=(const Ledger&) = delete;

  // Caller-synchronized. Capture the Snapshot OUTSIDE that synchronization;
  // only fixed-size copying/lookup occurs here. Never recycle entries or epoch.
  // On rejection output is fully cleared. Caller must still execute the normal
  // swap with its original stock words (fallback Encode(..., enabled=false)).
  Result issue(std::span<const uint32_t> stock, std::span<const uint32_t> fetch,
               const Snapshot& supplied_snapshot, Emission& output) noexcept {
    if (stock.size() != kStockWords) { output = {}; return Result::kInvalidStockWordCount; }
    std::array<uint32_t, kStockWords> frozen_stock{};
    std::array<uint32_t, kFetchWords> frozen_fetch{};
    for (size_t i = 0; i < kStockWords; ++i) frozen_stock[i] = stock[i];
    if (enabled_ && fetch.size() == kFetchWords)
      for (size_t i = 0; i < kFetchWords; ++i) frozen_fetch[i] = fetch[i];
    const Snapshot frozen_snapshot = supplied_snapshot;
    output = {};
    if (!enabled_) return Encode(frozen_stock, {}, output.payload);
    if (fetch.size() != kFetchWords) return Result::kInvalidFetchWordCount;
    if (!epoch_) return Result::kInvalidEpoch;
    if (issued_ == Capacity) return Result::kCapacityExhausted;
    // Since Capacity<=4096 and no slots recycle, serial can never wrap.
    const Token token{epoch_, uint64_t(issued_) + 1};
    Entry entry;
    entry.stock = frozen_stock;
    entry.fetch = frozen_fetch;
    entry.snapshot_result = NormalizeSnapshot(frozen_snapshot, entry.snapshot);
    entry.issued = true;
    entries_[issued_++] = entry;
    Encode(frozen_stock, token, output.payload, true);
    output.token = token;
    output.snapshot = entry.snapshot;
    output.snapshot_result = entry.snapshot_result;
    // A matched transport token is useful even when snapshot data is absent.
    // Its availability/reason remain separate; no timing provenance follows.
    return Result::kTokenIssued;
  }

  Result consume(std::span<const uint32_t> payload, std::span<const uint32_t> fetch,
                 Match& output) noexcept {
    Decoded packet;
    const Result decoded = Decode(payload, packet);
    if (decoded != Result::kExtendedPacket) { output = {}; return decoded; }
    std::array<uint32_t, kFetchWords> frozen_fetch{};
    if (fetch.size() == kFetchWords)
      for (size_t i = 0; i < kFetchWords; ++i) frozen_fetch[i] = fetch[i];
    output = {};
    if (!enabled_ || packet.token.epoch != epoch_) return Result::kTokenEpochMismatch;
    if (!packet.token.serial || packet.token.serial > issued_) return Result::kUnissuedToken;
    // Range checks precede indexing; serial is bounded by actual issuance.
    auto& entry = entries_[size_t(packet.token.serial - 1)];
    if (!entry.issued) return Result::kUnissuedToken;
    if (entry.consumed) return Result::kReplay;
    // A recognizable issued token gets ONE consume attempt, including a bad
    // stock/fetch binding. A repaired/retried packet must never gain admission.
    entry.consumed = true;
    if (fetch.size() != kFetchWords) return Result::kInvalidFetchWordCount;
    for (size_t i = 0; i < kStockWords; ++i)
      if (packet.stock[i] != entry.stock[i]) return Result::kStockMismatch;
    for (size_t i = 0; i < kFetchWords; ++i)
      if (frozen_fetch[i] != entry.fetch[i]) return Result::kFetchMismatch;
    output.token = packet.token;
    output.snapshot = entry.snapshot;
    output.snapshot_result = entry.snapshot_result;
    return Result::kTokenMatched;
  }

 private:
  struct Entry {
    std::array<uint32_t, kStockWords> stock{};
    std::array<uint32_t, kFetchWords> fetch{};
    Snapshot snapshot;
    SnapshotResult snapshot_result = SnapshotResult::kUnavailable;
    bool issued = false, consumed = false;
  };
  const uint64_t epoch_;
  const bool enabled_;
  std::array<Entry, Capacity> entries_{};
  size_t issued_ = 0;
};
}  // namespace rex::diagnostics::swap_emission
