// M11-002 / KI-066 police pursuit diagnostic (MCLA_PURSUIT v=1).
// See mcla_pursuit_probe.h for the guest layout and its citations, and
// docs/evidence/M11-002-diagnostics-triad-20261106.md.
//
// - F10 block "pursuit": always on, read-only, read through the checked
//   reader (mcla_guest_reader.h) on the diagnostics thread, like guest_ki026.
// - Transition log: three pass-through strong overrides, active only with the
//   init-only switch mcla_pursuit_probe (default off). Each calls the
//   ORIGINAL UNCHANGED and only reads guest memory:
//     sub_82705DA0 AddLawBreakScore: one fn=score line per call (score before
//       and after, delta, reason, dt);
//     sub_82704228 StartTactic: one fn=tactic line per call (type, HUD flag,
//       the returned tactic);
//     sub_8270AE38 mcPoliceManager::Update: a snapshot after each update; one
//       fn=state line when anything but the score changes (Changes()).
//   Lines are rate-limited per kind (album::Pacer).

#include "mcla_pursuit_probe.h"

#include "mcla_guest_reader.h"
#include "mcla_init.h"
#include "mcla_logging.h"

#include <rex/cvar.h>

#include <chrono>
#include <cmath>
#include <cstdio>
#include <mutex>
#include <sstream>
#include <string>

REXCVAR_DEFINE_BOOL(mcla_pursuit_probe, false, "MCLA",
    "Log KI-066 police law-breaking score changes, tactic starts and pursuit "
    "state transitions (MCLA_PURSUIT lines; diagnostic, read-only)")
    .lifecycle(rex::cvar::Lifecycle::kInitOnly);

namespace {

namespace pursuit = mcla::pursuit;
using Reader = mcla::guest::Reader;

bool ProbeOn() {
  static const bool on = REXCVAR_GET(mcla_pursuit_probe);
  return on;
}

uint64_t NowMs() {
  return static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(
                                   std::chrono::steady_clock::now().time_since_epoch())
                                   .count());
}

std::string Num(float value) {
  if (!std::isfinite(value)) {
    return "nan";
  }
  char text[32];
  std::snprintf(text, sizeof(text), "%.4f", static_cast<double>(value));
  return text;
}

pursuit::Thresholds ReadThresholds(const Reader &read) {
  pursuit::Thresholds t;
  bool ok = true;
  t.max = static_cast<int32_t>(read.U32(pursuit::kTuning + 0, ok));
  t.pull_over = static_cast<int32_t>(read.U32(pursuit::kTuning + 4, ok));
  t.bullhorn = static_cast<int32_t>(read.U32(pursuit::kTuning + 8, ok));
  t.pursuit = static_cast<int32_t>(read.U32(pursuit::kTuning + 12, ok));
  t.read = ok;
  return t;
}

float ReadDt(const Reader &read) {
  bool ok = true;
  const float dt = read.F32(pursuit::kDtAddress, ok);
  return ok ? dt : -1.0f;
}

// RTTI class name of a guest object: vtable - 4 = complete object locator,
// +12 = type descriptor, +8 = the decorated name.
std::string ClassName(const Reader &read, uint32_t vtable) {
  if (!vtable) {
    return "";
  }
  bool ok = true;
  const uint32_t col = read.U32(vtable - 4, ok);
  const uint32_t td = ok && col ? read.U32(col + 12, ok) : 0;
  return ok && td ? read.Text(td + 8) : std::string();
}

