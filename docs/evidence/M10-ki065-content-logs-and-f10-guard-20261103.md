# M10 KI-065: SDK content/save logs and F10 save-copy guard (l67), 2026-11-03

Bug: KI-065 "storage device was removed" after a police fine on the Deck
(l66 hot-on), with the save coinciding with an F10 export that copies the live
save tree. Analysis: [l66 Deck A/B part 2](M10-l66-deck-ab-20261103.md).
This change adds the missing evidence path (SDK logs) and closes the race on
the host side (F10 never copies while a save transaction is open). It is
implemented and unit-tested only; no runtime, PC or Deck result exists yet.

## A. SDK: branch `mcla/m10-content-logs`, commit `c63895c` (local, not pushed)

Base: published v0.20.0.7 `43577c8`. Files: `include/rex/content_log.h`,
`src/core/content_log.cpp` (new, in rexcore), `content_manager.{h,cpp}`,
`xam_content.cpp`, `xam_content_device.cpp`, `host_path_entry.cpp`,
`host_path_file.cpp`, `xboxkrnl_io.cpp`, `xboxkrnl_io_info.cpp`, tests.

### Log lines (WARN, prefix `[content]`, category sys)

Each kind is paced separately: burst of 16 lines, then 1 line per second; the
next line written after a suppression ends with
`(+N earlier <kind> lines suppressed)`. Lines name the save by its package
file name and content type, or by the device-relative path; never a host path,
so no Windows/Steam user name. Kinds: create, close, flush, delete,
transaction, header, file-open, file-write, file-setinfo, device.

| Where | Line (shape) |
|---|---|
| XamContentCreate / CreateEx / CreateInternal | `XamContentCreate failed: stage=<disposition\|capacity\|begin-transaction\|delete-existing\|create-package\|write-header\|open-package> flags=0x.. root='save' content='<file>' type=00000001 result=0x.. hresult=0x.. probe=<0\|1>`; `... rollback failed after stage=...`; bad content-data size |
| XamContentClose / Flush / Delete / DeleteInternal | `XamContentClose failed: root='..' result=0x.. hresult=0x..` |
| Transaction Begin | `Begin failed: stage=<recover\|already-active\|backup-package\|backup-header\|create-marker-directory\|create-marker> content='..' type=.. result=0x.. win32=N (<text>)` (BUSY = 0xaa) |
| Transaction Recover | `Recover failed: stage=<remove-package\|remove-header\|restore-package\|restore-header\|remove-marker\|remove-package-backup\|remove-header-backup> ... win32=N (..)`; success after an interruption: `Recover restored the previous save after an interrupted transaction: content='..'` |
| Commit / Rollback | `Commit failed: stage=<remove-marker\|remove-package-backup\|remove-header-backup> ...`; `Rollback failed: stage=recover ...` |
| Header / package / thumbnail / delete | `WriteHeader failed: stage=<create-directory\|open\|write>`, `CreateContent failed: stage=<package-exists\|create-directory>`, `DeleteContent failed: stage=<content-open\|remove-package>`, `UnmountAndDeleteContent failed`, `Get/SetThumbnail failed: stage=open` |
| Host files on `\Device\Content\N` | `content file open failed: path='..' access=0x.. status=0xc000000f win32=32 (The process cannot access the file because it is being used by another process)`; `content file create failed`, `content directory create failed`, `content entry delete failed`, `content file write failed: ... win32=N`, `content file set-length failed` |
| NtWriteFile / NtSetInformationFile | `NtWriteFile failed on content file: path='..' length=N status=0x..`; `NtSetInformationFile failed on content file: path='..' class=N status=0x..` |
| Storage device path | `XamContentGetDeviceState failed: device_id=0x.. result=0x48f` (also GetDeviceName, GetDeviceData, CreateDeviceEnumerator, CreateEnumerator) |

The Win32 error is `GetLastError()` taken immediately after the failing call
or the `std::error_code` of the filesystem call, with the English system text.
NtCreateFile failures keep their existing `[NtCreateFile] FAILED` line.

Behaviour changes beyond logging: `create_directories` in CreateContent and
WriteContentHeaderFile returns ACCESS_DENIED instead of throwing; thumbnail
get/set return ACCESS_DENIED instead of dereferencing a null FILE*.

### Transaction indicator

`rex::content_log::TransactionsActive()` (also
`rex::system::xam::ContentTransactionsActive()` and the exported C function
`rex_xam_content_transactions_active()` of rexruntime.dll) = saved-game
transactions begun and not yet committed or rolled back + content operations
mutating the save tree right now (Begin's recovery and backup copy, Recover,
Commit, Rollback). Atomic counters; a ContentManager destroyed with open
transactions stops counting them.

### SDK tests

