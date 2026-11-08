# KI-013 traffic lights over-light car paint: MIN/MAX blend source factor (draft)

Implementation by an Opus subagent on 2026-11-08 as SDK commit `d400149` on local branch `mcla/m11-blend-minmax` (parent `0755836`; own design; the defect description came from a colleague project's commit messages only, no GPL code consulted); accepted by the primary agent as code: ctest 2168/2168 (4 skipped), 0 new warnings; to be cherry-picked into l80.

Status: implementation candidate, **not physically tested**. Branch `mcla/m11-blend-minmax`
(worktree `private\l80-sdk-blend-wt`, base `mcla/m11-l79` 0755836), cvar
`d3d12_blend_minmax_fix` (default **false**, for the owner A/B). Build `private\l80-sdk-blend-b`
(Release, not installed). Commit: see the hand-off report.

## 1. What the colleague project says (descriptions only, GPL-3.0 source not opened)

holdmysocks/mcla-recomp (GPL-3.0). Read via `gh api`: commit messages and README text only;
no source or patch file was opened or copied.

- Commit 54481c20 "Car reflection fix: MIN/MAX blends with a SrcAlpha factor, D3D12 and Vulkan":
  "Cars came out white with a glow halo, worst at night. The Xbox 360's GPU applies the source
  blend factor in the MIN and MAX blend equations; Direct3D 12 and Vulkan ignore it. The light
  glows in the cars' environment map are drawn with max(source x SrcAlpha, destination), so each
  became a solid square that the paint mirrored." Their remedy (described): the translated pixel
  shader multiplies by its own alpha in that case, on both backends. Confirmed by them in game on
  Windows/D3D12.
- Commit d2aced0a: "Car reflection fix confirmed on a PS5; before and after in the README".
- README: "Car reflections are fixed on every renderer (cars no longer come out white with a glow
  at night)".

Link to KI-013: the traffic-signal lights are light glows mirrored by the paint's environment map;
drawn at full source strength they become saturated red/green blocks on the car body that follow
the signal phase - exactly the M6-004 fixed-camera repro (red strip, neon-green strip, gone).

## 2. The defect in our SDK (l79-sdk-wt @ 0755836)

Not a wrong op mapping: Xenos `color_comb_fcn`/`alpha_comb_fcn` MIN/MAX map to
`D3D12_BLEND_OP_MIN/MAX` correctly. The defect is a **missing emulation**: the guest's source
blend factor is silently dropped for MIN/MAX on every path.

- `src/graphics/d3d12/pipeline_cache.cpp:1666-1671` - RB_BLENDCONTROL factors and ops are captured
  into the pipeline description.
- `src/graphics/d3d12/pipeline_cache.cpp:3117-3147` - the PSO uses `kBlendOpMap`
  (MIN -> `D3D12_BLEND_OP_MIN`, MAX -> `D3D12_BLEND_OP_MAX`) and passes the factors, which D3D12
  ignores for MIN/MAX, so a guest `max(src * SrcAlpha, dst * ONE)` becomes `max(src, dst)` on the
  host (default host-RTV path; `render_target_cache.cpp:178-194`: RTV on NVIDIA/AMD, also the
  Deck via vkd3d; ROV only on Intel or with `render_target_path_d3d12=rov`).
- `src/graphics/pipeline/shader/dxbc_translator_om.cpp:1533-1571`
  (`CompletePixelShader_WriteToRTVs`) - nothing compensates in the shader.
- ROV path, same omission by design: `dxbc_translator_om.cpp:2315-2328` (color) and `2496-2510`
  (alpha), comments "Choose max/min of the colors/alphas without applying the factors".
- Vulkan FSI: `spirv_translator_rb.cpp:3283` "Handle min and max blend operations, which don't
  involve the factors" (Vulkan is off in our build).

