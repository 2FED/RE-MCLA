# KI-073 (photo album: no delete, "Publish Album" needs Live): album storage and publish path, export design

Read-only RE/design by an Opus subagent on 2026-11-06 (generated code, SDK XAM, an archived PHOTO_ALBUM package); accepted by the primary agent as the backlog design for KI-073 (owner: not necessarily this milestone).

Read-only analysis, 2026-11-06, Opus subagent. Sources: generated guest code
`generated/default/mcla_recomp.49.cpp` (album cluster `sub_8263C0E8`..`sub_82640130`),
`mcla_recomp.34.cpp` / `mcla_recomp.29.cpp` (online-state helpers), the import list in
`generated/default/mcla_init.{h,cpp}`, strings and tables read in memory from the admitted XEX
(same in-RAM decrypt method as the KI-026/KI-037 analyses; nothing derived from the image was
persisted beyond the identifiers quoted here), SDK worktree `l71-sdk-wt` (`d08813b`), host
`src/mcla_album_probe.{h,cpp}`. One archived test-fixture album file (`private/_archive/misc/ki047-ab/
.../user-after/.../PHOTO_ALBUM`) was parsed for its header, directory and JPEG markers only (sizes,
CRCs, dimensions); no image was decoded or copied. Nothing was built or launched. Spark: not used
(the work needs the local generated corpus and private image reads that must not leave the PC).

## 1. Where the photos are

### 1.1 One blob, 16 slots, in guest memory and on disk

The album object (`mcUIPhotoAlbum`) owns one buffer descriptor `{ptr +428, size +432}`:
`+432 = 0x3200CC` (ctor `sub_8263F330`, 49.cpp:~20548), allocated in `sub_8263D2A8`
(`sub_82183748(0x3200CC, 4096)`, 49.cpp:~15950, stored at `+428`). 0x3200CC = 204 + 16 x 0x32000.

Layout (big-endian), confirmed by the code and by the on-disk file:

| Offset in blob | Content | Writer / reader |
|---|---|---|
| 0x00 u32 | photo count, mirrors album `+856` | load: `sub_8263E4F0` LOAD_COMPLETE branch (49.cpp:18502-18515), PostLoad `sub_8263E860` (49.cpp:~18855) |
| 0x04 u32 | occupied-slot mask, mirrors `+860` | same |
| 0x08 u32 | per-slot flag mask, mirrors `+868` (set or cleared per slot at save from `sub_82178848()`; meaning unknown) | same; save `sub_8263DCF8` (49.cpp:17340) |
| 0x0C + 12*slot | directory entry `{u32 slot, u32 jpeg_size, u32 crc32}` | `sub_8263C248` (49.cpp:13098): `+12 = slot`, `+16 = size`, `+20 = sub_821D58D8(0, jpeg, size)` |
| 0xCC + slot*0x32000 | JPEG bytes, slot area zero-filled first (`sub_823D9890(dst, 0, 0x32000)`), then `memcpy(dst, jpeg, size)` (`sub_823DA950`) | `sub_8263C248`; readers `sub_8263C1C0` (slot pointer), hover `sub_8263C9C8`, thumbnail `sub_8263E1E8` |

- The checksum is standard CRC-32 (zlib): checked on slot 0 of the archived file, the stored CRC
  equals `zlib.crc32(jpeg)` exactly. The probe's `album::Crc32` is the same polynomial.
- Archived 1440p-session file (`.../PHOTO_ALBUM`, 3,277,004 bytes = exactly the blob size):
  header `count=13 mask=0x1FFF flags868=0x1FFF`; slots 0-12 each have `SOI FFD8` at the slot start,
  `EOI FFD9` exactly at `size-2`, a CRC match, zero tail padding, and **SOF dimensions 1280x720**;
  sizes 15-170 KB. Slots 13-15 are all-zero entries.
- So the photo store is **one separate savegame content package**, not part of `mc4.sav`:
  `<user-data>/user/<XUID>/545407F8/00000001/PHOTO_ALBUM/PHOTO_ALBUM` (plus its
  `Headers/00000001/PHOTO_ALBUM.header`; layout as in the KI-068 evidence). The file is a raw dump of
  the blob. The content name string is `PHOTO_ALBUM` (`0x820905A8`).
- Header writer on save: not traced (the save item serializes the `{+428,+432}` buffer; the file
  header agrees with `+856/+860/+868`). Not needed for export.

### 1.2 Stored quality and resolution