// Player tactic: the active tactic whose law-break record is the player's.
pursuit::Snapshot ReadSnapshot(const Reader &read, uint32_t manager) {
  pursuit::Snapshot s;
  s.manager = manager;
  if (!manager) {
    return s;
  }
  bool ok = true;
  s.score = static_cast<int32_t>(read.U32(manager + pursuit::kScore, ok));
  s.bits_now = read.U32(manager + pursuit::kBitsNow, ok);
  s.bits_latched = read.U32(manager + pursuit::kBitsLatched, ok);
  s.grace_s = read.F32(manager + pursuit::kGrace, ok);
  s.grace = s.grace_s > 0.0f;
  s.cutscene = read.U32(manager + pursuit::kCutscene, ok);
  s.busy = read.U8(manager + pursuit::kBusy, ok);
  s.fine = static_cast<int32_t>(read.U32(manager + pursuit::kFine, ok));
  const uint32_t list = read.U32(manager + pursuit::kTactics, ok);
  s.tactic_count = read.U16(manager + pursuit::kTacticCount, ok);
  s.present = ok;
  const uint32_t scan = s.tactic_count < pursuit::kMaxTactics ? s.tactic_count : pursuit::kMaxTactics;
  for (uint32_t i = 0; ok && list && i < scan; ++i) {
    const uint32_t tactic = read.U32(list + 4 * i, ok);
    if (!tactic) {
      continue;
    }
    bool t_ok = true;
    const bool active = read.U8(tactic + pursuit::kTacticActive, t_ok) != 0;
    const uint32_t record = read.U32(tactic + pursuit::kTacticRecord, t_ok);
    if (!t_ok || !active || record != manager + pursuit::kRecord) {
      continue;
    }
    s.player_tactic = true;
    s.tactic = tactic;
    const uint32_t vtable = read.U32(tactic, t_ok);
    const uint32_t getter = vtable ? read.U32(vtable + pursuit::kTacticTypeSlot, t_ok) : 0;
    s.tactic_type = pursuit::TacticTypeFromGetter(getter);
    if (const uint32_t fsm = read.U32(tactic + pursuit::kTacticFsm, t_ok)) {
      const int32_t index = static_cast<int32_t>(read.U32(fsm + pursuit::kFsmCurrent, t_ok));
      const uint32_t count = read.U32(fsm + pursuit::kFsmCount, t_ok);
      const uint32_t states = read.U32(fsm + pursuit::kFsmStates, t_ok);
      if (t_ok) {
        s.fsm_state = index;
        if (index >= 0 && static_cast<uint32_t>(index) < count && count <= 64 && states) {
          const uint32_t state = read.U32(states + 4 * static_cast<uint32_t>(index), t_ok);
          s.fsm_state_vtable = state ? read.U32(state, t_ok) : 0;
        }
      }
    }
    break;
  }
  s.complete = ok;
  return s;
}

std::string SnapshotText(const Reader &read, const pursuit::Snapshot &s,
                         const pursuit::Thresholds &t) {
  char head[160];
  std::snprintf(head, sizeof(head), "manager=%08X score=%d band=%s", s.manager, s.score,
                pursuit::ScoreBand(s.score, t));
  std::string text = head;
  text += " phase=" + std::string(pursuit::Phase(s));
  text += " breaking=" + pursuit::InfractionText(s.bits_now);
  text += " latched=" + pursuit::InfractionText(s.bits_latched);
  text += " grace_s=" + Num(s.grace_s);
  text += " cutscene=" + std::string(pursuit::CutsceneName(s.cutscene));
  text += " busy56=" + std::to_string(s.busy);
  text += " fine=" + std::to_string(s.fine);
  text += " tactics=" + std::to_string(s.tactic_count);
  text += " tactic=" + mcla::album::Hex8(s.tactic);
  text += " tactic_type=" + std::string(pursuit::TacticName(s.tactic_type));
  text += " fsm_state=" + std::to_string(s.fsm_state);
  const std::string state_class = ClassName(read, s.fsm_state_vtable);
  text += " fsm_class=" + (state_class.empty() ? std::string("n/a") : state_class);
  char limits[96];
  std::snprintf(limits, sizeof(limits), " thresholds=%d/%d/%d/%d", t.pull_over, t.bullhorn,
                t.pursuit, t.max);
  text += t.read ? limits : " thresholds=n/a";
  return text;
}

struct ProbeState {
  std::mutex mutex;
  pursuit::Snapshot last;
  bool have_last = false;
  uint64_t score_calls = 0, tactic_calls = 0, state_changes = 0;
  int32_t last_delta = 0;
  int32_t last_score_before = 0, last_score_after = 0;
  std::string last_reason;
  int last_started_type = -1;
  uint32_t last_hud_flag = 0;
  uint64_t last_change_ms = 0, last_score_ms = 0, last_tactic_ms = 0;
  std::string last_changes;
  mcla::album::Pacer score_pacer, tactic_pacer, state_pacer;
};

ProbeState &State() {
  static ProbeState state;
  return state;
}

}  // namespace

