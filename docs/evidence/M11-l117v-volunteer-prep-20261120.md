# M11 volunteer candidate l117v: preparation up to signing (2026-11-20)

Preparation by an Opus subagent for the primary agent. l117v is the next volunteer build after l81v: host main `ecfc182` (HEAD bytes) against a volunteer build of the l116 SDK `d889a16` with the FidelityFX presenter shaders compiled out. Built, unit-tested, scanned and staged as input; **not signed, not staged as a package, not published, not launched.** No `mcla.exe`, launcher or game process was started; no key, ticket, save, profile or MAIN was touched; no git write in the main checkout (no `git add`, no commit, no push). `wait-for-idle-pc.ps1` ran before every build and test command and returned idle each time. Spark not used: every step was a deterministic build, test, scan or source read.

## a. Base: host ecfc182 with SDK d889a16 (l116), not e6eeeb9 (l117)

**Question.** l117's SDK `e6eeeb9` adds three default-off per-draw levers; L3 `d3d12_tex_outdated_narrow` is known UNSAFE when enabled (140 k mismatches in the owner's cp-verify2 run, `ecfc182`). Volunteer builds compile the arm reader out, so the lever is off unless something else can set an SDK cvar.

**Answer: a tester can set any SDK cvar, but only outside the supported launcher path.**
- Supported path (launcher): the launcher passes a fixed argument list (`src/mcla_launcher_controller_win.cpp:934-948`) and removes inherited `REX_*` variables from the child (`src/mcla_managed_config_policy.h:12-13`); the managed session's config path is the comment-only sentinel `managed.toml`, so no `mcla.toml` is read (`src/mcla_app.cpp:1112-1180`, `OnConfigurePaths` `:1184`); and in a managed launch the SDK settings (F4) and console (backtick) overlays are unbound (`src/mcla_app.cpp:1280-1284`). The launcher UI has no cvar setting. Nothing a volunteer can set through the launcher reaches an SDK cvar.
- Unsupported path: a volunteer build admits a direct start of `mcla.exe` (`src/mcla_app.cpp:1114-1124`, "including direct EXE startup"). Then the SDK parses every `--<cvar>=` flag (`private/l116-sdk-wt/src/core/cvar.cpp:533-551`), every `REX_<CVAR>` environment variable (`cvar.cpp:72`, `:222`, `:602`), a `mcla.toml` next to `mcla.exe` (`src/ui/rex_app.cpp:148`, `:160`; `cvar.cpp:587-594`), and F4 / backtick open the settings and console overlays that set any cvar (`rex_app.cpp:404`, `:411`; `src/ui/overlay/settings_overlay.cpp:354`, `console_overlay.cpp:235`). `d3d12_tex_outdated_narrow` is a hot-reload bool (`private/l117-sdk-wt/src/graphics/d3d12/command_processor.cpp:257`, latched at `:2792`), so in an l117-SDK build F4 would show it as a checkbox.
- **Decision (per the task rule "if a tester could enable it, prefer l116's SDK"): SDK `d889a16`.** It has no l117 lever code at all (`d3d12_tex_outdated_narrow` not defined anywhere in `private/l116-sdk-wt`). The host side of l117 is key tables only (`2ef5dc3`: `src/mcla_dev_overrides.h`, `src/mcla_m10_defaults.h`, tests); with the arm reader compiled out and the l117 cvars only in `kNeverWrittenCvars`, host `ecfc182` is inert against d889a16 (the m10 module writes only kDefaults cvars, all present in d889a16). The default behaviour equals l116 and l117 (l117 changed no default).
- **Not taken:** the MCLA_THIN zero-GPU-time fix of `e6eeeb9`. It only affects which frames a diagnostic line reports (`kind=thin` frames whose GPU time reads exactly 0); volunteer logs are not analysed with it, and cherry-picking it would fork the SDK history away from the published chain (see e).
- **Residual risk of the same class (not new with l117):** d889a16 also exposes, through the same unsupported path, known-unsafe dev levers such as `shader_im_load_memo` (`private/l116-sdk-wt/src/graphics/d3d12/command_processor.cpp:181`; 131 mismatches on the Deck in l96). **Proposal (no code changed):** in builds without `MCLA_DEV_OVERRIDES`, unbind `bind_settings` / `bind_console` for direct starts too (or refuse a direct start in volunteer builds), so the only cvar path left is the command line / environment / `mcla.toml` that a tester must type deliberately.

