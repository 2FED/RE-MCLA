# M9 host hooks: M9-007 menu cap, KI-035 sampler, KI-026 state block (build evidence)

**Date:** 2026-10-29. **Status:** implemented host-only, unit tested and
compiled. Nothing was launched: no `mcla.exe`, launcher, game or helper run.
No physical PASS is claimed for any of the three pieces. The primary agent
has not reviewed or committed this yet.

Source proposals:
[M9-007 analysis](M9-007-menu-state-analysis-20261029.md) and
[diff](M9-007-menu-state-proposal-20261029.diff);
[KI-035 analysis](M9-006-ki-035-freeze-analysis-20261029.md) and
[diff](M9-006-ki-035-diagnostic-proposal-20261029.diff) (host part only);
[KI-026 analysis](M9-006-ki-026-camera-analysis-20261029.md) and
[diff](M9-006-ki-026-proposal-20261029.diff).

Worktree base: `92186e5` (branch `claude/admiring-jang-24d485`), working tree
changes uncommitted. SDK: the existing `private/l38-sdk-install`
(ReXGlue 0.20.0.2-dev.ga239b83). No SDK change was made; the SDK part of the
KI-035 proposal (`xboxkrnl_memory.cpp`) is left for the SDK branch.

## Shared integration

| File | Change |
| --- | --- |
| `CMakeLists.txt` | One delimited block appends the three new host TUs to `MCLA_SOURCES`; one `foreach` registers `mcla_menu_state_test` and `mcla_guest_sampler_test` under `MCLA_BUILD_DIAGNOSTIC_TESTS` (`EXCLUDE_FROM_ALL`, like `mcla_cursor_autohide_test`). |
| `src/mcla_app.cpp` | Three includes in a delimited block, one `Start` call in the accepted-F10 branch, one `Stop` in `OnWindowCloseRequested`, `Stop` + `ShutdownMenuStateCap` at the top of `OnShutdown`, one `guest_ki026_json` assignment in `CaptureDiagnosticState`. 16 added lines, none removed. |
| `src/mcla_diagnostics.h/.cpp` | `RuntimeState::guest_ki026_json`; `BuildStateJson` writes `"guest_ki026": {...}` before `native_threads` when it is not empty. |

New files use CRLF in the working tree like the rest of the checkout; all
changed files are ASCII.

## M9-007: menu-state pause hook

**Files.** `src/mcla_menu_state.h` (pure, header-only `Controller`),
`src/mcla_menu_state.cpp` (strong `REX_EXTERN(sub_822D29E8)` override and
the cvar access), `tests/mcla_menu_state_test.cpp`.

**Cvar.** `mcla_ui_cap_guest_pause` (bool, default **true**, `kInitOnly`,
category MCLA): kill switch. The hook writes the existing GPU cvar
`mcla_ui_cap_draws_low` (INT32, 0..100000, hot-reload) through
`rex::cvar::SetFlagByName`, and reads it and `mcla_guest_vblank_hz` with
`rex::cvar::Query<int32_t>`.

**Behaviour.** After the stock `__imp__sub_822D29E8`, when `r3` equals
`*(be32*)0x828CCEA0`, the hook reads `G+4` (simulation paused) and `G+6`
(paused locally). It acts only on edges of `paused = G+4 || G+6`; steady
frames take one uncontended mutex and compare one bool, and the cvars are
queried only on an edge.

- Pause edge, unlocked mode (`mcla_guest_vblank_hz > 0`) and UI cap enabled
  (`mcla_ui_cap_draws_low > 0`): save the current threshold and set it to
  100000, so the GPU plugin's existing cap turns on at the next frame. If the
  value already is 100000 it is not written.
- Resume edge: restore the saved value, but only if the cvar still holds
  100000 (a value the user changed while paused is kept). The GPU plugin's
  8-heavy-frame hysteresis then releases the cap.
- Host shutdown (`MclaApp::OnShutdown`, one call): the same restore, then the
  controller ignores all later edges.
