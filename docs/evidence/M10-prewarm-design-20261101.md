# M10-003 design: shader and pipeline pre-warm before the first launch

Read-only design by an Opus subagent on 2026-11-01 (SDK v0.20.0.6 pipeline_cache.cpp, owner runtime logs 2026-10-25..11-01, store record counts only; no shader bytes copied); accepted by the primary agent. Recommended first step: carry the local store across builds and BENCH copies (launcher merge) plus the SDK creation-thread count fix; nothing is distributed.

Read-only design study, 2026-11-01. Sources: `docs/M10-priorities.md` (M10-003 row, risks),
`docs/M9-priorities.md` (pre-warm note), `docs/evidence/M10-001-run1-1440p-cold-20261101.md`,
`docs/known-issues.md` (KI-037, KI-047, KI-056), `docs/source-data-policy.md`, SDK v0.20.0.6 source
(`src/graphics/d3d12/pipeline_cache.cpp`, `include/rex/graphics/d3d12/pipeline_cache.h`,
`src/graphics/graphics_system.cpp`, `src/ui/rex_app.cpp`, `src/graphics/vulkan/pipeline_cache.cpp`),
launcher `src/mcla_launcher_controller_win.cpp` and `src/mcla_launcher_build_win.cpp`, owner runtime
logs of 2026-10-25..2026-11-01 (storage and wait lines only) and the owner's local store files
(record counts and hashes only; no shader content was copied or kept).

Nothing was built or launched. Two helper scripts used for counting live next to this file
(`scan.py`, `union.py`).

## 1. What the store is and what happens cold versus warm

### 1.1 What is persisted

Per store directory `<profile>/cache/<build-fingerprint>/shaders/shareable/` (the SDK's own name
"shareable" means "movable between hosts"):

| File | Content | Size (owner) | Nature |
| --- | --- | --- | --- |
| `545407F8.xsh` | Header `XESH` + version `0x20201219`; then per guest shader: XXH3-64 of the ucode, dword count, type, **raw Xenos ucode dwords** | 230-330 KB, 362-669 shaders | **Game data** (copyrighted shader microcode) |
| `545407F8.rtv.d3d12.xpso` | Header `XEPS` + `DXRT`/`DXRO` + version `max(0x20210425, Modification::kVersion)`; 72-byte records: XXH3 of the description, VS/PS ucode hashes, VS/PS translator modification bits, fixed-function state (depth/stencil/blend/RT formats/topology/cull/MSAA) | 44-76 KB typical, up to 3,605 records | Derived metadata: hashes and state, no shader bytes |
| `545407F8.{fsi,fbo}.vk.xpso` | Same idea for the Vulkan backend (shares the `.xsh`) | not present on the PC yet | Derived metadata |

Not persisted by the SDK: translated DXBC/DXIL, root signatures, and D3D12 PSO blobs. The source
comment says host PSO blobs "would go to `shaders/local/`" but were never implemented;
`CachedPSO` is always null and there is no `ID3D12PipelineLibrary` and no `VkPipelineCache`
persistence. The only binary PSO cache is the **driver's own disk cache** (NVIDIA/AMD DX cache on
Windows; Mesa/vkd3d-proton caches under Proton), which is outside our control, per machine and per
driver, but survives our build changes when the translated bytecode and state are identical.

### 1.2 Keys and invalidation

- The file formats are keyed only by the three version constants above, the title id and
  ROV vs RTV (and backend). They do **not** depend on the host build, the SDK commit, the GPU,
  the resolution scale or the driver. Records are validated by XXH3 and the file is truncated at
  the first bad record.
- Translation is redone every run with the current translator (vendor id, bindless, ROV, gamma,
  MSAA support, **draw resolution scale** are translator inputs, not keys), so a store written by
  an older SDK is still correct input for a newer one unless someone changes the
  `PipelineDescription`/`Modification` layout without bumping `kVersion` (the fork rule to keep).
