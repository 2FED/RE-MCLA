// M11-002 / KI-056: pure parts of the album diagnostic (src/mcla_album_probe.h)
// and the generated code its offsets mirror (MCLA_ALBUM_GENERATED_RECOMP49).

#include "mcla_album_probe.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#ifndef MCLA_ALBUM_GENERATED_RECOMP49
#error "MCLA_ALBUM_GENERATED_RECOMP49 (path of generated/default/mcla_recomp.49.cpp) is set by CMake"
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

namespace album = mcla::album;

void TestCrc() {
  const char *text = "123456789";
  Check(album::Crc32(reinterpret_cast<const uint8_t *>(text), 9) == 0xCBF43926u,
        "CRC-32 check value of 123456789");
  Check(album::Crc32(nullptr, 0) == 0, "CRC of nothing is 0");
  std::vector<uint8_t> zeros(4096, 0);
  const uint32_t z = album::Crc32(zeros.data(), zeros.size());
  zeros[4095] = 1;
  Check(album::Crc32(zeros.data(), zeros.size()) != z, "one changed byte changes the CRC");
  // Incremental equals one pass.
  const uint32_t a = album::Crc32(reinterpret_cast<const uint8_t *>(text), 4);
  Check(album::Crc32(reinterpret_cast<const uint8_t *>(text) + 4, 5, a) == 0xCBF43926u,
        "incremental CRC");
  constexpr uint8_t kA[1] = {0x61};
  static_assert(album::Crc32(kA, 1) == 0xE8B7BE43u, "constexpr CRC of a");
}

void TestMarker() {
  Check(album::IsJpegStart(0xFF, 0xD8), "FF D8 is a JPEG start");
  Check(!album::IsJpegStart(0xFF, 0xD9) && !album::IsJpegStart(0x00, 0xD8) &&
            !album::IsJpegStart(0, 0),
        "anything else is not");
  Check(album::DecideNewPhotoPath(true, 0xFF, 0xD8) == album::NewPhotoPath::kDecode,
        "marker ok: decode and bind");
  Check(album::DecideNewPhotoPath(true, 0x00, 0x00) == album::NewPhotoPath::kBadMarker,
        "zero buffer: skip");
  Check(album::DecideNewPhotoPath(false, 0xFF, 0xD8) == album::NewPhotoPath::kUnreadable,
        "unread marker");
  Check(std::strcmp(album::NewPhotoPathName(album::NewPhotoPath::kDecode), "decode-bind") == 0 &&
            std::strcmp(album::NewPhotoPathName(album::NewPhotoPath::kBadMarker),
                        "bad-marker-skip") == 0 &&
            std::strcmp(album::NewPhotoPathName(album::NewPhotoPath::kUnreadable),
                        "unreadable") == 0,
        "new-photo path names");
}

// dword0: type 2, pitch (bits 22-30), tiled bit 31; dword1: format 6, base;
// dword2: width-1, height-1 << 13.
std::array<uint32_t, 6> MakeFetch(uint32_t base, uint32_t w, uint32_t h, bool tiled,
                                  uint32_t format = 6) {
  std::array<uint32_t, 6> f{};
  f[0] = 2u | ((w / 32) << 22) | (tiled ? 0x80000000u : 0u);
  f[1] = (base & 0xFFFFF000u) | format | (2u << 6);
  f[2] = (w - 1) | ((h - 1) << 13);
  return f;
}

