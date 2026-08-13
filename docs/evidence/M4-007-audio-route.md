# M4-007 frontend audio route

Status: **PASS for the autonomous title audio route**.

M4-007 adds an init-only, bounded, privacy-safe ReXGlue audit at XAudio client
registration, guest six-channel float submission, SDL device submission, and
decoded XMA PCM. It records only counters, finite/nonzero classifications,
normalized peaks, queue depth, and starvation totals—not audio samples,
buffers, guest addresses, device identity, or paths.

Accepted exact-release run `20260813-170202-44d2c7d8` clean-built the SDK and app, passed 5 focused cases / 16 assertions, completed the 300-second title soak, and exited through controlled `WM_CLOSE`:

- 62,533 guest submit frames, 60,594 nonzero;
- 62,526 SDL device frames, 60,587 nonzero;
- 319,325 decoded XMA frames, all nonzero;
- zero invalid frames, SDL submission failures, starvation fills, or drops;
- maximum queue depth 8 and a verified 1280x720 title capture.

The run is monolithic: its complete audio PASS summary precedes ordered `Window closing -> Execution complete -> hard exit`, exit code 0, with no force cleanup or surviving process. Earlier diagnostic split runs remain private but are not closure evidence.

This proves sustained finite nonzero PCM through XMA, guest XAudio submission,
and SDL device submission without runaway buffering or crash. It does not
identify individual UI events or a specific music track, claim human fidelity,
validate XMP policy, or cover gameplay audio, device switching, pause/resume,
or long-session stability. Those remain M4-008, M4-012, M5-009, and M6-007.

```powershell
scripts\test-audio-route-smoke.ps1
scripts\run-audio-route-smoke.ps1
scripts\verify-audio-route-smoke.ps1 -ResultPath <private-result.json>
```

Raw logs, captures, and aggregate JSON remain ignored under
`private/evidence/M4-007/`.
