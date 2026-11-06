#pragma once

// M11-002 / KI-056 album "Choose a slot" diagnostic: pure parts (offsets,
// CRC-32, JPEG start marker, texture fetch-constant decoder, the hover-path
// mirror, the line pacer and the new-photo tracker). The guest hooks are in
// mcla_album_probe.cpp; everything here is unit-tested
// (tests/mcla_album_probe_test.cpp).
//
// Analysis: docs/evidence/M10-ki056-album-preview-analysis-20261106.md and
// docs/evidence/M9-006-ki-037-photo-analysis-20261029.md. Offsets are on the
// album object (mcUIPhotoAlbum) unless noted; citations are lines of
// generated/default/mcla_recomp.49.cpp at the M11 codegen.
//
// - sub_8263CB78 (virtual tick, r3 = album + 456, r5 = phase): state at
//   r3+132 (lwz :14519; switch on state 3..6), sub-state at r3+136 (:14598),
//   capture frame counter at r3+732 (:14711). State 4 calls sub_8263C8B0
//   (:14700) and then stores state 0 (:14705); the encode chain ends with
//   event 9 snapshotTaken (:14685) and state 4 (:14692).
// - sub_8263C8B0 (r3 = album, new-photo decode): JPEG pointer +1148 (:14110,
//   :14127), start marker FF D8 (:14130, :14136; otherwise it returns with no
//   decode and no bind), decode into image +1140 (bl 0x82775de0 :14148, the
//   returned image is stored back at +1140 :14157), width/height of texture
//   +1132 through its vtable +24/+28 against the image u16 +0/+2 (:14163,
//   :14184), recreate through vtable +92 when they differ (:14209), then bind
//   material +1136 to texture +1132 (bl 0x8218f8d0 :14222). No event.
// - sub_8263C9C8 (r3 = album, r4 = slot; hover preview): mode byte +1225
//   (:14260; set: valid = slot < +1208 :14266 and the JPEG pointer is 0),
//   otherwise valid = bit `slot` of the occupied mask +860 (:14283); JPEG of
//   slot = *(+428) + slot * 0x32000 + 204 (:14317-:14322); start marker
//   (:14343); decode (:14361), bind (:14435), album event 4 (li r4,4 :14446,
//   bl 0x8268ee10 :14448). Invalid slot or bad marker: the failure path
//   loc_8263CB38 (:14453).
// - sub_8263DCF8 (r3 = album, r4 = slot; save): see mcla_album_probe.cpp.
// - Init sub_8263D2A8: +1144 = 4 KB-aligned physical allocation of 0x385000
//   bytes, the memory behind texture +1132 and (8th argument of
//   sub_8218DE38) image +1140; image layout (sub_8218DE38,
//   mcla_recomp.3.cpp): u16 width +0, u16 height +2, u16 stride +12, data
//   pointer +16.

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <string>

