# M7-016 Steam Deck first physical return

## Decision

`deck-proton-first-boot-partial-pass-performance-blocked`

The exact Syncthing-returned `0.9.0.0` child physically reached saved gameplay
and the garage under gamescope on Steam Deck, then was stopped through Steam.
The installed Proton version was not recorded and the complete M7-016 Deck
matrix was not performed, so this is not a Steam Deck support claim.

## Returned runtime evidence

The returned `logs/mcla.log` is 11,298 bytes with SHA-256
`848EA73474D0C2C965C42D20C62956D88DEDA347BF31AE9252C7C0EDDB12E2A1`.
It records:

- successful runtime initialization and first active guest output;
- D3D12 presentation through `AMD Custom GPU 0932 (RADV VANGOGH)`;
- guest source `1280x720` presented to a `1024x640` swapchain;
- no fatal, device-removed, or normal-shutdown marker.

The owner confirmed visible gameplay and garage entry. F10 package
`live-20260904T123320Z-340-1` completed at `2026-09-04T12:33:22Z`. Its
manifest SHA-256 is
`EA5DB1D95825EAE45294EB809D1D1A4C3FF5BBE3ECC0A3B2728FF4FB4C3B8B94`;
state SHA-256 is
`75C6910D9D3F63270C5BF874E2B4C8C7138A7D64A36D9CF96AB71768504F05FB`.
The atomically published package contains a client-surface frame, log tail,
native minidump, safe state, save metadata, and a private four-file profile
snapshot totaling 3,815,088 bytes. It reports focused fullscreen at
`1024x640`, proving default Deck fullscreen for this run.

## Cadence finding

The F10 state records 13,928 guest vblanks but only 2,116 successfully refreshed
active guest outputs. From session creation at `12:29:27Z` to package creation
at `12:33:22Z`, these cumulative counters average 59.27 vblanks/s and 9.00 new
guest outputs/s, with new output on only 15.19 percent of vblanks. This window
includes boot, loading, and menus, so 9.00 is not a gameplay-only FPS result.
It does prove that MangoHUD's reported 60+ host presentation cadence is not the
title's unique-frame throughput and is consistent with the owner's roughly
20-FPS gameplay observation. A later run must take two F10 snapshots about 30
seconds apart during active driving so the counter delta isolates gameplay.
CPU/GPU causality is not yet established.

## Interrupted close finding

Steam Stop terminated both title and launcher before the launcher could publish
its final result. Syncthing returned a stale exclusive `bundle.lock` plus
`results/session-20260904T122927Z-300.partial`; the initial recoverable profile
snapshot exists, but no `result.json` does. Version `0.9.0.1` therefore treats
only an unlocked, identity-valid portable session staging directory as an
interrupted prior run, captures the latest save, publishes a distinct
`mcla-portable-interrupted-session-result-v1`, and atomically removes the
`.partial` suffix. Active exclusive locks, reparse points, Syncthing conflicts,
unknown partial paths, and invalid session identities still fail closed.

The returned private F10 package, runtime log, stale lock, partial session, and
operator images are preserved outside Git before current-build retention can
remove the old child. The new recovery behavior has an end-to-end Windows
fixture; Proton recovery still needs the next physical launch.

## Successor bundle

Clean build `20260904-132416-c0cbf180` publishes version `0.9.0.1` as
`mcla-0.9.0.1-win64-030214B40241F737-70A34AEA064CBD79-58B3385C89D9E499`.
Its launcher SHA-256 is
`70A34AEA064CBD79C9025BAA6A8912FB45C5CFD6767657E27A5C78E634D4D87C`,
its manifest SHA-256 is
`E4305735929BCAC05D66A3F8BDD489687D168185746B069534130DBDB827D0BE`,
and its 25-file hash lock SHA-256 is
`B76295204D3C1AC8BF379C4E3573482B42C35AA49F0F549810282D24C0CDB1F4`.
The clean Windows relocation probe and diagnostic/save-session verification
both pass. Bounded retention removed the returned `0.9.0.0` child from the
Syncthing root only after its evidence was preserved and its stale lock was
opened exclusively to prove there was no live writer.

## Remaining physical gate

- record the exact installed Proton version;
- confirm audio explicitly;
- test Alt+Enter and left-button double-click when keyboard/mouse are attached;
- evolve the save, stop through Steam, relaunch, and verify recovered progress;
- capture two active-gameplay F10 samples for exact guest-output cadence;
- investigate the severe Deck performance deficit before campaign testing.
