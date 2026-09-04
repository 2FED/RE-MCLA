# MCLA-R

MCLA-R is an experimental, unofficial native recompilation project for the Xbox 360 version of **Midnight Club: Los Angeles Complete Edition**.

The project aims to translate the original PowerPC executable ahead of time and provide the Xbox 360 runtime behavior it expects through ReXGlue plus game-specific compatibility code. It is not an emulator, a remaster, or a replacement game engine.

## Status

MCLA-R has completed M6 stable-gameplay validation and is entering M7 campaign compatibility work. Version `0.9.0.0` retains the reviewed alpha compatibility matrix, pins ReXGlue v0.10.0.2, fixes black Photo Mode captures and the reached delivery-transition crash, preserves progressed profiles, and includes local F10 live diagnostics plus automatic out-of-process native crash packages. Accepted long-session evidence combines a 7,202-second frontend stage with a current-artifact 3,600-second mixed-gameplay process; it does not claim one same-artifact three-hour run. The intermittent Race Back camera and wheel-centering defects remain open. M7 now has a private, relocatable Windows/Proton campaign-test folder whose native launcher verifies the immutable runtime/game payload and keeps saves, cache, logs, diagnostics, and session results inside that one Syncthing-friendly root. Clean-path Windows relocation and diagnostic/save round-trip are verified; its runtime supports Alt+Enter and left-button double-click fullscreen switching, but no Steam Deck compatibility is claimed until the same folder passes physical pinned-Proton testing. Current evidence covers guest-backed presentation, local offline profiles and transactional saves, race progression, all major city regions, repeated races, bounded audio and performance telemetry, standard controllers, and model-agnostic SDL racing-wheel input with physically verified Thrustmaster T300 force feedback. Full campaign correctness, cross-model wheel validation, remaining rendering defects, higher/variable frame rates, secondary-platform builds, and public packaging are still open, so this remains an experimental developer build rather than a public game release.

The initial target is:

- Windows 10/11 x86-64
- Direct3D 12
- offline single-player
- original 30 FPS behavior until compatibility is established

Linux, native macOS ARM64, higher or variable frame rates, ultrawide support, and other enhancements are later gated goals and are not currently supported.

## Game data is not included

This repository does not contain the game, game assets, Xbox 360 executables, generated proprietary game code, title updates, DLC packages, or encryption/signing material.

Development and eventual user builds require a legally obtained user-supplied dump of the supported Xbox 360 Complete Edition. The implemented tooling validates the dump before extracting or using it and does not modify the source image.

Current research targets the following disc identity:

- Title ID: `545407F8`
- Media ID: `5940C9DB`
- ISO SHA-256: `AFCAAE68593246B1F83328681B690E254A13C37DD2E8DC34AB0DEB6C0F471FDB`

Support for any other region, revision, title update, or executable must be implemented and verified separately.

## Not a turnkey converter

Static recompilation does not automatically make an Xbox 360 game portable. MCLA-R requires game-specific work for PowerPC control flow, Xbox kernel and XAM behavior, graphics, audio, input, storage, timing, retired online-service paths, and progression compatibility.

The project uses an exact MCLA-R ReXGlue v0.10.0.2 fork as its recompilation/runtime base and uses Xenia Canary as a behavioral reference. Both upstream projects remain independent from MCLA-R; the fork is pinned for tested vector-codegen validation, Windows Unicode paths, corrected window/input destruction ordering, fail-closed game-data VFS behavior, deterministic offline-service states, Xenia-compatible guest-thread startup ordering, privacy-safe guest crash reports, guest-backed D3D12 presentation/capture telemetry, bounded runtime auditing, stable latest-active controller routing across Steam Input mirrors, model-agnostic SDL wheel input, the title's 64-slot force-feedback surface, runtime fullscreen shortcuts, correct mounted-root VFS resolution for saved profiles, nonvirtual guest-output/vblank timing diagnostics, and privacy-safe audio/performance telemetry. General forced guest-thread teardown can still poison a guest-heap lock; the synthetic diagnostic route is contained and normal external-close cycles are independently verified, but this SDK limitation is not claimed as fixed. Project diagnostics use nine independently filterable categories: app, PPC, kernel, XAM, VFS, GPU, audio, input, and patches.

## Planned user-supplied dump workflow

The intended public experience is the same broad model used by asset-free recompilation launchers: MCLA-R ships a launcher, runtime, validation metadata, and original project code, while the user selects their own supported disc dump. On first preparation, the launcher will verify the exact dump, safely extract the required files, run local code generation/compilation, and publish an atomic fingerprinted prepared-game directory. Later launches will reuse that prepared directory until repair or an update is required.

The current repository already implements the underlying developer pipeline—exact ISO validation, contained extraction, local code generation, native compilation, runtime launch, and data-integrity checks—but not the consumer launcher or clean-machine packaging UX. The source dump must never be uploaded, modified, bundled, or deleted; after successful preparation it need not be reread on every launch, but it remains user-owned and available for repair/rebuild. Generated proprietary guest code and game assets remain local and untracked.

## Private portable campaign-test bundle

M7 development uses an owner-private, toolchain-free Windows x64 folder below ignored `private/bundles/M7-016`. This is an internal test artifact containing the locally prepared game and selected save; it is not redistributable and is not a substitute for the future asset-free consumer workflow.

The stable Syncthing root is `private/bundles/M7-016`. `current.txt` names the latest fingerprinted child. After synchronization is completely idle, launch that child's `Launch-MCLA.exe`; the launcher resolves everything from its own directory, validates all immutable file hashes, and refuses incomplete/conflicted copies, reparse points, or a second concurrent writer. The bundle is locked to fullscreen startup and supports runtime windowed/fullscreen switching through both Alt+Enter and LMB double-click. Do not run the same writable bundle on two hosts at once.

Each run keeps its mutable state in sibling `user`, `cache`, `logs`, `diagnostics`, `results`, `update`, and `metadata` directories. Session and save-snapshot publications are atomic and retain at most 32 completed entries. Consequently a returned Syncthing copy contains the progressed profile, ordinary logs, F10/native-crash packages, and machine-readable session results without depending on repository paths or developer tools. Windows relocation is verified; Steam Deck/Proton remains pending physical proof.

## Diagnostics and crash reports

Press `F10` while the game is running to collect a local diagnostic package at the moment of a visual, input, audio, or transition softlock. The input callback only queues the background work, which records bounded process/window/runtime state, a screenshot of the already-presented window, a bounded log tail, a normal minidump, and a stable private save snapshot. A Windows sound confirms completion; repeated requests are ignored while one capture is active.

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
7. full Complete Edition campaign compatibility
8. optional modern enhancements
9. release hardening and asset-free distribution

## Contributing

The project is not yet ready for general contributions. During native boot work, coordinate before implementing broad runtime or enhancement changes.

Never open an issue or pull request containing copyrighted game files, memory dumps, private logs, credentials, tokens, or links to unauthorized game downloads.

## License

Original MCLA-R code and documentation are available under the [MIT License](LICENSE). This license applies only to material owned by the MCLA-R contributors. It does not grant rights to game data, generated proprietary game code, Rockstar/Take-Two/Microsoft/Xbox material, ReXGlue, Xenia, XenonRecomp, or other third-party dependencies and trademarks.

MCLA-R is not affiliated with or endorsed by Rockstar Games, Take-Two Interactive, Microsoft, Xbox, ReXGlue, XenonRecomp, or Xenia.
