# l101 SDK: per-draw levers for the PC dips (2026-11-14)

Implemented by an Opus subagent for the primary agent, from the design in
[l100 PC verify and Reflex analysis](M11-l100-pc-verify-and-reflex-analysis-20261114.md)
(texture memo facts, the verifier false-positive mechanism, the render-target Update inputs),
the owner priority in [l100 PC dips](M11-l100-pc-dips-analysis-20261114.md) (sustained CP-bound
dips driven by draw count), the cp-reg skip and verify as built in
[l100 SDK](M11-l100-sdk-deck-levers-20261114.md) and MCLA_HITCH as built in
[l99 SDK hitch](M11-l99-sdk-hitch-20261113.md). SDK commit `a49e511`
(`a49e5113928fd8d6164ee59d67ab50ba2f2319e8`) on the local branch `mcla/m11-l101`, parent
`1e81149` (l100); worktree `private/l101-sdk-wt`; not pushed. Installed to
`private/l101-sdk-install` (CMake version `0.20.0.11-dev.ga49e511`; the number moved from
`.10` because a `v0.20.0.11` tag is now an ancestor in the SDK repository, not because of this
commit). **Built and unit-tested only: nothing has run in the game, there is no physical PASS,
and no number in this document is measured with l101.** Every lever is default off; the new
log lines and detectors are always on. Nothing was taken from GPL forks or LARecomp; no code
was ported from Xenia Canary or macXenos. Spark was not used: deterministic source work
checked by unit tests. Owner build: [l101 owner build](M11-l101-owner-build-20261114.md).

## 0. What shipped and what did not

| Item | Status | Switch (default) | Host key |
|---|---|---|---|
| T1 texture fetch memo v2 | **shipped** with its verify (the existing one) | `d3d12_fetch_identity_v2` (false); verify `d3d12_fetch_identity_verify` (false, existing) | `tex_memo_v2`; verify = existing `fetch_identity_verify` |
| T1 memo miss reasons, bump reasons | **shipped**, always on | - | - |
| T2 cp-reg verifier split | **shipped** (counters only; the skip is unchanged) | - | - |
| T3 render-target Update memo | **shipped** with its verify | `d3d12_rt_update_memo` (false), `d3d12_rt_update_memo_verify` (false, wins) | `rt_memo`, `rt_memo_verify` |
| T4 PSO handle reuse | **not done** (optional; see section 6) | - | - |
| T5 MCLA_DIP detector | **shipped**, always on | - | - |
| T5 MCLA_HITCH threshold | **changed**: max(35 ms, 1.6 x median) (was 50 ms, 2 x) | - | - |

Nothing became verify-only. T3 ships its fast path because the no-op argument (section 3.2)
holds on every path the source shows and the verify compares the full Update's effect with
the skipped state at every would-be hit; it stays default off and the checklist runs
`verify-all` first.

## 1. T1: texture fetch memo v2 (`d3d12_fetch_identity_v2`)

### 1.1 The memo (include/rex/graphics/util/fetch_identity.h)

`FetchIdentityMemo2Way<Payload, kLog2Sets = 11>` (line 257): 2048 sets x 2 ways = 4096
entries (v1: 512 direct-mapped). Same contract as v1: an entry is valid only while its
generation stamp equals the current generation; a payload is stored with the generation read
before the lookup. `Find` refreshes the set's LRU bit (so it is not const). `Store` replaces,
in this order: the way that already holds these dwords (no duplicates); else a way that is
empty or stamped with another generation (the LRU one if both are); else the LRU way.

### 1.2 No generation bump on texture creation, and why that is sufficient

