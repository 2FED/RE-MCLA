// M11-007d / KI-047: pure parts of the lock probe (src/mcla_lock_probe.h) and
// the generated code its register and stack layout mirror
// (MCLA_LOCK_GENERATED_DIR).

#include "mcla_lock_probe.h"

#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#ifndef MCLA_LOCK_GENERATED_DIR
#error "MCLA_LOCK_GENERATED_DIR (generated/default) is set by CMake"
#endif

namespace {
int checks = 0;
void Check(bool value, const char *message) {
  if (!value) {
    std::fprintf(stderr, "FAIL: %s\n", message);
    std::exit(1);
  }
  ++checks;
}

bool Has(const std::string &text, const std::string &part) {
  return text.find(part) != std::string::npos;
}

namespace lock = mcla::lock_probe;

void TestPhysical() {
  static_assert(lock::kFlagsStackOffset == 84);
  Check(lock::GuestToPhysical(0xA39CA000u) == 0x039CA000u, "0xA0000000 view");
  Check(lock::GuestToPhysical(0xC39CA000u) == 0x039CA000u, "0xC0000000 view");
  Check(lock::GuestToPhysical(0xE39CA000u) == 0x039CB000u, "0xE0000000 view adds 0x1000");
  Check(lock::GuestToPhysical(0x039CA000u) == 0x039CA000u, "physical stays physical");
  Check(lock::GuestToPhysical(0xDFFFFFFFu) == 0x1FFFFFFFu, "end of the 0xC0000000 view");
  Check(lock::GuestToPhysical(0xE0000000u) == 0x00001000u, "start of the 0xE0000000 view");
}

void TestDecide() {
  // Both off: nothing (the hook does not even get here).
  lock::Plan p = lock::Decide(false, false, 4096, true, true, true);
  Check(!p.query && !p.log && !p.ensure, "both off: nothing");
  Check(!lock::ShouldQuery(false, false, 4096) && lock::ShouldQuery(true, false, 4096) &&
            lock::ShouldQuery(false, true, 4096) && !lock::ShouldQuery(true, true, 0),
        "query only with a switch on and a non-empty range");
  p = lock::Decide(true, true, 0, true, true, true);
  Check(!p.query && !p.log && !p.ensure, "empty range: nothing");
  // Probe only.
  p = lock::Decide(true, false, 4096, false, false, false);
  Check(p.query && !p.log && !p.ensure, "probe, no overlap: counted only");
  p = lock::Decide(true, false, 4096, true, true, true);
  Check(p.query && p.log && !p.ensure, "probe, overlap pending: log, no ensure");
  p = lock::Decide(true, false, 4096, true, false, false);
  Check(p.log && !p.ensure, "probe, overlap at entry only: logged");
  p = lock::Decide(true, false, 4096, false, true, false);
  Check(p.log, "probe, overlap after the original only: logged");
  // Ensure only for a copy still pending after the guest's fence wait.
  p = lock::Decide(true, true, 4096, true, true, false);
  Check(p.log && !p.ensure, "ensure: delivered after the fence wait, no call");
  p = lock::Decide(true, true, 4096, true, true, true);
  Check(p.log && p.ensure, "ensure: pending after the fence wait, call");
  p = lock::Decide(true, true, 4096, true, false, false);
  Check(!p.ensure, "ensure: key gone after the original, no call");
  p = lock::Decide(false, true, 4096, true, true, true);
  Check(p.query && !p.log && p.ensure, "ensure without the probe: no line, still ensures");
  p = lock::Decide(false, true, 4096, false, false, true);
  Check(!p.ensure, "no overlap: never ensure");
}

void TestPacer() {
  lock::LinePacer pacer;
  uint32_t suppressed = 0;
  int allowed = 0;
  for (int i = 0; i < 40; ++i) {
    allowed += pacer.Allow(1000, suppressed) ? 1 : 0;
  }
  Check(allowed == 32, "burst of 32");
  Check(!pacer.Allow(1499, suppressed), "no refill before 500 ms");
  Check(pacer.Allow(1500, suppressed) && suppressed == 9, "refill at 500 ms; 9 suppressed");
  Check(!pacer.Allow(1500, suppressed), "one token per 500 ms");
  allowed = 0;
  for (uint64_t t = 2000; t < 12000; t += 100) {
    allowed += pacer.Allow(t, suppressed) ? 1 : 0;
  }
  Check(allowed == 20, "2 lines per second after the burst");
  lock::LinePacer cap;
  allowed = 0;
  for (uint64_t t = 0; t < 4000000; t += 500) {
    allowed += cap.Allow(t, suppressed) ? 1 : 0;
  }
  Check(allowed == static_cast<int>(lock::LinePacer::kMaxLines), "session cap");
}

lock::KeyState Key039() {
  lock::KeyState k;
  k.overlap = true;
  k.key_base = 0x039CA000u;
  k.key_length = 0x40000u;
  k.slot_size = 0x40000u;
  k.width = 256;
  k.height = 256;
  k.format = 6;
  k.exempt = true;
  k.pending = true;
  k.copy_age_frames = 1;
  k.resolve_age_frames = 0;
  return k;
}

void TestLines() {
  lock::Lock l;
  l.resource = 0x4012A000u;
  l.r4 = 0;
  l.flags = 0x10u;
  l.flags_read = true;
  l.texture_base = 0xA39CA000u;
  l.start = 0xA39CA000u;
  l.length = 0x40000u;
  l.lr = 0x82410520u;
  l.tid = 1234;
  lock::KeyState entry = Key039();
  lock::KeyState after = Key039();
  after.pending = false;
  after.copy_age_frames = 0;
  lock::EnsureState none;
  const std::string a = lock::LockLine(l, entry, after, none, 3);
  Check(a.rfind("MCLA_LOCK v=1 fn=lock res=4012A000 r4=0000000000000000 flags=00000010 "
                "base=A39CA000 start=A39CA000 phys=039CA000 len=262144 lr=82410520 tid=1234 ",
                0) == 0,
        "lock line head");
  Check(Has(a, " key=039CA000 key_len=262144 size=256x256 fmt=6 slot=262144 exempt=1 pending=1 "
               "unsubmitted=0 copy_age=1 resolve_age=0 after_key=039CA000"),
        "entry key fields");
  Check(Has(a, "after_key=039CA000 key_len=262144 size=256x256 fmt=6 slot=262144 exempt=1 "
               "pending=0 unsubmitted=0 copy_age=0 resolve_age=0 ensure=0 suppressed=3"),
        "after fields, no ensure");
  lock::EnsureState e;
  e.called = true;
  e.pending = true;
  e.delivered = true;
  e.waited_ms = 2;
  e.waited_us = 1500;
  l.flags_read = false;
  entry.copy_age_frames = UINT32_MAX;
  const std::string b = lock::LockLine(l, entry, lock::KeyState{}, e, 0);
  Check(Has(b, " flags=n/a "), "unread flags");
  Check(Has(b, "copy_age=none"), "no delivered copy yet");
  Check(Has(b, " after_key=none ensure=1 ensure_pending=1 waited_ms=2 waited_us=1500 "
               "delivered=1 timed_out=0 suppressed=0"),
        "ensure fields");
  Check(lock::KeyText(lock::KeyState{}) == "key=none", "no overlap");
  for (const char c : a + b) {
    Check(c >= 0x20 && c < 0x7F, "ASCII single line");
  }
}

void TestMinute() {
  lock::Minute m;
  const lock::EnsureState none;
  m.Add(lock::KeyState{}, lock::KeyState{}, none);
  Check(m.locks == 1 && m.overlapping == 0 && m.key_count == 0, "non-overlapping lock counted");
  lock::KeyState k = Key039();
  lock::EnsureState e;
  e.called = true;
  e.pending = true;
  e.delivered = true;
  e.waited_ms = 3;
  m.Add(k, k, e);
  e.delivered = false;
  e.timed_out = true;
  e.waited_ms = 20;
  m.Add(k, k, e);
  lock::KeyState other = Key039();
  other.key_base = 0x03702000u;
  other.pending = false;
  m.Add(lock::KeyState{}, other, none);
  Check(m.locks == 4 && m.overlapping == 3 && m.pending_entry == 2 && m.pending_after == 2,
        "overlap and pending counts");
  Check(m.ensure_calls == 2 && m.ensure_pending == 2 && m.ensure_delivered == 1 &&
            m.ensure_timeouts == 1 && m.ensure_wait_ms_total == 23 && m.ensure_wait_ms_max == 20,
        "ensure counters");
  Check(lock::KeysText(m) == "[039CA000:2,03702000:1]", "keys in first-seen order");
  const std::string line = lock::MinuteLine(m, true, true);
  Check(line == "MCLA_LOCK v=1 kind=minute probe=1 ensure_on=1 locks=4 overlapping=3 "
                "pending_entry=2 pending_after=2 ensure_calls=2 ensure_pending=2 "
                "ensure_delivered=1 ensure_timeouts=1 ensure_wait_ms_total=23 "
                "ensure_wait_ms_max=20 keys=[039CA000:2,03702000:1]",
        "minute line");
  lock::Minute many;
  for (uint32_t i = 0; i < lock::kMaxKeys + 3; ++i) {
    lock::KeyState key = Key039();
    key.key_base = 0x01000000u + 0x1000u * i;
    many.Add(key, key, none);
  }
  Check(many.key_count == lock::kMaxKeys && many.other_keys == 3 &&
            Has(lock::KeysText(many), ",other:3]"),
        "key table overflow");
  Check(lock::KeysText(lock::Minute{}) == "[]", "empty keys");
  lock::MinuteClock clock;
  Check(!clock.Due(1000), "first call starts the window");
  Check(!clock.Due(60999), "before a minute");
  Check(clock.Due(61000), "a minute");
  Check(!clock.Due(61001), "window restarted");
}

// --- generated code --------------------------------------------------------------
std::string ReadFile(const std::string &path) {
  std::ifstream in(path, std::ios::binary);
  std::ostringstream text;
  text << in.rdbuf();
  return text.str();
}

std::string Body(const std::string &text, const std::string &name) {
  const std::string head = "DEFINE_REX_FUNC(" + name + ") {";
  const size_t begin = text.find(head);
  if (begin == std::string::npos) {
    return {};
  }
  const size_t end = text.find("DEFINE_REX_FUNC(", begin + head.size());
  return text.substr(begin, end == std::string::npos ? std::string::npos : end - begin);
}

void InOrder(const std::string &body, const std::vector<std::string> &lines, const char *what) {
  size_t from = 0;
  for (const auto &line : lines) {
    const size_t at = body.find(line, from);
    if (at == std::string::npos) {
      std::fprintf(stderr, "missing in %s: %s\n", what, line.c_str());
      Check(false, what);
    }
    from = at + line.size();
  }
  Check(true, what);
}

void TestGenerated() {
  const std::string g27 = ReadFile(std::string(MCLA_LOCK_GENERATED_DIR) + "/mcla_recomp.27.cpp");
  Check(!g27.empty(), "generated file readable");
  const std::string core = Body(g27, "sub_82421CA0");
  Check(!core.empty(), "lock core present");
  // Arguments saved at entry, before any other use of r3/r7/r9/r10.
  InOrder(core,
          {"// stwu r1,-176(r1)", "// mr r28,r9", "// mr r24,r3", "// mr r22,r6", "// mr r23,r7",
           "// lwz r9,2148(r31)", "// mr r26,r8", "// mr r27,r10", "// rldicl r11,r4,32,32",
           "// cmpldi cr6,r4,0"},
          "lock core saves r3, r7, r8, r9 and r10 at entry; r4 is a 64-bit argument");
  Check(core.find("// mr r28,r9") < core.find("// lwz r9,2148(r31)"),
        "r9 saved before it is reused");
  // Flags: first stack argument, the caller's r1 + 84.
  InOrder(core, {"// stwu r1,-176(r1)", "// lwz r10,260(r1)", "// ori r25,r10,2",
                 "// lwz r25,260(r1)", "// andi. r10,r25,4112"},
          "lock flags at the caller's r1 + 84");
  // Fence wait first, then the physical range [start, start + r10).
  InOrder(core,
          {"// bl 0x82411e98", "// clrlwi r10,r28,3", "// addi r11,r11,512",
           "// rlwinm r11,r11,0,19,19", "// add r30,r11,r10", "// add r29,r30,r27"},
          "fence wait, then physical start from r9 and end = start + r10");
  InOrder(core, {"// rlwinm r11,r28,12,20,31", "// clrlwi r10,r28,3", "// addi r11,r11,512",
                 "// rlwinm r11,r11,0,19,19", "// add r30,r11,r10"},
          "the GuestToPhysical arithmetic");
  // The texture lock caller passes the flags on the stack.
  const std::string caller = Body(g27, "sub_82410440");
  InOrder(caller,
          {"// stwu r1,-192(r1)", "// mr r27,r7", "// stw r27,84(r1)", "// rlwinm r7,r11,0,0,19",
           "// mr r3,r30", "// bl 0x82421ca0"},
          "sub_82410440 stores the flags at r1 + 84 and calls the core");
  // LockRect / LockBox reach sub_82410440.
  Check(Has(Body(g27, "sub_82410518"), "// bl 0x82410440"), "LockRect calls sub_82410440");
  Check(Has(Body(g27, "sub_824105A8"), "// bl 0x82410440"), "LockBox calls sub_82410440");
  Check(Has(Body(g27, "sub_82422320"), "// bl 0x82421ca0") ||
            Has(Body(g27, "sub_82422430"), "// bl 0x82421ca0"),
        "a buffer lock calls the core");
}

}  // namespace

int main() {
  TestPhysical();
  TestDecide();
  TestPacer();
  TestLines();
  TestMinute();
  TestGenerated();
  std::printf("mcla_lock_probe_test: %d checks passed\n", checks);
  return 0;
}
