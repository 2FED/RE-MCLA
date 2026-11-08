// M11-002 / KI-056 album "Choose a slot" diagnostic (MCLA_ALBUM v=1).
// See mcla_album_probe.h for the guest layout and its citations, and
// docs/evidence/M11-002-diagnostics-triad-20261106.md.
//
// Five pass-through strong overrides of generated weak functions. With the
// init-only switches mcla_album_probe and mcla_album_preview_restore off (the
// probe's default; the restore defaults to on since l81) each calls the
// original and returns; nothing is read or logged.
// With the probe on, each reads guest state through the checked reader
// (mcla_guest_reader.h) before and after calling the ORIGINAL UNCHANGED, and
// logs one MCLA_ALBUM v=1 line (rate-limited per kind; the tick only on a
// state or sub-state change). The probe writes no guest memory and runs no
// guest code other than the original. The last values go to the F10
// state.json block "album" (MclaAlbumStateJson).
//
// KI-056 fix A (docs/evidence/M11-ki056-l71-album-probe-20261106.md section 3,
// docs/evidence/M11-ki056-fix-a-20261106.md): with mcla_album_preview_restore
// on (default on since l81, album::kPreviewRestoreDefault; probe on or off),
// the hover override, after the original
// sub_8263C9C8 returned, re-runs the game's own new-photo decode
// sub_8263C8B0(album) when a new photo is pending (u8 +1170), the hovered slot
// is empty by the game's rule (album::HoverValid false), an occupied hover
// decoded over the shared preview image since the last new-photo decode
// (NewPhotoTracker::clobbered) and *(+1148) starts with FF D8
// (album::DecideRestore). The host writes no guest memory itself; the game
// call writes what it always writes (the decode into +1140/+1144, the bind of
// +1136, and 5 to *(*(0x8288B9AC) + 68)). One MCLA_ALBUM v=1 fn=restore line
// per restore (rate-limited).

#include "mcla_album_probe.h"

#include "mcla_guest_reader.h"
#include "mcla_init.h"
#include "mcla_logging.h"

#include <rex/cvar.h>

#include <array>
#include <chrono>
#include <cstdio>
#include <mutex>
#include <sstream>
#include <string>

REXCVAR_DEFINE_BOOL(mcla_album_probe, false, "MCLA",
    "Log KI-056 photo album preview decodes, hovers, saves and state changes "
    "(MCLA_ALBUM lines; diagnostic, read-only)")
    .lifecycle(rex::cvar::Lifecycle::kInitOnly);

REXCVAR_DEFINE_BOOL(mcla_album_preview_restore, mcla::album::kPreviewRestoreDefault, "MCLA",
    "KI-056 fix A: in the photo album save chooser, re-run the game's new-photo "
    "decode when the cursor returns to an empty slot after an occupied slot "
    "overwrote the shared preview (quality fix, default on since l81)")
    .lifecycle(rex::cvar::Lifecycle::kInitOnly);

namespace {

namespace album = mcla::album;
using Reader = mcla::guest::Reader;

bool ProbeOn() {
  static const bool on = REXCVAR_GET(mcla_album_probe);
  return on;
}

bool FixOn() {
  static const bool on = REXCVAR_GET(mcla_album_preview_restore);
  return on;
}

// Set while the hover override runs the fix A restore, so the new-photo
// override passes that call through (a restore, not a snapshot).
thread_local bool t_in_restore = false;

uint64_t NowMs() {
  return static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(
                                   std::chrono::steady_clock::now().time_since_epoch())
                                   .count());
}

struct Image {
  uint32_t ptr = 0;
  bool read = false;
  uint32_t width = 0, height = 0, stride = 0, data = 0;
  std::string Text() const {
    return album::ImageText(read, width, height, stride, data);
  }
};

Image ReadImage(const Reader &read, uint32_t ptr) {
  Image image;
  image.ptr = ptr;
  if (!ptr) {
    return image;
  }
  bool ok = true;
  image.width = read.U16(ptr + album::kImageWidth, ok);
  image.height = read.U16(ptr + album::kImageHeight, ok);
  image.stride = read.U16(ptr + album::kImageStride, ok);
  image.data = read.U32(ptr + album::kImageData, ok);
  image.read = ok;
  return image;
}

struct Crc {
  bool read = false;
  uint32_t value = 0;
  std::string Text() const { return read ? album::Hex8(value) : std::string("n/a"); }
};

// CRC-32 of the first 4 KiB at `address` (raw guest bytes).
Crc ReadCrc(const Reader &read, uint32_t address) {
  Crc crc;
  std::array<uint8_t, album::kCrcBytes> bytes{};
  if (address && read.Bytes(address, album::kCrcBytes, bytes.data())) {
    crc.read = true;
    crc.value = album::Crc32(bytes.data(), bytes.size());
  }
  return crc;
}

