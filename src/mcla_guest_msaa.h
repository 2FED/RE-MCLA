#pragma once

// M11 l106 guest MSAA off (MCLA_GMSAA v=1): pure parts of the default-off
// dev control that makes the game create and draw its main scene without
// MSAA, and of its always-on log line. The hooks are in mcla_guest_msaa.cpp;
// everything here is unit-tested (tests/mcla_guest_msaa_test.cpp, which also
// checks every address, offset and value against the generated code).
// Design and static analysis: docs/evidence/M11-l106-guest-msaa-off-20261115.md.
//
// Static analysis (generated/default at the M11 codegen, our own reading):
// - The renderer is a 464-byte object (sub_822E4AB0: `li r3,464`, the
//   allocator sub_82130528) built by sub_822E47E0 and stored at the word
//   0x8287E064 (`lis r11,-32120`, `stw r3,-8092(r11)`).
// - sub_822E47E0 (TU15) stores the multisample mode +384 = 2 (`li r11,2`,
//   `stw r11,384(r31)`); two command-line parameter objects (words +4 of
//   0x8287E0BC and 0x8287E094, registered with value 0) replace it with 4 and
//   the size +416 / +420 with 640x480 or 960x720.
// - sub_822E4AB0 (TU15, called once from the graphics start-up
//   sub_823041E8, return 0x823042BC) then stores +384 = 4 when XGetVideoMode
//   reports no HD output (sub_82178880 returns fIsHiDef, byte +16 of the mode
//   structure) and the display is at most 720x576, and +384 = 1 when either
//   of the parameter words +4 of 0x8287E0F8 / 0x8287E10C is non-zero (their
//   static registrations sub_827ACBB0 / sub_827ACBD0 store 0, so only the
//   command line could set them). The load at 0x822E4B80 is the first of
//   those two parameter tests. Nothing after loc_822E4BAC in the function
//   reads +384.
// - Consumers, all after sub_822E4AB0 has returned: the render-target set-up
//   sub_822E4CC8 (called once from sub_82305230 with r3 = the renderer,
//   return 0x82305250) copies +384 into the multisample word +4 of its target
//   descriptor (`lwz r9,384(r31)`, `stw r9,100(r1)`) and creates the scene
//   targets +396 and +400 (and +404 when one of the two size parameters is
//   set) at +416 x +420 through the factory call (vtable +56) with it; the
//   display-sized +412 gets 0 (`stw r28,100(r1)`); sub_82310E40 passes it to sub_825F4750 (the start-up size of the
//   depth / shadow-collector targets); every frame sub_822E5ED8 passes it to
//   BeginTiledRendering sub_8217A470 as r6 (3 becomes 4; some renderer bytes
//   force 4), sub_822E5218 / sub_823115E8 test it for > 0 and sub_822E55D0
//   enables render-state 15 only for > 1.
// - Values: 1 = one sample (no MSAA), 2 = 2x, 4 = 4x. The retail HD start
//   keeps 2.
//
// The control (default off, init-only host cvar mcla_guest_msaa_off, owner key
// guest_msaa_off): a strong override of sub_822E4AB0 runs the original and
// then, when the renderer pointer is plausible and the mode is one the game
// itself writes (2 or 4), stores 1 at +384: the same value the game's own
// branch at loc_822E4BA0 stores, at a point where no consumer has run yet.
// A read-only override of sub_822E4CC8 logs the mode the scene targets are
// created with (first call). With the cvar off no guest memory is written.

#include <cstdint>
#include <cstdio>
#include <string>

