# External MCLA recomp repositories: survey (2026-11-08)

Read-only survey by an Opus subagent on 2026-11-08 (GitHub API and raw files only; nothing built or run); accepted by the primary agent. Licence rule: holdmysocks and RenanQueiroz forks are GPL-3.0 and LARecomp has no licence and forbids copying, so from them we take ideas, measurements and guest addresses only; code only from permissive upstreams (Xenia Canary/Edge BSD-3, ForzaHorizon1-ReImagined MIT, NVAPI and AMD Anti-Lag 2 headers MIT).

Read-only survey via the GitHub API (READMEs, file trees, commit lists, issue lists, selected source and
documentation files). Nothing was built or run. All fetched content was treated as data.
Comparison base: MCLA-R `docs/known-issues.md`, README-AI 17c, SDK `d61c597` (l78) and host `main`.

Licence context: MCLA-R host is MIT; our SDK fork (2FED/rexglue-sdk) is BSD-3. GPL-3.0 code cannot be
copied into either without relicensing them, so for GPL repos only ideas, measurements and game facts
(guest addresses, struct offsets) are usable, unless the code is itself taken from a permissive upstream
(then take it from that upstream directly).

---

## 1. holdmysocks/mcla-recomp

- **Summary.** Static recomp of MCLA Complete Edition (545407F8) for Windows (D3D12), Linux (Vulkan) and
  jailbroken PS5. The README now says PC is **no longer maintained**; only PS5 updates continue, and it
  points users to RenanQueiroz's fork and to LARecomp. The owner is also writing a Rust rewrite
  (holdmysocks/rusty-mcla).
- **SDK base.** Stock ReXGlue SDK **v0.10.0** (rexglue/rexglue-sdk) plus 6 patches in `patches/`
  (an MCLA patch, MIN/MAX blend factor, Linux procmap/wait-sleep, Vulkan paint-pipeline format, FFmpeg PS5).
- **Licence.** GPL-3.0-or-later. Several host parts are ported from LARecomp "with its author's permission".
- **Activity.** Created 2026-10-03; last push 2026-10-30; releases v1.0.5..v1.0.14 (2026-10-19..10-29).
  65 files. Only 7 PRs, no issue tracker activity left visible.
- **Content of interest.** `src/frame_timing.cpp` (real dt hooks, pace at swap, UI movie pacing),
  `src/settings_menu.cpp` (in-game pause-menu tabs, from LARecomp), `src/warp_fix.cpp` (warp that never
  ends, from LARecomp), `src/save_wait.cpp` (QUIT waits for saves), car reflection MIN/MAX blend fix,
  PS5 DualSense haptics/adaptive triggers.

## 2. RenanQueiroz/mcla-recomp (fork of holdmysocks)

