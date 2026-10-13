#include "../src/mcla_virtual_detail_record.h"
#include <iostream>
#include <type_traits>
using namespace mcla::virtual_detail;
static_assert(std::is_trivially_copyable_v<Event>);
static_assert(!std::is_copy_constructible_v<Model<32>>);
static_assert(!std::is_copy_assignable_v<Model<32>>);
static_assert(!std::is_move_constructible_v<Model<32>>);
static_assert(!std::is_move_assignable_v<Model<32>>);
static_assert(!kNativeExecutionProven && !kResolvedTargetsProven && !kSimulationProven);

static Entry Make(uint64_t own = 1, uint64_t detail = 10, uint64_t router = 20) {
  mcla::consumer_detail::Entry parent{{17, detail}, {27, router}, {99, 37, 500},
      mcla::consumer_detail::kTarget, mcla::consumer_detail::kCaller, 0x1234, 0x5678};
  mcla::consumer_detail::Child child{kCaller, 0xffffffffBDF9D620ull,
      0x7ff80000deadbeefull, 0xaabbccdd8258D1A0ull, kTarget,
      mcla::consumer_detail::ChildState::kActive, true};
  return {{7, own}, parent, child, kTarget, kCaller, child.r3, child.f1_bits};
}

template <size_t C, size_t D>
static bool Pre(Model<C,D>& m, InvocationId id, uint8_t c) {
  return m.CallPre(id, c, kCallReturnLr[c], 0xffffffff00000020ull+c,
      c == 1 ? 0x8000000000000000ull : 0x7ff8000012345678ull,
      c == 1 ? 0 : 0xaabbccddFFFFFFFFull-c);
}

