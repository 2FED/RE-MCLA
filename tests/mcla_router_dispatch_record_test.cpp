#include "../src/mcla_router_dispatch_record.h"
#include <iostream>
#include <type_traits>
using namespace mcla::router_dispatch;
static_assert(std::is_trivially_copyable_v<Event>);
static_assert(!std::is_copy_constructible_v<Model<32>>);
static_assert(!std::is_move_constructible_v<Model<32>>);
static Entry Make(uint64_t serial = 1) {
  return {{7, serial}, {99, 17, 500}, 0x821316a0, 0xfedcba9876543210,
          0xffffffff00000002, 0x7ff80000deadbeef};
}
int main() {
  unsigned checks = 0, failures = 0;
  auto check = [&](bool value) { ++checks; if (!value) ++failures; };
  const auto e = Make(); const auto id = e.invocation;
  {
    Model<16> m(7);
    check(m.Enter(e));
    check(m.PredicatePre(id, 10, 20, 0x1111111182010000));
    check(m.PredicateReturn(id, 0xffffffffffffff00));
    check(m.Skip(id)); check(m.Exit(id, true));
    check(m.size() == 5 && m.pending_count() == 0 && m.fault() == Fault::kNone);
    const auto& frame = m.events()[4].frame;
    check(frame.entry == e && frame.predicate_result_r3 == 0xffffffffffffff00);
    check(frame.predicate_result_low8 == 0 && frame.predicate_returned);
    check(!frame.consumer.present && frame.stage == Stage::kSkipped);
    check(frame.predicate.ctr == 0x1111111182010000 &&
          frame.predicate.requested_target_u32() == 0x82010000);
    check(m.events()[3].kind == EventKind::kSkip);
  }
  {
    Model<16> m(7);
    check(m.Enter(e)); check(m.PredicatePre(id, 10, 20, 30));
    check(m.PredicateReturn(id, 0x0100000000000080));
    check(m.ConsumerPre(id, 11, 21, 31, 0x8000000000000000));
    check(m.ConsumerReturn(id)); check(m.Exit(id, true));
    const auto& frame = m.events()[5].frame;
    check(frame.predicate.r3 == 10 && frame.consumer.r3 == 11);
    check(frame.predicate.table == 20 && frame.consumer.table == 21);
    check(frame.predicate.ctr == 30 && frame.consumer.ctr == 31);
    check(frame.consumer.f1_bits == 0x8000000000000000 && frame.entry.f1_bits == e.f1_bits);
    check(frame.predicate_result_low8 == 0x80 && m.fault() == Fault::kNone);
  }
  for (bool consumer : {false, true}) {
    Model<16> m(7); check(m.Enter(e)); check(m.PredicatePre(id, 0, 0, 0));
    if (consumer) {
      check(m.PredicateReturn(id, 1)); check(m.ConsumerPre(id, 0, 0, 0, 0));
      check(m.ConsumerException(id));
    } else check(m.PredicateException(id));
    check(m.Exit(id, false));
    check(m.pending_count() == 0 && m.fault() == Fault::kNone);
    check(m.events()[m.size()-1].kind == EventKind::kExceptionalExit);
  }
  {
    Model<32> m(7); auto child = Make(2); child.update = {101, 17, 500};
    check(m.Enter(e)); check(m.PredicatePre(id, 10, 20, 30));
    check(m.Enter(child)); check(m.PredicatePre(child.invocation, 11, 21, 31));
    check(m.PredicateReturn(child.invocation, 0)); check(m.Skip(child.invocation));
    check(m.Exit(child.invocation, true));
    check(m.events()[6].frame.parent == id && m.events()[6].frame.entry.update.phase_call_id == 101);
    check(m.PredicateReturn(id, 1)); check(m.ConsumerPre(id, 12, 22, 32, 1));
    check(m.ConsumerReturn(id)); check(m.Exit(id, true));
    check(m.events()[10].frame.entry.update.phase_call_id == 99);
    check(m.events()[10].frame.parent == InvocationId{});
  }
  {
    Model<8> m(7); auto unbound = Make(); unbound.update = {};
    check(m.Enter(unbound)); check(m.Exit(id, false));
    check(m.events()[1].frame.entry.update == UpdateBinding{});
    check(!m.events()[1].frame.predicate.present && !m.events()[1].frame.consumer.present);
  }
  {
    Model<8> m(7); auto child = Make(2); child.update = {};
    check(m.Enter(e)); check(m.Enter(child));
    check(m.pending()[1].entry.update == UpdateBinding{});
    check(m.Exit(child.invocation, false));
    check(m.pending()[0].entry == e && m.pending_count() == 1);
    check(m.Exit(id, false));
    check(!m.Enter(child) && m.fault() == Fault::kIdentity); // No ID reuse after exit.
  }
  for (int stage = 0; stage < 3; ++stage) {
    Model<16> m(7); check(m.Enter(e));
    if (stage > 0) check(m.PredicatePre(id, 1, 2, 3));
    if (stage > 1) { check(m.PredicateReturn(id, 1)); check(m.ConsumerPre(id, 4, 5, 6, 7)); }
    const auto count = m.size(); const auto pending = m.pending(); const auto events = m.events();
    m.Freeze(); check(m.frozen() && m.pending_count() == 1);
    check(!m.Exit(id, false) && !m.Enter(Make(2)) && !m.ConsumerReturn(id));
    check(m.size() == count && m.pending() == pending && m.events() == events && m.fault() == Fault::kNone);
  }
  for (const auto stage : {Stage::kPredicateReturned, Stage::kSkipped,
                           Stage::kConsumerReturned, Stage::kDispatchException}) {
    Model<16> m(7); check(m.Enter(e)); check(m.PredicatePre(id, 1, 2, 3));
    if (stage == Stage::kDispatchException) {
      check(m.PredicateException(id));
    } else {
      check(m.PredicateReturn(id, stage == Stage::kConsumerReturned ? 1 : 0));
      if (stage == Stage::kSkipped) check(m.Skip(id));
      if (stage == Stage::kConsumerReturned) {
        check(m.ConsumerPre(id, 4, 5, 6, 7)); check(m.ConsumerReturn(id));
      }
    }
    check(m.pending()[0].stage == stage);
    const auto count = m.size(); const auto pending = m.pending(); const auto events = m.events();
    m.Freeze(); m.Freeze();
    check(m.frozen() && m.pending_count() == 1);
    check(!m.Exit(id, true) && !m.Exit(id, false) && !m.Skip(id));
    check(!m.ConsumerPre(id, 4, 5, 6, 7) && !m.PredicateReturn(id, 1));
    check(m.size() == count && m.pending() == pending && m.events() == events && m.fault() == Fault::kNone);
  }
  {
    Model<5> m(7); check(m.Enter(e)); check(m.PredicatePre(id, 1, 2, 3));
    check(m.PredicateReturn(id, 0)); check(m.Skip(id));
    check(m.size() == 4); check(m.Exit(id, true));
    check(m.size() == 5 && m.pending_count() == 0 && m.fault() == Fault::kNone);
    check(m.events()[4].kind == EventKind::kNormalExit);
    const auto events = m.events();
    check(!m.Enter(Make(2)) && m.fault() == Fault::kCapacity);
    check(m.size() == 5 && m.pending_count() == 0 && m.events() == events);
  }
  for (const uint64_t next : {uint64_t(0), uint64_t(1), UINT64_MAX-1, UINT64_MAX}) {
    Model<8> m(7); const auto last = Make(UINT64_MAX);
    check(m.Enter(last)); check(m.Exit(last.invocation, false));
    const auto events = m.events();
    check(!m.Enter(Make(next)) && m.fault() == Fault::kIdentity);
    check(m.size() == 2 && m.pending_count() == 0 && m.events() == events);
  }
  {
    Model<2> m(7); check(m.Enter(e)); check(m.PredicatePre(id, 1, 2, 3));
    const auto pending = m.pending();
    check(!m.PredicateReturn(id, 1)); check(m.fault() == Fault::kCapacity);
    check(m.size() == 2 && m.pending() == pending);
    check(!m.Exit(id, false)); m.Freeze(); check(m.pending_count() == 1);
  }
  {
    Model<8, 1> m(7); check(m.Enter(e)); check(!m.Enter(Make(2)));
    check(m.fault() == Fault::kDepth && m.pending_count() == 1 && m.size() == 1);
  }
  {
    Model<1> m(7); check(m.Enter(e)); check(!m.Enter(Make(2)));
    check(m.fault() == Fault::kCapacity && m.pending_count() == 1 && m.size() == 1);
  }
  for (int bad = 0; bad < 8; ++bad) {
    Model<16> m(7); check(m.Enter(e));
    bool accepted = true;
    switch (bad) {
      case 0: accepted = m.ConsumerReturn(id); break;
      case 1: accepted = m.Exit(id, true); break;
      case 2: accepted = m.PredicatePre({8, 1}, 0, 0, 0); break;
      case 3: accepted = m.Enter(e); break;
      case 4:
        check(m.PredicatePre(id, 0, 0, 0)); check(m.PredicateReturn(id, 0));
        accepted = m.ConsumerPre(id, 0, 0, 0, 0); break;
      case 5:
        check(m.PredicatePre(id, 0, 0, 0)); check(m.PredicateReturn(id, 1));
        accepted = m.Skip(id); break;
      case 6:
        check(m.Enter(Make(2))); accepted = m.Exit(id, false); break;
      case 7:
        check(m.PredicatePre(id, 0, 0, 0)); check(m.PredicateException(id));
        accepted = m.Exit(id, true); break;
    }
    check(!accepted && m.fault() != Fault::kNone);
    const auto count = m.size(); check(!m.Exit(id, false) && m.size() == count);
  }
  for (const auto binding : {UpdateBinding{1, 0, 0}, UpdateBinding{0, 1, 1},
                             UpdateBinding{1, 1, 0}, UpdateBinding{0, 0, 1}}) {
    Model<8> m(7); auto bad = Make(); bad.update = binding;
    check(!m.Enter(bad) && m.fault() == Fault::kIdentity && m.size() == 0);
  }
  {
    Model<8> m(0); check(!m.Enter(e) && m.fault() == Fault::kIdentity);
    Model<8> n(7); auto zero = Make(0); check(!n.Enter(zero));
  }
  std::cout << checks << " checks, " << failures << " failures\n";
  return failures ? 1 : 0;
}
