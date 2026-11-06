#pragma once

// M11-002 / KI-066 police pursuit diagnostic: pure parts (layout, names, the
// snapshot and its transition rule). The guest hooks and the F10 block are in
// mcla_pursuit_probe.cpp; everything here is unit-tested
// (tests/mcla_pursuit_probe_test.cpp, which also checks the offsets against
// the generated code).
//
// Static analysis (docs/evidence/M11-002-diagnostics-triad-20261106.md
// section 3; generated/default/mcla_recomp.<n>.cpp at the M11 codegen):
// - mcPoliceManager M = *(0x8286D8E0): created by sub_822CB9C8 (228 bytes,
//   `stw r31,-10016(r10)` with r10 = 0x82870000, 14.cpp:15308), 0 when no
//   police manager exists. Its per-frame update sub_8270AE38 (55.cpp:61215,
//   r3 = M) is called from sub_822CB350 (bl 0x8270ae38, 14.cpp:14330) and returns at
//   once while the simulation is paused; dt = *(float *)0x827D7508 (lis
//   -32131 / addi 29952 / lfs f31,8).
// - MCLA has no discrete heat level: each target has a law-breaking score
//   (s32, clamped to [0, T[0]]); the only writer is AddLawBreakScore
//   sub_82705DA0 (55.cpp:49108; r3 = M, r4 = target index, r5 = delta,
//   r6 = reason string; record = M + 68 * index; `stw r10,104(r28)`
//   55.cpp:49258). Only index 0 (the player) is evaluated.
// - Tuning table T = 0x8282CAC8 (lis -32125 / addi -13624 in sub_82705DA0):
//   T[0] maximum score (1000 in the image), T[1] pull-over (250), T[2]
//   bullhorn (500), T[3] pursuit (600); the live values are read.
// - Player record at M + 96: +96 infraction bits this frame (sub_82704F88),
//   +100 latched infraction bits (used for the fine and the ticket text,
//   sub_827078E8), +104 score, +116 post-resolution grace (s, float).
// - Manager: +44 tactic pointer array, +48 u16 count; +52 last tactic
//   result; +56 u8 resolution/cutscene busy; +60 cut scene on screen (0 none,
//   1 CopCam, 2 Ticket, 3 Arrest; script setters sub_82703428/90/510); +76
//   fine in $ (sub_82702420 result, 55.cpp:59725); +80 cops in the latest
//   tactic; +84 the pull-over police car.
// - Tactic (mcPoliceTacticBase, ctor sub_82712558): vtable slot +56 returns
//   the type through a `li r3,N; blr` getter: 0 DispatchToLoc ("responding"),
//   1 PullOver, 2 Pursuit, 3 PursuitDbl, 4 ArrowTrap; +20 u8 active, +24
//   result, +32 target racer, +36 law-break record, +72 cop count, +160 Fsm
//   (+4 state array, +8 count, +12 current state index, -1 none).
// - StartTactic sub_82704228 (55.cpp:44842; r3 = M, r4 = type, r6 = HUD
//   flag) sends the HUD message (sub_82650FF0: pullover / pursuit /
//   investigation / searching). There is no persistent HUD flag; the probe
//   reports the player's active tactic instead (the HUD shows it).

#include "mcla_album_probe.h"  // album::Pacer, album::Hex8

#include <cstdint>
#include <string>