namespace mcla::album {

// --- offsets ------------------------------------------------------------------
inline constexpr uint32_t kTickBase = 456;      // r3 of sub_8263CB78 = album + 456
inline constexpr uint32_t kTickState = 132;     // relative to the tick base
inline constexpr uint32_t kTickSubState = 136;
inline constexpr uint32_t kTickFrames = 732;

inline constexpr uint32_t kSlotJpegs = 428;     // album fields
inline constexpr uint32_t kSlotTextureTable = 608;  // + slot * 16 (sub_8263DCF8)
inline constexpr uint32_t kOccupiedMask = 860;
inline constexpr uint32_t kTexture = 1132;
inline constexpr uint32_t kMaterial = 1136;
inline constexpr uint32_t kImage = 1140;
inline constexpr uint32_t kPixels = 1144;
inline constexpr uint32_t kJpeg = 1148;
inline constexpr uint32_t kAltCount = 1208;
inline constexpr uint32_t kAltMode = 1225;

inline constexpr uint32_t kSlotStride = 0x32000;
inline constexpr uint32_t kSlotJpegOffset = 204;
inline constexpr uint32_t kPixelsSize = 0x385000;

inline constexpr uint32_t kImageWidth = 0;      // image object fields
inline constexpr uint32_t kImageHeight = 2;
inline constexpr uint32_t kImageStride = 12;
inline constexpr uint32_t kImageData = 16;

inline constexpr uint32_t kCrcBytes = 4096;     // first 4 KiB of *(+1144)
inline constexpr uint32_t kTextureWords = 16;   // header words of *(+1132) logged
inline constexpr uint32_t kTextureScanWords = 32;  // words scanned for a fetch constant

// --- CRC-32 (IEEE 802.3, reflected 0xEDB88320, as zlib crc32) ------------------
namespace detail {
constexpr std::array<uint32_t, 256> MakeCrcTable() {
  std::array<uint32_t, 256> table{};
  for (uint32_t i = 0; i < 256; ++i) {
    uint32_t c = i;
    for (int k = 0; k < 8; ++k) {
      c = (c & 1) ? 0xEDB88320u ^ (c >> 1) : c >> 1;
    }
    table[i] = c;
  }
  return table;
}
inline constexpr std::array<uint32_t, 256> kCrcTable = MakeCrcTable();
}  // namespace detail

constexpr uint32_t Crc32(const uint8_t *data, size_t size, uint32_t crc = 0) {
  crc = ~crc;
  for (size_t i = 0; i < size; ++i) {
    crc = detail::kCrcTable[(crc ^ data[i]) & 0xFF] ^ (crc >> 8);
  }
  return ~crc;
}

// JPEG start-of-image marker, the check of sub_8263C8B0 and sub_8263C9C8.
constexpr bool IsJpegStart(uint32_t b0, uint32_t b1) {
  return b0 == 0xFF && b1 == 0xD8;
}

// --- texture fetch constant ------------------------------------------------------
// Xenos texture fetch constant (6 dwords, the layout of Xenia's
// xe_gpu_texture_fetch_t): dword0 type:2 ... pitch:9 (bits 22-30) tiled:1
// (bit 31); dword1 format:6 endian:2 ... base_address:20 (bits 12-31); dword2
// (2D) width-1:13, height-1:13. The game texture object at +1132 is a class
// with a vtable whose layout is not traced; the decoder is used to find a
// D3D texture header inside it or behind one of its pointers (heuristic,
// logged as such).
struct Fetch {
  bool valid = false;
  uint32_t base = 0;     // guest physical base address (bits 12-31 of dword1)
  uint32_t width = 0;
  uint32_t height = 0;
  uint32_t format = 0;   // 6 = k_8_8_8_8
  uint32_t pitch = 0;    // in 32-texel units
  bool tiled = false;
};

constexpr Fetch DecodeFetch(const uint32_t *w) {
  Fetch f;
  if ((w[0] & 3u) != 2u) {  // type 2 = texture
    return f;
  }
  f.base = w[1] & 0xFFFFF000u;
  f.format = w[1] & 0x3Fu;
  f.width = (w[2] & 0x1FFFu) + 1;
  f.height = ((w[2] >> 13) & 0x1FFFu) + 1;
  f.pitch = (w[0] >> 22) & 0x1FFu;
  f.tiled = (w[0] >> 31) != 0;
  // Plausible 2D texture with a base address: the album texture is
  // 1280x720 (or the screen size) k_8_8_8_8.
  f.valid = f.base != 0 && f.width >= 16 && f.height >= 16 && f.width <= 8192 &&
            f.height <= 8192;
  return f;
}

// First word index k (k + 6 <= count) whose six words decode as a plausible
// k_8_8_8_8 fetch constant; -1 when none.
constexpr int FindFetch(const uint32_t *words, uint32_t count) {
  for (uint32_t k = 0; k + 6 <= count; ++k) {
    const Fetch f = DecodeFetch(words + k);
    if (f.valid && f.format == 6) {
      return static_cast<int>(k);
    }
  }
  return -1;
}

// True when a fetch base address and the +1144 allocation describe the same
// memory (+1144 holds a guest virtual address of the physical heap; the
// fetch constant holds the physical address).
constexpr bool SameMemory(uint32_t fetch_base, uint32_t pixels) {
  return fetch_base != 0 && pixels != 0 && (fetch_base & 0x1FFFF000u) == (pixels & 0x1FFFF000u);
}

// --- hover path mirror (sub_8263C9C8) -------------------------------------------
enum class HoverPath : uint8_t {
  kDecode,       // valid slot, start marker ok: decode, bind, event 4
  kNotValid,     // empty slot (mask bit clear) or alt mode slot >= count: failure path
  kBadMarker,    // valid slot, no FF D8 at the JPEG: failure path
  kUnreadable,   // the JPEG bytes could not be read (host refused)
};

constexpr const char *HoverPathName(HoverPath path) {
  switch (path) {
    case HoverPath::kDecode:
      return "decode";
    case HoverPath::kNotValid:
      return "not-valid";
    case HoverPath::kBadMarker:
      return "bad-marker";
    case HoverPath::kUnreadable:
      return "unreadable";
  }
  return "unknown";
}

// The occupied bit exactly as slw/and/cntlzw computes it (:14283-:14300):
// slw yields 0 for shift amounts with bit 5 set.
constexpr bool OccupiedBit(uint32_t mask, uint32_t slot) {
  const uint32_t bit = (slot & 0x20u) ? 0u : (1u << (slot & 0x1Fu));
  return (mask & bit) != 0;
}

struct HoverInputs {
  uint32_t slot = 0;
  bool alt_mode = false;   // +1225 != 0
  uint32_t alt_count = 0;  // +1208
  uint32_t mask = 0;       // +860
  uint32_t slot_jpegs = 0; // *(+428)
};

constexpr bool HoverValid(const HoverInputs &in) {
  return in.alt_mode ? in.slot < in.alt_count : OccupiedBit(in.mask, in.slot);
}

// The JPEG the hover decode reads: 0 in alt mode (li r30,0), otherwise the
// slot's record + 204.
constexpr uint32_t HoverJpeg(const HoverInputs &in) {
  return in.alt_mode ? 0u : in.slot_jpegs + in.slot * kSlotStride + kSlotJpegOffset;
}

constexpr HoverPath DecideHoverPath(const HoverInputs &in, bool marker_read, uint32_t b0,
                                    uint32_t b1) {
  if (!HoverValid(in)) {
    return HoverPath::kNotValid;
  }
  if (!marker_read) {
    return HoverPath::kUnreadable;
  }
  return IsJpegStart(b0, b1) ? HoverPath::kDecode : HoverPath::kBadMarker;
}

// --- new-photo path (sub_8263C8B0) ----------------------------------------------
enum class NewPhotoPath : uint8_t {
  kDecode,      // marker ok: decode + (maybe recreate) + bind
  kBadMarker,   // returns without decode or bind
  kUnreadable,  // JPEG pointer or bytes not readable by the host
};

constexpr const char *NewPhotoPathName(NewPhotoPath path) {
  switch (path) {
    case NewPhotoPath::kDecode:
      return "decode-bind";
    case NewPhotoPath::kBadMarker:
      return "bad-marker-skip";
    case NewPhotoPath::kUnreadable:
      return "unreadable";
  }
  return "unknown";
}

constexpr NewPhotoPath DecideNewPhotoPath(bool marker_read, uint32_t b0, uint32_t b1) {
  if (!marker_read) {
    return NewPhotoPath::kUnreadable;
  }
  return IsJpegStart(b0, b1) ? NewPhotoPath::kDecode : NewPhotoPath::kBadMarker;
}

// Whether the vtable +92 recreate ran, inferred from the texture size before
// the call (fetch constant) and the decoded image size after: "yes", "no" or
// "unknown" (no fetch constant found or no decode).
constexpr const char *RecreateGuess(bool decoded, bool fetch_valid, uint32_t fetch_w,
                                    uint32_t fetch_h, uint32_t image_w, uint32_t image_h) {
  if (!decoded || !fetch_valid || !image_w || !image_h) {
    return "unknown";
  }
  return fetch_w == image_w && fetch_h == image_h ? "no" : "yes";
}

// --- line pacer ---------------------------------------------------------------------
// Token bucket per line kind: a burst of kBurst lines, then one line per
// kRefillMs, at most kMaxLines per session. Suppressed calls are counted and
// reported on the next line.
class Pacer {
 public:
  static constexpr uint32_t kBurst = 20;
  static constexpr uint64_t kRefillMs = 500;
  static constexpr uint32_t kMaxLines = 1000;

