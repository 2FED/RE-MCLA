# KI-037 analysis: first Photo Mode picture broken, save preview shows the previous photo

Read-only analysis by an Opus subagent on 2026-10-29, reviewed and accepted as
evidence by the primary agent. Nothing is physically confirmed; the conditional fix in
[M9-006-ki-037-proposal-20261029.diff](M9-006-ki-037-proposal-20261029.diff) is not
applied and depends on the owner's Test A/B below. Game strings were read in memory
only; guest addresses and minimal identifiers are cited. Spark: not used.

Task M9-006, read-only analysis, 2026-10-29. Nothing was built, launched or
changed in the repository. The game image was decrypted in memory only (the
existing job-scratch `imgscan.ps1` reader) to find strings and tables. No
image, key, asset or save was copied or opened. Spark was not used: the work
depended on in-memory game-image reads that must not leave the PC.

Sources: the pinned SDK tree `private/l34-src/third_party/rexglue-sdk`
(HEAD `a239b83`, the M8 CP commit), the generated corpus `generated/default`,
`docs/known-issues.md` (KI-037), and `docs/evidence/M8-001-timebox-20261025.md`
(l18-photo, l20, l21 history). Owner F10/log packages: none recorded around
Photo Mode were found. The default `Documents\mcla\diagnostics` root holds only
an old runtime log. Per-build shader caches exist under the owner's save root
(see 3.3).

## 1. How the game takes a photo (guest side)

All addresses are guest addresses.

**Owner object.** `mcUIPhotoAlbum` (RTTI `.?AVmcUIPhotoAlbum@@`). Its script
descriptor is at `82826134` ("photoAlbum"). The event-name table at `82826030`
starts with loadThumbnails*, loadPreview*, saveSnapshot*; event index 9 is
`snapshotTaken`. The function-name table at `828260D8` lists PhotoSlotChosen,
SetPreviewPicture, SwapNewJpegBuffers, VerifyPhotoAlbum, TakePicture,
SnapshotCancelled, ..., SaveSnapshot, SetSavingSnapshot, EnableCity,
DisableCity. Its code is the cluster `sub_8263C0E8`..`sub_82640130`
(`mcla_recomp.49.cpp`).

**Resources** (init `sub_8263D2A8`, offsets on the album object):

| Offset | Content |
|---|---|
| +1144 | physical allocation of 0x385000 bytes (1280x720x4, 4 KB aligned) |
| +1132 | texture 1280x720, format `0x18280086` (LIN_A8R8G8B8, linear), backed by +1144 |
| +1140 | CPU image object 1280x720 (`sub_8218DE38`) aliasing the same +1144 memory |
| +1148 | JPEG output buffer, 0x64000 bytes |
| +1128/+1136 | preview material bound to texture +1132 (`sub_8218F8D0` = SetTexture) |

So the capture target is a linear 1280x720 32bpp texture whose memory the CPU
reads directly through the +1140 image.

**Capture window.** `sub_8263EAA8` runs every frame (called from `sub_821FD5B8`):
- request byte `0x8290622C` set: it calls **`sub_8263D120` (freeze for
  capture)**. That sets the capture-active byte `0x8290622E`, saves and ORs the
  renderer masks (renderer object `*(0x8287E064)`, +424 |= `0x28000000`, +428),
  stores 1 at two render/world flags and calls `sub_82224E58(..., 3)`.
- request byte `0x8290622D` set: it calls **`sub_8263D1E0` (unfreeze)**, which
  restores all of that.

**State machine.** `sub_8263CB78` is a virtual tick (state at +132 and
sub-state at +136, relative to the album base +456). While `0x8290622E` is set
it counts frames at +732, and only after more than 3 frames starts processing
(state 3, sub-state 9):
1. sub-state 9, `sub_8263C638`: resize image +1140 to the screen dimensions
   read from `827D426C`..`827D4278`, if they differ;
2. sub-state 12, `sub_8263C728`: JPEG-encode +1140 into +1148 with quality 85
   (`sub_82775C60`, the libjpeg compress wrapper identified through
   `jpeg_std_error`/message table `820D7F90`);
3. sub-state 10, `sub_8263C7E0` / `sub_8263C5C8`: re-encode with lower quality
   until the size is below about 0x31FFF;
4. fire event 9 `snapshotTaken`, go to state 4;
5. state 4, `sub_8263C8B0`: check the JPEG SOI marker, then **decode the JPEG
   back into image +1140** (`sub_82775DE0`, the decompress wrapper). If the
   dimensions changed it re-creates the texture (vtable +92), then binds the
   preview material to texture +1132. **This decoded picture is the preview.**