void TestFetch() {
  const auto f = MakeFetch(0x1ABCD000u, 1280, 720, false);
  const album::Fetch d = album::DecodeFetch(f.data());
  Check(d.valid && d.base == 0x1ABCD000u && d.width == 1280 && d.height == 720 &&
            d.format == 6 && d.pitch == 40 && !d.tiled,
        "decode a linear 1280x720 k_8_8_8_8 fetch");
  const auto t = MakeFetch(0x07D76000u, 960, 540, true);
  const album::Fetch dt = album::DecodeFetch(t.data());
  Check(dt.valid && dt.tiled && dt.width == 960 && dt.height == 540, "tiled 960x540");
  std::array<uint32_t, 6> wrong = f;
  wrong[0] = (wrong[0] & ~3u) | 1u;  // type 1 (vertex)
  Check(!album::DecodeFetch(wrong.data()).valid, "type must be 2");
  std::array<uint32_t, 6> nobase = f;
  nobase[1] &= 0xFFFu;
  Check(!album::DecodeFetch(nobase.data()).valid, "base address required");
  const auto tiny = MakeFetch(0x1000000u, 8, 8, false);
  Check(!album::DecodeFetch(tiny.data()).valid, "8x8 is not the album texture");

  uint32_t words[32] = {};
  words[0] = 0x8203C0DEu;  // vtable
  words[1] = 0x00000001u;
  Check(album::FindFetch(words, 32) == -1, "no fetch in a plain object");
  std::memcpy(words + 7, f.data(), sizeof(uint32_t) * 6);
  Check(album::FindFetch(words, 32) == 7, "fetch found at word 7");
  Check(album::FindFetch(words, 12) == -1, "a fetch crossing the scan end is not found");
  Check(album::FindFetch(words, 13) == 7, "a fetch ending at the scan end is found");
  const auto other = MakeFetch(0x1ABCD000u, 1280, 720, false, 0x12);
  uint32_t only_other[8] = {};
  std::memcpy(only_other, other.data(), sizeof(uint32_t) * 6);
  Check(album::FindFetch(only_other, 8) == -1, "other formats are not taken");

  Check(album::SameMemory(0x1ABCD000u, 0xBABCD000u), "physical vs A-range virtual");
  Check(album::SameMemory(0x1ABCD000u, 0xDABCD000u), "physical vs D-range virtual");
  Check(!album::SameMemory(0x1ABCD000u, 0xBABCE000u), "another page");
  Check(!album::SameMemory(0, 0xBABCD000u) && !album::SameMemory(0x1ABCD000u, 0),
        "zero is never the same memory");

  Check(std::strcmp(album::RecreateGuess(true, true, 1280, 720, 960, 540), "yes") == 0,
        "size change: recreate");
  Check(std::strcmp(album::RecreateGuess(true, true, 1280, 720, 1280, 720), "no") == 0,
        "same size: no recreate");
  Check(std::strcmp(album::RecreateGuess(false, true, 1280, 720, 960, 540), "unknown") == 0,
        "no decode: unknown");
  Check(std::strcmp(album::RecreateGuess(true, false, 0, 0, 960, 540), "unknown") == 0,
        "no fetch: unknown");

  Check(album::FetchText("obj", 7, d) == "obj+28:1ABCD000:1280x720:fmt6:pitch40:linear",
        "fetch text");
  Check(album::FetchText("w3->", 2, dt) == "w3->+8:07D76000:960x540:fmt6:pitch30:tiled",
        "fetch text through a pointer");
  Check(album::FetchText("obj", -1, d) == "none", "no fetch text");
}

