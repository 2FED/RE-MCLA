# M7-016: private CPU draw-phase probe

Status: source, helper tests, linked-code review, native OFF/ON and Deck capture
are verified. This is diagnostic instrumentation,
not a performance fix. M7 remains0/16.

The [existing-counter capture](M7-016-existing-draw-counters.md) leaves material
CPU draw work unattributed. Private K is
`private/evidence/M7-016/draw-phase-probe-20260907`. It divides non-COPY IssueDraw
work into eight timed segments while preserving COPY as a separate cohort.

## Baseline and source preservation

The baseline Vulkan source is SDK HEAD, SHA256
`85961609B2DC88FA9FC30BC712DFA29601C24B5AB95DB8B50F00E90F9684504E`.
It excludes the previously rejected dirty gather experiment. A one-file VFS
overlay preserves the original virtual source/include identity. The private
baseline compile reuses51 other GPU objects and15 explicit libraries.

The rebuilt baseline is byte-identical to GPU18A3 after normalizing only the
four-byte COFF timestamp at0x80–0x83. This covers all seven sections, constants,
unwind and loader data, not just instructions. The private build does not change
SDK source, headers, original objects, install trees or the normal game bundle.

- Baseline DLL: `1D8972EEA107EC898E2F909B8444EB222F42F8ED1A149B2E61BE86DF08D6D988`
- Baseline verification: `82CE5C205EC0EFC8CEA190E62FCF8234D77EC2A3AD8784DC1FFC6C2C69661131`
- Phase source manifest: `AC06518D3DF056F0DEB5455AF54DA41CBCCBB92B82D5485F329674E2C383A6F9`
- Source restoration verification: `5FC92A22C000B7500A2C21E02F140FD385F6997627FF9A79AB201895986B4913`

The phase source adds17 marked blocks, wraps26 outer return expressions and
replaces one destructor definition. Primary reconstruction restores every
baseline byte after reversing those changes. Source line directives preserve
existing logging/assert locations. No class layout, vtable, graphics policy,
queue, wait or coherence change is introduced.

## Measurement and lifecycle

The default-off, init-only CVar is `vulkan_draw_phase_probe`. Its stages are
shader/setup; retry submission/primitive/translation; samplers; textures;
render targets/pipeline; state/constants/bindings; residency/barriers/enqueue;
and memexport. Revisited retry stages accumulate. The `raster_*` field names
mean all non-COPY calls, including early/no-op returns, rather than emitted draws.

A scope created before the original locals collects nanoseconds and explicit
return outcomes. Local cleanup precedes final timing. A nothrow-destructor
assertion covers ScratchBufferAcquisition; exception detection adds no
uncaught-exception query. Normal draw work uses local state and trivial TLS,
with no extra draw-time registry lookup, lock, atomic or log call.

A fixed eight-entry registry assigns object/context identities. The worker
publishes complete totals under a private mutex at primary-buffer boundaries.
Snapshot getters copy that published record under the mutex and log after
unlocking, with a16-attempt lifetime cap. Cold failures and invalidity remain
sticky. Unavailable data is explicit. Normal title exit can use `_Exit(0)`, so
required endpoints must precede exit; destructor publication is optional.

Independent source review accepts the intended worker-per-owner lifecycle.
Rebinding one thread to a different owner without shutdown, at depth zero,
does not immediately invalidate the previous entry; that unsupported misuse
is outside the inspected runtime path.

Getter records contain Win32 process/thread IDs. Under Proton these must not be
equated with the Linux IDs used by the existing ownership harness. Ordinary
snapshot counters identify the associated raw sample, while published phase
totals remain a separate boundary. Concurrent capture sequence and timestamp
order may differ. Publication age and instrumentation overhead remain explicit;
these counters are not an exclusive CPU/GPU frame budget or a clean FPS result.

## Build and fixture evidence

Controller review corrected a source-pin gap before compilation: the actual
five listed sources now must match the reviewed manifest, in addition to the
manifest hash. A reproduced old acceptance of mocked drift and one positive/
eight negative cases verify the correction. Original draft D32 is preserved.

