# M8 Windows launcher and baseline quality

Owner decision October 10, 2026. M8 starts with a usable internal Windows
launcher and selectable personal save profiles, then fixes problems visible on
the RTX 5060 baseline. Stock and a validated 60 FPS mode are the ceiling of this
milestone. Higher caps, variable/uncapped modes and secondary platforms are later
work, not mandatory M8 deliverables. M7 remains closed with its existing limits.

## Execution order

1. Separate immutable build files from personal data and define the save-profile
   library. Review the cross-build lock, full-profile identity and legacy import
   before allowing a new launcher to write real saves.
2. Build the working launcher: select build/game data, GPU, supported display
   settings, named save profile and local in-game display name; open the exact
   selected save and diagnostics directories. Keep command-line verification.
3. Prepare a reviewed successor to p1, preserving the original and an archived
   complete profile. Prove startup, actual F10, save/reload and window/audio
   lifecycle on that exact successor. Source-build PASS is not gameplay PASS.
   Owner accepted this on l2. Before handing it to volunteers, M8-019 separates
   the normal Play screen from infrequent Profiles/Setup controls, preserving
   controller actions, draft settings and keyboard/DPI support. Keep l2 intact.
4. Measure quiet/busy/race routes, including the reported intersection slowdown,
   and fix baseline Windows bottlenecks and reproducible graphics/audio/lifecycle
   defects. New progression blockers or save hazards interrupt this order.
5. Validate one opt-in 60 FPS mode and correct simulation through below-target
   dips. Reconsider the temporal test design before experiments; do not resume
   consumed WGC attempts or pretend image capture proves game-time correctness.
6. Run the bounded M7-derived regression for the supported stock/60 presets and
   close M8 only against the actually agreed capabilities and bug dispositions.

The launcher and personal-data model are working product internals, not a
throwaway UI and not the final installer. Importing an ISO, public installation,
updates, signing and release-ready distribution remain M9/later work.
The owner subsequently approved signed internal test-build expiry as M8-018
work, separate from release signing:14-day allowance and signed public extensions,
never terminating a running session. [Policy](test-build-expiry.md).

Display controls should select explicit render targets such as1280x720,
1920x1080,2560x1440 and3840x2160, independently of window size. M8-004 must verify
the SDK's actual supported dimensions and reject unsupported targets; relabeling
1x/2x alone is not acceptance. The current l2 UI still uses1x/2x.

## Named personal profiles

The local launcher library registers an opaque stable profile ID, a user label
and an explicit data-root path. Example labels are Main, Clean Start and Subtitle
Test. Labels are not XUIDs, filesystem names or in-game display names. Renaming a
label must not change storage identity. The library itself stays outside builds.

The selected profile, full path and build identity are visible before Play.
Selecting another profile changes only the launch selection: no automatic copy,
move, merge, restore, newest-save selection or fresh-profile fallback. Missing or
invalid data stops launch with an explanation. Switching profiles/settings is
disabled while a game session owns that profile.

One managed personal root contains the complete user/profile tree and its
recovery history. Cache, logs and private F10/crash packages also stay outside
the shareable build; cache namespaces include the compatible build fingerprint.
Runtime flags, diagnostics and receipts must all refer to the same selected
profile. The canonical profile lock excludes writers from different builds and
launcher instances, including path aliases. Network/cloud-sync use is not a
concurrent-writer guarantee; unsupported redirected/conflicted roots fail closed.

Existing managed roots can be registered directly. Legacy technical bundles
require explicit full-profile import into a new managed root, with source
quiescence, verified before/after inventories, source provenance and no overwrite.
The original remains intact. Do not point a new launcher directly at p1/user:
its old bundle lock does not coordinate with a new external-root lock. No legacy
save is imported or chosen automatically just because it was modified recently.

New profile, register existing profile, import legacy profile, duplicate a branch,
and restore backup are distinct actions. Destructive removal and automatic
branch reconciliation are not required for the first launcher. Backup/restore
must be explicit and recoverable; an ordinary folder switch never restores data.