void TestHover() {
  Check(album::OccupiedBit(0x1u, 0) && !album::OccupiedBit(0x1u, 1), "bit 0");
  Check(album::OccupiedBit(0x80000000u, 31), "bit 31");
  Check(!album::OccupiedBit(0xFFFFFFFFu, 32) && !album::OccupiedBit(0xFFFFFFFFu, 63),
        "slw gives 0 for shift amounts with bit 5 set");
  Check(album::OccupiedBit(0xFFFFFFFFu, 64), "slw uses the low six bits only (64 -> 0)");

  album::HoverInputs in;
  in.slot = 13;
  in.mask = 0x1FFFu;  // 13 photos in slots 0..12
  in.slot_jpegs = 0x40000000u;
  Check(!album::HoverValid(in), "first empty slot is not valid");
  Check(album::DecideHoverPath(in, false, 0, 0) == album::HoverPath::kNotValid,
        "empty slot: failure path, no decode");
  in.slot = 12;
  Check(album::HoverValid(in), "occupied slot is valid");
  Check(album::HoverJpeg(in) == 0x40000000u + 12 * 0x32000u + 204, "slot JPEG address");
  Check(album::DecideHoverPath(in, true, 0xFF, 0xD8) == album::HoverPath::kDecode,
        "occupied with marker: decode + event 4");
  Check(album::DecideHoverPath(in, true, 0x00, 0x00) == album::HoverPath::kBadMarker,
        "occupied without marker: failure path");
  Check(album::DecideHoverPath(in, false, 0, 0) == album::HoverPath::kUnreadable,
        "unreadable JPEG");
  in.alt_mode = true;
  in.alt_count = 3;
  in.slot = 2;
  Check(album::HoverValid(in) && album::HoverJpeg(in) == 0, "alt mode: slot < count, JPEG 0");
  in.slot = 3;
  Check(!album::HoverValid(in), "alt mode: slot == count is not valid");
  Check(std::strcmp(album::HoverPathName(album::HoverPath::kDecode), "decode") == 0 &&
            std::strcmp(album::HoverPathName(album::HoverPath::kNotValid), "not-valid") == 0 &&
            std::strcmp(album::HoverPathName(album::HoverPath::kBadMarker), "bad-marker") == 0 &&
            std::strcmp(album::HoverPathName(album::HoverPath::kUnreadable), "unreadable") == 0,
        "hover path names");
}

void TestPacer() {
  album::Pacer pacer;
  uint32_t suppressed = 99;
  for (uint32_t i = 0; i < album::Pacer::kBurst; ++i) {
    Check(pacer.Allow(1000, suppressed) && suppressed == 0, "burst allowed");
  }
  Check(!pacer.Allow(1000, suppressed), "burst exhausted");
  Check(!pacer.Allow(1499, suppressed), "no refill before 500 ms");
  Check(pacer.Allow(1500, suppressed) && suppressed == 2, "refill after 500 ms; two suppressed");
  Check(!pacer.Allow(1500, suppressed), "one token per 500 ms");
  Check(pacer.Allow(100000, suppressed) && suppressed == 1, "long quiet: refilled");
  uint32_t allowed = 1;  // the line above
  for (uint32_t i = 0; i < 30; ++i) {
    allowed += pacer.Allow(100000, suppressed) ? 1 : 0;
  }
  Check(allowed == album::Pacer::kBurst, "refill is capped at the burst");
  // Session cap.
  album::Pacer capped;
  uint64_t now = 0;
  uint32_t lines = 0;
  for (uint32_t i = 0; i < 5000; ++i) {
    now += album::Pacer::kRefillMs;
    lines += capped.Allow(now, suppressed) ? 1 : 0;
  }
  Check(lines == album::Pacer::kMaxLines && capped.lines() == album::Pacer::kMaxLines,
        "at most kMaxLines per session");
  Check(capped.pending_suppressed() == 5000 - album::Pacer::kMaxLines, "the rest counted");
}

