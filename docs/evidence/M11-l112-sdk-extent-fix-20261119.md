# l112 SDK: KI-084 (minimap flicker) and the rectangle draw-extent bound (2026-11-19)

**SDK READY: `private/l112-sdk-install`** (CMake version `0.20.0.19-dev.g3173c2e`). Local commit `3173c2e`
(`3173c2e64fa2aa65fa82d7fd11c349f045698320`) on the branch `mcla/m11-l112` (parent `c403f35` = l110), worktree
`private/l112-sdk-wt`; not pushed, no tag. Implemented by an Opus subagent for the primary agent.
**Built and unit-tested only: nothing has run in the game, there is no physical PASS.** Spark not
used: the work was source reading and deterministic code checked by unit tests; a model answer
would cost more to check than to write.

Licence: no code was copied; the new header and test are our own. Nothing was taken from GPL or
unlicensed sources.

## 0. Result

| Item | Status |
|---|---|
| Root cause of KI-084 at file:line | **not pinned**: two candidate classes remain (section 2). The mechanism that turns the bound into a different picture is narrowed to the tiles the bound leaves with their previous owner (section 1) |
| Fix in the SDK | **none for the root cause**. A safe restriction that keeps the saving does not exist (section 3) |
| Safety fallback | **taken**: l112 sets the bound's default **OFF** in the host (owner build doc). The SDK default was always off |
| New verify / audit | **shipped**: the below audit (`MCLA_XFER_EXTENT ... below_read=`, `MCLA_XFER_BELOW`, `MCLA_XFER_BELOW_READ`). It is on whenever the bound or its verify is on |
| Unit tests | 4 new cases `[l112]` in `tests/unit/graphics/extent_below_audit_test.cpp` (47 assertions) |

## 1. What the bound changes, from the source (c403f35)

- The bound only shortens the claim. In the common `Update`
  (`src/graphics/pipeline/render_target/cache.cpp:594-598` at c403f35), `height_used` is capped by
  the rectangle's rows. The lengths are rounded **outward** to whole tile rows
  (`:651-658`: `ceil(rows * samples_y / 16) * pitch`). So every tile row the rectangle touches,
  partly or fully, is claimed and transferred exactly as before. Hypothesis (b), a boundary row
  cleared but not owned, is **ruled out**: no row is inside the rectangle and outside the claim.
  The l108 elision cut only removes parts of transfers that exist. It never adds tiles beyond the
  bound (`render_target_cache.cpp:5385-5453`).
- (c) **ruled out**:
  - The l101 RT memo never skips a clip-disabled draw. It also records such a draw as invalid
    (`cp_l101_levers.h:252-254`, `:282-283`), so the draw after a bounded one always runs the full
    `Update`.
  - The per-draw extent and elision state is reset for every draw (`SetXferDrawPrimitive`,
    `include/rex/graphics/d3d12/render_target_cache.h:85-95`).
  - Colour and depth at the same base are resolved before the bound applies (`cache.cpp:534-547`).
    The bound applies to all bound render targets alike, and a rectangle that writes only one
    plane makes the other plane's claim larger than needed, never smaller.
- (d) **ruled out**:
  - Host depth store and transfers read only ranges the source owns (`ChangeOwnership`,
    `cache.cpp:1312-1460`).
  - Resolves dump each range from its current owner (`GetResolveCopyRectanglesToDump`,
    `cache.cpp:870-948`). `direct_host_resolve` (default true) still dumps the same way
    (`render_target_cache.cpp:7063-7108`).
  - `ClearCache` keeps every render target that owns a range (`cache.cpp:365-397`), and the EDRAM
    snapshot restore claims the whole EDRAM.
- **What does change.** Without the bound, a clear claims the whole old extent. That extent is
  often the rest of the EDRAM, because the clears' scissor is 8192 rows high (l110 section 1). The
  clear transfers the previous owner A's data into its own render target M, and M becomes the
  owner. With the bound, those tiles stay with A. A later reader then gets A's data directly
  (A -> reader) instead of a copy of it (A -> M -> reader). A resolve dumps A's data, not M's copy.
  Both are equal only if every transfer between the formats involved is exact. Two ways they can
  differ are a non-exact pair, or a reader that does not go through ownership. This is the only
  semantic change, and the measured saving consists of exactly these transfers. So no rule can
  keep the saving and leave the data provably unchanged (section 3).