v1 bumped the global generation on every texture creation (texture/cache.cpp, "conservative";
4.2 bumps per frame on the l100 PC, the hit rate fell to 52-66 %). A memo payload is
(key, host swizzle, signs, texture pointers) for 6 raw fetch dwords; the textures are what
`FindOrCreateTexture(key)` returned. That function's result for a key depends on: the key, the
host size limits (constant), `IsScaledResolveSupportedForFormat` (pure), the scaled-resolve page
bits (`IsRangeScaledResolved`; their changes bump, `MarkRangeAsResolved` and the scaled-resolve
watch callback) and the `textures_` map lookup. Creating a texture adds a map entry for a key
that was not present; it cannot change the lookup of any other key, and an entry is only
stored with textures that exist (failures are never memoised). The only ways an existing
texture pointer becomes wrong are its destruction (eviction in `CompletedSubmissionUpdated`,
`DestroyAllTextures`) and the scaled-resolve change, and both keep their bumps. So with v2 a
creation is counted (`creates_nobump`) instead of bumping. A switch of `d3d12_fetch_identity_v2`
bumps once (reason `other`), so neither memo keeps an entry across a switch.

### 1.3 Verify

The existing `d3d12_fetch_identity_verify` is storage-agnostic: at every memo hit it runs the
full decode and `FindOrCreateTexture` and compares key, signs, host swizzle and both texture
pointers with the payload (counting `tex_memo_mismatch` and using the slow result). It covers
v2 unchanged; with v2 on it checks exactly what the removed creation bump protected.

### 1.4 Miss reasons and bumps (always on), MCLA_TEXMEMO v=1

Every miss is classified (`ClassifyTexMemoMiss`, fetch_identity.h:151; v1 and v2 both):
`generation` (an entry with these dwords exists but is stale: would have hit without the bumps
since), `empty` (no way ever stored), `conflict` (every way holds a live entry of other dwords:
capacity / index conflict), `new_dwords` (room, but these dwords are not stored: new content).
Bumps are counted by reason (`create`, `destroy`, `resolve`, `other`; atomics, the
scaled-resolve callback can run on another thread). One line per perf-split window (same
reason and seq as MCLA_PERF_SPLIT, after MCLA_CP_L100):

`MCLA_TEXMEMO v=1 reason= seq= enabled= v2= verify= entries=512|4096 ways=1|2 lookups= hits=
miss_generation= miss_empty= miss_conflict= miss_new_dwords= mismatch= bumps_create=
bumps_destroy= bumps_resolve= bumps_other= creates_nobump=`

and `MCLA_TEXMEMO v=1 kind=config enabled= v2= verify= entries= ways= frame=` at start and at
every change. On the default arm (v1) `miss_generation` and `bumps_create` tell how much v2
can gain from the removed bump, `miss_conflict` how much from the capacity.

## 2. T2: the cp-reg verifier split (no change to the skip)

`ConstantSkipVerifier::OnUploadClassified` (cp_l100_levers.h:239) classifies a `kMismatch` by
size (`ClassifyCbMismatch`, `CbMismatchClass`, line 188): `new_size_zero` (the new upload is
empty), `size_changed_nonzero`, `same_size_diff_bytes`. The three upload sites
(UpdateBindings: float vertex, float pixel, bool/loop) count `cb_mm[kind][class]`; the total
`cb_mismatch` is kept and equals the sum. MCLA_CP_L100 v=1 appends
` cb_mm_new_size_zero=vf:N,pf:N,bl:N cb_mm_size_changed_nonzero=vf:N,pf:N,bl:N
cb_mm_same_size_diff_bytes=vf:N,pf:N,bl:N` (vf/pf float vertex/pixel, bl bool/loop; only these
three buffers are verified, so there is no "other" kind).

**Should the shadow be marked in the empty-stage case? No.** The normal path does not mark a
buffer when a stage's constant map becomes empty (`if (float_constant_count_vertex)`,
`if (float_constant_count_pixel)`, the memset for a null pixel shader); the empty upload in the
l100 mechanism only happens because a write the skip would have skipped marked the real buffer.
With the skip on, that buffer stays up to date and the old buffer stays bound; the draw reads
no float constants. So the shadow (which models the skip) is right not to be marked, and
`new_size_zero` is a benign class by construction, not a stale draw. `size_changed_nonzero`
should not happen (a map change between non-empty maps marks the shadow too);
`same_size_diff_bytes` is the only class that would be a real stale draw. Gate for cp-reg:
`same_size_diff_bytes` (and `size_changed_nonzero`) 0 on a verify run.