Attempt1 compiled and ran the fixture, which reported46 checks/zero failures
and exit0. The Job wrapper nevertheless observed a nonzero active count
immediately after parent exit, forced cleanup and rejected the run. It remains
a failed attempt; no phase DLL was built from it.

The replacement wrapper allows at most one second of normal Job drain after
parent exit0 and still requires zero active processes. Suspended hidden creation,
assignment before resume, no breakaway and failure cleanup remain intact. Review
caught a missing import before this wrapper ran. Both drafts are preserved.

Attempt2 passes all five jobs normally: fixture compile/link/run, then private
Vulkan compile/link. The fixture passes46 checks, with exact success output and
empty stderr. All1,337 inputs/969,953,754 bytes remain unchanged. No forced
cleanup or surviving Job process is accepted. All four exports and ABI2 remain.

- Job wrapper: `7F78007C40FA7308161936353B8C68990460B6136C791D3F1C363EA5991544AF`
- Build controller: `926F3A0D474E704ED3CD5EABE46BB92E8A8298D67ADB64183615CF385897FC30`
- Build inputs: `40F9E71F692C08521A640D010A62273D9D2FBAAA7B61E0ACCEBC099E3269CFF8`
- Build result: `127153D1FC7775F1FC5218A52F6F2AF39649EC48A61922439A42239C6F7427CB`
- Phase GPU DLL: `6EDC64C173B70F186ED28946A40CB472102AFF4DE533BE32DCC4B5AE37D14787`
- Linker map: `094F02A72253EF2C57D83521C5EC9417E2F15023D7D16276042796C176AAC706`
- Package manifest: `864FCEF462DEB77A54B9CA6E429CA7FB9110AFB8CBC389316BA2B835BE7B6660`

The package changes only the GPU DLL relative to J. Its C74 host and the other
seven companions remain exact.

Independent linked-code review verifies the240-byte Vulkan vtable's30 relocation
identities and all51 reused objects. The360 previous imports retain their order;
the two additions are thread-system-ID and `_getpid`, resolved in retained
runtime/CRT evidence. Of749 old code-symbol records,719 compare identically
after relocation and verified lambda-number normalization. The remaining
changes are the seven instrumented methods, CVar initialization and their
exception handlers. No unrelated method change is found.

The312-byte POD TLS is zero-initialized with no dynamic TLS initializer or
destructor. Normal scope paths contain no diagnostic lock, atomic, logging or
uncaught-exception query. This does not mean zero overhead: IssueDraw's stack
grows208 bytes, and its object body grows from8336 to8912 bytes. OFF still
initializes188 bytes of local storage and executes checks/helper work. These
costs remain part of the runtime interpretation.

## Native controls

OFF63850/PID40664 and ON36289/PID42984 both close normally with exit0 after
173.9235/173.6501 seconds. There is no forced cleanup, surviving owned process,
source/binary drift or changed four-file profile. Seven loaded images and six
frames per run verify. OFF omits the flag and emits no phase records. ON enables
it explicitly and supplies five records, including four unambiguous sample joins.

Primary analysis reproduction matches the saved ON result. The four publication
ages are6.6911/10.3849ms unpaused and3.1972/3.9089ms paused, within the250ms gate.
Readiness, invalidity, counter algebra, identity and timestamp checks pass.

Root views five frames per run: rear view/HUD0mph before the unpaused window,
an idle camera change afterward, then rendered pause. These endpoint observations
do not establish equal work, a fixed camera or audio quality. OFF records595/600
outputs and ON597/600 over their approximately20-second unpaused/paused windows;
this is not a performance comparison or Deck FPS evidence.

- OFF result: `691C60D960F7B40E15D8FA63FFDFA8E427861940A1AC8258E89221071A56D72E`
- OFF verification: `BB0ABC9E61A99F034F2DBBE1E40A7A483A111BCB10A97A7DBA4ABC0C6CE35C11`
- ON result: `C2A9A905901B6E3E432E939F2ECAF7D52AC635ED9F109EA237C3D6114858B655`
- ON analysis: `A6E1176BB9AC298A2BB3E04A54D42C41B3A414310EE9DFE12A424112F7B51D3E`
- ON verification: `35298747E4C2BDAC94FAD395A89D096E5DFAB97A034B0D6CBDCB1BE7C2A0C255`