void TestTracker() {
  album::NewPhotoTracker t;
  Check(t.snapshots() == 0 && !t.clobbered() && t.restores() == 0, "nothing before a photo");
  t.OnHover(true);
  Check(t.clobbered(), "an occupied hover before any photo dirties the shared image");
  t.OnNewPhoto(true);
  Check(t.snapshots() == 1 && t.hovers_since() == 0 && !t.clobbered() && t.decoded(),
        "a new-photo decode cleans the shared image");
  t.OnHover(false);
  Check(t.hovers_since() == 0 && !t.clobbered(), "an empty hover decodes nothing: still clean");
  t.OnHover(true);
  Check(t.hovers_since() == 1 && t.clobbered(), "an occupied hover overwrote +1140: dirty");
  t.OnRestore();
  Check(!t.clobbered() && t.restores() == 1 && t.snapshots() == 1,
        "a restore cleans and counts, and is not a snapshot");
  t.OnHover(false);
  Check(!t.clobbered(), "a second empty hover after the restore: clean, no second restore");
  // "While dirty", not once per snapshot: each occupied-to-empty return
  // restores once.
  t.OnHover(true);
  t.OnHover(true);
  Check(t.clobbered() && t.hovers_since() == 3, "two more occupied hovers: dirty");
  t.OnRestore();
  Check(!t.clobbered() && t.restores() == 2 && t.snapshots() == 1,
        "the second return to an empty slot restores again (same snapshot)");
  t.OnHover(true);
  t.OnNewPhoto(false);
  Check(t.clobbered() && !t.decoded() && t.snapshots() == 2,
        "a skipped new-photo decode (bad marker) leaves the image dirty");
  t.OnNewPhoto(true);
  Check(!t.clobbered() && t.snapshots() == 3, "the game's own re-decode (thumbnail path) cleans");
  t.OnSave();
  Check(t.saved() && !t.clobbered(), "a save does not dirty the image");
  t.OnThumbLoad();
  t.OnThumbLoad();
  Check(t.thumb_loads() == 2 && !t.clobbered(), "thumbnail loads are counted only");
}

album::RestoreInputs Restorable() {
  album::RestoreInputs in;
  in.fix_on = true;
  in.read = true;
  in.pending = 1;
  in.hover.slot = 2;
  in.hover.mask = 0x3u;  // slots 0 and 1 occupied, 2 empty
  in.clobbered = true;
  in.marker_read = true;
  in.b0 = 0xFF;
  in.b1 = 0xD8;
  return in;
}

void TestRestoreDecision() {
  using D = album::RestoreDecision;
  Check(album::DecideRestore(Restorable()) == D::kRestore,
        "pending + empty slot + dirty + FF D8: restore");
  auto in = Restorable();
  in.fix_on = false;
  Check(album::DecideRestore(in) == D::kOff, "switch off: nothing");
  in = Restorable();
  in.read = false;
  Check(album::DecideRestore(in) == D::kUnread, "unread fields: nothing");
  in = Restorable();
  in.pending = 0;
  Check(album::DecideRestore(in) == D::kNotPending, "no new photo pending (+1170 == 0)");
  in = Restorable();
  in.hover.slot = 1;
  Check(album::DecideRestore(in) == D::kOccupied, "occupied slot shows its own photo");
  in = Restorable();
  in.clobbered = false;
  Check(album::DecideRestore(in) == D::kClean, "clean shared image: no decode");
  in = Restorable();
  in.b1 = 0xD9;
  Check(album::DecideRestore(in) == D::kNoMarker, "no FF D8 at +1148 (e.g. reused buffer)");
  in = Restorable();
  in.marker_read = false;
  Check(album::DecideRestore(in) == D::kNoMarker, "unreadable +1148");
  // Alt mode (+1225 set): the game's rule is slot < +1208.
  in = Restorable();
  in.hover.alt_mode = true;
  in.hover.alt_count = 3;
  in.hover.mask = 0;
  in.hover.slot = 2;
  Check(album::DecideRestore(in) == D::kOccupied, "alt mode: slot < count is valid");
  in.hover.slot = 3;
  Check(album::DecideRestore(in) == D::kRestore, "alt mode: slot >= count is empty");
  // The occupied bit as slw computes it: slot 32 and up is never valid.
  in = Restorable();
  in.hover.mask = 0xFFFFFFFFu;
  in.hover.slot = 32;
  Check(album::DecideRestore(in) == D::kRestore, "slw: slot 32 is empty for any mask");
  // Every combination of the five conditions: a restore only when all hold.
  int restores = 0;
  for (int bits = 0; bits < 32; ++bits) {
    album::RestoreInputs c = Restorable();
    c.fix_on = (bits & 1) != 0;
    c.pending = (bits & 2) ? 1u : 0u;
    c.hover.slot = (bits & 4) ? 2u : 0u;  // 2 empty, 0 occupied
    c.clobbered = (bits & 8) != 0;
    c.b0 = (bits & 16) ? 0xFFu : 0x00u;
    const bool restore = album::DecideRestore(c) == D::kRestore;
    Check(restore == (bits == 31), "restore iff every condition holds");
    restores += restore ? 1 : 0;
  }
  Check(restores == 1, "exactly one restoring combination");
  Check(std::strcmp(album::RestoreDecisionName(D::kRestore), "restore") == 0 &&
            std::strcmp(album::RestoreDecisionName(D::kNotPending), "not-pending") == 0 &&
            std::strcmp(album::RestoreDecisionName(D::kNoMarker), "no-marker") == 0,
        "decision names");
}

