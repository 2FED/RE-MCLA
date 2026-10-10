# M7-016: single-range stack storage

Status: original GPU reproduction, native and exact-Proton controlled witnesses
and optimized GPU binary review PASS. Native and clean Deck A/B/A controls
pass. The Deck series shows one tiny positive unpaused signal and no paused
gain. M7 remains 0/16; stable 30, repeatable speedup and promotion are unproven.
Work stopped at the owner's 20% weekly-remaining limit.

## Reason and scope

[Eight-phase attribution](M7-016-draw-phase-probe.md) identifies substantial P2
and P7 work. Both contain SharedMemory::RequestRange calls, which delegate one
pair to RequestRanges. The retained original object confirms a temporary vector
allocation even when the pages are already valid. Removing this allocation is
a testable opportunity, not a claim that the measured phase time is recoverable.

Private experiment `private/evidence/M7-016/single-range-stack-20260907` (L)
changes only RequestRanges and adds `<span>`. Count one copies both caller
scalars into a stack pair. Multiple ranges retain the original owned vector,
validation, sorting and merging. Zero length still returns before bounds
validation. A common span is created only after validation/merging; it never
views caller storage or the persistent upload vector.

Ensure precedes the fresh acquire validity load. The lock, second fresh relaxed
load, upload-vector clearing, unlock-before-upload and exception ordering remain
unchanged. The profiling scope spans the same work. The intentionally removed
single temporary-allocation failure is the sole expected semantic difference.
No memory rights, watch/coherence, cache threshold, retry or timing patch is added.

## Source and reproduction

Root and independent reviews pass. Replacing the modified function and removing
the include insertion reconstructs the complete original source exactly. The two
witness includes are exact method slices with only the class qualifier changed.
The shared tail differs by five view-identifier replacements. Both original
bit-63 guards remain present; a preliminary shift-by-64 concern was retracted.

| Artifact | SHA-256 |
|---|---|
| Original SharedMemory source | `01085C1E24C747E6D3CD98FA11045D2B97DE6AF4C19132DB3B204919B4CCED99` |
| Stack candidate source | `8189DD317197BAF46A1F9ADB5B719B05229356697012AC92C3C5B18716C692E0` |
| Eight-source manifest | `FA5648C6DFF1E0A44A0878F09C950FB4BCA6A723EDEB7682B185136DF92688EF` |
| Build controller | `9AC4D6F0FAB71177AB0496B4726BF546EE474714BAF1D4B44F5C77A1A3B3CAC5` |
| Baseline build result | `F4417093B18D8CE28F0366083C4CCF2C35D87F09A4C02E22A153FF1B1F3020C2` |
| Reproduced GPU | `68AF3C4AE412B8744CE4708147049B96B943B1E395AB695BCE4AE3873F8333FC` |

The first preparation failed before compilation because a legitimate cached
SharedMemory dependency was outside the old Vulkan dependency set. That attempt
is preserved. The reviewed controller now pins all 346 actual cached dependencies
before preprocessing and verifies them afterward, including failure cleanup.
It binds the prepared receipt to the input manifest before execution.

Baseline attempt 2 freezes 1,343 files (manifest `04BF73351B546D389F50F602F85FD726E62199E30CA52912717CD12C42A2D4DC`).
Compile PID43732 and link PID43552 close normally with zero survivors. The whole
GPU equals accepted 18A except COFF timestamp bytes 0x80–0x83. The link retains
the original 52 objects/15 libraries, substitutes the rebuilt SharedMemory and
K's clean baseline Vulkan object, and preserves the other 50 objects. This
excludes both the dirty gather source and intrusive K phase instrumentation.
The baseline retains three existing unused-parameter/field compiler warnings.
Independent acceptance hashes 1,390 unique current files across both input
freezes, artifacts, snapshots and predecessor links. Actual dependency sets
match preflight (346 baseline, 240 witness); timestamp-only equality is independently
replayed. Witness compilation has no warnings.

## Native controlled witness

The witness compiles exact old/new bodies against controlled facades. Fixed
trace storage, real atomic flag-pointer loads, three validity banks, 4096-byte
pages and the original 2^29-byte buffer bounds make ordering and range effects
observable. An independent interval/page oracle checks return/exception,
Ensure/lock/load/profile/upload traces, output ranges and mutation/alias cases.
Ordinary-alignment allocation counting excludes setup and trace storage.

