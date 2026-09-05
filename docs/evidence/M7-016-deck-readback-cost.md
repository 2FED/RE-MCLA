# M7-016: isolated Deck readback-cost control

2026-09-05. KI-027 remains open. This is an A-B-A diagnostic, not a shipping
readback policy or a performance PASS.

## Identity and isolation

The owner's session `20260905T175147Z-296` closed normally, exit 0, with one
snapshot and no watcher errors. The complete return was copied over paired
Devkit SSH and verified before/after against 121 files / 67,307,014 bytes.
The four-file profile/album is unchanged at tree SHA-256
`3BBC44D0E0CBE03633DFA3BFFBC776B0C5E62CFDC5AA34BEFB346DE022322B5A`.
It is retained in `private/save-archive/M7-009`; full mutable archive identity:
`B673A45B17051658213F9055B67A894CF3E43E53C9DE8070335A2225CEC80951`.
None of the experimental profiles is owner progress or a publication seed.

The opt-in host has SHA-256
`E1233390F6B12E362FFD4CA7B17E28FB9396103264AA5C7D618AD1368C1AF96C`
(0.16.0.2 source plus the then-uncommitted diagnostic). The unchanged SDK is
0.18.0.1 / `9098e122afa116f4316d6cec47e561ebc5186974`; runtime/GPU DLLs match
the published 0.16.0.1 bundle. This host is not the published Deck binary.

All successful arms use the same official app-local C++ runtime 14.44.35211.0,
Proton 9.0 installation, native Vulkan, Desktop mode, 1280x720 window, fixed
owner profile and fresh independent copies of the same host shader cache.
The first A-B-A arms used a per-process native C++ DLL preference; no prefix
registry, system files, clocks, affinity or Steam shortcut was changed.
An ensuing minimal-dependency/no-override control is recorded separately in
[the C++ runtime report](M7-016-proton-cpp-runtime.md).

The default-off, init-only `mcla_readback_cost_probe` uses the existing synthetic
controller driver to enter the saved game and pause. It captures title,
gameplay, pause-before and pause-after via renderer readback. After 20 seconds
of pause warmup it samples 23 address-free cumulative counters over 20 seconds;
capture and disk I/O stay outside the interval. No physical audio counter is
available, and independent atomic snapshots are not a transaction.

Samples are published through `samples.jsonl.partial` -> `samples.jsonl` only
after both valid records are written and closed. A deferred UI close is
canceled by a lifetime token before teardown. An external exact-process
watchdog bounds graphics calls that can outlive the internal retry deadline.
CAPTURED means complete telemetry, **not semantic gameplay or visual parity**.
The primary inspected renderer captures: same white Eclipse, spawn location,
night scene and actual pause menu. Traffic and pause camera are not bitwise
identical; no missing-world parity claim is made.

## Results

| Arm | New outputs / 20 seconds | Outputs/s | CPU readback bytes | Nested copy time |
|---|---:|---:|---:|---:|
| A: fast + app-local CRT | 243 | 12.150 | 9,001,746,432 | 1.157 s |
| B: some + same CRT | 290 | 14.500 | 0 | 0 |
| A repeat: fast + same CRT | 285 | 14.250 | 10,584,457,216 | 1.341 s |

All successful runs exit 0 normally in about 154 seconds, without forced
cleanup. Published binaries, original profile and original host cache remain
byte-identical. A Windows mechanism control reaches 30 outputs/s; this is not
Deck performance evidence.

The first A-to-B comparison superficially suggests about 19% improvement.
**A repeat recovers almost all of it without changing readback policy.**
Therefore a robust material cadence improvement is not established; cache,
thermal or workload drift can dominate this small sample. The host cache was
isolated, but driver-global caches and detailed thermals were not controlled.

The repeated A measures about 4.70 ms CPU-copy time per output and 37.1 MB
copied per output. B retains roughly 65 GPU resolves/output while eliminating
warm CPU copies. Draws/output differ by less than 0.5% between B and repeated
A, but workload resemblance is not exclusive causal attribution. Neither
approaches stock 30 outputs/s, and sound/moving simulation were not accepted.

The `some` mode retains resolve allocations, GPU transfers and barriers but
may leave CPU guest data stale indefinitely. Never ship it as a performance
fix or use its saves/Photo Mode output as acceptance evidence. In particular,
the copy goes through raw `TranslatePhysical`, bypassing virtual-alias write
watches. Removing it does not directly remove those write-fault handlers;
changed stale guest content can affect downstream behavior indirectly.

Timings for primary buffers, draws, bindings, watch arming/disarming and fault
handlers nest/overlap. They cannot be summed into an exclusive CPU budget.
Zero resolve waits is not zero copies. The remaining command/protection path
is the next bounded investigation; safe disarm-range fragmentation should be
counted before considering coalescing. No global coherency bypass, delayed
watch installation or affinity-only change is justified.

## Evidence and regression tooling

Private arm folders: `readback-cost-deck-fast-native-crt-20260905`,
`readback-cost-deck-some-native-crt-20260905`, and
`readback-cost-deck-fast-native-crt-repeat-20260905` under
`private/evidence/M7-016`. The failed first builtin-CRT run is retained under
`readback-cost-deck-fast-failed-20260905`; it timed out before START and is
excluded, not substituted with a title-screen measurement.

The complete original experiment was archived as 115 files / 218,804,182 bytes;
compressed archive SHA-256
`27BC295E72E33E35BCF4362E363BE31719C6B598CCC1AF75618CCF36AAD5CDEC`.
Every member hash and directory was independently verified locally without
extracting or executing archive contents. The remote experiment was then
reversibly relocated outside the playable bundle. Final location is outside
the entire Syncthing root (receipt `experiment-final-relocation-20260905.json`
under private evidence); 276 entries remained identical, with no deletion.
This prevents unfinished diagnostic directories from blocking normal launch
and keeps experimental children out of the publisher's strict root inventory.

Reusable source: `src/mcla_readback_cost_sample.h`,
`tests/mcla_readback_cost_sample_test.cpp`, `scripts/analyze-readback-cost.py`,
and `tests/test_readback_cost_analysis.py`. The C++ serializer passes 127
checks; the Python analyzer passes 24 tests, including bounded input, duplicate
keys, integer overflow, counter regression, duration, mode and backend checks.
The analyzer explicitly leaves artifact/scene equality unverified and never
emits a performance PASS. The primary reviewed code and accepted independent
parser/lifetime review; AST scan and three rule fixtures pass.

M7 remains 0/16. The original physical performance/audio limitation and
evolved successor-profile reload are still pending; no new long owner test
or another unchanged F10 set is required for this investigation.
