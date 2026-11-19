# l116 SDK: the lead-2 guard skips frames that say nothing about play and cools down instead of latching; thin-frame, repeat-present and black-frame probes (2026-11-19)

Implementation by an Opus subagent for the primary agent, from
`docs/evidence/M11-l115-main-session-20261119.md` (KI-087: in session 29865940 the game was paused
with its window unfocused while another application squeezed video memory; GPU busy rose to 41 ms
per frame, 74 lead-2 frames tripped the guard at 15:39:41.6 and guard action 0 latched strict for
the rest of the session, about -11 % fps in the last race; KI-086: no log candidate for the black
frame). SDK commit `d889a16` (`d889a169703a1ce9e4701b7e856028cf04d57123`) on the local branch
`mcla/m11-l116`, parent `389d3bd` (l114); worktree `private/l116-sdk-wt`. Not pushed, no tag.
Built, ctest-ed and installed to `private/l116-sdk-install` (CMake version
`0.20.0.22-dev.gd889a16`). **Unit-tested only: no game run, no physical PASS.** Spark not used:
source work checked by deterministic unit tests.

## 1. The guard rule (G1)

1. Every frame end gets a play state from two host signals: the window focus and the game's own
   pause. Unfocused wins over paused.
2. A frame is **skipped** while unfocused or paused, for 120 frames after either ends (the settle
   span keeps the skip reason), and when its interval or its GPU time is above 3x the running
   median of play frames (an outlier).
3. A skipped frame never enters the 2700-frame window. The window is **frozen**, not cleared, so it
   still holds the last 2700 counted frames. A skipped frame is not a clean frame of action 2 and
   does not advance a cool-down or the re-trip interval.
4. The trip rule is unchanged: more than 80 lead-2 frames among the last 2700 counted relaxed frames.
5. **Guard action 4 (new SDK default):** after a trip the configured strict mode applies for 60 s of
   counted play. Then the window is emptied and Reflex replaces strict again (`kind=guard_rearm`).
6. A trip within 300 s of counted play after a re-arm doubles the cool-down (60, 120, 240, 480 s).
   A quick re-trip above `backoff_max` (3) latches strict for the session. A later trip starts at
   60 s again. Action 0 still latches for the session (host key `lead_guard_latch_session`).

Why freeze instead of clear: clearing on every alt-tab or pause would let a burst of real lead-2
frames that started before the pause escape the guard. Freezing keeps the counted evidence and drops
only the frames that say nothing about play.

Why an outlier rule as well: a video-memory squeeze can also happen while the game is focused and
running (another application in the background). The median is taken over play frames only,
including outliers and settle frames, so a lasting new level becomes the baseline within about 150
frames. After that the frames count again, and the guard protects real play under that load.

Why a new action number instead of changing action 0: action 0 keeps its l97 meaning for arms and
A/B runs. Only the default (the cvar value) changes from 0 to 4.

## 2. Signals and where they come from

| signal | source | SDK side |
|---|---|---|
| window focus | host `MclaApp::OnWindowFocusChanged` (`src/mcla_app.cpp:1429`), called from the SDK `ReXApp::OnGotFocus` / `OnLostFocus` events, the same events that write `MCLA_WINDOW_EVENT` | `rex_play_window_focus` (`src/core/perf/perf_split.cpp:357`) -> `SetHostWindowFocused` (`:265`) |
| game paused | host menu-state hook `sub_822D29E8` (`src/mcla_menu_state.cpp:195`, `CapPaused(simulation, local, racer_off)`: the same flag that forces the 60 Hz UI cap and writes `MCLA menu state: game paused`), published on every change (`:201`) | `rex_play_game_paused` (`perf_split.cpp:361`) -> `SetHostGamePaused` (`:270`) |
| host resolve | `src/mcla_low_latency_win.cpp:130-132` (GetProcAddress on rexruntime.dll, separate from the l99 hitch entries, one `MCLA_PLAY_HOST v=1 kind=entry sdk_entry=found\|absent` line) | `GetHostPlayState` (`perf_split.cpp:275`), lock-free relaxed atomics; focused until the host says otherwise |
| frame interval, GPU time | the CP itself: steady clock between closing frames, the resolved GPU timestamp total (`perf_gpu_busy_ticks_total_`; GPU time lags up to 2 frames) | `D3D12CommandProcessor::PlayFrameEnd` (`src/graphics/d3d12/command_processor.cpp:5063`), called once per closing frame before the guard (`:8019`) |