All 40 ordinary cases and two allocation-failure cases pass: **206 checks,
zero failures**, exactly 44 output lines. Single requests avoid the temporary
allocation; multiple requests preserve it and its failure behavior. Native
compile/link/run PIDs28404/18620/3732 all exit normally, with unchanged inputs,
no forced cleanup and zero surviving owned processes.

| Artifact | SHA-256 |
|---|---|
| Witness source | `4C18142177841948A7340E778E8D1DF4426DCE4B855F29A0A6DBBF2178D3802C` |
| Witness PE | `D39708B7EDFF3EDC6A676480AA1F76189246C177743793CE604EF811AC540E3A` |
| Witness frozen inputs | `DF9BA973DBB90DEF9471BE193F319A62F4DA7DA75D82DBC4DFD6474D9035B12B` |
| Witness output | `1A2B08D502DC70692F5F3683FF5858C132FE271849B4B9780C90F4C6B48CCA34` |
| Witness result | `0BC7F10A4499467ED70D38A7F928878ADA45669615B3B86509B96A0C12ECC687` |

This proves controlled body behavior, not real GPU residency, concurrency,
watch coherence, driver behavior, gameplay, audio or FPS. The source preparation
plan remains frozen as historical input; its original pending status is not the
current execution status.

## Separate runtime envelope

The separately frozen wrapper includes the unchanged core under a non-inline
renamed entry. Native preparation and execution close normally. The runtime
input manifest binds 1,362 files; compile PID43716, link PID7816 and run PID25860
exit zero without forced cleanup or survivors. Exactly 51 rows contain six
module reports, the flushed two-second hold marker and all 44 unchanged core
rows. Module paths and file hashes match the private PE/CRT files and native
UCRT. These native module records are self-reports, not an external live-map
snapshot; the Proton map gate remains separate.

- Runtime source manifest: `FADC107A40AAA3DF1FF1E70EB8019D6C71A4DE550C7AFA56EE137DE143EAD91B`
- Runtime inputs: `2DA256449D1EDE0FCC1CEFD903F539884E69C49902E47FBD1BDD0456325EEF27`
- Wrapper object: `D44EF8788331706B22E811A1EC0B2A93C02BBB92E229F045E144111D6AD77BC7`
- Wrapper PE: `6410B12460D9A42BE7E441F66F0D2DDA459412AC17E625C56D5597ECC9A206B8`
- Native runtime result: `33F8704A33A39A2A379FA0FBCBDD66F8B35B3B69F91AB84522FDE2E3BF82CEDC`
- Normalized core output: `5702CDD8A2A9985255961A54F8E80EFA8B996C933B83E94D03ED8D7B122827BE`

The normalized LF output hash differs from the original raw CRLF file hash;
the rows compare exactly after that normalization.

Independent static review preserves all 86 original code records, 72 exact
`.pdata` extents and 226 transitively referenced data/unwind regions. Applying
416 code and 264 data relocations reproduces the linked PE bytes. The sole
symbol normalization is main to LCoreWitnessMain; only the new envelope main
is added. Primary and independent disassembly confirm successful fflush,
Sleep(2000), a separate call to the core and normal return. Root accepts this
in proof `2CBB7DD7B55046DB00C7D8F5F96A864E2F6FB7F173D5F6028F41F76053D04DC4`.
Root replays the retained verifier `BCE00392218CE1609D755FB266757CCDFA1258BA73FD3741FC574AE5C37C893E`;
verification `328D1D324D77A272439D0F49F8D8273E3A303997FBFB71852D55C6EEC4D6E4D1` passes.

## Accepted exact-Proton witness

Only 11 small staged files/103,754 bytes are transferred; the four CRT files
are copied locally on Deck. The fresh isolated prefix is outside the normal
bundle/sync root. Exact Proton10/UCRT, live process ownership and five mapped
images agree with the wrapper module reports. The optional atomic-wait DLL is
consistently absent. No forced module loading or DLL overrides are used.

