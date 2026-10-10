# M7-016: neutral unpaused FMA diagnostic window

Status2026-09-06: source/build/native and Deck capture gates complete; current
hot-leaf attribution is in progress. M7 remains0/16; KI-027 remains open.

## Purpose and source scope

The [owner's hardware-FMA session](M7-016-fma-tu-candidate.md) feels clearly
better, but the five-arm paused comparison does not show a cadence benefit.
This diagnostic provides an autonomous unpaused interval to investigate that
difference while the owner is AFK. It does not automate driving.

Only `src/mcla_app.cpp` changes. The default-off, init-only
`mcla_readback_unpaused_window` flag requires the existing readback-cost probe.
After saved gameplay capture, it releases and acknowledges neutral probe
input, captures an endpoint frame, records20seconds of existing counters,
captures the other endpoint and atomically publishes `unpaused-samples.jsonl`.
The original pause/warmup/sample/close sequence then runs unchanged. Errors
return through the existing failed-probe path; stop-aware waiting is reused.

Physical SDL input remains enabled. Neutral probe input and nontrivial frames
alone do not establish an unpaused or stationary guest scene. Primary must
inspect both endpoints. Log markers surround extra capture/publication work;
they are not the exact counter timestamps. Independent cumulative counters
and overlapping timers do not establish exclusive subsystem budgets.

Reviewed sourceSHA256:
9A28942A0DBE45EC8F0D6DDFCFDEDE5F74323103CD95683A3622CE1AAEFF87FE.

## Build and static validation

New helpers/builds live separately under private `fma-unpaused-window-20260906`
and `win-amd64-fma-unpaused-{strict,hardware}-20260906` directories. The tested
previous B0C90E33... hardware and9C2663D2... strict artifacts stay frozen.

The controller pins2613files/694,252,457bytes, including historical source
snapshots/manifests/artifacts. Exactly one shared source differs from the
earlier experiment. All eight negative configure guards pass. All78 paired
CMake commands and72 actual host Ninja commands match their corresponding
historical arms after build-path normalization. TU0 retains strict FP,
contraction-off and explicit hardware/no-hardware FMA options; other guest
sources retain their previous flags. No global ISA/fast-math change.

Both77-step builds exit0 without timeout cleanup. Root ast-grep scan, all
three rule fixtures,133 serializer checks and28 parser tests pass. Both
generated-integration gates pass67files/133,916,451bytes and65objects.

| Property | Strict | Hardware |
|---|---|---|
| HostSHA256 | DA3D4EAC064C1CE70D8A88D61F50139F858B7D062F50D495B40CFEF4CECFB7AB | 44E82B46E46E601387E40D98330B7D93476C64AD799ACBF202580A6B6E597DF6 |
| Package manifestSHA256 | 9CD23B0FD9E3365FED39BB769B99A29B3ADDC3A7D8E64DEE6326648204C2D249 | 9D3F740B446FE2A5BA9C5A24409A738CA2FEF134542BCCA656A0E21D3769F5D9 |
| Known function imported FMA calls | 96 | 0 |
| Known function hardware FMA instructions | 0 | 96 |
| Reviewed narrow/widen pipelines | 96 | 96 |

All665 imports match. Independent review confirms all65 generated objects
match each arm's historical counterpart after zeroing only COFF timestamp
bytes4–7. Of72host objects, only app.cpp differs from the historical same arm;
between new strict and hardware, only TU0 differs. Each package copies the
same eight accompanying binaries from the prior accepted package.

## Native functional gates

The independently reviewed native helper checks exact source/full four-file
profile and binary pins, seven required loaded images, optional-if-loaded
Tracy, held title/helper identities, bounded cleanup and no forced PASS.
It reuses the pinned strict sample analyzer instead of duplicating its schema
checks. Native polling and module inspection are functional validation,
not an overhead-free performance benchmark.

Default OFF passes:153.4547845seconds harness elapsed, internal window-close
path/exit0, no external close/force/survivors/errors, full source/test profile
unchanged3BBC44D0.... Four original frames and one valid20.000003second sample
pair exist; no unpaused phase markers, sequence502 or artifacts exist.
Primary inspected gameplay and PAUSE frames: saved Eclipse/HUD/PAUSE render.

- Helper: 3CBAD8EC178094A10E2C0FCF18EC213A8ED50D8E44CDA4D22E7D29FF8609AC1B
- OFF result: 2CA3960059708D08D85D8830062FBB5715CCBDBB6507C8441A494C6AC0B2F6CC

Opt-in also passes:173.8040871seconds harness elapsed, both exact neutral
source/guest acknowledgements, six complete frames, valid20.000858second
unpaused and20.000525second paused pairs, internal close0 and no external
close/force/survivors/errors. Full source/test seed remains unchanged.
Primary inspected both unpaused endpoints and final PAUSE. The initial rear
camera/HUD changes to an automatic front idle camera with HUD hidden; the
Eclipse remains parked. This is not a fixed-camera/equal-workload interval.

Interrupted-window control passes:113.2581853seconds, explicit native window
close shortly after the BEGIN marker, exit0/no force/survivors/errors, full
source/test seed unchanged. It contains the before-frame and neutral
acknowledgements, but no END, after-frame, final sample pair or summary marker.
It is a cancellation-path PASS, not a completed measurement.

- ON result: A7DD4C091E5EDCAC154745FDFE2EBFE138E0FE7146FFB3D6D56B0B3B97DEDAE3
- Cancel result: 4D89129CF03BDE84D8E294036E180E6FF9B73AE4C2DCEB5E9E1BAC54B56B8F09

Independent return review reproduces all three controls,100-module snapshots
per run,27 binary copies and13 BMP hashes/headers.

The existing SDK close path uses `std::_Exit(0)`; these checks do not prove
graceful subsystem teardown. The exact reused perf parser also passes28
completed-receipt and3 negative raw-parser cases without filesystem writes.

## Completed Deck comparison and profile capture

Clean strict/hardware/strict counters are followed by one distinct hardware
profiling arm, with fresh isolated prefixes and the pinned Proton10/UCRT,
full historical seed/cache, Type0ON, fast readback and1280x720 Vulkan.

The profile records12seconds of99Hz userspace task-clock leaves across current
title threads, without inheritance, stacks or kernel samples. A ready snapshot
during startup precedes the unpaused marker; recording is scheduled inside
the observed phase, with exact PID/birth and mapped images plus before/after
thread ticks. Raw/text/header/child-lifecycle checks remain mandatory. Python,
perf and Windows steady-clock timestamps are separate domains; no exact
counter-window containment is claimed without a validated clock bridge.
Profiled rates must stay outside the clean throughput comparison.

The independently reviewed Deck runner passes24 in-memory phase/duration/
chronology/environment checks. It removes private-prefix variables from perf
children and seals failed attempts after cleanup, including attachment
uncertainty and bounded partial-artifact hashes.

- Runner: 38B34A4BCF0B0DD15CFDBC0C2537C5B5CD7C41B28C666E6EC9FB49B6C919F56C
- Host-only stage:10files/105,446,034bytes, including two reviewed EXEs and
  small manifests/helpers; the other eight binaries are copied locally on Deck.
- Stage inventory:18D1EBA1B1CAC33AC70325ABEA571FEE9778706D190ED77526B8B3A1FE513A38
- Preparation:96BC42500C5F4AA35741A61568B7ABD374C8764DDCB873E036292D1E5FA3325C

All four arms finish with exit0, no external force, survivors, errors or source
drift. All16 terminal profile files match the complete baseline. Sessions96254,
98460,53517 and76783 are closed. The full130-file/125,515,812-byte return is
verified against its archive/inventory; the exact-return analyzer succeeds.
Primary inspected all20 gameplay/unpaused/pause phase frames. The saved Eclipse
stays parked with rear camera/HUD0mph at the unpaused endpoints; PAUSE follows.
These Deck endpoints do not show the idle-camera transition seen natively.
Traffic, pedestrians, music labels and lighting vary; equal workload is not
established by endpoint similarity.

| Arm | Unpaused outputs/seconds | Unpaused outputs/s | Draws/output | Paused outputs/s |
|---|---:|---:|---:|---:|
| Strict | 360/20.000003 | 18.00 | 3288.32 | 14.85 |
| Hardware | 376/20.000004 | 18.80 | 3304.79 | 14.80 |
| Strict repeat | 367/20.000004 | 18.35 | 3331.12 | 14.60 |
| Hardware with profiler | 341/20.000007 | 17.05 | 3329.56 | 14.80 |

The clean hardware arm is2.45–4.44% above the strict arms; strict endpoints
drift1.94%. This is a modest bounded stationary signal, not stable30, moving
gameplay FPS, audio acceptance or a broadly repeatable quantified speedup.
The profiled arm is excluded from the clean throughput comparison; no profiler
overhead percentage can be isolated from this separate varying workload.

The profile contains2933 raw/text/header-consistent samples on19 sampled TIDs.
All sampled TIDs retain their birth identity in both snapshots; no added,
departed or reused TIDs are observed. Recording lasts12.100799731seconds;
the separate CPU snapshot bracket is12.366456655seconds. The two decoders exit0
under their retained Popen ownership. Recording is scheduled between observed
phase markers, with exact loaded-image identities. This remains phase-associated
evidence, not a validated mapping between perf/Python/Windows clock domains or
proof of exact containment within the counter snapshots.

- Comparison fingerprint: B4D7B2AC9BD23A24B2967DFB0DED364BDCAA81875C33237FFD58CA0CF407D377
- Archive: 212D2C6534AF3B385CC07E810A20F328C903D7C77B9FD819BDF8D7BCACA9A670
- Inventory: F7B68C0A5F5D1295FD9874DF7A0C3A36696BB4B0E859B9EFF16DD73865654789
- Analysis: B6A746DF382CC6322BECA6910F3A4A19E8052339F579F27090578AA92F18A4B0
- Analyzer: F6CDB2022B4CBE5295939723F19FF9F553351F883A2287766C3205BA57DB2204
- Collector: 1AD815C6FACBBD9B1997581009A761CD16451F4E4D37DB0949F8134CCECE9073

Independent final return review reproduces all130 files and all eight sample
pairs. Host/SDK/UCRT attribution is complete; see
[guest-scope overhead](M7-016-guest-scope-overhead.md) for the newly identified
exception-count lookup path. The full process maps change, while all sampled
executable IP mappings and the selected12 images remain stable. No additional
FMA translation unit or synchronization change has been selected.

No physical driving/audio acceptance, version bump, task closure, normal-bundle
promotion, commit or push is claimed here.