## 3. T3: render-target Update memo (`d3d12_rt_update_memo`)

### 3.1 Predicate (include/rex/graphics/util/cp_l101_levers.h, `RtUpdateMemo`)

A draw skips `render_target_cache_->Update` (the base part and the D3D12 part: transfers,
resolve clears, binding) when, in this order (the first failing reason is counted):

| Miss reason | Rule |
|---|---|
| `path` | host render-target path only (never pixel-shader interlock: its Update places barriers and marks the EDRAM buffer every draw) |
| `cpu_vs` | `PA_CL_CLIP_CNTL.clip_disable` = 0: a clip_disable draw may get its extent from the vertex shader run on the CPU (reads guest vertex memory): never memoised, never recorded |
| `first` | a successful previous Update is recorded (a failed Update or a switch invalidates) |
| `inputs` | every input equal, bit for bit (below) |
| `epoch` | `RenderTargetCache::update_epoch()` equals the value read after the previous Update |
| `copy` | no resolve (IssueCopy) since |
| `submission` / `frame` | same submission and frame |
| `transfers` | the previous Update produced no ownership transfer |

Inputs (`RtInput`, line 106; gathered in `GatherRtUpdateInputs`): the Update arguments
`is_rasterization_done`, normalized RB_DEPTHCONTROL, normalized colour mask and the vertex
shader; the cvar `mrt_edram_used_range_clamp_to_min`; registers RB_SURFACE_INFO, RB_DEPTH_INFO,
RB_COLOR_INFO, RB_COLOR1_INFO, RB_COLOR2_INFO, RB_COLOR3_INFO (Update), PA_SC_WINDOW_OFFSET,
PA_SC_WINDOW_SCISSOR_TL, PA_SC_WINDOW_SCISSOR_BR, PA_SC_SCREEN_SCISSOR_BR, PA_CL_CLIP_CNTL,
PA_CL_VTE_CNTL, PA_SU_SC_MODE_CNTL, PA_SU_VTX_CNTL, PA_CL_VPORT_YSCALE, PA_CL_VPORT_YOFFSET
(DrawExtentEstimator::EstimateMaxY, the clip_disable = 0 branch). Constant for the cache's
lifetime and not compared: path, resolution scale, maximum render-target width, the gamma
storage choice (`gamma_render_target_as_unorm16_`, set at initialisation) and the host depth
encoding.

### 3.2 Why a hit is a no-op

With identical inputs Update computes the same keys and EDRAM ranges;
`GetOrCreateRenderTarget` finds the render targets the previous Update created;
`ChangeOwnership` finds every range already owned by the same key (no split, no claim, no
transfer: the ranges of one Update are disjoint, each clamped to the next base); the used and
accumulated render-target arrays are rebuilt identical (the accumulated check passes for
unchanged pointers and surface info); the D3D12 part has no transfer and no clear, every
resource state is already the draw state, and the command-list binding is still valid (only a
new submission, a transfer or other render targets clear it). `last_update_transfers_` is
already empty (the `transfers` rule). The epoch makes the "nothing changed since" part
explicit.

### 3.3 Epoch audit (`update_epoch_`, bumped by)

Base `RenderTargetCache`: `InitializeCommon`, `DestroyAllRenderTargets` (and so shutdown),
`ClearCache`, `ResetAccumulatedRenderTargets` (BeginFrame), render-target creation in
`GetOrCreateRenderTarget`, every `ChangeOwnership` call with a non-zero length (also inside
Update and the resolve clears; conservative), the EDRAM-snapshot ownership reset,
`PixelShaderInterlockFullEdramBarrierPlaced`. D3D12: `BeginSubmission`, `Resolve` (entry),
`RestoreEdramSnapshot`, `DumpRenderTargets`, `PerformTransfersAndResolveClears` (entry),
`InitializeTraceSubmitDownloads`, `InitializeTraceCompleteDownloads`. Audit basis: every
`SetResourceState` / `D3DOMSetRenderTargets` / `are_current_command_list_render_targets_valid_`
site of render_target_cache.cpp lies in BeginSubmission, PerformTransfersAndResolveClears,
RestoreEdramSnapshot, SetCommandListRenderTargets (inside Update) or DumpRenderTargets; every
`ownership_ranges_` / `render_targets_` mutation in the base file is listed above; the command
processor's only other render-target-cache calls are getters, descriptor writers and
`CompletedSubmissionUpdated` (pool reclaim only). The epoch is read after each full Update, so
bumps inside it do not prevent the next hit.