With 1280x720 album JPEGs (KI-021) the resize is a no-op, so the preview
pixels are written by the CPU into the same +1144 memory the capture used.

**Not identified:** the exact guest call that issues the GPU resolve into
+1144. No function in the album cluster passes +1132 to a resolve. It is
issued by the renderer during the frozen frames, after the mask change. The
3-frame wait is the guest allowing for GPU latency before the CPU reads.

## 2. Lead 1: first multi-tile EDRAM resolve (SDK render-target cache)

`D3D12RenderTargetCache::Resolve` (`render_target_cache.cpp:1213-1420`):
- With host render targets, the `direct_host_resolve` route
  (`TryResolveCopyDirectly`, `:5887-5935`) is only a preflight that always ends
  in `DumpRenderTargets`.
- `DumpRenderTargets` copies only the render targets that **currently own**
  the affected EDRAM tile span into `edram_buffer_`
  (`GetResolveCopyRectanglesToDump`). The resolve compute shader then reads
  `edram_buffer_`.
- Tiles that no render target owns keep whatever `edram_buffer_` last held:
  zero if never dumped in the session, otherwise a stale dump from a render
  target of another pitch or format.
- Dump, transfer and resolve pipelines are created synchronously
  (`GetOrCreateTransferPipelines` `:2071`, `GetOrCreateDumpPipeline`,
  `resolve_copy_pipelines_`). Host render targets are created lazily and keep
  their contents across the session.

I found **no code path that returns partial data on the first resolve and
correct data later** for a fixed draw stream. Lazy creation, ownership
transfer and dump all behave deterministically for a given guest command
stream.

The "black top, horizontal row-band stripes" pattern is, however, exactly what
this cache produces when the frame that should fill the source tiles **did not
render**:
- tiles that were never owned read as zero, which gives black;
- tiles transferred from a render target with a different pitch or format give
  row bands.

So lead 1 describes how the corruption looks, not its first-use trigger.

Scaled-resolution caveat (scale > 1 only): `IssueCopy_ReadbackResolvePath`
downscales with `resolve_downscale.cs`. That shader assumes the destination is
in Xbox 32x32-tiled layout (`tile_count = written_length / (32*32*bpp)`), but
the photo target is linear. If photos were taken at `draw_resolution_scale > 1`,
the CPU copy may be scrambled. That would affect **every** photo, not only the
first, so it does not explain KI-037. It is a residual risk.

## 3. Lead 2: draws skipped during asynchronous pipeline creation

### 3.1 The pin drops draws, unlike upstream Xenia

- `async_shader_compilation` defaults to true, with hot reload
  (`command_processor.cpp:80`, common).
- `PipelineCache::ConfigurePipeline` (`d3d12/pipeline_cache.cpp:946`) queues
  new pipelines to creation threads when async is on.
- In `D3D12CommandProcessor::IssueDraw` (`d3d12/command_processor.cpp:2571-2583`),
  if the pipeline state is still null the function **returns true without
  recording the draw**. Only `gpu_render_audit` counts this
  (`render_audit_pso_pending_skip_`). That count is printed once, cumulatively,
  in `XENOS_AUDIT_CP_SUMMARY` (`:1671-1692`). There is no per-frame counter and
  no line in the M8 per-minute diagnostics.
- `PipelineCache::EndSubmission` (`:799-817`) waits for queued pipelines, but
  by then the skipped draws are already gone. Upstream Xenia never drops the
  draw; it waits at submission end. Dropping is a fork-specific trade-off
  ("may cause brief visual artifacts").
- Render-target `Update` runs before `ConfigurePipeline`, so a skipped draw
  still moves EDRAM ownership and transfers. The render target then holds
  transferred or stale data with nothing drawn over it, which is the signature
  described in section 2.

### 3.2 Why the photo is the worst case

The capture is taken 1 to 3 frames after `sub_8263D120` changes the renderer
masks and freezes the world, and the guest then encodes that frame on the CPU.
Any pipeline first needed in those frames is still compiling (DXIL plus PSO
creation, typically tens to hundreds of milliseconds), so its draws are
missing from exactly the frame that becomes the JPEG.

Such pipelines include capture-only passes and any render target format or
MSAA variant used only for the capture; the PSO key includes the bound render
target formats. Once created they stay: later photos are correct and the
broken first photo can be retaken. This matches the KI-037 description.

