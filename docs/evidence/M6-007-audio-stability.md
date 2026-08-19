# M6-007 long-session audio stability

Decision: `two-hour-audio-and-device-recovery-pass`

The canonical split physical run is `20260819-122150-2ad3b961`; its private
`result.json` SHA-256 is
`CE4B4700CEA1108243989165211A8C70AC0C67F72130708A129C1BD834948B5B`.

## Evidence

- Exact ReXGlue `v0.9.0.25` commit
  `f28ddabbae3bca56ddf5ffea067982c49c9549b7` clean-built and installed. The
  focused audio suite passed 10 cases / 42 assertions.
- One controlled title-route process ran for exactly 7,200 seconds. Its audio
  audit observed 1,356,394 submitted frames, 1,356,386 device frames, and
  15,057,615 XMA frames. Of those, 1,345,373 submitted, 1,345,365 device, and
  15,057,610 XMA frames were nonzero.
- Maximum queued audio depth was eight. Invalid input, device submission
  failures, starvation fills, and consecutive starvation fills were all zero.
- Baseline plus twelve ten-minute process samples bounded peak/final growth at
  983,359,488 private bytes, 382,537,728 working-set bytes, 106 handles, and
  zero additional threads. These remain below the declared 1-GiB, 512-MiB,
  128-handle, and 16-thread limits. The private-memory bound includes measured
  animated-title shader/pipeline warm-up rather than claiming zero growth.
- A separate same-artifact process completed two two-second pause/resume cycles,
  recovered nonzero output after each, observed a privacy-safe Windows default
  playback-endpoint migration, recovered nonzero output on the new endpoint,
  and received owner confirmation that audio was audible.
- Both processes exited 0 through exact external `WM_CLOSE`. The source-game
  manifest, four runtime artifacts, focused build/test logs, resource samples,
  rotated logs, prior M4-007/M4-008/M5-009/M5-013 results, and complete private
  evidence tree are hash-bound and revalidated.
- The accepted endpoint-switch process continued to report nonzero XMA and SDL
  output after migration, with no starvation or invalid sample. The owner's
  observation that the short title music later stopped while ambient/SFX
  remained is therefore not classified as an audio-route loss.

## Scope

The two-hour soak and endpoint-switch checks are separate physical processes;
the result does not claim one monolithic two-hour device-change session. The
audit proves sustained title-route XMA/XAudio/SDL output, pause/resume recovery,
default-device recovery, bounded resources, and clean lifecycle behavior. It
does not distinguish music from ambient/SFX inside the final mix, prove exact
track duration or console mix fidelity, implement XMP decoding or user-selected
system music, identify an endpoint, or cover every gameplay stream transition.
Those limitations remain explicit despite the PASS.
