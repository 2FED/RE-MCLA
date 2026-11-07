# KI-071 step 3: the l75 dev keys (frame trim, block-granular trim, PCM dump) (host)

Draft by an Opus implementation subagent, 2026-11-07. Host patch on main `3c30fd7` (the six source
files are byte-identical to `0df9289`; scratch `%TEMP%\claude\C--BDU-MCLA-Recomp\l75-host-patch`,
copy-only `apply.ps1`). Nothing was launched; no physical result is claimed. Input:
`M11-ki071-xma-frame-trim-sdk-20261107.md` (SDK `c526cb3`, `private/l75-sdk-install`).

## Dev keys (src/mcla_dev_overrides.h, 44 -> 48)
| key | phase | writes |
|---|---|---|
| `xma_honor_frame_trim` | startup (rexruntime) | `=true` writes true, `=false` nothing |
| `xma_frame_trim_block_granular` | startup (rexruntime) | same |
| `xma_dump_all_rates` | startup (rexruntime) | same |
| `xma_dump_contexts_on` | startup (rexruntime) | `=true` writes a composed directory to the string cvar `xma_dump_contexts`; `=false` nothing |

- None of the four is in a conflict pair.
- `xma_honor_frame_trim`, `xma_frame_trim_block_granular`, `xma_dump_all_rates` and
  `xma_dump_contexts` are in `kNeverWrittenCvars` (src/mcla_m10_defaults.h, 36 -> 40).
- An SDK without them (anything before `c526cb3`) logs the keys as unavailable.
- `xma_dump_contexts` itself is not a key: the strict `name=true|false` format cannot carry a path,
  and a line `xma_dump_contexts=...` is ignored as malformed (unit-tested).

## The dump directory (xma_dump_contexts_on=true)
- `KeyInfo` gains `true_source` (`TextSource::kLiteral` for every other key;
  `kXmaDumpDirectory` for this key only). Its `true_text` is only a description.
- Path (pure, unit-tested `XmaDumpDirectory` + `FormatDumpStamp`):
  `<diagnostics root>/xma-dump/<yyyyMMdd-HHmmss>`.
  - The diagnostics root is the one the diagnostics manager uses: `mcla_diagnostics_root`, or
    `<user-data>/diagnostics` when that is empty.
  - Session id: the host has no session id of its own. The launcher creates
    `<profile>\diagnostics\<session-id>` for each session and passes it as `mcla_diagnostics_root`
    (mcla_launcher_controller_win.cpp). So a launcher session dumps to
    `<profile>\diagnostics\<session-id>\xma-dump\<stamp>`, next to its `live\` F10 packages. A
    direct start uses `<user-data>\diagnostics\xma-dump\<stamp>`.
  - `<stamp>` is the UTC time of the override apply (game start), `yyyyMMdd-HHmmss`. It keeps
    sessions that share a root apart; the SDK numbers its files from 0 in each session.
- `OnStartup` now takes the two roots (`OnStartup(user_data_root(), mcla_diagnostics_root)` in
  `MclaApp::OnPostInitLogging`, which already runs before the guest and before the diagnostics
  manager starts).
- Order in the apply loop: the cvar must be registered and still at its default (as for every key);
  only then is the directory made, made absolute and normalised, and created with
  `std::filesystem::create_directories`. That is the diagnostics manager's own posture (the same
  call, under the same root, inheriting the folder's ACL). After creation `symlink_status` must be a
  plain directory (a reparse point or junction there is refused).
- The cvar text is `path::string()`, the exact inverse of the SDK's
  `std::filesystem::path(std::string)` (process code page on Windows). A path that the code page
  cannot represent is refused with an error line instead of being written broken.
- Any failure logs one ERROR line and writes nothing (the SDK then dumps nothing). Success logs
  once at INFO: `MCLA dev override: xma_dump_contexts_on=true (xma_dump_contexts=<path>)`.
- The diagnostics manager prunes only `live\` and `runtime\`; `xma-dump\` is never pruned or
  exported automatically. The SDK caps a session at 64 files and 60 s per stream.

## l75 arms (private\l75-arms, LF, 48 values each, 1428-1436 bytes)
| arm | = |
|---|---|
| `pc\base` | l74 `pc\base` + the four keys false |
| `pc\trim-trace` | base + `xma_sibling_trace` + `mcla_xma_consume_probe` + `xma_dump_contexts_on` |
| `pc\trim-fix` | trim-trace + `xma_honor_frame_trim` |
| `pc\trim-block` | trim-fix + `xma_frame_trim_block_granular` |
| `pc\steady`, `pc\fetch-off`, `pc\fetch-verify`, `deck\saves-fix`, `deck\steady` | the l74 arm + the four keys false |

`xma_dump_all_rates` is false in every arm (44.1 kHz only). Checked with the production parser
(`check_arms.exe` built from the patched header): 9 arms, status parsed, 48 values, 0 ignored lines,
0 mismatches against the expected true sets. Checklist: `private\l75-CHECKLIST.txt` (placeholders
`<PREPARE-SCRIPT>`, `<BUILD-FOLDER>`, `<HOST-COMMIT>`, `<SDK-COMMIT>`; steady on hold).

## Tests
- `tests/mcla_dev_overrides_test.cpp`: lookup, phase, mapping of the four keys (incl.
  `true_source`, only one composed key), conflict-pair absence, near misses (prefix / suffix / case,
  the string cvar name itself), the stamp format and range checks, the path composition for a direct
  start, a launcher session and the empty cases, the four l75 arms (48 values, nothing ignored,
  under 2 KiB) and an l75 near-miss file (a path value, non-bool values, unknown prefix, repeat).
  The l74 arm check now asserts 44 values (it asserted `kKeyCount`).
- `tests/mcla_m10_defaults_test.cpp`: the four cvars are never written and not defaults; the set has
  40 names.

## Build check
Scratch tree `%TEMP%\claude\C--BDU-MCLA-Recomp\l75-host-src` (main `3c30fd7` tracked build inputs +
this patch) against `private\l75-sdk-install` (SDK `c526cb3`), build dir `private\l75-host-check`, the
l74 option set (expiry, raw timer, MCLA_DEV_OVERRIDES, M10 defaults, image/diagnostic tests; LLVM clang++
toolchain, Ninja, Release). Run only after `wait-for-idle-pc.ps1` reported IDLE.
- Build: 270/270 steps, 0 warnings, 0 errors (`mcla.exe` sha256
  `2d05f49ef6eebf7279eb22e36d0982eb9709a480d0f272479bd1c427acddeb58`, check build only, not a
  candidate).
- Tests: 52/52 test executables rc=0 (`mcla_dev_overrides_test` 502 checks,
  `mcla_m10_defaults_test` 3211 checks). `mcla_diagnostic_image_client_test` also passed this time
  (it failed "bounded observed return" in the l74 check; unrelated, not touched).
- ast-grep 0.50.0: `scan` rc=0, `test` 3/3.
- Arms: 9/9 parsed with the production parser, 48 values, 0 mismatches.
- No game, launcher or mcla.exe was started.

## Risks
- The dump directory is created even if the SDK then writes no file (no 44.1 kHz stream); an empty
  `xma-dump\<stamp>` folder is harmless.
- The stamp is UTC (the owner's local time differs); two starts within one second under one root
  would share a folder (not reachable with the launcher's per-session roots).
- File I/O on the audio thread is the SDK's (dump arm only); see the SDK evidence.
