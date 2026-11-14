// C++20 in-process mapping/storage prerequisite. No exports, DLL, SDK or game.
// Primary owns compilation and execution; all full-size inventories are heap
// owned. C-wire comparison enumerates fields, never casts/memcmps C++ Core ABI.
#include "../src/mcla_clock_commit_history_transport.h"
#include <cstring>
#include <iostream>
#include <limits>
#include <memory>
#include <type_traits>
#include <utility>

namespace {
namespace h = mcla::clock_commit_history;
namespace t = h::transport;
unsigned checks{}, failures{};
void Check(bool value, const char* label) {
  ++checks; if (!value) { ++failures; std::cerr << "FAIL " << label << '\n'; }
}
constexpr h::RunToken run{0x1122334455667788ull, 0x99AABBCCDDEEFF01ull};
constexpr uint64_t max64 = std::numeric_limits<uint64_t>::max();
constexpr uint32_t max32 = std::numeric_limits<uint32_t>::max();
struct GuardedWire {
  uint64_t before;
  MclaClockHistorySnapshotV1 wire;
  uint64_t after;
};
struct Owned {
  std::unique_ptr<t::Core> core{new t::Core};
  std::unique_ptr<t::Core::Snapshot> scratch{new t::Core::Snapshot};
  std::unique_ptr<t::Core::Snapshot> independent{new t::Core::Snapshot};
  std::unique_ptr<GuardedWire> output{new GuardedWire};
  t::Transport transport{*core, *scratch};
  Owned() {
    std::memset(scratch.get(), 0, sizeof(*scratch));
    std::memset(independent.get(), 0, sizeof(*independent));
    output->before = 0x0123456789ABCDEFull; output->after = 0xFEDCBA9876543210ull;
    std::memset(&output->wire, 0, sizeof(output->wire));
    output->wire.abi_version = MCLA_CH_ABI_V1;
    output->wire.struct_size = sizeof(output->wire);
  }
  bool Guards() const { return output->before == 0x0123456789ABCDEFull && output->after == 0xFEDCBA9876543210ull; }
};
h::Binding Bound(uint64_t timer = 7, uint64_t read = 0) {
  return {run.high, run.low, timer, read, 11, 13, 17, 1, 1, 0};
}
h::Metadata Advance(uint64_t value = 100) {
  return {value, value * 5, value - 1, value * 5 - 5, value, value * 5, 5, 1, 5, 1, h::kQueryFields, 0};
}
h::Metadata Ratio() { return {0, 0, 100, 500, 100, 500, 5, 1, 3, 2, h::kRatioFields, 0}; }
h::Metadata Unordered() { return {101, 505, 0, 0, 0, 0, 5, 1, 0, 0, h::kUnorderedFields, 0}; }

bool BindingEqual(const h::Binding& a, const MclaClockHistoryBindingV1& b) {
  return a.run_high == b.run_high && a.run_low == b.run_low && a.timer_invocation == b.timer_invocation &&
    a.read_ordinal == b.read_ordinal && a.phase_call_id == b.phase_call_id && a.phase_thread_epoch == b.phase_thread_epoch &&
    a.phase_native_thread_id == b.phase_native_thread_id && a.phase_present == b.phase_present &&
    a.present == b.present && a.reserved == b.reserved;
}
bool MetadataEqual(const h::Metadata& a, const MclaClockHistoryMetadataV1& b) {
  return a.host_sample == b.host_sample && a.returned_guest == b.returned_guest && a.pre_host == b.pre_host &&
    a.pre_guest == b.pre_guest && a.post_host == b.post_host && a.post_guest == b.post_guest &&
    a.pre_ratio_numerator == b.pre_ratio_numerator && a.pre_ratio_denominator == b.pre_ratio_denominator &&
    a.post_ratio_numerator == b.post_ratio_numerator && a.post_ratio_denominator == b.post_ratio_denominator &&
    a.presence == b.presence && a.reserved == b.reserved;
}
bool RecordEqual(const h::Record& a, const MclaClockHistoryRecordV1& b) {
  return a.storage_ordinal == b.storage_ordinal && a.mutex_ordinal == b.mutex_ordinal &&
    uint32_t(a.kind) == b.kind && uint32_t(a.ordering) == b.ordering && BindingEqual(a.binding, b.binding) &&
    MetadataEqual(a.metadata, b.metadata) && uint32_t(a.disposition) == b.disposition && a.issues == b.issues;
}
bool FullEqual(const t::Core::Snapshot& a, const MclaClockHistorySnapshotV1& b) {
  const auto& c = a.counts;
  if (b.abi_version != MCLA_CH_ABI_V1 || b.struct_size != sizeof(b) || b.capacity != MCLA_CH_CAPACITY_V1 ||
      b.row_count != a.size || b.run_high != a.run.high || b.run_low != a.run.low ||
      b.observed_attempts != c.observed_attempts || b.mutex_ordinals != c.mutex_ordinals ||
      b.unordered_attempts != c.unordered_attempts || b.stored_entries != c.stored_entries ||
      b.published_entries != c.published_entries || b.aborted_entries != c.aborted_entries ||
      b.overflow_entries != c.overflow_entries || b.diagnostic_entries != c.diagnostic_entries ||
      b.saturated_rejections != c.saturated_rejections || b.counter_saturated != c.counter_saturated ||
      b.counts_reserved || b.proof_flags || b.reserved) return false;
  // Includes ALL unused rows and every raw field; not a struct layout cast.
  for (size_t i = 0; i < MCLA_CH_CAPACITY_V1; ++i) if (!RecordEqual(a.rows[i], b.rows[i])) return false;
  return true;
}
void Compare(Owned& value) {
  Check(value.core->TryFreezeInto(*value.independent), "independent core freeze succeeds after drain");
  Check(FullEqual(*value.independent, value.output->wire), "all core envelope counters and 4096 row fields match wire");
  Check(value.Guards(), "wire guard sentinels survive full transport copy");
}

void InfoAndBadPrefixes() {
  static_assert(sizeof(t::Transport) == 2 * sizeof(void*));
  static_assert(!std::is_copy_constructible_v<t::Transport>);
  Owned value;
  struct GuardedInfo { uint64_t before; MclaClockHistoryInfoV1 info; uint64_t after; } info{};
  info.before = 101; info.after = 202; info.info.abi_version = MCLA_CH_ABI_V1; info.info.struct_size = sizeof(info.info);
  Check(value.transport.Info(&info.info) == MCLA_CH_OK, "Info correct prefix succeeds");
  const auto& i = info.info;
  Check(i.abi_version == 1 && i.struct_size == 72 && i.binding_version == 1 && i.binding_size == 64 &&
        i.metadata_version == 1 && i.metadata_size == 88 && i.record_version == 1 && i.record_size == 184 &&
        i.snapshot_version == 1 && i.snapshot_size == 753784 && i.rows_offset == 120 && i.capacity == 4096 &&
        !i.proof_flags && !i.reserved && !i.reserved_words[0] && !i.reserved_words[1] && info.before == 101 && info.after == 202,
        "Info enumerates exact independent sizes versions offsets zero proofs and guards");
  Check(value.transport.Info(nullptr) == MCLA_CH_BAD_ABI && value.transport.Snapshot(nullptr) == MCLA_CH_BAD_ABI,
        "null pointers reject before access");
  info.info.abi_version = 2; auto before = info;
  Check(value.transport.Info(&info.info) == MCLA_CH_BAD_ABI && std::memcmp(&before, &info, sizeof(info)) == 0,
        "wrong Info version leaves all bytes and guards unchanged");
  info.info.abi_version = 1; info.info.struct_size = 8; before = info;
  Check(value.transport.Info(&info.info) == MCLA_CH_BAD_ABI && std::memcmp(&before, &info, sizeof(info)) == 0,
        "wrong Info size leaves all bytes and guards unchanged");
  alignas(8) unsigned char small[24]; unsigned char small_before[24];
  std::memset(small, 0xA7, sizeof(small));
  const uint32_t prefix[2]{1, 8}; std::memcpy(small + 8, prefix, sizeof(prefix));
  std::memcpy(small_before, small, sizeof(small));
  Check(value.transport.Snapshot(reinterpret_cast<MclaClockHistorySnapshotV1*>(small + 8)) == MCLA_CH_BAD_ABI &&
        std::memcmp(small, small_before, sizeof(small)) == 0, "small aligned old prefix rejects without oversized write");
  Check(value.transport.Info(reinterpret_cast<MclaClockHistoryInfoV1*>(small + 8)) == MCLA_CH_BAD_ABI &&
        std::memcmp(small, small_before, sizeof(small)) == 0, "small aligned Info prefix rejects without oversized write");
  auto saved = std::unique_ptr<GuardedWire>(new GuardedWire);
  value.output->wire.abi_version = 77; std::memcpy(saved.get(), value.output.get(), sizeof(GuardedWire));
  Check(value.transport.Snapshot(&value.output->wire) == MCLA_CH_BAD_ABI &&
        std::memcmp(saved.get(), value.output.get(), sizeof(GuardedWire)) == 0, "wrong Snapshot version never mutates output");
  value.output->wire.abi_version = 1; value.output->wire.struct_size = 655480;
  std::memcpy(saved.get(), value.output.get(), sizeof(GuardedWire));
  Check(value.transport.Snapshot(&value.output->wire) == MCLA_CH_BAD_ABI &&
        std::memcmp(saved.get(), value.output.get(), sizeof(GuardedWire)) == 0, "old witness-size Snapshot never mutates output");
}

void UnalignedGuards() {
  Owned value;
  constexpr size_t bytes = sizeof(MclaClockHistorySnapshotV1) + 24;
  std::unique_ptr<unsigned char[]> buffer(new unsigned char[bytes]), before(new unsigned char[bytes]);
  std::memset(buffer.get(), 0x6D, bytes);
  const uint32_t info_prefix[2]{1, sizeof(MclaClockHistoryInfoV1)};
  std::memcpy(buffer.get() + 9, info_prefix, sizeof(info_prefix)); std::memcpy(before.get(), buffer.get(), bytes);
  Check(reinterpret_cast<uintptr_t>(buffer.get() + 9) % 8 != 0 &&
        value.transport.Info(reinterpret_cast<MclaClockHistoryInfoV1*>(buffer.get() + 9)) == MCLA_CH_BAD_ABI &&
        std::memcmp(buffer.get(), before.get(), bytes) == 0, "unaligned Info rejects without prefix or guard mutation");
  const uint32_t snapshot_prefix[2]{1, sizeof(MclaClockHistorySnapshotV1)};
  std::memcpy(buffer.get() + 9, snapshot_prefix, sizeof(snapshot_prefix)); std::memcpy(before.get(), buffer.get(), bytes);
  Check(value.transport.Snapshot(reinterpret_cast<MclaClockHistorySnapshotV1*>(buffer.get() + 9)) == MCLA_CH_BAD_ABI &&
        std::memcmp(buffer.get(), before.get(), bytes) == 0, "unaligned Snapshot rejects without any large buffer mutation");
}

void OnceStopAndPending() {
  Owned value;
  auto before = std::unique_ptr<GuardedWire>(new GuardedWire);
  std::memcpy(before.get(), value.output.get(), sizeof(GuardedWire));
  Check(value.transport.Snapshot(&value.output->wire) == MCLA_CH_PENDING &&
        std::memcmp(before.get(), value.output.get(), sizeof(GuardedWire)) == 0, "OFF Snapshot PENDING preserves entire first zero input and sentinel guards");
  Check(value.transport.Arm(0, 0) == MCLA_CH_REJECTED && value.transport.Arm(run.high, run.low) == MCLA_CH_OK,
        "zero run rejection does not consume valid once Arm");
  Check(value.transport.Arm(run.high, run.low) == MCLA_CH_REJECTED, "second Arm rejects without reset");
  auto held = value.core->BeginLocked(h::Kind::Advance, Bound());
  value.transport.Stop(); value.transport.Stop();
  Check(value.transport.Snapshot(&value.output->wire) == MCLA_CH_PENDING &&
        std::memcmp(before.get(), value.output.get(), sizeof(GuardedWire)) == 0, "held lease Stop is nonblocking and leaves every pending output byte unchanged");
  Check(held.Publish(Advance()) && value.transport.Snapshot(&value.output->wire) == MCLA_CH_OK,
        "already admitted publisher drains after Stop before copy");
  Compare(value);
  std::memcpy(before.get(), value.output.get(), sizeof(GuardedWire));
  Check(value.transport.Snapshot(&value.output->wire) == MCLA_CH_OK &&
        std::memcmp(before.get(), value.output.get(), sizeof(GuardedWire)) == 0, "unchanged prior successful envelope supports repeat byte-stable Snapshot");
  Check(value.transport.Arm(run.high, run.low + 1) == MCLA_CH_REJECTED &&
        value.core->BeginUnordered().status() == h::StartStatus::Stopped, "drained transport never reopens recorder");
  Owned stopped; stopped.transport.Stop();
  Check(stopped.transport.Arm(run.high, run.low) == MCLA_CH_REJECTED &&
        stopped.transport.Snapshot(&stopped.output->wire) == MCLA_CH_OK && stopped.output->wire.row_count == 0,
        "Stop-before-Arm permits empty frozen wire and forbids Arm");
  Compare(stopped);
}

void RawRowsAndMapping() {
  Owned value; Check(value.transport.Arm(run.high, run.low) == MCLA_CH_OK, "raw mapping Arm");
  auto binding = Bound(max64, max64 - 1);
  binding.run_high = 101; binding.run_low = 103; binding.phase_call_id = 107; binding.phase_thread_epoch = 109;
  binding.phase_native_thread_id = 113; binding.phase_present = 127; binding.present = 131; binding.reserved = max32;
  h::Metadata metadata{137, 139, 149, 151, 157, 163, 167, 173, 179, 181, 0xFEDCBA98u, 0x87654321u};
  Check(value.core->BeginLocked(static_cast<h::Kind>(0xDEADBEEFu), binding).Publish(metadata), "unknown kind malformed raw request metadata admitted for retention");
  Check(value.core->BeginUnordered().Publish(Unordered()), "explicit unbound unordered admitted");
  Check(value.core->BeginLocked(h::Kind::Ratio).Publish(Ratio()), "unbound ratio shares locked ordinal");
  { auto abort = value.core->BeginLocked(h::Kind::Cached, Bound(19)); (void)abort; }
  value.transport.Stop();
  Check(value.transport.Snapshot(&value.output->wire) == MCLA_CH_OK, "raw mapping Snapshot");
  Compare(value);
  const auto& out = value.output->wire;
  Check(out.row_count == 4 && out.rows[0].kind == 0xDEADBEEFu && out.rows[0].binding.reserved == max32 &&
        out.rows[0].metadata.reserved == 0x87654321u && out.rows[0].metadata.presence == 0xFEDCBA98u && out.rows[0].issues,
        "unknown kind presence issues and row reserved fields survive exactly");
  Check(out.rows[1].storage_ordinal == 2 && out.rows[1].mutex_ordinal == 0 && !out.rows[1].binding.present &&
        out.rows[2].storage_ordinal == 3 && out.rows[2].mutex_ordinal == 2 && !out.rows[2].binding.present &&
        out.rows[3].disposition == MCLA_CH_ABORTED, "wire preserves explicit unbound and unordered storage versus mutex identities and abort");
  Check(!out.counts_reserved && !out.reserved && !out.proof_flags, "envelope reserved and proofs are zero independently of raw row reserved");

  // Synthetic field-copy vocabulary test ONLY. Recorder does not emit arbitrary
  // ordering/disposition/issues values through its public API. Exercise those
  // conversions explicitly without forging recorder storage or a DLL receipt.
  h::Record raw{max64, max64 - 1, static_cast<h::Kind>(0xAABBCCDDu), static_cast<h::Ordering>(0x99887766u),
      binding, metadata, static_cast<h::Disposition>(0x55667788u), 0xF0E1D2C3u};
  MclaClockHistoryRecordV1 copied{}; t::detail::CopyRecord(raw, copied);
  Check(RecordEqual(raw, copied), "synthetic unknown raw ordering disposition and issue vocabulary copied without masking");
#if defined(MCLA_CLOCK_COMMIT_TRANSPORT_FIELD_MUTANT)
  // TEST-ONLY after-return corruption of ONE field. No live producer mutation,
  // FFI execution or assertion of a production transport bug.
  value.output->wire.rows[0].metadata.returned_guest ^= 1;
#endif
  Check(FullEqual(*value.independent, value.output->wire), "transport returned_guest field mapping lost raw data");
}

void MaximumInventory() {
  Owned value; Check(value.transport.Arm(run.high, run.low) == MCLA_CH_OK, "4096 mapping Arm");
  bool stored = true;
  for (uint64_t i = 0; i < MCLA_CH_CAPACITY_V1; ++i)
    stored = value.core->BeginLocked(h::Kind::Advance, Bound(i + 1, i)).Publish(Advance(i + 101)) && stored;
  Check(stored && value.core->BeginLocked(h::Kind::Ratio).status() == h::StartStatus::Overflow,
        "exact real core 4096 capacity then one explicit overflow");
  value.transport.Stop();
  Check(value.transport.Snapshot(&value.output->wire) == MCLA_CH_OK && value.output->wire.row_count == 4096 &&
        value.output->wire.mutex_ordinals == 4097 && value.output->wire.observed_attempts == 4097 &&
        value.output->wire.overflow_entries == 1, "wire retains full capacity plus unrecorded mutex overflow count");
  Compare(value);
  bool all = true;
  for (uint64_t i = 0; i < MCLA_CH_CAPACITY_V1; ++i) {
    const auto& row = value.output->wire.rows[i];
    all = all && row.storage_ordinal == i + 1 && row.mutex_ordinal == i + 1 && row.binding.timer_invocation == i + 1 &&
        row.binding.read_ordinal == i && row.metadata.host_sample == i + 101 && row.metadata.returned_guest == 5 * (i + 101);
  }
  Check(all, "all 4096 independent request and metadata row markers remain in order");
  auto before = std::unique_ptr<GuardedWire>(new GuardedWire);
  std::memcpy(before.get(), value.output.get(), sizeof(GuardedWire));
  Check(value.transport.Snapshot(&value.output->wire) == MCLA_CH_OK &&
        std::memcmp(before.get(), value.output.get(), sizeof(GuardedWire)) == 0, "full-capacity repeat Snapshot is byte stable");
}
} // namespace
int main() {
  InfoAndBadPrefixes(); UnalignedGuards(); OnceStopAndPending(); RawRowsAndMapping(); MaximumInventory();
  Check(!t::kFfiAdmitted && !t::kDllAdmitted && !t::kSdkBuildReady && !t::kSourceRoutedCoverageProven &&
        !t::kCompleteClockHistoryProven && !t::kOriginalSemanticsProven && !t::kNativeStatePreservationProven &&
        !t::kSimulationProven && !t::kFpsProven, "wire transport never creates engineering proof");
  std::cout << "checks=" << checks << " failures=" << failures << '\n';
  return failures ? 1 : 0;
}
