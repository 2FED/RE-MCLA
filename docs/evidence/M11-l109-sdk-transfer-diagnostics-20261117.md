# l109 SDK: transfer refusal / trigger diagnostics, exact sample coverage, clip-plane Z, lazy-stencil count mode (2026-11-17)

**SDK READY: `private/l109-sdk-install`** (CMake version `0.20.0.17-dev.g4a29e93`).

Implemented by an Opus subagent for the primary agent, from the owner's l108 `pc/elide-verify` result
(session 55a88bcf: `check=n:14040,ok:14040,bad:0` in the last minute window, 37,794 proofs over the
run, 0 bad; proven draws colour only, `planes=depth:0,stencil:0`; refusals per minute window
`coverage:29006, clip:5278, vs:4678, rect:1904`, `memo:80331`; `cut_samples` 1.3 % of the transferred
samples; `cat_ms_pf=color:0.956,depth:6.993`). SDK commit on the local branch `mcla/m11-l109` (parent
`5e65c7a` = l108): `4a29e93` (`4a29e9382f1fc37e44cd0f1f3f7e3c25fed78440`), worktree `private/l109-sdk-wt`; not pushed, no tag. **Built and
unit-tested only: nothing has run in the game, there is no physical PASS, and no number here is
measured with l109.** Spark not used: deterministic source work checked by unit tests; checking a
model answer would cost more than writing and testing the code.

Licence: no code was copied. The sample positions are the SDK's own `draw_util`
`kD3D10StandardSamplePositions2x / 4x` (BSD-3 Xenia code already in the tree); the rasterization
rules (1/256 snapping, top-left rule, inclusive depth clipping) are the public Direct3D functional
specification. Nothing was taken from GPL or unlicensed peers.

## 0. What shipped

| Goal | Status | Switch (default, phase) |
|---|---|---|
| D1 refusal diagnostics | **shipped**: `MCLA_XFER_REFUSE v=1` per perf-split window, a signature per evaluated rectangle draw (proven ones too) | while `d3d12_transfer_elide_full_overwrite` or its verify is on (as l108) |
| D2 trigger profile | **shipped**: `MCLA_XFER_TRIG v=1`, every transfer by (MCLA_XFER signature, triggering draw class) with the first later event per plane | `d3d12_transfer_trigger_profile` (false, init-only) |
| D3a exact sample coverage | **shipped** in the proof (replaces the l108 1/16-pixel criterion), partial proofs, verify with an expected range | under the existing elision / verify switches |
| D3b proof extensions | **clip-plane Z shipped** (only the exactly computed cases); **vertex shader: nothing** (the only non-geometric `vs` refusal is "not interpretable" = texture fetch, which the CPU interpreter cannot do safely); everything else stays refused | as D3a |
| D3c lazy stencil | **count / verify mode only**: `MCLA_XFER_LAZY v=1`; the deferred transfer itself is **not built** (section 5) | `d3d12_stencil_transfer_lazy_verify` (false, init-only); no `d3d12_stencil_transfer_lazy` |

With all switches off nothing changes against l108 except the proof's coverage criterion, which is
only evaluated while the elision or its verify is on.

## 1. D3a: exact sample coverage (`transfer_elision.h`)

The l108 proof refused a rectangle unless its inner edges were within 1/16 host pixel of the
scissor (a margin derived from "no standard sample is closer than 1/8 to a pixel edge"). That is a
sufficient but not the exact criterion, and it can never prove a rectangle a little smaller than
the scissor. l109 computes the covered samples exactly:

- The host draws a rectangle list as two triangles; the D3D rasterizer snaps vertices to 1/256 host
  pixel and applies the top-left rule, so with the snapped edges L, R, T, B a sample at (x, y) is
  covered exactly when `L <= x < R` and `T <= y < B` (left / top inclusive, right / bottom
  exclusive; the diagonal covers each sample once).