## 2. KI-084 and the evidence

**Owner reports.**
- l111 pure defaults, RTX 5060, 1080p target (guest 960x540 x2, forced HiDef), session 4fc85a45, in
  a race with opponents. The owner's correction of 2026-11-19: for a moment the minimap was a
  **white square** (the whole quad, alpha included, so the round mask was lost too), then normal
  again; it happened now and then, not every frame.
- The F10 frames were taken after a flicker and show the minimap intact (different backgrounds
  behind a translucent disc), so they show nothing about the defect.
- A/B on the same profile: pc/extent-off (session 30e44db9, only `d3d12_draw_extent_from_rect`
  off) showed **no flicker**, with GPU 18.7 against 13.4 ms per frame.

**From the logs.** The trigger profile and category timing were off in both sessions, so the
sequence can only be partly rebuilt:
- The minimap surfaces are small 1x colour targets with a 4x depth alias at the same EDRAM base.
  `MCLA_GPU_SURF last_frame_keys` lists `p160m1c945`, `p80m4c945d945`, `p160m1c961`,
  `p80m4c961d961` and `p160m1c977`.
- Without the bound (session 30e44db9, `MCLA_XFER top`), each frame shows this chain of whole
  EDRAM tail claims by clears (each about 5.2 M host samples):
  - `d1m4p12b0>c0m1p2b945`
  - `c0m1p2b945>d0m4p2b945`
  - `d0m4p2b945>c0m1p2b961`
  - `c0m1p2b961>d0m4p2b961`
  - `d0m4p2b961>c0m4p4b288` (twice)
- So the 1x colour clear at 945 (pitch 2 tiles) claims 1024 tiles, from tile 945 to tile 1969.
  That covers the next minimap surfaces (961, 977) and the scene colour (`c3m4p24b960`, base 960,
  overlapping the last tile of 945's 16).
- With the bound (session 4fc85a45), these signatures are gone from the top list. The bounded
  clears take only their 16 tiles (`MCLA_XFER_EXTENT`: 22 bounded draws per frame, rows 124.8 M ->
  8.6 M per minute, `check=n:0`, the verify was off).
- `MCLA_XFER_REFUSE` names the minimap clears:
  - `none.partial.m4s2x2.w-zs ... r=0,0,80,128;sc=0,0,160,16384`: a 4x depth/stencil clear of
    40x64 guest pixels on pitch 80, four per frame.
  - `none.partial.m1s2x2.wc-- ... r=160,0,256,256;sc=0,0,320,16384`: a 1x colour clear of columns
    80-128, rows 0-128 on pitch 160, two per frame.
  - In EDRAM samples, these two cover the left tile column and part of the right one: a 128x128
    texture cleared through two aliases.
- A white square with the alpha included means the whole minimap texture held another surface's
  data for that frame. The depth/stencil clear value of the 4x alias read as 8888 colour
  (depth 1.0 = 0xFFFFFF) is one plausible source.

**Candidate classes that remain (confidence).**
1. **Readers of left-behind tiles (medium).** A resolve or a later claim reads tiles that a minimap
   clear's bound left with another owner. With the bound, the data comes straight from that owner
   instead of from the clearer's copy:
   - The white square fits a resolve whose span covers tiles another minimap alias (or the scene
     colour at 960) still owns, i.e. the minimap target's ownership was never re-established
     before the resolve.
   - "Intermittent" fits the owner layout of 945-993 changing with what else is drawn in a race.
     Without the bound, the large claims rebuilt the same layout every frame.
   - The source shows nothing in Xenia's ownership logic that would read stale data. The
     difference must therefore come from transfer composition (A -> reader not equal to
     A -> M -> reader) or from the game relying on data that only the old whole-EDRAM claims
     supplied.
2. **A wrong bound in race-only rectangle draws (low to medium).** Every geometric verify so far
   (l110/l111: 103,904 checks, `bad=0`) ran outside races with opponents. A race-only rectangle
   draw could have a CPU-interpreted rectangle that differs from the GPU's, e.g. vertex data
   changed between interpretation and use. The minimap's content would then be drawn outside the
   tiles its target owns, and lost at the next claim or resolve.

**The single owner run that separates them.** Run 2 of l112: `pc/extent-verify2` in a race with
opponents, 5 minutes.
- `MCLA_XFER_EXTENT ... check=bad>0`, or any `MCLA_XFER_EXTENT_VERIFY result=bad` line: class 2,
  and the verify names the rows and scissor.
- `bad=0` but `below_read=resolve:>0` (or `transfer:`) with `MCLA_XFER_BELOW top=` and
  `MCLA_XFER_BELOW_READ` naming the minimap keys (`c0m1p2b945` / `d0m4p2b945` / `b961` / `b977`):
  class 1, and the lines name the clearer, the owner left in place and the reader.
- Run 3 (`pc/extent-new`, the bound on) reproduces the flicker with the same audit active.

## 3. Why there is no SDK fix in l112, and what the restriction rule would be

- A restriction that is provably unaffected must leave every later read with the same data with
  and without the bound. By section 1, that holds only for left-behind tiles whose owner is the
  clearer itself or nobody. These tiles are exactly the ones that never needed a transfer, so the
  restricted bound saves nothing.
- Every tile that does save a transfer is one where A -> reader replaces A -> M -> reader.
- Other rules (by pitch, by format pair, by MSAA) are not provable without the root cause. They
  would only be bisect arms, so none was added to the SDK.
- The fix therefore waits for Run 2's audit. When the reader is known (e.g. a resolve of a minimap
  target), a principled fix can claim, at the bounded draw, the tiles that reader will use.
