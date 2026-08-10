# MCLA-R

MCLA-R is an experimental, unofficial native recompilation project for the Xbox 360 version of **Midnight Club: Los Angeles Complete Edition**.

The project aims to translate the original PowerPC executable ahead of time and provide the Xbox 360 runtime behavior it expects through ReXGlue plus game-specific compatibility code. It is not an emulator, a remaster, or a replacement game engine.

## Status

MCLA-R is in the repository-foundation and feasibility phase. There is currently no playable build and no public release.

The initial target is:

- Windows 10/11 x86-64
- Direct3D 12
- offline single-player
- original 30 FPS behavior until compatibility is established

Linux, higher frame rates, ultrawide support, and other enhancements are later goals and are not currently supported.

## Game data is not included

This repository does not contain the game, game assets, Xbox 360 executables, generated proprietary game code, title updates, DLC packages, or encryption/signing material.

Development and eventual user builds require a legally obtained user-supplied dump of the supported Xbox 360 Complete Edition. The planned tooling will validate the dump before extracting or using it and will not modify the source image.

Current research targets the following disc identity:

- Title ID: `545407F8`
- Media ID: `5940C9DB`
- ISO SHA-256: `AFCAAE68593246B1F83328681B690E254A13C37DD2E8DC34AB0DEB6C0F471FDB`

Support for any other region, revision, title update, or executable must be implemented and verified separately.

## Not a turnkey converter

Static recompilation does not automatically make an Xbox 360 game portable. MCLA-R requires game-specific work for PowerPC control flow, Xbox kernel and XAM behavior, graphics, audio, input, storage, timing, retired online-service paths, and progression compatibility.

The project uses ReXGlue as its intended recompilation/runtime base and uses Xenia Canary as a behavioral reference. Both upstream projects remain independent from MCLA-R.

## Development plan

The authoritative architecture, prerequisites, task ledger, milestone gates, acceptance criteria, risk register, and current decisions are maintained in [README-AI](README-AI).

Rules for handling game dumps, generated code, logs, traces, and release artifacts are defined in [docs/source-data-policy.md](docs/source-data-policy.md).

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

The project is not yet ready for general contributions. Until the feasibility milestone is complete, coordinate work before implementing broad runtime or enhancement changes.

Never open an issue or pull request containing copyrighted game files, memory dumps, private logs, credentials, tokens, or links to unauthorized game downloads.

## License

Original MCLA-R code and documentation are available under the [MIT License](LICENSE). This license applies only to material owned by the MCLA-R contributors. It does not grant rights to game data, generated proprietary game code, Rockstar/Take-Two/Microsoft/Xbox material, ReXGlue, Xenia, XenonRecomp, or other third-party dependencies and trademarks.

MCLA-R is not affiliated with or endorsed by Rockstar Games, Take-Two Interactive, Microsoft, Xbox, ReXGlue, XenonRecomp, or Xenia.
