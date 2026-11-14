#include "../scripts/diagnostics/mcla_atlas_producer_policy.h"
#include <cstdint>
#include <iostream>
#include <limits>

namespace {
using namespace mcla::diagnostics::atlas_producer;
unsigned checks = 0, failures = 0;
void Check(bool value, const char* name) {
  ++checks;
  if (!value) { ++failures; std::cerr << "FAIL " << name << '\n'; }
}
Event Hit(EventKind kind = EventKind::ResolveQueued) {
  return {kind, kBase, kLength, 7, 11, false, 0};
}
void Tests() {
  {
    Policy p;
    Event invalid{static_cast<EventKind>(255), UINT32_MAX, UINT32_MAX, 0, 0, true, UINT64_MAX};
    Check(p.Observe(invalid).outcome == Outcome::Ignored, "disabled invalid event noop");
    Check(p.Finish(0, 0, false).outcome == Outcome::Ignored, "disabled invalid checkpoint noop");
    p.Fail();
    Check(!p.TakeFailureNotification() && p.failure() == Failure::None && p.events() == 0,
          "disabled no failure or count mutation");
    Check(!p.active() && !p.complete() && !p.incomplete(), "disabled state");
  }
  {
    Policy p(true);
    Check(p.active() && p.incomplete() && !p.complete(), "missing checkpoint not zero-hit completion");
    Event before = Hit(); before.base = kBase - 1; before.length = 1;
    Event after = Hit(); after.base = kBase + kLength; after.length = 1;
    Check(p.Observe(before).outcome == Outcome::Ignored, "left exclusive adjacency");
    Check(p.Observe(after).outcome == Outcome::Ignored, "right exclusive adjacency");
    Check(p.events() == 0 && p.Finish(1, 1, true).outcome == Outcome::Finished && p.complete(),
          "explicit finished zero hits");
    Check(p.Observe({static_cast<EventKind>(255), UINT32_MAX, 0}).outcome == Outcome::Ignored &&
              p.complete() && p.events() == 0, "postfinish ordinary events ignored including unexamined metadata");
    Check(p.Finish(1, 1, true).outcome == Outcome::Failed &&
              p.failure() == Failure::DuplicateFinish && !p.complete(), "duplicate checkpoint sticky failure");
  }
  for (auto kind : {EventKind::ResolveQueued, EventKind::ResolveCpuCopy,
                   EventKind::MemexportCpuCopyFull, EventKind::MemexportCpuCopyFast,
                   EventKind::DeclaredGpuRange}) {
    Policy p(true);
    const auto r = p.Observe(Hit(kind));
    Check(r.outcome == Outcome::Recorded && r.ordinal == 1 && r.overlap_base == kBase &&
              r.overlap_length == kLength && !r.coarse_unknown_range, "each kind exact interval");
    Check(p.Finish(1, 1, true).outcome == Outcome::Finished && p.complete(), "each kind finish");
  }
  {
    Policy p(true);
    Event e = Hit(); e.base = kBase - 99; e.length = 100;
    auto r = p.Observe(e);
    Check(r.overlap_base == kBase && r.overlap_length == 1 && r.ordinal == 1, "left crossing clipped");
    e.base = kBase + kLength - 1; e.length = 100;
    r = p.Observe(e);
    Check(r.overlap_base == kBase + kLength - 1 && r.overlap_length == 1 && r.ordinal == 2,
          "right crossing clipped");
    e = {EventKind::DeclaredGpuRange, 0, kPhysicalBytes};
    r = p.Observe(e);
    Check(r.overlap_length == kLength && r.coarse_unknown_range, "whole memory notification explicitly coarse");
    e.kind = EventKind::ResolveQueued;
    Check(!p.Observe(e).coarse_unknown_range, "coarse marker only declaration kind");
  }
  for (const auto pair : {Event{EventKind::ResolveQueued, kBase, 0},
                          Event{EventKind::ResolveQueued, kPhysicalBytes, 1},
                          Event{EventKind::ResolveQueued, kPhysicalBytes - 1, 2},
                          Event{EventKind::ResolveQueued, UINT32_MAX, UINT32_MAX},
                          Event{EventKind::ResolveQueued, 1, UINT32_MAX}}) {
    Policy p(true);
    Check(p.Observe(pair).outcome == Outcome::Failed && p.failure() == Failure::InvalidRange,
          "invalid physical range fail closed");
    Check(p.Observe(Hit()).outcome == Outcome::Failed && p.events() == 0 &&
              p.Finish(1, 1, true).outcome == Outcome::Failed, "range failure sticky");
    Check(p.TakeFailureNotification() && !p.TakeFailureNotification(), "one failure notification");
  }
  {
    Policy p(true);
    Event e = Hit(); e.base = kPhysicalBytes - 1; e.length = 1;
    Check(p.Observe(e).outcome == Outcome::Ignored && p.failure() == Failure::None,
          "last physical byte valid nonoverlap");
    for (uint32_t i = 1; i <= kEventLimit; ++i)
      Check(p.Observe(Hit()).ordinal == i, "64 monotonic event ordinals");
    Check(p.Observe(e).outcome == Outcome::Ignored && p.events() == 64, "nonoverlap does not consume cap");
    Check(p.Observe(Hit()).outcome == Outcome::Failed && p.failure() == Failure::EventLimit &&
              p.events() == 64 && !p.complete(), "65th overlapping event cannot silently drop");
  }
  {
    Policy p(true);
    for (uint32_t i = 0; i < kEventLimit; ++i) p.Observe(Hit());
    Check(p.Finish(1, 1, true).outcome == Outcome::Finished && p.complete(), "64 records can finish");
    p.Fail();
    Check(!p.complete() && p.incomplete() && p.failure() == Failure::External,
          "publication failure revokes completion");
  }
  for (unsigned which = 0; which < 4; ++which) {
    Policy p(true);
    Check(p.Finish(which == 0 ? 0 : which == 1 ? 2 : 1,
                   which == 2 ? 2 : 1, which != 3).outcome == Outcome::Failed &&
              p.failure() == Failure::InvalidCheckpoint, "wrong id generation shape rejects checkpoint");
  }
  {
    Policy p(true);
    Check(p.Finish(1, 0, true).outcome == Outcome::Failed, "zero generation rejected");
  }
  {
    Policy p(true);
    Event e = Hit(EventKind::MemexportCpuCopyFast);
    e.source_submission_known = true; e.source_submission = 3;
    Check(p.Observe(e).outcome == Outcome::Recorded && e.call_submission == 11 &&
              e.source_submission == 3 && e.call_frame == 7, "stale known slot distinct from call time");
    e.source_submission = e.call_submission;
    Check(p.Observe(e).outcome == Outcome::Recorded, "same-submission slot allowed");
  }
  for (unsigned which = 0; which < 6; ++which) {
    Policy p(true);
    Event e = Hit(EventKind::MemexportCpuCopyFast);
    e.source_submission_known = true; e.source_submission = 3;
    if (which == 0) e.source_submission_known = false;
    if (which == 1) e.source_submission = 0;
    if (which == 2) e.source_submission = 12;
    if (which == 3) e.kind = EventKind::ResolveCpuCopy;
    if (which == 4) e.kind = EventKind::MemexportCpuCopyFull;
    if (which == 5) e.kind = static_cast<EventKind>(255);
    Check(p.Observe(e).outcome == Outcome::Failed && p.failure() == Failure::InvalidMetadata,
          "unknown stale future or wrong-kind source submission rejected");
  }
  {
    Policy p(true);
    auto e = Hit(); e.call_frame = UINT64_MAX; e.call_submission = UINT64_MAX;
    Check(p.Observe(e).outcome == Outcome::Recorded, "call metadata maximum needs no arithmetic");
  }
}
}  // namespace

int main() {
  Tests();
  std::cout << checks << " checks, " << failures << " failures\n";
  return failures ? 1 : 0;
}
