# M5-009 audio event matrix

Decision: `six-class-audio-stream-presence-pass`

## Scope

This gate verifies the presence of six required audio classes during one
bounded saved-gameplay route: music, ambient sound, voice, engine, collision,
and UI. A default-off synthetic input route opens the pinned post-OOBE save,
enters gameplay, dismisses the known overlays, and creates fixed listening
windows for passive and input-driven events.

The listening criterion is presence, not isolation: unrelated sounds may be
audible in the same window. The result does not claim mix balance, exact asset
identity, spatial accuracy, absence of distortion below human perception,
exhaustive event coverage, XMP playback, or long-session stability.

## Instrumentation

ReXGlue v0.9.0.20 adds the default-off, InitOnly
`sdl_audio_event_audit`. It accepts only the ordered vocabulary
`music,ambient,voice,engine,collision,ui`. Each window records aggregate SDL
device-frame count, nonzero count, invalid-sample count, submission failures,
and normalized peak. It emits no PCM, device name, path, pointer, asset name,
or controller identity.

The app's default-off, InitOnly `mcla_audio_event_probe` provides these bounded
windows:

- eight seconds of title music;
- eight seconds of saved-world ambience;
- thirty seconds of passive voice/radio opportunity;
- eight seconds at full throttle for engine output;
- fifteen seconds at full throttle and steering for collision output;
- a four-second pause/navigation UI sequence.

The same process keeps the existing XMA/XAudio/SDL route audit active. The
event summary is accepted only before a healthy route summary and an externally
requested `WM_CLOSE`, execution-complete marker, hard-exit marker, and exit 0.

## Accepted physical result

Private result:
`private/evidence/M5-009/20260814-170657-f44949d7/result.json`

- ReXGlue: `v0.9.0.20`, commit
  `c4aa30c35386bb4d2ef051a59ea8e71bab667172`.
- Build: clean Windows AMD64 `RelWithDebInfo`.
- Result tree: 9 files, 12 directories, 5,583,791 bytes,
  SHA-256 `BA03D8C840442E7333F15076FE67808596BC52A0491A1FBCD8C6D4999D2AE3F7`.
- Runtime log set: one 20,347-byte log,
  SHA-256 `9E62042D35F3FE6D827A5364149BAD09F68C8BFE4FF3A175D6DB05A9EF3960BC`.
- Title capture: canonical 1280x720x32 BMP,
  SHA-256 `BD17B90E18CF68398201AE81F244133AA7033226A1C35FE40AEC4A7CAB8A9CA4`.

| Class | Device frames | Nonzero | Ratio | Peak ppm |
|---|---:|---:|---:|---:|
| Music | 1,500 | 1,500 | 1,000,000 | 3,635 |
| Ambient | 1,500 | 1,500 | 1,000,000 | 5,478 |
| Voice | 5,625 | 5,625 | 1,000,000 | 6,077 |
| Engine | 1,513 | 1,513 | 1,000,000 | 12,412 |
| Collision | 2,822 | 2,822 | 1,000,000 | 10,764 |
| UI | 891 | 891 | 1,000,000 | 9,243 |

Every class has zero invalid frames and zero device-submission failures. Across
the complete route, the existing telemetry records 351,604 XMA frames (351,595
nonzero), 34,572 guest submission frames (32,661 nonzero), and 34,564 SDL device
frames (32,653 nonzero). Maximum queue depth is eight, with zero starvation and
zero consecutive starvation.

After the machine-audited windows completed, the owner entered the exact
presence-only confirmation
`PASS MUSIC AMBIENT VOICE ENGINE COLLISION UI`. The runner records only the
boolean decision and six allowlisted category names. It does not publish raw
audio or device identity.

The source game remains the exact 15-file, 6,569,586,392-byte manifest before
and after. The pinned save/header and all four RelWithDebInfo runtime artifacts
also remain byte-identical. The game exits 0 through external window closure,
and no exact-path process survives.

## Automated checks

- `scripts/test-audio-event-smoke.ps1`: one physical diagnostic positive, 32
  fail-closed negatives, and 25 source-contract checks.
- Focused ReXGlue `[audio][route-audit],[audio][event-audit]`: eight cases and
  30 assertions pass.
- PowerShell AST parsing: all three M5-009 scripts pass.
- Current-source RelWithDebInfo configure/build/link succeeds against exact
  installed ReXGlue v0.9.0.20.
- The prior build-matrix fixture gate remains 1/1 positive and 6/6 negative;
  the M5-008 timing fixture remains one positive, 20 negatives, and 18 source
  checks under the current pin compatibility rule.

The verifier rejects missing, duplicate, reordered, unknown, or malformed event
markers; insufficient duration, nonzero ratio, or peak; invalid frames or
submission failures; unhealthy current route counters; queue/starvation drift;
missing lifecycle markers; noncanonical capture; log-rotation errors; reparse
traversal; process leakage; save/game/artifact drift; and persisted-result versus
physical-evidence disagreement.

## Limits

The voice window is a passive opportunity and the collision window is a bounded
full-throttle steering challenge; the owner's confirmation establishes that the
named class was audible, not which exact guest asset produced every sample.
Long-session transitions, pause/resume stability, device changes, volume/mix
controls, XMP decoding, and exhaustive race audio remain M6-007/M6-011 scope.
