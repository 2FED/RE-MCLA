# M7-016 private portable campaign bundle

## Decision

`portable-windows-bundle-and-relocation-pass`

Version `0.9.0.0` clean-built the native Windows x64 title and launcher against
ReXGlue v0.10.0.2 commit
`492614eec92c31f11d75dd8fa0f09785cbae4a66`. The accepted private child is:

`mcla-0.9.0.0-win64-3DA6ECE6DA966595-E0FADD16DDCFFC54-58B3385C89D9E499`

Its immutable manifest SHA-256 is
`F3AF06067E8E057CCA62C40CB2BD9DBA47C3EC37425D2180BA6802D4C0A7139B`;
the 25-file hash lock SHA-256 is
`1A126222E2D1D15D7895B58DE56E30340AD0DE9BFF8164410EF2912BD21FADC6`.

## Accepted Windows proof

Private run `bundle-20260904-100935-4aae6764` verified:

- a clean Release build and atomic fingerprinted publication;
- 15 prepared game files totaling 6,569,586,392 bytes, with no source ISO;
- selected complete-profile save SHA-256
  `58B3385C89D9E4999B6791CCB95B6F7ED7963CE07F9C47618850841236784585`;
- exact SDK version and commit plus fullscreen-startup/Alt+Enter/LMB-double-click
  declarations in the immutable manifest;
- native `Launch-MCLA.exe --verify-only` from the final child;
- a cloned relocation path containing spaces with no repository/toolchain
  dependency;
- exit-zero relocated diagnostics launch, sibling log/diagnostic publication,
  and one atomic completed session with a content-addressed save snapshot;
- bounded retention of 32 sessions and 32 per-session save snapshots, plus
  current-child-only build retention.

The bundle and session fixtures pass one positive plus 13 and 10 fail-closed
negative cases respectively. The bundle source contract has 31 checks and its
representative private game path is ignored by Git.

## Physical gate

This proves the Windows portable boundary only. M7-016 remains open until this
exact child round-trips through Syncthing and is physically tested on Steam Deck
under one recorded Proton version for D3D12-to-Vulkan presentation, default
fullscreen and runtime window switching, Steam Input/SDL control, audio,
progressed-save load/evolution/reload, F10 diagnostics, external close, and
returned session/save/diagnostic verification.