- **Default:** the SDK default of `d3d12_draw_extent_from_rect` stays false (it always was). The
  l112 host stops setting it (owner build doc).

## 4. The below audit (new, `include/rex/graphics/util/extent_below_audit.h`)

- **Recording** (common `Update`, `cache.cpp`):
  - For every render target of a bounded `Update` (the bound applied, or computed for the verify),
    the tiles between the bounded and the estimated length are gathered before the ownership
    changes (`GatherBelowPending`), one range per owner other than the clearer and not empty.
  - They are recorded after the `Update`'s own claims (`ExtentBelowAudit::Leave`). In verify mode
    the unbounded claim itself is therefore not counted.
  - A newer range replaces an older one over the same tiles.
  - Capacity is 512 ranges; the oldest are dropped (`overflow`).
- **Reads:**
  - every `ChangeOwnership` (`OnClaim`) is classified:
    - `draw`: the clearer itself, later (bound: a transfer from the owner now; old way: none)
    - `owner`: the owner taking the tiles back
    - `transfer`: another render target
    - `rclear`: a resolve clear (`PrepareHostRenderTargetsResolveClear`)
  - The claimed part is removed: after the claim, the owner is the same with and without the
    bound.
  - Every resolve copy on the host render-target path reports its EDRAM tile span
    (`GetCopyEdramTileSpan`) once (`AuditResolveRead`, before the direct or the dump path), and
    each overlapping range counts as `resolve`. A resolve removes nothing.
- **Lines:**
  - `MCLA_XFER_EXTENT` gains
    ` below=left:,tiles:,live:,overflow: below_read=draw:,owner:,transfer:,rclear:,resolve: below_tiles=...`
    (window totals; `live` = ranges held). The l110 fields and their order are unchanged, still
    `v=1`.
  - After it: `MCLA_XFER_BELOW v=1 reason= seq= sigs= overflow= top=kind.clearer>owner>reader:n:tiles|...`
    (the 12 most frequent; keys as in `MCLA_XFER`, resolves as `res@b<base>`).
  - The first 32 events of the session:
    `MCLA_XFER_BELOW_READ v=1 read= clearer= owner= reader= tiles=<start>+<n> age=` (`age` =
    bounded draws since the range was left).
- **Cost:** a map lookup per `ChangeOwnership` and per resolve while ranges are held. Nothing at
  all while the bound and the verify are off: no range is ever left, and `OnClaim` / `OnResolve`
  return at once.

## 5. Tests, build, install

- `tests/unit/graphics/extent_below_audit_test.cpp` (4 cases, `[l112]`):
  - the key format against the `MCLA_XFER` signatures of the session (`c0m1p2b945`, `d0m4p2b945`,
    `d1m4p12b0`, `c3m4p24b960`, `res@b977`)
  - the failing pattern in the ownership-range arithmetic: the 945 clear leaving 1008 tiles of
    the scene depth; the 4x alias claiming only its own tiles (no read); the 961 surface reading
    16 left tiles (`transfer`); a newer leave replacing the old one; a resolve of the 977 surface
    (`resolve`, nothing removed); the clearer drawing below its bound (`draw`); the owner taking
    tiles back (`owner`); a resolve clear (`rclear`); the remaining tiles
  - ignored leaves (owner = clearer, empty, length 0), the EDRAM wrap for leaves, claims and a
    resolve span, the capacity
  - the exact window fields, the top list, the event line, window reset, `Reset`