struct Marker {
  bool read = false;
  uint32_t b0 = 0, b1 = 0;
  std::string Text() const {
    if (!read) {
      return "n/a";
    }
    char text[8];
    std::snprintf(text, sizeof(text), "%02X%02X", b0, b1);
    return text;
  }
};

Marker ReadMarker(const Reader &read, uint32_t jpeg) {
  Marker m;
  if (!jpeg) {
    return m;
  }
  bool ok = true;
  m.b0 = read.U8(jpeg, ok);
  m.b1 = read.U8(jpeg + 1, ok);
  m.read = ok;
  return m;
}

// The texture object at +1132: its first words, and a k_8_8_8_8 fetch
// constant found inside it or behind one of its pointers (heuristic).
struct Texture {
  uint32_t ptr = 0;
  bool read = false;
  std::array<uint32_t, album::kTextureWords> words{};
  std::string fetch_where = "none";
  album::Fetch fetch;
  int fetch_index = -1;
  std::string FetchText() const {
    return album::FetchText(fetch_where.c_str(), fetch_index, fetch);
  }
};

Texture ReadTexture(const Reader &read, uint32_t ptr) {
  Texture tex;
  tex.ptr = ptr;
  if (!ptr) {
    return tex;
  }
  std::array<uint32_t, album::kTextureScanWords> scan{};
  bool ok = true;
  for (uint32_t i = 0; i < album::kTextureScanWords && ok; ++i) {
    scan[i] = read.U32(ptr + 4 * i, ok);
  }
  if (!ok) {
    // At least the header words.
    ok = true;
    for (uint32_t i = 0; i < album::kTextureWords && ok; ++i) {
      scan[i] = read.U32(ptr + 4 * i, ok);
    }
    if (!ok) {
      return tex;
    }
  }
  tex.read = true;
  for (uint32_t i = 0; i < album::kTextureWords; ++i) {
    tex.words[i] = scan[i];
  }
  tex.fetch_index = album::FindFetch(scan.data(), album::kTextureScanWords);
  if (tex.fetch_index >= 0) {
    tex.fetch = album::DecodeFetch(scan.data() + tex.fetch_index);
    tex.fetch_where = "obj";
    return tex;
  }
  // Behind a pointer word (word 0 is the vtable).
  for (uint32_t i = 1; i < album::kTextureWords; ++i) {
    const uint32_t p = tex.words[i];
    if (!p || p % 4 || !read.Readable(p, album::kTextureScanWords * 4)) {
      continue;
    }
    std::array<uint32_t, album::kTextureScanWords> inner{};
    bool inner_ok = true;
    for (uint32_t k = 0; k < album::kTextureScanWords && inner_ok; ++k) {
      inner[k] = read.U32(p + 4 * k, inner_ok);
    }
    const int index = inner_ok ? album::FindFetch(inner.data(), album::kTextureScanWords) : -1;
    if (index >= 0) {
      tex.fetch = album::DecodeFetch(inner.data() + index);
      tex.fetch_index = index;
      char where[24];
      std::snprintf(where, sizeof(where), "w%u->", i);
      tex.fetch_where = where;
      return tex;
    }
  }
  return tex;
}

// Everything the probe reads from the album object at one moment.
struct AlbumView {
  uint32_t album = 0;
  bool read = false;
  uint32_t jpeg = 0, image_ptr = 0, pixels = 0, texture_ptr = 0, material = 0;
  uint32_t mask = 0, alt_mode = 0, alt_count = 0, slot_jpegs = 0;
  uint32_t pending = 0, cursor = 0;  // u8 +1170, u32 +1180
  Marker marker;
  Image image;
  Crc crc;
  Texture texture;
};

AlbumView ReadAlbum(const Reader &read, uint32_t album_ptr, bool with_texture) {
  AlbumView v;
  v.album = album_ptr;
  if (!album_ptr) {
    return v;
  }
  bool ok = true;
  v.jpeg = read.U32(album_ptr + album::kJpeg, ok);
  v.image_ptr = read.U32(album_ptr + album::kImage, ok);
  v.pixels = read.U32(album_ptr + album::kPixels, ok);
  v.texture_ptr = read.U32(album_ptr + album::kTexture, ok);
  v.material = read.U32(album_ptr + album::kMaterial, ok);
  v.mask = read.U32(album_ptr + album::kOccupiedMask, ok);
  v.alt_mode = read.U8(album_ptr + album::kAltMode, ok);
  v.alt_count = read.U32(album_ptr + album::kAltCount, ok);
  v.slot_jpegs = read.U32(album_ptr + album::kSlotJpegs, ok);
  v.pending = read.U8(album_ptr + album::kNewPhotoPending, ok);
  v.cursor = read.U32(album_ptr + album::kCursorSlot, ok);
  v.read = ok;
  v.marker = ReadMarker(read, v.jpeg);
  v.image = ReadImage(read, v.image_ptr);
  v.crc = ReadCrc(read, v.pixels);
  if (with_texture) {
    v.texture = ReadTexture(read, v.texture_ptr);
  }
  return v;
}

