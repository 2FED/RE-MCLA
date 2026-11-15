# M11 anti-aliasing and resolution-scaling research for the GPU-bound "1080p" mode (2026-11-15)

Read-only research by an Opus subagent for the primary agent. No build, no test, no game
run, no code change. **Spark not used:** reading public sources and our SDK source; checking a
model answer would cost more than the reading. Inputs: `M11-gpu-time-research-20261115.md`,
`M11-l103-pc-verify-and-fast-20261115.md`, `M11-external-repos-survey-20261108.md`,
`M11-macxenos-survey-20261113.md`, `M2-015-xenia-patch-audit.md`, `M8-render-targets-20261011.md`,
the SDK worktree `private/l101-sdk-wt` at `a49e5113928fd8d6164ee59d67ab50ba2f2319e8`, and host `src/`.
Downloads (public raw files and GitHub API only) are in the scratch folder
`%LocalAppData%\Temp\claude\C--BDU-MCLA-Recomp\aa-research-work\dl`.
(Written here because the write to `docs/evidence/` was refused by the session's worktree guard;
the primary agent should copy it to `docs/evidence/M11-aa-and-scaling-research-20261115.md`.)

**Licence rule applied.** RenanQueiroz/mcla-recomp and holdmysocks/mcla-recomp are GPL-3.0;
mzzvxm/larecomp has no licence. From those three only ideas, measurements and guest addresses
are taken here, and no code is quoted. Code reuse is proposed only from Xenia Canary (BSD-3)
and from AMD FidelityFX (MIT) shaders that the SDK already has.

All "expected ms" figures for our RTX 5060 Laptop are **estimates** unless they say
"measured". Their basis is peer measurements on an RTX 5080 desktop (about 2.5-3x faster),
scaled by pixel count (our 1080p = 2.25x native pixels; their 2x = 4x, their 3x = 9x).

## Sources

| Id | Source | Commit / date | Licence | Used for |
|---|---|---|---|---|
| R | github.com/RenanQueiroz/mcla-recomp (`README.md`, `docs/settings.md`, `docs/off-switches.md`, `KNOWN-ISSUES.md`, `patches/README.md`, `config/perf_options.toml`, `config/render_perf.toml`, `launcher/dev/profiling-2026-10-14.md`, `launcher/dev/upscaler-research.md`) | `4a459f62e88a8a9ca09db5683246d434211ce517` (pushed 2026-11-15) | GPL-3.0 | ideas, measurements, guest addresses |
| H | github.com/holdmysocks/mcla-recomp (`config/perf_options.toml`, `src/perf_options.cpp`) | `2ed208199fcab064e67a83c5ab8f7e8f2aece444` | GPL-3.0 | option names, guest addresses |
| L | github.com/mzzvxm/larecomp (`README.md`, `documentation/TECHNICAL_NOTES.md`, `src/gpu/msaa_level.h`, `src/gpu/render_scale.h`, file tree) | `b396c0679dc26233a5235a429fb3f72a19a69314` (2026-11-15) | none (all rights reserved) | ideas only |
| X | github.com/xenia-canary/xenia-canary, branch `canary_experimental` (`gpu_flags.cc`, `render_target_cache.cc`, `d3d12/d3d12_render_target_cache.cc`, `draw_util.cc`, `texture_cache.cc`, `ui/presenter.cc`, `app/emulator_window.cc`, `graphics_system.cc`) | `9eedf606f0c94749e17a370e683021445caedf26` (2026-11-15) | BSD-3 | cvars, port candidates |
| GP | github.com/xenia-canary/game-patches, `545407F8 - Midnight Club Los Angeles (Complete Edition).patch.toml` | `fe957a050e37f5a1e3f485ca43c2e37ee2a9c92d` | no licence file reported by the API | guest addresses (facts) |

Canary commits cited: `74db632ab32e179dd0d6858b51caae07fc444949` (draw_resolution_scale_threshold),
`4ae7c9f538a1e0dc758be29d1d8f8df69e47cb37` (extended 40 MB EDRAM), `437a7280cf95310d518a2f68087aab61403956ac`
(single-sample EDRAM addressing), `cc4981a4659086acb1c25a485adebefc3f432ae1` (Xenos sample positions),
`a635ac64f5ca37c0b789e8b4166b53dc673b213f` (scaled readback through a downscale CS), `ef505fbe3` (Vulkan
no_discard_stencil), `d36b1b383` (gpu_allow_invalid_fetch_constants default true, 2023).