Execution 19816 closes normally after 11.7058 seconds, Linux PID155687,
start168763169, with exit zero, no forced cleanup and zero surviving owned
processes. All 42 cases/206 checks and all 44 normalized core rows match native.
Primary return analysis rechecks the 17 files/91,734 bytes, stage/proof bindings,
core validator, process record and mapped-image rows. This remains a controlled
body witness; no title, real GPU-coherence, driving/audio or FPS claim follows.

- Stage manifest: `A97DFD1C32C0CA9B0BA289D408167EA0E949C3AC35C2C4B59FB85B3BC756FD7D`
- Run result: `6C3136AE6AF62CB6C663D3BA8AE2D9CE7DC186653719AAF20F8BF9BE039AA307`
- Return archive: `FF5B2A87515FF4A2E7F1006E536EB0DC432FAEDD22DB7985BA349ADEC3F3AB47`
- Return inventory: `349DAD9544EDD8B4323BA9FBBCADCBB7CD29194D901522C42D0DE6A76C2035E9`
- Reviewed extractor: `CAE7BB4F4EA8B77C902AFD8DFECE251064B626C7DA112A2A3C2969A3BBC44040`
- Primary acceptance: `D7E8A1A89B141C4EA5EF92FA792A90B1EE97CC5EEC5C3FDA0C70D80ECE390F13`

The retained Proton stderr contains a 32-bit MangoHud missing-dependency warning
and fsync startup notice. The pinned 64-bit witness completes successfully;
this does not establish anything about a gameplay overlay or measured cadence.

## Next gate

Candidate build 63867 closes normally (compile PID12792, link PID33596), with
1,345 frozen inputs unchanged and no forced cleanup or surviving processes.
The frozen build receipt retains its original pending-static status; the
separate accepted review below records the subsequent gate.

- GPU: `D237D0311AA4FA13857FA090F6741D4C865299553CDB1E620D8C2D9AD2104F7F`
- SharedMemory object: `5D2D77B02C0B845E30D6B7D0988452B861D5F9FE1EC652C564018F7022544FF9`
- Link map: `E6DD6D333D4EF75B716094B27A7FD6B2C31D71362855A6D293142D459E510CC7`
- Build input manifest: `7D10B86D55DBC0A4F17180CFEC7F5F8F702F6001DFFD32DA8571E0D2C28B8B2E`
- Build result: `9090A7A775CDEBFF68DA57F16CAFA5D676F2C798566B26EF62D1DEF4D9070278`

GPU review `455F92EFE3D1FA5A98A171DEC0EA6697D2ABBF27E70F64D9F5F7DDD426C7675A`
passes: 99 same-name code records and four renamed cleanup bodies are exact;
only RequestRanges changes (2198 to 2245 bytes). Stack reservation remains
0xC8. The actual count-one branch snapshots both fields and skips vector
allocation/collection/sort before joining Ensure. RequestRange and sort code
remain unchanged. 102 MAP-bound code records reproduce linked PE bytes after
354 relocations. Two unchanged locale COMDATs lack MAP binding; no linked
location is claimed for those. All 1,345 inputs, 347 dependencies, 19 artifacts,
the 52-object link, 360 imports, four exports and ABI 2 agree; phase strings are
absent.

Package `4D880AB3E04ED54D99C3954671BA608682060265571CB71A6E7D599441023995`
contains nine files/64,765,984 bytes, with only GPU changed against J. Native
helper `62026DE15B82387F69C1C4B654EC191A45D7854EE70364B4D285727D050EEAE6`
passes source-diff and syntax review. It now requires both complete probe-profile
equality and no phase records for PASS. The inherited J result had reported
profile equality without gating PASS on it; its retained equality was verified
separately, so this does not invalidate that accepted capture.

Native control 31382/PID43136 closes normally after 173.6922 seconds. The owned
crash helper PID43316 exits, with no forced cleanup or surviving process. All
seven required images, six frame hashes, full four-file profile, source/package/
helper identities and no-phase gates pass. Root replays both strict v3 sample
analyses and views five frames: rear HUD0mph before, automatic front idle camera
afterward, and visible pause. Camera/work differences remain; no active-driving,
audio or Deck cadence inference follows.