int main() {
  unsigned checks = 0, failures = 0;
  auto check = [&](bool v) { ++checks; if (!v) ++failures; };
  const auto e = Make(); const auto id = e.invocation;
  {
    Model<8> m(7); check(m.parent_detail_thread_epoch() == 0 && m.parent_router_thread_epoch() == 0);
    check(m.Enter(e));
    check(m.parent_detail_thread_epoch() == 17 && m.parent_router_thread_epoch() == 27);
    for (uint8_t c = 0; c < 3; ++c) { check(Pre(m,id,c)); check(m.CallReturn(id,c)); }
    check(m.Exit(id,true)); check(m.size() == 8 && m.pending_count() == 0 && m.fault() == Fault::kNone);
    const auto& f = m.events()[7].frame;
    check(f.entry == e && f.returned_calls == 3 && f.stage == Stage::kCallReturned);
    check(f.calls[0].f1_bits == 0x7ff8000012345678ull && f.calls[1].f1_bits == 0x8000000000000000ull);
    check(f.calls[1].f1_bits != e.f1_bits && f.calls[2].f1_bits == f.calls[0].f1_bits);
    check(f.calls[0].indirect_ctr == 0xaabbccddFFFFFFFFull && f.calls[0].requested_target == UINT32_MAX);
    check(f.calls[1].indirect_ctr == 0 && f.calls[1].requested_target == 0);
    check(f.calls[2].r3 == 0xffffffff00000022ull);
    check(f.entry.parent_entry.update == e.parent_entry.update);
    check(m.events()[1].frame.calls[0].state == CallState::kActive);
  }
  // Early optional/late unobserved calls may throw; no invented call outcomes.
  for (int completed = 0; completed <= 3; ++completed) {
    Model<8> m(7); check(m.Enter(e));
    for (uint8_t c = 0; c < completed; ++c) { check(Pre(m,id,c)); check(m.CallReturn(id,c)); }
    check(m.Exit(id,false)); check(m.fault() == Fault::kNone && m.pending_count() == 0);
    const auto& f = m.events()[m.size()-1].frame;
    check(m.events()[m.size()-1].kind == EventKind::kExceptionalExit && f.returned_calls == completed);
    for (uint8_t c = static_cast<uint8_t>(completed); c < 3; ++c) check(f.calls[c] == Call{});
  }
  for (uint8_t failed = 0; failed < 3; ++failed) {
    Model<8> m(7); check(m.Enter(e));
    for (uint8_t c = 0; c <= failed; ++c) {
      check(Pre(m,id,c));
      if (c < failed) check(m.CallReturn(id,c)); else check(m.CallException(id,c));
    }
    check(m.Exit(id,false)); const auto& f = m.events()[m.size()-1].frame;
    check(f.returned_calls == failed && f.calls[failed].state == CallState::kException && f.stage == Stage::kCallException);
    for (uint8_t c = uint8_t(failed+1); c < 3; ++c) check(f.calls[c] == Call{});
  }
  {
    Model<8> m(7); check(m.Enter(e)); check(Pre(m,id,0)); check(m.Exit(id,false));
    check(m.events()[2].frame.calls[0].state == CallState::kActive); // Not a fabricated child exception.
  }
  {
    Model<16> m(7); auto nested = Make(2,11,21); nested.parent_entry.update = {};
    check(m.Enter(e)); check(Pre(m,id,0)); check(m.Enter(nested));
    check(m.events()[2].frame.parent == id && m.events()[2].frame.entry.parent_entry.update == mcla::consumer_detail::UpdateBinding{});
    for (uint8_t c = 0; c < 3; ++c) { check(Pre(m,nested.invocation,c)); check(m.CallReturn(nested.invocation,c)); }
    check(m.Exit(nested.invocation,true)); check(m.pending_count() == 1 && m.pending()[0].entry == e);
    check(m.CallReturn(id,0));
    for (uint8_t c = 1; c < 3; ++c) { check(Pre(m,id,c)); check(m.CallReturn(id,c)); }
    check(m.Exit(id,true)); check(m.fault() == Fault::kNone && m.size() == 16);
  }
  // Duplicate/reordered own, parent-detail or parent-router IDs cannot be reused.
  for (bool nested : {false,true}) for (int changed = 0; changed < 3; ++changed) {
    Model<16> m(7); check(m.Enter(e)); if (!nested) check(m.Exit(id,false));
    auto x = Make(2,11,21);
    if (changed == 0) x.invocation.serial = 1;
    if (changed == 1) x.parent_entry.invocation.serial = 10;
    if (changed == 2) x.parent_entry.router.serial = 20;
    const auto n = m.size(); const auto stack = m.pending();
    check(!m.Enter(x) && m.fault() == Fault::kIdentity);
    check(m.size() == n && m.pending() == stack);
  }
  {
    Model<8> m(7); auto maximum = Make(UINT64_MAX,UINT64_MAX,UINT64_MAX);
    check(m.Enter(maximum)); check(m.Exit(maximum.invocation,false));
    check(!m.Enter(e) && m.fault() == Fault::kIdentity);
  }
  for (int changed = 0; changed < 3; ++changed) {
    Model<8> m(7); check(m.Enter(e)); check(m.Exit(id,false));
    const auto gap = Make(10,100,200); check(m.Enter(gap)); check(m.Exit(gap.invocation,false));
    auto x = Make(11,101,201);
    if (changed == 0) x.invocation.serial = 9;
    if (changed == 1) x.parent_entry.invocation.serial = 99;
    if (changed == 2) x.parent_entry.router.serial = 199;
    check(!m.Enter(x) && m.fault() == Fault::kIdentity);
    check(m.size() == 4 && m.pending_count() == 0);
  }
  for (int stage = 0; stage <= 6; ++stage) {
    Model<16> m(7); check(m.Enter(e));
    for (uint8_t c = 0; c < 3; ++c) {
      if (stage >= 1+2*c) check(Pre(m,id,c));
      if (stage >= 2+2*c) check(m.CallReturn(id,c));
    }
    const auto n = m.size(); const auto events = m.events(); const auto stack = m.pending();
    m.Freeze(); m.Freeze(); check(m.frozen() && m.pending_count() == 1);
    check(!m.Enter(Make(2,11,21)) && !Pre(m,id,0) && !m.CallReturn(id,0) && !m.CallException(id,0) && !m.Exit(id,false));
    check(m.size() == n && m.events() == events && m.pending() == stack && m.fault() == Fault::kNone);
  }
  {
    Model<8> m(7); check(m.Enter(e)); check(Pre(m,id,0)); check(m.CallException(id,0));
    const auto stack = m.pending(); const auto events = m.events(); m.Freeze();
    check(!m.Exit(id,false) && m.pending() == stack && m.events() == events);
  }
  // Capacity failures preserve complete pending raw state before the failed event.
  for (int transition = 0; transition < 4; ++transition) {
    Model<2> m(7); check(m.Enter(e)); check(Pre(m,id,0));
    const auto stack = m.pending(); const auto events = m.events(); bool accepted = true;
    if (transition == 0) accepted = m.CallReturn(id,0);
    if (transition == 1) accepted = m.CallException(id,0);
    if (transition == 2) accepted = m.Exit(id,false);
    if (transition == 3) accepted = m.Enter(Make(2,11,21));
    check(!accepted && m.fault() == Fault::kCapacity);
    check(m.size() == 2 && m.pending() == stack && m.events() == events);
  }
  {
    Model<1> m(7); check(m.Enter(e)); const auto stack = m.pending();
    check(!Pre(m,id,0) && m.fault() == Fault::kCapacity && m.pending() == stack);
    Model<8,1> depth(7); check(depth.Enter(e)); check(!depth.Enter(Make(2,11,21)) && depth.fault() == Fault::kDepth);
    check(depth.size() == 1 && depth.pending_count() == 1);
    Model<7> exit(7); check(exit.Enter(e));
    for (uint8_t c = 0; c < 3; ++c) { check(Pre(exit,id,c)); check(exit.CallReturn(id,c)); }
    const auto before = exit.pending(); check(!exit.Exit(id,true) && exit.fault() == Fault::kCapacity && exit.pending() == before);
  }
  for (int bad = 0; bad < 11; ++bad) {
    Model<16> m(7); check(m.Enter(e)); bool accepted = true;
    switch (bad) {
      case 0: accepted = m.Exit(id,true); break;
      case 1: accepted = m.CallReturn(id,0); break;
      case 2: accepted = m.CallException(id,0); break;
      case 3: accepted = m.CallPre(id,1,kCallReturnLr[1],0,0,0); break;
      case 4: accepted = m.CallPre(id,255,0,0,0,0); break;
      case 5: accepted = m.CallPre(id,0,kCallReturnLr[1],0,0,0); break;
      case 6: accepted = m.CallPre({7,2},0,kCallReturnLr[0],0,0,0); break;
      case 7: check(Pre(m,id,0)); accepted = Pre(m,id,0); break;
      case 8: check(Pre(m,id,0)); accepted = m.CallReturn(id,1); break;
      case 9: check(Pre(m,id,0)); check(m.CallException(id,0)); accepted = Pre(m,id,1); break;
      case 10: check(m.Enter(Make(2,11,21))); accepted = m.Exit(id,false); break;
    }
    check(!accepted && m.fault() != Fault::kNone);
    const auto n = m.size(); check(!m.Exit(id,false) && m.size() == n);
  }
  for (int bad = 0; bad < 20; ++bad) {
    Model<8> m(7,17); auto x = e;
    switch (bad) {
      case 0: x.invocation.thread_epoch = 17; break;
      case 1: x.invocation.serial = 0; break;
      case 2: x.parent_entry.invocation.thread_epoch = 0; break;
      case 3: x.parent_entry.invocation.thread_epoch = 18; break;
      case 4: x.parent_entry.invocation.serial = 0; break;
      case 5: x.parent_entry.router.thread_epoch = 0; break;
      case 6: x.parent_entry.router.serial = 0; break;
      case 7: x.parent_entry.update = {1,0,0}; break;
      case 8: x.parent_entry.update = {0,1,1}; break;
      case 9: x.parent_entry.actual_target = 0; break;
      case 10: x.parent_entry.caller_lr = kCaller; break;
      case 11: x.parent_child.state = mcla::consumer_detail::ChildState::kReturned; break;
      case 12: x.parent_child.indirect = false; break;
      case 13: x.parent_child.caller_lr = kCallReturnLr[0]; break;
      case 14: x.parent_child.requested_target = 0; break;
      case 15: x.parent_child.indirect_ctr ^= 1; break;
      case 16: x.actual_target = 0; break;
      case 17: x.caller_lr = kCallReturnLr[0]; break;
      case 18: x.r3 ^= 1ull<<40; break;
      case 19: x.f1_bits ^= 1ull<<40; break;
    }
    check(!m.Enter(x) && m.fault() == Fault::kIdentity && m.size() == 0);
    check(m.parent_router_thread_epoch() == 0);
  }
  for (bool router : {false,true}) {
    Model<8> m(7); check(m.Enter(e)); check(m.Exit(id,false)); auto x = Make(2,11,21);
    if (router) x.parent_entry.router.thread_epoch = 28; else x.parent_entry.invocation.thread_epoch = 18;
    check(!m.Enter(x) && m.fault() == Fault::kIdentity);
    check(m.parent_detail_thread_epoch() == 17 && m.parent_router_thread_epoch() == 27);
  }
  {
    Model<8> zero(0); check(!zero.Enter(e) && zero.fault() == Fault::kIdentity);
    Model<8> frozen(7); frozen.Freeze(); check(!frozen.Enter(e));
    check(frozen.parent_detail_thread_epoch() == 0 && frozen.parent_router_thread_epoch() == 0);
    Model<8> m(7); auto empty = e; empty.parent_entry.update = {};
    check(m.Enter(empty) && m.events()[0].frame.entry.parent_entry.update == mcla::consumer_detail::UpdateBinding{});
  }
  std::cout << checks << " checks, " << failures << " failures\n";
  return failures ? 1 : 0;
}
