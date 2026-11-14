# AMD Anti-Lag 2 readiness: Radeon 860M (laptop iGPU) and RX 6800 XT (VM) (2026-11-14)

Read-only source study by a subagent for the coordinator, plus the owner test preparation
("do all the Anti-Lag preparations"). Sources: host `main` at 1906c33 (the l102 host; `src/` in
the working tree is identical to 1906c33), SDK worktree `private/l101-sdk-wt` at a49e511 (the
l101/l102 SDK), `docs/evidence/M11-l95-sdk-low-latency-20261112.md`,
`docs/evidence/M11-ki076-pacing-and-low-latency-design-20261111.md`, `private/l102-arms`.
Spark not used: source reading with exact line references; nothing to offload. No network.
Nothing was run on a GPU; every runtime statement below is a prediction from the source until
the owner's logs show it.

**Verdict.** As built, nothing in our code stops Anti-Lag 2 (AL2) on the 860M or on a
passed-through RX 6800 XT: the only gates are the adapter vendor id 0x1002, not Wine, the AMD
driver DLL `amdxc64.dll`, and the driver accepting the AL2 interface. Whether the driver accepts
it on these GPUs is not knowable from the source; the init log line answers it. The existing l102
arms already give two fair A/B pairs, so no new arm files or helpers were created. One variant is
not expressible in l102: "AL2 replaces strict with the lead-2 guard", because no host key writes
the SDK switch that lets the guard path count an AL2 sleep (section 3, proposed l103 host keys).

## a. Adapter choice (launcher GPU switch)

- The launcher lists D3D12 hardware adapters with `IDXGIFactory1::EnumAdapters1`, skipping
  software adapters and adapters that cannot create a feature level 11_0 device
  (`src/mcla_launcher_controller_win.cpp:229-243`). The UI GPU list is `UiGpu`
  (`src/mcla_launcher_ui_win.cpp:561-568`); the choice is saved by name in the launcher
  preferences (`gpu_name`). A never-chosen GPU defaults to the adapter with the most dedicated
  video memory, i.e. the RTX 5060, not the 860M (`src/mcla_launcher_gpu_policy.h:19-35`).
- At Start Game the saved name is mapped back to its raw EnumAdapters1 ordinal
  (`AdapterIndex`, `src/mcla_launcher_controller_win.cpp:244-256`; an ambiguous or missing name
  refuses the launch, no fallback) and passed as `--d3d12_adapter=<ordinal>`
  (`src/mcla_launcher_controller_win.cpp:878`, `:939`); the session's `launch-receipt.txt` records
  `gpu <name>` and `adapter <ordinal>` (`:958-960`).
- SDK: `d3d12_adapter` (init-only, default -1 = first hardware adapter, -2 = WARP)
  (`private/l101-sdk-wt/src/ui/d3d12/d3d12_provider.cpp:33-35`); the provider walks
  EnumAdapters1 and stops at that ordinal (`:258-284`). The texture budget code picks the same
  adapter the same way (`src/mcla_texture_budget_win.cpp:135-164`).
- Confirming log line (game log, GPU category): `DXGI adapter: <description> (vendor 0x1002,
  device 0x....)` (`d3d12_provider.cpp:306-307`). 0x1002 = AMD, 0x10DE = NVIDIA. The same vendor
  id reaches the low-latency layer (`d3d12_provider.cpp:473`) and is printed again in the
  `MCLA_LOWLAT kind=init` line (`vendor=0x....`).
- Risk: the ordinal is computed in the launcher process and applied in `mcla.exe`. If Windows'
  per-app graphics preference differs between `MCLA-Launcher.exe` and `mcla.exe`, the two
  processes can enumerate in a different order and the ordinal can name the other GPU. The
  `DXGI adapter:` line is the check; a 0x10DE line in an "860M" run makes the run invalid.

## b. Our Anti-Lag 2 path

Conditions, in order:

1. `d3d12_low_latency_mode` (rexruntime string, restart-only, read once at the first frame;
   `private/l101-sdk-wt/src/ui/low_latency.cpp:21-27`): `off` (default) | `reflex` | `antilag2` |
   `auto` (also `0`..`3`; anything else logs `result=bad_mode`, `:62-76`, `:568`). Host key
   `low_latency_antilag2=true` writes `antilag2` (`src/mcla_dev_overrides.h:844`).
2. `SelectBackend` (`low_latency.cpp:106-135`): under Wine -> none (`wine`) unless
   `d3d12_low_latency_allow_wine` (no host key; and `auto` is always off under Wine,
   `:111-114`); `antilag2` needs vendor 0x1002 (`kVendorAmd`,
   `include/rex/ui/low_latency.h:44`), else none (`vendor`). **`auto` on AMD selects nothing**
   (`result=auto_antilag2`) unless `d3d12_low_latency_auto_antilag2` is true (`:53-56`,
   `:126-128`), and no l102 host key writes that cvar. So on AMD use `low_latency_antilag2`, not
   `low_latency_auto`.
3. Device: the provider's `ID3D12Device` from `low_latency::Init(device, VendorId)`
   (`d3d12_provider.cpp:473`, `low_latency.cpp:625-633`); null device -> `no_device`
   (`src/ui/d3d12/d3d12_low_latency.cpp:253-256`). Activation is lazy at the first frame hook
   (`low_latency.cpp:554-592`); the host's frame-timer hook is the sleep point
   (`src/mcla_raw_timer.cpp:175`, `src/mcla_low_latency.h:5-12`).
4. Driver DLL: `amdxc64.dll` via `GetModuleHandleW`, else `LoadLibraryExW` from System32 only
   (`d3d12_low_latency.cpp:46-53`, `:209-212`); missing -> `amdxc64_not_found`. amdxc64.dll is the
   AMD D3D12 user-mode driver, so on an AMD device it is normally already loaded
   (`amdxc64_preloaded=1`).
5. `AMD::AntiLag2DX12::Initialize(&context, device)` (`d3d12_low_latency.cpp:213-217`): the
   vendored header calls the driver export `AmdExtD3DCreateInterface(device,
   IAmdExtAntiLagApi)` and then `UpdateAntiLagState` with AL2 disabled
   (`thirdparty/amd_antilag2/ffx_antilag2_dx12.h:116-154`). Non-S_OK ->
   `antilag2_initialize_0x<HRESULT>`: E_INVALIDARG 0x80070057 if the export is missing
   (`:118`, `:126`), E_HANDLE 0x80070006 if the module is not loaded (`:150`), otherwise the
   driver's own code (for example E_NOINTERFACE 0x80004002 if it does not offer the interface).
6. `d3d12_low_latency_markers_only=true` (key `low_latency_markers`) initialises AL2 disabled
   and never calls `Update`, because the controller skips the sleep when markers-only
   (`d3d12_low_latency.cpp:218`, `low_latency.cpp:298-306`). AL2 has no markers and no latency
   report (`d3d12_low_latency.cpp:225`, `:229-230`), so on AMD "markers" = init check only.

Sleep: once per guest swap at the frame timer, an optional waitable-timer pre-sleep (average of
the last sleeps minus `d3d12_low_latency_presleep_margin_us` 1500, capped by
`d3d12_low_latency_max_sleep_ms` 30; `low_latency.cpp:38-46`, `:303-311`), then
`Update(&context, enable, 0)`, which inserts the driver delay
(`d3d12_low_latency.cpp:226-228`; header `:172-213`; maxFPS 0 = no AL2 frame limiter).

Log lines (`REXLOG_INFO`, game log):

- Init, only when the mode is not `off` (`low_latency.cpp:263-271`, `:576-580`):
  `MCLA_LOWLAT v=1 kind=init mode=antilag2 vendor=0x1002 backend=antilag2 result=ok
  amdxc64_preloaded=<0|1> markers_only=0 boost=0 presleep_margin_us=1500 max_sleep_ms=30 wine=0`.
  Failures print `backend=none` with `result=` one of `vendor`, `wine`, `bad_mode`,
  `auto_antilag2`, `no_device`, `amdxc64_not_found`, `antilag2_initialize_0x........`,
  `no_d3d12` (`low_latency.cpp:480-486`). Any failure leaves the layer off for the session.