  // True when a line may be written now; `suppressed` receives the number of
  // calls suppressed since the previous line.
  bool Allow(uint64_t now_ms, uint32_t &suppressed) {
    if (!started_) {
      started_ = true;
      tokens_ = kBurst;
      last_ms_ = now_ms;
    }
    if (now_ms > last_ms_) {
      const uint64_t refill = (now_ms - last_ms_) / kRefillMs;
      if (refill) {
        tokens_ = static_cast<uint32_t>(std::min<uint64_t>(kBurst, tokens_ + refill));
        last_ms_ += refill * kRefillMs;
      }
    }
    if (lines_ >= kMaxLines || tokens_ == 0) {
      ++suppressed_;
      return false;
    }
    --tokens_;
    ++lines_;
    suppressed = suppressed_;
    suppressed_ = 0;
    return true;
  }
  uint32_t lines() const { return lines_; }
  uint32_t pending_suppressed() const { return suppressed_; }

 private:
  bool started_ = false;
  uint32_t tokens_ = 0;
  uint64_t last_ms_ = 0;
  uint32_t lines_ = 0;
  uint32_t suppressed_ = 0;
};

// --- new-photo tracker (seam for fix A) ------------------------------------------
// Counts new-photo decodes ("snapshots") and what replaced the shared preview
// image +1140 since. Diagnostic now; the KI-056 fix A ("re-run the new-photo
// decode once when the chooser opens", analysis section 4.2) would hang off
// PendingRedecode(): at most once per snapshot, only while no hover decode
// and no save consumed it. Nothing acts on it in this build.
class NewPhotoTracker {
 public:
  void OnNewPhoto(bool decoded) {
    ++snapshots_;
    decoded_ = decoded;
    hovers_since_ = 0;
    saved_ = false;
    redecoded_ = false;
  }
  void OnHover(bool decoded) {
    if (decoded) {
      ++hovers_since_;
    }
  }
  void OnSave() { saved_ = true; }
  void OnRedecode() { redecoded_ = true; }