// AddLawBreakScore: r3 = manager, r4 = target index, r5 = delta, r6 = reason.
REX_EXTERN(sub_82705DA0) {
  if (!ProbeOn()) {
    __imp__sub_82705DA0(ctx, base);
    return;
  }
  const uint32_t manager = ctx.r3.u32;
  const uint32_t index = ctx.r4.u32;
  const int32_t delta = ctx.r5.s32;
  const uint32_t reason_ptr = ctx.r6.u32;
  const uint32_t lr = static_cast<uint32_t>(ctx.lr);
  const Reader read = mcla::guest::HookReader(base);
  const uint32_t record = index < 8 ? manager + pursuit::kRecordStride * index : 0;
  bool ok = record != 0;
  const int32_t before = ok ? static_cast<int32_t>(read.U32(record + pursuit::kScore, ok)) : 0;
  const std::string reason = reason_ptr ? read.Text(reason_ptr) : std::string();
  __imp__sub_82705DA0(ctx, base);
  const int32_t after = ok ? static_cast<int32_t>(read.U32(record + pursuit::kScore, ok)) : 0;
  const uint32_t bits_now = ok ? read.U32(record + pursuit::kBitsNow, ok) : 0;
  const uint32_t bits_latched = ok ? read.U32(record + pursuit::kBitsLatched, ok) : 0;
  const float dt = ReadDt(read);
  const pursuit::Thresholds t = ReadThresholds(read);
  auto &state = State();
  std::lock_guard lock(state.mutex);
  ++state.score_calls;
  state.last_delta = delta;
  state.last_score_before = before;
  state.last_score_after = after;
  state.last_reason = reason;
  state.last_score_ms = NowMs();
  uint32_t suppressed = 0;
  if (state.score_pacer.Allow(NowMs(), suppressed)) {
    MCLA_APP_INFO("MCLA_PURSUIT v=1 fn=score manager={:08X} lr={:08X} target={} delta={} "
                  "reason=\"{}\" before={} after={} band={} breaking={} latched={} dt={} "
                  "suppressed={} complete={}",
                  manager, lr, index, delta, reason, before, after,
                  pursuit::ScoreBand(after, t), pursuit::InfractionText(bits_now),
                  pursuit::InfractionText(bits_latched), Num(dt), suppressed, ok ? 1 : 0);
  }
}

// StartTactic: r3 = manager, r4 = type, r6 = HUD/player flag; returns r3.
REX_EXTERN(sub_82704228) {
  if (!ProbeOn()) {
    __imp__sub_82704228(ctx, base);
    return;
  }
  const uint32_t manager = ctx.r3.u32;
  const int type = ctx.r4.s32;
  const uint32_t hud_flag = ctx.r6.u32;
  const uint32_t lr = static_cast<uint32_t>(ctx.lr);
  __imp__sub_82704228(ctx, base);
  const uint32_t tactic = ctx.r3.u32;
  const Reader read = mcla::guest::HookReader(base);
  const pursuit::Thresholds t = ReadThresholds(read);
  const pursuit::Snapshot s = ReadSnapshot(read, manager);
  const float dt = ReadDt(read);
  auto &state = State();
  std::lock_guard lock(state.mutex);
  ++state.tactic_calls;
  state.last_started_type = type;
  state.last_hud_flag = hud_flag;
  state.last_tactic_ms = NowMs();
  uint32_t suppressed = 0;
  if (state.tactic_pacer.Allow(NowMs(), suppressed)) {
    MCLA_APP_INFO("MCLA_PURSUIT v=1 fn=tactic lr={:08X} type={} ({}) hud_flag={} "
                  "returned={:08X} dt={} {} suppressed={} complete={}",
                  lr, type, pursuit::TacticName(type), hud_flag, tactic, Num(dt),
                  SnapshotText(read, s, t), suppressed, s.complete ? 1 : 0);
  }
}

// mcPoliceManager::Update: r3 = manager.
REX_EXTERN(sub_8270AE38) {
  if (!ProbeOn()) {
    __imp__sub_8270AE38(ctx, base);
    return;
  }
  const uint32_t manager = ctx.r3.u32;
  __imp__sub_8270AE38(ctx, base);
  const Reader read = mcla::guest::HookReader(base);
  const pursuit::Snapshot s = ReadSnapshot(read, manager);
  auto &state = State();
  std::lock_guard lock(state.mutex);
  const pursuit::Snapshot previous = state.last;
  const bool first = !state.have_last;
  state.last = s;
  state.have_last = true;
  const std::string changes = first ? std::string("first") : pursuit::Changes(previous, s);
  if (changes.empty()) {
    return;
  }
  ++state.state_changes;
  state.last_changes = changes;
  state.last_change_ms = NowMs();
  uint32_t suppressed = 0;
  if (state.state_pacer.Allow(NowMs(), suppressed)) {
    const pursuit::Thresholds t = ReadThresholds(read);
    MCLA_APP_INFO("MCLA_PURSUIT v=1 fn=state changed={} from_phase={} dt={} {} suppressed={} "
                  "complete={}",
                  changes, first ? "n/a" : pursuit::Phase(previous), Num(ReadDt(read)),
                  SnapshotText(read, s, t), suppressed, s.complete ? 1 : 0);
  }
}