- Stock mode or UI cap disabled: nothing is written.
- At most 200 transitions are logged: `MCLA menu state: game paused|running
  (sim N local N), UI cap forced|draw-count, mcla_ui_cap_draws_low V`.
  A failed write logs `MCLA menu state: could not set ...` (error).

**Tests.** `mcla_menu_state_test`: new, **447 checks passed**, exit 0
(steady state, pause/resume restore, stock mode, disabled cap, already-forced
value not written, user change kept, shutdown restore and stop, 200-line log
bound, repeated cycles).

**Owner must verify (D3D12, unlocked mode).** The analysis route: drive 10 s,
pause menu 10 s, map then Arcade menu 10 s, resume and the wager/race-start
screen at a meet spot, race start and intro cinematic, photo mode, a traffic
light. Accept if `MCLA menu state: game paused` and `MCLA UI cap: on` appear
within a frame of the pause/map/Arcade screens, `running` and `UI cap: off`
after resume, about 30 fps in those menus, 55-74 fps driving, and the
traffic-light segment stays uncapped. Also check that after a normal exit the
saved `mcla.toml` does not hold `mcla_ui_cap_draws_low = 100000`.

**Limits.**
- No runtime confirmation. The wager/meet-spot screen is not proven to pause
  the simulation; photo mode and cutscenes are unverified.
- Vulkan: the SDK Vulkan command processor does not call
  `MclaReportGuestFrameDraws` at this pin (analysis side observation), so the
  hook has no visible effect there.
- The hook mutates a user-visible hot-reload cvar. A settings save while a
  pause is forced (SDK settings overlay; disabled in managed builds) could
  persist 100000; the shutdown restore covers only saves made after
  `OnShutdown`. The cleaner SDK-atomic variant from the analysis needs an SDK
  pin.
- Online local pause (`G+6`) also caps, the same as stock 30.

## KI-035: F10 guest-thread sampler (host part)

**Files.** `src/mcla_guest_sampler.h` (pure `Histogram`, `FormatTopFunctions`,
`FormatImports`, `SanitizeName`, `FormatLine`, plus the `Start(KernelState*)`
/ `Stop()` API), `src/mcla_guest_sampler.cpp` (thread walk),
`tests/mcla_guest_sampler_test.cpp`.

**Cvars.** None. The sampler runs for each accepted F10 (the
`request.audit_attempted` branch, which is set only when the snapshot was
queued). A second F10 restarts the window; the partial window is logged first.

**Behaviour.** 10 s at a 2 ms interval. Per running guest thread it counts
`PPCContext::current_function` and `last_import` and logs at the end:

    MCLA_GUEST_SAMPLE v=1 start threads=N seconds=10 interval_ms=2
    MCLA_GUEST_SAMPLE v=1 thread='<name>' samples=N fn= XXXXXXXX:n ... last_import= name:n ...
    MCLA_GUEST_SAMPLE v=1 end stopped=0|1

`fn` holds the 12 most frequent functions (ties by address), `last_import`
every import name merged by text (`none` for no import yet), most frequent
first. Thread names are limited to 64 printable characters with `'` replaced.
`Stop()` is called from `OnWindowCloseRequested` and `OnShutdown`; it joins
the sampler.

**Thread-exit safety (verified against SDK a239b83 source,
`src/system/xthread.cpp`).**
- `GetObjectsByType<XThread>` returns retained `object_ref`s; the sampler
  holds them for the whole window, so no `XThread` is destroyed while it is
  sampled. The refs are released on the sampler thread when the window ends.
- `XThread::Exit` and `Terminate` free the guest stack and call
  `ReleaseHandle`, but never reset `thread_state_`. It is destroyed only with
  the `XThread` object, so the `PPCContext` storage stays valid while the ref
  is held. Reading it after the thread exited is safe.