- What makes the owner's runs cold is the **launcher**, not the SDK: the store directory is
  `cache/<SHA-256 of the build manifest>` **inside each profile**. Every new build (any file change)
  and every new profile copy (every fresh BENCH copy) starts empty. The owner's machine holds 52
  such directories.

### 1.3 Warm start (store present)

Blocking, inside `RexApp` deferred launch after `PrepareModuleLaunch` and **before** the guest
main thread is resumed ("Initializing shader storage for title 545407F8..."):

1. Read all `.xpso` descriptions, collect needed (ucode hash, modification) pairs.
2. Read `.xsh`, ucode analysis + DXBC translation on up to cores-1 "Shader Translation" threads.
   Owner logs: 376-628 shaders in **6-32 ms**.
3. Create every stored PSO on the "D3D12 Pipelines" threads and the calling thread. Owner logs:
   468-3,605 PSOs in **35-90 ms** in most warm starts (138-174 ms in two); **394-1,568 ms** in 8 of 33 warm
   starts (driver cache partly cold: new driver, other scale so different DXBC, or eviction).
4. Then the game runs; only content not yet in the store compiles at draw time. Warm l53 minute 1
   still had 55 waits / 299 ms (content the cold run had not stored); later warm runs 2-48 waits/min.

Side finding (SDK, low risk, worth fixing with this work): in step 3 the loop that should add extra
creation threads uses `creation_thread_original_count` instead of `creation_thread_needed_count`,
so no extra threads are ever started and the "blocking" completion wait (only taken when extra
threads exist) never runs. The startup creation therefore uses only the persistent creation threads
(3/4 of the cores) plus the CP thread, and the last few PSOs may still be compiling when the game
starts; the "Created N ... in X ms" line understates the real time. Harmless on a warm driver cache,
relevant once thousands of PSOs are pre-created on a driver-cold machine (Deck).

### 1.4 Cold start (store empty)

No startup work. Every first-seen guest shader is appended to `.xsh` by the storage writer thread;
on its first draw `ConfigurePipeline` queues **translation and PSO creation** to the creation
threads, and the CP thread waits (l51 always-on bounded wait; before l51 the draw was skipped).
Each new state combination of a known shader is a new PSO and another wait.

Measured first minutes (always-on wait, 0 skips, 0 timeouts in all):

| Run | Minute 1 waits / total / longest | Minutes 2-4 waits |
| --- | --- | --- |
| l51 cold 1440p | 406 / 2,090 ms / 182 ms | n/a |
| l52 cold (owner session) | 344 / 1,799 ms / 50 ms | 41, 40, 12 |
| l53 BENCH-B cold 1440p | 448 / 434 ms / 28 ms | 17, 23, 32 |
| 2026-11-01 cold session A | 607 / 2,777 ms / 59 ms | 44, 48, 45 |
| 2026-11-01 cold session B | 453 / 2,655 ms / 65 ms | 15, 55, 170 |
| l53 warm 1440p | 55 / 299 ms / 32 ms | 13, 3, 2 |
| l51 warm | 122 / 1,207 ms / 53 ms | n/a |
| other warm sessions 2026-11-01 | 3-10 / 0-7 ms | 2-133 (new content) |

Where the cost is: from the F10 audit windows, guest shader translation costs about 75 us per
shader (cheap); PSO creation costs 0.1-0.4 ms each when the driver cache hits and about 1.9 ms each
when it misses. So the cold penalty is (a) **driver compilation** of new PSOs, and (b) **CP-thread
serialisation**: the CP blocks on each one at the moment of first use, mostly at world load. The
spread of cold minute 1 (0.43 s to 2.8 s for about the same wait count) is consistent with the
driver cache: when an earlier build at the same scale already compiled identical bytecode, the
driver hits even though our store is cold. (Inference from the numbers, not separately measured.)

### 1.5 Relation to KI-047 and KI-037/KI-056

