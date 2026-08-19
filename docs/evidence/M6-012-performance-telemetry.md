# M6-012 timestamped performance telemetry

## Decision

`PASS — TIMESTAMPED GUEST-FRAME PERFORMANCE TELEMETRY ESTABLISHED`

The accepted autonomous run is `20260819-192540-2a9e82fe`; its private
`result.json` SHA-256 is
`BFEA11FB1A7E8C5D380343C252612BC3BCBE471F0DC1BFB1FCA56B771FB27CB3`.
It used exact ReXGlue v0.9.0.28 commit
`6354bbe2150c7ce06bee5ffe399f17a94c948616`, a clean SDK install, focused
2-case / 7-assertion tests, and a clean RelWithDebInfo host build.

## Physical trace

One default-off InitOnly audit recorded exactly 300 monotonically ordered
samples, correlated to guest frames 1 through 300 and monotonic host
microseconds. The frozen summary balanced every sample aggregate and no marker
appeared after it. The application then exited 0 through external `WM_CLOSE`.

| Metric | Result |
|---|---:|
| CPU frame time p50 / p95 / max | 33,151 / 34,687 / 71,911 us |
| GPU frame time p50 / p95 / max | 708 / 1,457 / 3,118 us |
| Host streaming reads / bytes | 2,904 / 94,885,557 |
| Host reads at or above 5 ms | 0 |
| SDL silence-fill underruns | 4 |
| Shader translations / failures | 4 / 0 |
| PSO creations / failures | 2 / 0 |

GPU values come from D3D12 timestamp queries resolved after fence completion;
they are not inferred from CPU submission time. CPU values cover the host
command-thread work for the correlated guest frame. Streaming counters cover
host-file reads while the audit is active. Shader and PSO elapsed values are
accumulated in the same per-frame records, while the public result retains only
their bounded counts.

The one-file runtime manifest is 99,714 bytes with SHA-256
`ACE0741B16AC9B1DD31092D1468DCFDA6E809437B73B399AC82864F2EECE2724`.
The final verifier re-read that physical file, recomputed all percentiles and
summary balances, rehashed the executable and three build/test logs, and
rejected fatal, device-loss, malformed, duplicate, missing, reordered,
post-summary, overflow, and failed compilation evidence.

## Scope

This is a bounded boot/title-route sample, not a city-driving benchmark or a
performance target. It establishes a reusable timestamped telemetry boundary
for CPU, GPU, streaming, audio underruns, shader translation, and PSO creation.
It does not claim a fixed 30 FPS target, shader-cache completeness, long-session
stability, or that four silence fills caused the separately reported music cue
to stop. KI-017 remains open because the current counters do not identify
music-stream lifetime or mix state.

Raw logs, build transcripts, user/cache roots, and runtime binaries remain in
ignored private evidence. Public evidence contains only bounded counts,
timings, hashes, and semantic scope.