Effect: any low-alpha highlight drawn with MAX + SRC_ALPHA is drawn at full strength. Not yet
proven by a capture that MCLA's lighting/reflection draw uses exactly
`MAX, SRC_ALPHA, ONE` (the colleague's statement, plus the KI-013 symptom, is the evidence).

## 3. Upstream references and licences

- Xenia (BSD-3): xenia-project/xenia 6c0ffe881a "[D3D12] ROV: Min/max blend operations"
  (2018-10-17) and e39bb04875 "[D3D12] ROV: Fix min/max blending operation" (2018-10-29, a
  swizzle fix). Both keep "factors ignored" semantics; no upstream commit applies the factors.
  xenia-canary commit/issue search for min/max blend factor: no hits. Nothing ported.
- Mesa (MIT), `src/mesa/state_tracker/st_atom_blend.c:300-320`: GL MIN/MAX are translated with
  src/dst factors forced to ONE ("Min/max are special"), consistent with ATI-family hardware
  applying factors in MIN/MAX (indirect corroboration only, not Xenos documentation).
- Our implementation is an own design from the problem description; no third-party code.

## 4. The fix

- `include/rex/graphics/util/blend_min_max.h` (new, header-only, pure): `GetMinMaxSourceScale
  (RB_BLENDCONTROL)` -> per render target 2-bit RGB scale + 2-bit alpha scale
  (none / src alpha / 1 - src alpha / zero). Applied only when the op is MIN/MAX **and the
  destination factor is ONE** (what the host min/max already does), and the source factor
  depends only on the source: RGB ONE/ZERO/SRC_ALPHA/ONE_MINUS_SRC_ALPHA; alpha also
  SRC_COLOR/ONE_MINUS_SRC_COLOR (color modes act as alpha modes, like `kBlendFactorAlphaMap`).
  The alpha scale is skipped if a non-MIN/MAX color blend reads the source alpha
  (SRC_ALPHA, ONE_MINUS_SRC_ALPHA, SRC_ALPHA_SATURATE as src or dst factor). All other cases
  (dst factor not ONE, SRC_COLOR for RGB, destination/constant/saturate factors) stay unchanged.
  `GetMinMaxSourceScaleAllTargets` skips targets with an empty normalized color mask.
- `include/rex/graphics/pipeline/shader/dxbc_translator.h`: new pixel modification field
  `min_max_source_scale : 16` in the free upper 16 bits of pixel word 1. Zero = old translation,
  so `Modification::kVersion` is unchanged and existing pipeline storage stays valid.
- `src/graphics/d3d12/pipeline_cache.cpp`: cvar `d3d12_blend_minmax_fix` (default false);
  `GetCurrentPixelShaderModification` fills the field only with host render targets and the
  cvar on.
- `src/graphics/pipeline/shader/dxbc_translator_om.cpp` `CompletePixelShader_WriteToRTVs`:
  after the exponent bias (and so after alpha test / alpha to coverage, which use the unscaled
  alpha) and before the optional PWL gamma: rgb *= a | (1 - a) | 0, then a *= a | (1 - a) | 0.
- Not covered: ROV path (would need restructuring the in-shader factor/min-max branches;
  non-default on our hardware), Vulkan (off), destination factor other than ONE.
- Cost with the cvar on: one extra shader variant per pixel shader used with an affected blend
  state (one-time compile), 1-2 ALU per affected output. Off: one bool test per draw.

## 5. Tests

- `tests/unit/graphics/blend_min_max_test.cpp` (new, `[blend_min_max]`): pack/unpack; non-min/max
  unchanged; the KI-013 `MAX, SRC_ALPHA, ONE` case; ONE/ONE no-op; every supported factor;
  every unsupported factor and dst != ONE left alone; alpha scale vs color factors reading the
  source alpha; per-RT packing with the color mask; padding bits ignored.
- Build/ctest/warnings/clang-format results: see the hand-off report.

## 6. How the owner verifies

1. Same l80 build, two runs differing only in `d3d12_blend_minmax_fix` (false, then true).
2. At night, stop a car right next to a traffic light, rear/side view so the body faces the
   signal; hold the camera; wait for red, then green (then the phase change).
3. Expected with the fix: the body highlight stays a modest tinted reflection, no saturated
   red / neon-green strip or block on the paint; headlight/streetlight glows on cars are no
   longer solid white squares with a halo. Off: the old KI-013 look.
4. Also glance at anything else that might use MIN/MAX blending (glows, HUD, menus, minimap,
   weather): any new darkening or missing glow is a regression to report.

## 7. Results (hand-off)

- Commit `d400149` on `mcla/m11-blend-minmax` (parent 0755836), not pushed.
- Build `private\l80-sdk-blend-b` (Release, output `l80-sdk-blend-b\o`, not installed): 1463/1463,
  0 errors. Warnings normalised (`l80-blend-scripts\w80.txt`, 220 entries) identical to l79
  `w79.txt`: **0 new warnings**.
- ctest: **2172 tests, 2168 passed, 4 skipped** (the same BitStream Write skips), 0 failed;
  +9 new `unit.MinMax blend:*` cases over the 2163/2159 baseline.
- clang-format 20.1.8: new files and `dxbc_translator_om.cpp`/`dxbc_translator.h` clean;
  `pipeline_cache.cpp` keeps its one pre-existing finding (line 3413, not in the hunk).
- Logs: `%TEMP%\claude\C--BDU-MCLA-Recomp\l80-blend-scripts\` (configure/build1/ctest).
- Spark: not used (bounded single-file source reading; validation cost exceeds benefit).
