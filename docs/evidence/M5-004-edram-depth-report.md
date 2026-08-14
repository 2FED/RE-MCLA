# M5-004 EDRAM/depth validation report

## Decision

M5-004 is accepted as `host-rtv-ordering-depth-bounded-s2`. No renderer behavior
patch was made because the canonical saved-gameplay route did not reproduce a
target ordering or depth-restore failure. The reached D3D12 host-RTV path is
physically covered and bounded; unexecuted ROV/interlock and trace-player
snapshot-restoration paths are not represented as fixed.

## Immutable evidence

- Accepted M5-003 run: `20260814-104624-fde51a30`.
- Accepted result SHA-256:
  `299392E59CB38AFB44256E773884856FF0C869A96D2045086A65396C1ED4EFCA`.
- M5-004 report run: `20260814-114751-430e5724`.
- M5-004 result SHA-256:
  `C5B4100E7040073FF2B5FC3D3AC5F234DE3B17126E569512AFA6CE63CC8A9B89`.
- ReXGlue SDK: `v0.9.0.18`, commit
  `923c92d1d1cb721cb704ac603fba263a01ba06aa`.

The final verifier re-hashes and re-parses the accepted M5-003 source game,
pinned save, runtime artifacts, build log, complete rotated logs, all captures,
contact sheet, and recorded owner review. Raw logs, captures, and paths remain
private.

## Reached GPU coverage

| Measurement | Accepted value |
|---|---:|
| Unique ownership-mode records | 47 |
| Ownership mode IDs | 0 through 7 |
| Ownership-transfer draws | 536,929 |
| Host-depth-store dispatches | 19,814 |
| Host-bound depth tuples | 17 |
| Depth resolve tuples | 3: 1x, 2x, and 4x |
| Depth-tested draws | 5,893,101 |
| Depth-writing draws | 5,773,063 |
| Stencil-enabled draws | 5,433,389 |
| Depth/stencil-state draws without host depth target | 1,599 |
| Successful staged resolves | 93,261 / 93,261 |

The gate requires native guest-to-host sample agreement for every recorded
ownership tuple and rejects any render-target creation, ownership overflow,
binding overflow, unknown resolve shader, failed resolve, device loss, fatal
marker, or snapshot-route activity. The 1,599 unbound-depth-state draws are a
small observed guest-state class, not silently converted into successful depth
bindings; they remain explicitly capped at 2,000 for this route.

The source audit confirms that D3D12 host-depth storage dispatches and marks the
EDRAM buffer before the later ownership-transfer barrier/draw phase. It also
confirms that the only external call into EDRAM snapshot restoration is owned by
the trace player.

## Scope boundary

This evidence supports the host-RTV route used by the accepted night free-roam
slice. The owner visual PASS applies to road, buildings, player vehicle, traffic,
night sky, shadows, particles, and HUD; it is not recast as a pixel-perfect depth
oracle.

The following remain unexercised and unclaimed:

- D3D12 ROV/pixel-shader-interlock rendering;
- trace-player EDRAM snapshot restoration;
- PWL gamma;
- true-direct host resolve;
- whole-frame Xenia equivalence and all weather/time/city conditions.

These exclusions are accepted S2 scope. A future canonical route that reaches
one of them must add direct telemetry/evidence or reproduce a concrete defect
before a behavior patch is justified.
