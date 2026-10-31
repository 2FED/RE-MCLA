# M9-006 KI-047: always-on bounded pipeline wait, owner candidate l51 (2026-10-31)

Status: built and unit-tested. **Not run.** Nothing in this document is a
physical result. `private/l51-internal-prepare.ps1` has not been run, so
`private/l51-internal` and the l51 build fingerprint do not exist yet.

## 1. Why

The l50 diagnostic runs (`M9-006-ki-047-diag-runs-20261031.md`) showed that on a
cold per-build store the world-load frame skipped 1,705 draws whose pipelines
were still being created, into one-off read-back render targets, and Photo Mode
entry skipped 515. Whatever those passes render is baked into textures such as
car liveries, so a skip there persists for the session. The KI-037 fix
(`M9-006-ki-037-fix-build-20261029.md`) waits only inside the Photo Mode capture
window. l51 makes the bounded wait always on.

## 2. Design and bounds

SDK (ReXGlue, local branch `mcla/ki-047-diag`, worktree `private/l50-sdk-wt`):
commit **851f03bd23797e72da4091c3fe7fec7566c0d1c9** ("gpu(d3d12): optional
always-on pending-pipeline wait with a per-minute budget (KI-047)") on top of
the l50 diagnostics commit 928f6d3 (v0.20.0.4 base daa1ae5). Local only, not
pushed.

- New hot-reload cvar `d3d12_async_pipeline_wait_always` (GPU/D3D12, SDK
  default **false**). With `async_shader_compilation` on, every draw whose
  pipeline is pending waits for it (same `PipelineCache::AwaitPipeline` as the
  photo-window mode: the command processor thread helps by creating queued
  pipelines, otherwise yields).
- Bounds (`PendingWaitMinuteBudget` in
  `include/rex/graphics/util/pending_pipeline_wait.h`): at most **2,000 ms per
  wait** and **15,000 ms of total waiting per one-minute window**. A window
  starts when the mode is first seen on and rolls over at the first frame end
  60 s or more later (so a single very long frame is not split). Once the
  window's budget is spent, draws with pending pipelines are skipped as in the
  default asynchronous mode until the next window.
- Logging: one `D3D12 async pipeline wait (always): pipeline wait budget
  exhausted (N ms this minute), skipping draws ...` warning per window; one
  warning for the first slow (>= 500 ms) or timed-out wait per window; and a
  summary per window with activity: `D3D12 async pipeline wait (always): minute
  summary over N ms, W waits (T ms total, M ms longest), X timed out, F failed,
  S skipped after budget exhausted`. A window is also summarised when the mode
  is turned off. Idle windows log nothing.
- Composition with the photo window (`d3d12_async_pipeline_wait`, unchanged:
  2 s per wait, 8 s per on-period): `SelectPendingWaitMode(window_on,
  always_on)` picks the window budget whenever the window is on, else the
  minute budget when always-on is on, else no wait. The two budgets are
  tracked separately: waits in the window do not consume the minute budget,
  and an exhausted minute budget does not shorten a capture window.
- Cost when off: one cvar read and one bool compare per draw. When on, mode
  changes are detected per draw; the clock is read only on a mode change and
  once per frame end.
- The `MCLA_LIVERY_DIAG kind=start` line now also carries
  `async_pipeline_wait_always=0|1`. All other l50 diagnostic lines are
  unchanged; `pso_pending_skip_per_minute` still counts only real skips
  (a draw that waited successfully is not a skip).

Host (worktree `claude/admiring-jang-24d485`, uncommitted; HEAD db9440a when
built, which differs from f16d5ff only in `docs/M9-priorities.md`):

- `src/mcla_photo_capture.cpp`: new init-only cvar `mcla_pipeline_wait_always`
  (MCLA, default **true**). `StartAlwaysPipelineWait()` runs from
  `MclaApp::OnPostLoadGraphicsPlugin` (`src/mcla_app.cpp`, after the GPU plugin
  cvars exist) and writes `d3d12_async_pipeline_wait_always=true` through
  `rex::cvar::SetFlagByName`. Log lines (category GPU): `MCLA pipeline wait:
  always on (2 s per wait, 15 s per minute)`, `... always-on wait disabled by
  option`, `... always-on wait unavailable in this SDK`, or `... set by the
  user (...), left alone`. `ShutdownPhotoCapture()` turns an owned value back
  off so a settings save cannot persist it. The photo-window logic is
  unchanged.
- Pure decision helpers in `src/mcla_photo_capture.h`:
  `DecideAlwaysWaitStart(option_on, sdk_has_cvar, user_set)` and
  `AlwaysWaitShutdownWrite(owned)`.
- "User-set" uses `rex::cvar::HasNonDefaultValue`, which only sees a value that
  differs from the SDK default, i.e. `true`. A user-supplied
  `--d3d12_async_pipeline_wait_always=false` is indistinguishable from the
  default and would be overridden; the supported off switch is
  `--mcla_pipeline_wait_always=false`.

## 3. Tests and build

SDK (`private/l51-work/l51_sdk_build.ps1`, the l50 configuration; build
`private/l50-sdk-b`, install `private/l50-sdk-install`, overwriting the l50
dev install):

- New unit cases (fake clock), `unit_tests [pipeline-wait]`: **108 assertions
  in 9 test cases passed** (l50: 42 in 5). New cases: window start/rollover
  and report only with activity; exhaustion, single warning per window and
  reset in the next minute; partial window reported on turn-off and fresh
  budget on re-enable; composition with the window mode and separate budgets.
- ctest `-C Release -j 8`: **1912/1912 passed** (baseline 1908 + 4), run twice
  (before and after the final amend). `[livery-diag]` 50 assertions / 8 cases
  passed. Logs `private/l51-sdk-test.log`, `private/l51-sdk-test2.log`.
- Warnings: incremental build, 99 warning lines, all from pre-existing
  sources; the warning set of `src/graphics/d3d12/command_processor.cpp`
  (message multiset, line numbers ignored) is identical to the l50 build, and
  `pending_pipeline_wait.h` and its test have none
  (`private/l51-sdk-build.log`, `l51-sdk-build2.log`).
- The CMake package still reports `ReXGlue SDK 0.20.0.1-dev.g928f6d3`: the
  version string is computed at SDK configure time and the build tree was not
  reconfigured. The DLL hashes below identify the build.

Host (`private/l51-work/l51_host_build.ps1`, l50 options, `REXSDK_VERSION`
empty, build `private/l51-build` against `private/l50-sdk-install`):

- 221/221 steps, **0 warnings** (`private/l51-build.log`, rebuilds
  `l51-build2.log`, `l51-build3.log`).
- 36 test executables (`l51_host_tests.ps1`, `private/l51-tests.log`):
  **36/36**, 0 failed executables; `mcla_photo_capture_test`: 214 checks (two
  new test functions).
- ast-grep 0.50.0: `scan` rc=0, `test` 3/3 (`private/l51-astgrep.log`).
- Path-string scan (`private/l51-work/l51_pathscan.py`, l50 needles plus
  `l51`): **0 hits in all seven files**; admission marker once in `mcla.exe`
  (`private/l51-pathscan.log`).
- `private/l51-build/rexgpu-xenos.dll` is a stale copy from the first SDK
  build (791BA967...); the candidate takes its DLLs from the SDK install.

## 4. Owner candidate l51 (INTERNAL, V1)

`private/l51-int-input` (seven files):

| File | Size | SHA-256 |
|---|---:|---|
| mcla.exe | 53093376 | 728CE2C62668419BDB44F3C9D22F3EFEC0C2DFC5C6E0D0A77CD080F7594CDAE1 |
| MCLA-Launcher.exe | 1373696 | 6EBF1442F6F2748C35D5A5305BFA4FA4E48E5C5934840CD680AC57D24ED35BB1 |
| mcla_crash_handler.exe | 211968 | E155D6D82959A4A0D444170EF7E543616CBC8A7AA030DF762E6F2CEF8F02CED6 |
| mcla_diagnostic_image_helper.exe | 60928 | 93E703D26A3BF7D76DFEC416C779CFF9473C2771CEB86F7631FB4C3C2B5DDD7B |
| rexgpu-xenos.dll (dev SDK 851f03b) | 3026432 | 181F54B1B2201CE9A203DE311E02D47802B1315E72F384A843347126CAF2A4CD |
| rexruntime.dll (dev SDK, = l50) | 10984960 | 9B5F34EA378842AAC36E43ECCA69E7F492D57116631F7495CC981E5232B51462 |
| TracyClient.dll (dev SDK, = l50) | 246784 | DCBE9DE43D31EF366D76E6DB8CBB1045FAAB660ACA6508CD9202B3AA7EBB6BBB |

The cvar name, the exhaustion warning and the summary text are present in the
staged `rexgpu-xenos.dll`; `mcla_pipeline_wait_always` and the start log line
in `mcla.exe`.

- `private/l51-internal-prepare.ps1` (SHA-256
  86664166EE50EB56C265D7B9F7944E445978C25F52384D6C9A9DEBF294D01C2B), generated
  from `l50-internal-prepare.ps1` by `private/l51-work/mk_prepare.py` with
  `l51-work/l51-pins.json`. Same steps as l50: checks the seven pins and the
  exact inventory, signs a plain V1 ticket (no kill switch), stages
  `private/l51-internal` (CRT from l12, no guide), runs the internal staging
  audit, adds the l51 fingerprint as `l51-internal` to
  `owner-admitted-builds.txt` (existing rows kept). Parses without errors in
  Windows PowerShell 5.1 and PowerShell 7.
- Direct-run scripts (CRLF, ASCII; generated by `private/l51-work/mk_cmds.py`
  from `ki047-diag-1440p-normalcache.cmd`; same arguments, input checks,
  running-process refusal, MAIN user-tree copies before and after,
  `command.txt`, `exit.txt`, `cache-state.txt`; cache root
  `<profile>\cache\<l51 fingerprint>` computed at run time):
  - `private/ki047-wait-1440p-normalcache.cmd` (SHA-256
    1C9259F01536F48E7E83BF3DD0C42771EE5B272998418FE0CFF7BB0C6B1A8D0E): runs
    `private\l51-internal\build\mcla.exe` with `--d3d12_livery_diag=true`;
    the always-on wait is the host default. Run folder
    `private\ki047-ab\<UTC>-l51-wait-1440p-normalcache-some`.
  - `private/ki047-nowait-1440p-normalcache.cmd` (SHA-256
    30B6B5B6E490C4CD6101A2D2581C81FE13131EE4119E2B7CACE72FF9DC75DC9C): the
    same plus `--mcla_pipeline_wait_always=false` (control). Run folder
    `...-l51-nowait-1440p-normalcache-some`.

## 5. Owner steps

1. `C:\BDU\MCLA-Recomp\private\tools\pwsh\pwsh.exe -NoProfile -File
   C:\BDU\MCLA-Recomp\private\l51-internal-prepare.ps1`.
2. Cold run: `private\ki047-wait-1440p-normalcache.cmd` before any launcher
   session of l51 (`cache-state.txt` should say `absent`). Load, drive about
   60 s, Photo Mode on a police car side-on, F10 twice, quit.
3. Warm run: `ki047-wait-1440p-normalcache.cmd` again, same routine.
4. Control: `ki047-nowait-1440p-normalcache.cmd`, same routine. The store is
   warm by then, so this mainly confirms the switch and the warm skip rate.
5. Send each run folder's `mcla.log` (plus F10 packages and
   `cache-state.txt`); say whether the police livery and the first photo were
   correct.

