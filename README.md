# MCLA-R

MCLA-R is an experimental, unofficial native recompilation project for the Xbox 360 version of **Midnight Club: Los Angeles Complete Edition**.

The project aims to translate the original PowerPC executable ahead of time and provide the Xbox 360 runtime behavior it expects through ReXGlue plus game-specific compatibility code. It is not an emulator, a remaster, or a replacement game engine.

## Status

MCLA-R has representative offline single-player coverage on a local Windows
RTX 5060 laptop using Direct3D 12 and stock game timing. Accepted routes include
delivery, Red Light, Time Trial, Series, Tournament, Pinkslip, Wager loss,
Freeway, Goal Attack and Arcade Landmark, with the outcome and persistence
limits recorded in the [coverage matrix](docs/campaign-coverage-matrix.md).
This is not a full-campaign or every-event claim.

Clean startup, local display-name changes, progressed save reload, motorcycle/
tuning recovery, representative media/subtitles and window-only F10 capture have
bounded acceptance on separately identified test packages. The active owner
branch is the unchanged private p1 candidate; new source builds do not inherit
its gameplay acceptance. See [Windows acceptance](docs/evidence/M7-016-windows-acceptance-20261010.md)
and the [compatibility summary](docs/alpha-compatibility.md).

M7 is closed at source version0.19.0.0 after independent review and verified
dependency-first publication. [Closure receipt](docs/evidence/M7-closure-20261010.md).
[README-AI](README-AI) owns the ledger; the next step is owner review of M8 scope.
Payback, unconfirmed South Central event access and Unordered remain deferred
without PASS. Steam Deck/Proton, exact high-FPS timing and full campaign/end-game
coverage are outside this M7 acceptance.

Rendering shimmer/lighting defects, busy-area slowdowns, music continuity,
intermittent Race Back and wheel-centering issues remain documented in
[known issues](docs/known-issues.md). Stock timing is a correctness baseline,
not a measured stable-30-FPS guarantee. Linux, native macOS ARM64, high/variable
frame rates and ultrawide remain later gated goals.

This is an experimental developer project, not a public turnkey game release.

## Game data is not included

This repository does not contain the game, game assets, Xbox 360 executables, generated proprietary game code, title updates, DLC packages, or encryption/signing material.

Development and eventual user builds require a legally obtained user-supplied dump of the supported Xbox 360 Complete Edition. The implemented tooling validates the dump before extracting or using it and does not modify the source image.

Current research targets the following disc identity:

- Title ID: `545407F8`
- Media ID: `5940C9DB`
- ISO SHA-256: `AFCAAE68593246B1F83328681B690E254A13C37DD2E8DC34AB0DEB6C0F471FDB`

Support for any other region, revision, title update, or executable must be implemented and verified separately.

## Not a turnkey converter

Static recompilation still requires game-specific work for PowerPC control flow,
Xbox kernel/XAM behavior, graphics, audio, input, storage, timing and progression.
The source pins the MCLA-R ReXGlue v0.20.0.0 fork at an exact reviewed commit;
Xenia Canary remains an independent behavioral reference.
[SDK provenance](docs/rexglue-sdk.md) and [build instructions](docs/build.md)
separate the current source integration from tested older binaries.

Fresh SDK20 Release builds pass compilation with WGC enabled (82/82 actions)
and with its default OFF setting (77/77). These are engineering results, not
gameplay acceptance or promoted replacements for p1. General guest/diagnostic
teardown limitations remain; exit code zero does not prove graceful shutdown
of every worker.

## Planned user-supplied dump workflow

The intended public experience is the same broad model used by asset-free recompilation launchers: MCLA-R ships a launcher, runtime, validation metadata, and original project code, while the user selects their own supported disc dump. On first preparation, the launcher will verify the exact dump, safely extract the required files, run local code generation/compilation, and publish an atomic fingerprinted prepared-game directory. Later launches will reuse that prepared directory until repair or an update is required.

The current repository already implements the underlying developer pipeline—exact ISO validation, contained extraction, local code generation, native compilation, runtime launch, and data-integrity checks—but not the consumer launcher or clean-machine packaging UX. The source dump must never be uploaded, modified, bundled, or deleted; after successful preparation it need not be reread on every launch, but it remains user-owned and available for repair/rebuild. Generated proprietary guest code and game assets remain local and untracked.

## Private portable campaign-test bundle