void TestRestoreJson() {
  // l81: KI-056 fix A is on by default (owner decision 2026-11-08); the owner
  // control key album_preview_restore_off turns it off for one arm.
  static_assert(album::kPreviewRestoreDefault, "fix A default on since l81");
  Check(album::kPreviewRestoreDefault, "KI-056 fix A restore default on (l81)");
  album::RestoreStats s;
  Check(album::RestoreJson(s) ==
            "{\"enabled\": false, \"restores\": 0, \"no_marker\": 0, \"clobbered\": false}",
        "F10 preview_restore block, nothing yet");
  s.enabled = true;
  s.restores = 3;
  s.no_marker = 1;
  s.clobbered = true;
  s.last_seen = true;
  s.last_slot = 4;
  s.last_crc_before = "049EA761";
  s.last_crc_after = "6F46AAAF";
  s.last_age_ms = 250;
  Check(album::RestoreJson(s) ==
            "{\"enabled\": true, \"restores\": 3, \"no_marker\": 1, \"clobbered\": true, "
            "\"last\": {\"slot\": 4, \"crc4k_before\": \"049EA761\", \"crc4k_after\": "
            "\"6F46AAAF\", \"age_ms\": 250}}",
        "F10 preview_restore block with the last restore");
}

void TestFormat() {
  Check(album::Hex8(0x8263C8B0u) == "8263C8B0" && album::Hex8(0) == "00000000", "hex");
  Check(album::ImageText(true, 1280, 720, 5120, 0xBAB00000u) == "1280x720@BAB00000/5120",
        "image text");
  Check(album::ImageText(false, 1, 2, 3, 4) == "n/a", "unread image");
  const uint32_t words[3] = {0x82000000u, 1u, 0xFFFFFFFFu};
  Check(album::WordsText(words, 3) == "82000000,00000001,FFFFFFFF", "words");
  Check(album::WordsText(words, 0).empty(), "no words");
}

