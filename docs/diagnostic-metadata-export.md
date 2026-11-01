# Local diagnostic metadata export

The launcher's Play page has `Export report...`. It builds a new, typed report
from one completed managed session and one completed live F10 package. It never
sends anything. The original F10/crash folders stay private and are not copied.

History: written in M8-018 (private l8 to l12), ported to the repository launcher
in M9-002, extended for Unlocked timing in M9-003, for ticket-granted sessions and
the per-minute performance summary in M9-005, and for the controller trigger
reasons in M9-010. The policy is `src/mcla_diagnostic_export_policy.h`; the native
reader/publisher is `src/mcla_diagnostic_export_win.cpp`. Native and policy test
suites cover it. The M9 evidence records no separate physical owner acceptance of
the export dialog, and this document does not approve external delivery.

The volunteer workflow (M9-005) is separate. The game writes a prefilled
`report.txt` into each package folder, and the launcher's post-session prompt
fills in the session facts. That file is for the tester's answers. It is not a
manifest artifact (it has no hash), and the export never reads or includes it.

## Report contents

The output contains exactly `report.json` and `EXPORT-COMPLETE.txt`. The latter
records the report's SHA256 and byte count; it does not authenticate its origin.

Every field in `report.json` is built from typed scalars or enums, never copied
from a source subtree:

- `mcla_version`, and `reason`: one of `f10`, `unexpected-guest-exit`,
  `wheel-r2`, `wheel-l2`, `pad`, or `other` for any unknown value. The
  manifest's `trigger` object (category `general`/`wheel`, source) is not
  exported as such; the reason carries the source.
- `session_recorded`: the recorded build and game fingerprints, `timing`
  (`stock` or `unlocked`), scale, VSync, fullscreen, GPU vendor category
  (`NVIDIA`, `AMD`, `Intel` or `other`), and the render target and guest mode
  when the session receipt has them. `gpu_driver` and `cpu` are always
  `unavailable`; the exporter does not inspect the current machine instead.
  `capabilities` is `recorded-v1` when the session has a capability record and
  `unavailable-legacy` otherwise. With a record, `guest_refresh_hz` is the fixed
  stock value 60, also for Unlocked sessions; it is not the vblank rate the
  session actually used.
- `capture.frame_written`: whether the package recorded an image.
- `diagnostic_window`: logical/physical window size and fullscreen, when
  `state.json` has them. Window size is not a render-resolution measurement.
- `performance_per_minute` (optional): read from the package's private
  `log-tail.txt` when present (pinned, at most 1 MiB, bound into the
  confirmation). Only four counter lines are parsed into numbers: frame pacing,
  draws per frame, GPU busy and CP replay. Up to the last 10 minutes are kept, as
  `fps`, `frame_p50_ms`, `frame_p95_ms`, `frames_over_50ms`,
  `gpu_busy_ms_per_frame`, `draws_p50` and `cp_replay_ms_per_minute`. A value
  that does not parse is left out, not guessed. No other log text can reach the
  report. When present, the disclaimer
  `performance-counters-from-log-not-certified` is added.
- `disclaimers`: local directory provenance is not authenticated; settings are
  as recorded, not observed; no measured FPS or world certification; review
  before sharing.

Admission. A Stock or Unlocked session exports. A session whose capability record
ends in `admission owner-allowlist` (owner admission file, KI-045) or
`admission ticket-grant` (signed ticket grant, M9-005) exports only if the record
re-serializes byte for byte. The export has no ticket to check, so a ticket-grant
record is rebuilt with every token granted. An ungated record that claims a grant,
a gated record without one, or a record with two admission keys is rejected.

Older session receipts may lack target dimensions or a capability record.
Missing fields stay unavailable; they are not rebuilt from current preferences.
Recorded settings are not observed runtime configuration, a rendering-quality
verdict or proof of an authenticated build.

Excluded: saves and their inventories, snapshots, images, dumps, raw logs (beyond
the parsed counters above), `report.txt`, profile/session IDs, local paths,
timestamps, process/thread information and free-form diagnostic strings.
Source `safe_to_share` flags grant no authority.

## Operator procedure

1. Close the game normally, select the profile in the launcher and choose
   `Export report...`.
2. Browse to one completed `live-*` package (not `.partial`) and to an existing,
   separate destination parent folder.
3. `Preview report` shows the two files and their new destination without writing
   anything. Read the scrollable preview.
4. `Save report` publishes locally; `Cancel` releases the preview. Changing either
   path invalidates the preview. Moving keyboard focus is not a confirmation.
   Opening or cancelling the dialog does not save or discard Setup drafts.
5. Review the saved report before sharing it by hand. Nothing uploads, and this
   procedure does not authorize distribution of a game build or assets.

The window fits the monitor work area; outer scrolling is used only when the
controls do not fit, and keyboard focus brings controls into view.

## Command line

Build target `mcla_diagnostic_export_win` (excluded from the default build). It
produces `MCLA-Export-Metadata.exe` in the build directory; it is not part of a
staged owner or volunteer bundle. No lease, running game or game assets are
needed to read an old package. The configured root directories must be
accessible so the destination can be checked against every protected location.

1. Close the profile's game session normally. No import, restore or new profile
   is needed.
2. Identify the exact session and package. The tool never picks a latest package
   or save. Only complete `live-*` packages are supported; native-crash and
   partial packages fail closed.
3. Use an existing, separate local folder outside every registered profile,
   build, game and launcher-settings directory.
4. Run the tool with `--settings-root`, `--session`, `--package` and
   `--destination-parent`. `--preview-only` writes nothing and prints the two
   exact file contents, exclusions and the newly allocated destination.
5. For an interactive export, read the preview and type `EXPORT`. Any other
   answer or end of input cancels without creating a folder. The source profile
   is held against cooperative writers while the preview is open.
6. Review the report before sharing it by hand.

## Integrity and limits

Session and package directories must be beneath the same managed profile and
session ID. This is local directory provenance, not cryptographic attribution.
The exporter pins the consumed marker, session records, capability record,
manifest, state and optional log tail against writes and replacement, and checks
the state's manifested hash and length. It does not read or certify the other
private artifacts.

Confirmation binds the exact preview, destination and consumed source hashes.
Publication uses a new random child folder and exclusive file creation; a
collision never overwrites data. Output is flushed, read back and checked against
the exact two-file inventory. Interrupted output is kept for investigation; a
completion marker without a valid inventory is not acceptance.

As in the profile-creation code, Win32 directory creation and handle pinning are
separate calls. Held ancestors, random names and empty-directory checks protect
the cooperative local-writer workflow; they do not make directory creation
atomic against a hostile process of the same user.

Not part of this feature: native-crash metadata export, optional attachments,
whole-package ZIP export, automatic upload, and save export disguised as a bug
report.

Evidence: [M9-002 port](evidence/M9-002-launcher-source-reconciliation-20261028.md),
[M9-003 Unlocked export](evidence/M9-003-launcher-mode-option-20261028.md),
[M9-005 performance summary](evidence/M9-005-volunteer-build-20261028.md),
[M9-005 ticket grants](evidence/M9-005-ticket-capabilities-20261029.md). The
original M8 report (`docs/evidence/M8-metadata-export-20261011.md`) is kept on the
local archive branch `wip/main-dirty-m8-20261101`, not on main.
