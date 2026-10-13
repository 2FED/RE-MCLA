# Source publication timing audit

This M8-001 prerequisite binds a saved91-slot guest-source census to conservative
mailbox-publication intervals. It is an offline decoder/audit, not a native
capture permit, FPS measurement or proof of correct simulation. M8-001 and the
stock/60/stock comparison remain open. The shipping launcher is unchanged.

## Contract

The full native scout metadata and exact canonical BMP inventory validate before
any image is read. Decoding streams current/previous RGB instead of retaining
all91 images. Repeated source IDs must keep their publication metadata and RGB;
failed calls and missed/unattempted slots cannot contain stale pixels. The fixed
metadata includes the original40second readiness delay and91absolute100ms slots.
Invalid metadata, image format, inventory or duplicate binding rejects the report.

The source-side witness is
`[refresh_cpu_end_qpc, first acquire_after_qpc]`. The diagnostic SDK completes
producer metadata before the mailbox release-CAS; the consumer timestamps its
successful acquisition after copying those exact properties under the mailbox
lock. Those witnesses enclose publication, not creation of the depicted game
state, GPU completion, scanout or guest simulation time. The source patch used
for the private run has SHA256
9F0E73D8D03E55A116BF71F7BB7849FF6356858DFB994A7207031DF283AAF0CF.

All91 slots remain in order. Duplicate identities retain the first acquisition
upper bound; the elapsed time between the same publication and itself is exactly
zero. For distinct publications, interval subtraction preserves negative/zero
lower bounds as overlap uncertainty. No midpoint, interpolation or fitted time
replaces those observations. Source-ID jumps and uncollected scheduler slots are
counted separately; neither identifies simulation frames or dropped displays.

The result binds session token, presenter epoch/sequence, BMP/RGB hashes and the
caller-supplied PID/birth/build identity. Caller execution fields are validated
claims, not independent process attestation. Metadata is frozen before image
callbacks can mutate caller inputs. Every admission flag remains false; no
CycleEvidence, landmarks, periods, rates or FPS are emitted.

## Verification

The streaming decoder passes18 tests, including a measured decoder-only peak
below24MiB, full91-slot validation before pixel reads, duplicate corruption,
timestamp/schema errors, absent images, terminal suffixes and callback mutation.
The publication audit adds12 tests for duplicate windows/zero elapsed, null holes,
source jumps, signed overlap, equal ticks, binding hashes, exact integer bounds,
caller mutation and no-retry failures. Together30 tests require only Python's
standard library; the broader model run totals51 tests.

Run from scripts:

```text
python -E -S -B -m unittest test_mcla_game_source_scout_decode test_mcla_source_publication_windows
```

## Actual saved scout

The separate private native experiment collected91 different title-screen images
and source IDs1073..1300 at epoch1/QPF10000000. Primary and independent audits
verified82input pins before disarming, all91BMPs, seven loaded package modules,
six unchanged historical seed files and normal close ACK/exit0/Job0 without
forced cleanup. First/middle/last show the sunset-beach PRESS START screen.
This commit contains only the offline contracts, not the native implementation
or authorization to repeat the consumed experiment.

Accepted run: private/evidence/M8-001/game-source-scout-20261013-1.
Supervisor resultSHA256
099266970A0A65F32C9861F5F24B5AC3715DFE4E33D53745F26813598BB2FB45.
Native metadataSHA256
9631C2F48A9811CC327AB947927DAFE9390DDE342FFADADA628D30BD502BAB70.

One separate offline replay validates the saved image hashes and creates
private/evidence/M8-001/source-publication-audit-20261013-1/result.json,
117315bytes,SHA25639E89E3E79963A32D9A2A37935B7DA47BAED8801B3A8F31BFEC035E01B9D2E8D.
No game or native helper runs during this replay.

| Observation | Result |
| --- | --- |
| Collected scheduler slots | 91of91 |
| Publication witness widths | 12.3815–61.0148ms |
| Minimum selected-publication elapsed lower bound | 31.7773ms |
| Maximum selected-publication elapsed upper bound | 167.2839ms |
| Uncollected slots | 0 |
| Unobserved source IDs between sampled endpoints | 137 |

These are host mailbox observations, not animation timing. A known repeating
animation/phase and its relationship to simulation are still missing. The old
cycle extractor and its cadence gates are unchanged. No current stock/60 timing,
gameplay regression or milestone closure follows from this prerequisite.