The SDK's UI cap state (`MclaUiCapActive`, `src/graphics/graphics_system.cpp:408`) is not a skip
signal. It is only logged on MCLA_THIN lines (`ui_cap=`). The host pause signal is used instead
because it is exact in both the Locked and the Unlocked mode.

## 3. State machine and code

- Pure: `include/rex/perf/play_state.h` (new). `PlayFrameFilter` (`:162`): `kPlaySettleFrames = 120`
  (`:99`) and `kPlayOutlierFactor = 3` (`:102`). The baselines are 255-frame medians, refreshed
  every 32 pushes and used from 64 samples on.
- `include/rex/graphics/util/frame_lead_limit.h`:
  - `FrameLeadGuardAction::kCooldown = 4` (`:401`);
  - `FrameLeadGuardCooldown` and `FrameLeadGuardCooldownUs`: the shift is capped at one day;
  - `FrameLeadGuardFrame` (the skip reason and the interval);
  - `OnFrameEndEvent` gains two defaulted arguments (`:631`). The skip branch is at `:639`, the
    cool-down branch at `:653` and the backoff decision at `:722`. The old callers and the old tests
    are unchanged.
- States (`FrameLeadGuardStateKind`):
  - `relaxed`;
  - `cooldown` (action 4, latched until the cool-down ends);
  - `latched` (actions 0-2);
  - `session` (action 4 after the backoff cap).
- CP (`command_processor.cpp:8051`): the skip reason (when `_guard_skip` is true) and the interval
  go into `OnFrameEndEvent`. Action 4 has its own trip and re-arm lines. Skip spans are logged by
  `PlayGuardSkipSpan` (`:5081`).

### Cvars (GPU/D3D12, hot reload)

| cvar | default | meaning |
|---|---|---|
| `d3d12_frame_lead_strict_low_latency_guard_action` | **4** (was 0), range 0..4 | 4: cool-down with back-off; 0: the l97 session latch; 1-3 as l99 |
| `d3d12_frame_lead_strict_low_latency_guard_cooldown_s` | 60 (1..3600) | base cool-down, seconds of counted play |
| `d3d12_frame_lead_strict_low_latency_guard_backoff_max` | 3 (0..16) | doublings before the session latch |
| `d3d12_frame_lead_strict_low_latency_guard_retrip_s` | 300 (0..86400) | a trip within this much counted play after a re-arm is quick (0: never) |
| `d3d12_frame_lead_strict_low_latency_guard_skip` | true | false: every frame counts (l115) |
| `mcla_black_frame_probe` | true | the MCLA_BLACK probe (G2c) |

### Lines

- `MCLA_LEAD_LL v=1 kind=guard_trip frame= lead2_in_window= window_filled= guard_frames=
  guard_lead2= strict_now= guard_action=4 trips= quick= backoff= backoff_max= cooldown_s= retrip_s=
  skipped=unfocused:a,paused:b,outlier:c skipped_lead2= (...)`. The text in brackets is either "the
  configured mode for N s of counted play, then relaxed again" or "latched for the session: quick
  re-trip above the backoff cap". Actions 0-3 keep their l97/l99 text.
- `MCLA_LEAD_LL v=1 kind=guard_rearm frame= guard_action=4 cooldown_s= trips= rearms= backoff=
  skipped= (relaxed again)`.
- `MCLA_LEAD_LL v=1 kind=guard_skip frame= from=<none|unfocused|paused> to=<...> span_frames=
  span_lead2= guard_state= cooldown_left_s= skipped= skipped_lead2= (...)`:
  - one line per change of the skip span, only while `d3d12_frame_lead_strict_low_latency` is on;
  - outlier frames are counted but are not spans;
  - at most 64 lines a minute, the rest counted.
- Appended to the MCLA_READBACK v=1 minute and mark lines (the line that carries `lead2_frames`),
  after the l99 fields: ` lead_ll_skipped=unfocused:a,paused:b,outlier:c lead_ll_skipped_lead2=
  lead_ll_guard_skip= lead_ll_guard_state= lead_ll_guard_backoff= lead_ll_guard_cooldown_s=
  lead_ll_guard_cooldown_left_s= lead_ll_guard_session_latches= lead_ll_skip_lines_suppressed=`.