### 3.3 "First photo in a session" versus "first photo in a build cache"

- `store_shaders` defaults to true (`graphics_system.cpp:71`). Each pipeline
  description is appended to `<cache_root>/shaders/shareable/545407F8.rtv.d3d12.xpso`
  (`pipeline_cache.cpp:284`, `:1074`).
- On the next start, stored pipelines are created **blocking** before the
  guest runs (`rex_app.cpp:463`, `InitializeShaderStorage(..., true)`).
- The launcher passes a per-build `cache_root`. The owner's save root holds one
  `cache\<64-hex build hash>\` directory per build, about 40 of them, with
  dates from 11.10 to 29.10.
- **Every test build therefore starts with an empty pipeline cache.**

All recorded reproductions (l18-photo, l20, l21, l29-sunc) were first photos
on freshly staged builds. The l17b control is the one case where the build was
not new; whether l17b's cache already held a photo session is unknown.

Lead 2 predicts:
- a second session on the same build, after one photo session, has a correct
  first photo;
- `--async_shader_compilation=false` gives a correct first photo even on a
  fresh build cache.

### 3.4 Same-frame first-use shaders

These cannot be listed statically. The pipeline cache does not log per
pipeline, and the renderer's capture call was not identified (section 1).

## 4. Preview shows the previous photo

### 4.1 Readback layer, `IssueCopy_ReadbackResolvePath` (`d3d12/command_processor.cpp:3176-3355`)

- **Delayed modes (`fast`, `some`)** copy `rb.mapped_data[1 - write_index]`,
  the previous resolve with the same (address, length) key, into guest memory.
- A key counts as a miss, and is waited on and copied synchronously, only if
  that buffer is absent; buffers are evicted 60 frames after last use
  (`:5344-5378`).
- In `some` mode a non-miss resolve is copied **only every 4th time**
  (`kReadbackSomeRefreshInterval`, `command_processor.h:405-409`).
- The delayed read never checks that the previous buffer's GPU copy has
  completed (`submission_written` is unused here). Fresh readback buffers are
  created `CREATE_NOT_ZEROED`. A key resolved twice within one in-flight
  window can therefore deliver uninitialised or partially written data.

  This is a real latent race. It does not explain a one-off photo key
  (miss, then synchronous). The l17b `fast` control and the l21 stale-key
  synchronous test both failed to fix the first photo, which agrees.

### 4.2 Texture cache and write watches (`shared_memory.cpp`)

- `RangeWrittenByGpu` (`:427-443`) fires watches, marks the pages valid and
  GPU-written, and write-protects the guest views A0/C0/E0
  (`xmemory.cpp:682-690`).
- The CP readback `memcpy` goes through `TranslatePhysical`, the unprotected
  physical view (`xmemory.h:365`, base `+0x100000000`). It does not fire
  watches, which is correct.
- The guest's JPEG decode into +1144 goes through a protected view, so it
  faults, runs `MemoryInvalidationCallback` (`:704-780`), invalidates and fires
  watches, and the texture is re-uploaded. Because neighbouring pages are
  GPU-written, every 4 KB page faults separately. That is slow but correct.
- `clear_memory_page_state=false` only skips the per-swap reset
  (`SetSystemPageBlocksValidWithGpuDataWritten`, `command_processor.cpp:3890`).
  CPU writes are still caught by watches, as in upstream Xenia.
- Bulk Type-0 writes do not touch resolve or coherency handling.
- All M8 changes post-date the defect (l17b reproduces it).

I found no write-watch gap that leaves the decoded preview stale.

### 4.3 Guest double buffering and timing

The album has explicit `SwapNewJpegBuffers` and `SetPreviewPicture` script
functions, so the slot-chooser preview is driven by script ordering against
the frame-counted state machine.

The preview symptom was first recorded on **l29-sunc, unlocked frame rate**.
There, frame-counted guest logic runs faster (compare the photo album cursor
speed issue earlier in M8). A script-versus-state-machine ordering race could
show the other JPEG buffer, i.e. the previous photo. It is untested at stock
30 FPS.

### 4.4 Conclusion on the preview

It is **not** explained by the async-pipeline lead. The best candidates are:
- **(a)** unlocked-mode guest timing, with the swap and preview order broken;
- **(b)** if the preview is fed by a GPU resolve into a recurring key, the
  `some` or delayed readback rule above, under which a non-miss one-off is not
  copied and the CPU sees the previous content. The l21 rule (stale key, so
  synchronous) was never tested against the preview symptom.

## 5. Ranked hypotheses

| # | Hypothesis | Confidence | Fits | Against |
|---|---|---|---|---|
| H1 | Async pipeline creation drops draws in the capture frames; the resolve bakes stale or zero EDRAM into the photo | **~60% (first photo)** | First photo only, later correct, retake works, readback-mode independent (l17b/l20/l21), black-plus-stripes signature, every repro on a fresh per-build cache | "Per session" wording if a non-fresh cache ever reproduced (l17b history unknown) |
| H2 | Render-target cache ownership: tiles never owned or transferred from another pitch on first use, without skipped draws | ~10% standalone | Same visual signature | No first-use-only mechanism found; deterministic |
| H3 | Readback delayed/`some` stale or in-flight buffer (incl. the missing completion check) | ~10% first photo; **~35% preview** | Preview "previous photo"; code race real | l17b `fast` and l21 synchronous one-offs did not fix the first photo |
| H4 | Guest timing race (SwapNewJpegBuffers/SetPreviewPicture) at unlocked FPS | **~40% preview** | Preview symptom first seen on l29-sunc | Untested at 30 FPS; guest order not traced |
| H5 | Scaled-resolve downscale assumes tiled layout for a linear target | <5% | Stripes | Would break every photo, and only at scale > 1 |

## 6. Decisive diagnostic (no build needed)

**Test A, cheapest, zero risk.** On the current daily build (same build folder
and profile), take one photo in a session and exit normally. Then start a
**new** session on the same build and take a photo as the first action in
Photo Mode.
- Correct: H1 strongly supported. The pipeline descriptions were persisted and
  pre-created at startup.
- Still broken: H1 is unlikely, and lead 1 or a guest-side cause rises.

**Test B, the A/B switch.** On a build or profile cache where no photo has
been taken (any freshly staged build), launch with
`--async_shader_compilation=false` and take the first photo.
- Correct, while the same fresh build without the flag is broken: H1
  confirmed.

Expect extra hitches on first-seen effects in gameplay with the flag. It is an
A/B only.

Optional supporting log: `--gpu_render_audit=true` prints a cumulative
`XENOS_AUDIT_CP_SUMMARY ... pso_pending_skip=` at shutdown. Compare it between
a run with exactly one photo on a fresh cache and the same run with the flag
off. It is cumulative, so it is weaker than A or B.

**Preview (separate).** Repeat the save-slot preview check on the stock 30 FPS
build (`l31-s30`), taking photo 1 and photo 2 and saving each:
- correct at 30 FPS but wrong unlocked: H4;
- wrong at 30 FPS too: run once with `--readback_resolve=full` (fully
  synchronous; accepted at this pin, `command_processor.cpp:92-103`) to test H3.

## 7. Fix proposal

`ki-037-proposal.diff` (not applied; apply only if Test A or B confirms H1):
- **SDK:** add `PipelineCache::AwaitPipeline(handle)` (create queued pipelines
  on the CP thread, then wait for `creation_complete`) and a hot-reload cvar
  `d3d12_async_pipeline_wait`. When the cvar is true, `IssueDraw` waits for a
  pending pipeline instead of dropping the draw.
- **MCLA:** `src/mcla_photo_capture.cpp` strongly overrides `sub_8263D120` and
  `sub_8263D1E0`, the same pattern as `mcla_menu_state.cpp`, and sets the cvar
  for the capture window only. The world is frozen then, so the one-off hitch
  is invisible.
- A runtime flip of `async_shader_compilation` itself is unsafe. Pipelines
  already queued would be recorded with a null PSO, and the deferred-list
  replay silently keeps the previous PSO
  (`deferred_command_list.cpp:522-528`).

The tester-guide workaround (discard and retake) stays in place regardless.

## 8. Unresolved risks

- The renderer's capture resolve call and its source render target
  configuration were not identified. "Capture 1-3 frames after the mask change"
  is inferred from the state machine, not traced.
- l17b cache history is unknown (one control repro on a possibly warm cache).
- The persisted pipeline set may not cover every capture variant (dynamic
  modification bits), so Test A could be a false negative. Test B is the
  stronger one.
- The preview mechanism is unconfirmed (H3 versus H4).
- The latent delayed-readback completion race (4.1) is real but not proven to
  matter here.
- Hunk line numbers in the diff are approximate. The SDK hunks ship through
  the private SDK patch flow.
