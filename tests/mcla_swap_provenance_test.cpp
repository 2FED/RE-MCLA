#include <rex/ui/guest_capture_provenance.h>
#include <iostream>

int main() {
  using namespace rex::ui;
  namespace sw = rex::diagnostics::swap_emission;
  unsigned checks = 0, failures = 0;
  auto check = [&](bool value) { ++checks; if (!value) ++failures; };
  const auto valid = [](const auto& p) {
    return ValidGuestCaptureProvenance(p, 32, 16, 128, 2048);
  };
  GuestCaptureProvenance p{1, 7, 10000000, 10, 20, 19, 21, 22, 30, 31};
  check(valid(p));
  p.swap_emission.result = sw::Result::kTokenMatched;
  p.swap_emission.match.token = {1, 1};
  p.swap_emission.consume_qpc = 9;
  check(valid(p));  // Transport-only binding with explicitly unavailable snapshot.
  p.swap_emission.match.snapshot = {2, 3, 4, 5, 6, 7, 8, true};
  static_assert(sw::kSnapshotAbiVersion == 2);
  p.swap_emission.match.snapshot.callback_id = 77;
  p.swap_emission.match.snapshot_result = sw::SnapshotResult::kAvailable;
  check(valid(p));
  auto q = p; q.swap_emission.match.token.epoch = 0; check(!valid(q));
  q = p; q.swap_emission.match.token.serial = 0; check(!valid(q));
  for (auto n : {uint64_t(0), uint64_t(2), uint64_t(11), UINT64_MAX}) {
    q = p; q.swap_emission.consume_qpc = n; check(!valid(q));
  }
  q = p; q.swap_emission.consume_qpc = 10; check(valid(q));
  for (auto n : {uint64_t(0), uint64_t(4), UINT64_MAX}) {
    q = p; q.swap_emission.match.snapshot.qpc_before = n; check(!valid(q));
  }
  for (auto n : {uint64_t(0), uint64_t(1), uint64_t(10), UINT64_MAX}) {
    q = p; q.swap_emission.match.snapshot.qpc_after = n; check(!valid(q));
  }
  for (auto field : {&sw::Snapshot::native_thread_id, &sw::Snapshot::guest_caller_lr,
                     &sw::Snapshot::timer_address}) {
    q = p; q.swap_emission.match.snapshot.*field = 0; check(!valid(q));
  }
  q = p; q.swap_emission.match.snapshot_result = sw::SnapshotResult::kUnavailable;
  check(!valid(q));
  q = p; q.swap_emission.match.snapshot.snapshot_available = false; check(!valid(q));
  for (auto result : {sw::SnapshotResult::kUnavailable, sw::SnapshotResult::kInvalidQpc,
                      sw::SnapshotResult::kMissingIdentity}) {
    q = p; q.swap_emission.match.snapshot = {};
    q.swap_emission.match.snapshot_result = result; check(valid(q));
    q.swap_emission.match.snapshot.callback_id = 77; check(!valid(q));
  }
  q = p; q.swap_emission.match.snapshot = {}; check(!valid(q));
  q = p; q.swap_emission.match.snapshot_result = static_cast<sw::SnapshotResult>(999);
  check(!valid(q));
  std::cout << "swap provenance: " << checks << " checks, " << failures << " failures\n";
  return failures ? 1 : 0;
}