// --- F10 state block ---------------------------------------------------------------
namespace {

std::string HexQ(uint32_t value) {
  return "\"" + mcla::album::Hex8(value) + "\"";
}

std::string JsonFloat(float value) {
  return std::isfinite(value) ? Num(value) : std::string("null");
}

}  // namespace

std::string MclaPursuitStateJson(rex::memory::Memory *memory, uint8_t *base) {
  std::ostringstream out;
  out << "{\"probe\": " << (ProbeOn() ? "true" : "false")
      << ", \"verified\": \"static analysis only (M11-002 section 3); no in-game run yet\"";
  if (!memory || !base) {
    out << "}";
    return out.str();
  }
  const Reader read(memory, base);
  bool ok = true;
  const uint32_t manager = read.U32(pursuit::kManagerPtr, ok);
  const pursuit::Thresholds t = ReadThresholds(read);
  out << ", \"manager\": " << HexQ(manager);
  if (t.read) {
    out << ", \"thresholds\": {\"pull_over\": " << t.pull_over << ", \"bullhorn\": " << t.bullhorn
        << ", \"pursuit\": " << t.pursuit << ", \"max\": " << t.max << "}";
  }
  out << ", \"dt\": " << JsonFloat(ReadDt(read));
  if (manager) {
    const pursuit::Snapshot s = ReadSnapshot(read, manager);
    bool m_ok = true;
    const int32_t last_result = static_cast<int32_t>(read.U32(manager + pursuit::kLastResult, m_ok));
    const int32_t cops = static_cast<int32_t>(read.U32(manager + pursuit::kCops, m_ok));
    const uint32_t pull_over_police = read.U32(manager + pursuit::kPullOverPolice, m_ok);
    const std::string state_class = ClassName(read, s.fsm_state_vtable);
    out << ", \"score\": " << s.score << ", \"band\": \"" << pursuit::ScoreBand(s.score, t)
        << "\", \"phase\": \"" << pursuit::Phase(s) << "\", \"breaking_bits\": "
        << HexQ(s.bits_now) << ", \"breaking\": \"" << pursuit::InfractionText(s.bits_now)
        << "\", \"latched_bits\": " << HexQ(s.bits_latched) << ", \"latched\": \""
        << pursuit::InfractionText(s.bits_latched) << "\", \"grace_s\": " << JsonFloat(s.grace_s)
        << ", \"cutscene\": \"" << pursuit::CutsceneName(s.cutscene) << "\", \"cutscene60\": "
        << s.cutscene << ", \"busy56\": " << s.busy << ", \"fine\": " << s.fine
        << ", \"last_result52\": " << last_result << ", \"cops80\": " << cops
        << ", \"pull_over_police84\": " << HexQ(pull_over_police)
        << ", \"tactic_count\": " << s.tactic_count << ", \"player_tactic\": {\"active\": "
        << (s.player_tactic ? "true" : "false") << ", \"ptr\": " << HexQ(s.tactic)
        << ", \"type\": \"" << pursuit::TacticName(s.tactic_type)
        << "\", \"fsm_state\": " << s.fsm_state << ", \"fsm_class\": \"" << state_class
        << "\"}, \"hud\": \"" << (s.player_tactic ? pursuit::TacticName(s.tactic_type) : "none")
        << "\", \"complete\": " << (s.complete && m_ok ? "true" : "false");
  }
  if (ProbeOn()) {
    auto &state = State();
    std::lock_guard lock(state.mutex);
    const uint64_t now = NowMs();
    auto age = [now](uint64_t at) { return at && now >= at ? static_cast<int64_t>(now - at) : -1; };
    out << ", \"log\": {\"score_calls\": " << state.score_calls
        << ", \"tactic_calls\": " << state.tactic_calls
        << ", \"state_changes\": " << state.state_changes << ", \"last_score\": {\"delta\": "
        << state.last_delta << ", \"before\": " << state.last_score_before
        << ", \"after\": " << state.last_score_after << ", \"reason\": \"" << state.last_reason
        << "\", \"age_ms\": " << age(state.last_score_ms) << "}, \"last_tactic\": {\"type\": \""
        << pursuit::TacticName(state.last_started_type) << "\", \"hud_flag\": "
        << state.last_hud_flag << ", \"age_ms\": " << age(state.last_tactic_ms)
        << "}, \"last_change\": {\"changed\": \"" << state.last_changes
        << "\", \"phase\": \"" << pursuit::Phase(state.last) << "\", \"age_ms\": "
        << age(state.last_change_ms) << "}}";
  }
  out << "}";
  return out.str();
}