## 1. Peer projects: anti-aliasing, resolution and UI sharpness

### Game facts (guest addresses, from R/H/GP; facts only)
- **MSAA switch.** `0x822E4B80` in the renderer creation (`sub_822E4AB0`): `lwz r11,0x4(r10)`. Forcing r11 = 1
  selects no MSAA (renderer+384 = 1 instead of 2). GP "Disable MSAA" writes `li r11,0x1` (`39600001`). H and R
  implement it as a mid-asm hook `mcla_msaa` at `0x822E4B80` with `jump_address_on_true = 0x822E4B84`. R also
  switches renderer+384 during play (between frames), but sizes the "Shadow Collector" and "Depth Buffer"
  targets once at start-up (`sub_82310E40` -> `sub_825F4750`).
- **Tiling.** `BeginTiledRendering` = `sub_8217A470`. Hook point `0x8217A700` (r7/r8 tile w/h, r18 MSAA mode
  1/2/4, r19 format, r25/r28 target w/h). `EndTiledRendering` = `sub_8217B430`. The game sizes a tile so that its
  colour and its depth take 1,024 EDRAM tiles each: 720p 2x MSAA = two tiles (1280x512 + 1280x208); 4x = three.
  Surface limit check `0x82410E34` (jump `0x82410E48`). The tile-surface cache has 8 slots; a ninth key ends in
  `sub_821BD618`, which never returns.
- **Motion blur.** GP "Disable Motion Blur": `0x8260D0B8` and `0x8260D0D4` (`bl 0x8218a568` -> `li r3,0`).
  Post: `rage::grPostFX` at `0x829054A0`, motion blur setup `sub_8260E9A0` (StreakMotionBlur, quarter-resolution
  buffers), DoF composite `sub_8260EBB8` (R `upscaler-research.md`).
- **Developer switches.** `0x821C06EC` (`blr` of `sub_821C06C8`) registers developer options such as
  `noraceshadows`, `noimpostors` and `nofsblur` (full-screen blur); H/R set the value string there.
- **Frame structure at 3x** (R `profiling-2026-10-14.md`, RTX 5080): main scene `2_10_10_10_FLOAT` + `D24FS8`,
  **2x MSAA**, 1280 wide at 1x. Shadow map 4x MSAA depth-only, small 2x/4x reflection targets, half-resolution
  post, luminance, a full-size `16_16_16_16_FLOAT` pass, final 1x `8_8_8_8` with the HUD at EDRAM tile 720.
  Single 4x MSAA draws on the scene's EDRAM range (clears) force full ownership transfers.
- **No resolution patch exists for MCLA** in GP; the title's back buffer is 1280x720 (our `sub_8217AC30` pixel cap).

### What each project does

