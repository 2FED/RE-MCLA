# M4-008 XMP state-correct fallback

Status: accepted on 2026-08-13.

## Decision

The supported autonomous title route uses XMP as a high-rate status query
surface, not as the source of the already verified autonomous frontend audio.
ReXGlue v0.9.0.15 therefore provides a state-correct metadata-only fallback:
unsupported title-play and output-capture operations fail explicitly and never
advertise a fictional `Playing` state; empty playlist navigation is guarded;
known metadata queries return bounded results.

This closes M4-008 as `xmp-metadata-only-fallback-pass`. It does not implement
an XMP decoder, prove user-selected music playback, identify individual audio
events, or extend the sustained XMA/XAudio/SDL proof from M4-007.

## Canonical evidence

- Private run: `20260813-182745-5b65003b`
- Result SHA-256: `578B0F7CA1E531A9F56E172A9625E37D549E69FDD23D7D5D77BBF0C33B85A1EB`
- SDK: ReXGlue `v0.9.0.15`, commit `6f2a0f36c153495711a5c66487064ed86f6bb614`
- Focused SDK tests: 4 cases / 20 assertions
- XMP calls / known status queries: 4,625 / 4,625
- Playback calls / state changes / unexpected calls / inconsistent calls: 0
- Runtime state: `Idle`, decoder absent, no playlists or active songs
- Capture SHA-256: `A5F64FA054444DD5CD7058850A64E1168A2EA98C0A5BE5EAC0ED2E861284C810`
- Lifecycle: exact-PID `WM_CLOSE`, exit 0 in 461 ms, no force cleanup or survivor
- Integrity: source-game tree and four runtime artifacts match before/after and were physically rehashed

The canonical runner performed a clean SDK install and clean host build. Its
runtime-log manifest, capture, cycle tree, three build/test logs, source-game
identity, and four runtime artifacts are hash-bound in the ignored private
aggregate.

## Gate

```powershell
scripts/test-xmp-route-smoke.ps1
scripts/run-xmp-route-smoke.ps1
scripts/verify-xmp-route-smoke.ps1 -ResultPath <private-result.json>
```

The fixture suite passes one positive, 16 fail-closed negatives, and 24 source
contract checks. Raw logs, build output, and the BMP remain private under
`private/evidence/M4-008/`.