namespace mcla::guest_msaa {

// The renderer and its multisample mode.
inline constexpr uint32_t kRendererPtr = 0x8287E064u;  // lis -32120 / -8092
inline constexpr uint32_t kRendererBytes = 464;
inline constexpr uint32_t kModeOffset = 384;
inline constexpr uint32_t kWidthOffset = 416;
inline constexpr uint32_t kHeightOffset = 420;
// The two command-line parameter objects whose word +4 selects mode 1.
inline constexpr uint32_t kParamNoMsaaA = 0x8287E0F8u;  // lis -32120 / -7944
inline constexpr uint32_t kParamNoMsaaB = 0x8287E10Cu;  // lis -32120 / -7924
inline constexpr uint32_t kParamValueOffset = 4;
// Functions, call sites and return addresses.
inline constexpr uint32_t kRendererCtor = 0x822E47E0u;
inline constexpr uint32_t kRendererCreate = 0x822E4AB0u;
inline constexpr uint32_t kFirstParamLoad = 0x822E4B80u;  // lwz r11,4(r10)
inline constexpr uint32_t kGraphicsStartup = 0x823041E8u;
inline constexpr uint32_t kRendererCreateReturn = 0x823042BCu;
inline constexpr uint32_t kTargetSetup = 0x822E4CC8u;
inline constexpr uint32_t kTargetSetupCaller = 0x82305230u;
inline constexpr uint32_t kTargetSetupReturn = 0x82305250u;
inline constexpr uint32_t kSceneBegin = 0x822E5ED8u;
inline constexpr uint32_t kBeginTiledRendering = 0x8217A470u;

// The modes the game stores (sample counts).
inline constexpr int kModeOne = 1;
inline constexpr int kMode2x = 2;
inline constexpr int kMode4x = 4;
// mode_before / mode_after when the renderer pointer is not plausible.
inline constexpr int kModeUnknown = -1;

// A renderer pointer the hook may read and write the mode of.
constexpr bool PointerPlausible(uint32_t p) {
  return p >= 0x00010000u && p <= 0xFFFFFFFFu - kRendererBytes && (p & 3u) == 0;
}

// A mode value the game itself stores (1, 2 or 4).
constexpr bool KnownMode(int64_t mode) {
  return mode == kModeOne || mode == kMode2x || mode == kMode4x;
}

// The mode as text for the F10 report: "1x" / "2x" / "4x", else "other".
inline const char *ModeName(int64_t mode) {
  switch (mode) {
    case kModeOne:
      return "1x";
    case kMode2x:
      return "2x";
    case kMode4x:
      return "4x";
    default:
      return mode == kModeUnknown ? "unknown" : "other";
  }
}

// The sample count BeginTiledRendering is handed for a mode (sub_822E5ED8:
// `cmpwi cr6,r8,3` / `li r6,4`, before the renderer flags that force 4).
constexpr int TiledMode(int mode) {
  return mode == 3 ? kMode4x : mode;
}

// What the creation hook does.
enum class Outcome : uint8_t {
  kNotRequested,  // cvar off: nothing written
  kApplied,       // mode was 2 or 4, 1 written
  kAlreadyOne,    // mode was already 1 (the game's own parameters): nothing written
  kNoRenderer,    // pointer not plausible: nothing written
  kUnknownMode,   // a value the game never stores: nothing written
};

constexpr Outcome Decide(bool requested, bool pointer_plausible, int64_t mode_before) {
  if (!requested) {
    return Outcome::kNotRequested;
  }
  if (!pointer_plausible) {
    return Outcome::kNoRenderer;
  }
  if (mode_before == kModeOne) {
    return Outcome::kAlreadyOne;
  }
  if (mode_before == kMode2x || mode_before == kMode4x) {
    return Outcome::kApplied;
  }
  return Outcome::kUnknownMode;
}

constexpr bool Writes(Outcome outcome) {
  return outcome == Outcome::kApplied;
}

inline const char *OutcomeName(Outcome outcome) {
  switch (outcome) {
    case Outcome::kNotRequested:
      return "not_requested";
    case Outcome::kApplied:
      return "applied";
    case Outcome::kAlreadyOne:
      return "already_1x";
    case Outcome::kNoRenderer:
      return "no_renderer";
    case Outcome::kUnknownMode:
      return "unknown_mode";
  }
  return "unknown";
}

// The mode after the hook: 1 when it wrote, otherwise what it read.
constexpr int64_t ModeAfter(Outcome outcome, int64_t mode_before) {
  return Writes(outcome) ? kModeOne : mode_before;
}

// Where a line is written.
inline constexpr const char *kWhereRendererCreate = "renderer_create";
inline constexpr const char *kWhereTargetSetup = "rt_create";

// The always-on line (one after the renderer creation, one at the first
// render-target set-up). applied=1 at rt_create means the write of the
// creation hook is still in effect when the scene targets are created.
inline std::string Line(bool requested, bool applied, int64_t mode_before, int64_t mode_after,
                        const char *where) {
  char text[160];
  std::snprintf(text, sizeof(text),
                "MCLA_GMSAA v=1 requested=%d applied=%d mode_before=%lld mode_after=%lld where=%s",
                requested ? 1 : 0, applied ? 1 : 0, static_cast<long long>(mode_before),
                static_cast<long long>(mode_after), where);
  return text;
}

// The session state for the F10 report.
struct State {
  bool requested = false;
  bool created = false;  // the creation hook ran
  Outcome outcome = Outcome::kNotRequested;
  uint32_t renderer = 0;
  int64_t mode_before = kModeUnknown;
  int64_t mode_after = kModeUnknown;
  uint32_t width = 0;
  uint32_t height = 0;
  uint64_t target_setups = 0;               // sub_822E4CC8 calls
  int64_t target_setup_mode = kModeUnknown;  // the mode at the first call
  int64_t mode_now = kModeUnknown;           // read when the report is written
};

// The F10 "guest_msaa" object.
inline std::string StateJson(const State &s) {
  char text[512];
  std::snprintf(text, sizeof(text),
                "{\"version\": 1, \"requested\": %s, \"created\": %s, \"outcome\": \"%s\", "
                "\"applied\": %s, \"renderer\": \"0x%08X\", \"mode_before\": %lld, "
                "\"mode_after\": %lld, \"size\": \"%ux%u\", \"rt_create_calls\": %llu, "
                "\"rt_create_mode\": %lld, \"mode_now\": %lld, \"mode_now_name\": \"%s\"}",
                s.requested ? "true" : "false", s.created ? "true" : "false",
                OutcomeName(s.outcome), Writes(s.outcome) ? "true" : "false",
                unsigned(s.renderer), static_cast<long long>(s.mode_before),
                static_cast<long long>(s.mode_after), unsigned(s.width), unsigned(s.height),
                static_cast<unsigned long long>(s.target_setups),
                static_cast<long long>(s.target_setup_mode), static_cast<long long>(s.mode_now),
                ModeName(s.mode_now));
  return text;
}

}  // namespace mcla::guest_msaa

// The F10 state block "guest_msaa" (always present once the module is linked;
// "created": false before the renderer exists).
std::string MclaGuestMsaaStateJson();