- The stored JPEG is the **full photo, not a thumbnail**. It is the JPEG produced by the capture
  state machine `sub_8263CB78`: encode `+1140` (the captured image) at quality 85 (`sub_8263C728`),
  re-encode lower (`sub_8263C7E0`/`sub_8263C5C8`) until the size fits under about 0x31FFF; result at
  `+1148`, size at `+864` (KI-037 analysis). Save copies `+1148`/`+864` verbatim into the slot
  (`sub_8263DCF8` -> `sub_8263C248(album, slot, r5=*(+1148), r6=*(+864))`, 49.cpp:17083-17091).
- Dimensions = the `+1140` image at encode time. All 13 archived photos are 1280x720; the l71 probe
  saw 1280x720 slot decodes. (The M10 note that `sub_8263C638` may resize `+1140` to the screen
  dimensions at 1080p is not contradicted, but no stored 960x540 photo has been seen.)
- Grid thumbnails are **not stored**: `sub_8263E1E8` / `sub_8263DCF8` decode the slot JPEG and build a
  64x32 texture (`sub_8218EE38(64,32,...)`) at load/save time.
- Hover decode source pointer: `*(album+428) + slot*0x32000 + 204` (`sub_8263C9C8`, 49.cpp:~14317;
  `sub_8263C1C0`, 49.cpp:13014).

## 2. The "Publish Album" path

### 2.1 Script surface

Only the label `PA_publish` (`0x8201D49C`) is in the XEX; the visible text lives in game data. The
album is driven by script calls dispatched in **`sub_8263FA10`** (49.cpp:21415): the function index is
`s16 *(r5+2)` (switch, 22 cases); the album object is looked up from the `PhotoAlbumScene` object
(`sub_821FA230` + `sub_823DEDD8` RTTI cast). The function-name table at `0x828260D8` maps the cases:

| idx | name | handler (49.cpp) |
|---|---|---|
| 0 | PhotoSlotChosen | stores chosen slot `+1160` |
| 1 | SetPreviewPicture | `sub_8263DFC0` |
| 2 | SwapNewJpegBuffers | `memcpy(+1152, +1148, 0x32000)` (22372) |
| 9 | SavePhotoAlbum | `sub_8263DCF8(album, +1160)` if `*(0x8290621C)` (album loaded), else posts `CREATE_NEW_PHOTO_ALBUM` (21980) |
| 10 / 11 | SaveSnapshot / SetSavingSnapshot | `+1170 = 1` |
| **14** | **UploadPhoto** (= the publish action; it uploads every occupied slot) | **loc_8263FE88 (22087)** |
| 15 | DownloadAllPhotos | `sub_8263DBE8` |
| 16 | ToggleMode | local <-> online view (`+1225`) (22292) |
| 17 | DeletePhoto | **online mode only** (`+1225 != 0`): `sub_8263C530(album, cursor)`, event 28 deletingRecordsStart = deletes the *published* record, never a local slot (22220) |
| 18 | CheckAvailSpace | `sub_82200818(mgr, "PHOTO_ALBUM")` (22258) |
| 19 | CancelUpload | `sub_8263F9C0` |
| 20 / 21 | PostLoad / PostSave | `sub_8263E860` (thumbnail rebuild from the blob) |

Script events (table `0x82826030`): 13-15 uploadPhotoStart/Success/Fail, **16-18
uploadAlbumStart/Success/Fail**, 19-21 downloadPhoto*, 22-24 downloadAllPhotos*, 25-27 downloadRecords*,
28-30 deletingRecords*, 31-33 overwriteRecords*, 34 maxPhotosReached, 35 chooseSlot, 36
noAlbumSnapshotCreated, 37 noAlbum, 38 savingDisabled, **39 isOffline**, 40 localMode.

### 2.2 UploadPhoto (idx 14), step by step

1. Gate: `sub_824E70F0(gamer+16)` = bit 0x40 of byte `+92` of the local gamer's online-presence
   record (34.cpp:61491). If clear, **return 0 immediately: no event, no UI, nothing**. Then require
   `+856 > 0` and local mode (`+1225 == 0`).