// Fix A without the probe: only the scalar fields the hover rule and the
// restore decision need (no image, CRC or texture reads).
AlbumView ReadAlbumFields(const Reader &read, uint32_t album_ptr) {
  AlbumView v;
  v.album = album_ptr;
  if (!album_ptr) {
    return v;
  }
  bool ok = true;
  v.jpeg = read.U32(album_ptr + album::kJpeg, ok);
  v.pixels = read.U32(album_ptr + album::kPixels, ok);
  v.mask = read.U32(album_ptr + album::kOccupiedMask, ok);
  v.alt_mode = read.U8(album_ptr + album::kAltMode, ok);
  v.alt_count = read.U32(album_ptr + album::kAltCount, ok);
  v.slot_jpegs = read.U32(album_ptr + album::kSlotJpegs, ok);
  v.pending = read.U8(album_ptr + album::kNewPhotoPending, ok);
  v.cursor = read.U32(album_ptr + album::kCursorSlot, ok);
  v.read = ok;
  return v;
}

album::HoverInputs HoverInputsOf(const AlbumView &v, uint32_t slot) {
  album::HoverInputs in;
  in.slot = slot;
  in.alt_mode = v.alt_mode != 0;
  in.alt_count = v.alt_count;
  in.mask = v.mask;
  in.slot_jpegs = v.slot_jpegs;
  return in;
}

const char *Bool01(bool value) {
  return value ? "1" : "0";
}

// --- shared state (guest thread writes, F10 thread reads) ---------------------
struct LastTick {
  bool seen = false;
  uint32_t from_state = 0, from_sub = 0, to_state = 0, to_sub = 0, frames = 0;
  uint64_t at_ms = 0;
};
struct LastNewPhoto {
  bool seen = false;
  std::string path;
  std::string marker;
  uint32_t jpeg = 0;
  std::string image_before, image_after;
  uint32_t pixels = 0;
  bool data_is_pixels = false;
  std::string crc_before, crc_after;
  bool crc_changed = false;
  uint32_t texture_before = 0, texture_after = 0;
  std::string fetch_before, fetch_after;
  std::string recreate;
  std::string texture_words;
  uint64_t at_ms = 0;
};
struct LastHover {
  bool seen = false;
  uint32_t slot = 0;
  bool occupied = false;
  uint32_t mask = 0;
  uint32_t alt_mode = 0;
  uint32_t pending = 0, cursor = 0;  // +1170, +1180 after the call
  std::string path;
  uint32_t jpeg = 0;
  std::string marker;
  std::string image_after;
  std::string crc_before, crc_after;
  uint64_t at_ms = 0;
};
struct LastSave {
  bool seen = false;
  uint32_t slot = 0;
  uint32_t mask_before = 0, mask_after = 0;
  std::string image_after;
  std::string crc_before, crc_after;
  uint32_t slot_texture = 0;
  uint64_t at_ms = 0;
};

struct LastThumb {
  bool seen = false;
  uint32_t slot = 0;
  uint32_t pending = 0;
  std::string crc_before, crc_after;
  uint64_t at_ms = 0;
};
struct LastRestore {
  bool seen = false;
  uint32_t slot = 0;
  std::string crc_before, crc_after;
  uint64_t at_ms = 0;
};

struct ProbeState {
  std::mutex mutex;
  uint32_t album = 0;
  LastTick tick;
  LastNewPhoto new_photo;
  LastHover hover;
  LastSave save;
  LastThumb thumb;
  LastRestore restore;
  uint64_t ticks_logged = 0, new_photo_calls = 0, hover_calls = 0, save_calls = 0;
  uint64_t thumb_calls = 0;
  uint32_t restore_no_marker = 0;
  album::Pacer tick_pacer, new_photo_pacer, hover_pacer, save_pacer, thumb_pacer, restore_pacer;
  album::NewPhotoTracker tracker;
};

ProbeState &State() {
  static ProbeState state;
  return state;
}

}  // namespace

// --- hooks ----------------------------------------------------------------------