- Build: fresh `private/l112-sdk-b` with the l110 options (Ninja Multi-Config Release,
  clang 20.1.8, VS 2022 build tools, FFX presenter shaders, Tracy and perf counters on, Vulkan
  off), output `private/l112-sdk-o`; third-party trees copied from `private/l110-sdk-wt`.
  Normalized warning set: 211 entries (union of the full, post-format and install build logs), **0 new, 0 gone against l110**.
- `git-clang-format` against `c403f35`: applied (`git-clang-format -f`, one hunk in `cache.cpp`) and `clang-format -i` on the two new files; rebuilt; then "clang-format did not modify any files". `git diff --check`: clean.
- ctest (full suite, `-j 8`): **2366 of 2366 passed** (l110 2362 + 4 new) three times in a row before formatting. After formatting and the rebuild: run 1 had one failure in an unrelated test (`unit.Partial mapping collision cleans views before retry and destruction`, `tests/unit/memory/physical_mapping_test.cpp:184`, `reused.base != nullptr`: a host address reuse race, untouched code), runs 2 and 3 passed, and a further three runs in a row passed 2366 of 2366 (logs `ctest-a1..3`, `ctest-b1..3`, `ctest-c1..3`).
- Commit `3173c2e` on `mcla/m11-l112`, parent `c403f35`; 8 files, 646 insertions, 0 deletions (`extent_below_audit.h` 347 new, the test 161 new, common `cache.cpp` +78 / `cache.h` +22, D3D12 `render_target_cache.cpp` +26 / `.h` +7, `command_processor.cpp` +4, tests CMakeLists +1). Not pushed, no tag.
- Install `private/l112-sdk-install`, CMake `0.20.0.19-dev.g3173c2e`; adds
  `include/rex/graphics/util/extent_below_audit.h`.

| bin file | Bytes | SHA-256 |
|---|---|---|
| rexruntime.dll | 11776000 | 246034EEA505B098E6403DAED7C6FBA63FCDE19E7BF43CEFA15EB96CB7D7A5DF |
| rexgpu-xenos.dll | 3825152 | 55A85E6C94B1522E541AC705309CBA4F3086FFD29E9133FD339ED922DADA489D |
| TracyClient.dll | 246784 | FA24D8FF5F71BFB82956D01DC5DA65ECCA41D148CBD317772566522B6F90658A |
| rexglue.exe | 4113920 | C4859B2BE44FAED2A42BC34CF8297F13338F9DBBBF82B6C37E1BE7D554A7F87E |

rexruntime.dll and TracyClient.dll have no source change but new hashes (fresh build tree, version string).

**ABI:** `RenderTargetCache` and `D3D12RenderTargetCache` gained members, so the host must be
rebuilt against this SDK. No new cvars.

## 6. Performance

- l112 by default = l111 with the bound off = the l111 `pc/extent-off` behaviour: elision and
  forced HiDef on, the bound off.
- Owner numbers: GPU 18.7 against 13.4 ms per frame (sessions 30e44db9 / 4fc85a45, RTX 5060,
  1080p); l110 driving minutes 48 -> 66-69 fps with the bound.
- Expected l112 default: about 18-19 ms GPU and l109-level transfers, i.e. the bound's whole
  saving is lost until the fix. The l110 audit rows were 147.0 M -> 17.6 M per minute; the
  bounded rows return to 147 M.
- The audit adds nothing while the bound is off. With pc/extent-new it costs a map lookup per
  claim.

## 7. Risks

- No game run. The audit's counts are only as good as the hook coverage: every ownership change
  goes through `ChangeOwnership`, and every resolve copy through the hooked span. A reader outside
  ownership (none found) would not be counted; the verify's `bad` would not show it either.
- If Run 2 shows `bad=0` and no below reads with minimap keys, neither class explains the flicker,
  and the next step is a trigger-profile run focused on the 945-993 tiles.
- Scratch: `%LocalAppData%\Temp\claude\C--BDU-MCLA-Recomp\l112-work`.
