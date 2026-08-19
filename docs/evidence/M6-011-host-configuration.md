# M6-011 host configuration

M6-011 closes the documented host-configuration surface with a build-only,
fail-closed contract gate. The accepted private result is
`private/evidence/M6-011/20260819-184541-fa4b1cc6/result.json`, SHA-256
`F9977148797A625F15B20AC53807DC5B0CCD748A7C19205611FEBD71D4337C93`.
It uses exact ReXGlue v0.9.0.27 commit
`8cc7c272035a3bb4124d15060694943f848d9ee1`.

## Configuration surface

The shipped `config/mcla.toml.example` contains exactly 25 documented keys:

- guest resolution, video mode, refresh rate, host window size, fullscreen,
  and monitor selection;
- default or exact-name SDL playback-device selection, bounded host output
  volume, and mute;
- SDL input backend and mouse/keyboard mode;
- log level, destination, verbosity, noisy tracing, flush interval, rotation
  size, and retained-file count;
- game, user, update, cache, and metadata roots.

The build copies this file byte-for-byte beside `mcla.exe` as
`mcla.toml.example`. It never creates or overwrites a live `mcla.toml`, so
private canonical test roots and deliberate command-line launches remain
unchanged unless a user explicitly copies and edits the example. F4 remains
the runtime settings editor supplied by ReXGlue.

ReXGlue resolves `audio_device = "default"` through the operating-system
default endpoint. Any other value must exactly match one enumerated SDL
playback-device name; an unavailable request fails initialization instead of
silently selecting another output. `audio_volume` accepts only 0.0 through
1.0 and scales converted host PCM without modifying guest audio buffers.

## Verification

```powershell
scripts/test-host-config-smoke.ps1
scripts/run-host-config-smoke.ps1
scripts/verify-host-config-smoke.ps1 `
  -ResultPath <private-result.json>
```

The accepted run clean-built and installed ReXGlue, passed 3 focused cases /
27 assertions, then clean-built the RelWithDebInfo MCLA host. The final
verifier rebound the SDK/install, focused-test, and host-build logs, required
the source and staged examples to share SHA-256
`FCDEFE2702592DF54680AE76F2B30A35FC395488F4EFD784AB5AC20C29DD1EA7`,
and accepted only the exact four-file private result topology.

Fixture coverage passes one positive, twelve fail-closed negatives, and
sixteen source-contract checks. Negatives cover invalid volume type/range,
unavailable device and invalid backend/window/log/resolution defaults, plus
missing, duplicate, unknown, malformed, and private-path template keys. The
live build-matrix regression also passes one positive and six negatives.

## Scope

This evidence validates defaults, ranges, bad input, exact device-selection
policy, build staging, and the documented host surface. It does not claim a
physical alternate playback endpoint, perceived loudness, continuous music,
persisted controller identity, a launcher UI, or that every key can change
without restart. The user's real `mcla.toml` and private device/path identities
are neither captured nor published.