// Virtual tick of the album UI object: r3 = album + 456, r5 = phase.
REX_EXTERN(sub_8263CB78) {
  if (!ProbeOn()) {
    __imp__sub_8263CB78(ctx, base);
    return;
  }
  const uint32_t tick = ctx.r3.u32;
  const Reader read = mcla::guest::HookReader(base);
  bool before_ok = true;
  const uint32_t from_state = read.U32(tick + album::kTickState, before_ok);
  const uint32_t from_sub = read.U32(tick + album::kTickSubState, before_ok);
  __imp__sub_8263CB78(ctx, base);
  bool ok = before_ok;
  const uint32_t to_state = read.U32(tick + album::kTickState, ok);
  const uint32_t to_sub = read.U32(tick + album::kTickSubState, ok);
  if (!ok || (from_state == to_state && from_sub == to_sub)) {
    return;
  }
  const uint32_t frames = read.U32(tick + album::kTickFrames, ok);
  const uint32_t album_ptr = tick - album::kTickBase;
  const Image image = ReadImage(read, read.U32(album_ptr + album::kImage, ok));
  auto &state = State();
  std::lock_guard lock(state.mutex);
  state.album = album_ptr;
  state.tick = {true, from_state, from_sub, to_state, to_sub, frames, NowMs()};
  uint32_t suppressed = 0;
  if (state.tick_pacer.Allow(NowMs(), suppressed)) {
    ++state.ticks_logged;
    MCLA_APP_INFO("MCLA_ALBUM v=1 fn=tick album={:08X} from={}/{} to={}/{} frames732={} "
                  "image={} suppressed={} complete={}",
                  album_ptr, from_state, from_sub, to_state, to_sub, frames, image.Text(),
                  suppressed, Bool01(ok));
  }
}

// New-photo decode (state 4): r3 = album.
REX_EXTERN(sub_8263C8B0) {
  if (t_in_restore || (!ProbeOn() && !FixOn())) {
    // Off, or the fix A restore itself (counted and logged by the hover
    // override as a restore, not as a snapshot).
    __imp__sub_8263C8B0(ctx, base);
    return;
  }
  if (!ProbeOn()) {
    // Fix A only: track the decode so a new photo clears `clobbered`.
    const uint32_t album_ptr = ctx.r3.u32;
    const Reader read = mcla::guest::HookReader(base);
    bool ok = true;
    const uint32_t jpeg = album_ptr ? read.U32(album_ptr + album::kJpeg, ok) : 0;
    const Marker marker = ok ? ReadMarker(read, jpeg) : Marker{};
    __imp__sub_8263C8B0(ctx, base);
    const bool decoded = album::DecideNewPhotoPath(marker.read, marker.b0, marker.b1) ==
                         album::NewPhotoPath::kDecode;
    auto &state = State();
    std::lock_guard lock(state.mutex);
    state.album = album_ptr;
    ++state.new_photo_calls;
    state.tracker.OnNewPhoto(decoded);
    return;
  }
  const uint32_t album_ptr = ctx.r3.u32;
  const uint32_t lr = static_cast<uint32_t>(ctx.lr);
  const Reader read = mcla::guest::HookReader(base);
  const AlbumView before = ReadAlbum(read, album_ptr, true);
  __imp__sub_8263C8B0(ctx, base);
  const AlbumView after = ReadAlbum(read, album_ptr, true);
  const album::NewPhotoPath path =
      album::DecideNewPhotoPath(before.marker.read, before.marker.b0, before.marker.b1);
  const bool decoded = path == album::NewPhotoPath::kDecode;
  const char *recreate =
      album::RecreateGuess(decoded, before.texture.fetch_index >= 0 && before.texture.fetch.valid,
                           before.texture.fetch.width, before.texture.fetch.height,
                           after.image.width, after.image.height);
  const bool data_is_pixels = after.image.read && after.image.data == after.pixels;
  const bool fetch_on_pixels =
      after.texture.fetch_index >= 0 && album::SameMemory(after.texture.fetch.base, after.pixels);
  const bool crc_changed =
      before.crc.read && after.crc.read && before.crc.value != after.crc.value;
  auto &state = State();
  std::lock_guard lock(state.mutex);
  state.album = album_ptr;
  ++state.new_photo_calls;
  state.tracker.OnNewPhoto(decoded);
  auto &last = state.new_photo;
  last.seen = true;
  last.path = album::NewPhotoPathName(path);
  last.marker = before.marker.Text();
  last.jpeg = before.jpeg;
  last.image_before = before.image.Text();
  last.image_after = after.image.Text();
  last.pixels = after.pixels;
  last.data_is_pixels = data_is_pixels;
  last.crc_before = before.crc.Text();
  last.crc_after = after.crc.Text();
  last.crc_changed = crc_changed;
  last.texture_before = before.texture_ptr;
  last.texture_after = after.texture_ptr;
  last.fetch_before = before.texture.FetchText();
  last.fetch_after = after.texture.FetchText();
  last.recreate = recreate;
  last.texture_words =
      after.texture.read ? album::WordsText(after.texture.words.data(), album::kTextureWords)
                         : std::string("n/a");
  last.at_ms = NowMs();
  uint32_t suppressed = 0;
  if (state.new_photo_pacer.Allow(NowMs(), suppressed)) {
    MCLA_APP_INFO("MCLA_ALBUM v=1 fn=new_photo album={:08X} lr={:08X} jpeg1148={:08X} marker={} "
                  "path={} bind={} image_before={} image_after={} pixels1144={:08X} "
                  "data_is_pixels={} crc4k_before={} crc4k_after={} crc_changed={} "
                  "tex1132_before={:08X} tex1132_after={:08X} fetch_before={} fetch_after={} "
                  "fetch_on_pixels={} recreate92={} tex_words={} material1136={:08X} "
                  "snapshot={} suppressed={} complete={}",
                  album_ptr, lr, before.jpeg, last.marker, last.path, Bool01(decoded),
                  last.image_before, last.image_after, after.pixels, Bool01(data_is_pixels),
                  last.crc_before, last.crc_after, Bool01(crc_changed), before.texture_ptr,
                  after.texture_ptr, last.fetch_before, last.fetch_after,
                  Bool01(fetch_on_pixels), recreate, last.texture_words, after.material,
                  state.tracker.snapshots(), suppressed, Bool01(before.read && after.read));
  }
}