- Every minute while active (`low_latency.cpp:235-261`): `MCLA_LOWLAT v=1 kind=minute
  mode=antilag2 backend=antilag2 markers_only=0 sleeping=1 frames= fps= sims= sleeps=
  sleep_skipped= sleep_ms_p50= sleep_ms_p99= sleep_ms_max= sleep_ms_total= presleep_ms_total=
  spin_ms_est= overshoots= ... lat_frames=0 lead_hist=a,b,c,d`. `sleeps>0` = Update is called;
  sleep time above ~0 = the driver is delaying (the pre-sleep follows the measured sleeps, so it
  is ~0 while the driver does not sleep). `lat_frames=0` is expected on AMD. An `Update` failure
  is not logged by itself (it only feeds the Reflex-style strict replacement).
- `MCLA_LOWLAT v=1 kind=mismatch ...` (at most 8) for frames with 0 or 2+ simulation starts per
  swap (`:351-357`); `MCLA_LOWLAT v=1 kind=shutdown` at exit (`:648`).
- The arm itself: `MCLA dev override: low_latency_antilag2=true (d3d12_low_latency_mode=antilag2)`
  (`src/mcla_dev_overrides.cpp:183`).

Discrete-only restriction: none in our code. The only hardware gate is the vendor id
(`low_latency.cpp:119-121`); there is no dedicated-memory, UMA, adapter-flag or device-id check in
`d3d12_low_latency.cpp` or `low_latency.cpp`. The vendored MIT header says nothing about supported
GPUs; the only relevant comment is:

```
    // A return value of S_OK indicates that Anti-Lag 2.0 is available on the system.
    HRESULT Initialize( Context* context, ID3D12Device* device );
```
(`ffx_antilag2_dx12.h:33-34`), and for the per-frame call:
```
    // Update function - call this just before the input to the game is polled.
    ...
    // maxFPS - sets a framerate limit. Zero will disable the limiter.
```
(`:41-44`). Which GPUs/drivers return S_OK is decided by the AMD driver; the KI-076 design
recorded it only as "RDNA1+? driver 24.x+" with a question mark
(`M11-ki076-pacing-and-low-latency-design-20261111.md:48`). Both the 860M (RDNA 3.5) and the
6800 XT (RDNA 2) are in that range if that note is right; the owner's init line is the proof.

## c. Fair A/B with the existing l102 arms

All four arms are the l102 PC base (8 switches + census + warp/album/pursuit probes) plus:

| Arm (helper) | Extra keys | Effective |
|---|---|---|
| pc/default (`l102-arm-default.cmd`) | none | strict 1 (m10 default, `src/mcla_m10_defaults.h:138`), no AL2 |
| pc/antilag2 (`l102-arm-antilag2.cmd`) | `low_latency_antilag2=true` | strict 1 + AL2 sleep every frame |
| pc/strict-off (`l102-arm-strict-off.cmd`) | `frame_lead_strict_off=true` | strict 0, no AL2 |
| pc/antilag2-nostrict (`l102-arm-antilag2-nostrict.cmd`) | `low_latency_antilag2=true`, `frame_lead_strict_off=true` | strict 0 + AL2, no guard |