- Samples: the D3D10+ standard positions (`draw_util::kD3D10StandardSamplePositions2x / 4x`, already
  assumed by this SDK's host render-target and ROV paths; 1x: the centre), on the 1/16 grid, so all
  comparisons are exact integers in 1/256 units (`ElideCoverAxis`).
- The snapped edge is uncertain by the CPU / GPU rounding of the position (`ElideSnapEdge`: 1/1024
  host pixel or 4 float ulps of the coordinate, whichever is larger, then either tie rule). The proof
  uses the least favourable end (`*_worst`), the verify accepts the count range (`samples` ..
  `samples_max`, equal unless an edge sits within that rounding of a half 1/256 step).
- The proven rectangle is now the set of guest pixels of the scissor all of whose host samples
  (scale x scale host pixels x samples) are covered: the whole scissor (`full`), or a smaller one
  (`partial`) when edge pixels are only partly covered. A rectangle that covers no whole guest pixel
  is refused (`coverage.nopixel`). The verify's expectation counts every covered sample of the host
  scissor (border pixels included), so `check=` stays exact.
- Changed l108 test expectations (the criterion was too strict): a rectangle 1/8 guest pixel inside
  at scale 2 / 2x puts the left edge exactly on a sample (inclusive): now proven in full; "one guest
  pixel short" and a bottom edge 1/4 guest pixel short are now partial proofs (959 columns / 539 rows).

## 2. D3b: Z exactly on a clip plane

l108 refused any host Z within 1e-5 of 0 or 1 with clipping on (`clip`, 5278 per minute window in
the owner run). D3D clipping keeps `0 <= z <= w` inclusive; the host vertex shader writes
`z' = z * ndc_scale.z + ndc_offset.z * W` (one mad) and `w' = W`. Accepted now, and only:

- `z == 0` with `ndc_offset.z == 0`: z' is exactly 0 (also when Z is pre-divided: 0 x W);
- `z == w` with `ndc_scale.z == 1`, `ndc_offset.z == 0`, Z not pre-divided and W not reciprocal
  (`vtx_w0_fmt`, no GPU division): z' == w' exactly.

With W reciprocal (1 / w computed by the GPU's `div`, not correctly rounded) the case stays refused
and is reported as `.zr` in `MCLA_XFER_REFUSE` (`zplane_needs_reciprocal`). Residual assumption: the
GPU vertex shader produces the same z and w bits as the CPU interpreter (true when they are moved,
not computed differently); the verify's occlusion check measures it (Run 1 before Run 4).
Proofs on a clip plane are counted (`proven=...,zplane:`).

## 3. D1: `MCLA_XFER_REFUSE v=1` (`transfer_diagnostics.h`, `XferRefuseTable`)

```
MCLA_XFER_REFUSE v=1 reason= seq= frames= n=<evaluated> proven=full:,partial:,zplane:
  l108=would_prove:,coverage_now_proven: sigs= overflow= writes=<reason>/<czs>:<n>,...
  top=<signature>:<count>:<first example>|...   (24 by count)
```

Signature `<reason>.<detail>.m<samples>s<sx>x<sy>.w<c|-><z|-><s|->[.e<LTRB>.o<0|1>][.zp|.zr][.vs<hash>]`:

- `reason`: the l108 names (`none` = proven); `detail`: `full`, `partial`; `prim`, `count`,
  `indexed`, `tess` (rect); `tfetch` (vertex shader not interpretable), `nopos`, `vkill`, `w`,
  `wdiff`, `nonfinite` (vs); `ucp`, `znear`, `zfar`, `zoutside` (clip); `degen`, `corners`, `dup`,
  `nopixel` (coverage).
- `w...`: the planes the draw writes at all: colour mask, z write, stencil (enabled, a write mask
  and an operation other than KEEP). `writes=` totals the same per reason, so "does any rectangle
  write depth or stencil" is answered directly.
- `e<LTRB>` (once the corners were found): per edge left, top, right, bottom the shortfall of the
  rectangle against the scissor in host pixels: `L` larger by more than 1/256, `E` within 1/256, `h`
  short by at most half a host pixel, `p` at most one host pixel, `g` at most one guest pixel, `P`
  more (a partial clear). `o1` / `o0`: whether the l108 criterion passes on the same edges (the line
  also counts `l108=would_prove` and `coverage_now_proven`).
- `.vs<hash>`: the vertex shader's ucode hash, for vertex-shader refusals only.
- Example: `r=<left>,<top>,<right>,<bottom>` (host pixels, the rectangle's inner edges),
  `sc=<l>,<t>,<r>,<b>` (host scissor), `z=<min>/<max>` (host Z, clipping on).

## 4. D2: `MCLA_XFER_TRIG v=1` (`XferTriggerProfile`, `XferPendingTracker`)

```
MCLA_XFER_TRIG v=1 reason= seq= frames= n= sigs= overflow= samples=color:,depth:
  cat_ms_pf=color:,depth: top=<xfer sig>~<draw class>:<count>:<samples>:<est ms>:<next>|...  (16 by samples)
```

- `<xfer sig>`: the MCLA_XFER signature (`cause.trigger[.hd].src>dst`).
- `<draw class>` of the triggering draw (`rclear` for a resolve-clear ring):
  `p<prim>v<count bucket 3|6|32|256|4k|big>[R][I] z<enable><write><func>
  s0 | s1[b]:<func><fail><zfail><zpass>r<read mask>w<write mask>[/<back face>]
  c<colour mask hex>b<blended RTs> a<alpha test><a2c> ps<pixel shader hash>[D: writes depth][K: kills]
  sc<full|part: scissor vs surface pitch> m<samples>` (raw xenos enums: compare NEVER=0 .. ALWAYS=7,
  stencil op KEEP=0, ZERO=1, REPLACE=2, INCRSAT=3, DECRSAT=4, INVERT=5, INCRWRAP=6, DECRWRAP=7).
- `<est ms>`: the category GPU ms per frame of the window (colour, or depth + stencil, from
  MCLA_GPU_CAT when on) split by samples, as MCLA_XFER `top=`.
- `<next>` per plane (`c=` colour, `d=` depth, `s=` stencil): the first later event on the
  transferred plane of the destination, counted per transfer: `test` (a draw tests or reads it: depth
  func not ALWAYS / NEVER, stencil func not ALWAYS / NEVER with a read mask, colour blending that
  reads the destination), `rmw` (a stencil op INCR / DECR / INVERT), `write` (written partly without
  reading), `out` (the destination becomes a transfer source), `resolve` (a resolve of that kind;
  conservative: any), `clear` (a resolve clear covering the whole pending rectangle: never needed),
  `pclear`, `ow` (a proven full overwrite covering it: never needed), `src` (the transfer's source
  written first), `multi` (another transfer into the same plane in a later update), `edram`. Draws
  count only when their scissor intersects the pending rectangle (the bounding box of the transferred
  rectangles, destination guest pixels). The triggering draw itself is the first draw looked at.

This answers "is the transferred depth ever tested or read before being fully rewritten" (`d=`) and
"is the transferred stencil ever tested before the next transfer out or clear" (`s=`) per signature.

## 5. D3c: `MCLA_XFER_LAZY v=1` (count mode) and why the lazy transfer is not built

```
MCLA_XFER_LAZY v=1 reason= seq= frames= on=0 verify=1 st_xfers= cand= eager=
  next=test:,rmw:,write:,out:,resolve:,clear:,pclear:,ow:,src:,multi:,edram:
  never= never_passes= flushed= flushed_passes= open= open_cand=
```

`st_xfers`: depth-destination transfers with per-bit stencil passes (no PS stencil reference);
`cand`: those whose triggering draw does not use stencil (or a resolve-clear ring) - what a lazy
scheme could defer; `eager`: the triggering draw uses stencil; `next=`: how the candidates ended;
`never` (cleared or proven overwritten first) with their bit passes (`never_passes` = the passes a
lazy scheme would have saved); `flushed` (and passes) everything else; `open` / `open_cand`: still
pending at the end of the window (resolved in a later one).

Not built: a real deferral must keep the source's stencil valid until the flush. The render-target
cache gives no such guarantee today: after the transfer the destination owns the range, the source
host render target may be redrawn (its own ownership elsewhere), reused for another key, its EDRAM
range restored from a snapshot or cleared by a resolve of another surface, and the transfer
pipeline also reads the host depth copy of the source. Each of these would have to flush the pending
transfer first, from places (ownership changes in the common `RenderTargetCache::Update`, resolve
clears of overlapping surfaces, `RestoreEdramSnapshot`, render-target eviction) that do not see the
D3D12 transfer state. The count mode tracks exactly those events (`src`, `out`, `multi`, `edram`,
`resolve`) conservatively; if `never_passes` turns out large, l110 can build the deferral against
the measured event mix. No `d3d12_stencil_transfer_lazy` cvar exists.

## 6. Tests, build, install

- New `tests/unit/graphics/transfer_diagnostics_test.cpp` (10 cases, `[l109]`, 264 assertions):
  snapping and axis helpers (an exact edge, a half-step tie, the floor / ceil divisions, the sample
  counts); exact coverage for 1x / 2x / 4x at scales 1, 2 and 3 (exactly the scissor, shifted by plus
  and minus half a host pixel, by half a guest pixel, larger than the scissor, too short for any whole
  guest pixel) with the proven rectangle and the expected samples; an edge inside the snapping
  ambiguity (`samples_max - samples` = one column); clip-plane Z (far plane with W not reciprocal,
  near plane also pre-divided, reciprocal W refused and reported, Z offset, outside, near the plane);
  refusal details and written planes; edge buckets, refusal keys and the exact MCLA_XFER_REFUSE line;
  plane use (stencil test / rmw / write / none, depth, blended colour) and the class text; the lazy
  state machine (never needed by a full clear with the exact MCLA_XFER_LAZY line; flushes by a test,
  no event outside the scissor, the source written, transferred out, a later transfer with the same
  batch merging, a proven overwrite, resolves of each kind, an EDRAM restore, a partial clear, eager
  transfers); the exact MCLA_XFER_TRIG line.
- `transfer_elision_test.cpp`: the l108 expectations of the 1/16-pixel criterion replaced by the exact
  ones (section 1); the l108 refusal table without "one pixel short" (now a partial proof).
- Build: fresh `private/l109-sdk-b` with the l108 options (Ninja Multi-Config Release, clang 20.1.8,
  VS 2022 build tools, `REXGLUE_ENABLE_FFX_PRESENTER_SHADERS=ON`, Tracy and perf counters on, Vulkan
  off), output `private/l109-sdk-o`; third-party submodule trees copied into the worktree from
  `private/l108-sdk-wt`. Normalized warning set (union of the unit-test, full, post-format and install
  build logs) **211 entries, 0 new, 0 gone** against l108.
- `git-clang-format` against `5e65c7a` (include, src, tests): applied, rebuilt, then "clang-format did
  not modify any files"; `git diff --check`: clean.
- ctest (full suite, `-j 8`): **2355 of 2355 passed** (l108 2345 + 10 new; 4 BitStream skips counted as
  in l108) three times in a row before formatting and three times in a row after formatting and the
  rebuild.
- Commit `4a29e93` (`4a29e9382f1fc37e44cd0f1f3f7e3c25fed78440`) on `mcla/m11-l109`, parent `5e65c7a`;
  9 files, 2082 insertions, 50 deletions (`transfer_diagnostics.h` 977 new, `transfer_elision.h` +390,
  `command_processor.cpp` 95, `render_target_cache.cpp` 160, the two headers 31, tests 479). Not pushed,
  no tag.
- Install `private/l109-sdk-install`, CMake `0.20.0.17-dev.g4a29e93`; adds
  `include/rex/graphics/util/transfer_diagnostics.h`.

| bin file | Bytes | SHA-256 |
|---|---|---|
| rexruntime.dll | 11776000 | 7D5F8B0ACF48782A3C441E570C8B43D432390993F84D4836A5CBD2383721CE69 |
| rexgpu-xenos.dll | 3777024 | 94C75488B4D14BA8D446368CD3199A56A678C11DEBC731F412AA6EE0F4EC6DFA |
| TracyClient.dll | 246784 | 7609ED6F921CC33EE3B7B14038EC5CDDCE47C0E4617CB1291EF733C62032933E |
| rexglue.exe | 4113920 | F4FFF9AE177FD5DCFEE9923FCBEF5ED96BEB2265AE78B1F68C257EF2FA68522F |

rexruntime.dll and TracyClient.dll have no source change but new hashes (fresh build tree, version
string). **ABI:** `D3D12RenderTargetCache` and `D3D12CommandProcessor` changed: the host must be
rebuilt against this SDK. New SDK cvars: `d3d12_transfer_trigger_profile`,
`d3d12_stencil_transfer_lazy_verify` (both init-only, default off).
- Scratch: `%LocalAppData%\Temp\claude\C--BDU-MCLA-Recomp\l109-work` (`sdk-build.ps1`, `sdk-test.ps1`,
  `sdk-install.ps1`, `ctest3.ps1`, `build_then_test.ps1`, `copy_tp.ps1`, `normwarn.py`, `prove.txt`,
  `splice_prove.py`, `edit_cp.py`, `edit_rtc.py`, `edit_l108_test.py`, `sdk-commit-msg.txt`; logs
  `sdk-configure.log`, `sdk-build-ut1.log`, `sdk-build1.log`, `sdk-build2.log`, `ctest-a1..3.log`,
  `ctest-b1..3.log`, `sdk-install.log`, `w109all.txt`).

## 7. Risks

- No game run. The exact coverage criterion and the clip-plane rule are new parts of the proof: the
  audit (Run 1, `bad` must stay 0) comes before any elision run.
- Partial proofs cut the transfers' inner part and keep the 1-pixel border: correct only if the
  sample positions are the standard ones (the render targets are created with quality 0, which the
  SDK already treats as the standard pattern elsewhere); the verify measures it.
- Diagnostics cost CPU only while their switches are on (a map lookup per evaluated draw /
  transfer, a short scan of the pending entries per draw).
- The tracker is conservative (resolves of a kind count for every pending plane of that kind;
  ownership taken by another surface's clear is not seen): `never` is a lower bound.