2. `sub_821E6800("UI_PHOTO_UPLOAD_MASTER")`, event 16 uploadAlbumStart, `sub_8263DB38` (event 25
   downloadRecordsStart: fetch the player's existing published records via `sub_824F6EA0`), then for
   each occupied slot `sub_8263EB00(album, slot)` pushes the slot onto the upload queue
   (`+1228` array, `+1232` count).
3. The tick `sub_8263EB58` (49.cpp:19218, state `+596`) pops the queue and calls `sub_8263D878(album,
   slot)` (49.cpp:16402): stores `{slot, size+1}` and the slot CRC in an album-side record
   (`+872 + slot*16`, `+880 + slot*16`), compares a 16-byte hash with the downloaded record and, if
   the photo is already published, skips it; otherwise fires event 13 and uploads the slot's JPEG
   (`sub_8263C1F0` slot pointer) through `sub_824F7468` / `sub_824F7308` on the records service
   object `*(0x829061D4)` under the name `PhotoAlbum` (`0x820253BC`).
4. Per-photo result in the tick: success -> event 14 + a stat/achievement update `aNPP`
   (`0x820907EC`, `vfunc48(...,4,1)`); when the queue is empty -> **event 17 uploadAlbumSuccess**
   (49.cpp:19391). Failure -> event 15, or **event 39 isOffline** when the online bit is clear
   (49.cpp:19523).

**Transport.** The game imports no `XStorage*`, `XHttp*`, `XOnline*` or `XUserWriteProfileSettings`
(full import list in `mcla_init.h`). Online services are Rockstar's own records service over raw
sockets: `NetDll_socket/connect/send/recv/select`, `NetDll_XNetServerToInAddr` /
`XNetRegisterKey` / `XNetConnect` (XLSP-style title server), `XNetGetConnectStatus`,
`XNetGetTitleXnAddr`, `XNetQos*`, plus `XamUserGetSigninState`, `XamUserCheckPrivilege`,
`XamShowSigninUI`, `XNetLogonGetTitleID`. That server no longer exists.

### 2.3 What happens offline in MCLA-R

- The online bit is computed in `sub_824E7250` (34.cpp:61722): bit 0x40 = `sub_824E6D08()` =
  network-up check (`sub_825019B0`) AND `XamUserGetSigninState(...) == 2` (`sub_824575F8` in
  29.cpp wraps `__imp__XamUserGetSigninState`).
- SDK (`l71-sdk-wt`): `XamUserGetSigninState` returns the cvar `xam_user_signin_state`, default **1**
  (signed in locally) (`src/kernel/xam/xam_user.cpp:39`, `:489`); `XamUserCheckPrivilege` grants
  nothing (`*out = 0`, `:871-893`); `NetDll_XNetGetConnectStatus` returns LOST and
  `NetDll_XNetServerToInAddr` returns WSAENETUNREACH with an `[OFFLINE]` WARN
  (`src/kernel/xam/xam_net.cpp:566-600`); `XNetLogonGetTitleID` returns 0 (`xam_misc.cpp:19`).
- Therefore **"Publish" offline is a silent no-op in native code**: `UploadPhoto` returns before any
  event or network call. Whatever the owner sees in the menu (option visible, nothing happens, or a
  script-side message) is the script layer; the native side never shows sign-in UI on this path.
  With `xam_user_signin_state=2` the gate could pass (if the link check passes) and the flow would
  run into the record download and fail on the dead server (event 26/15/39) — not useful.

## 3. Does the game clear the album after publishing?

**No.** The upload path (`sub_8263FA10` idx 14, `sub_8263DB38`, `sub_8263EB00`, `sub_8263EB58`,
`sub_8263D878`) writes only upload bookkeeping (`+588/+596`, `+872..+884 + slot*16`, `+1212`,
`+1228/+1232`, `+1186`, `+1224/+1225/+1227`) and never `+860`, the blob directory or the slot data.
All writers of `+860` in the cluster:

- `sub_8263DCF8` save: `mask |= 1<<slot` (49.cpp:~17319);
- `sub_8263E4F0` LOAD_COMPLETE: `mask = blob[4]` (49.cpp:18502-18515);
- `sub_8263E860` PostLoad: byte copy of the blob header (49.cpp:~18855);
- `sub_8263F330` ctor: 0;
- **`sub_8263E4F0` `CREATE_NEW_PHOTO_ALBUM` (msg id `+1296`, string `0x8209088C`, 49.cpp:18361-18460):
  if the storage check `sub_821FEF90` and `sub_82200818(mgr,"PHOTO_ALBUM")` pass, it sets
  `+860 = 0` and `+856 = 0` and requests a save of `PHOTO_ALBUM` (`mgr->vfunc8`), sets `+1178`.**
  This is the "new album" path that `SavePhotoAlbum` posts when no album has been loaded yet
  (`*(0x8290621C) == 0`); it is not a menu "clear" in normal play.

Also, the online-mode `DownloadAllPhotos` path (`sub_8263C480` from the tick) writes downloaded
photos **into the same blob** and sets `+856` to the remote count while `+1225 != 0`; it never runs
offline. An exporter must therefore require local mode (`+1225 == 0`).

Consequence for the owner: publishing would not have given a "delete all". A host "clear album"
would have to be built separately (section 4.4).

## 4. Design

### 4.1 Recommended: export from guest memory at save time, plus "export all" on Publish

**(c) Per-save export (cheapest, always useful).** The probe already overrides `sub_8263DCF8`
(`src/mcla_album_probe.cpp:669`). After `__imp__sub_8263DCF8` returns (the slot is written,
`+860` has the bit), and independently of `ProbeOn()`:

- `blob = u32(album+428)`; `size = u32(blob + 12 + 12*slot + 4)`; `crc = u32(blob + 12 + 12*slot + 8)`;
  `data = blob + 204 + slot*0x32000`. Do **not** use `+864`/`+1148` after the call: `+1148` is reused
  (l71 package 7 saw `24D8` there after a save), while the blob copy is stable.
- Validate: `slot < 16`, `0 < size <= 0x32000`, `data[0..1] == FF D8`, `data[size-2..] == FF D9`,
  `Crc32(data,size) == crc`, and `u8(album+1225) == 0` (local mode). On any failure log one WARN
  line and skip.
- Copy the bytes out through `mcla::guest::Reader` on the guest thread (<= 200 KB), then write on a
  host worker: `<user_data_root>/photos/<XUID16>/<yyyyMMdd-HHmmss>-slotNN-<crc8>.jpg` (UTC or local
  time, decided once), via a temp name + rename. Skip if a file with the same `<crc8>` already
  exists for that profile (idempotent; an index file is optional).
- Lossless: the bytes are written verbatim; no re-encode. Quality is whatever the game stored (q<=85,
  1280x720).

**(a/b) "Export all" on Publish.** New strong override `REX_EXTERN(sub_8263FA10)`
(`src/mcla_album_export.cpp` or inside the probe TU):

- `idx = s16 at u32(r5)+2` (read via the checked reader). If the cvar is off or `idx != 14`, tail
  into `__imp__sub_8263FA10` unchanged.
- Album pointer: use the pointer remembered by the album overrides (`State().album`, set by
  `sub_8263CB78`/`sub_8263C9C8`/`sub_8263DCF8`), and verify it by calling the original lookup is not
  needed; if none is known, fall through to the original (which is a no-op offline).
- Conditions: `u8(+1225) == 0`, `u32(+856) > 0`. For every bit of `u32(+860)` (slots 0-15): same
  validation as above, copy out, then write the set to
  `<user_data_root>/photos/<XUID16>/album-<yyyyMMdd-HHmmss>/slotNN-<crc8>.jpg` plus a small
  `manifest.tsv` (slot, size, crc, dims parsed from SOF). One INFO line `MCLA_ALBUM v=1 fn=export
  reason=publish slots=N bytes=B dir=<relative>`.
- Return `ctx.r3 = 0` **without calling the original** (offline it does nothing anyway; online it
  would try the dead server). Restore nothing else: the original's only side effects are the ones we
  skip.
- UI feedback (optional, second step): fire `sub_8268EE10(0x82826134, 16, 3, -1)` then 17
  (uploadAlbumStart/Success) so the script shows its own "published" result. Unknown script
  consequences (it may expect the state-machine values `+588/+596` and the record bookkeeping);
  keep this behind a separate default-off switch and test it once. Without it the press is silent.
- Hotkey alternative: the same "export all" from the F10/diagnostic hotkey path or a launcher
  button reading the **file on disk** (section 4.2) avoids any guest hook and works even when the
  owner never opens the album.

Switches (pattern of `mcla_album_preview_restore`): `mcla_album_export` (bool, init-only, default
off; enables both the save-time and publish-time export), dev-override key, listed in
`kNeverWrittenCvars` until the owner accepts it. If the owner wants it on for volunteers later, it
writes only into the user-data dir.

### 4.2 Simplest fallback: extract from the PHOTO_ALBUM file (no guest interaction)

A pure function `album::ParseAlbumBlob(bytes) -> vector<{slot,size,crc,offset}>` (header, 16 entries,
same validation) used by:
- the publish/save hooks (on guest memory), and
- an offline exporter that reads `<user-data>/user/<XUID>/545407F8/00000001/PHOTO_ALBUM/PHOTO_ALBUM`
  with read+write+delete sharing (the KI-065 F10 copier pattern; skip while a content transaction
  is active or a `.rexpending`/`.rexbackup` marker exists) and writes the JPEGs. Trigger: host
  startup (exports anything new from the last session), a launcher action, or the F10 export. The
  file is written by the game's own save request at the end of `sub_8263DCF8`
  (`mgr->vfunc8("PHOTO_ALBUM", 0)`, 0x8263DEC0), so it lags the in-memory blob by one save.

This alone already satisfies "all photos to disk in good quality", with zero runtime risk, and can
be unit-tested on a synthetic blob.

### 4.3 Optional: lossless PNG of each capture

The raw capture is the 1280x720 A8R8G8B8 linear image at `+1144` (stride 5120) right before the
encode in `sub_8263C728`. A pass-through override of `sub_8263C728` could save a PNG of it (3.6 MB
read). Higher quality than the game ever stores, but it fires for every photo taken (also discarded
ones) and costs a PNG encode; only if the owner wants lossless.

### 4.4 Not recommended now: host "delete" / "clear all"

The game has no local delete. Options, all needing owner sign-off:
- **Clear all**: post the game's own `CREATE_NEW_PHOTO_ALBUM` message (it zeroes `+860/+856` and
  saves). Needs the message-dispatch call (`sub_8268B770(*(0x8286D810), 0x8209088C, 0)` as in
  `SavePhotoAlbum` idx 9 at 21980) from the album context; the blob directory and data stay but are
  unreachable (the mask is authoritative for every reader), and the next save of a slot rewrites
  its entry.
