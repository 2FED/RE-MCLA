#include "../scripts/diagnostics/mcla_atlas_upload_policy.h"

#include <array>
#include <iostream>
#include <limits>

namespace {
using namespace mcla::diagnostics::atlas_upload;
unsigned checks = 0, failures = 0;
void Check(bool value, const char* label) {
  ++checks;
  if (!value) { ++failures; std::cerr << "FAIL " << label << '\n'; }
}
// Real backing storage for all spans, no fake oversized pointer ranges.
const std::array<std::uint8_t, 2 * kBaseBytes> storage{};
std::span<const std::uint8_t> Bytes(std::size_t count) { return {storage.data(), count}; }
constexpr auto max64 = std::numeric_limits<std::uint64_t>::max();
constexpr auto max32 = std::numeric_limits<std::uint32_t>::max();

void RangesAndDisabled() {
  static_assert(kBaseBytes == 1048576 && kRetainedBytesLimit == 2097152);
  static_assert(PhysicalRange(0, kPhysicalBytes));
  Check(PhysicalRange(kPhysicalBytes, 0), "empty physical endpoint valid");
  Check(PhysicalRange(kPhysicalBytes - kBaseBytes, kBaseBytes), "last complete base range valid");
  Check(!PhysicalRange(kPhysicalBytes, 1), "one byte over physical endpoint rejected");
  Check(!PhysicalRange(max64, 1) && !PhysicalRange(1, max64), "addition overflow rejected by subtraction");
  Check(!PhysicalRange(max64, max64) && !PhysicalRange(kPhysicalBytes + 1, 0), "invalid empty and wrapped ranges rejected");
  Policy p;
  for (unsigned i = 0; i < 10; ++i) {
    Check(p.Arm(99, 0, max64, 0).kind == Kind::Disabled, "disabled malformed arm no-op");
    Check(p.ObserveQueuedUpload(max64, Bytes(100)).kind == Kind::Disabled, "disabled malformed upload no-op");
    Check(p.Complete(0, max32).kind == Kind::Disabled, "disabled complete no-op");
    Check(p.Fail().kind == Kind::Disabled, "disabled failure no-op");
  }
  Check(!p.enabled() && !p.active() && !p.incomplete() && !p.inconclusive_seen() &&
        p.failure() == Failure::None && p.arms() == 0 && p.retained_bytes() == 0 &&
        p.covered_bytes() == 0 && p.TakeFailureNotification() == Failure::None,
        "disabled all counters and state untouched");
  const auto r = p.ObserveQueuedUpload(0, Bytes(kBaseBytes));
  Check(r.id == 0 && r.count == 0 && r.source_offset == 0 && r.destination_offset == 0,
        "disabled authorizes no retention");
}
void FullSplitAndCrossing() {
  Policy p(true);
  Check(p.ObserveQueuedUpload(0, Bytes(10)).kind == Kind::Ignored, "unarmed ordinary upload ignored");
  auto r = p.Arm(1, 1, 4096, kBaseBytes);
  Check(r.kind == Kind::Armed && r.id == 1 && r.generation == 1 && r.arm == 1 && p.incomplete(), "exact arm metadata");
  Check(p.ObserveQueuedUpload(4096, {}).kind == Kind::Ignored && p.covered_bytes() == 0, "empty upload does not cover bytes");
  Check(p.ObserveQueuedUpload(0, Bytes(4096)).kind == Kind::Ignored, "end touching base has no intersection");
  Check(p.ObserveQueuedUpload(4096 + kBaseBytes, Bytes(1)).kind == Kind::Ignored, "start touching end has no intersection");
  r = p.ObserveQueuedUpload(0, Bytes(4096 + 128));
  Check(r.kind == Kind::Fragment && r.source_offset == 4096 && r.destination_offset == 0 && r.count == 128,
        "crossing start clips unrelated prefix");
  r = p.ObserveQueuedUpload(4096 + 128, Bytes(kBaseBytes - 128 + 4096));
  Check(r.kind == Kind::Fragment && r.source_offset == 0 && r.destination_offset == 128 &&
        r.count == kBaseBytes - 128 && r.covered == kBaseBytes, "crossing end clips unrelated suffix");
  Check(p.active() && p.incomplete(), "coverage alone is not matching load completion");
  r = p.Complete(1, 1);
  Check(r.kind == Kind::FullCoverage && r.covered == kBaseBytes && !p.active() && !p.incomplete(), "full coverage closes arm");
  Check(p.retained_bytes() == kBaseBytes, "only intersections counted");
  Check(p.Arm(1, 2, 4096, kBaseBytes).kind == Kind::Armed, "second generation same immutable base");
  r = p.ObserveQueuedUpload(4096, Bytes(kBaseBytes));
  Check(r.kind == Kind::Fragment && r.count == kBaseBytes && r.generation == 2 && r.arm == 2, "second generation full upload");
  Check(p.Complete(1, 2).kind == Kind::FullCoverage && p.retained_bytes() == 2 * kBaseBytes, "two generations exact lifetime cap");
  Check(p.Arm(1, 3, 0, kBaseBytes).kind == Kind::Failed && p.failure() == Failure::GenerationLimit,
        "third arm never resets or reuses budget");
  Policy edge(true);
  Check(edge.Arm(1, 1, kPhysicalBytes - kBaseBytes, kBaseBytes).kind == Kind::Armed, "last physical base arm admitted");
  Check(edge.ObserveQueuedUpload(kPhysicalBytes - kBaseBytes, Bytes(kBaseBytes)).kind == Kind::Fragment &&
        edge.Complete(1, 1).kind == Kind::FullCoverage, "last physical base exactly covered");
  Policy enclosing(true); enclosing.Arm(1, 1, 4096, kBaseBytes);
  const auto large = enclosing.ObserveQueuedUpload(0, Bytes(2 * kBaseBytes));
  Check(large.kind == Kind::Fragment && large.source_offset == 4096 && large.destination_offset == 0 &&
        large.count == kBaseBytes && enclosing.retained_bytes() == kBaseBytes,
        "larger chunk retains only selected intersection");
}
void PartialAndResident() {
  for (const auto count : {std::uint64_t(0), std::uint64_t(1), kBaseBytes - 1}) {
    Policy p(true); p.Arm(1, 1, 0, kBaseBytes);
    if (count) Check(p.ObserveQueuedUpload(0, Bytes(count)).kind == Kind::Fragment, "partial uploaded prefix observed");
    const auto r = p.Complete(1, 1);
    Check(r.kind == Kind::Inconclusive && r.covered == count && p.inconclusive_seen() &&
          p.incomplete() && !p.active() && p.failure() == Failure::None,
          "partial and resident-only data never accepted full");
    Check(p.Arm(1, 2, 0, kBaseBytes).kind == Kind::Armed, "inconclusive first arm permits second generation");
    p.ObserveQueuedUpload(0, Bytes(kBaseBytes));
    Check(p.Complete(1, 2).kind == Kind::FullCoverage && p.inconclusive_seen() && p.incomplete() &&
          p.retained_bytes() == kBaseBytes + count, "later full generation never erases earlier inconclusive");
  }
}
void InvalidArmsAndIdentity() {
  for (const auto id : {0u, 2u, max32}) {
    Policy p(true);
    Check(p.Arm(id, 1, 0, kBaseBytes).kind == Kind::Failed && p.failure() == Failure::InvalidIdentity,
          "only selected ordinal admitted");
  }
  for (const auto size : {std::uint64_t(0), kBaseBytes - 1, kBaseBytes + 1, max64}) {
    Policy p(true);
    Check(p.Arm(1, 1, 0, size).kind == Kind::Failed && p.failure() == Failure::InvalidRange, "exact one MiB only");
  }
  for (const auto base : {kPhysicalBytes - kBaseBytes + 1, kPhysicalBytes, max64}) {
    Policy p(true);
    Check(p.Arm(1, 1, base, kBaseBytes).kind == Kind::Failed, "base physical bounds fail closed");
  }
  Policy zero(true); Check(zero.Arm(1, 0, 0, kBaseBytes).kind == Kind::Failed, "zero generation rejected");
  Policy active(true); active.Arm(1, 1, 0, kBaseBytes);
  Check(active.Arm(1, 2, kBaseBytes, kBaseBytes).kind == Kind::Failed && active.failure() == Failure::ActiveConflict,
        "active arm cannot be replaced or aliased");
  for (const auto generation : {0u, 2u, max32}) {
    Policy p(true); p.Arm(1, 1, 0, kBaseBytes);
    Check(p.Complete(1, generation).kind == Kind::Failed, "wrong generation cannot complete");
  }
  Policy wrongId(true); wrongId.Arm(1, 1, 0, kBaseBytes);
  Check(wrongId.Complete(2, 1).kind == Kind::Failed, "wrong ordinal cannot complete");
  Policy absent(true); Check(absent.Complete(1, 1).kind == Kind::Failed, "completion without arm rejected");
  Policy stale(true); stale.Arm(1, 1, 0, kBaseBytes); stale.Complete(1, 1);
  Check(stale.Arm(1, 1, 0, kBaseBytes).kind == Kind::Failed, "same generation cannot be reused");
  for (const auto gen : {0u, 2u, max32}) {
    Policy skipped(true);
    Check(skipped.Arm(1, gen, 0, kBaseBytes).kind == Kind::Failed, "first generation must be exactly one");
  }
  for (const auto gen : {0u, 1u, 3u, max32}) {
    Policy skipped(true); skipped.Arm(1, 1, 0, kBaseBytes); skipped.Complete(1, 1);
    Check(skipped.Arm(1, gen, 0, kBaseBytes).kind == Kind::Failed, "second generation must be exactly two");
  }
  Policy moved(true); moved.Arm(1, 1, 0, kBaseBytes); moved.Complete(1, 1);
  Check(moved.Arm(1, 2, 4096, kBaseBytes).kind == Kind::Failed && moved.failure() == Failure::InvalidIdentity,
        "same ordinal cannot alias a new base address");
  Policy duplicate(true); duplicate.Arm(1, 1, 0, kBaseBytes); duplicate.Complete(1, 1);
  Check(duplicate.Complete(1, 1).kind == Kind::Failed, "duplicate completion rejected");
}
void FragmentFailuresAndStickyState() {
  for (unsigned scenario = 0; scenario < 5; ++scenario) {
    Policy p(true); p.Arm(1, 1, 4096, kBaseBytes);
    if (scenario != 0) p.ObserveQueuedUpload(4096, Bytes(scenario == 4 ? kBaseBytes : 128));
    Result r;
    if (scenario == 0) r = p.ObserveQueuedUpload(4097, Bytes(128)); // initial gap
    if (scenario == 1) r = p.ObserveQueuedUpload(4096 + 129, Bytes(128)); // middle gap
    if (scenario == 2) r = p.ObserveQueuedUpload(4096 + 127, Bytes(128)); // one-byte overlap
    if (scenario == 3) r = p.ObserveQueuedUpload(0, Bytes(4096 + 128)); // old prefix
    if (scenario == 4) r = p.ObserveQueuedUpload(4096, Bytes(kBaseBytes)); // full duplicate
    Check(r.kind == Kind::Failed && p.failure() == Failure::FragmentOrder, "noncontiguous duplicate or out-of-order fails");
    const auto retained = p.retained_bytes();
    Check(p.Complete(1, 1).kind == Kind::Failed && p.ObserveQueuedUpload(4096, Bytes(kBaseBytes)).kind == Kind::Failed &&
          p.Arm(1, 2, 0, kBaseBytes).kind == Kind::Failed && p.retained_bytes() == retained,
          "failure permanently prevents ambiguous full result and more bytes");
    Check(p.TakeFailureNotification() == Failure::FragmentOrder &&
          p.TakeFailureNotification() == Failure::None, "failure notified once");
    p.Fail(); Check(p.failure() == Failure::FragmentOrder, "first error retained");
  }
  for (const auto base : {kPhysicalBytes, max64}) {
    Policy p(true); p.Arm(1, 1, 0, kBaseBytes);
    Check(p.ObserveQueuedUpload(base, Bytes(1)).kind == Kind::Failed && p.failure() == Failure::InvalidRange,
          "upload chunk outside physical space rejected even if unrelated");
  }
  Policy external(true); external.Arm(1, 1, 0, kBaseBytes);
  external.ObserveQueuedUpload(0, Bytes(kBaseBytes)); external.Fail();
  Check(external.Complete(1, 1).kind == Kind::Failed && external.incomplete(),
        "caller retention failure cannot report full coverage");
  Policy invalidReason(true); invalidReason.Fail(static_cast<Failure>(999));
  Check(invalidReason.failure() == Failure::ExternalFailure, "invalid failure reason stays failed");
}
}  // namespace
int main() {
  RangesAndDisabled(); FullSplitAndCrossing(); PartialAndResident();
  InvalidArmsAndIdentity(); FragmentFailuresAndStickyState();
  std::cout << checks << " checks, " << failures << " failures\n";
  return failures ? 1 : 0;
}
