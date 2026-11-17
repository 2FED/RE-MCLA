# l108 SDK: ownership-transfer elision before a full overwrite (2026-11-17)

**SDK READY: `private/l108-sdk-install`** (CMake version `0.20.0.16-dev.g5e65c7a`).

Implemented by an Opus subagent for the primary agent, from the l107 measurements on the RTX 5060
([l107 force_hidef](M11-l107-pc-hidef-20261117.md): GPU 19.1 ms per frame, of which ownership transfers
8.5 ms; 83 transfers per frame, 81 % triggered by a full-surface rectangle draw, 89 % overwrite
candidates) and the l107 census design ([l107 SDK](M11-l107-sdk-transfers-20261117.md) section 7: "the
proof needs the draw's covered area and pixel kill, which the render-target cache does not have").
SDK commit on the local branch `mcla/m11-l108` (parent `cb92be8` = l107): `5e65c7a`
(`5e65c7afe49bba89a395dbf519c6b50a5634891f`), worktree `private/l108-sdk-wt`; not pushed, no tag.
**Built and unit-tested only: nothing has run in the game, there is no physical PASS, and no number
here is measured with l108.** Spark not used: deterministic source work checked by unit tests; checking
a model answer would cost more than writing and testing the proof.

Licence: no code was copied. The proof reuses, inside this SDK, the Xenia `ShaderInterpreter`
(BSD-3, already in the tree for the draw extent estimator) and the existing cutout mechanism of
`Transfer::GetRectangles` (BSD-3, used by resolve clears). Nothing was taken from GPL or unlicensed
peers (the 2026-11-15 research only supplied the idea that the game's clears are drawn as rectangles).

## 0. What shipped

| Goal | Status | Switch (default, phase) |
|---|---|---|
| S1 transfer elision before a full overwrite | **shipped**, per plane, as a cutout of the transfer rectangles | `d3d12_transfer_elide_full_overwrite` (false, hot: read at every draw that needs transfers) |
| S1 verify / audit | **shipped**: every transfer performed, the cut counted, an occlusion query per proven draw | `d3d12_transfer_elide_full_overwrite_verify` (false, init-only); wins over the elision when both are on |
| S1 counters | **shipped**: `MCLA_XFER_ELIDE v=1` after each `MCLA_XFER` line, only while one of the two is on | none |
| S2 depth 2x <-> 4x round trip | **analysis only** (section 6); the rectangle-triggered half is covered by S1 when the proof holds | - |
| S3 F10 capture size | **host fix** (host report); the SDK capture path unchanged | - |

With both switches off nothing changes: the proof is not evaluated (no CPU interpretation, no
counters, no line), the transfers, the census and every l107 line are as before.

## 1. Design

One descriptor per draw (`ElideDescriptor`, `include/rex/graphics/util/transfer_elision.h`): plane
bits (depth, stencil, colour 0-3), the proven rectangle (guest pixels of the render targets) and the
host samples of that rectangle (the verify expectation). Flow:

1. `IssueDraw` stores the proof inputs of the draw (vertex and pixel shader, primitive type, index
   count, indexed, explicit major mode, memexport) before the render-target update
   (`src/graphics/d3d12/command_processor.cpp`, the block after `SetXferDrawPrimitive`);
   `SetXferDrawPrimitive` also resets the elision state of the draw (`render_target_cache.h`).
2. `D3D12RenderTargetCache::Update` (`render_target_cache.cpp` :1468): only when the draw needs
   transfers, is a rectangle list and the elision or its verify is on, it asks
   `D3D12CommandProcessor::ProveFullOverwriteForCurrentDraw` (`command_processor.cpp` :5301), which
   gathers the state and runs the pure `ProveFullOverwrite` (`transfer_elision.h` :246); the vertex
   shader is interpreted for the 3 vertices (:5408) only when everything else can still pass.
3. `PerformTransfersAndResolveClears` (`render_target_cache.cpp` :4983-5065): per destination,
   `ElideCutsForDest` (`transfer_elision.h` :402) says which parts the planes cover: the colour copy,
   the depth copy (with a pixel-shader stencil reference the depth copy writes stencil too, so it needs
   both planes), and, without a stencil reference, the stencil clear and per-bit passes separately.
   The covered parts use the proven rectangle as the cutout of `Transfer::GetRectangles` (the
   mechanism resolve clears use), so only the rest of each transfer's range is copied; an invocation
   with nothing left is dropped (:5115 stencil, :5164 colour / depth); the merged draws and the stencil
   clear rectangles use the same cutouts (`invocation_cutout`, :5267, :5275, :5350).
4. Ownership bookkeeping is untouched (it is done by the common `RenderTargetCache::Update` before the
   transfers): the destination becomes the owner of the whole range as before, without the copy.
5. If the draw is then not issued (pipeline still being created and skipped, any failure after the
   update), a guard in `IssueDraw` (:5565) calls `RestoreElidedTransfers` (`render_target_cache.cpp`
   :1224): all of the update's transfers are performed after all (nothing has drawn since, so the
   sources still hold the data; repeating the parts already copied gives the same values, also for the
   host-depth copy path). Counted as `restored=`.

The l101 RT Update memo: a memo hit only happens when the previous update had no transfers and nothing
changed, so a hit has no transfers to elide; the descriptor is evaluated only inside a real `Update`
(never stale: reset per draw), and the transfers bump the update epoch, which invalidates the memo as
before. Memo hits of rectangle-list draws are counted (`refuse=...,memo:`).

## 2. Proof conditions (`ProveFullOverwrite`, `transfer_elision.h`)

All required; any failure keeps the whole transfer (first failing reason counted):

| Condition | Reason | Line |
|---|---|---|
| rectangle list, exactly 3 auto-indexed vertices (`VGT_DRAW_INITIATOR` source auto, no DMA index buffer), not tessellated | `rect` | :254 |
| no memexport (vertex or pixel shader) | `memexport` | :257 |
| MSAA 1x / 4x or native 2x (2x emulated with a 4x sample mask refused) | `msaa` | :260 |
| no pixel-shader kill | `kill` | :263 |
| no alpha test (a pixel shader with `alpha_test_enable` and a function not ALWAYS) | `alpha_test` | :266 |
| no alpha to coverage (`alpha_to_mask_enable`, refused whenever set) | `a2c` | :269 |
| depth test off or ALWAYS (normalized depth control) | `depth_func` | :272 |
| stencil test off or ALWAYS, also the back face when two-sided | `stencil_func` | :276 |
| at least one plane written in full (`ElideWrittenPlanes` :217): colour: all 4 bits of the normalized mask (which marks components the format lacks as written) and ONE / ZERO / ADD for colour and alpha; depth: z test and z write on; stencil: on, pass operation REPLACE or ZERO, write mask 0xFF (both faces when two-sided) | `write_mask`, `blend` | :281 |
| scissor (`draw_util::GetScissor`, render-target pixels with the window offset) not empty, and the host viewport (`draw_util::GetHostViewportInfo`, exactly as `IssueDraw` computes it) contains it | `scissor` | :284, :294 |
| clipping on: no user clip planes | `clip` | :297 |
| vertex shader interpretable (no texture fetch), every vertex with all four position components exported and no vertex kill | `vs` | :300, :309 |
| W (1 / w unless `vtx_w0_fmt`) finite, positive and equal for the 3 vertices (relative 1e-6) | `vs` | :315, :320 |
| host positions finite (NDC = xy, or xy / W unless `vtx_xy_fmt`; host NDC = NDC x ndc_scale + ndc_offset; Direct3D viewport mapping, y down) | `vs` | :329 |
| clipping on: every host NDC Z inside [1e-5, 1 - 1e-5] (a Z on the far or near plane is refused: CPU / GPU rounding could clip it) | `clip` | :335 |
| non-degenerate, three distinct corners of an axis-aligned rectangle (tolerance 1/256 host pixel) | `coverage` | :344, :359, :373 |
| the inner edges outside the scissor by at least -1/16 host pixel (no sample of the standard 1x / 2x / 4x patterns is closer than 1/8 host pixel to a pixel edge; programmable sample positions are not used in this SDK) | `coverage` | :381 |

The proven rectangle is then the scissor; the host scissor of the draw is that rectangle times the draw
resolution scale, and with the host viewport containing it and one rectangle covering it every sample
inside is written exactly once. Half-pixel offset, window offset (predicated tiling tiles), viewport
enables and the clip-disabled huge viewport are all inside `GetHostViewportInfo`'s ndc_scale /
ndc_offset, so the proof uses the same numbers the host vertex shader uses.

## 3. Verify (`d3d12_transfer_elide_full_overwrite_verify`)

- Init-only: an occlusion query heap of 1024 and a mapped read-back buffer (`render_target_cache.cpp`
  :441). With the verify latched the elision is never applied (the counters show what it would cut).
- Around the draw command of every draw with a proof that needed transfers
  (`ElideVerifyBeginDraw` / `EndDraw`, `command_processor.cpp` :5950 non-indexed, :6012 indexed): the
  occlusion count must equal the proven rectangle's host samples (pixels x resolution scale x samples).
  This measures the claim itself, kill, coverage and tests included: a short count means some sample
  was not written. Not measured (`lost`) while a guest occlusion query is open (D3D12 queries of one
  type do not nest) or the ring is full.
- Read when the submission's fence passed (`ElideVerifyCollect` :1280); the first 16 mismatches as
  `MCLA_XFER_ELIDE_VERIFY v=1 result=bad expected= got= planes=0x.. rect=x,y,wxh` (warning).
- What it does not check: that the GPU's coverage of a rectangle equals the CPU interpretation in every
  frame of the elision run (the verify run proves it on the frames it saw; an elision run has no
  queries); image comparison remains the final check (the owner's by-eye run).

## 4. Counters (`MCLA_XFER_ELIDE v=1`, `FormatElideLine` :456)

```
MCLA_XFER_ELIDE v=1 reason= seq= frames= on= verify= draws= proven= planes=depth:,stencil:,color:
  refuse=rect:,memexport:,msaa:,scissor:,vs:,clip:,coverage:,kill:,alpha_test:,a2c:,depth_func:,
  stencil_func:,write_mask:,blend:,memo: xfers= cut= whole= samples= cut_samples=color:,depth:,stencil:
  st_passes= restored= check=n:,ok:,bad:,lost:
```

`draws` rectangle-list draws with transfers that were evaluated, `proven` with a proof, `planes`
proven draws by plane, `refuse` the first failing reason (`memo` counts memo hits of rectangle-list
draws), `xfers` transfers into destinations of evaluated draws, `cut` with a part cut (or that would be
with the verify), `whole` with nothing left, `samples` the destination host samples of those transfers,
`cut_samples` the samples cut per plane, `st_passes` per-bit stencil passes left out (8 per stencil
fully cut), `restored` late restores, `check` the verify. Window totals like `MCLA_XFER`.

## 5. What is not covered

- Non-rectangle draws (the "4x -> 2x on draw" round trip is triggered by an ordinary draw), several
  rectangles in one draw, indexed rectangles, vertex shaders with texture fetches, Z on the clip planes
  with clipping on, user clip planes, alpha to coverage, emulated 2x.
- Partial overwrites by a stencil test or depth test that is not ALWAYS (data dependent).
- Resolve-clear rings (already cut out by the existing resolve-clear cutout) and the host depth store of
  a destination whose depth is fully cut (still done; harmless, small).
- Vertex data written by the GPU (a resolve or memexport into the vertex buffer) is not visible to the CPU
  interpreter; the clear rectangles are CPU-written; the verify would show a mismatch.

## 6. S2: the depth 2x <-> 4x round trip in the HiDef path

From the l107 census and the guest RE ([guest tiling](M11-l107-guest-tiling-and-msaa-20261117.md)
section 2): with force_hidef the scene is 2x in one tile and the shadow / depth collector
`sub_825F48E8` always hands BeginTiledRendering 4 (`li r6,4`, return 0x825F494C), so its tile surfaces
are 4x and share EDRAM with the scene depth. The two directions:

- scene depth 2x -> collector 4x: triggered when the collector pass starts; the collector's own tiled
  begin clears its tiles (flags bits 2 / 3), drawn as rectangles (`trig=rectfull`). If that clear writes
  depth with ALWAYS (and stencil with REPLACE) over the scissor, S1 proves it and the depth / stencil
  copy is cut: this half is an EDRAM-aliasing artefact (the collector overwrites what it inherits).
- collector 4x -> scene 2x "on draw": triggered by an ordinary scene draw after the collector pass. On
  the console the scene then sees whatever the collector left in EDRAM, so the host must reproduce it
  unless the scene re-clears; whether the scene reads that depth before overwriting it cannot be proven
  at the draw (it would need the lazy / deferred transfer of l107 section 7). Not elided.
- What would remove the round trip at its root (guest side, not implemented here): a host arm making
  the collector use the scene's sample count, i.e. lower r6 4 -> 2 for the caller with return address
  0x825F494C in the existing `mcla_guest_tiling` override (`AppliedCount` today leaves the collector
  untouched). Then both surfaces are 2x and the MSAA-conversion transfers become same-count transfers or
  vanish when the surfaces coincide; visual effect (shadow depth precision / edges) unknown. Recommended
  as a separate default-off key after the S1 audit; reported to the primary, not written here.

## 7. Tests, build, install

- New `tests/unit/graphics/transfer_elision_test.cpp` (7 cases, `[l108]`): the game's clear proven
  for every plane (corner order, 1x / 2x / 4x samples, a window-offset tile, a rectangle larger than the
  scissor); each refusal reason in a table (27 rows: rect x4, memexport, msaa, kill, alpha test, a2c,
  depth func, stencil func front / back, write mask, blend, scissor x2, clip x2, vs x5, coverage x4);
  planes (stencil op, mask, back face, depth write and test, colour per target, blending); the sample
  margin (1/32 host pixel inside passes, 1/4 and a bottom edge 1/2 inside fail); clipping on with W
  not reciprocal / reciprocal, Z outside, the exact viewport, a viewport smaller than the scissor; cuts
  per destination (10 rows, with and without a stencil reference); counters and the exact line.
- Build: fresh `private/l108-sdk-b` with the l107 options (Ninja Multi-Config Release, clang 20.1.8,
  VS 2022 build tools, `REXGLUE_ENABLE_FFX_PRESENTER_SHADERS=ON`, Tracy and perf counters on, Vulkan
  off), output `private/l108-sdk-o`; third-party submodule trees copied into the worktree from
  `private/l107-sdk-wt`. Normalized warning set (union of the build and install logs) **211 entries,
  0 new, 1 gone** against l107 (the unused `index_count` parameter of `IssueDraw`, now used).
- `git-clang-format` against `cb92be8` (include, src, tests): applied, rebuilt, clean.
- ctest (full suite, `-j 8`): **2345 of 2345 passed** (2341 passed + 4 skipped BitStream; l107 2338 +
  7 new) three times in a row before formatting and three times in a row after formatting and rebuild.
- Install `private/l108-sdk-install`, CMake `0.20.0.16-dev.g5e65c7a`; adds
  `include/rex/graphics/util/transfer_elision.h`.

| bin file | Bytes | SHA-256 |
|---|---|---|
| rexruntime.dll | 11776000 | 2767E8B71E3A2ECB6A51E9EF2A9381036B5189F014CBEDBB9212726663FD764A |
| rexgpu-xenos.dll | 3705344 | F7D9EE8FC8C3D7B6B9DE5EAC842BAEDA9E1A14988708E5BFCE8112278897CB30 |
| TracyClient.dll | 246784 | 64BEBD17F8BAC9554AAE8ED8F2DF352EFB3B2993401AB2B9C7AD3D30EEEBA4E9 |
| rexglue.exe | 4113920 | 9F3037EDB3FC3BABD1C4E9A0A8D42D3964B656B67546A531201E45A2390890A7 |

rexruntime.dll and TracyClient.dll have no source change but new hashes (fresh build tree, version
string). **ABI:** `D3D12RenderTargetCache` and `D3D12CommandProcessor` changed: the host must be rebuilt
against this SDK. New SDK cvars: `d3d12_transfer_elide_full_overwrite`,
`d3d12_transfer_elide_full_overwrite_verify`.

SDK diff `cb92be8..5e65c7a`: 7 files, 1394 insertions, 12 deletions.

## 8. Risks

- No game run: the proof, the cut, the restore path and the verify are unobserved. Run the audit first
  (`bad` must be 0); if `proven` or `cut` stays 0 the game's clears fail a condition (`refuse=` names it:
  for example `clip` if they are drawn with clipping on at Z = 1, or `vs` if their vertex shader fetches
  textures) and the lever needs a different proof.
- The elision run has no queries: a frame type not seen in the audit (a menu, a cut scene, a photo mode)
  is covered only by the static proof.
- The cut changes only GPU work, but a wrong proof shows as stale data in the cut region (old shadows,
  previous-frame colours at the screen edges): the by-eye run looks for exactly that.
- Interpreting the vertex shader costs CPU per evaluated draw (3 vertices; only rectangle lists that need
  transfers, while a switch is on).
- The evidence file could not be written into `docs/evidence/` by the subagent directly (shared-checkout
  write guard); it was copied in by a script from the scratch folder below.
- Scratch: `%LocalAppData%\Temp\claude\C--BDU-MCLA-Recomp\l108-work` (`sdk-build.ps1`, `sdk-test.ps1`,
  `sdk-install.ps1`, `copy_tp.ps1`, `normwarn.py`, `edit_lines.py`, `sdk-commit-msg.txt`; logs
  `sdk-configure.log`, `sdk-build1..3.log`, `ctest1..6.log`, `sdk-install.log`, `w108all.txt`).