- **Summary.** The most active and best-documented of the three. Adds a .NET launcher + setup wizard
  (builds the game from the user's ISO, self-updates), ~80 SDK patches with in-run A/B measurements, HDR10,
  FSR 3.1 AA/upscaling/frame generation, **NVIDIA Reflex / AMD Anti-Lag 2**, Smooth UI, ultrawide (PR #38),
  stick dead zones, vibration switch, PC DualSense adaptive triggers, XMA decoder fixes, saved-game backup.
  Every change has an off switch (`docs/off-switches.md`) and a measured write-up (`patches/README.md`,
  `KNOWN-ISSUES.md`).
- **SDK base.** Stock ReXGlue **v0.10.0** (`f5337cdc`) + ordered patch stack (`FORK.md`, `patches/`).
  Also ports from Xenia Canary/Edge (BSD-3), ForzaHorizon1-ReImagined (MIT), rexglue PR #529 (BSD-3).
- **Licence.** GPL-3.0-or-later (launcher has its own LICENSE file; MIT parts listed in
  `THIRD-PARTY-NOTICES.md`).
- **Activity.** Very high: 100+ commits since 2026-10-01, releases v0.3.1..v0.3.10 (latest 2026-11-08).
  25 branches (cpu/*, perf/*, fix/*, diag/*). 66 issues/PRs; open: #65 imported-save crash, #64 clone,
  #62 texture flickering, #56 crash, #55, #42, #32, #15 choppy + audio hiccups, #14 no vinyls/Vulkan (5700G).
- The same author (RenanQueiroz) also contributes to LARecomp: LARecomp's Reflex code says "design
  followed (not copied): the MCLA fork's rexglue-low-latency.patch, written by the same author".

## 3. mzzvxm/larecomp (LARecomp)

- **Summary.** "The original" MCLA recomp (in development since 2025). The largest feature set: 112
  mid-asm hooks, 155 settings, 24,927 function hints; in-game settings menu (8 tabs), ISO install wizard,
  save import from Xenia/RPCS3, custom MP3 radio, Discord RPC, mod loader, km/h speedometer, region-locked
  languages, steering wheel (DirectInput + FFB), keyboard/mouse, online (system link, private server,
  Rate My Ride, leaderboards), photo mode fixes, DLSS/FSR 1/3.1/4/CAS, HDR10, GTAO, Reflex/Anti-Lag 2.
- **SDK base / renderer.** ReXGlue SDK **0.10.0** (README) plus an unpublished custom SDK build. Most
  importantly, the default path is a **native renderer that removes the emulated GPU**:
  `mcla_native_gfx_nocp = true` leaves `gpu_plugin` empty so `rexgpu-xenos` is never loaded (no command
  processor, no PM4 parser, no EDRAM cache, no shader translator); `mcla_gpu = true` renders with
  `src/gpu` on **Plume** (mzzvxm/plume fork, RT64's abstraction; D3D12 + Vulkan) fed by hooks on the
  game's own D3D device functions (`src/native_gfx/hooks.cpp`: DrawIndexedVertices `sub_8241D620`,
  EndVertices `sub_8241D220`, Resolve `sub_82420BA8`, Swap `sub_82419E98`, CreateTexture `sub_82410C50`,
  Lock/Unlock `sub_82421CA0`/`sub_82421F38`, DestroyTexture `sub_82177CB0`, BlockUntilIdle
  `sub_82412990` ...). Shaders are precompiled from the game's RPFs by LAR-XenosRecomp into shader packs.
- **Licence.** **None** (no LICENSE file) and an explicit README caution: "I'm not granting permission to
  use LARecomp's features in your own recomps ... please don't copy and paste our code." Treat as
  all-rights-reserved: ideas and game facts at most; credit if any finding is used.
- **Activity.** Created 2026-02-28; very active (100+ commits since 2026-11-01; DLSS, HDR, Reflex,
  GTAO merged 2026-11-06/07). 13 branches. Open issues: #19 settings never persist on stock SDK (TOML
  quoting), #18 wheel/H-shifter, #17 Pinkslip loss crash, #16 0.1 FPS, #10 driving hitches on the GPU
  Commands thread (CreateAllocation + GPU waits).

---

## 4. Our issues/topics vs their status

Legend: solved / partial / different approach / absent. Links are to GitHub paths at HEAD
(`H` = holdmysocks, `R` = RenanQueiroz, `L` = mzzvxm/larecomp).

| Our issue / topic | H (holdmysocks) | R (Renan fork) | L (LARecomp) |
|---|---|---|---|
| **KI-071** cut-scene voice doubled / "barrel" (our W1 frame walk + 5.1->stereo ITU downmix) | absent | **solved, overlapping**: `patches/rexglue-xma-decoder-fixes.patch` `xma_canary_fixes` (default on) = Xenia Canary/Edge commits after 2026-05-11: `aee0871dd` *a frame whose 15-bit header crosses a packet boundary is counted (else the next frame is skipped)* = **our W1**; plus `6bef1fbed` (Work() must not end while a frame is partly handed out - lost frame), `7015473a7` (next-frame search follows a frameless packet's skip count), `5dd1cdbbf` (loop start 1 bit short of a frame boundary), `ade7e610b` (no-progress pass ends the loop). `xma_race_fixes`: lock re-check, **no write-back of guest-owned `output_buffer_read_offset`** (Canary still has it), CAS word merge, kick decodes inline only. Symptom there: radio L/R drift, frames lost every 10-20 s, cut-scene reverb/static (issues #2, #7). Downmix: explicitly "not the cause" (unchanged). Also audio thread priority/MMCSS, adaptive SDL queue, mixer-thread priority on <=5 LP. | absent in code read; TECHNICAL_NOTES lists "audio jitter in dense areas" as open |
| **KI-047** livery corruption / read-back sync; **KI-074** flicker/despawns at high fps (our deferred EVENT_WRITE_SHD fences + steady-key exemption) | absent | **different approach**: no fence change. `rexglue-resolve-readback-rules.patch`: per-resolve read-back rules (`addr=`,`len=`,`pitch=`,`height=`,`fmt=`,`src=`,`phase=`,`mode=`), default rule = vinyl bake target 512x512 `8_8_8_8` at `0x050A0000` + mips `0x051A0000-0x051F7FFF` (`mcla_vinyl_readback`); `resolve_log_frames`/`resolve_log_new` to discover classes. Rain fix `resolve-unscaled-copy`. Flicker: #62 open (unanswered), #45 fences/trees blink without MSAA (closed as limitation), #34 reflections flicker on Low preset (env map every 4th frame). | **different approach**: `src/mc_engine/hooks/readback_window.cpp` turns the global `readback_resolve=fast` on only while a vinyl composite or photo needs it (per-client deadlines); photo mode reads back just the two front buffers (`photo_mode.cpp`). Native renderer has its own guest fence pair (`src/native_gfx/guest/guest_fence.cpp`) - no EVENT_WRITE_SHD emulation at all on the default path. |
| **KI-068** Deck/Proton second save fails | partial (`save_wait.cpp`: QUIT waits for a save) | **related, different**: `rexglue-saved-game-backup.patch` (v0.3.10): before `CREATE_ALWAYS`/`TRUNCATE_EXISTING` deletes a save package it is moved to `Backups/` with an `.inprogress` marker, restored at next start if the game died; `rexglue-content-header-padding.patch` zeroes 8 uninitialised padding bytes in the content header. Issue #65 (imported save crash) open. Not Proton-specific. | partial (`save_protect.h`: QUIT waits up to 10 s for a save) |
| **KI-035** garage freeze (warper state-20 race) / **KI-026** Red Light race-back camera stuck | **solved, broader than ours**: `src/warp_fix.cpp` (from LARecomp `warper.cpp`): after `__imp__sub_8221BE98`, a state 19/20 transition that was running and no longer is gets finished (clear GPS interlock, +276, state 29) then `sub_8221B790` or the handler `sub_8221B8C8`; a transition that never starts gets duration+3 s. Commit d18463a, issue #6 (Red Light race entry). | same as H (inherited) | **solved** (origin): `src/mc_engine/hooks/warper.cpp`, full RE map (+288 state, +296/+300, +276, transition +76 duration / +96 time) |
| **KI-056** album preview shows wrong picture | absent | absent | **solved, different hook**: `src/mc_engine/hooks/photo_album.cpp` `photo_album_keep_new_preview` (default on): `SetPreviewPicture` `sub_8263DFC0` decodes the highlighted slot into the shared PreviewPicture in save mode; they override `sub_8263C9C8` to keep the new snapshot (`sub_8263C8B0`) while a photo waits for its slot. |
| **KI-066** police fines | absent | absent | absent |
| **KI-069** async logging | absent | partial: log hygiene on <6 LP (KNOWN-ISSUES "Log flood") | partial: tells users to set `REX_LOG_LEVEL=warn` (trace = 1.4 MB/s sync writes) |
| **KI-070** F10 minidump stall | n/a | n/a (crash tracer `src/crash_trace.cpp` writes host+guest stack into log) | n/a (`src/crash_handler.cpp`) |
| **KI-067/072** CP-bound cost, high-speed stutter | absent (PS5 notes only) | **many solved items** (D3D12): `d3d12-placed-texture-heaps` (textures in 256 MB placed heaps; create 0.19 ms -> 8-11 us; race 1% low 50->96 FPS native, worst frame 102->13 ms; map zoom-out 158->44 ms); `threaded-submission-d3d12` (+ idle-wait fix for AMD; -15% CP time/draw); `gpu-regwrite-bulk` (-10.5%); `shared-memory-request-fast-path` (-2.4%; -17% with hot pages); `hot-pages-watched-ranges` (write faults 12,000/s -> 200, VirtualProtect 190,000/s -> 1-2,000); `perf-counters-batched`, `inline-bit-helpers`, `clear-rectangle-draws`, `direct-resolve`, `texture-load-direct`, `async-pipelines-no-frame-wait`, `edram-size` + single-tile rendering, `guest-atomics-lock-free`, `codegen-csr-lazy`, `timer-queue-blocking-wait` (our SDK's timer queue still spins: `spin_wait_strategy`), shader pack/pipeline preparation. None of these cvars exist in our `d61c597`. | **architectural**: no command processor at all (native renderer). Also: `Patch_FenceSpinThrottle` (YieldProcessor/SwitchToThread in `sub_82412F98`), `mc_FlushDataCache` `0x821D5510` bypass (~5,000 calls/s, keep the fence), single-tile + EDRAM 4,096 tiles, PM4 per-packet QPC profiling compiled out (25% of the CP thread in stock 0.10.0), `pump_wake.cpp` (frame-pacing thread woken on demand). |
| **Frame rate / dt** (we feed measured frame time via our own `sub_821BDA90` override, smoothing, whole-ms steps, hitch debt) | **different**: hooks `0x821BDB58` (skip first fixed-step overwrite), `0x821BDB90` (second fixed-step path, taken every gameplay frame; f11 = measured dt), `0x821BDAB0` hitch clamp, `0x82419AA0` present interval; limiter at the swap | same + `mcla_frame_start` at `0x821BDA90` (driver wait before the timer reads time) | origin of the above (TECHNICAL_NOTES s.1-2); keeps the `[r3+56]` reset guard ("skipping it caused a loud audio blowout") |
| Continuous-time smoothing at high fps (relates to KI-054) | camera/chassis hooks | **solved**: `config/frame_timing.toml`: camBoomCS::Update `0x82320468` (pos), `0x823204F4` (look-at), chassis depth `0x82563720`, bike gyro `0x8236895C`/`0x823689F0`; formula `1-(1-k30)^(dt*30)` | origin (`hooks/camera.cpp`, `vehicle.cpp`) |
| UI/Flash movies run fast above 30 fps; race countdown; menu input | partial (menus 30 Hz, HUD rate) | **solved**: `mcla_ui_movie_advance` `0x825EDC6C`, HUD rate, race countdown `0x822124DC` (`mcla_ui_countdown_fix`), Smooth UI interpolation (`0x825FB484`, scene hooks `0x82722524`...), menu key press/repeat `0x822240C0`/`0x822241D0`, script pad latch | origin (`hooks/ui.cpp`, `ui_smooth.cpp`, `script_input.cpp`) |
| vblank rate / interrupt storm | n/a | n/a | **finding**: SDK 0.10.0 vblank loop `while (current - last >= interval)` (unsigned) never exits if the guest tick steps back once -> 11.7 M vblank IRQs/s, <10 FPS after 7-30 min; workaround `clock_no_scaling=true`; proper fix: clamp `last` to `current`, cap catch-up. Our `d61c597` still has the unclamped loop, but our `UpdateGuestClock` is a locked monotonic accumulator, so the trigger (non-monotonic `QueryGuestClockFast`) appears absent here. |
| **NVIDIA Reflex / AMD Anti-Lag 2** | absent | **solved** (`rexglue-low-latency.patch`, see s.5) | **solved** in its own renderer (`src/gpu/latency.cpp`, `latency_d3d12.cpp`, `latency_vulkan.cpp`), port of R's design |
| **In-game options menu** | **yes** (game's own pause menu, see s.6) | **yes, extended** | **yes, origin** |
| Resolution / render scale | 720p | 1x/2x/3x (`resolution_scale`), VRAM trims (`render-target-max-height`, `render-target-trim`, `gpu-memory`) | own `render_scale.cpp`, DLSS/FSR/CAS; notes `resolution_scale=2` broke cull/spawn in the SDK path |
| Widescreen / ultrawide / FOV | absent | `src/aspect_ratio.cpp` (PR #38: camera, HUD, world markers) | aspect patch on `flt_8201E7EC` (.rdata; unprotect first) |
| Input / wheel | controllers; PS5 DualSense | dead zones, vibration, PC adaptive triggers (SDL3); wheel declined (points to LARecomp) | wheel DirectInput + FFB (`src/mc_engine/wheel/`), keyboard/mouse with keycaps, adaptive triggers |
| Launcher | build script | .NET launcher + wizard (`launcher/`), shader preparation | ISO install wizard (ImGui), shader pack builder |
| Vulkan backend (our KI-062 glitches) | title screen only | many Vulkan fixes (`vulkan-odepth-scale` plate/lights smear, hoisted uploads, threaded submission, pipeline cache) | Plume Vulkan path |
| D3D12 DEVICE_HUNG after minutes | absent | **solved**: `rexglue-dxbc-relative-constant-clamp.patch` (+ bindless descriptor index), issues #44/#56 | n/a |
| Alpha-to-mask dot lattice | absent | **solved** `alpha-to-mask-tolerance` (alpha 0.99999994), issue #8 | listed as open "dithered alpha" |
| AMD MSAA line sample positions | absent | **solved** `d3d12-msaa-line-guard` (#28) | PR #21 |

---

## 5. NVIDIA Reflex / AMD Anti-Lag 2 details

**RenanQueiroz (`patches/rexglue-low-latency.patch`, ~1,500-line write-up in `patches/README.md`).**
- Files: `include/rex/ui/low_latency.h`, `src/ui/low_latency.cpp` (policy, frame ids, cvars),
  `src/ui/d3d12/d3d12_low_latency.cpp` (+ `amd_antilag2_dx12.h`), `src/ui/vulkan/vulkan_low_latency.cpp`,
  markers in `src/graphics/command_processor.cpp`, swap counting in `src/kernel/xboxkrnl/xboxkrnl_video.cpp`,
  presenter hooks in `d3d12_presenter.cpp` / `vulkan_presenter.cpp` (+ frame-gen variants).
- SDKs: **no NVIDIA SDK shipped or linked**: `nvapi64.dll` from System32 via `nvapi_QueryInterface` with
  interface ids from NVAPI's public MIT `nvapi_interface.h`; only `NV_SET_SLEEP_MODE_PARAMS`,
  `NV_LATENCY_MARKER_PARAMS`, `NV_GET_SLEEP_STATUS_PARAMS`, latency report and marker enum copied with
  `static_assert` sizes. Vulkan: `VK_NV_low_latency2` (+ `VK_KHR_present_id`, timeline semaphores),
  `VkSwapchainLatencyCreateInfoNV`, `vkLatencySleepNV` (bounded wait), `VkLatencySubmissionPresentIdNV`.
  AMD: `ffx_antilag2_dx12.h` (MIT, loads `amdxc64.dll`), Vulkan `VK_AMD_anti_lag`. Both AMD paths UNTESTED.
- Frame model: a guest frame = engine timer pass (hook `mcla_frame_start` at `0x821BDA90`, the timer's first
  instruction, **before** it reads the time) to its swap packet (`VdSwap`), id = ordinal of the swap packet;
  update/render/CP threads derive the same id without a queue. Sleep at frame start (before input read),
  SIMULATION/RENDERSUBMIT markers in the CP, PRESENT markers around the presenter's present.
- Limiter: `low_latency_limiter` auto/fork/driver: with Reflex on (no frame gen) the cap goes to the
  driver as `minimumIntervalUs`; 60 FPS cap shows input 8 ms newer, steadier dt (sigma 0.002-0.035 ms vs
  0.15-0.22 ms). `mcla_reflex_sleep_assist` waits on a high-res timer until a margin before the modelled
  deadline because `NvAPI_D3D_Sleep` busy-waits (6.4-6.9 ms CPU/frame at 60 cap).
- Robustness: refusal retries, long-wait (>250 ms) give-up with 5..60 s back-off, menu status strings
  (`NO NVAPI`, `NEEDS A NEWER DRIVER`, `SLEEP TOO LONG` ...).
- Measured (RTX 5080): no change at native (GPU not the limit); at 3x D3D12 swap-to-screen 22.0 -> 8.4 ms,
  input-to-screen 38.4 -> 15.9 ms for -2.7..-4.8% FPS; Vulkan -0.8%. Default: on for NVIDIA, off for AMD.
- Relevance to us: we are CP-bound at 33-40 fps (GPU not the limit), so expected latency gain is small
  except at 2160p; the CPU cost of `NvAPI_D3D_Sleep` matters for a CP-bound title. Licence: GPL patch;
  the NVAPI/AMD headers are MIT and could be used directly.

**LARecomp (`src/gpu/latency*.cpp`).** Same design re-written for the Plume renderer (frame generation
left out), cvar `mcla_gpu_low_latency` (auto/off/on/boost, live), row hidden where unsupported; credits
NVIDIA Reflex + Streamline (DLSS) and AMD Anti-Lag 2 in the README. All-rights-reserved code.

## 6. In-game options menu details

All three use the **game's own Flash pause menu**, not a host ImGui overlay (LARecomp additionally has an
ImGui settings dialog, `src/mc_engine/ui/settings.cpp`, mirroring SDK cvar categories).
- Mechanism (RE by LARecomp; addresses listed in R's `config/settings_menu.toml`): capture the pause tab via
  `0x82660644` (PM_Continue state), find the Settings state at `0x82660C2C` (where the dev-only save button is
  disabled; also re-enabled as SAVE GAME), click dispatch `0x826686B8`, cancel `0x82669864`, populate redirect
  `0x82220314` (`sub_82220308`), list render `0x82631FDC` (`sub_82631FD0`) installs rows built from the
  game's own checkbox/value option classes (`kUIObjectCtorFn 0x8263AD60`, alloc `0x8268D578`, string table
  `0x8286D7FC`, VHSM `0x8286D804`). LARecomp needs `MCLA_MenuListNullTableGuard` at `0x8266126C`
  (null table-source deref in `sub_82661210`).
- H: DISPLAY (fps target, HUD updates, vsync, fullscreen, motion blur, DoF), PERFORMANCE (detail options),
  CONTROLS (button prompts, intro speed), QUIT GAME; saved to `mcla.toml`.
- R: Settings tab submenus incl. resolution, graphics API, AA/FSR/frame gen, HDR, Reflex, gamma, aniso,
  Smooth UI, presets, vibration, triggers, language; "(RESTART NEEDED)" labels, RESTART GAME; only changed
  keys merged into `mcla.toml`; text-width levels (`mcla_menu_text_level`). Launcher edits the same file.
  Ported from LARecomp with permission (GPL).
- L: 8 tabs (ReXGlue Settings, Recomp Settings, Performance, Fidelity FX, Debug Camera, Time of Day, Carbon
  Fiber, Languages), plus DISPLAY rows for upscalers/HDR/AO/Reflex, "Restart now?" prompt, auto-save on
  leaving a submenu, garage TIRE OFFSET row, cutscene gallery, server browser.

---

## 7. Worth adopting (ranked by value to us)

1. **XMA decoder: compare our W1 with Canary `aee0871dd`, and check the other Canary/Edge commits**
   (`6bef1fbed` partial-frame loss when Work() ends, `7015473a7`, `5dd1cdbbf`, `ade7e610b`) plus the
   `output_buffer_read_offset` write-back race (we have it behind `xma_work_lock_recheck`, default off).
   Directly relevant to KI-071 residuals and radio balance. Take the code **from Xenia Canary/Edge (BSD-3)**,
   not from R's GPL patch; R's write-up gives the test method (decode vs FFmpeg, L/R correlation).
2. **D3D12 placed texture heaps** (R `patches/rexglue-d3d12-placed-texture-heaps.patch`): matches the
   KI-072/KI-067 symptom profile (streaming hitches, `NtGdiDdDDICreateAllocation` on the CP thread per L #10).
   Idea only (GPL); implement ourselves (first-fit in 256 MB heaps, >128 MB committed, tier-1 heap split).
3. **Timer queue blocking wait / high-res wait** (R `rexglue-timer-queue-blocking-wait.patch`): our SDK
   timer queue still spins a core; R measured at 4 LP +17-21% FPS, p99 27-30 -> 10-12 ms, audio late
   wake-ups 300 -> 20 per run. High value for the Steam Deck (KI-060 underruns, CP wall). Idea only;
   includes a disruptorplus `wait_until` argument-order bug worth checking in our copy.
4. **Warper finish for transitions that end unseen / never start** (H `src/warp_fix.cpp`, L `warper.cpp`):
   our KI-035 fix covers only "callback arrives in state 20"; theirs also finishes a transition that ended
   with the handler never called, and times out one that never starts (duration + 3 s). Candidate fix for
   **KI-026** (Red Light race camera). GPL/no-licence: re-implement from the guest facts (+288 state,
   +296/+300, +276, `sub_8221B790`, `sub_8221B8C8`).
5. **CP thread costs** (R): `gpu-regwrite-bulk` (approach from xenia-canary, BSD), `shared-memory-request-
   fast-path`, `hot-pages-watched-ranges` (cuts VirtualProtect 190,000/s -> ~1,500/s), `threaded-submission-
   d3d12` (design from FH1-ReImagined, MIT; R adds an AMD idle-wait fix), `perf-counters-batched`. Port
   from canary/FH1 originals where possible; otherwise re-implement.
6. **Continuous-time smoothing hooks** for camera/chassis/bike (addresses in R `config/frame_timing.toml`)
   and **UI movie pacing / race countdown** (`0x825EDC6C`, `0x822124DC`): we feed real dt but have none of
   these; relevant to KI-054 camera jumps and to Unlocked correctness. Game addresses are facts; formula
   trivial; re-implement.
7. **Saved-game backup around CREATE_ALWAYS** (R `rexglue-saved-game-backup.patch`) and content-header
   padding zeroing: cheap robustness on top of our KI-068 fix. Re-implement (GPL).
8. **D3D12 DXBC relative constant clamp** (R, DEVICE_HUNG #44/#56) and **alpha-to-mask tolerance** (#8):
   small, high-confidence correctness fixes; check whether our translator has the same unbounded index.
9. **Reflex / Anti-Lag 2**: low priority while CP-bound; if done, use NVAPI public MIT header + AMD MIT
   header and R's frame-id design (idea). Mind `NvAPI_D3D_Sleep` busy-wait CPU cost.
10. **vblank loop defensive clamp** (L TECHNICAL_NOTES s.4): one-line hardening
    (`if (current < last) last = current;` + catch-up cap) even though our clock looks monotonic.
11. **`mc_FlushDataCache` (`0x821D5510`) early return keeping the fence** and **fence spin throttle in
    `sub_82412F98`** (L): measure first; our CP-bound profile may benefit. Facts only (no licence).
12. **In-game settings menu** in the game's pause menu: proven approach and addresses (R
    `config/settings_menu.toml`); code is GPL (R/H) or unlicensed (L), so re-implement from the hook map,
    ideally asking mzzvxm for permission as R/H did.
13. Long-term watch: LARecomp's **no-CP native renderer** is the structural answer to KI-067/072 but is
    not published under any licence and depends on an unpublished SDK; track only.