// Hover preview: r3 = album, r4 = slot. With mcla_album_preview_restore on,
// also KI-056 fix A after the original returned (see the file comment).
REX_EXTERN(sub_8263C9C8) {
  const bool probe = ProbeOn();
  const bool fix = FixOn();
  if (!probe && !fix) {
    __imp__sub_8263C9C8(ctx, base);
    return;
  }
  const uint32_t album_ptr = ctx.r3.u32;
  const uint32_t slot = ctx.r4.u32;
  const uint32_t lr = static_cast<uint32_t>(ctx.lr);
  const Reader read = mcla::guest::HookReader(base);
  const AlbumView before =
      probe ? ReadAlbum(read, album_ptr, false) : ReadAlbumFields(read, album_ptr);
  const album::HoverInputs in = HoverInputsOf(before, slot);
  const uint32_t jpeg = album::HoverJpeg(in);
  const Marker marker = before.read && album::HoverValid(in) ? ReadMarker(read, jpeg) : Marker{};
  const album::HoverPath path = album::DecideHoverPath(in, marker.read, marker.b0, marker.b1);
  // Only a hover whose fields were read and whose path is a decode counts as
  // a write over the shared image.
  const bool decoded = before.read && path == album::HoverPath::kDecode;
  __imp__sub_8263C9C8(ctx, base);
  bool clobbered = false;
  if (probe) {
    const AlbumView after = ReadAlbum(read, album_ptr, false);
    const bool occupied = album::OccupiedBit(before.mask, slot);
    auto &state = State();
    std::lock_guard lock(state.mutex);
    state.album = album_ptr;
    ++state.hover_calls;
    state.tracker.OnHover(decoded);
    clobbered = state.tracker.clobbered();
    auto &last = state.hover;
    last.seen = true;
    last.slot = slot;
    last.occupied = occupied;
    last.mask = before.mask;
    last.alt_mode = before.alt_mode;
    last.pending = after.pending;
    last.cursor = after.cursor;
    last.path = album::HoverPathName(path);
    last.jpeg = jpeg;
    last.marker = marker.Text();
    last.image_after = after.image.Text();
    last.crc_before = before.crc.Text();
    last.crc_after = after.crc.Text();
    last.at_ms = NowMs();
    uint32_t suppressed = 0;
    if (state.hover_pacer.Allow(NowMs(), suppressed)) {
      MCLA_APP_INFO("MCLA_ALBUM v=1 fn=hover album={:08X} lr={:08X} slot={} mask860={:08X} "
                    "occupied={} alt1225={} count1208={} pending1170={} cursor1180={} "
                    "jpeg={:08X} marker={} path={} success={} event4={} image_after={} "
                    "crc4k_before={} crc4k_after={} hovers_since_snapshot={} clobbered={} "
                    "suppressed={} complete={}",
                    album_ptr, lr, slot, before.mask, Bool01(occupied), before.alt_mode,
                    before.alt_count, after.pending, after.cursor, jpeg, last.marker, last.path,
                    Bool01(path == album::HoverPath::kDecode),
                    Bool01(path == album::HoverPath::kDecode), last.image_after,
                    last.crc_before, last.crc_after, state.tracker.hovers_since(),
                    Bool01(clobbered), suppressed, Bool01(before.read && after.read));
    }
  } else {
    auto &state = State();
    std::lock_guard lock(state.mutex);
    state.album = album_ptr;
    ++state.hover_calls;
    state.tracker.OnHover(decoded);
    clobbered = state.tracker.clobbered();
  }
  if (!fix) {
    return;
  }
  // KI-056 fix A. The pending flag and the new JPEG are read after the
  // original (it changes neither); the slot rule uses the fields the original
  // evaluated.
  const AlbumView now = ReadAlbumFields(read, album_ptr);
  album::RestoreInputs ri;
  ri.fix_on = true;
  ri.read = before.read && now.read;
  ri.pending = now.pending;
  ri.hover = in;
  ri.clobbered = clobbered;
  const Marker new_marker = now.read ? ReadMarker(read, now.jpeg) : Marker{};
  ri.marker_read = new_marker.read;
  ri.b0 = new_marker.b0;
  ri.b1 = new_marker.b1;
  const album::RestoreDecision decision = album::DecideRestore(ri);
  if (decision == album::RestoreDecision::kNoMarker) {
    auto &state = State();
    std::lock_guard lock(state.mutex);
    ++state.restore_no_marker;
    return;
  }
  if (decision != album::RestoreDecision::kRestore) {
    return;
  }
  const Crc crc_before = ReadCrc(read, now.pixels);
  // The game's own new-photo decode, through the hooked symbol (its override
  // passes it through while t_in_restore is set). It takes only r3; the
  // callers of the hover tail-return and do not use r3, but r3 and lr are
  // put back anyway.
  const auto saved_r3 = ctx.r3;
  const auto saved_lr = ctx.lr;
  t_in_restore = true;
  ctx.r3.u64 = album_ptr;
  sub_8263C8B0(ctx, base);
  t_in_restore = false;
  ctx.r3 = saved_r3;
  ctx.lr = saved_lr;
  bool pixels_ok = true;
  const uint32_t pixels_after = read.U32(album_ptr + album::kPixels, pixels_ok);
  const Crc crc_after = pixels_ok ? ReadCrc(read, pixels_after) : Crc{};
  auto &state = State();
  std::lock_guard lock(state.mutex);
  state.tracker.OnRestore();
  auto &last = state.restore;
  last.seen = true;
  last.slot = slot;
  last.crc_before = crc_before.Text();
  last.crc_after = crc_after.Text();
  last.at_ms = NowMs();
  uint32_t suppressed = 0;
  if (state.restore_pacer.Allow(NowMs(), suppressed)) {
    MCLA_APP_INFO("MCLA_ALBUM v=1 fn=restore album={:08X} lr={:08X} slot={} pending1170={} "
                  "jpeg1148={:08X} marker={} crc_before={} crc_after={} restores={} "
                  "snapshot={} suppressed={}",
                  album_ptr, lr, slot, now.pending, now.jpeg, new_marker.Text(),
                  last.crc_before, last.crc_after, state.tracker.restores(),
                  state.tracker.snapshots(), suppressed);
  }
}