// --- the generated code the offsets mirror -------------------------------------
std::string ReadFile(const char *path) {
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

size_t InOrder(const std::string &body, size_t from, const std::vector<std::string> &lines,
               const char *what) {
  for (const auto &line : lines) {
    const size_t at = body.find(line, from);
    if (at == std::string::npos) {
      std::fprintf(stderr, "missing in %s: %s\n", what, line.c_str());
      Check(false, what);
    }
    from = at + line.size();
  }
  Check(true, what);
  return from;
}

std::string Off(const char *reg, uint32_t offset) {
  return std::string("(") + reg + ".u32 + " + std::to_string(offset) + ")";
}

std::string Store(const char *reg, uint32_t offset) {
  return std::string("REX_STORE_U32(") + reg + ".u32 + " + std::to_string(offset) + ", ";
}

void TestGenerated() {
  const std::string text = ReadFile(MCLA_ALBUM_GENERATED_RECOMP49);
  Check(!text.empty(), "generated mcla_recomp.49.cpp readable");

  const std::string tick = Body(text, "sub_8263CB78");
  Check(!tick.empty(), "tick body");
  InOrder(tick, 0,
          {"// cmplwi cr6,r30,2", "// lwz r11," + std::to_string(album::kTickState) + "(r31)",
           "// lwz r11," + std::to_string(album::kTickSubState) + "(r31)",
           "// addi r3,r31,-" + std::to_string(album::kTickBase), "// bl 0x8263c638"},
          "tick: phase 2, state, sub-state, album = r3 - 456, resize");
  InOrder(tick, 0, {"// li r4,9", "// bl 0x8268ee10", "// li r10,4",
                    "// stw r10," + std::to_string(album::kTickState) + "(r31)",
                    "// bl 0x8263c8b0", "// li r11,0",
                    "// stw r11," + std::to_string(album::kTickState) + "(r31)",
                    "// lwz r11," + std::to_string(album::kTickFrames) + "(r31)"},
          "tick: event 9 then state 4; state 4 decodes and returns to 0; frame counter");

  const std::string decode = Body(text, "sub_8263C8B0");
  Check(!decode.empty(), "new-photo body");
  InOrder(decode, 0,
          {"REX_LOAD_U32" + Off("ctx.r31", album::kJpeg), "// bl 0x821cb740",
           "REX_LOAD_U32" + Off("ctx.r31", album::kJpeg), "// cmplwi cr6,r8,255",
           "// bne cr6,0x8263c9b0", "// cmplwi cr6,r11,216", "// bne cr6,0x8263c9b0",
           "REX_LOAD_U32" + Off("ctx.r31", album::kImage), "// bl 0x82775de0",
           "REX_LOAD_U32" + Off("ctx.r31", album::kTexture),
           Store("ctx.r31", album::kImage), "// lhz r30,0(r10)",
           "// lwz r8,24(r9)", "// lhz r30,2(r11)", "// lwz r9,28(r10)", "// lwz r9,92(r10)",
           "REX_LOAD_U32" + Off("ctx.r31", album::kMaterial), "// bl 0x8218f8d0"},
          "new-photo: marker, decode, size compare, recreate, bind");
  Check(decode.find("bl 0x8268ee10") == std::string::npos, "new-photo decode fires no event");

  const std::string hover = Body(text, "sub_8263C9C8");
  Check(!hover.empty(), "hover body");
  InOrder(hover, 0,
          {"REX_LOAD_U8" + Off("ctx.r31", album::kAltMode),
           "REX_LOAD_U32" + Off("ctx.r31", album::kAltCount),
           "REX_LOAD_U32" + Off("ctx.r31", album::kOccupiedMask), "// slw r8,r11,r4",
           "// lis r11,3", "// ori r6,r11,8192", "// li r30,0",
           "REX_LOAD_U32" + Off("ctx.r31", album::kSlotJpegs), "// mullw r10,r4,r6",
           "// addi r30,r11," + std::to_string(album::kSlotJpegOffset), "// cmplwi cr6,r10,255",
           "// cmplwi cr6,r11,216", "// bl 0x82775de0", "// bl 0x8218f8d0", "// li r4,4",
           "// bl 0x8268ee10", "loc_8263CB38:"},
          "hover: mode, count, mask, slot JPEG, marker, decode, bind, event 4, failure path");
  static_assert(album::kSlotStride == ((3u << 16) | 8192u), "lis r11,3 / ori r6,r11,8192");

  const std::string save = Body(text, "sub_8263DCF8");
  Check(!save.empty(), "save body");
  InOrder(save, 0,
          {"REX_LOAD_U32" + Off("ctx.r31", album::kJpeg), "// bl 0x8263c248",
           "// bl 0x82775de0", "// bl 0x8218ee38", "// addi r11,r29,38",
           "REX_LOAD_U32(ctx.r11.u32 + 604)", "// bl 0x8218f8d0", "// or r7,r30,r8",
           Store("ctx.r31", album::kOccupiedMask)},
          "save: copy, decode, thumbnail, slot texture (+608 + slot*16), mask bit");
  static_assert(album::kSlotTextureTable == 38 * 16, "addi r11,r29,38 ; rlwinm 4");

  const std::string init = Body(text, "sub_8263D2A8");
  Check(!init.empty(), "init body");
  InOrder(init, 0,
          {Store("ctx.r31", album::kPixels), "// li r4,1280",
           "REX_LOAD_U32" + Off("ctx.r31", album::kPixels),
           Store("ctx.r31", album::kTexture), "// bl 0x8218de38",
           Store("ctx.r31", album::kImage),
           Store("ctx.r31", album::kSlotJpegs),
           Store("ctx.r31", album::kJpeg)},
          "init: pixels, texture, image over pixels, slot JPEGs, new JPEG");

  // KI-056 fix A: the game's own restore after a thumbnail load, gated by
  // the pending flag, which the save path clears; the cursor move stores
  // +1180 and then hovers; the new-photo decode's global store; the hover
  // callers return without reading r3.
  const std::string thumb = Body(text, "sub_8263E1E8");
  Check(!thumb.empty(), "thumbnail load body");
  InOrder(thumb, 0,
          {"REX_LOAD_U8" + Off("ctx.r31", album::kAltMode),
           "REX_LOAD_U32" + Off("ctx.r31", album::kSlotJpegs), "// bl 0x8218f8d0",
           "REX_LOAD_U8" + Off("ctx.r31", album::kNewPhotoPending), "// beq cr6,0x8263e41c",
           "// mr r3,r31", "// bl 0x8263c8b0"},
          "thumbnail load: slot JPEG, bind, then +1170 gates the new-photo re-decode");
  const std::string chooser = Body(text, "sub_8263E4F0");
  Check(!chooser.empty(), "chooser body");
  InOrder(chooser, 0,
          {"REX_LOAD_U8" + Off("ctx.r31", album::kNewPhotoPending), "// lwz r4,1160(r31)",
           "// bl 0x8263dcf8", "// li r11,0",
           "REX_STORE_U8(ctx.r31.u32 + " + std::to_string(album::kNewPhotoPending) + ", "},
          "chooser: save when +1170 is set, then clear +1170");
  const std::string cursor = Body(text, "sub_8263DFC0");
  Check(!cursor.empty(), "cursor move body");
  InOrder(cursor, 0,
          {Store("ctx.r31", album::kCursorSlot), "// bl 0x825ee0e0", "// mr r4,r30",
           "// mr r3,r31", "// bl 0x8263c9c8", "loc_8263E1DC:"},
          "cursor move: store +1180, prompts, hover, return");
  InOrder(decode, 0,
          {"// lis r10,-32119", "// li r9,5", "// lwz r11,-18004(r10)", "// stw r9,68(r11)",
           "REX_LOAD_U32" + Off("ctx.r31", album::kJpeg)},
          "new-photo: stores 5 to *(*(0x8288B9AC) + 68) before the marker check");
  static_assert((0x82890000u - 18004u) == 0x8288B9ACu, "lis -32119 / -18004");
  for (const char *caller : {"sub_8263CDC0", "sub_8263CE88"}) {
    const std::string body = Body(text, caller);
    const size_t call = body.find("sub_8263C9C8(ctx, base);");
    Check(call != std::string::npos, "hover caller calls the hover");
    const std::string tail = body.substr(call, 400);
    Check(tail.find("// addi r1,r1,112") != std::string::npos &&
              tail.find("ctx.r3.") == std::string::npos,
          "hover caller returns after the hover without reading r3");
  }
  static_assert(album::kPixelsSize == ((56u << 16) | 20480u), "lis 56 / ori 20480 = 0x385000");
}

}  // namespace

int main() {
  TestCrc();
  TestMarker();
  TestFetch();
  TestHover();
  TestPacer();
  TestTracker();
  TestRestoreDecision();
  TestRestoreJson();
  TestFormat();
  TestGenerated();
  std::printf("mcla_album_probe_test: %d checks passed\n", checks);
  return 0;
}
