#include "../src/mcla_consumer_detail_record.h"
#include <iostream>
#include <type_traits>
using namespace mcla::consumer_detail;
static_assert(std::is_trivially_copyable_v<Event>);
static_assert(!std::is_copy_constructible_v<Model<32>>);
static_assert(!std::is_copy_assignable_v<Model<32>>);
static_assert(!std::is_move_constructible_v<Model<32>>);
static_assert(!std::is_move_assignable_v<Model<32>>);

static Entry Make(uint64_t detail = 1, uint64_t router = 10) {
  return {{7, detail}, {17, router}, {99, 23, 500}, kTarget, kCaller,
          0xffffffff82900000ull, 0x7ff80000deadbeefull};
}

int main() {
  unsigned checks = 0, failures = 0;
  auto check = [&](bool v) { ++checks; if (!v) ++failures; };
  const auto e = Make(); const auto id = e.invocation;
  {
    Model<16> m(7, 17); check(m.Enter(e));
    check(m.Branch(id, 0xffffffff00000000ull)); check(m.Exit(id, true));
    check(m.fault() == Fault::kNone && m.pending_count() == 0 && m.size() == 3);
    const auto& f = m.events()[2].frame;
    check(f.entry == e && f.branch_r11 == 0xffffffff00000000ull);
    check(f.branch_observed && !f.branch_nonzero && f.returned_children == 0);
    for (const auto& c : f.children) check(c == Child{});
  }
  {
    Model<16> m(7, 17); check(m.Enter(e)); check(m.Branch(id, 0xffffffff00000100ull));
    for (uint8_t c = 0; c < 3; ++c) {
      check(m.ChildPre(id, c, kChildReturnLr[c], 0xffff000000000020ull+c,
                      c == 1 ? 0x8000000000000000ull : 0x7ff8000012345678ull,
                      c == 0 ? 0xaabbccdd82300000ull : 0));
      check(m.ChildReturn(id, c));
    }
    check(m.Exit(id, true)); check(m.size() == 9 && m.pending_count() == 0);
    const auto& f = m.events()[8].frame;
    check(f.branch_nonzero && f.returned_children == 3 && f.stage == Stage::kChildReturned);
    check(f.children[0].indirect && f.children[0].indirect_ctr == 0xaabbccdd82300000ull);
    check(f.children[0].requested_target == 0x82300000);
    for (uint8_t c = 1; c < 3; ++c) {
      check(!f.children[c].indirect && f.children[c].indirect_ctr == 0);
      check(f.children[c].requested_target == kDirectTarget[c]);
    }
    check(f.children[1].f1_bits == 0x8000000000000000ull);
    check(f.children[2].f1_bits == 0x7ff8000012345678ull);
    check(f.children[2].r3 == 0xffff000000000022ull);
    check(m.events()[2].frame.children[0].state == ChildState::kActive);
  }
  // The earlier unobserved call may fail after Branch, before child0.
  for (bool branched : {false, true}) {
    Model<16> m(7, 17); check(m.Enter(e));
    if (branched) check(m.Branch(id, 1));
    check(m.Exit(id, false)); check(m.fault() == Fault::kNone);
    check(m.events()[m.size()-1].kind == EventKind::kExceptionalExit);
    for (const auto& c : m.events()[m.size()-1].frame.children) check(c == Child{});
  }
  for (uint8_t failed = 0; failed < 3; ++failed) {
    Model<16> m(7, 17); check(m.Enter(e)); check(m.Branch(id, 1));
    for (uint8_t c = 0; c <= failed; ++c) {
      check(m.ChildPre(id, c, kChildReturnLr[c], c, 0));
      if (c < failed) check(m.ChildReturn(id, c));
      else check(m.ChildException(id, c));
    }
    check(m.Exit(id, false));
    const auto& f = m.events()[m.size()-1].frame;
    check(f.returned_children == failed && f.children[failed].state == ChildState::kException);
    check(f.stage == Stage::kChildException);
    for (uint8_t c = uint8_t(failed+1); c < 3; ++c) check(f.children[c] == Child{});
  }
  {
    Model<32> m(7, 17); auto child = Make(2, 11); child.update = {};
    check(m.Enter(e)); check(m.Branch(id, 1)); check(m.ChildPre(id, 0, kChildReturnLr[0], 1, 2));
    check(m.Enter(child)); check(m.Branch(child.invocation, 0)); check(m.Exit(child.invocation, true));
    check(m.events()[5].frame.parent == id && m.events()[5].frame.entry.update == UpdateBinding{});
    check(m.pending_count() == 1 && m.pending()[0].entry == e);
    check(m.ChildReturn(id, 0));
    for (uint8_t c = 1; c < 3; ++c) { check(m.ChildPre(id, c, kChildReturnLr[c], c, 0)); check(m.ChildReturn(id, c)); }
    check(m.Exit(id, true)); check(m.fault() == Fault::kNone);
  }
  // A router invocation can yield at most one actual entry, also after exit.
  for (bool nested : {false, true}) {
    Model<16> m(7, 17); check(m.Enter(e));
    if (!nested) check(m.Exit(id, false));
    const auto count = m.size(); const auto pending = m.pending();
    check(!m.Enter(Make(2, 10)) && m.fault() == Fault::kIdentity);
    check(m.size() == count && m.pending() == pending);
  }
  for (bool nested : {false, true}) {
    Model<16> m(7, 17); check(m.Enter(e));
    if (!nested) check(m.Exit(id, false));
    const auto count = m.size(); const auto pending = m.pending();
    check(!m.Enter(Make(1, 11)) && m.fault() == Fault::kIdentity);
    check(m.size() == count && m.pending() == pending);
  }
  {
    Model<16> m(7, 17); check(m.Enter(e)); check(m.Exit(id, false));
    check(m.Enter(Make(2, 20))); check(m.Exit({7, 2}, false));
    check(!m.Enter(Make(3, 11)) && m.fault() == Fault::kIdentity);
  }
  for (int stage = 0; stage < 9; ++stage) {
    Model<32> m(7, 17); check(m.Enter(e));
    if (stage >= 1) check(m.Branch(id, 1));
    for (uint8_t c = 0; c < 3; ++c) {
      if (stage >= 2+2*c) check(m.ChildPre(id, c, kChildReturnLr[c], c, 0));
      if (stage >= 3+2*c) check(m.ChildReturn(id, c));
    }
    const auto count = m.size(); const auto events = m.events(); const auto pending = m.pending();
    m.Freeze(); m.Freeze();
    check(m.frozen() && m.pending_count() == 1);
    check(!m.Exit(id, false) && !m.Enter(Make(2, 11)) && !m.Branch(id, 0) && !m.ChildReturn(id, 2));
    check(m.size() == count && m.events() == events && m.pending() == pending && m.fault() == Fault::kNone);
  }
  {
    Model<2> m(7, 17); check(m.Enter(e)); check(m.Branch(id, 1));
    const auto pending = m.pending(); check(!m.ChildPre(id, 0, kChildReturnLr[0], 0, 0));
    check(m.fault() == Fault::kCapacity && m.size() == 2 && m.pending() == pending);
    check(!m.Exit(id, false)); m.Freeze(); check(m.pending_count() == 1);
  }
  {
    Model<8, 1> m(7, 17); check(m.Enter(e)); check(!m.Enter(Make(2, 11)));
    check(m.fault() == Fault::kDepth && m.size() == 1 && m.pending_count() == 1);
    Model<2> n(7, 17); check(n.Enter(e)); check(n.Branch(id, 0));
    check(!n.Exit(id, true) && n.fault() == Fault::kCapacity && n.pending_count() == 1);
    Model<3> exact(7, 17); check(exact.Enter(e)); check(exact.Branch(id, 0)); check(exact.Exit(id, true));
    check(exact.size() == 3 && exact.pending_count() == 0 && exact.fault() == Fault::kNone);
  }
  for (int bad = 0; bad < 12; ++bad) {
    Model<16> m(7, 17); check(m.Enter(e)); bool accepted = true;
    switch (bad) {
      case 0: accepted = m.Exit(id, true); break;
      case 1: accepted = m.ChildReturn(id, 0); break;
      case 2: accepted = m.Branch({7, 2}, 0); break;
      case 3: check(m.Branch(id, 0)); accepted = m.Branch(id, 0); break;
      case 4: check(m.Branch(id, 0)); accepted = m.ChildPre(id, 0, kChildReturnLr[0], 0, 0); break;
      case 5: check(m.Branch(id, 1)); accepted = m.ChildPre(id, 1, kChildReturnLr[1], 0, 0); break;
      case 6: check(m.Branch(id, 1)); accepted = m.ChildPre(id, 0, kChildReturnLr[1], 0, 0); break;
      case 7: check(m.Enter(Make(2, 11))); accepted = m.Exit(id, false); break;
      case 8: check(m.Branch(id, 1)); accepted = m.ChildPre(id, 255, 0, 0, 0); break;
      case 9: check(m.Branch(id, 1)); check(m.ChildPre(id, 0, kChildReturnLr[0], 0, 0)); accepted = m.ChildReturn(id, 1); break;
      case 10: check(m.Branch(id, 1)); check(m.ChildPre(id, 0, kChildReturnLr[0], 0, 0)); check(m.ChildException(id, 0)); accepted = m.Exit(id, true); break;
      case 11: check(m.Branch(id, 1)); check(m.ChildPre(id, 0, kChildReturnLr[0], 0, 0)); check(m.ChildReturn(id, 0)); accepted = m.ChildPre(id, 1, kChildReturnLr[1], 0, 0, 99); break;
    }
    check(!accepted && m.fault() != Fault::kNone);
    const auto count = m.size(); check(!m.Exit(id, false) && count == m.size());
  }
  for (int bad = 0; bad < 8; ++bad) {
    Model<8> m(7, 17); auto x = e;
    switch (bad) {
      case 0: x.invocation.serial = 0; break;
      case 1: x.invocation.thread_epoch = 17; break;
      case 2: x.router.serial = 0; break;
      case 3: x.router.thread_epoch = 7; break;
      case 4: x.actual_target = 0; break;
      case 5: x.caller_lr = 0x822C5304; break;
      case 6: x.update = {1, 0, 0}; break;
      case 7: x.update = {0, 1, 1}; break;
    }
    check(!m.Enter(x) && m.fault() == Fault::kIdentity && m.size() == 0);
  }
  {
    Model<8> a(0, 17), b(7, 0); check(!a.Enter(e));
    check(b.router_thread_epoch() == 0 && b.Enter(e) && b.router_thread_epoch() == 17);
    check(b.Exit(id, false)); auto other = Make(2, 11); other.router.thread_epoch = 18;
    check(!b.Enter(other) && b.fault() == Fault::kIdentity && b.router_thread_epoch() == 17);
    Model<8> m(7, 17); auto maximum = Make(UINT64_MAX, UINT64_MAX);
    check(m.Enter(maximum)); check(m.Exit(maximum.invocation, false));
    check(!m.Enter(Make(1, 1)) && m.fault() == Fault::kIdentity);
  }
  {
    Model<8> m(7); auto invalid = e; invalid.router.thread_epoch = 0;
    check(!m.Enter(invalid) && m.router_thread_epoch() == 0 && m.fault() == Fault::kIdentity);
    Model<8> frozen(7); frozen.Freeze(); check(!frozen.Enter(e) && frozen.router_thread_epoch() == 0);
    Model<8> partial(7, 17); check(partial.Enter(e)); check(partial.Branch(id, 1));
    check(partial.ChildPre(id, 0, kChildReturnLr[0], 0, 0)); check(partial.ChildException(id, 0));
    const auto events = partial.events(); const auto pending = partial.pending();
    partial.Freeze(); check(!partial.Exit(id, false));
    check(partial.events() == events && partial.pending() == pending && partial.pending_count() == 1);
  }
  std::cout << checks << " checks, " << failures << " failures\n";
  return failures ? 1 : 0;
}