## b. Volunteer flavour versus the internal build (from the source)

| Item | Internal (owner) build | l117v volunteer build | Source |
|---|---|---|---|
| Arm reader `mcla-dev-overrides.txt` | read (180 keys) | compiled out (`MCLA_DEV_OVERRIDES` OFF) | `src/mcla_dev_overrides.cpp:14-26`, `:194-199` |
| F10 live-package retention | unlimited, refuses below 2 GiB free | newest 50 within 768 MiB, no free-space floor (l81v: 10 / 256 MiB) | `src/mcla_diagnostic_trigger.h:400-426`, `src/mcla_diagnostics.cpp:107-110` |
| `memory_watch_census` | SDK default true | false (written by m10 defaults) | `src/mcla_m10_defaults.h:203` kDefaults (diagnostic=true), Decide `:625`; SDK `src/system/watch_census.cpp:27` |
| `d3d12_readback_gpu_timing` | SDK default true | false | same; SDK `src/graphics/d3d12/command_processor.cpp:465` |
| All other m10 defaults (HiDef, strict, lever 1, per-draw levers, low latency auto, transfer elision and extent bound on Windows, Wine bridging / upload-ahead) | written | written, identical | `src/mcla_m10_defaults.h:203-238` kDefaults (20 entries) |
| Perf split | on (`MCLA_PERF_SPLIT_DEFAULT`) | on (as l81v) | `src/mcla_perf_split.cpp:31-32`, `:159-170` |
| FidelityFX CAS / FSR1 presenter effects | compiled in (`REXGLUE_ENABLE_FFX_PRESENTER_SHADERS` ON) | compiled out (OFF) | SDK `CMakeLists.txt:19`, `src/ui/CMakeLists.txt:120`, `src/system/CMakeLists.txt:103` |

**Always-on diagnostics added since l81 and their cost in a volunteer build** (none needs a change; proposals only):

| Diagnostic | Where it runs | Cost class | Proposal |
|---|---|---|---|
| MCLA_GPU_SURF draws-by-key (l105/l107) | CP, every draw: a last-key compare and an increment, linear search only on a key change (`include/rex/graphics/util/gpu_category_timing.h:507-525`, call `command_processor.cpp:6369`) | per draw, a few ns | keep |
| MCLA_GPU_CAT counters (l105) | CP, per draw integer adds; GPU timestamps only with `d3d12_gpu_category_timing` (default false, `:470`) | per draw, a few ns | keep |
| MCLA_XFER census (l107) | per ownership transfer counters; the per-draw transfer tracker runs only with the trigger profile or lazy verify (`src/graphics/d3d12/render_target_cache.cpp:500`, `:1642-1646`) | per transfer | keep |
| MCLA_THIN / MCLA_BLACK (l116) | per swap: repeat-present check; black probe samples 144 texels of a buffer already in guest memory, no read-back (`command_processor.cpp:411`, `:5101-5145`) | per frame, microseconds | keep (KI-086 evidence from volunteers) |
| MCLA_DIP (l101) | per-second interval counters | per frame | keep |
| MCLA_CP_PACKETS (l96) | CP, one increment per PM4 packet | per packet, ns | keep |
| MCLA_TEXMEMO miss reasons (l101) | per texture memo miss | per miss | keep |
| MCLA_GTILE (l107, host) | render thread, at BeginTiledRendering (a few calls per frame): reads and a mutex; no guest write at default (`src/mcla_guest_tiling.cpp:1-16`) | per frame | keep |
| Traffic probe hooks (l107, host) | one cached static bool test per hooked call (`src/mcla_traffic_motion.cpp:32-45`), probe default off | negligible | keep |
| Guest tiling / MSAA override (l107, host) | `mcla_guest_msaa_scene` 0 = no change (`src/mcla_guest_tiling.cpp:29-35`) | none at default | keep |
| Perf split (since l46) | CP draw-phase timing, TSC since l98 | the largest of these; carried from l81v | keep (volunteer logs carry MCLA_PERF_SPLIT, needed for hitch reports) |

## c. Licences

