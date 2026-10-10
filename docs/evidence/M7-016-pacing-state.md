# M7-016: sampled presentation-pacing state

Two live snapshots of the retained host8E/runtimeAEE/count-only U16 build show
interval selector2, phase threshold8, rate60 and no registered target-override
callback. This confirms configuration at those two points; it does not measure
callback arrival phase or prove that pacing causes the approximately20FPS rate.

| Field | Title-before-window | Pause-before |
|---|---:|---:|
| Interval selector |2|2|
| Threshold H |8|8|
| Phase calculation rate |60|60|
| Override callback pointer |0|0|
| Encoded threshold word |84240000|84240000|

The recognized interval decoding is conditional on the zero-extra-argument path.
The twelve-field snapshots are nontransactional. Stable pointers and mappings do
not establish that all fields belonged to one atomic state or remained unchanged
between the two observations.

## Address proof and the rejected first probes

Import slot82000864 is the xboxkrnl variable import at ordinal1BE, VdGlobalDevice.
The retained and bundled XEX hash isC386F4001FA569E6AD4B982F441F67412F00B3F47C166134555CD4B59854A432;
its offline slot word is000101BE. AEE registers a four-byte physical system
allocation, not an ordinary zero-offset virtual allocation.

The original112-byte reader correctly rejected the intermediate pointer before
dereferencing it. A diagnostic-only message revision exposed already-read
P=FFCAE000 after four reads/28bytes, with no extra target read. Both rejected
runs were closed through owned cleanup, with no survivors and Mango restored.
They are preserved separately; neither provided valid pacing-field observations.

AEE LookupHeap and LookupHeapByType select the E heap at Memory+620.
Its uint32 host_address_offset member is at heap+2C, hence Memory+64C.
The value depends on Windows allocation granularity obtained through GetSystemInfo.
The corrected reader samples and rechecks that member and requires1000.
It admits only the observed P=FFCAE000 within[E0000000,FFD00000), then reads
BE32(virtual_membase+P+1000) and requires device40002080, matching the current
callback registration. It does not enable arbitrary physical-memory access.

The runtime singleton, Memory pointer and virtual base are derived from pinned
AEE exported getters and rechecked. Six exact code slices and four additional
export/MAP identities bind the physical translation, including the offset store.
The singleton occupies proven BSS; it is not incorrectly treated as file-backed
initialized data. PID/birth/UID, mapped modules, readable ranges and caller-owned
command/log provenance are checked throughout.

## Reader validation

Physical readerEA0F72FED7336D5AD3EA04F1441C2FC225484CAD582952389804ADB135A061E0;
proof23586803FE68C31B6B37415519F6C1755324F1A169842E6051EFC317E32757E3;
tests9E581141ED80031459FEA9463513D983847B6166540021711E3A3F512E0BBC2E.
34 offline mocked tests passed in3.153s, including exact address/read counts,
offset drift, wrong pointers, unreadable mappings and failure cleanup.
Independent source, binary-proof and parent-harness reviews passed.

Each successful observation reads120 target bytes in24 operations:
56 host bytes and64 guest bytes. Two observations total240 bytes/48 reads.
Existing CVar observations add13 bytes per phase separately. Disk artifact and
process-map metadata validation is separate from the target-memory budget.
No memory writes, injected calls, protection changes or pacing-setting changes
occurred in these probes.

## Completed run and evidence

ResultF6CCB5EF19EA24D871F9412CCEE34596005A9516444626EDD3C7962A0D05D62A:
owned PID210504/start185390790,177.904737s, normal exit0, no forced cleanup or
survivors. Stock Mango service/configuration restored. The read spans were
1.322ms and5.267ms; these are not whole-probe overhead measurements.

Observation hashes:
F041E54BD3814CB67418036C9ED9AD574F4C8920512023F2B095D011CD3B34D3 title;
B209C43BECDA150098B9039891D955DE45DD982D67F5355F87DB143EC91A4A1B pause.

Archive8E3857DA83A0B2813D020BB1D9DD3EB691D775032D67EBA34FDEA8414B21A078;
manifestAD03A892CBAB48339AA9BAEE94819688DB4A46DD15CE9365AE9EA881D10208C7.
Local private root pacing-state-snapshot-20260909/physical-return.
Independent audit passed94 payloads/46,887,732 bytes, both live receipts/log
prefixes,24 mapped-image records,2x13-byte CVar observations, complete four-file
profile, six frame hashes,94 counter deltas,420 CPU samples and focus.
The42 final source/proof files are bound by physical-stage-manifest72176453.
Files named parent-source-pins/parent-source-plan retain ancestry metadata;
they are not attestations of the final runner.

Diagnostic cadence was20.149995/18.999993 guest/s with zero protection failures
or interval lock misses. This run is explicitly ineligible as a speedup comparison.
Root inspected the two representative scene/pause captures without obvious
corruption. It is not physical gameplay/audio acceptance.

## Next controlled test

The original guest setter can produce either H8 or H100. An initialization path
passes zero to its flag setter, selecting H8. In the default non-override I2 path,
H100 disables the extra late-arrival deferral while future targets remain L+2.
That supports a hypothesis to test, not an established performance explanation.

A separate H8/H100/H8 experiment is being designed. It must retain identical
binaries/configuration, alter only the seven threshold bits, preserve other
DWORD bits, apply the same intervention to controls, and settle before measuring.
Independent resumption protection and exact normal-lifecycle evidence are
required if an owned temporary title is briefly quiesced. Later observations
must confirm assigned H with I2/rate60/override0; any observed reset invalidates
the run. The completed comparison is now recorded in
[M7-016-pacing-threshold-control.md](M7-016-pacing-threshold-control.md).