// Slot thumbnail load: r3 = album, r4 = slot (probe only; pass-through). It
// decodes the slot JPEG and re-runs sub_8263C8B0 itself when +1170 is set.
REX_EXTERN(sub_8263E1E8) {
  if (!ProbeOn()) {
    __imp__sub_8263E1E8(ctx, base);
    return;
  }
  const uint32_t album_ptr = ctx.r3.u32;
  const uint32_t slot = ctx.r4.u32;
  const uint32_t lr = static_cast<uint32_t>(ctx.lr);
  const Reader read = mcla::guest::HookReader(base);
  const AlbumView before = ReadAlbumFields(read, album_ptr);
  const Crc crc_before = before.read ? ReadCrc(read, before.pixels) : Crc{};
  __imp__sub_8263E1E8(ctx, base);
  const AlbumView after = ReadAlbumFields(read, album_ptr);
  const Crc crc_after = after.read ? ReadCrc(read, after.pixels) : Crc{};
  auto &state = State();
  std::lock_guard lock(state.mutex);
  state.album = album_ptr;
  ++state.thumb_calls;
  state.tracker.OnThumbLoad();
  auto &last = state.thumb;
  last.seen = true;
  last.slot = slot;
  last.pending = before.pending;
  last.crc_before = crc_before.Text();
  last.crc_after = crc_after.Text();
  last.at_ms = NowMs();
  uint32_t suppressed = 0;
  if (state.thumb_pacer.Allow(NowMs(), suppressed)) {
    MCLA_APP_INFO("MCLA_ALBUM v=1 fn=thumb_load album={:08X} lr={:08X} slot={} pending1170={} "
                  "cursor1180={} mask860={:08X} crc4k_before={} crc4k_after={} snapshot={} "
                  "suppressed={} complete={}",
                  album_ptr, lr, slot, before.pending, before.cursor, before.mask,
                  last.crc_before, last.crc_after, state.tracker.snapshots(), suppressed,
                  Bool01(before.read && after.read));
  }
}