- The l81v package carried **no** licence or notices file: the volunteer audit allows exactly one root text file, `TESTER-GUIDE.txt` (`scripts/Test-MclaVolunteerStaging.ps1:19-25`; staging parameter `-TesterGuide`, `scripts/Stage-MclaInternalLauncher.ps1:6-7`, `:123`, `:177-179`). Xenia-derived code is covered by the ReXGlue SDK `LICENSE` (BSD-3, names the Xenia project), which therefore also belongs in the notices.
- New third-party code in the volunteer DLLs since l81v: the NVIDIA NVAPI headers and the AMD Anti-Lag 2 header (both MIT, vendored in l95: `private/l116-sdk-wt/thirdparty/nvapi`, `thirdparty/amd_antilag2`, compiled into the low-latency layer, `src/ui/CMakeLists.txt:164-175`).
- **FidelityFX CAS / FSR1: built OFF.** The presenter effects are reachable only through dev overrides (`present_fsr` / `present_cas` keys) or the unsupported direct-start cvar path, the launcher does not expose them, and the AMD FidelityFX MIT notice is not in the SDK tree (only compiled bytecode is committed: `src/ui/shaders/bytecode/d3d12_5_1/guest_output_ffx_*`; recorded as missing in `M11-l105-owner-build-20261115.md`). OFF is the upstream default and the l81v state; the scan below confirms no CAS / FSR shader strings in the volunteer DLLs.
- **Prepared file:** `private/l117v-THIRD-PARTY-NOTICES.txt` (6790 bytes, ASCII CRLF, SHA-256 `3EAA8C437AB88B1C2A72AFB883EAB01701C4D2F286690C10B8C1CCE23DF876EB`), generated by `mk_notices.py` from the exact files: MCLA-R `LICENSE` (`7837689710A6...6428`); ReXGlue SDK `private/l116-sdk-wt/LICENSE` (`8E065BE1DA2F...BB18`); `thirdparty/nvapi/License.txt` (`84726B60C17F...D8BA`, the README-MCLA pin) plus the copyright line of `thirdparty/nvapi/nvapi.h` line 3; `thirdparty/amd_antilag2/LICENSE.txt` (`020463D3B92D...5C71`, the README-MCLA pin). No text came from the network.
- **How it ships without changing tracked scripts:** the prepare script writes `private/l117v-TESTER-GUIDE.txt` = `docs/volunteer-tester-guide.txt` + separator + the pinned notices, and stages that as `TESTER-GUIDE.txt` (allowlisted, recorded in the receipt with its hash). Alternative for the primary: add `THIRD-PARTY-NOTICES.txt` to the audit allowlist and a `-Notices` staging parameter (a tracked-script change, not made).
- **Gap carried from l81v (not new, not solved here):** the SDK DLLs also contain other third-party libraries with their own notices in `private/l116-sdk-wt/thirdparty` (fmt, spdlog, imgui, toml++, CLI11, SDL3, xxHash, snappy, utfcpp, o1heap, aes_128, FFmpeg (LGPL/GPL texts), libmspack, and TracyClient.dll from `thirdparty/tracy`). Their review belongs to REL-002 / REL-004; the l81v package shipped without them too.

## d. Build, gates, staging input

Scratch folder `%LocalAppData%\Temp\claude\C--BDU-MCLA-Recomp\l117v-work` (scripts and logs: `l117v_sdk.ps1`, `make_src.py`, `l117v_host_build.ps1`, `l117v_host_tests.ps1`, `stage_input.py`, `l117v_scan.py`, `mk_notices.py`, `mk_guide.py`, `mk_prepare.py`, `prepare-template.ps1`, `warndiff2.py`; `sdk-configure.log`, `sdk-install.log`, `sdk-warndiff.log`, `sdk-ctest.log`, `host-configure.log`, `host-build.log`, `host-tests.log`, `stage_input.log`, `l117v-scan.log`, `dryrun.log`, `dryrun-refuse.log`, `gate-build.log`, `gate-tests.log`).

