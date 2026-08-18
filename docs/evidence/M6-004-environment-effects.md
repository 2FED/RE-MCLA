# M6-004 representative environment-effects matrix

## Decision

`representative-environment-effects-pass-open-s2-defects`

The supported Release route now has a deterministic, operator-free comparison
between saved dry-night free roam and an Arcade `RAINY` / `DAWN` race. The
canonical run physically captures the Arcade options, stationary rain, moving,
stopped, and particle-input states and exits through external `WM_CLOSE`.

This closes representative environment-effect presence and transition coverage.
It does **not** close the three known S2 rendering defects: saturated local-light
color on vehicle paint (KI-013), stippled/shimmering traffic signals and foliage
(KI-015), or intermittent minimap flicker (KI-016).

## Canonical route and evidence

The accepted private run is `20260818-192321-c0a2276b`. After one clean Release
build, the default-off InitOnly probe:

1. enters the completed saved free-roam route and captures a dry-night baseline;
2. opens Pause -> Modes -> Arcade;
3. selects Ordered Race -> Sunset and Vine;
4. changes Weather from Dynamic to `RAINY` and Time of Day from Dynamic to
   `DAWN`;
5. captures the option screen, stationary rain, movement, stopped motion, and a
   bounded brake/throttle particle input;
6. closes the console-style title externally.

The six 1280x720 captures have strictly increasing successful-present sequences
`2001, 3301, 3966, 4057, 4139, 4263`. The option capture visibly contains the
exact `WEATHER RAINY` and `TIME OF DAY DAWN` selections. The gameplay captures
show dense rain, wet-road reflections, changed ambient light, traffic/local
lights, moving and stopped camera states, and the race checkpoint particle
effect.

The causal input contract contains exactly 72 frontend records for 18
source/guest down/up pulses and eight render-input records for movement and the
particle challenge. The final summary is unique and precedes one ordered
`Window closing -> Execution complete -> hard-exit` lifecycle.

Sampled comparisons use 57,600 pixels per pair and a 30-channel-sum difference
floor. The accepted different-sample counts are:

| Comparison | Different samples | Mean channel delta (x1000) |
|---|---:|---:|
| dry night -> rainy dawn stationary | 45,458 | 35,029 |
| stationary -> moving | 34,314 | 24,043 |
| moving -> stopped | 31,540 | 24,000 |
| stopped -> particle input | 30,778 | 22,912 |

These metrics prove distinct physical output states; they are not semantic image
recognition or console-parity scores. The menu capture and deterministic input
route provide the condition-selection evidence.

## Inherited coverage

The result rebinds prior accepted evidence rather than relabeling the six new
frames as exhaustive coverage:

- M5-003 supplies the owner-reviewed night world, sky, shadows, particles,
  traffic, vehicle, road, buildings, and HUD categories plus successful Xenos
  draw/resolve activity.
- M6-001 supplies one continuous city route whose start and return captures show
  a physical sunset-to-night transition.
- M5-012 supplies the race transition/results frame used for representative
  motion/post-processing presence.
- M5-013 supplies the five-race resource result and the original owner report of
  intermittent minimap flicker.
- Three fixed-camera traffic-light images preserve the exact red/green/off
  KI-013/KI-015 repro without publishing them.

The final verifier rehashes those prerequisites, the current 15-file source
game, completed-save seed, four Release artifacts, clean-build log, complete
runtime-log manifest, all six captures, the cycle tree, exact SDK tag/commit,
and absence of a surviving canonical process.

## Open defects and exclusions

- **KI-013 remains open S2.** Rain/wet-road reflections prove that reflections
  are present, not that the colored vehicle-light transfer is correct. No ROV
  comparison or material fix is claimed.
- **KI-015 remains open S2.** The environment matrix does not localize the
  stippled alpha/MSAA behavior or prove temporal stability.
- **KI-016 remains open intermittent S2.** No high-cadence HUD observation was
  performed, so absence of a flicker in six frames is not evidence of a fix.
- Only dry night and rainy dawn are selected directly. Every weather/time pair,
  ROV/interlock, PWL gamma, true-direct resolves, complete motion-blur quality,
  indefinite HUD stability, and whole-frame console equivalence remain
  unclaimed.

## Reproduction

```powershell
scripts/test-environment-effects-smoke.ps1
scripts/run-environment-effects-smoke.ps1
scripts/verify-environment-effects-smoke.ps1 `
  -ResultPath <private-result.json>
```

The fixture suite passes one physical positive, one synthetic positive, 25
fail-closed negatives, and 22 source-contract checks.