Fair pairs: default vs antilag2 (AL2 on top of strict; strict already holds the lead at 1, so a
small effect is expected) and strict-off vs antilag2-nostrict (AL2 as the only bound).
Re-checked now with the l102 `check_sparse.exe` (built against the l102 `mcla_dev_overrides.h`,
identical to 1906c33 apart from CRLF, 131 keys): 4 arms, each `status=2 ignored=0`,
`mismatches: 0`. SHA-256: pc/default BBF6396D...958E (as recorded in the l102 evidence),
pc/antilag2 5B1B4333...4287, pc/strict-off 2692954E...BDBD, pc/antilag2-nostrict
CCE855A9...BF98; helpers `l102-arm-default.cmd` F1A22845...5288, `l102-arm-antilag2.cmd`
2EAEB455...26B8, `l102-arm-strict-off.cmd` 92D369C7...6961, `l102-arm-antilag2-nostrict.cmd`
F07375F7...8510, `l102-arm-clear.cmd` 428C0FCE...1D1F (as recorded). The helpers were dry-run in
l102 and are unchanged, so no new dry run was needed.

Lead-2 guard and stale-sleep fallback on AMD: **they do not apply in l102.** They live in
`d3d12_frame_lead_strict_low_latency` (`private/l101-sdk-wt/src/graphics/d3d12/command_processor.cpp:330-341`,
guard `:348-379`), evaluated per frame open by `EvaluateLeadActive(QueryLeadState(),
d3d12_frame_lead_strict_low_latency_antilag2)` (`:6966-6985`). With an AL2 backend and that SDK
bool false (its default, `:343-346`), the reason is `antilag2` on every frame
(`low_latency.cpp:175-178`), so the configured strict applies and the guard never relaxes or
trips; with it true the same checks as Reflex apply, including `stale_sleep`
(`low_latency.cpp:185-197`). No l102 host key writes that bool (keys `src/mcla_dev_overrides.h:843-895`;
the l97 keys `lead_ll_off_when_reflex` / `lead_ll_adaptive_when_reflex` write only
`d3d12_frame_lead_strict_low_latency`, `:862-865`). Therefore: an AMD arm with `lead_ll_*` keys
would silently behave like pc/antilag2 (strict on), and pc/antilag2-nostrict is unguarded. No
such arm was created.

Missing for the full Reflex-equivalent AMD matrix (needs l103, host only; the SDK already has
both cvars):

- key `lead_ll_count_antilag2` -> `d3d12_frame_lead_strict_low_latency_antilag2=true`
  (Phase::kGraphicsPlugin, hot reload; =false writes nothing);
- key `low_latency_auto_antilag2` -> `d3d12_low_latency_auto_antilag2=true` (Phase::kStartup);
- then arm bodies (PC base as above, plus):
  - `amd/antilag2-replace`: `low_latency_antilag2=true`, `lead_ll_off_when_reflex=true`,
    `lead_ll_count_antilag2=true` (AL2 replaces strict while its sleep is active; l97 guard
    latches strict back);
  - `amd/antilag2-replace-noguard`: the same + `lead_ll_guard_off=true` (guard observe-only);
  - `amd/auto`: `low_latency_auto=true`, `low_latency_auto_antilag2=true`,
    `lead_ll_off_when_reflex=true`, `lead_ll_count_antilag2=true` (the shipping-candidate shape).
  These bodies are proposals only; they would be rejected by the l102 parser as unknown keys.

## d. What could make an 860M run invalid or risky

- **Wrong adapter** (section a): check `DXGI adapter: ... vendor 0x1002` in every log.
- **Shader warm-up:** the first start on a new GPU builds the driver's pipeline cache from the
  stored pipelines; the first session will load longer and stutter. One uncounted warm-up
  session first. (Shader translation only branches on Intel:
  `private/l101-sdk-wt/src/graphics/pipeline/shader/dxbc_translator.cpp:178`; vendor passed in at
  `src/graphics/d3d12/pipeline_cache.cpp:111-115`.)
- **Vendor-specific backend branches:** render targets use the host render-target path on
  everything except Intel (`src/graphics/d3d12/render_target_cache.cpp:193-195`; the AMD-specific
  branch is in a disabled `#else`, `:196-202`); the stencil-reference export is gated for Intel
  only (`:376-380`). No AMD-only path is active.