- **Delete one slot**: clear bit in `+860`, decrement `+856`, zero the directory entry, rebuild the
  slot thumbnail, then the game's save request. More state to keep consistent (`+868`, thumbnails
  `+604/+608 + slot*16`).
- **Offline file edit** (game closed): zero header mask/count and directory in `PHOTO_ALBUM`
  (backup first). No load-time validation was traced (`VerifyPhotoAlbum` not read), so untested.

## 5. Risks

- Hook placement: `sub_8263FA10` is the generic album script dispatcher; the override must forward
  every index except 14 exactly (r3-r5, lr untouched). Unit-check against the generated switch
  (case 14 -> `loc_8263FE88`).
- Album pointer staleness: the remembered pointer must be cleared on the album destructor
  (`sub_8263F748`) or validated (`u32(album+428) != 0` and `u32(album+432) == 0x3200CC`).
- Online-mode blob contents (downloaded photos) are excluded by `+1225 == 0`; offline this mode
  never loads anything.
- The script may grey out or hide Publish when offline, in which case idx 14 is never called; the
  hotkey/startup export (4.2) covers that.
- No sign-in UI is involved on this path (the gate is a cached presence bit, not
  `XamShowSigninUI`).
- File I/O on the guest thread: copy out on the guest thread, write on a host worker; at most
  16 x 200 KB.