| # | Step | Result |
|---|---|---|
| 1 | Volunteer SDK: new build tree `private/l117v-sdk-b` from worktree `private/l116-sdk-wt` (branch `mcla/m11-l116`, HEAD `d889a169703a1ce9e4701b7e856028cf04d57123`, clean; used read-only), output `private/l117v-sdk-o`, install `private/l117v-sdk-install` | configure rc 0, `Version: 0.20.0.22-dev.gd889a16`, `REXGLUE_ENABLE_FFX_PRESENTER_SHADERS:BOOL=OFF`; every other option equal to `private/l116-sdk-b` (Ninja Multi-Config, C++23, clang 20.1.8, `-march=x86-64-v3`, D3D12 only, Tracy and perf counters ON, tests ON, same prefix map and linker flags); build + install rc 0, `INSTALL OK` |
| 2 | SDK warnings against the l117 full fresh-tree set (`l117-work/w117all.txt`) | 213 normalized vs 210: **3 new**, all in upstream presenter code that is used only with the FFX define (`d3d12_presenter.cpp` unused `temporal_effect_selected`, `presenter.cpp` unused `output_width_clamped` / `output_height_clamped`); 0 gone |
| 3 | SDK ctest | `100% tests passed, 0 tests failed out of 2398` (4 BitStream skipped, as before), 23.8 s |
| 4 | Host source | scratch `l117v-host-src` = main HEAD `ecfc1826693cb201a8a4c6a007a4c0930e29c6fb`, 631 tracked build files (HEAD bytes; tracked src/tests/CMake clean; untracked `tests/` files not copied) + `generated/` + inja/xxHash headers |
| 5 | Host configure (`private/l117v-host-build`) | rc 0, `Found ReXGlue SDK 0.20.0.22-dev.gd889a16`; cache: `MCLA_DEV_OVERRIDES:BOOL=OFF`, `MCLA_EXPERIMENTAL_F10_WGC:BOOL=OFF`, EXPIRY / RAW_TIMER / PERF_SPLIT / LIVERY_DIAG2 / READBACK_SYNC_COMPOSITE / PREWARM_SEED / M10_SPEEDUP / image and diagnostic tests ON (the l81v set) |
| 6 | Host build | 295/295 steps, rc 0, **0 warnings** |
| 7 | Host tests (`l117v_host_tests.ps1`) | `count 60`, **`failed executables: 0`** (`mcla_dev_overrides_test: 7388 checks passed`, `mcla_m10_defaults_test: 18145 checks passed`) |
| 8 | Primary gates, main checkout: `gate_host_build.ps1 build` | rc 0, `ninja: no work to do.` (already current at ecfc182), **0 warnings** |
| 9 | `l57_host_tests.ps1` | `count 60`, **`failed executables: 0`** (7388 / 18145 checks) |
| 10 | Volunteer binary scan (`l117v_scan.py`, 26 checks) | **failures: 0**: mcla.exe lacks `MCLA dev override`, `MCLA dev overrides`, `conflicting keys for one cvar`, `line {} ignored`; the launcher lacks `mcla-dev-overrides.txt` and `MCLA dev override`; UTF-16 `MCLA test-build admission` and `MCLA m10 default` present; no build / user / temp / repo paths in any of the 7 binaries (ASCII and UTF-16; the Windows user name in path form included); no FidelityFX presenter shader strings (`xe_cas_source`, `xe_fsr_easu`, ...) in either SDK DLL, while the l116 owner `rexruntime.dll` has all four (reference); `nvapi64.dll`, `amdxc64.dll`, `MCLA_LOWLAT` present in `rexruntime.dll`; no l117 lever strings (`d3d12_cb_reuse`, `d3d12_tex_outdated_narrow`, `MCLA_CP_L117`) in the SDK DLLs |
| 11 | Prepare script parse | 0 errors in PowerShell 7.4.22 and 5.1.26100 |
| 12 | Entries dry run (`-DryRunDirectory`, empty temp directory) | 1 entry, host `A65B6E7E...9DF2`, 30 days; a second run into the now non-empty directory refused; directory removed. No entries, list, ticket, guide or staging output exists in `private/` |

The volunteer audit (`Test-MclaVolunteerStaging.ps1 -RequireVolunteer`) needs the signed ticket; it runs inside the prepare script.

**Staged input `private/l117v-volunteer-input`** (no ticket; `private/l117v-volunteer-input.SHA256SUMS.txt`, SHA-256 `C86809F96E367634582B1798566E2F928B30FDBE7EB906F49A318589EB472B61`; every file differs from the l116 owner input):