// Save into a slot: r3 = album, r4 = slot.
REX_EXTERN(sub_8263DCF8) {
  if (!ProbeOn()) {
    __imp__sub_8263DCF8(ctx, base);
    return;
  }
  const uint32_t album_ptr = ctx.r3.u32;
  const uint32_t slot = ctx.r4.u32;
  const uint32_t lr = static_cast<uint32_t>(ctx.lr);
  const Reader read = mcla::guest::HookReader(base);
  const AlbumView before = ReadAlbum(read, album_ptr, false);
  __imp__sub_8263DCF8(ctx, base);
  const AlbumView after = ReadAlbum(read, album_ptr, false);
  bool ok = true;
  const uint32_t slot_texture =
      slot < 64 ? read.U32(album_ptr + album::kSlotTextureTable + slot * 16, ok) : 0;
  auto &state = State();
  std::lock_guard lock(state.mutex);
  state.album = album_ptr;
  ++state.save_calls;
  state.tracker.OnSave();
  auto &last = state.save;
  last.seen = true;
  last.slot = slot;
  last.mask_before = before.mask;
  last.mask_after = after.mask;
  last.image_after = after.image.Text();
  last.crc_before = before.crc.Text();
  last.crc_after = after.crc.Text();
  last.slot_texture = slot_texture;
  last.at_ms = NowMs();
  uint32_t suppressed = 0;
  if (state.save_pacer.Allow(NowMs(), suppressed)) {
    MCLA_APP_INFO("MCLA_ALBUM v=1 fn=save album={:08X} lr={:08X} slot={} jpeg1148={:08X} "
                  "marker={} mask860_before={:08X} mask860_after={:08X} image_after={} "
                  "crc4k_before={} crc4k_after={} slot_tex={:08X} snapshot={} "
                  "hovers_since_snapshot={} suppressed={} complete={}",
                  album_ptr, lr, slot, before.jpeg, before.marker.Text(), before.mask,
                  after.mask, last.image_after, last.crc_before, last.crc_after, slot_texture,
                  state.tracker.snapshots(), state.tracker.hovers_since(), suppressed,
                  Bool01(before.read && after.read && ok));
  }
}

// --- F10 state block ---------------------------------------------------------------
namespace {

std::string Quoted(const std::string &text) {
  return "\"" + text + "\"";
}

std::string HexQ(uint32_t value) {
  return Quoted(album::Hex8(value));
}

}  // namespace