The Windows intervals contain 598 outputs/20.000134 seconds unpaused and
600/20.000003 seconds paused. This is native regression evidence, not a Deck
performance gain. Native result:
`0C2322372836844ABCAA083AF22154D7BE5665439024D7343E43902487F60F60`;
module snapshot:
`C8E5CBECFB1B0E408EFD279546AE9F6B18B941A1E1F3CB52F61307C4B891BDF3`;
primary verification:
`C43AAAAD6C90A5C9ABDBCC742D4B4093A488D4270A440B06C015F62BF57EC6D6`.

The reviewed clean Deck baseline/candidate/baseline controllers use all 47 v3
counters, exact image/seed/runtime identities and normal per-arm exit gates.
Source manifest `A3AD5564A2A516267615623D5FC6256019B4CB5B4709C66D40F0537380FC55A8`
and four helpers pass root/independent review. Stage manifest
`99B1F009263D95DE7C9B745DC9C01FBA50B7CA70265D6B6070B119FD68E407EA`
binds 13 files/6,975,007 bytes; only the changed GPU binary and small inputs are
transferred, with eight companions copied locally. Preparation
`A21631256C9BB1D0A2C5A76B9EEFECE57CD527F474E6E9255F786D155C3130F4` passes.

First baseline execution 89684 closes normally after 178.1534 seconds, with
no forced cleanup, survivors, phase records or source/artifact drift. Result:
`CD4CC8F030D9A8122D4F480534EE09E67B24133574DC951B2963219EDCBDC390`.
Comparison fingerprint:
`D569AA611C7894246ABCD882FBC278B6B32A8ECB4163D308ED35BA32643490B8`.
Candidate execution 39137 and second baseline 43671 also close normally after
179.0961 and 178.6421 seconds. All three preserve the comparison fingerprint,
full seed, source/artifact identity, twelve-image snapshots and no-phase gates,
with zero forced cleanup or surviving owned processes.

## Returned comparison and disposition

All 97 returned files/93,231,110 bytes verify. The analyzer checks 1,345 GPU and
2,761 reused-host inputs and all 47 counters. Independent review reproduces its
output exactly and separately checks 282 raw deltas, cadence/work arithmetic,
lifecycle and image identities. Root views twelve endpoint frames: rear view
and HUD0mph throughout the unpaused endpoints, with the pause menu visible in
each arm. Traffic, pedestrians, minimap and lighting vary. Endpoint agreement
does not prove fixed camera or equal work throughout an interval.

| Interval | First baseline | Stack candidate | Second baseline |
|---|---:|---:|---:|
| Unpaused outputs/s | 19.10 | 19.30 | 19.05 |
| Paused outputs/s | 14.95 | 14.90 | 14.90 |
| Unpaused draw ms/output | 35.094 | 34.198 | 35.098 |
| Unpaused draws/output | 3327.534 | 3302.368 | 3298.543 |

Unpaused cadence is 1.047% and 1.312% above the two baselines; draws per output
differ by -0.756% and +0.116%. This is one small positive candidate signal,
approximately 0.20–0.25 additional outputs/s, with no paused gain. It does not
establish repeatability, fixed work, active-driving/audio behavior or stable30.
Overlapping/quantized timers remain qualified separately from cadence.

- Candidate result: `A411D7DAD223DB356610AF0471715DDBAE61F72E77F9D9EEB163D34834505E48`
- Return archive: `0F4F2CBCCBF4C7F76AB8B49274C16AC1A86ECE1531A30F00FD0403F17FF98511`
- Return inventory: `14424879EB27487EAD892BB1D77951B23607F036E70ED673D1123CD3CA7CDB67`
- Analysis: `AA91066236B30595856616942105AC1A1A759372B762DE2922F705CD9796F1CB`
- Primary acceptance: `5D22CBD8D5927F03CF92D236CBE8D9B3372DC73AE1A08CC5E0AB57EBBAC0B77C`

Retain the tested private candidate and complete evidence. This small result
does not resolve KI-027 or justify a normal-bundle update. No identical rerun
is queued. After the user resumes and the live allowance permits work, use
existing K/J attribution to select a larger remaining CPU-renderer cost before
another bounded experiment. Do not infer a recoverable budget from overlapping
timers or bypass memory/watch/coherence semantics.

The live API reached 80% used/20% remaining after the series and primary
analysis. Independent review finished before the stop signal. No further
experiment, reset, task closure, version bump, commit or push was performed.
The ordinary bundle, four-file save lineage and SDK source remain unchanged.