- **Video-memory budget on a UMA iGPU:** the host reads `DXGI_MEMORY_SEGMENT_GROUP_LOCAL` of the
  chosen adapter (`src/mcla_texture_budget_win.cpp:166-169`, `:405-408`). On a shared-memory
  iGPU that "local" budget is system RAM granted to the GPU, so the limits come from it:
  hard = clamp(min(0.8 x budget, budget - 1536 MB), 768, 8192), soft = clamp(budget / 2, 384,
  4096) (`src/mcla_vram_census_win.h:40-47`, `:94-101`); the 2048 MB soft cap applies only at
  draw scale 2+ and a budget under 10 GB (`:102-117`), so at 720p (scale 1) it is `scale_1`. The
  monitor lowers hard by 256 MB per 10 s when usage is within 512 MB of the budget, never below
  the start soft limit (`mcla_texture_budget_win.cpp:104-107`, `:330-344`). Net effect: the
  texture cache may keep up to several GB of textures in shared RAM; that is the same RAM the game
  and Windows use. Not a validity problem at 720p, but MCLA_VRAM "local" numbers on the 860M mean
  shared RAM and are not comparable with the 5060's. The startup lines `MCLA graphics:
  video-memory budget ... MB` and `texture cache soft cap scale_1 ...` (`:447-457`) record what was
  chosen. D3DKMT clock/temperature fields may read n/a on the iGPU.
- **Resolution:** the launcher's default render target is 720p (`src/mcla_launcher_preferences.h:11`,
  `src/mcla_launcher_capabilities.h:70`); 1080p/2160p need the ticket grant or owner admission
  (`mcla_launcher_capabilities.h:242-250`). The owner's saved choice is global in the launcher
  preferences, so set 720p explicitly and restore afterwards. There is no 4K default.
- **Recommended 860M settings (Deck-style):** 720p (draw scale 1), VSync on (forced), the same
  timing for every arm (Unlocked as in the PC runs; AL2 only helps when the GPU is the limit, so
  at Stock 30 FPS expect no difference), laptop on the charger and the same power mode, AMD
  Software's own Anti-Lag / Chill / Boost / frame-rate target off (they would act on all arms).
- **Night flicker (KI-074):** strict off (arms 3 and 4) re-opens the frame lead; without a guard
  on the AMD path the flicker can return in both. Report it per arm.
- **VM (RX 6800 XT, PCIe passthrough):** nothing in our code distinguishes a VM; the vendor check
  and `amdxc64.dll` behave as on bare metal. A virtual display adapter may also be listed:
  choose the 6800 XT by name. Streaming adds latency, so only numbers are meaningful there.

## Prepared for the owner

- No new arm files and no new helpers (the l102 arms cover both fair pairs; the guarded variant
  cannot be expressed in l102).
- `private/l102-AMD-CHECKLIST.txt` (new, ASCII, CRLF, 6064 bytes, SHA-256
  07F2B6C9F73901C3208FC66EDC9B0706C0C35584BDE7D2E1FEB299A021D73916): build-folder reminder,
  copy profile, arm order 1 default, 2 antilag2, 3 strict-off, 4 antilag2-nostrict, 5 default
  again; Part 1 860M by feel (launcher GPU switch, 720p, warm-up session, 5 minutes free roam per
  arm, F10 twice, steering response, KI-074 flicker); Part 2 RX 6800 XT in the VM over
  Parsec/Moonlight, numbers only (1080p, 10 minutes per arm, F10 twice, HWiNFO); the exact init /
  minute / failure lines; send back the session log and the diagnostics folder without
  `save-private` folders and `save-files-private.json`
  (`src/mcla_crash_handler_win.cpp:526-528`).

## Risks and open points

- Whether AMD's driver accepts AL2 on the 860M and in the VM is unknown until the init line is
  seen; a refusal code goes to the owner report.
- The adapter ordinal can differ between the launcher and the game process under per-app GPU
  preferences (section a).
- AL2 `Update` failures are not logged per frame; only the minute line's sleep times show that the
  driver delays.
- With strict on, AL2 has little room to act; the meaningful comparison is strict-off vs
  antilag2-nostrict, which carries the KI-074 risk without a guard.