Expected (not observed): cold run: `MCLA pipeline wait: always on` once;
`pso_pending_skip_per_minute` near 0 in minute 1 unless the 15 s budget is
exhausted (then one `budget exhausted` warning and skips for the rest of that
minute); visible hitches during world load and Photo Mode entry; a correct
first photo; minute-summary lines. Warm run: few or no waits, no summary for
idle minutes. Control: `always-on wait disabled by option`, skips as in l50.

## 6. Limits and risks

- Hitch cost on cold caches: a pending pipeline now stalls the GPU thread up
  to 2 s per draw and up to 15 s per minute. On a cold store the world load
  and Photo Mode entry will hitch visibly; with the budget exhausted the
  remaining skips of that minute still happen, so a very cold store may still
  bake an incomplete pass (the minute summary and `budget_skips` show it).
- Per-minute windows roll over at frame end, so one frame longer than a
  minute keeps its window's budget.
- The SDK commit is local and unpinned (dev SDK, `REXSDK_VERSION` empty).
  Volunteer builds need it published and pinned later; until then only
  owner-internal candidates carry it.
- `--d3d12_async_pipeline_wait_always=false` on the command line is not
  honoured as a user choice (see section 2); use
  `--mcla_pipeline_wait_always=false`.
- Not covered by tests: the D3D12 glue (IssueDraw/frame-end calls, logging)
  and the host start/shutdown wiring; compilation only.

## 7. DGX Spark routing

No Spark job: the work was a small, tightly-coupled code change whose
validation (unit tests, build, hash pins) is deterministic and cheaper to do
directly than to review a model answer.