## 4. G2: KI-086 probes (always on)

- **(a) MCLA_THIN v=1 kind=frame** (`ThinFrameDetector`, `play_state.h:250`; CP
  `HitchFrameEnd`, `command_processor.cpp:5050`). A frame is thin when it ends in the play state
  and either its GPU busy is under 1 ms, or its draws are under 25 % of the running median draws of
  play frames. At most 20 lines a minute. Fields: `frame= by=gpu|draws|both draws= base_draws=
  gpu_ms= frame_ms= since_play_frames= settling= ui_cap= fade=na seq=`.
  - A fade is not detectable in the SDK, so `fade=na`.
  - `since_play_frames` and `settling` separate the race-start / resume frames from anything else.
- **(b) Repeat presents** (`SwapWriteTracker`, `play_state.h:320`):
  - the resolve destinations since the previous swap are recorded at both `IssueCopy` paths
    (`command_processor.cpp:6700`, `:6719`);
  - at the swap (`:2999`) the source, texture fetch constant 0 (what `RequestSwapTexture` uses), is
    `written`, `repeat` or `unknown` (more than 64 resolves in the frame);
  - the first 8 repeats of the session get a `MCLA_THIN v=1 kind=repeat_present` line;
  - counts go on the new per-minute line `MCLA_THIN v=1 kind=minute thin= logged= suppressed=
    by_gpu= by_draws= settling= ui_cap= swaps= swap_written= repeat_presents= swap_unknown=
    black_probe= black_checked= black_unchecked= black_samples= black= ...`.
- **(c) MCLA_BLACK v=1: possible on an existing path.**
  - The read-back resolve path (on in the MCLA defaults) already copies resolve results into guest
    memory. When the swap source range was delivered that way by the CP since the previous swap
    (`OnDeliver`, `:7361`), the probe samples 144 texels of those guest-memory bytes. No new GPU
    read-back and no wait are added. The probe is skipped while the read watch is on.
  - It runs only for 8:8:8:8 sources.
  - The per-lane means are sorted and the three smallest are taken as the colour lanes. The probe
    does not know which byte is alpha; dropping the largest lane is conservative.
  - A frame is black when its level is below 3/255 and fewer than 2 % of its texels are bright. Its
    neighbours are lit when their level is above 20/255 or more than 10 % of their texels are
    bright.
  - A line is written when checked swap N is black and the previous and next checked swaps are lit,
    each at most 4 swaps away (at most 10 lines a minute).
  - Limits:
    - the delivered copy can be one frame older than the presented one (the read-back
      previous-slot fast path);
    - cadence or fenced deliveries made elsewhere (`DeliverCompleted`) are not seen;
    - whether the front buffer is on the CP delivery path at all in normal play is unknown until a
      run: `black_checked=` against `black_unchecked=` on the minute line answers it. If it stays 0,
      the probe has no data and only a new read-back could add it, which was not done.

## 5. G3: paused and unfocused labels

- `MCLA_HITCH`: a frame that ended while paused or unfocused gets `cause_hint=paused|unfocused`
  (`HitchLineCauseName`, `include/rex/perf/frame_hitch.h:297`). ` state= raw_cause=` is appended at
  the end, so the computed cause is kept.
- `MCLA_DIP`: a second in which at least half of the frames ended paused or unfocused gets
  `DipCause::kPaused` / `kUnfocused` (`frame_dip.h:184`; unfocused wins a tie). Second lines append
  ` paused_frames= unfocused_frames=`, and the summary takes the most frequent cause as before.
- The dip baseline rule is unchanged: paused seconds still enter it after 60 s, as before.

## 6. Tests, build, install

- New `tests/unit/core/play_state_test.cpp` with 7 cases `[l116]`:
  - the host signals and names;
  - the baseline median;
  - the filter table: pause, unfocus, settle, outliers by interval and GPU, a learnt new level, no
    inputs;
  - thin frames: GPU, draws, both, not in pause, the line budget, the minute counters and the line
    text;
  - repeat presents: masking, coverage from below, the end of a range, zero lengths, overflow and
    delivery;
  - the luminance sample and isolated black frames: both alpha lane orders, a lit frame, a dark red
    frame, a lit strip, a fade of two black frames, the gap limit and the line budget;
  - the C entry points.
