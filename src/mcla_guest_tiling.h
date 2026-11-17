#pragma once

// M11 l107 guest tiling and scene MSAA (MCLA_GTILE v=1): pure parts of the
// always-on tiling line and of the default-off scene sample-count control.
// The hook is in mcla_guest_tiling.cpp; everything here is unit-tested
// (tests/mcla_guest_tiling_test.cpp, which also checks every address, offset
// and value against the generated code). Design, static analysis and risks:
// docs/evidence/M11-l107-guest-tiling-and-msaa-20261117.md.
//
// Static analysis (generated/default at the M11 codegen, our own reading):
// - BeginTiledRendering sub_8217A470 (TU2): r3 = the colour surface wrapper
//   (null: the display size from 0x827D426C.. globals), r4 is kept for
//   EndTiledRendering, r5 = the clear parameters (7 words, copied to
//   0x827D4330), r6 = the sample count (kept in r18), r7 = flags (bit 0:
//   split the surface into columns instead of rows; bits 2 / 3: clear
//   flags), r8 = the tile overlap in pixels. The surface size comes from the
//   wrapper (vtable +24 / +28) and its format from +60.
// - The game's EDRAM rule: one EDRAM tile is 80x16 pixels at 1x, 80x8 at 2x
//   and 40x8 at 4x; a tile of the scene may use 1024 EDRAM tiles (its depth
//   takes the other 1024 of the 2048). For row tiling the width is padded to
//   160 pixels, the rows per tile are 1024 x tile height / tiles per row,
//   rounded down to 32, and count = (rows - 2 overlap + h - 1) /
//   (rows - overlap) (0x8217A680..0x8217A6FC; the column variant at
//   0x8217A5FC). 960x540: 4x -> rows 320, two tiles; 2x -> rows 672 and 1x
//   -> 1344, one tile. 1280x720: 4x three tiles, 2x two (512 + 208), 1x one.
// - The tile count is stored at 0x827D42A4 and the tile rectangles (x1, y1,
//   x2, y2; 16 bytes each, abutting: tile i is [i*(rows-overlap), ...]) at
//   0x82839258; EndTiledRendering sub_8217B430 resolves each tile's rectangle
//   (colour and depth, sub_82420BA8) to the same position of the destination
//   and resets the count to -1.
// - Callers (three, all direct): the per-frame scene begin sub_822E5ED8
//   (return 0x822E6020; r6 = +384 of the renderer, 3 -> 4, forced to 4 when
//   +393 is set or when the SD flag is set: +373, or +433 while the request
//   flag +432 is set on a thread whose TLS byte +72 is 0; overlap 0, flags 0
//   or 12); the SD-only pass sub_822E5358 (return 0x822E543C; r3 = 0, r6 =
//   +380, forced to 4 by +380 == 3, +393 or +373; runs only when +375 is
//   set, which sub_822E6408 computes as (+380 != 0 or +373) and +364 !=
//   0x28000000); the 4x depth pass of the shadow / depth collector
//   sub_825F48E8 (return 0x825F494C, `li r6,4`).
// - +373 is "output is not HD": the renderer constructor sub_822E47E0 stores
//   1 when XGetVideoMode reports fIsHiDef = 0 (or a command-line parameter at
//   0x8287E06C is set). Our SDK reports fIsHiDef only for at least 1280 wide
//   or 720 high (xboxkrnl_video.cpp), so the 960x540 guest mode of the
//   1080p / 2x2 configuration is an SD output for the game: +384 stays 2
//   (wider than 720), but +373 = 1 forces the scene to 4x and two tiles. The
//   deferred request sub_822E5B20 (+433 = value, +432 = 1, applied by
//   sub_822E5B60 each frame) is used by two save / force-1 / restore pairs
//   (TU7 sub_82215A60 / sub_822150D8, TU49 sub_8263BFC8 / sub_8263CF80);
//   +393 is set by sub_82304648 / sub_823050B8 (and read by a dozen render
//   functions). +373 also scales two camera / LOD factors (sub_822D5A30,
//   sub_8230F3A0) and picks state 9 = 16 in sub_822E60E0, so it is NOT
//   cleared here; only the sample count handed to the tiling changes.
//
// The control (default off, init-only host int mcla_guest_msaa_scene, owner
// keys guest_msaa_scene_1x / guest_msaa_scene_2x): a strong override of
// sub_8217A470 lowers r6 to the requested count for the two renderer passes
// (scene and SD pass) when the game hands a higher one; the shadow / depth
// collector pass is left alone. With 1 the l106 creation write (+384 = 1,
// mcla_guest_msaa.cpp) is also made, so the scene targets, the depth /
// shadow collector size and render state 15 follow the game's own 1x path.
// With 2 +384 already reads 2 (stock). Single-tile rendering then follows
// from the game's own rule (960x540 at 2x or 1x is one tile); 4x is never
// forced into one tile (it needs 3264 EDRAM tiles, our SDK has 2048).

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <string>

