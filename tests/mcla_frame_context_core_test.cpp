#include "../src/mcla_frame_context_core.h"
#include <iostream>
#include <type_traits>
using namespace mcla::frame_context;
static_assert(!std::is_copy_constructible_v<Tracker> && !std::is_move_constructible_v<Tracker>);
static_assert(!std::is_copy_assignable_v<Tracker> && !std::is_move_assignable_v<Tracker>);
int main() {
  unsigned checks = 0, failures = 0;
  auto check = [&](bool yes) { ++checks; if (!yes) ++failures; };
  const Lane lane{0x1234, 0x5000};
  auto update = [&](Tracker& t) {
    auto u = t.enter(lane, Kind::kUpdate, true, 10);
    check(u.valid() && t.current().invocation == u.id);
    check(t.leave(lane, u, 20, true));
    check(t.pending_update() == u.id && !t.current().invocation.valid());
    return u;
  };
  {
    Tracker t(1, lane);
    auto u = update(t);
    auto c = t.enter(lane, Kind::kCoordinator, true, 30, 77);
    check(c.valid() && !t.pending_update().valid() && t.current().update == u.id);
    auto r = t.enter(lane, Kind::kSyncRender, true, 31, 77);
    check(r.valid() && t.current().update == u.id && t.current().invocation != c.id);
    check(t.leave(lane, r, 32, true));
    check(t.current().invocation == c.id);
    check(t.leave(lane, c, 33, true));
    check(!t.current().invocation.valid() && !t.pending_update().valid());
    check(!t.enter(lane, Kind::kCoordinator, true, 34, 77).valid());
    check(t.failed()); // Completed update cannot be reused.
  }
  {
    Tracker worker(2, lane);
    check(worker.worker_credit(lane, 900, 77, true));
    auto r = worker.enter(lane, Kind::kWorkerRender, true, 10, 77);
    check(r.valid() && worker.current().wait_operation == 900);
    check(!worker.current().update.valid()); // Offline credit audit owns that join.
    check(worker.leave(lane, r, 11, true));
    check(!worker.enter(lane, Kind::kWorkerRender, true, 12, 77).valid());
    check(worker.failed());
  }
  for (const Lane bad : {Lane{}, Lane{0x1235, 0x5000}, Lane{0x1234, 0x5001}}) {
    Tracker t(1, lane);
    check(!t.enter(bad, Kind::kUpdate, true, 10).valid());
    check(t.failed() && !t.enter(lane, Kind::kUpdate, true, 11).valid());
  }
  for (const auto kind : {Kind::kCoordinator, Kind::kSyncRender, Kind::kWorkerRender,
                           static_cast<Kind>(99)}) {
    Tracker t(1, lane);
    check(!t.enter(lane, kind, true, 10, 77).valid());
    check(t.failed());
  }
  for (uint64_t qpc : {uint64_t(0), uint64_t(INT64_MAX) + 1}) {
    Tracker t(1, lane);
    check(!t.enter(lane, Kind::kUpdate, true, qpc).valid());
  }
  {
    Tracker t(1, lane);
    check(!t.enter(lane, Kind::kUpdate, false, 10).valid());
    check(t.failed());
  }
  for (int mode = 0; mode != 6; ++mode) {
    Tracker t(1, lane);
    auto u = t.enter(lane, Kind::kUpdate, true, 10);
    if (mode == 0) ++u.id.serial;
    if (mode == 1) ++u.id.thread_epoch;
    if (mode == 2) ++u.begin;
    if (mode == 3) ++u.depth;
    check(!t.leave(lane, u, mode == 4 ? 9 : 11, mode != 5));
    check(t.failed() && !t.pending_update().valid() && !t.current().invocation.valid());
  }
  {
    Tracker t(1, lane);
    auto u = t.enter(lane, Kind::kUpdate, true, 10);
    check(!t.enter(lane, Kind::kUpdate, true, 11).valid());
    check(!t.leave(lane, u, 12, true));
  }
  {
    Tracker t(1, lane);
    update(t);
    auto c = t.enter(lane, Kind::kCoordinator, true, 30, 77);
    auto r = t.enter(lane, Kind::kSyncRender, true, 31, 77);
    check(r.valid());
    check(!t.leave(lane, c, 32, true)); // LIFO, not merely matching an old ID.
  }
  {
    Tracker t(1, lane);
    update(t);
    check(!t.enter(lane, Kind::kUpdate, true, 30).valid()); // No stale update overwrite.
  }
  for (int mode = 0; mode != 5; ++mode) {
    Tracker t(1, lane);
    if (mode == 0) check(t.worker_credit(lane, 1, 77, true));
    if (mode == 1) check(t.enter(lane, Kind::kUpdate, true, 10).valid());
    check(!t.worker_credit(lane, mode == 2 ? 0 : 1, mode == 3 ? 0 : 77, mode != 4));
    check(t.failed());
  }
  {
    Tracker t(1, lane);
    check(t.worker_credit(lane, 1, 77, true));
    check(!t.enter(lane, Kind::kWorkerRender, true, 10, 78).valid());
  }
  {
    Tracker t(1, lane, UINT64_MAX);
    check(!t.enter(lane, Kind::kUpdate, true, 10).valid());
    check(t.failed());
    Tracker bad(0, lane), bad_lane(1, {});
    check(bad.failed() && bad_lane.failed());
  }
  for (uint64_t replay : {uint64_t(900), uint64_t(899)}) {
    Tracker t(2, lane);
    check(t.worker_credit(lane, 900, 77, true));
    auto r = t.enter(lane, Kind::kWorkerRender, true, 10, 77);
    check(t.leave(lane, r, 11, true));
    check(!t.worker_credit(lane, replay, replay == 900 ? 88 : 77, true));
    check(t.failed());
  }
  {
    Tracker t(1, lane);
    update(t);
    check(!t.worker_credit(lane, 900, 77, true));
    check(t.failed());
  }
  {
    Tracker a(1, lane), b(2, lane);
    const auto x = a.enter(lane, Kind::kUpdate, true, 10);
    const auto y = b.enter(lane, Kind::kUpdate, true, 10);
    check(x.id != y.id); // Native TID reuse must receive a new epoch externally.
  }
  {
    Tracker t(1, lane);
    Id previous;
    for (uint64_t cycle = 0; cycle != 3; ++cycle) {
      const uint64_t qpc = 10 + cycle * 10;
      auto u = t.enter(lane, Kind::kUpdate, true, qpc);
      check(u.valid() && u.id != previous);
      check(t.leave(lane, u, qpc + 1, true));
      auto c = t.enter(lane, Kind::kCoordinator, true, qpc + 2, 77);
      auto r = t.enter(lane, Kind::kSyncRender, true, qpc + 3, 77);
      check(r.valid() && t.current().update == u.id);
      check(t.leave(lane, r, qpc + 4, true) && t.leave(lane, c, qpc + 5, true));
      previous = u.id;
    }
    check(!t.failed());
  }
  {
    Tracker t(2, lane);
    for (uint64_t op : {uint64_t(900), uint64_t(902)}) {
      check(t.worker_credit(lane, op, 77, true));
      auto r = t.enter(lane, Kind::kWorkerRender, true, op, 77);
      check(r.valid() && t.current().wait_operation == op);
      check(t.leave(lane, r, op + 1, true));
    }
    check(!t.failed());
  }
  {
    Tracker t(1, lane);
    update(t);
    auto c = t.enter(lane, Kind::kCoordinator, true, 30, 77);
    auto r = t.enter(lane, Kind::kSyncRender, true, 31, 77);
    check(!t.leave(lane, r, 32, false));
    check(!t.current().invocation.valid() && !t.pending_update().valid());
    check(!t.leave(lane, c, 33, false));
  }
  std::cout << "frame context: " << checks << " checks, " << failures << " failures\n";
  return failures ? 1 : 0;
}