- KI-037 (first photo) and the skip mechanism of KI-047 are already covered by the always-on wait
  (0 skips). Pre-warm removes the waits, not a correctness hole.
- KI-047 (police livery, open): the leading cause is now a stale `some` read-back of a shared
  composite key (high), and it was caught on warm 720p too. The cold correlation (about 3 of 10 cold
  versus 0 of 10 warm) is at most a timing amplifier (CP stalls widen the race). **Pre-warm is not
  a KI-047 fix**; it may lower the rate on the first session of a build. Do not use KI-047 as the
  acceptance metric for M10-003.

## 2. Options

### (a) Ship a pre-recorded pipeline description set and pre-create at first launch

What it needs: the PSO needs translated shaders, which need the **ucode**. A shipped `.xpso` alone
(hashes + state) is useless at startup without the `.xsh`, and the `.xsh` is game data that may not
be distributed (`source-data-policy.md`: no extracted game assets; raw guest memory/pages). So a
shippable set must be keys only, and the ucode must come from the user's own game at run time.
Three ways to get the ucode locally:

1. **Lazy expansion (SDK, moderate):** ship the union `.xpso` (derived) as a read-only "hint" set.
   When a guest shader hash is seen for the first time (`LoadShader`), immediately queue
   translation of every modification and every PSO in the hint set that references it, in the
   background. First use of a shader still waits once; all its other state variants are ready.
   Expected: removes the "many PSOs per shader" part of the cold minute (union data: 758 shaders,
   6,822 descriptions, about 9 PSOs per shader), not the per-shader first wait.
2. **Effect-load harvest (host + RE, larger):** RAGE loads its effect files (`.fxc`, all
   techniques) at boot and at level load, long before draws. A host hook on the effect loader
   (same style as the existing `mcla_photo_capture` hooks) hashes each ucode blob in guest memory,
   matches it against the shipped hash list and feeds ucode + hint descriptions into the pipeline
   cache during the game's own loading screens. Needs RE of the loader and proof that the hashed
   bytes equal what the CP later sees via IM_LOAD (unverified). Gives the full Azimuth-style effect
   without distributing ucode.
3. **Offline extraction from the user's dump (launcher):** parse the user's RPF archives to find
   the effects. This is the "asset-free prepare pipeline" the M9 plan already calls large; not
   recommended for M10.

Privacy/legal: `.xpso` contains XXH3 hashes of ucode plus fixed-function state, no game bytes;
it is comparable to the hash/metadata the policy already allows, but it is a **new artifact type
and distribution channel**, so `source-data-policy.md` must be amended and the file reviewed before
it ships. Keys recorded on volunteer machines may only come back inside the volunteer boundary
(M9-005 private channel), never the `.xsh`; diagnostic/metadata exports must keep excluding
`cache/`. The `.xsh` never leaves the machine that produced it.

Effect: (a1) partial, (a2) near-complete for first sessions on new installs. Effort: a1 about 2-3
days SDK + tests; a2 1-2 weeks including RE. Risk: a1 low (only background work, behind a cvar);
a2 medium (hook correctness, memory). A 6,822-PSO pre-creation may also cost noticeable driver
memory on the 8 GB card (KI-048 budget) — measure process commit/VRAM before and after.

### (b) Carry the store across builds and profiles (local only)

What it needs: launcher change only. At session preparation, if `cache/<fingerprint>` has no store,
seed it by merging the stores of earlier fingerprints (newest first; dedupe `.xsh` by ucode hash and
`.xpso` by description hash; write valid headers; keep the file versions of the target SDK and
skip sources whose header version differs). Better still, keep one **installation-level union
store** per backend that every profile and BENCH copy seeds from, and merge new records back after
a clean session end. No SDK change needed: the store format does not depend on the host build, and
the SDK re-validates and re-translates everything.