- `frame_lead_limit_test.cpp`: 9 cases `[l116]`, table-driven:
  - the cool-down length and the cap;
  - trip, cool-down, re-arm;
  - lead-2 frames during the cool-down;
  - a pause, an unfocus and outliers during the cool-down, which do not advance it;
  - quick re-trips back off 1 -> 2 s, then 4 s, then the session latch at the cap, and a pause after
    a re-arm does not count toward the re-trip interval;
  - a slow re-trip resets the backoff, `retrip_s` 0 and `backoff_max` 0;
  - skipped frames freeze the window, for each of the three reasons and with the sleep inactive;
  - the l115 replay: 2600 play frames, then 2700 unfocused frames with 74 lead-2 frames, then 20000
    play frames. With the skip: no trip. Without it: action 0 latches and action 4 trips once and
    re-arms;
  - action 0 keeps the session latch, and switching to action 4 while latched cools down;
  - the line and field texts.
- The l99 mapping test is updated: `FromCvar(4)` is now `kCooldown`, 5 is the latch, and the
  effective-mode sweep includes action 4.
- `frame_hitch_test.cpp` and `frame_dip_test.cpp` gain 1 case `[l116]` each.
- ctest: **2398 tests, 100 % passed, 0 failed** (2394 passed + 4 skipped BitStream; l114: 2376 + 4;
  +18 new), **three runs in a row** after the install.
- Build:
  - fresh `private/l116-sdk-b` (Ninja Multi-Config, clang 20.1.8, the l114 options), output
    `private/l116-sdk-o`;
  - normalized warning set of the baseline build: 211, identical to l114;
  - the incremental, test and install builds: **0 new warnings**, no warning in a changed file. The
    l114 `unused parameter 'frontbuffer_ptr'` in `IssueSwap` is gone, because the parameter is now
    used.
- `git-clang-format` against `389d3bd`: applied (wraps in four files), then clean.
  `git diff --check`: clean.
- 14 files, 1905 insertions(+), 26 deletions(-) (one new header and one new test file).

| bin file | bytes | SHA-256 |
|---|---|---|
| rexruntime.dll | 11776000 | 8A9F2F9A3E3A1AEFE49BCD13ED50DDAB64C5C29B44AAC1D41A1496FBBEAF886C |
| rexgpu-xenos.dll | 3898880 | 6399B7B20D572B83EC315F039254B7BACDC29CE787F57FA60938EC5F103EA68D |
| TracyClient.dll | 246784 | 1851F2978A244BA9A8FE8A5496992A3C573C3615E3F7EFEC76E358545D1299C0 |

**ABI:**
- New exports: `rex_play_abi_version`, `rex_play_window_focus`, `rex_play_game_paused`
  (rexruntime).
- `OnFrameEndEvent` and `DipAccumulator::AddFrame` gain defaulted arguments.
- `HitchFrame` and `DipSecondStats` gain fields.
- A host built against an older SDK keeps working. It just publishes nothing (`sdk_entry=absent`),
  and the guard then sees only the outlier rule.

## 7. Not done, limits, risks

- Nothing has run in the game.
- Unverified in a run:
  - whether the focus events arrive in exclusive fullscreen exactly as the l115 `MCLA_WINDOW_EVENT`
    lines suggest;
  - how many play frames the outlier rule skips in normal races. The `outlier:` counter shows it;
    with 3x the median it should be the hitch frames only.
- The settle span (120 frames) follows every racer-off span too (race starts, hangouts), so lead-2
  frames there do not count. This is intended: those frames are not driving.
- The guard can still trip in normal play, as in l100. The cost is now 60 s of strict instead of
  the session, at most 480 s per quick re-trip, then the session.
- G2a reads the GPU time with up to two frames of lag, so a GPU-thin frame can be attributed one or
  two frames late; `by=draws` has no lag.
- Scratch folder `%LocalAppData%\Temp\claude\C--BDU-MCLA-Recomp\l116-work`:
  - scripts: `copy_tp.ps1`, `sdk-build.ps1`, `sdk-install.ps1`, `sdk-test.ps1`, `ctest3.ps1`,
    `normwarn.py`, `fl_test_append.cpp`, `sdk-commit-msg.txt`;
  - logs: `sdk-configure.log`, `sdk-build0.log` to `sdk-build2.log`, `sdk-install.log`,
    `ctest-a1.log` to `ctest-a3.log`, `w116all.txt`.