### 3.4 Verify (`d3d12_rt_update_memo_verify`, wins over the memo)

For every draw the memo would skip, the full Update runs and the state before and after is
compared (`CompareRtUpdateStates`, line 217): `failed` (Update returned false), `bound` and
`formats` (GetLastUpdateBoundRenderTargets), `key` (LastUpdateRenderTargetKey: the accumulated
objects), `used` and `depth` (the used render targets), `transfers` (non-empty), `cmds`
(command-list commands written, a new DeferredCommandList counter, or pending barriers
changed). A correct skip leaves the state before as it is, so every kind must stay 0. The
shader modifications are computed before Update from registers and are not skipped, so there
is nothing of theirs to compare.

### 3.5 MCLA_CP_L101 v=1

Per perf-split window after MCLA_TEXMEMO: `MCLA_CP_L101 v=1 reason= seq= rt_memo= rt_memo_verify=
rt_lookups= rt_memo_hits= rt_verified=
rt_miss=path:N,cpu_vs:N,first:N,inputs:N,epoch:N,copy:N,submission:N,frame:N,transfers:N
rt_mm=failed:N,bound:N,formats:N,key:N,used:N,depth:N,transfers:N,cmds:N`, and
`MCLA_CP_L101 v=1 kind=config rt_memo= rt_memo_verify= frame=` at start and at every change.
With both switches off the memo costs one branch per draw (no counters).

## 4. T5: MCLA_DIP v=1 and the MCLA_HITCH threshold (include/rex/perf/frame_dip.h)

At every frame end the CP adds the frame interval (the MCLA_HITCH interval) to a one-second
accumulator. When a second is complete: its fps against the median fps of the last 30 seconds
(needs 10); a dip opens when the second is more than 20 % below (fps < 0.8 x median) and
closes at the first second that is not. Seconds inside a dip do not enter the baseline for
the first 60 seconds of the dip, then they do (a lasting new level becomes the baseline). The
per-second values are the MCLA_HITCH counters (`HitchCounters`) as deltas over the second,
divided by its frames:

`MCLA_DIP v=1 kind=second seq= sec= frame= fps= median_fps= ratio= cause_hint= frame_ms=
draws= base_draws= cp_busy_ms= cp_work_ms= gpu_busy_ms= guest_sim_ms= ll_sleep_ms=
strict_wait_ms= fence_wait_ms= wrm_ms= upload_ms= draw_ms= watch_arm_ms= present_ms= subm=
tex_creates= pso_new= split=` (ms per frame; draws and subm per frame; tex_creates and pso_new
in the second; cp_work = cp_busy - fence_wait - strict_wait), at most 30 per minute (the rest
counted); `MCLA_DIP v=1 kind=summary seq= frame= seconds= duration_s= min_fps= mean_fps=
median_fps= max_draws= cause_hint=` when it closes (the most frequent second cause); and
`MCLA_DIP v=1 kind=minute dips= seconds= logged= suppressed= min_fps= total= open_ratio=0.80
baseline_s=30 lines_max=30` every pacing minute.

cause_hint (`DipCauseHint`, line 155; first rule that holds): `guest` (guest_sim - Reflex sleep
>= 25 % of the frame), `gpu` (gpu_busy >= 90 %), `cp_draws` (cp_work >= 60 % and draws >=
1.25 x the baseline median draws: the l100 PC case), `strict` (strict wait >= 10 % while
cp_work >= gpu_busy), `cp_other` (cp_work >= 60 %), `unknown`. The l100 dip proposal text was
not found in the dips analysis, so these fields and thresholds are this implementation's,
derived from that analysis' table (the l100 dip second classifies as `cp_draws`; unit test).