The current owner entry, `Run M7 Profile Candidate.cmd`, targets `private/p1`.
That independently copied, owner-private Windows package includes prepared game
data and its own selected save; it is not redistributable. Its accepted hybrid
identities begin host `82A69B5E`, runtime `542DD347`, launcher `67B8F527`
and GPU `E2EA29B7`; full hashes are in the
[profile report](docs/evidence/M7-profile-rename-20261010.md).
It uses named RTX 5060 selection, stock timing, 2x target, VSync and fullscreen.
Do not merge the separate Clean, Sandbox or subtitle-test branches into it.

The launcher resolves paths from its own folder, verifies immutable hashes,
rejects conflicting/incomplete copies and prevents a second local writer.
Mutable saves, caches, logs, diagnostics and session records stay beside it.
Stop writers and complete synchronization before any archive or publication;
ambiguous progression requires review rather than choosing the newest file.

The unchanged normal `private/bundles/M7-016/current.txt` branch and historical
Deck packages are separate from p1. The new source launcher rejects a 33rd
combined session/rename record instead of pruning away lineage; synthetic native
tests pass, but that guard is not present in p1's accepted launcher.
Preserve/archive the complete branch before capacity requires a successor.
[Save acceptance and limits](docs/evidence/M7-009-save-acceptance-20261010.md).

## Diagnostics and crash reports

Press `F10` while the game is running to collect a local diagnostic package at the moment of a visual, input, audio, or transition softlock. The input callback only queues background work: bounded process/window/runtime state, an optional image, log tail, normal minidump and private save snapshot. The overlay shows `F10: RECORDING...`, then `F10: PACKAGE READY (IMAGE UNVERIFIED)` after publication. `BUSY` reports a repeated request and `FAILED` an unsuccessful publication. The overlay accepts no input. Package completion does not verify the image or guarantee a coherent snapshot of arbitrary concurrent save writes.

Window-only WGC images are accepted on the explicitly enabled p1 candidate.
WGC is OFF by default in source builds. A still image and completed audit do
not establish exact frame/sample alignment, capture cadence or measured FPS.
Never assume an image from an older or different package is valid merely because
its F10 package completed.

Unhandled native crashes are captured automatically by the separate `mcla_crash_handler.exe`. After the dump is safe, the failed title is allowed to exit while the helper finishes the local package and shows its exact folder path. Nothing is uploaded automatically. Dumps, logs, screenshots, and save snapshots can contain private paths, profile/gameplay data, or memory fragments, so send the folder path first and inspect individual files before publishing them.

The newest live or crash package can be located without browsing folders:

```powershell
.\scripts\show-latest-diagnostic.ps1
```

By default packages are below the Windows Documents folder in `mcla\diagnostics`; an explicit `user_data_root` moves them with the save, while `mcla_diagnostics_root` can place them in a separate explicit root such as the portable bundle's sibling `diagnostics` directory. The configuration template documents the enable switch, crash dialog, path, and rebindable `F10` action. Retention is bounded and stale partial packages are cleaned automatically.

## Development plan

The authoritative architecture, prerequisites, task ledger, milestone gates, acceptance criteria, risk register, and current decisions are maintained in [README-AI](README-AI).

Rules for handling game dumps, generated code, logs, traces, and release artifacts are defined in [docs/source-data-policy.md](docs/source-data-policy.md).

The bounded tested-build, reference-hardware, route, and limitation summary is maintained in [docs/alpha-compatibility.md](docs/alpha-compatibility.md); the full issue register is [docs/known-issues.md](docs/known-issues.md).

Developers with the legally obtained supported dump can validate the complete local environment without installing or modifying anything:

```powershell
powershell.exe -NoLogo -NoProfile -ExecutionPolicy Bypass -File .\scripts\bootstrap.ps1
```

The plan progresses through:

1. repository and tooling foundation
2. reproducible source validation and extraction
3. Xenia baseline and ReXGlue feasibility audit
4. native boot and frontend
5. first playable race
6. stable gameplay systems
7. representative Windows Complete Edition compatibility, with explicit deferred content
8. optional modern enhancements
9. release hardening and asset-free distribution

## Contributing

The project is not yet ready for general contributions. Coordinate before implementing broad runtime or enhancement changes.

Never open an issue or pull request containing copyrighted game files, memory dumps, private logs, credentials, tokens, or links to unauthorized game downloads.

## License

Original MCLA-R code and documentation are available under the [MIT License](LICENSE). This license applies only to material owned by the MCLA-R contributors. It does not grant rights to game data, generated proprietary game code, Rockstar/Take-Two/Microsoft/Xbox material, ReXGlue, Xenia, XenonRecomp, or other third-party dependencies and trademarks.

MCLA-R is not affiliated with or endorsed by Rockstar Games, Take-Two Interactive, Microsoft, Xbox, ReXGlue, XenonRecomp, or Xenia.