| Topic | R (RenanQueiroz, GPL) | H (holdmysocks, GPL) | L (LARecomp, no licence) |
|---|---|---|---|
| Renderer | ReXGlue SDK 0.10.0 + about 80 patches (EDRAM emulation, RTV) | SDK 0.10.0 + 6 patches; PC unmaintained | own native renderer on Plume; no EDRAM emulation (`mcla_native_gfx_nocp`) |
| Guest back buffer | 1280x720 always | 1280x720 | 1280x720 always (`kAssumedScreenWidth/Height`) |
| Resolution | `resolution_scale` 1/2/3 = 720p/1440p/2160p, integer only; "1080p" only as FSR 3.1 Ultra Quality inside 2x | 720p | any scale 0.5x-3x on a 1/20 grid; default = the screen's (so 3x on 4K) |
| MSAA | default 2x (as on the console); `mcla_anti_aliasing` = msaa / fsr / off; off sets renderer+384 = 1 via the hook | `mcla_msaa` (next start) | the game always draws its 2-sample layout; the **host** picks 1/2/4/8 samples (`mcla_gpu_msaa`) and drops to 1 when a temporal upscaler runs |
| Post AA | FSR 3.1 native AA (jitter + motion vectors from depth, vehicle-id stencil and the game's car matrices) | none | FXAA 3.11, SMAA, FSR 3.1/4, DLSS |
| Upscaling | FSR 3.1 scene squeeze: scene drawn in a sub-rectangle of the full-size targets, HUD and post at output size | none | FSR1 presets, FSR 3.1/4, DLSS |
| Present filter | `present_effect` bilinear / fsr (FSR1 EASU+RCAS, `present_fsr_sharpness_reduction` 0.2) / cas (`present_cas_additional_sharpness` 0..1): FidelityFX enabled in their build | bilinear | FSR1 EASU/RCAS, CAS |
| UI sharpness | HUD drawn at the output scale (1440p/4K); with FSR upscaling the HUD stays at output size | n/a | everything at the screen scale; FSR1/CAS sharpening |
| Anisotropy | `anisotropic_override` default 16x, "costs next to nothing" | game | own |

### Peer measurements (RTX 5080 desktop unless noted)
- 3x GPU frame shares before their transfer and resolve patches (profiling 2026-10-14): game draws 32 %, EDRAM
  transfers 32 %, resolves 17 %, texture loads 15 %, uploads 4 % (12.0 ms). At 2x: 51 / 19 / 11 / 8 / 12 % (8.8 ms).
  ROV path at 3x: 25.7 ms against 12.0 ms (RTV).
- **MSAA off** (`mcla_msaa=false`) at 3x: -18 to -25 % (13.2 -> 9.9 ms night city, 12.6 -> 10.2 ms Hollywood) while the
  scene was two tiles; **only -2 to -5 % once single-tile rendering was on**. Visible cost: more aliasing; R #45
  "fences/trees blink without MSAA" was closed as a limitation (alpha-to-mask foliage).
- **Single tile** (EDRAM 4,096 tiles + hook at `0x8217A700`): frames -11 % (city) / -19.7 % (race) at 3x on D3D12,
  GPU -1.0 to -2.2 ms, GPU command thread CPU -28 to -35 %, 30-37 % fewer draws per frame; at native the race -14 %.
- **Clears done as host clears** (`clear-rectangle-draws`): the game clears targets by drawing a 4x MSAA rectangle,
  which forces a transfer of the 2x scene range into a 4x target and back. Before: 3.6 ms of a 13.7 ms GPU frame at 3x
  in the city, 3.3 of 13.2 in a race; 49 % of transferred samples were the clears' targets, 28 % the copies back.
- **Direct resolve** (real `TryResolveCopyDirectly`): resolve time 1.09 -> 0.67 ms at 2x (city), 2.23 -> 1.08 ms at 3x;
  at native 0.48 -> 0.33 ms. Dumps were 60 % of resolve time at 3x.
- **Texture load direct** (decode resolved data straight into the texture): 1.62 -> 1.12 ms at 3x.
- **Skip overwritten transfers** (tone map over the scene's range): 0.26 ms at 3x on the 5080, 7.24 ms at 2x on a
  2-CU iGPU; their implementation does not cut it yet (default off).
- FSR 3.1 upscaling at 3x: Quality 8.59 ms, Performance 8.22, native AA 9.39, MSAA 8.41: small gain, because the
  shadows, transfers, resolves, texture loads and post stay at output size.

## 2. Xenia Canary upstream (BSD-3) against our SDK `a49e511`

| Item | Canary (X, `9eedf606`) | Our SDK | Notes / cost |
|---|---|---|---|
| `draw_resolution_scale_x/y` | int, 1-7, `texture_cache.cc:20-38`; integer only | int 1-8, default 1, `src/graphics/pipeline/texture/cache.cpp:78-85`; launcher sets it | no fractional scale upstream either |
| `draw_resolution_scale_threshold` | uint, default 0, `render_target_cache.cc:153`; commit `74db632ab` (2026-07-16): render targets with pitch <= N px stay 1x (bloom/DoF chains), per-surface scale class in `RenderTargetKey`, transfers with separate source/destination scale, 1x resolves go straight to shared memory; host RT only | **absent** | port (about 500 lines over about 15 files); lever for 2.25x post passes |
| MSAA vs scale | guest sample count kept; host samples = guest count at every scale. No "MSAA off" cvar. `debug_msaa_2x_as_4x` (false, `render_target_cache.cc:24`) | `native_2x_msaa` true, `src/graphics/flags.cpp:21` (same thing, inverted name) | already the cheap setting |
| Sample positions | `xenos_sample_positions` (false, `gpu_flags.cc:129`; `cc4981a46`) | absent | correctness, not speed |
| EDRAM layout | `437a7280c` single-sample addressing across 1x/2x/4x views (rewritten transfers/resolves) | old layout | correctness; large |
| Extended EDRAM | `edram_tile_count` standard/extended (8192 tiles), `render_target_cache.cc:216`; `4ae7c9f53`, aimed at native 1080p/1440p patches; host RTs grow as needed | absent (`kEdramSizeBytes` fixed, `d3d12/render_target_cache.cpp:209-210`) | port base for single-tile rendering (with R's idea for the hook) |
| `postprocess_antialiasing` none/fxaa/fxaa_extreme | `app/emulator_window.cc:74` | present as **`swap_post_effect`** none/fxaa/fxaa_extreme, default none, `src/graphics/graphics_system.cpp:39`, restart; runs in the guest-output refresh at guest output size (`d3d12/command_processor.cpp:2958-2996`) | exists, not exposed |
| `postprocess_scaling_and_sharpening` bilinear/cas/fsr; `postprocess_ffx_cas_additional_sharpness` (0), `postprocess_ffx_fsr_max_upsampling_passes` (4), `postprocess_ffx_fsr_sharpness_reduction` (0.2), `postprocess_dither` (true) | `emulator_window.cc:91-147` | present as **`present_effect`** (`src/ui/presenter.cpp:45`), `present_cas_additional_sharpness` (`:50`, 0.0), `present_fsr_max_upsampling_passes` (`:57`, 4), `present_fsr_sharpness_reduction` (`:63`, 0.2), `present_dither` (`:81`, false); **compiled only with `REX_HAS_FIDELITYFX_SDK`**, otherwise bilinear only (`:76`) | see section 3a |
| `native_stencil_value_output` | true, `render_target_cache.cc:184`: "otherwise much more expensive drawing of 8 quads" | true, `d3d12/render_target_cache.cpp:50`; used only if `IsPSSpecifiedStencilReferenceSupported` (`:376-380`); NVIDIA: no | **no upstream fix for NVIDIA**; same 8 passes (`d3d12_render_target_cache.cc:4402-4436`) |
| `native_stencil_value_output_d3d12_intel` | false | false, `:43` | Intel only |
| `no_discard_stencil_in_transfer_pipelines` | false, `gpu_flags.cc:95` (D3D12 `d3d12_render_target_cache.cc:4262`; Vulkan `ef505fbe3`) | absent | debug knob: without the discard every bit pass writes its bit everywhere (REPLACE, ALWAYS), so stencil becomes wrong; not a fix |
| `depth_transfer_not_equal_test` | true | true, `flags.cpp:25` | same |
| `resolve_resolution_scale_fill_half_pixel_offset` | true, `draw_util.cc:23` | true, `src/graphics/util/draw.cpp:31` | correctness, no cost |
| Direct resolve | **none upstream** (no direct host resolve in Canary) | `direct_host_resolve` true (`pipeline/render_target/cache.cpp:40`) but `TryResolveCopyDirectly` is a stub that dumps (`d3d12/render_target_cache.cpp:5936-5981`) | new code (R's idea) |
| Readback of scaled resolves | `a635ac64f` downscale CS (from Xenia Edge) | `readback_resolve` "none" (`command_processor.cpp:58`), `readback_resolve_half_pixel_offset` false (`:67`); our read-back is already 74 % elided, 0.49 ms | minor |
| `gpu_allow_invalid_fetch_constants` | **true** since `d36b1b383` | false, `flags.cpp:16` | not a GPU-cost lever |
| `render_target_path_d3d12` | "" -> RTV except Intel; ROV "much slower now" | same, `d3d12/render_target_cache.cpp:46-48`, `:179-206` | R: ROV 2.1x slower at 3x |
| `depth_float24_convert_in_pixel_shader` / `_round` | false / false; convert "costly ... 2 or 4 times heavier with MSAA" | false / false, `flags.cpp:22-23` | already off |
| `anisotropic_override` | -1 (none) | **3 (4x forced)**, `texture/cache.cpp:64` | small |
| `custom_internal_display_resolution_x/y` | guest video mode up to 1920x1080, `graphics_system.cc:29-33` | not checked as a cvar here; our launcher sets the guest mode | 1080p guest still needs EDRAM space |

## 3. Our backend: assessment

### Exposed or not
Present in the SDK but **not** host dev-override keys (`src/mcla_dev_overrides.h` has none of them; keys are
`name=true/false`, with string-valued convenience keys like `shared_memory_upload_ahead_64` at `:672-675`):
`swap_post_effect`, `present_effect` and its three sharpness keys (and only with `REX_HAS_FIDELITYFX_SDK`),
`present_dither`, `anisotropic_override`, `native_2x_msaa`, `render_target_path_d3d12`,
`native_stencil_value_output`, `direct_host_resolve`, `depth_transfer_not_equal_test`, `depth_float24_*`,
`gamma_render_target_as_unorm16` (true, `flags.cpp:27`), `half_pixel_offset` (true, `util/draw.cpp:29`),
`resolve_resolution_scale_fill_half_pixel_offset`, `draw_resolution_scaled_texture_offsets`
(true, `pipeline/shader/dxbc_translator_fetch.cpp:27`), `mrt_edram_used_range_clamp_to_min`,
`gpu_render_audit` (false, `flags.cpp:18`).

### (a) FXAA / CAS / FSR1 in the presenter
- The CAS and FSR1 (EASU + RCAS) pixel shaders are **already committed as bytecode**
  (`src/ui/shaders/bytecode/d3d12_5_1/guest_output_ffx_cas_*`, `guest_output_ffx_fsr_*`). They are compiled out
  because `REX_HAS_FIDELITYFX_SDK` is defined only when `REXGLUE_ENABLE_FIDELITYFX=ON` fetches the FidelityFX SDK
  (`src/ui/CMakeLists.txt:114-120`, `cmake/rexglue_fidelityfx.cmake:48-55`); our staging sets it OFF
  (`scripts/stage-profile-name-sdk.ps1:119`). The gated code under `REX_HAS_FIDELITYFX_SDK` uses only the bytecode
  headers and the presenter's own constant setup. The `ffx_api` includes and calls sit under the separate
  `REX_HAS_FIDELITYFX_RUNTIME` gate (`presenter.cpp:25-28`, `:149-224`; `d3d12_presenter.cpp:29-33`, `:82-180`).
  **Inference to check by compiling:** defining `REX_HAS_FIDELITYFX_SDK=1` without the fetch gives `present_effect`
  cas and fsr. Choosing fsr2/fsr3 there must stay unusable (no runtime).
- Cost: one full-screen pass at 4K output per EASU step (720p -> 4K = 2 steps), plus RCAS. It runs in the presenter's
  list, outside `gpu_busy` but on the same GPU. Estimate +0.3-0.8 ms on the 5060. It sharpens the HUD/menus and the
  scene alike. This is the cheap UI-sharpness fix. It makes **720p + FSR1** a candidate for the owner's 4K panel:
  GPU 9.0 ms measured against 17.8 ms (a5cbe38e against fb4bf9d9), fps 66 against 51.5.
- FXAA (`swap_post_effect`) already exists. It runs at the guest output size (1920x1080 at "1080p") before the
  presenter and blurs HUD text slightly. Use it only together with MSAA off.

### (b) Guest MSAA to 1 sample at scale 2x2
- **Host-side "ignore MSAA" mode: no upstream precedent** in Canary. In our EDRAM path the guest sample count is part
  of the guest layout: 2x doubles rows and 4x doubles both axes in EDRAM tiles, pitches and tile counts are guest
  facts, resolves average the samples, dumps and transfers are per sample count, and alpha-to-mask writes coverage.
  A host 1-sample target in a 2x layout would need new dump, transfer and resolve shaders that replicate one sample.
  L can do it only because it has no EDRAM emulation (`src/gpu/msaa_level.h`: host level independent of the
  game's 2-sample layout).
- **Game-side switch is the practical route:** a mid-asm hook at `0x822E4B80` (r11 = 1, jump `0x822E4B84`). Our
  codegen already supports `[[midasm_hook]]` (`config/mcla_functions.toml:46+`), so this means a codegen and
  host rebuild, not a binary patch. Start-up only, as in H; switching during play needs R's tile-transition care.
  The game then lays out 1x itself, and resolves and transfers shrink with it. Breaks: edge aliasing at 1920x1080
  bilinear-stretched to 4K; alpha-to-mask foliage and fences turn into a dither or blink (R #45, our KI-015 family).
  Pair it with FXAA or CAS. Expected: -0.5 to -2 ms (estimate). R measured only -2 to -5 % once the scene was one
  tile, but -18 to -25 % against two tiles. So the gain depends on whether our 960x540 scene is one tile (below).
- The 4x MSAA clears and depth-only passes are separate from the scene's +384 mode; MSAA off does not remove them.

### (c) Guest size and scale
- The SDK scale is integer only (`texture/cache.cpp:78-85`), and so is Canary's. 1280x720 at 1.5x would need
  non-integer EDRAM, resolve and texture scaling: not available upstream (Canary chose an extended-EDRAM
  native-resolution path instead, `4ae7c9f53`). L reaches 1.5x only with its own renderer.
- 960x540 x2 is therefore the cheapest integer route to 1920x1080. 1280x720 x2 = 1440p measured 25.4-26.5 ms
  (M10-001): worse. The UI at 960x540 x2 is rasterized at 1920x1080, as sharp as 1080p; the owner's "blurry" is the
  720p mode's 3x bilinear stretch, which FSR1/CAS addresses.
- **Tiling inference (check before building single-tile):** 960x540 at 2x MSAA is 12 x 68 = 816 EDRAM tiles per
  32-bpp surface, under the game's 1,024-tile budget. By R's rule the game would draw it as one tile by itself,
  while 720p 2x (1,472) is two tiles. Our draws per frame (4619 at 1080p against 4277 at 720p, different windows)
  do not show the 30 % difference this would give. So either the content differed or 960x540 does not change the
  tiling. One audit run settles it: log the tile count, r18 MSAA mode and target size at `0x8217A700`, or use
  `gpu_render_audit` resolve lines. If 720p is two tiles, single tile is also a **CP lever for 720p**
  (R: -28 to -35 % CP-thread time), exactly where 720p is CP-bound (12.3 ms CP against 9.0 GPU).

### (d) Cheaper resolve
- Our `TryResolveCopyDirectly` is a stub (`d3d12/render_target_cache.cpp:5936-5981`; preflight counters at
  `:1316-1328`). No BSD upstream exists. R's measured gain (2x: -0.42 ms, 3x: -1.15 ms on the 5080; dump about 50-60 %
  of resolve time) suggests -0.6 to -1.2 ms for us (estimate). New code from the idea: shaders = resolve copy shaders
  with EDRAM loads replaced by render-target sample loads plus the dump conversion. Batch the loads, which R found
  matters on small GPUs. Texture-load-direct (decode scaled resolved data straight into the texture through a UINT
  view): -0.3 to -0.5 ms (estimate).

### (e) Stencil transfer passes on NVIDIA
- No upstream alternative: Canary does the same 8 passes on D3D12 without `SV_StencilRef`. `no_discard_stencil...`
  is not correct output. Our site: `d3d12/render_target_cache.cpp:4499-4508` (`need_stencil_bit_draws`),
  pipelines `:4058-4086`.
- The best lever is to avoid the transfers: clear-rectangle draws done as host clears (R's idea) remove the 4x
  clear transfers and their copies back, which were most of the transferred samples. Every depth transfer removed
  also removes its 8 stencil passes. Then, in our own code: skip bit passes whose bits are known zero (a source
  that was cleared to a uniform stencil value can take one `ClearDepthStencilView` instead of 8 passes), or write
  only the bits the game uses (the vehicle id 1-5 needs 3 bits). Estimate: clears as clears -1.5 to -3 ms; bit
  pruning a further -0.3 to -1 ms. Measure the transfer share first.

## 4. Ranked levers (1080p on the RTX 5060 Laptop, profile 68e5)

| # | Lever | Expected GPU ms at 1080p | Image quality | Risk | Effort | One-run owner A/B |
|---|---|---|---|---|---|---|
| 1 | Presenter FSR1/CAS (define `REX_HAS_FIDELITYFX_SDK` without the fetch; host keys `present_effect_fsr`, `present_effect_cas`, sharpness) and then **720p + FSR1** | 720p: **-8.8 measured** (17.8 -> 9.0) plus about +0.5 present (estimate); 1080p + CAS: +0.3-0.5 (estimate) | sharper HUD and edges on 4K; 720p scene detail stays 720p | low (existing upstream code, MIT shaders) | SDK CMake define + 3-4 host keys | same route 3 x 5 min: 720p+fsr, 1080p+cas, 1080p bilinear; HUD/map stills + MCLA_PERF_SPLIT |
| 2 | GPU category timestamps (planned `MCLA_GPU_CAT`; R used the same method) | 0 (decides 3-6) | none | none | new, small | one 5-min 1080p run |
| 3 | Guest clear rectangles as host clears | -1.5 to -3 (estimate; R 3x: 3.6 of 13.7 ms) | none if exact | medium (exactness checks: formats, 20e4 depth, 8x8 depth alignment) | new code from idea | verify arm + perf; transfer count per frame |
| 4 | Guest MSAA off (mid-asm `0x822E4B80`) + FXAA (`swap_post_effect=fxaa`, exists) | -0.5 to -2 (estimate) | more aliasing at 1080p on 4K; foliage/fence dither | medium (KI-015 family) | codegen hook + host cvar + key | 1080p on vs off, same route; stills of foliage, fences, car edges |
| 5 | Real direct resolve | -0.6 to -1.2 (estimate; R 2x -0.42 / 3x -1.15 on the 5080) | none (bit-exact if done right) | medium-high (all formats, MSAA, scaled) | new code | verify arm comparing to the dump path + perf |
| 6 | Single tile (extended EDRAM from Canary `4ae7c9f53` + tile hook `0x8217A700`) | -0.6 to -1.5 GPU if 1080p is two tiles; else 0; at 720p CP -25-35 % (R) | none | medium (tile surface cache 8 slots, MSAA mode changes between one-tile frames) | port (BSD) + new hook | first an audit of the tile count at 720p and 1080p |
| 7 | `draw_resolution_scale_threshold` port (Canary `74db632ab`) for half/quarter post targets | -0.3 to -1 (estimate) | softer bloom/blur | medium | port BSD with attribution (about 500 lines) | arm threshold 480 vs 0 |
| 8 | Texture load direct | -0.3 to -0.5 (estimate; R 3x -0.5) | none | low-medium | new code | perf arm |
| 9 | Stencil bit pruning / clear instead of 8 passes | -0.3 to -1 after #3 (estimate) | none if exact | medium | new code | counters: stencil passes per frame |
| 10 | Motion blur off (`0x8260D0B8`/`0x8260D0D4`) or `nofsblur` (`0x821C06EC` registrar) | -0.3 to -1 (estimate) | no blur | low-medium | codegen hook | stills + perf |
| 11 | `anisotropic_override` 3 -> -1 | 0 to -0.3 (R: "costs next to nothing") | slightly softer road textures | low | one key | perf arm |
| - | ROV path | +several ms (R: 25.7 vs 12.0 ms at 3x) | - | high | - | rejected |
| - | Fractional scale (1.5x) | - | - | very high | none upstream | rejected |
| - | `no_discard_stencil_in_transfer_pipelines` | - | wrong stencil | - | - | rejected |

### First three to try
1. **Lever 1:** build with `REX_HAS_FIDELITYFX_SDK` (no fetch), add the present-effect keys, and run the owner A/B
   720p+FSR1 against 1080p+CAS against 1080p bilinear. 720p is already measured at 9.0 ms GPU / 66 fps. If FSR1 makes
   its HUD acceptable on 4K, that is the largest win with the lowest risk.
2. **Lever 2 plus the tiling audit** in one measurement build: GPU time by category (draws, transfers by
   colour/depth/stencil-bit, resolves dump/copy, texture loads, uploads) and the tile count / MSAA mode at
   `0x8217A700` for 720p and 1080p. This decides between levers 3, 4, 5 and 6.
3. **Lever 3** (clears as host clears), if transfers are as large a share here as in R's data. It also removes the
   NVIDIA stencil-bit passes of those transfers. Lever 4 (MSAA off) is a one-hook experiment that can run alongside
   as an owner A/B, but its gain is uncertain once tiling is known.

## Files
- New: this file. Scratch downloads: `%LocalAppData%\Temp\claude\C--BDU-MCLA-Recomp\aa-research-work\dl` (public
  files only).
- Read only: SDK worktree files cited above, `src/mcla_dev_overrides.h`, `src/mcla_launcher_controller_win.cpp`,
  `config/mcla_functions.toml`, `scripts/stage-profile-name-sdk.ps1`, the evidence files named at the top.
  No save folders, `save-files-private.json` or dumps were opened.
