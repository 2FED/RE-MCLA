# M2-001 stock Xenia gameplay baseline

Date: 2026-08-11
Result: PASS

## Reproducible launch

`scripts/run-xenia-baseline.ps1` validates the pinned Xenia Canary executable and supported `default.xex` by SHA-256 before launch. It creates a new isolated directory below ignored `private/`, refuses existing/outside/reparse-point destinations, and launches with these behavior-affecting settings:

```text
gpu=d3d12
apu=xaudio2
hid=any
apply_patches=false
apply_title_update=false
time_scalar=1
vsync=true
fullscreen=false
headless=false
window_size=1280x720
```

The source XEX and all storage/content/cache/log paths are explicit. No global Xenia profile, Documents directory, title update, patch file, or prior save was consumed.

The launcher passed PowerShell syntax parsing, a positive no-write `-WhatIf` preflight, and rejection tests for an outside-private destination, an existing baseline directory, and a wrong XEX fixture.

## Observed run

The verified Xenia Canary build `canary_experimental@7d8db5a2c` loaded the Complete Edition module and reported:

- module hash `1984A3354B78CE19`
- Title ID `545407F8`
- Media ID `5940C9DB`
- XEX version `0.0.0.8`
- entry point `0x821322B8`
- title `Midnight Club: LA`
- renderer `Direct3D 12 - RTV/DSV`
- audio `XAudio2`
- NVIDIA GeForce RTX 3090 host adapter

With a clean isolated content root, the operator created a profile/save, started a new game, and confirmed that gameplay controls responded. Private F12 guest-output captures then showed:

1. the opening vehicle sequence
2. the Carney's introduction cutscene
3. a moving street cutscene
4. active gameplay with HUD, minimap, race timer `00:59.72`, position `1/2`, and displayed speed `40 mph`

The gameplay-frame file is ignored at `private/tools/xenia-canary/artifacts/screenshots/545407F8/545407F8 - 2026-08-11T00-26-20.png`, SHA-256 `391C42BAAD919B6884AF34F3039B28126F3F88465EF14257855EC1F67B5EE345`. The image itself is not tracked or distributed.

At the recorded gate the process had run for more than five minutes, remained responsive, and the private log contained zero matches for fatal, assertion, crash, device-lost, or unhandled-exception markers.

## Known non-blocking observations

- Xenia detected the connected Xbox-compatible controller through SDL.
- The optional `gamecontrollerdb.txt` file was absent; the detected controller still initialized.
- Xenia reported several unimplemented force-feedback ordinals and other warnings. They did not block profile creation, cutscenes, rendering, HUD, or gameplay in this route.
- Warning/import classification is not claimed by M2-001; it is recorded by M2-002 and triaged under M2-010/M2-014.

This task proves stock execution and gameplay only. It does not claim save reload, complete controller mapping, all required visual states, or full-game compatibility; those remain M2-003/M2-004 and later milestones.