## Accepted Deck capture

The single instrumented run94098 closes normally after179.2254 seconds,
Linux PID154426/start168290786. The log's Win32 PID316 is kept separate. All39
returned files/31,277,479 bytes, twelve mapped images, full seed, source inputs
and process cleanup verify. Primary and independent return replays match the
saved phase analysis exactly. Four joins,94 ordinary deltas,64 phase deltas and
four publication ages are independently checked.

Root inspects the five phase frames: rear view/HUD0mph at both unpaused
endpoints, with traffic, minimap, pedestrians and lighting varying; pause shows
the same endpoint angle and visible car/menu. This is not a full-scene equality
or driving/audio test. The measured356/287 outputs over approximately20 seconds
give17.80/14.35 outputs/s with instrumentation; no comparison or gain is claimed.

| Non-COPY phase | Unpaused ms/raw output | Paused ms/raw output |
|---|---:|---:|
| P1 shader/setup |0.665|0.981|
| P2 submission/primitive/translation |9.245|9.516|
| P3 samplers |1.498|2.286|
| P4 textures |4.033|5.391|
| P5 render targets/pipeline |2.286|3.147|
| P6 state/constants/bindings |6.870|10.553|
| P7 residency/barriers/enqueue |9.302|9.780|
| P8 memexport/cleanup |1.338|1.969|

Separate COPY time is4.545/5.582ms per raw output. The phases partition measured
non-COPY invocation time, while COPY and other SDK timers remain separately
qualified. These ratios use ordinary outputs as context and do not erase the
publication-boundary difference. Ages are6.1177/7.4286ms unpaused and
18.8233/15.8793ms paused.

P1=P2=P3=P4 visits equal all non-COPY calls:1,177,007 unpaused and1,495,098 paused.
Every measured call reaches P4, which occurs at most once, and reaches P2 once;
therefore no sampler retry occurs in these intervals. P2/P7 dominate unpaused
non-COPY time; P6 is largest while paused.

- Fingerprint: `A7C6C3897F3B140E517D616C618DD0E1DAEBE7A4E7F2A1541D7D555AA23B54EA`
- Archive: `78CF000F03755A3887E6A7795FBC93375D77C70FB608B6B1A4932FEA2F9B2C54`
- Inventory: `084EA63E2927B73F96EB6A78AAB83406C8C210A0932DDD8CE16D495B32E9B517`
- Returned phase analysis: `0FB25BD5067D6DA0B675FDFB2D4BE1F6FD2CB6490D3CDCF24A626B0204BC7AC7`
- Return analyzer: `2AF66B47C2619058115923A21EB2D62F0D94606441FABCF6E8E3C74EFCD7B238`
- Complete analysis: `21F5300847BC01BC3F0A3FDB1385CA27F24E5F0DAB52E1EB6F2BA388C8DD3F6D`
- Verification: `D12DBBB582BE564EB038EA5131A20A6A899CC33C7A1AB7B44936E8A3F65F0B3E`

## Selected next experiment

P2 contains ordinary index-buffer SharedMemory::RequestRange calls, even when
CPU conversion is unnecessary; P7 requests vertex/memexport residency. The
4096 primitive-cache threshold affects separate conversion paths and is not
changed. Source and retained object review identify a common unnecessary
temporary vector allocation in RequestRange→RequestRanges for a single range,
including requests whose pages are already valid.

The next candidate will use a stack snapshot for exactly one range, preserving
zero-length/bounds rules, Ensure-before-validity ordering, fresh flag reads,
locking, uploads and multi-range sorting/merging. It must not retain a view into
caller storage. A controlled witness will compare actual old/new method bodies
and independent expected traces before any title experiment. The removed
allocation is a concrete bookkeeping opportunity, not a9ms recoverable budget
or predicted FPS gain.

No audio/performance gain, save progression, promotion, task closure, version
bump, commit or push is claimed.