Invalidation: the SDK version constants (`XESH 0x20201219`, `XEPS max(0x20210425,
Modification::kVersion)`), ROV/RTV, backend. Key the shared store by those, not by the build
fingerprint; then it survives every host-only rebuild and every SDK rebuild that does not bump a
format version. Guard: a launcher-side "SDK store format" string from the build manifest so a fork
change can force a reset.

Privacy/legal: local copies of the user's own derived data on the user's own machine; nothing is
distributed. The owner may copy his own store to his own Deck (same `.xsh` and `.rtv.d3d12.xpso`
under Proton's D3D12 path; the Vulkan arm writes its own `.vk.xpso`).

Effect: on the owner's PC the union of the 52 existing directories already covers 758 shaders and
6,822 descriptions; every new build or BENCH copy would start at least as warm as the best existing
profile (minute 1 at the warm level: 3-55 waits instead of 344-607). For volunteers who update
builds, every build after their first is warm. Does nothing for a brand-new install.
Effort: 1-2 days launcher + host tests. Risk: low; the store is self-validating. Startup grows with
the union size (measure "Created N in X ms"; on a driver-cold machine thousands of PSOs may take
seconds; see (d)). Measurement rule impact: BENCH runs must state "seeded" or "cold"; a launcher
option to disable seeding is required for cold baselines.

### (c) Driver-level caches

- Windows: the NVIDIA/AMD driver disk cache already works implicitly and already survives our
  builds (warm loads of 3,600 PSOs in 44-62 ms). An `ID3D12PipelineLibrary` in `shaders/local/`
  keyed by adapter + driver version + SDK translator would only help when the driver cache is cold;
  measured gain on the PC would be small. Not portable across GPUs or drivers (M10 risk note); never
  distributable. Effort 2-4 days SDK; low priority.
- Deck (Proton, vkd3d-proton or native Vulkan): Mesa/RADV keeps its own on-disk cache and
  vkd3d-proton has an internal pipeline cache (exact defaults to verify on the Deck's Proton
  version). Steam's shader pre-caching/Fossilize is tied to Steam app ids and Valve's pipeline
  collection; for a non-Steam shortcut it is not a channel we control. Recommendation: do not build
  anything here; measure on the Deck whether the second launch after a driver-cold first launch is
  already fast, which tells whether the driver cache does the job.

### (d) Background pre-creation at the title screen / with progress

- With any key source ((b) or (a)) the pre-creation already happens: the SDK creates all stored
  PSOs at startup, blocking, before the guest starts (black window, no progress). Options:
  1. keep blocking but fix the thread-count bug and log real wall time (cheap, step 1);
  2. non-blocking initialisation (`GraphicsSystem::InitializeShaderStorage(..., false)` path
     exists) so the title screen runs while PSOs compile; early draws simply wait if they hit a
     pending one (bounded wait already on);
  3. a launcher "Preparing shaders N/M" pass: start the host in a pre-warm-only mode (create device,
     load store, create PSOs, write progress to a status file, exit without resuming the guest).
     Only worth it where startup creation takes seconds (Deck, driver update, union of thousands).
- On its own (no key source) (d) does nothing for a cold store.

Effort: d1 0.5 day, d2 1 day + regression check of early frames, d3 2-3 days launcher + host.
Risk: d2 can move waits into the intro instead of removing them; d3 adds a launcher state.

### Summary

| Option | Removes cold first-minute waits for | Ships game data? | Effort | Risk |
| --- | --- | --- | --- | --- |
| (b) seed/union store locally | every new build and BENCH/profile copy on a machine that has played before | no (local only) | 1-2 d | low |
| (d1) fix creation threads + timing | n/a (makes (b) startup honest and faster) | no | 0.5 d | low |
| (a1) shipped hint `.xpso`, lazy expansion | part of the cost on new installs | no (derived keys; policy amendment) | 2-3 d | low |
| (a2) effect-load harvest + hint set | most of the cost on new installs | no | 1-2 w | medium |
| (d2/d3) background or launcher progress | startup seconds on driver-cold machines | no | 1-3 d | low-medium |
| (c) PSO library / Deck caches | driver-cold starts only | never distributable | 2-4 d | low gain |

## 3. Recommended plan

Each step behind a default-off switch until the owner regression passes (M10 risk rule), one
change per candidate.

**Step 0 — counters (with step 1's build).** Already present: "Translated N shaders ... in X ms",
"Created N graphics pipelines ... in X ms", per-minute wait summary (waits, total, longest,
timeouts, skips), `draws_pso_skip`. Add:
- `MCLA_PIPELINE_STORE v=1 source=<cold|seeded|union> seeded_from=<n dirs> xsh=<records>
  xpso=<records> format=<xesh/xeps versions>` (launcher receipt + host log line);
- per minute: PSOs created at run time (not from storage) and their summed creation ms, and
  shaders translated at run time;
- startup: wall ms from "Initializing shader storage" to the guest resume, after all creation
  threads are idle (needs the d1 fix), plus process commit before/after.

**Step 1 — (b) local seeding + (d1).** Launcher: seed a new fingerprint directory from a
per-installation union store (merge, dedupe, version-checked; option "Seed shader cache" default
on for owner builds after the A/B, off switch kept for cold baselines); merge new records back at
clean session end. SDK: use `creation_thread_needed_count` and keep the blocking completion wait.
Tests: launcher unit tests for merge (dedupe, truncated file, wrong version, ROV/RTV, Vulkan file
kept separate), host suites, SDK ctest.

Owner test (PC, l-next): two fresh BENCH copies from MAIN through the launcher.
Arm A seeding off (cold), arm B seeding on; same 1440p Unlocked bench route; two runs per arm
(harness rule). Record for minute 1 and minutes 2-5: waits, total ms, longest, skips, run-time PSOs;
the startup line and time to title. Pass: arm B minute-1 waits and wait ms at or below the warm
reference (l51 warm 122 / 1.2 s; l53 warm 55 / 0.3 s) with 0 skips, startup growth under 1 s, no
regression in first photo, police livery and album preview (correctness travels with pipeline
changes). Then the Deck: copy the owner's own seeded store to the Deck profile and run cold vs
seeded the same way (Deck numbers taken on the Deck).

**Step 2 — (d2/d3) only if step 1 shows seconds of startup** on the Deck or after a driver update
(measure by clearing only the DirectX shader cache through Windows Disk Cleanup, a non-destructive
cache operation the owner can choose to do). Prefer d2 if early-frame waits stay low; d3 if the
owner wants a visible progress bar.

**Step 3 — (a1) hint set for volunteers.** Policy amendment for a shipped `.xpso` hint file (hashes
+ state only, owner-reviewed, built from the owner's union; volunteer keys only via the private
channel). SDK: on first `LoadShader` of a hash, queue all hint descriptions for it in the background.
Owner test: a fresh install-like profile with an empty union store, cold vs hint, same counters.
Expected partial gain; decide on (a2) from the measured remainder.

**Step 4 — (a2) research spike, timeboxed 2 days:** locate the RAGE effect loader, check that
hashed ucode from the loaded effect equals the CP's ucode hash for a sample of shaders (counts only
in evidence, no ucode in reports). Continue only if the match rate is high.

**Not recommended for M10:** distributing any `.xsh`, driver PSO blobs or Fossilize data; RPF
extraction; using KI-047 frequency as the success criterion.

## 4. Open questions

- Why some profiles hold 3,600 descriptions against about 700-1,000 for others (multiple scales,
  MSAA or ROV/RTV experiments in the same fingerprint?). Matters for the union size and startup
  cost; check record distribution by `host_msaa_samples` and render-target formats (counts only).
- Driver memory per PSO on the 8 GB card when pre-creating 6,800 (KI-048 budget).
- Deck: which caches persist under the current Proton for a non-Steam shortcut, and whether the
  Vulkan backend (l59 A/B) changes the plan (it needs its own `.vk.xpso`; the `.xsh` is shared).
