# KI-047 / M11-007b: who reads the per-frame 256x256 resolve 039CA000 (PC) / 03702000 (Deck)? (static RE, 2026-11-07)

Read-only RE by an Opus subagent on 2026-11-07 (generated code, retail XEX strings/vtables expanded in temp only, SDK command processor); accepted by the primary agent.

Read-only static RE by an Opus subagent. Sources: `generated/default/mcla_recomp.*.cpp` (ReXGlue output),
the retail `default.xex` decrypted and expanded in memory (basic compression, retail AES key; image base
0x82000000, 10,354,688 bytes; used only to read strings and vtables; the image is not kept in the
repository), SDK worktree `mcla/m11-steady2` (`bd9314f`), `src/graphics/command_processor.cpp` and
`src/graphics/d3d12/command_processor.cpp`. Nothing was built, launched or changed.

## 0. Result in short

- **No CPU consumer of 039CA000 was found statically, and static RE cannot rule one in or out.**
  The address is a heap/pool allocation (render-target memory pools), so it never appears as a constant.
  What *was* established is how every game CPU read of a GPU-written texture has to work, and
  that gives a cheap runtime test and a design that does not depend on naming the consumer:
  1. **Every CPU texture lock goes through one function.** D3D `LockRect` / `LockBox` (`sub_82410518`,
     `sub_824105A8`) call `sub_82410440`, which calls `sub_82421CA0`, the XDK "lock resource" core.
     Vertex- and index-buffer locks reach the same core through `sub_82422320` / `sub_82422430`.
     The only game code that reads a texture's base address directly is the release/free path
     (`sub_82177CB0`, `sub_82184458`). No game function turns a fetch-constant base into a pointer
     to read pixels from.
  2. **The game's own synchronisation is the D3D resource fence.** `D3DDevice_Resolve`
     (`sub_82420BA8`) writes the destination texture's `Fence` (+8) and `ReadFence` (+12) with
     `device+10908`, the next fence ID (`mcla_recomp.27.cpp:43564-43576`, `43753-43763`). It also
     emits the XDK post-resolve coherency sequence: COHER_SIZE/BASE_HOST over the destination,
     then `WAIT_REG_MEM` (`0xC0043C00`) on COHER_STATUS_HOST (`27.cpp` around 43590-43640).
     `sub_82421CA0` then blocks in `sub_82411E98` (BlockOnFence core, `27.cpp:4754`) until
     `*(device+10896)` (the GPU-written fence word) reaches that fence. If the fence has not been
     issued yet, it calls `sub_82412710`, which kicks the segment so that the fence gets inserted.
  3. **On the host, that fence is written too early.** The GPU writes the fence word with
     `EVENT_WRITE_SHD` (`0xC0025800`, built by `sub_82411CD0`, `27.cpp:4483`; issued at every
     command-segment kick by `sub_82412588`). The SDK writes it to guest RAM as soon as the CP
     *parses* the packet (`CommandProcessor::ExecutePacketType3_EVENT_WRITE_SHD`,
     `src/graphics/command_processor.cpp:1489-1511`, a `memory::store` with no reference to host
     GPU completion). Until now this was harmless only because rule c makes the resolve's
     read-back synchronous: the CP waits at draw 26 before it reaches the next fence. Once a key is
     exempt, any guest `LockRect` on it returns as soon as the CP has passed the next fence. At that
     point the guest RAM holds the stale `some` slot (0-3 frames old) or nothing new at all.
     **This is the "fence signalled before the resolve copy lands" pattern**, a host contract
     violation. It is real whoever the consumer is.
- **Bonus (KI-047 livery consumer named):** `sub_8236F4F8` (`mcla_recomp.20.cpp:44867`) is the
  vinyl/livery atlas builder. It logs "Starting vinyl compression, if you crash here you ran out
  of memory..." (string at 0x82049420) and "Vinyl compression finished" (0x82049400). It calls
  `BlockOnResource` (`sub_82412178`) on both source render targets, locks each mip through the rage
  `LockRect` virtual (vtable +76; rage `grcRenderTargetXenon::LockRect` = `sub_82182A08`, texture =
  `sub_82183398`), and compresses with `sub_824336C0`, source fmt `0x18280086` LIN_A8R8G8B8 and
  destination `0x1A200054` LIN_DXT4_5. Its render targets are the 1024x1024 / 512x512 colour
  targets made in `sub_82370C38`. **So the KI-047 consumer also synchronises only through the
  resource fence plus the lock.** Both fixes below therefore cover the livery too.

## 1. Resolve side (where 039CA000 is written)