| File | Bytes | SHA-256 |
|---|---|---|
| MCLA-Launcher.exe | 1452544 | 83F558146E5C0B5D2F1002CB53F8133D6B261C114B0CB3B7AEAA91029C1BE565 |
| mcla.exe | 53969408 | A65B6E7E86863758E5ED6B5B5DBA14621FAA4E1A8282260AF4E0A95356939DF2 |
| mcla_crash_handler.exe | 211968 | 4E5A1060EED2F22B11531093898C2CB074B4CC5857680C513154ABA8266B3755 |
| mcla_diagnostic_image_helper.exe | 60928 | D4516ADC76E098905BD8153679B36CC43C0D051BF7F00E97A32B67ACC858FC35 |
| rexgpu-xenos.dll | 3898880 | 1EFD7C4ED72BC3BD44E77F4DF9B2A280EABF1D0BDB5321220F261AD3BCD71079 |
| rexruntime.dll | 11702272 | E1F0064FB97C63936D99FB943F6537DCF965BA7304D5124C542EFBA69C9A5418 |
| TracyClient.dll | 246784 | 07C4384D4F46A0481A58CB758C63564F69B3973ACD453F80629DADBBA9C9F85E |

The three DLLs equal `private/l117v-sdk-install/bin` and the build-directory copies (asserted).

**Owner step files (not run):**
- `private/l117v-volunteer-prepare.ps1` (10740 bytes, ASCII CRLF, SHA-256 `AB6B77535FDECC0AC19ADCB18CF2DEAD59E220BD0A2108A3EAF80EC1DA6CA034`): the l81v flow with l117v names, the seven pins above, the notices pin, the guide-with-notices step (CreateNew), the l117v-only allowance list (l81v withdrawn on publication), grant `unlocked,1080p,1440p` (no 2160p). Run: `C:\BDU\MCLA-Recomp\private\tools\pwsh\pwsh.exe -NoProfile -File C:\BDU\MCLA-Recomp\private\l117v-volunteer-prepare.ps1`.
- `private/l117v-CHECKLIST.txt` (ASCII CRLF, SHA-256 `B9A32F52E871D5A91B3FD83A5E889D17FA78964BF1226AB2E31F8B1A1F271328`).
- Entries / active-builds: produced by the prepare script at signing (drafted only as the dry run above, as for l81v).

## e. Documents and the SDK publication plan

- `docs/volunteer-changelog-l117v.txt` (UTF-8, CRLF, 7507 bytes, SHA-256 `04AD4B749A5841CBBF9F5F9CCA32FE30DA69E859409BB309B4C0318E2D3AE63B`): EN + UK, l81v style. Wording follows `docs/known-issues.md` rows: KI-054 "fixed for traffic jumps; camera filters not done" -> only the traffic part is claimed; KI-060 fixed, residual peak-voice dips -> "very rarely"; KI-074 fixed (night flicker); KI-083 fixed for the 1080p target; KI-075 "fix in l84 launcher, awaiting owner check" -> stated as new behaviour, to be confirmed in the pre-share check; KI-084 is not mentioned (it existed only in owner builds l111-l114); known: KI-081 (open, low priority), KI-082 (open, deferred, also at 30 fps), KI-086 (open, unconfirmed), KI-079 (open, low priority), KI-026 (open). Frame-rate claim: the measured RTX 5060 1080p Unlocked driving minutes 48 -> 66-69 fps (`M11-l110-pc-extent-20261118.md`, quoted in `src/mcla_m10_defaults.h` l111 block).
- `docs/volunteer-tester-guide-l117v-proposed.txt` (UTF-8, CRLF, SHA-256 `44D0F2B6347E2CC245906362D2A1FE6D8AC732D815D347DE7BC81748221D0F66`): the full guide with EN and UK edits, not applied in place: GPU choice on two-GPU laptops (the card chosen is the one Reflex / Anti-Lag 2 is enabled for), section 2 "better than l81v" and the low-latency note, the F10 limit (newest 50, about 768 MB), the Deck crackle wording, four new known items (KI-081, KI-082, KI-079, KI-086) and three "should be fixed" items (KI-074, KI-083, KI-054). The prepare script uses whatever `docs/volunteer-tester-guide.txt` holds when the owner signs, so the primary should apply the proposal (or not) before that.
- `private/l117v-CHECKLIST.txt`: sign, offline pre-share check (PC race with minimap and night free roam, garage, save, cut-scene; Deck loop by ear), log checks, then "publish l117v" / "publish SDK".