- `[content-log]` 6 cases / 91 assertions: burst 16 then 1/s, cap at the
  burst after a quiet period, backwards clock, per-kind independence and the
  suppressed count, `\Device\Content\` match, error text, indicator with busy
  scopes and saturation.
- `[content]` 12 cases / 153 assertions, two new: indicator across Begin,
  refused second Begin (BUSY), enumeration and Commit (and a second Commit);
  Rollback and teardown with an open transaction.
- ctest: 100% of 2014 passed (2006 at v0.20.0.7 + 8 new; 8 skipped as at the
  base). Warning set identical to the l66c build; clang-format clean.

Build: `private\l67-sdk-b` (Ninja Multi-Config, Release), output
`private\l67-sdk-o`, install `private\l67-sdk-install`, D3D12 only (l66c/l65v
configuration).

## B. Host (base main `267ae4b`)

The M10-008 triage (267ae4b) already copies saves through a fully shared
read, leaves `.rexbackup`/`.rexpending` out and logs `save copy start/end`.
This patch adds only:

- `src/mcla_save_copy_guard.h` (pure): gate decision, reasons, skipped note,
  retry rule. `tests/mcla_save_copy_guard_test.cpp` (registered with the M9
  hook tests in CMakeLists.txt).
- `src/mcla_diagnostics.cpp`:
  - Declares `rex_xam_content_transactions_active` with
    `/alternatename` to a local fallback that returns 0xFFFFFFFF
    ("unavailable"), so the host still links against an SDK without the
    indicator (verified with a probe: without rexruntime.lib it prints the
    fallback; with the l67 import library it imports the SDK function).
  - Before the save copy: poll every 50 ms, up to 2 s, the SDK indicator and
    the save tree for `*.rexpending` / `*.rexbackup` (either one means busy).
    Clear: copy (log `save copy start (gate waited N ms, SDK transaction
    indicator K)`). Still busy at 2 s (or export cancelled): no save copy for
    this package, `save-copy-skipped.txt` written into the package with the
    reason, WARN `save copy skipped: <reason> after N ms`.
  - After the copy, a WARN if the indicator shows a transaction began during
    it.
  - A copy that failed with ERROR_SHARING_VIOLATION (32) or
    ERROR_LOCK_VIOLATION (33) waits 100 ms once before the next attempt
    (`save copy retry <file> in 100 ms (Win32 error N)`).
  - `save-metadata.json` gains `copy_gate` (copied/skipped),
    `copy_gate_wait_ms`, `sdk_transaction_indicator` (available/unavailable);
    the manifest capture gains `save_copy_skipped`. `save-copy-skipped.txt` is
    not a manifest artifact (like `report.txt`); the export policy's artifact
    list is unchanged.

Log lines contain no paths beyond the save file name and no profile id.

### Expected log lines when the owner reproduces

Reproduction on the Deck with the l67 build: drive until a police pull-over
starts and press F10 exactly as it begins (the game saves after the fine).

- Guard working, save in flight: `MCLA diagnostics: save copy start (gate
  waited 150 ms, SDK transaction indicator 0)` then `save copy end ...`; or,
  if the save outlasts 2 s, `MCLA diagnostics: save copy skipped: SDK reports
  1 active content transaction(s) after 2000 ms (SDK transaction indicator
  1)` and `save-copy-skipped.txt` in the package. No dialog expected.
- If the dialog still appears, the `[content]` WARN line(s) just before it
  name the failing step, the X_RESULT and the Win32 error, e.g.
  `[content] Begin failed: stage=backup-package content='..' ... win32=32
  (...)` or `[content] XamContentCreate failed: stage=begin-transaction ...`.
  These are the lines to send.

### Host tests and build

- `mcla_save_copy_guard_test`: 43 checks (timing contract, indicator
  available/unavailable, busy before/at/after the 2 s limit, cleared after a
  wait, pending marker names and non-matches, reasons and note without paths,
  retry only once and only for errors 32/33, simulated poll loops: copy after
  150 ms; skip after 41 polls at 2000 ms).
- Scratch tree `private\l67-host-src` = main `267ae4b` (tracked files) +
  this patch, built in `private\l67-host-build` against
  `private\l67-sdk-install` with the l65 volunteer-like options
  (MCLA_M10_SPEEDUP_DEFAULTS ON, MCLA_PREWARM_SEED_DEFAULT ON,
  MCLA_DEV_OVERRIDES OFF, perf split, livery diag2, readback sync composite
  ON, expiry ON, WGC OFF, REXSDK_VERSION empty = dev SDK): default build +
  52 targets, 0 warnings; all 47 `*_test` executables pass
  (`mcla_diagnostic_image_client_test` failed once on the first, loaded run
  with "bounded observed return" and passed 3 reruns and the full rerun; a
  timing flake unrelated to this change). ast-grep 0.50.0 scan clean, rule
  tests 3/3.
- `mcla.exe` imports `rex_xam_content_transactions_active` from
  rexruntime.dll; path scan of mcla.exe, crash handler and launcher: no
  build-path strings (only machine-code byte matches and existing launcher
  strings).

| Artifact | SHA-256 |
|---|---|
| SDK commit | `c63895c8ed53748d04de91403c467ece2e8f6fa8` (branch mcla/m10-content-logs, base 43577c8) |
| l67-sdk-install bin/rexruntime.dll | `dc0950f1f84184c4f66ddbcf6cf48dac8ca10612530a3e22b9c39d371103dce6` |
| l67-sdk-install bin/rexgpu-xenos.dll | `d812c51bd07b2235fd1dac12bf0ffa27e850b657859ab5b7a7268fbd01487771` |
| l67-sdk-install lib/rexruntime.lib | `2dfea9b61de3dc9c330d17cb5b269893b28238ee286d31348db54d4b51c36919` |
| l67-host-build mcla.exe | `49cb4b3f2639cc7cfee64eac9ab5b12e75bf4cf6be5932095367cc992eba66c0` |

This is a build/test record only. No owner build was packaged and nothing was
run on the PC or the Deck.

## Risks

- A transaction that stays open longer than 2 s (or a stale `.rexpending`
  left by an interruption and not yet recovered by the SDK) makes every F10 in
  that window skip the save copy; the note says why. The SDK recovers stale
  markers on the next enumeration/open of that save.
- The guard narrows but cannot close the window: a save can begin right after
  the gate clears. The shared-read copier (267ae4b) is what keeps that case
  from blocking the game; the post-copy WARN records it.
- `/alternatename` and the weak import were verified with lld-link only.