std::string MclaAlbumStateJson(rex::memory::Memory *memory, uint8_t *base) {
  std::ostringstream out;
  out << "{\"probe\": " << (ProbeOn() ? "true" : "false");
  if (!ProbeOn() && !FixOn()) {
    out << "}";
    return out.str();
  }
  auto &state = State();
  std::lock_guard lock(state.mutex);
  const uint64_t now = NowMs();
  auto age = [now](uint64_t at) { return at && now >= at ? static_cast<int64_t>(now - at) : -1; };
  if (FixOn()) {
    album::RestoreStats stats;
    stats.enabled = true;
    stats.restores = state.tracker.restores();
    stats.no_marker = state.restore_no_marker;
    stats.clobbered = state.tracker.clobbered();
    stats.last_seen = state.restore.seen;
    stats.last_slot = state.restore.slot;
    stats.last_crc_before = state.restore.crc_before;
    stats.last_crc_after = state.restore.crc_after;
    stats.last_age_ms = age(state.restore.at_ms);
    out << ", \"preview_restore\": " << album::RestoreJson(stats);
  }
  if (!ProbeOn()) {
    out << "}";
    return out.str();
  }
  out << ", \"album\": " << HexQ(state.album);
  if (state.album && memory && base) {
    const Reader read(memory, base);
    const AlbumView v = ReadAlbum(read, state.album, true);
    bool ok = v.read;
    const uint32_t tick = state.album + album::kTickBase;
    const uint32_t live_state = read.U32(tick + album::kTickState, ok);
    const uint32_t live_sub = read.U32(tick + album::kTickSubState, ok);
    const uint32_t live_frames = read.U32(tick + album::kTickFrames, ok);
    out << ", \"live\": {\"state132\": " << live_state << ", \"substate136\": " << live_sub
        << ", \"frames732\": " << live_frames << ", \"mask860\": " << HexQ(v.mask)
        << ", \"alt1225\": " << v.alt_mode << ", \"count1208\": " << v.alt_count
        << ", \"pending1170\": " << v.pending << ", \"cursor1180\": " << v.cursor
        << ", \"jpeg1148\": " << HexQ(v.jpeg) << ", \"marker\": " << Quoted(v.marker.Text())
        << ", \"image1140\": " << HexQ(v.image_ptr) << ", \"image\": " << Quoted(v.image.Text())
        << ", \"pixels1144\": " << HexQ(v.pixels)
        << ", \"data_is_pixels\": " << (v.image.read && v.image.data == v.pixels ? "true" : "false")
        << ", \"crc4k\": " << Quoted(v.crc.Text()) << ", \"tex1132\": " << HexQ(v.texture_ptr)
        << ", \"material1136\": " << HexQ(v.material)
        << ", \"fetch\": " << Quoted(v.texture.FetchText()) << ", \"tex_words\": "
        << Quoted(v.texture.read ? album::WordsText(v.texture.words.data(), album::kTextureWords)
                                 : std::string("n/a"))
        << ", \"complete\": " << (ok ? "true" : "false") << "}";
  }
  if (state.tick.seen) {
    out << ", \"last_tick\": {\"from\": \"" << state.tick.from_state << "/" << state.tick.from_sub
        << "\", \"to\": \"" << state.tick.to_state << "/" << state.tick.to_sub
        << "\", \"frames732\": " << state.tick.frames << ", \"age_ms\": " << age(state.tick.at_ms)
        << "}";
  }
  if (state.new_photo.seen) {
    const auto &n = state.new_photo;
    out << ", \"last_new_photo\": {\"path\": " << Quoted(n.path) << ", \"marker\": "
        << Quoted(n.marker) << ", \"jpeg1148\": " << HexQ(n.jpeg)
        << ", \"image_before\": " << Quoted(n.image_before)
        << ", \"image_after\": " << Quoted(n.image_after) << ", \"pixels1144\": " << HexQ(n.pixels)
        << ", \"data_is_pixels\": " << (n.data_is_pixels ? "true" : "false")
        << ", \"crc4k_before\": " << Quoted(n.crc_before)
        << ", \"crc4k_after\": " << Quoted(n.crc_after)
        << ", \"crc_changed\": " << (n.crc_changed ? "true" : "false")
        << ", \"tex1132_before\": " << HexQ(n.texture_before)
        << ", \"tex1132_after\": " << HexQ(n.texture_after)
        << ", \"fetch_before\": " << Quoted(n.fetch_before)
        << ", \"fetch_after\": " << Quoted(n.fetch_after) << ", \"recreate92\": "
        << Quoted(n.recreate) << ", \"tex_words\": " << Quoted(n.texture_words)
        << ", \"age_ms\": " << age(n.at_ms) << "}";
  }
  if (state.hover.seen) {
    const auto &h = state.hover;
    out << ", \"last_hover\": {\"slot\": " << h.slot << ", \"occupied\": "
        << (h.occupied ? "true" : "false") << ", \"mask860\": " << HexQ(h.mask)
        << ", \"alt1225\": " << h.alt_mode << ", \"pending1170\": " << h.pending
        << ", \"cursor1180\": " << h.cursor << ", \"path\": " << Quoted(h.path)
        << ", \"jpeg\": " << HexQ(h.jpeg) << ", \"marker\": " << Quoted(h.marker)
        << ", \"image_after\": " << Quoted(h.image_after)
        << ", \"crc4k_before\": " << Quoted(h.crc_before)
        << ", \"crc4k_after\": " << Quoted(h.crc_after) << ", \"age_ms\": " << age(h.at_ms) << "}";
  }
  if (state.save.seen) {
    const auto &s = state.save;
    out << ", \"last_save\": {\"slot\": " << s.slot << ", \"mask860_before\": "
        << HexQ(s.mask_before) << ", \"mask860_after\": " << HexQ(s.mask_after)
        << ", \"image_after\": " << Quoted(s.image_after)
        << ", \"crc4k_before\": " << Quoted(s.crc_before)
        << ", \"crc4k_after\": " << Quoted(s.crc_after) << ", \"slot_tex\": "
        << HexQ(s.slot_texture) << ", \"age_ms\": " << age(s.at_ms) << "}";
  }
  if (state.thumb.seen) {
    const auto &t = state.thumb;
    out << ", \"last_thumb_load\": {\"slot\": " << t.slot << ", \"pending1170\": " << t.pending
        << ", \"crc4k_before\": " << Quoted(t.crc_before)
        << ", \"crc4k_after\": " << Quoted(t.crc_after) << ", \"age_ms\": " << age(t.at_ms)
        << "}";
  }
  out << ", \"calls\": {\"tick_lines\": " << state.ticks_logged
      << ", \"new_photo\": " << state.new_photo_calls << ", \"hover\": " << state.hover_calls
      << ", \"save\": " << state.save_calls << ", \"thumb_load\": " << state.thumb_calls << "}"
      << ", \"tracker\": {\"snapshots\": " << state.tracker.snapshots()
      << ", \"decoded\": " << (state.tracker.decoded() ? "true" : "false")
      << ", \"hovers_since\": " << state.tracker.hovers_since()
      << ", \"saved\": " << (state.tracker.saved() ? "true" : "false")
      << ", \"clobbered\": " << (state.tracker.clobbered() ? "true" : "false")
      << ", \"restores\": " << state.tracker.restores() << "}}";
  return out.str();
}