  // Fix A precondition (not acted on in this build): a snapshot exists, its
  // own decode ran, it was not saved and not re-decoded yet.
  bool PendingRedecode() const { return snapshots_ != 0 && decoded_ && !saved_ && !redecoded_; }

  uint32_t snapshots() const { return snapshots_; }
  uint32_t hovers_since() const { return hovers_since_; }
  bool saved() const { return saved_; }
  bool decoded() const { return decoded_; }

 private:
  uint32_t snapshots_ = 0;
  bool decoded_ = false;
  uint32_t hovers_since_ = 0;
  bool saved_ = false;
  bool redecoded_ = false;
};

// --- formatting ------------------------------------------------------------------------
inline std::string Hex8(uint32_t value) {
  char text[16];
  std::snprintf(text, sizeof(text), "%08X", value);
  return text;
}

// "WxH@data/stride" of an image object, or "n/a".
inline std::string ImageText(bool read, uint32_t width, uint32_t height, uint32_t stride,
                             uint32_t data) {
  if (!read) {
    return "n/a";
  }
  char text[64];
  std::snprintf(text, sizeof(text), "%ux%u@%08X/%u", width, height, data, stride);
  return text;
}

// "+k:BASE:WxH:fmt:tiled" for a found fetch constant, or "none".
inline std::string FetchText(const char *where, int index, const Fetch &f) {
  if (index < 0 || !f.valid) {
    return "none";
  }
  char text[96];
  std::snprintf(text, sizeof(text), "%s+%d:%08X:%ux%u:fmt%u:pitch%u:%s", where, index * 4, f.base,
                f.width, f.height, f.format, f.pitch, f.tiled ? "tiled" : "linear");
  return text;
}

// Comma-separated hex words.
inline std::string WordsText(const uint32_t *words, uint32_t count) {
  std::string text;
  for (uint32_t i = 0; i < count; ++i) {
    if (i) {
      text.push_back(',');
    }
    text += Hex8(words[i]);
  }
  return text;
}

}  // namespace mcla::album

namespace rex::memory {
class Memory;
}

// Runtime (mcla_album_probe.cpp): the F10 state.json "album" object. Always a
// JSON object: {"probe": false} while mcla_album_probe is off; otherwise the
// album pointer seen by the hooks, a live read of the album fields and the
// last values of each hook.
std::string MclaAlbumStateJson(rex::memory::Memory *memory, uint8_t *base);