MCLA_HITCH: `kHitchFloorUs` 50 -> 35 ms, factor 2 -> 1.6 (8/5, integer:
`HitchThresholdUs` = max(35000, 8 x median / 5)); the per-minute budget of 20 lines is kept;
the summary line prints `floor_ms=35 factor=1.6`.

## 5. Tests, build, install

- New `tests/unit/graphics/cp_l101_levers_test.cpp` (12 cases, `[l101]`): memo v2 geometry and
  empty / stale finds; 2-way LRU replacement, overwrite of an existing key, stale-way
  preference; against a reference map (30 seeds x 20000 steps, random bumps: a hit always
  returns the latest value stored at the current generation); v2 against v1 on two conflicting
  keys (v1 0 hits, v2 98 of 100); the miss classes; the no-creation-bump model (20 seeds:
  textures created on miss without a bump, destroyed with a bump: every memo hit equals the
  slow lookup); the MCLA_TEXMEMO lines; the cb mismatch classes (the l100 empty-stage
  mechanism gives `new_size_zero`; the split sums to `cb_mismatch`; `OnUpload` keeps its l100
  result); the RT memo predicate (every input dword, the vertex shader, each context rule and
  the order, transfers, clip_disable never recorded, invalidate); the epoch model (50000 steps:
  a hit only when the modelled Update result is unchanged); the verify comparison (each kind);
  the MCLA_CP_L101 lines.
- New `tests/unit/core/frame_dip_test.cpp` (7 cases, `[l101]`): the one-second accumulator; no
  detection before 10 baseline seconds; open / continue / close and the summary with the
  frozen baseline; the 30-lines budget and the minute reset; the 60-second re-baseline; the
  cause rules (the l100 dip second = `cp_draws`); the second stats and the line formats.
- Updated: `frame_hitch_test.cpp` (threshold max(35 ms, 1.6 x median), summary line),
  `cp_l100_levers_test.cpp` (the MCLA_CP_L100 line with the three appended fields).
- SDK build: fresh `private/l101-sdk-b` (the l100 options), output `private/l101-sdk-o`; the
  first build of the edited tree compiled without error. Normalized warning set 215 entries,
  **0 new** against l100 (3 gone: unused-parameter warnings of IssueDraw that the l100 set had).
- `git-clang-format` against `1e81149` (include, src, tests): applied (wrapping in the new
  headers, the test, command_processor.cpp/.h, cp_l100_levers.h, texture/cache.cpp), rebuilt
  (15 steps), then clean.
- ctest: **100 % passed, 0 failed of 2317 (2313 passed + 4 skipped BitStream; l100 2294 + 4;
  +19 new)** before the format, after it and after the install.
- Install `private/l101-sdk-install`, CMake `0.20.0.11-dev.ga49e511`:

| bin file | Bytes | SHA-256 |
|---|---|---|
| rexruntime.dll | 11680768 | 87B7EBE7F45F6AEA28391B14102F0C313E1E046713C84497B0268C9A16BB8C49 |
| rexgpu-xenos.dll | 3606528 | 196DAC21D6EE9E47CFEA9387B024FF46DCB31746FD290BA92B8AE3DDED1B4FFF |
| TracyClient.dll | 246784 | 137C81421248A4682005D77A74B2C7E62B1EE86E8A6FADBC1ADB5EC0E1538A12 |
| rexglue.exe | 4113920 | 7E8A4BD51B562772F6710D170FF6185727040DE98854E7D1CE9A471F0113BE45 |

