# M1-013 Xenia Canary baseline

Date: 2026-08-10

## Pin

- Official repository: `https://github.com/xenia-canary/xenia-canary`
- Release/tag: `7d8db5a`
- Immutable source commit: `7d8db5a2cd06373aefba29dc019db74e78d6fb7d`
- Published: 2026-08-10 07:20:31 UTC
- Windows asset: `xenia_canary_windows.7z`, 3,583,154 bytes
- GitHub-published and locally verified asset SHA-256: `9E716B163B74A59E21E4CBC8D0D7A978820CCE91D9890EBEC10C162AA5236268`
- Extracted executable: 16,542,208 bytes
- Extracted executable SHA-256: `C51D73364180D5F09B29BC348732A5B79D3959D5639321BDA58D490B4ABCF06A`
- Local install: ignored `private/tools/xenia-canary/`

## No-game startup smoke

The executable was launched hidden with null GPU/audio/input backends and explicit private storage, content, cache, and log roots. It remained running through the startup gate and wrote:

```text
Build: canary_experimental@7d8db5a2c on Aug 10 2026
```

The smoke process was then explicitly terminated by PID. No game was loaded, no data was written outside `private/tools/xenia-canary/smoke/`, and the private log was not tracked.

This build is the MCLA behavioral-baseline emulator until a later task explicitly reviews and changes the pin. “Latest” URLs are not acceptable substitutes because Canary releases move frequently.