- `sub_82420BA8` = `D3DDevice_Resolve(device r3, flags r4, srcRect r5, destTexture r6->r18,
  destPoint r7, level, slice, clear colour f1, ..., params r10)`. It writes the copy registers with
  `SET_CONSTANT` (for example `0xC0012D01` + `0x00040319` = RB_COPY_DEST_BASE,
  `27.cpp:42670-42690`), the coherency/WAIT_REG_MEM sequence, and the fence stamps above. Its game
  callers are `sub_82178F70`, `sub_8217B430`, `sub_8217B7B0` and rage `grcRenderTargetXenon`
  virtuals `sub_82183C68` / `sub_82184588` / `sub_82184990` (RT vtable 0x8200A524, slots
  +120/+100/+104). Most RT resolves therefore come through
  `grcTextureFactory::UnlockRenderTarget` (factory `*(0x82839D70)`, vtable +64) and have no static
  call site that names the target.
- **256x256 32bpp colour render targets created through `grcTextureFactory::CreateRenderTarget`**
  (factory vtable +56; full scan of the call sites):
  - `FastCollector` and `FastCollectorTemp` (shadow collector, `sub_82310E40`). `FastCollector`
    shares a 0x40000-byte `FastCollectorPool` (heap 1) with `FastCollectorNight` 512x512 8bpp
    (heap 0), i.e. the two alias each other.
  - `Road Reflection Color` (`sub_82314218`).
  - `__nav_arrow` (GPS arrow, `sub_821F7920`).
  - `sidekickColorRT` (`sub_821F9DC8`).
  - `uiOverlay` (`sub_821FDED8`).

  `ShadowImpostor` is 256x256 but 8bpp, and `ImpostorColor` is 256x128. Users of `FastCollector`
  (`sub_823115E8`, `sub_823120C8`) only lock/unlock it as a render target (factory +60/+64); they
  take no CPU lock. Which of these is 039CA000 cannot be settled statically. Render-target memory
  is pooled (`grcRenderTargetMemPool`, "hack rendertarget mem pool"), so the key may also alias
  another target. The probe below names it.

## 2. Consumer side: candidates checked

- All functions that call the rage lock/unlock virtuals (+76/+80): 19 functions. Apart from vinyl
  compression they are UI/Bink/particles/damage: `sub_822EF0D8` rain particles,
  `sub_823614F0` damage textures, `sub_82468C28` Bink, `sub_826xxxxx` UI, `sub_827xxxxx` library
  code, `sub_82302FA0` a creation-time lock. None is an obvious per-frame reader of a 256x256
  RGBA target.
- Direct calls to BlockOnFence/BlockOnResource from game code: only `sub_82177CB0` (texture
  release), `sub_8236F4F8` (vinyl) and `sub_82178B20` (front-buffer/photo capture). No game code
  reads `device+10896/10908` directly.
- Hence: **if a CPU reader of 039CA000 exists, it can only read through `sub_82421CA0`** (or through
  D3D-internal code). One logged run settles it.

## 3. When the CPU would read relative to the resolve

On hardware a `LockRect` of the resolved target in the same frame blocks until the GPU passes the
first fence issued after the resolve. That fence is issued at the next segment kick (`sub_82412588`
calls `sub_82411CD0`). The resolve's own WAIT_REG_MEM coherency step guarantees the bytes are in
RAM by then. On the host the guest waits only for the CP to *parse* that `EVENT_WRITE_SHD`. With
rule c the CP had already waited about 6 ms at draw 26, so the data was valid. Exempt, it is not.
A reader that locks in the same frame, or later, gets stale data. That matches "flicker even with
a <=1-frame-old copy, and fast delivery often skipped".

## 4. Recommended host design

**Step 0, decisive probe (one owner run, no behaviour change).** Add
`REX_EXTERN(sub_82421CA0)` in a host probe source (pattern: `src/mcla_ki026_probe.cpp`). It
logs and then calls `__imp__sub_82421CA0`. Fields: `r3` resource, `r4` flags, `r7` texture base
phys (`(tex+32)&~0xFFF`), `r8` mip base, `r9` locked start phys, `r10` locked length (verified from
the body: offset = `r9 - r7`, CPU cache invalidate over `[r9, r9+r10)`; flags `0x1010` select
`ReadFence`, `0x12` skip the flush), `lr`, guest thread and the SDK frame/draw counters. Budget the
lines, and always log any lock that overlaps a read-back key (ideally through a new SDK export
`ReadbackKeyInfo(phys, len)`: exempt yes/no, last resolve frame, slot age). Also log
`sub_82412178` / `sub_82412158` callers.
- If a lock overlapping 039CA000 shows up: the consumer is `lr`'s function. Go to step 1.
- If no lock ever overlaps 039CA000 in a flicker run: the CPU-reader hypothesis is refuted for
  game code, because every CPU texture read funnels through this function. The flicker then needs
  another cause, for example the +16 % frame rate (KI-041-like) or something GPU-side; recheck the
  texture-cache/upload logs.

