# M9-003 launcher frame-rate option (2026-10-28)

**Status:** implemented on top of the M9-002 launcher, with tests passing.
Staged as `private/l36`. Pending: the owner check with both modes (M9-002
smoke can run on the same folder). Carries M8-006.

## Behaviour

- The Play page has a **Frame rate** dropdown: "30 FPS (recommended)" (default)
  or "Unlocked FPS (experimental)". The limits text sits next to it:
  - stock: "Stock 30 FPS, the original pacing. Recommended.";
  - unlocked: "Experimental. Menus over the city can run fast, heavy
    intersections drop to 30-40 FPS, some lights flicker at night. Start Game
    checks build support."
- Changing it invalidates a previous CHECK SETUP result, like any other setting.
- **30 FPS emits exactly the stock command line.** No vblank argument, and the
  capability record and session receipt are byte-identical to policy V1.
  The controller fixture fails if a stock launch carries `mcla_guest_vblank_hz`.
- **Unlocked** adds only `--mcla_guest_vblank_hz=240`. The SDK's own 45 s
  start delay, draw-count UI cap and tear-free present then apply, as in
  l33/l34-sunc. The session receipt says `timing unlocked`. The capability
  record says `timing unlocked` and adds one key, `guest_vblank_hz 240`.
- **Gated per build:** unlocked is admitted only for the exact tested build
  fingerprints in `kUnlockedTestedBuilds` (now the l34/l35/l36 build,
  `0789DBB5...`). Any other build is Blocked with "Unlocked FPS is not verified
  for this build; choose 30 FPS", never silently started as stock. Unlocked
  does not widen 1080p/2160p admission.
- **Preferences:** unlocked is stored as format V3 (the V2 layout plus a timing
  code). Stock settings still encode as V1/V2 byte for byte, so switching back
  to 30 FPS restores the exact stock file. A launcher without the mode refuses
  a V3 file instead of misreading it.
- **Diagnostic export:** an unlocked session exports, and the report records
  `timing unlocked`. Before, the export rejected anything but stock.
- One build serves both modes. The M9-001 CMake option
  `MCLA_UNLOCKED_VBLANK_HZ` is removed; its code was compiled out in the stock
  build. `l34-sunc` stays as a historical artifact.

## Tests

| Suite | Before | Now | Failures |
|---|---|---|---|
| Capabilities | 168 | 200 | 0 |
| Preferences | 1064 | 2088 | 0 |
| Native UI | 1065 | 1087 | 0 |
| Launcher controller | 335 | 338 | 0 |
| Profile policy | 117 | 118 | 0 |
| Diagnostic export policy | 505 | 515 | 0 |
| All other launcher, export and lease suites | unchanged | unchanged | 0 |

New coverage:

- Admission with the tested build, refusal for other builds and one-nibble
  mutations, the exact unlocked record, and the stock record on the same
  build after switching back.
- Preferences: V3 round trip, every truncation, invalid timing codes, and a
  V3 mode byte on V2 bytes.
- UI dropdown: mapping, readiness invalidation, the limits text, Save and Check
  marshalling, and a missing selection sent as invalid.
- Export of an unlocked session.

The argument rule is a pure function (`TimingLaunchArguments`) that the
controller appends; the capabilities test checks it for both modes. (A first
staging, `06053DAE...`, was discarded before use: a review found the scripted
edit had not inserted the argument, which the then-missing positive test would
have caught.) Not covered by a unit test: the full controller launch with a
real tested build, because the fixtures use synthetic build identities. The
owner check covers it, and the host log must show "guest vblank: 240 Hz" after
the start delay.

No compiler warnings; ast-grep scan and rule tests pass.

## Staging

`private/l36`: the l34-s30 build (stock `mcla.exe` `A50E30C6...`, ticket
unchanged, build fingerprint `0789DBB5...`) with launcher `D8E4C25D...`. Staging tests 63/0/0.

## Spark routing

Not used: a small, test-driven code change, not a bounded extraction or
triage task.