namespace mcla::guest_tiling {

// Functions, call sites and return addresses.
inline constexpr uint32_t kBeginTiled = 0x8217A470u;
inline constexpr uint32_t kEndTiled = 0x8217B430u;
inline constexpr uint32_t kSceneBegin = 0x822E5ED8u;
inline constexpr uint32_t kSceneReturn = 0x822E6020u;
inline constexpr uint32_t kSdPass = 0x822E5358u;
inline constexpr uint32_t kSdPassReturn = 0x822E543Cu;
inline constexpr uint32_t kShadowPass = 0x825F48E8u;
inline constexpr uint32_t kShadowReturn = 0x825F494Cu;
// Tiling state written by BeginTiledRendering.
inline constexpr uint32_t kTileCount = 0x827D42A4u;  // lis -32131 / 17164, -104
inline constexpr uint32_t kTileRects = 0x82839258u;  // lis -32124 / -28072
inline constexpr uint32_t kTileRectBytes = 16;
inline constexpr uint32_t kTileFlagsOffset = 296;  // rects + 296: the r7 flags
inline constexpr uint32_t kMaxTileRects = kTileFlagsOffset / kTileRectBytes;  // 18
// The renderer (pointer at 0x8287E064, as in mcla_guest_msaa.h).
inline constexpr uint32_t kRendererPtr = 0x8287E064u;
inline constexpr uint32_t kRendererBytes = 464;
inline constexpr uint32_t kModeOffset = 384;
inline constexpr uint32_t kSdFlagOffset = 373;       // byte: output is not HD
inline constexpr uint32_t kSdPassOnOffset = 375;     // byte: run sub_822E5358
inline constexpr uint32_t kSdPassModeOffset = 380;   // word: its base count
inline constexpr uint32_t kForce4Offset = 393;       // byte: force 4x
inline constexpr uint32_t kPendingOffset = 432;      // byte: request pending
inline constexpr uint32_t kPendingValueOffset = 433; // byte: requested +373
inline constexpr uint32_t kTlsByteOffset = 72;       // [r13 + 0] + 72
// The game's EDRAM budget per tiled surface (1024 of the 2048 tiles).
inline constexpr uint32_t kTileBudget = 1024;

// Where a BeginTiledRendering call comes from.
enum class Where : uint8_t { kScene, kSdPass, kShadow, kOther, kCount };

constexpr Where FromReturn(uint32_t lr) {
  return lr == kSceneReturn    ? Where::kScene
         : lr == kSdPassReturn ? Where::kSdPass
         : lr == kShadowReturn ? Where::kShadow
                               : Where::kOther;
}

inline const char *WhereName(Where where) {
  switch (where) {
    case Where::kScene:
      return "scene";
    case Where::kSdPass:
      return "sd_pass";
    case Where::kShadow:
      return "shadow";
    default:
      return "other";
  }
}

// The two renderer passes the control acts on.
constexpr bool Controlled(Where where) {
  return where == Where::kScene || where == Where::kSdPass;
}

// The requested scene sample count from the cvar: 1 or 2, else 0 (off;
// other values are treated as off and reported as invalid).
constexpr int RequestedCount(int32_t cvar) {
  return cvar == 1 || cvar == 2 ? cvar : 0;
}
constexpr bool RequestValid(int32_t cvar) {
  return cvar == 0 || RequestedCount(cvar) != 0;
}

// mcla_guest_msaa.cpp: the scene 1x request also makes the l106 creation
// write (+384 = 1), so every consumer of +384 sees the game's own 1x mode.
constexpr bool RequestsCreationOne(int32_t cvar) {
  return RequestedCount(cvar) == 1;
}

// The count handed to the tiling: the game's own count, lowered to the
// request for the two renderer passes (never raised; 0 or a negative count
// is not a tiled call the game makes and is left alone).
constexpr int AppliedCount(int game_count, int requested, Where where) {
  if (requested <= 0 || !Controlled(where) || game_count <= requested) {
    return game_count;
  }
  return requested;
}

// The base count the game starts from before its force-4 bytes (scene:
// +384 with 3 -> 4; SD pass: +380 with 3 -> 4; shadow: 4).
constexpr int BaseCount(Where where, int32_t mode, int32_t sd_pass_mode) {
  switch (where) {
    case Where::kScene:
      return mode == 3 ? 4 : mode;
    case Where::kSdPass:
      return sd_pass_mode == 3 ? 4 : sd_pass_mode;
    case Where::kShadow:
      return 4;
    default:
      return -1;
  }
}

// The game passed 4 although its base count is lower (a force-4 byte).
constexpr bool Forced4x(Where where, int game_count, int base_count) {
  return Controlled(where) && game_count == 4 && base_count != 4;
}

// The effective SD flag as sub_822E5B30 / sub_822E5ED8 read it.
constexpr uint8_t EffectiveSdFlag(uint8_t sd, uint8_t pending, uint8_t pending_value,
                                  uint8_t tls_byte) {
  return pending != 0 && tls_byte == 0 ? pending_value : sd;
}

// EDRAM tile size in pixels for a sample count (sub_8217A470 at
// 0x8217A5B8..0x8217A5E8: <= 1: 80x16, 2: 80x8, otherwise 40x8).
struct TilePixels {
  int width;
  int height;
};
constexpr TilePixels EdramTile(int samples) {
  return samples <= 1 ? TilePixels{80, 16} : samples == 2 ? TilePixels{80, 8} : TilePixels{40, 8};
}

// The game's tile height (row tiling) or width (column tiling) and count
// for a surface; 0 for an input the game would trap on.
struct TileRule {
  int span = 0;   // rows per tile (row tiling) or columns per tile
  int count = 0;  // tiles
};
constexpr TileRule GameTileRule(int width, int height, int samples, bool columns, int overlap) {
  if (width <= 0 || height <= 0) {
    return {};
  }
  const TilePixels t = EdramTile(samples);
  if (!columns) {
    const int padded_w = (width + 159) / 160 * 160;
    const int per_row = padded_w / t.width;
    const int rows = (t.height * static_cast<int>(kTileBudget) / per_row) & ~31;
    if (rows - overlap <= 0) {
      return {};
    }
    return {rows, (rows - 2 * overlap + height - 1) / (rows - overlap)};
  }
  const int padded_h = (height + 31) & ~31;
  const int per_col = padded_h / t.height;
  const int cols = (t.width * static_cast<int>(kTileBudget) / per_col) / 160 * 160;
  if (cols - overlap <= 0) {
    return {};
  }
  return {cols, (cols - 2 * overlap + width - 1) / (cols - overlap)};
}

// One tile rectangle as the game stores it.
struct Rect {
  int32_t x1 = 0;
  int32_t y1 = 0;
  int32_t x2 = 0;
  int32_t y2 = 0;
};

// The surface size covered by the tiles (the right / bottom edge of the
// union of the rectangles) and the first tile's height.
struct Cover {
  int32_t width = 0;
  int32_t height = 0;
  int32_t first_span = 0;
};
inline Cover CoverOf(const Rect *rects, int count) {
  Cover c;
  const int n = std::clamp(count, 0, static_cast<int>(kMaxTileRects));
  for (int i = 0; i < n; ++i) {
    c.width = std::max(c.width, rects[i].x2);
    c.height = std::max(c.height, rects[i].y2);
  }
  if (n > 0) {
    c.first_span = rects[0].y2 - rects[0].y1;
  }
  return c;
}

// What one call looked like; a line is written when this changes.
struct Observation {
  int tiles = -1;
  int msaa = -1;       // the count actually handed to the tiling
  int game_msaa = -1;  // the count the game handed
  int32_t width = 0;
  int32_t height = 0;
  bool forced4x = false;
  bool applied = false;
  int32_t mode = -1;    // renderer +384
  int sd = -1;          // effective SD flag
  int force4 = -1;      // renderer +393
  int32_t first_span = 0;
  int rule_tiles = 0;
  bool operator==(const Observation &) const = default;
};

// The always-on line (first call of each caller, then on every change).
inline std::string Line(const Observation &o, int requested, Where where, uint64_t calls) {
  char text[384];
  std::snprintf(text, sizeof(text),
                "MCLA_GTILE v=1 tiles=%d msaa=%d surface=%dx%d forced4x=%d requested_msaa=%d "
                "applied=%d where=%s game_msaa=%d mode=%d sd=%d force4=%d tile_span=%d "
                "rule_tiles=%d rule_ok=%d calls=%llu",
                o.tiles, o.msaa, int(o.width), int(o.height), o.forced4x ? 1 : 0, requested,
                o.applied ? 1 : 0, WhereName(where), o.game_msaa, int(o.mode), o.sd, o.force4,
                int(o.first_span), o.rule_tiles, o.rule_tiles == o.tiles ? 1 : 0,
                static_cast<unsigned long long>(calls));
  return text;
}

// Change lines per caller before further changes are only counted.
inline constexpr uint64_t kMaxChangeLines = 32;

struct PerWhere {
  uint64_t calls = 0;
  uint64_t changes = 0;     // lines written (first call included)
  uint64_t suppressed = 0;  // changes beyond kMaxChangeLines
  uint64_t rewrites = 0;    // calls with r6 lowered
  uint64_t rule_mismatches = 0;
  bool seen = false;
  Observation last;
};

// Records a call; true when a line is to be written.
inline bool Record(PerWhere &w, const Observation &o, bool rewrote) {
  ++w.calls;
  w.rewrites += rewrote ? 1 : 0;
  w.rule_mismatches += o.rule_tiles != o.tiles ? 1 : 0;
  if (w.seen && w.last == o) {
    return false;
  }
  w.seen = true;
  w.last = o;
  if (w.changes >= kMaxChangeLines) {
    ++w.suppressed;
    return false;
  }
  ++w.changes;
  return true;
}

inline std::string PerWhereJson(const PerWhere &w) {
  const Observation &o = w.last;
  char text[512];
  std::snprintf(text, sizeof(text),
                "{\"calls\": %llu, \"changes\": %llu, \"suppressed\": %llu, \"rewrites\": %llu, "
                "\"rule_mismatches\": %llu, \"tiles\": %d, \"msaa\": %d, \"game_msaa\": %d, "
                "\"surface\": \"%dx%d\", \"forced4x\": %s, \"applied\": %s, \"mode\": %d, "
                "\"sd\": %d, \"force4\": %d, \"tile_span\": %d}",
                static_cast<unsigned long long>(w.calls),
                static_cast<unsigned long long>(w.changes),
                static_cast<unsigned long long>(w.suppressed),
                static_cast<unsigned long long>(w.rewrites),
                static_cast<unsigned long long>(w.rule_mismatches), o.tiles, o.msaa, o.game_msaa,
                int(o.width), int(o.height), o.forced4x ? "true" : "false",
                o.applied ? "true" : "false", int(o.mode), o.sd, o.force4, int(o.first_span));
  return text;
}

struct State {
  int32_t cvar = 0;
  std::array<PerWhere, static_cast<size_t>(Where::kCount)> where{};
};

// The F10 "guest_tiling" object.
inline std::string StateJson(const State &s) {
  std::string out = "{\"version\": 1, \"requested_msaa\": ";
  out += std::to_string(RequestedCount(s.cvar));
  out += ", \"cvar\": ";
  out += std::to_string(s.cvar);
  out += ", \"cvar_valid\": ";
  out += RequestValid(s.cvar) ? "true" : "false";
  for (size_t i = 0; i < s.where.size(); ++i) {
    out += ", \"";
    out += WhereName(static_cast<Where>(i));
    out += "\": ";
    out += PerWhereJson(s.where[i]);
  }
  out += "}";
  return out;
}

}  // namespace mcla::guest_tiling

// The F10 state block "guest_tiling" (always present once the module is
// linked).
std::string MclaGuestTilingStateJson();

// True when mcla_guest_msaa_scene asks for 1x (mcla_guest_msaa.cpp then also
// makes the l106 creation write +384 = 1).
bool MclaGuestTilingRequestsCreationOne();