**Step 1, preferred fix: lazy completion at the lock (consumer hook).**
- Keep the steady key exempt: no CP wait at draw 26.
- Wrap the lock: `REX_EXTERN(sub_82421CA0)` first lets the guest's own fence wait run, so that
  the CP has really processed the resolve. Either wrap `sub_82411E98` (call the original, then act
  when `r6` = resource is non-null), or reimplement the cheap fence test before calling the
  original.
- Then call a new SDK export, `ReadbackSyncEnsureRange(phys = r9 & 0x1FFFFFFF, len = r10)`. For each
  read-back key with an in-flight copy that overlaps the range:
  - if its submission is still open, ask the CP to end it (atomic flag checked between packets);
  - wait on that submission's fence (event, not spin);
  - memcpy that slot into guest RAM (`TranslatePhysical`, as today) and mark it delivered.
- Then call `__imp__sub_82421CA0`.
- The wait moves to the reading guest thread and happens only when it really reads. By then the
  GPU is usually done (the consumer runs later in the frame), so the expected cost is about 0 to
  the remaining GPU time, against 6 ms of CP stall every frame.
- The same hook makes the vinyl builder correct without the rule a/b/c forced waits. That
  extension (drop forced waits for every key) is for later, after the A/B.
- Threading: the CP owns the submissions, so `EnsureRange` needs a mutex-protected pending-copy
  table: key -> {slot, submission, addr, len, delivered}.

**Step 2, alternative / root-cause variant: defer fence publication on the CP.**
- In `ExecutePacketType3_EVENT_WRITE_SHD` (and the MEM_WRITE/interrupt signals), if any exempt
  copy recorded earlier is not yet landed, queue `{address, value}` in order instead of storing it.
- Between packets, poll `GetCompletedValue`. When the copies have completed, memcpy their slots
  and publish the queued fence words in order.
- When the CP would idle (ring empty) with deferred fences, it must `EndSubmission` and wait;
  otherwise the guest deadlocks.
- This is the exact hardware contract and covers any fence-synchronised reader, including ones not
  using `LockRect`.
- Risk: the guest render thread's ring-space and frame-throttle waits (`sub_82411E98` /
  `sub_82412F98`, the same fence word) can then wait for real GPU progress, giving back part of the
  +6 fps. It changes pacing globally.
- Use it only if step 1's hook cannot cover the reader, or as a measured arm.

**Also possibly explains KI-038:** before l55 this key was never forced. If step 0 finds a lock on
it, the pre-l55 billboard/sign placeholders (KI-038) would have the same cause: a lock served the
stale `some` slot. That is a hypothesis to test with the probe, not a finding.

## 5. Function index

| guest fn | role (evidence) |
|---|---|
| `sub_82420BA8` (27.cpp:41623) | D3DDevice_Resolve: writes RB_COPY_* via SET_CONSTANT, coherency WAIT_REG_MEM, stamps dest Fence/ReadFence = device+10908 |
| `sub_82411CD0` (27.cpp:4483) | InsertFence: EVENT_WRITE_SHD 0xC0025800 to the fence word `*(device+10896)`; next fence += 2 |
| `sub_82412588` (27.cpp:5866) | segment kick, inserts a fence (called from `sub_82412710`) |
| `sub_82411E98` (27.cpp:4754) | BlockOnFence core: pending while (next - fence) < (next - completed); kicks if fence == next; spins in `sub_82412F98` |
| `sub_82412158` / `sub_82412178` (27.cpp:5223/5244) | D3DDevice_BlockOnFence / BlockOnResource (resource+8) |
| `sub_82410518`, `sub_824105A8` -> `sub_82410440` (27.cpp:709) -> `sub_82421CA0` (27.cpp:44257) | LockRect/LockBox -> lock core: fence wait, coherency packet, cache invalidate over [r9, r9+r10), lock count +256 |
| `sub_82182A08` / `sub_82183398` | rage RT / texture LockRect (vtable 0x8200A524 / 0x8200A45C, slot +76; unlock +80) |
| `sub_8236F4F8` (20.cpp:44867) | vinyl (livery) DXT compression, the KI-047 CPU consumer |
| `sub_82310E40` (16.cpp:52535) | creates FastCollector* 256x256 targets and FastCollectorPool |
| host `CommandProcessor::ExecutePacketType3_EVENT_WRITE_SHD` (`src/graphics/command_processor.cpp:1489`) | stores the fence value at parse time, with no host GPU completion |