namespace mcla::pursuit {

inline constexpr uint32_t kManagerPtr = 0x8286D8E0u;
inline constexpr uint32_t kTuning = 0x8282CAC8u;
inline constexpr uint32_t kDtAddress = 0x827D7508u;

// Manager fields.
inline constexpr uint32_t kTactics = 44;
inline constexpr uint32_t kTacticCount = 48;  // u16
inline constexpr uint32_t kLastResult = 52;
inline constexpr uint32_t kBusy = 56;          // u8
inline constexpr uint32_t kCutscene = 60;
inline constexpr uint32_t kFine = 76;
inline constexpr uint32_t kCops = 80;
inline constexpr uint32_t kPullOverPolice = 84;
// Player law-break record (target index 0); other targets at + 68 * index.
inline constexpr uint32_t kRecord = 96;
inline constexpr uint32_t kRecordStride = 68;
inline constexpr uint32_t kBitsNow = 96;
inline constexpr uint32_t kBitsLatched = 100;
inline constexpr uint32_t kScore = 104;
inline constexpr uint32_t kGrace = 116;        // float seconds
// Tactic fields.
inline constexpr uint32_t kTacticTypeSlot = 56;  // vtable slot
inline constexpr uint32_t kTacticActive = 20;    // u8
inline constexpr uint32_t kTacticResult = 24;
inline constexpr uint32_t kTacticTarget = 32;
inline constexpr uint32_t kTacticRecord = 36;
inline constexpr uint32_t kTacticCops = 72;
inline constexpr uint32_t kTacticFsm = 160;
inline constexpr uint32_t kFsmStates = 4;
inline constexpr uint32_t kFsmCount = 8;
inline constexpr uint32_t kFsmCurrent = 12;
inline constexpr uint32_t kMaxTactics = 32;      // scan bound

// The `li r3,N; blr` getters a tactic vtable slot +56 points to.
constexpr int TacticTypeFromGetter(uint32_t getter) {
  switch (getter) {
    case 0x821A5CC0u:
      return 0;
    case 0x82130008u:
      return 1;
    case 0x823CD3D8u:
      return 2;
    case 0x821A8AA0u:
      return 3;
    case 0x821A7B50u:
      return 4;
  }
  return -1;
}

constexpr const char *TacticName(int type) {
  switch (type) {
    case 0:
      return "responding";  // DispatchToLoc
    case 1:
      return "pull-over";
    case 2:
      return "pursuit";
    case 3:
      return "pursuit-dbl";
    case 4:
      return "arrow-trap";
  }
  return "unknown";
}

constexpr const char *CutsceneName(uint32_t value) {
  switch (value) {
    case 0:
      return "none";
    case 1:
      return "copcam";
    case 2:
      return "ticket";
    case 3:
      return "arrest";
  }
  return "unknown";
}

// Infraction bits (reason strings of sub_82705DA0, ticket categories of
// sub_82674568). "+" separated, "none" for 0, unknown bits as hex.
inline std::string InfractionText(uint32_t bits) {
  struct Bit {
    uint32_t mask;
    const char *name;
  };
  static constexpr Bit kBits[] = {
      {0x1, "speed-low"},      {0x2, "speed-mid"},     {0x4, "speed-high"},
      {0x8, "off-road"},       {0x10, "damage-prop"},  {0x20, "damage-police"},
      {0x40, "damage-police2"}, {0x80, "hit-vehicle"}, {0x200, "red-light"},
      {0x400, "two-wheel"},    {0x800, "burn-out"},    {0x1000, "roof-jump"},
      {0x2000, "wrong-way"},   {0x4000, "block-traffic"},
  };
  if (!bits) {
    return "none";
  }
  std::string text;
  uint32_t known = 0;
  for (const Bit &b : kBits) {
    if (bits & b.mask) {
      text += text.empty() ? "" : "+";
      text += b.name;
      known |= b.mask;
    }
  }
  if (bits & ~known) {
    text += text.empty() ? "" : "+";
    text += "0x" + album::Hex8(bits & ~known);
  }
  return text;
}

struct Thresholds {
  bool read = false;
  int32_t max = 0, pull_over = 0, bullhorn = 0, pursuit = 0;
};

// Score band against the live thresholds: "unknown" without them.
inline const char *ScoreBand(int32_t score, const Thresholds &t) {
  if (!t.read || t.pull_over <= 0 || t.pursuit <= 0) {
    return "unknown";
  }
  if (t.max > 0 && score >= t.max) {
    return "max";
  }
  if (score >= t.pursuit) {
    return "pursuit";
  }
  if (score >= t.pull_over) {
    return "pull-over";
  }
  return score > 0 ? "below" : "zero";
}

// What the probe reads once per police update (and at F10).
struct Snapshot {
  bool present = false;  // manager pointer non-zero and read
  bool complete = false;
  uint32_t manager = 0;
  int32_t score = 0;
  uint32_t bits_now = 0;
  uint32_t bits_latched = 0;
  bool grace = false;    // +116 > 0
  float grace_s = 0;
  uint32_t cutscene = 0;
  uint32_t busy = 0;
  int32_t fine = 0;
  uint32_t tactic_count = 0;
  bool player_tactic = false;  // an active tactic whose record is the player's
  uint32_t tactic = 0;
  int tactic_type = -1;
  int fsm_state = -1;
  uint32_t fsm_state_vtable = 0;
};

// The derived pursuit phase shown in logs and F10.
inline const char *Phase(const Snapshot &s) {
  if (!s.present) {
    return "no-manager";
  }
  if (s.cutscene == 2) {
    return "ticket-cutscene";
  }
  if (s.cutscene == 3) {
    return "arrest-cutscene";
  }
  if (s.cutscene == 1) {
    return "copcam-cutscene";
  }
  if (s.player_tactic) {
    return TacticName(s.tactic_type);
  }
  if (s.grace) {
    return "grace";
  }
  return "none";
}

// A transition is a change of anything but the score itself (score changes
// are logged by the AddLawBreakScore hook) and the grace seconds.
inline std::string Changes(const Snapshot &a, const Snapshot &b) {
  std::string text;
  auto add = [&text](const char *name) {
    text += text.empty() ? "" : ",";
    text += name;
  };
  if (a.present != b.present || a.manager != b.manager) add("manager");
  if (a.bits_latched != b.bits_latched) add("latched");
  if (a.grace != b.grace) add("grace");
  if (a.cutscene != b.cutscene) add("cutscene");
  if (a.busy != b.busy) add("busy");
  if (a.fine != b.fine) add("fine");
  if (a.player_tactic != b.player_tactic || a.tactic != b.tactic ||
      a.tactic_type != b.tactic_type) {
    add("tactic");
  }
  if (a.fsm_state != b.fsm_state) add("fsm");
  if ((a.bits_now != 0) != (b.bits_now != 0)) add("breaking");
  return text;
}

}  // namespace mcla::pursuit

namespace rex::memory {
class Memory;
}

// Runtime (mcla_pursuit_probe.cpp): the F10 state.json "pursuit" object, a
// live read of the police manager (always, read-only; probe on or off) plus
// the last logged values when mcla_pursuit_probe is on.
std::string MclaPursuitStateJson(rex::memory::Memory *memory, uint8_t *base);