**SDK publication (only on the owner's "publish SDK"; commands not run).** Proposal **`v0.20.0.12` = `d889a16`** (the next free number: tags `v0.20.0.0`..`v0.20.0.11` exist in `private/l39-sdk-src`). Published: fork `2FED/rexglue-sdk` branch `mcla/mcla-r-hotfixes` at `2aff82e` = `v0.20.0.11` (`M11-l81-sdk-20261108.md`). `2aff82e` is an ancestor of `d889a16` (22 commits, 0 merges): a fast-forward. First-parent chain: `c40db4b` l82 -> `7cbfe3b` l88 -> `4ec7abd` l90 -> `ef72fd1` l91 -> `e6d6bd3` l92 -> `f9f5178` l95 -> `ccac775` l96 -> `6c5792d` l97 -> `d3d7b6c` l99 -> `1e81149` l100 -> `a49e511` l101 -> `13ef300` l105 -> `6a4b82e` l106 -> `87076c8` / `cb92be8` l107 -> `5e65c7a` l108 -> `4a29e93` l109 -> `c403f35` l110 -> `3173c2e` l112 -> `9595fc4` l113 -> `389d3bd` l114 -> `d889a16` l116 (plus their intermediate commits). `e6eeeb9` (l117) descends from `d889a16`, so a later publication of it stays a fast-forward. The FFX option is a build option, not source: source parity holds. Commands for the primary:

```
cd C:\BDU\MCLA-Recomp\private\l39-sdk-src
git fetch fork
git rev-parse fork/mcla/mcla-r-hotfixes            # expect 2aff82e8b91a8c5817d1d3b0007652c7ea2d46ea
git merge-base --is-ancestor fork/mcla/mcla-r-hotfixes d889a169703a1ce9e4701b7e856028cf04d57123
git tag -a v0.20.0.12 d889a169703a1ce9e4701b7e856028cf04d57123 -m "MCLA M11: l82-l116 (CP lever 1, register-write skip, texture memo v2, RT update memo, strict frame lead, Reflex / Anti-Lag 2 low-latency layer, forced HiDef switch, transfer elision and draw extent bound with resolve-only deferred claims, lead-2 guard, diagnostics; the levers default off in the SDK and the MCLA-R host writes its defaults); on top of v0.20.0.11. ctest 2398/2398 (4 skipped). Source of the l117v volunteer DLLs."
git push fork d889a169703a1ce9e4701b7e856028cf04d57123:refs/heads/mcla/mcla-r-hotfixes
git push fork refs/tags/v0.20.0.12
git ls-remote fork refs/heads/mcla/mcla-r-hotfixes refs/tags/v0.20.0.12
```

## f. What remains

- **Owner:** run `private/l117v-volunteer-prepare.ps1` (signs the V2 ticket and the l117v-only list, stages `private/l117v-volunteer`); the offline pre-share check of `private/l117v-CHECKLIST.txt`; say **"publish l117v"**; say **"publish SDK"** for v0.20.0.12.
- **Primary:** review; decide on the guide proposal and apply it to `docs/volunteer-tester-guide.txt` before signing; commit the tracked drafts (`docs/volunteer-changelog-l117v.txt`, `docs/volunteer-tester-guide-l117v-proposed.txt` or the applied guide, this file, `docs/evidence/M11-l117-deck-plan-20261120.md`); on the owner's words: publish the package and the signed `l117v-active-builds.txt` as `active-builds.txt` on `DebugBundle` of `2FED/RE-MCLA` (as l81v, commit and byte check), and the SDK commands above.

## Open risks

- Pursuits (police chases) have not been run on the l104-l116 defaults by the owner in a volunteer-like session.
- KI-085 (device hung) is unexplained; it happened only in a heavy GPU verify arm that volunteers cannot enable through the launcher.
- The Steam Deck has not run the l98-l116 defaults yet (`private/l117-DECK-CHECKLIST.txt`); l117v on the Deck is unverified until then. The l104 levers were verified on the PC only.
- AMD discrete (RX 6800 XT) has not run the current defaults; Anti-Lag 2 was seen on AMD only in owner arms; KI-081 seen on the integrated 860M.
- The three volunteer SDK DLLs are a new build configuration (FFX presenter shaders OFF): not byte-identical to any owner-run build. The only source difference is the `REX_HAS_FIDELITYFX_SDK` define (presenter layout and effect cases); OFF is the upstream default and the l81v state; SDK ctest passed.
- The unsupported direct-start path exposes every SDK cvar (section a).
- The SDK install registered `rexglue` in the CMake user package registry (as every earlier SDK install does); host builds pass `CMAKE_FIND_USE_PACKAGE_REGISTRY=OFF`.