- Privacy: photos stay in the user-data dir; never added to F10 packages by default.

## 6. Test plan

Unit (host, no game): `ParseAlbumBlob` on a synthetic blob (0, 1, 16 photos; bad SOI; size >
0x32000; CRC mismatch; mask/entry disagreement); file naming and CRC de-duplication; the override's
index filter (forwards 0-13, 15-21); generated-code checks: `sub_8263C248` stores at
`+12/+16/+20` of `blob + slot*12` and copies to `blob + slot*0x32000 + 204`; `sub_8263FA10` case 14 ->
`loc_8263FE88`; `sub_8263D2A8` allocates `0x3200CC`.

Owner (PC, one session, export switch on, album probe on):
1. Census first (cheap, before enabling the skip): log every `sub_8263FA10` index once per call.
   Press Publish Album offline. Expected: `idx=14` (proves the menu reaches native code), and with
   the original: no events, no network WARN.
2. With export on: press Publish. Expected `fn=export reason=publish slots=N`; a new
   `photos/<XUID>/album-<ts>/` with N JPEGs that open at 1280x720 and whose CRCs equal the
   directory CRCs; the album is unchanged in game (mask same, no dialog or a silent return).
3. Take a photo, save into a slot. Expected: one `<ts>-slotNN-<crc>.jpg` written; overwriting a slot
   writes a new file (different CRC) and keeps the old one.
4. Restart: startup/F10 extractor exports nothing new (all CRCs already present).
5. Deck: same steps 2-3 under Proton (path, sharing).