- Even so, the context pointer is snapshotted only for threads that are
  running at the start. `running_` (atomic) is set after `thread_state_` is
  assigned in `Create()`, so a thread still being created is never read. A
  thread that exits during the window is skipped from then on
  (`is_running()` false) instead of repeating its last breadcrumbs.

**Tests.** `mcla_guest_sampler_test`: new, **9 checks passed**, exit 0 (empty
histogram, top-12 order and limit, deterministic ties, import merge and order,
exact v=1 line, name sanitising).

**Owner must verify.** One F10 in normal driving near a garage (control) and
one during a KI-035 freeze; wait at least 15 s after each F10 before closing
the window. Then read the full `runtime-*.log` (not only `log-tail.txt`) for
the `MCLA_GUEST_SAMPLE` lines and the `REX_PERF_AUDIT_*` output, and compare
the Main XThread functions and repeated imports between the two.

**Limits.**
- The reads are racy (aligned 32-bit and pointer scalars written by the guest
  thread); diagnostic only.
- Threads created during the window are not sampled.
- Function-level resolution only (innermost guest function); a polling state
  machine with little work per frame may be under-sampled.
- One thread is created per F10 and joined by the next F10 or at shutdown.
- `Stop()` joins on the UI thread; worst case about one 2 ms interval plus
  formatting.
- Without the SDK part (allocation-failure logging), hypothesis 2a stays
  untestable.

## KI-026: F10 state block, probe and watchdog

**Files.** `src/mcla_ki026_probe.h/.cpp`; `src/mcla_diagnostics.h/.cpp` and
`src/mcla_app.cpp` as above. Applied by hand: the proposal diff contains a
broken string literal in the `mcla_diagnostics.cpp` hunk (a literal newline
inside `"},\n"`, from stripping CR), so it could not be applied as is.

**Cvars.**
- `mcla_ki026_probe` (bool, default **false**, `kInitOnly`): logging only.
- `mcla_ki026_watchdog` (bool, default **false**, `kInitOnly`): implemented,
  because the diff's watchdog was complete; it stays default OFF.

**F10 block (always on, read-only).** `state.json` gains the top-level object
`guest_ki026` with UILogic and `*(UIManager)+48`, `gps_jumping`,
`camera_paused`, `camera_pause_requested`, `camera_pause_id`, the mcWarper
state/queue/request/type/flags/prestream/camera, the pause manager
(`+4/+6/+8/+10`, ring count and entries), timer scale/step and game time
scale, the cutscene player, the local racer and the on-screen player camera
slot, and `"complete": true|false` (false means at least one read was
refused). The block is omitted when the runtime has no memory.

**Guest-read checks (verified in the code).** The diff's claim held only for
the F10 block; its probe and watchdog paths dereferenced guest pointers
(cutscene, racer, request, UI manager) without checks. Every guest read in
`mcla_ki026_probe.cpp` now goes through one `GuestReader`:
- the address must lie inside the heap that `Memory::LookupHeap` returns, by
  that heap's own `heap_base`/`heap_size`, checked before `QueryProtect`,
  because the SDK `BaseHeap::QueryProtect` bounds test is `>` (it accepts an
  index equal to the page-table size);
