# Private Windows GPU launcher — 2026-10-02

The owner observed the B256 diagnostic using integrated AMD Radeon 860M and
requested a GPU/resolution/VSync launcher. The pinned SDK's D3D12 provider uses
raw `EnumAdapters1` indices; default `d3d12_adapter=-1` takes the first physical
adapter despite already exported vendor preference hints.

The ignored package `private/bundles/M8-001-Windows-B256-Uncapped-Scale2-20261002`
now includes `MCLA Launcher.cmd`, a Windows PowerShell 5.1 WinForms frontend.
It enumerates actual DXGI adapters with D3D12 feature-level 11 support, displays
names/indices/dedicated memory, and initially selects greatest dedicated memory.
This laptop: AMD 860M index 0; NVIDIA RTX 5060 Laptop index 1, 7,899 MiB dedicated.
Controls select stock/B256 diagnostic, GPU/automatic, 1x/2x render scale,
VSync and fullscreen. B256 remains labeled diagnostic and rejected for timing.

Actual pinned startup precedence is CLI, then environment, then TOML. The
upstream wiki's environment-last description does not match this fork.
`Set-MclaVideo.ps1` atomically replaces only `d3d12_adapter`, `resolution_scale`,
`vsync` and `fullscreen`, preserving first-original and previous-config backups.
The runner passes all five absolute game/user/cache/update/metadata roots on
CLI because those resolve before TOML. Obsolete `cache_path` is corrected to
`cache_root`. No registry mutation is used. GUI and legacy CMDs share a per-bundle
mutex held through the child lifetime and refuse an already running session;
stock/unlocked keep separate saves and caches. Legacy fixed 2x CMDs select this
laptop's index 1; use the GUI after a topology change.

Verification passed:

- Windows PowerShell 5.1 parser, actual control construction and DXGI enumeration;
  initial setter/control checks also passed PowerShell 7.
- Temporary space-containing config: both directions of all four setting changes,
  unrelated-value preservation, original-backup preservation, atomic replacement.
- A second real PowerShell process refused the held session mutex.
- Both exact EXEs initialized the actual xenos D3D12 provider via lifecycle probe,
  selected NVIDIA RTX 5060, skipped guest runtime, and exited 0. Final copies ran
  in a path with spaces with quoted absolute roots. No owner save was used.
- Final evidence: `private/evidence/M8-001/windows-launcher-gpu-20261002-160801-454a6d8b with spaces/result.json`
  and per-profile logs. Earlier probe also passed; its copied crash helper was
  absent and produced a diagnostic warning, addressed in the final copied set.
- Executable identities unchanged: unlocked
  `1F186BB07399717DDACF251D459A76CCD6CE1753F61D3B9E13BDE11BFC7651E9`, stock
  `2D7A949F61BD9E30A17D47ACA7888894CDF3BB1B5BB54B2ED9483D70A445BAF4`.

This proves real adapter selection and the launcher settings path. Gameplay,
timing, performance and save progression are not newly validated. Owner bug
hunting through the launcher is next; M8 remains open.