File set against `l100-sdk-install`: 1238 -> 1240 files; added
`include/rex/graphics/util/cp_l101_levers.h`, `include/rex/perf/frame_dip.h`; none removed; 29
differ (the 4 bin files, 7 changed public headers, `rex/version.h`, the 2 CMake config files,
12 static libraries, 3 `.pc` files). rexgpu-xenos.dll grows by 29696 bytes; rexruntime.dll keeps
its size; TracyClient.dll and rexglue.exe have no source change (fresh build tree).

**ABI:** `D3D12CommandProcessor`, `TextureCache`, `RenderTargetCache` and `DeferredCommandList`
grew (installed headers): a host must be rebuilt against this SDK (it is, for l101). The host
change uses no new SDK symbol, so the main checkout also builds against the gate SDK (l67).

SDK diff (`git show --numstat a49e511`): 19 files, 2116 insertions, 43 deletions:
include/rex/graphics/d3d12/command_processor.h +35, include/rex/graphics/d3d12/deferred_command_list.h
+3, include/rex/graphics/pipeline/render_target/cache.h +36 -2,
include/rex/graphics/pipeline/texture/cache.h +26 -1, include/rex/graphics/util/cp_l100_levers.h
+68 -4, include/rex/graphics/util/cp_l101_levers.h +333 (new),
include/rex/graphics/util/fetch_identity.h +159, include/rex/perf/frame_dip.h +360 (new),
include/rex/perf/frame_hitch.h +8 -6, src/graphics/d3d12/command_processor.cpp +216 -11,
src/graphics/d3d12/deferred_command_list.cpp +1, src/graphics/d3d12/render_target_cache.cpp +14,
src/graphics/pipeline/render_target/cache.cpp +8, src/graphics/pipeline/texture/cache.cpp +55 -8,
tests/unit/CMakeLists.txt +2, tests/unit/core/frame_dip_test.cpp +194 (new),
tests/unit/core/frame_hitch_test.cpp +16 -10, tests/unit/graphics/cp_l100_levers_test.cpp +4 -1,
tests/unit/graphics/cp_l101_levers_test.cpp +578 (new).

## 6. Not done / risks

- No game run: the memo v2 hit rate, the RT memo hit rate, the dip lines and their cost are
  unobserved. `fast` must follow a clean `verify-all`.
- T4 PSO handle reuse: not done. The analysis gives it -0.4 ms at 7.2 k draws (the smallest of
  the four); a correct reuse needs the pipeline cache's own key inputs (shader translations,
  primitive processing, render-target formats, blend / depth state), which is a separate audit.
- T1: the creation-bump argument relies on `FindOrCreateTexture` having no other input than
  those listed in 1.2; the verify checks every hit, including the texture pointers.
- T3: correctness relies on the epoch audit (3.3). A missed mutation site would show as
  `rt_mm` kinds in `verify-all` (it compares the full Update's effect on the bound state and the
  command list at every would-be hit), but only on the paths a run exercises (not trace
  playback). The memo saves the Update call; `GetLastUpdateBoundRenderTargets` and the census
  key are still computed per draw.
- T2 is counters only; cp-reg stays blocked (arms removed) until `same_size_diff_bytes` and
  `size_changed_nonzero` are seen at 0.
- T5: `cause_hint` thresholds are first guesses from one session's table; the fields are there
  to re-classify offline. The minute line uses the pacing minute (CP thread).
- MCLA_HITCH will log more lines (35 ms floor): still at most 20 per minute.
- Scratch folder `%LocalAppData%\Temp\claude\C--BDU-MCLA-Recomp\l101-work`: `copy_tp.ps1`,
  `sdk-build.ps1`, `sdk-test.ps1`, `sdk-install.ps1`, `t2.py` .. `t8.py`, `t3a.py` .. `t3d.py`
  (SDK edits), `orig/` (pre-edit copies), `normwarn.py`, `fileset_diff.py`,
  `sdk-commit-msg.txt`; logs `copy_tp.log`, `sdk-configure.log`, `sdk-build1.log`,
  `sdk-build2.log`, `w101all.txt`, `ctest1.log`, `ctest2.log`, `sdk-install.log`,
  `ctest-install.log`, `fileset_diff.log`.