- the page protection must include read access;
- both ends of each span are checked (spans are at most one page);
- debug-printf strings are checked page by page and limited to 96
  characters, with `"` and `\` replaced;
- hooks obtain the `Memory` through `rex::system::kernel_state()`.
The edge hooks log register values only. Non-finite guest floats are written
as JSON `null`, so the block is always valid JSON.

**Probe hooks (with the probe off, each costs one cached bool test).** Strong
overrides that always call the stock `__imp__` function: `sub_82130000`
(stripped debug printf; logs only the mcWarper texts `0x8201F980..0x8201FCC4`,
on change and then every 600th repeat, "Processing Request" and "Game Camera"),
the edge functions `82204480/82204558/82204468`, `822079C8/822079E8/82207A48`,
`8221A440/8221B790/8221BB90/8221B8C8`, `822B0F10/822B34B0/822B33F0`, and the
warper update `sub_8221BE98` (state-change log). Log tags:
`MCLA_KI026_WARPER_TEXT`, `_PAUSE_REQUEST`, `_GAME_CAMERA`, `_EDGE`,
`_WARPER_STATE`, `_WATCHDOG`, all `v=1`. No other source or
`config/mcla_functions.toml` entry overrides these symbols; the link has one
strong definition each (checked with `llvm-nm`).

**Watchdog (default off).** Evaluated after each stock warper update. It
fires once per episode, and only when the camera is paused, the simulation is
not paused, the override byte is clear, no cutscene is playing, the local
racer is active and every read succeeded, and either signature 1 (warper idle
29, empty queue, no request) holds for 20 s, or signature 2 (warper state
12/17, `+274` set, cutscene clock < 0) holds for 15 s. Signature 2 clears
`+274` and calls Finish (`sub_8221B790`, `r3` = warper), checked against the
stock branch at `0x8221C8C8`. Both signatures then call `sub_82204468` with
`r3 = *(*(0x8286D804)+48)`, as at `0x8221C940`. Deviation: if that owner
pointer cannot be read, the watchdog does nothing; the diff fell back to
UILogic.

**Consumers of `state.json`.** The export policy reads only `schema` and
`reason` from the state and builds its report from typed fields; unknown
keys have no authority. `mcla_diagnostic_export_win.cpp` reads at most
256 KB of `state.json`; the block is about 1-2 KB. `verify-diagnostics-smoke.ps1`
checks only that the required keys are present. A new case in
`mcla_diagnostic_export_policy_test` confirms that a state with an extra
`guest_ki026` block (including `null` and a private sentinel) is accepted and
projects exactly the same report as the baseline.

**Owner must verify.** Normal play with the default build: one F10 anywhere;
`state.json` must contain `guest_ki026` with `"complete": true` in gameplay,
and nothing else must change. For KI-026 runs: `--mcla_ki026_probe=true` for
normal play; on the next softlock take an F10 and keep the full runtime log.
Read the result with the table in the analysis (section 3). Enable
`--mcla_ki026_watchdog=true` only after a recorded occurrence shows a
signature, and check photo mode once with the probe log first.

**Limits.** Addresses and offsets come from static analysis and are
unverified at runtime. The check and the read are not atomic (a page
decommitted in between is a residual, diagnostic-only risk). The watchdog
risks from the analysis (legitimate long scripted camera holds, the skipped
Career_Loading movie calls, photo mode) still apply. KI-026 stays open.

## Build and gates

Build directory `private/l39-hooks-build` (new), configured from this
worktree with the same cache options as `private/l39-cursor-build`: Ninja,
Release, clang++ 20 (`C:/Program Files/LLVM`), `CMAKE_PREFIX_PATH` =
`private/l38-sdk-install`, `MCLA_TEST_BUILD_EXPIRY=ON`,
`MCLA_RAW_TIMER_DEFAULT_ON=ON`, `MCLA_BUILD_IMAGE_CAPTURE_TESTS=ON`,
`MCLA_BUILD_DIAGNOSTIC_TESTS=ON`, the same `-ffile-prefix-map` flags.

| Step | Result |
| --- | --- |
| Baseline `mcla` (before the changes) | 86/86 actions, 0 warnings |
| `mcla` + the two new tests | 10/10 actions, 0 warnings |
| 11 existing test targets (below) | 13/13 actions, 0 warnings |
| 4 nlohmann-dependent test targets in `json-tests` (see notes) | 27/27 actions, 0 warnings |
| `-fsyntax-only -Wall -Wextra -Wshadow` on the three new host TUs (same flags as the build) | no diagnostics in the new files |
| `-Wall -Wextra -Wshadow -Wconversion -Wsign-conversion -Wpedantic` on both new tests and the pure headers | no diagnostics |
| `ast-grep scan` (0.50.0, whole worktree) | no findings, exit 0 |
| `ast-grep test` | 3 passed, 0 failed |

**`mcla.exe`:** 55,230,464 bytes, SHA-256
`090E32AD8DBDC6470C0B1D3C266109BA0EBFD978102102866B592461BCF9FD36`
(`private/l39-hooks-build/mcla.exe`, linked 2026-10-29 14:53 local). Not
launched.

Test results (all exit 0). "Before" is the count of the same suite at this
base without the changes.

| Suite | Before | After |
| --- | ---: | ---: |
| `mcla_menu_state_test` (new) | - | 447 checks |
| `mcla_guest_sampler_test` (new) | - | 9 checks |
| `mcla_diagnostic_export_policy_test` | 538 | 549 (+1 case, 11 checks) |
| `mcla_diagnostic_export_win_test` | 105, 1 skip | 105, 1 skip (symlink fixture, Windows 1314) |
| `mcla_diagnostic_export_ui_win_test` | 249 | 249 |
| `mcla_diagnostic_hotkey_test` | 8 | 8 |
| `mcla_session_report_test` | 17 | 17 |
| `mcla_launcher_ui_win_test` | 971 | 971 |
| `mcla_launcher_capabilities_test` | 200 | 200 |
| `mcla_launcher_import_policy_test` | 72 | 72 |
| `mcla_launcher_preferences_test` | 2088 | 2088 |
| `mcla_launcher_profile_policy_test` | 118 | 118 |
| `mcla_managed_config_policy_test` | 50 | 50 |
| `mcla_cursor_autohide_test` | 266 | 266 |
| `mcla_diagnostic_image_protocol_test` | 119 | 119 |
| `mcla_frame_signal_test` | 24 | 24 |

The "before" counts of unchanged suites are inferred: those tests do not
compile any changed file, so their counts cannot differ. Only the export
policy test changed (one new case).

Build notes:
- The worktree has no `third_party/rexglue-sdk` submodule checkout, so the
  four targets that include `nlohmann/json.hpp` do not compile in
  `private/l39-hooks-build`. They were built in the sub-directory
  `private/l39-hooks-build/json-tests` with the same options plus
  `-isystem` pointing at the main checkout's
  `third_party/rexglue-sdk/thirdparty/inja/third_party/include` (read-only).
- `mcla_launcher_controller_win_test` was built there but not run: it copies
  itself as a synthetic `mcla.exe` into a preserved temp fixture, and it does
  not compile any changed file.
- `scripts/test-diagnostics-smoke.ps1` was not run (smoke script, outside
  this host-only scope).

Spark: no job was sent. The work was implementation against recorded
proposals plus deterministic compiler and unit-test gates; there was no
bounded extraction or log triage to offload.

## Deviations from the proposals

- M9-007: the logic moved into a pure header with a unit test; acts only on
  pause edges (the proposal re-queried the cvars every paused frame in stock
  mode); never writes 100000 when it is already set; does not restore over a
  user change; restores on host shutdown; mutex for the shutdown path.
- KI-035: own files instead of a `mcla_app.cpp` block; snapshot of running
  threads only and per-sample `is_running()` check; deterministic top-N and
  import order, imports merged by text; thread names sanitised; start/end
  lines added.
- KI-026: every guest read checked (probe and watchdog included), heap bounds
  checked before `QueryProtect`, JSON `null` for non-finite floats, empty
  block without memory, debug-printf override still calls the stock nullsub,
  watchdog acts only on fully read state and without the UILogic fallback,
  `mcla_diagnostics.cpp` hunk applied by hand.

## Unresolved risks

- None of the three pieces has run. All addresses and offsets are from static
  analysis.
- M9-007 writes a user-visible cvar; see its limits.
- KI-035 and KI-026 reads race with the guest threads (diagnostic only).
- Another branch will touch `src/mcla_app.cpp` and `CMakeLists.txt`; the
  edits here are delimited blocks and single lines to keep that merge small.