## Volunteer boundary

The same eligible build can be used by different testers with independent
personal roots. Sharing a build must not include the owner's profile library,
saves, snapshots, settings, absolute personal paths, logs, crash/F10 packages,
API keys or source game data. No upload or automatic telemetry is introduced.

The current portable publisher intentionally includes game data and a selected
profile. Its output, including p1, is NOT the new volunteer artifact. A separate
allowlisted staging/audit route is required; do not ZIP the working bundle.
Repository restrictions on proprietary game/generated content still apply to
external delivery. Review actual distributable contents before any transfer;
this scope decision does not authorize sending existing private bundles.

Tester bug reports identify build, effective settings, hardware and observed
steps. Personal diagnostics remain local until a separate explicit export/share
action with a visible contents list. Save import/export is not hidden in reports.

## Acceptance for the launcher foundation

- Two different builds selecting one profile cannot write concurrently; different
  personal profiles do not exchange saves, settings or diagnostics.
- Empty, malformed, missing, read-only, Unicode/space/long-path, redirected,
  overlapping and interrupted-state cases have bounded positive/negative tests.
- Failed selection/import/settings writes preserve old valid data and selection;
  no game starts on a rejected configuration. No silent fallback to a new save.
- The settings layer is versioned and atomic, with explicit precedence and a
  capability allowlist. It does not rewrite the immutable build manifest/config.
- Legacy M7 verification and save lineage remain valid. Cross-build recovery and
  history capacity use explicit schemas and archive policy, not deleted evidence.
- Native fake-child tests precede any real-title test; physical acceptance is
  recorded only after the exact candidate is exercised.
- Volunteer staging tests reject planted saves, secrets, diagnostics and game
  assets, and never alter the original working package.

## Carried problems and deferred work

Active baseline triage includes paint/light amplification and overexposure
(KI-013/020), shimmer (KI-015), neon (KI-019), minimap flicker (KI-016), music
continuity (KI-017), Race Back (KI-026), busy-area slowdown and shutdown/reporting.
Separate hypotheses remain separate until evidence proves a shared cause.
Wheel hardware testing waits for available hardware; no unrelated feature grind.

Payback/SC/Unordered remain untested until naturally available. Deck performance,
Proton deployment, Linux, macOS, ultrawide, optional shader-cache distribution,
90/120 FPS, VRR and uncapped modes are later milestones without PASS or deadlines.
Their historical evidence remains preserved. Stock is not a measured stable-FPS
guarantee, and the old one-third Deck frame-time target is not a proved budget.

Spark remains the bounded advisory worker using short aliases and the ignored
key file, never PC-local inference. Primary review/tests own acceptance.

## Implementation checkpoint

October10: native launcher, managed host configuration, full-profile import,
cross-build locking, before/after archives and restore-to-new-profile passed
bounded fixture gates. Private candidate `l1` passed exact staged guest-free
host checks. M8-015/016 remain open until visible UI and real gameplay/reload/F10
acceptance; no automatic owner-profile migration or external transfer occurred.
See [engineering evidence](evidence/M8-launcher-foundation-20261010.md) and
[owner test](internal-launcher-owner-test.md). Original p1 remains unchanged.

October11: successor l2 adds signed expiry with policy1417/runtime37/HTTP26/
signing66 checks, three actual host rejection cases, staging63 and exact staged
guest-free host14 PASS. Owner subsequently approved the one-file DebugBundle
publication; exact public bytes and native signed extension/cache now PASS.
The owner subsequently completed l2 controls/audio/AltEnter/F10/reload and
launcher-close survival. Three exact sessions/snapshots and the real F10 frame
pass review; controller69 adds literal cross-build isolation. M8-015 is committed
as b67e1be; M8-016 acceptance closes separately at source0.20.1.0. Shutdown
reporting, render targets,60 and volunteer export remain open.
[Owner acceptance](evidence/M8-launcher-owner-acceptance-20261011.md),
[expiry evidence](evidence/M8-build-expiry-20261011.md).
