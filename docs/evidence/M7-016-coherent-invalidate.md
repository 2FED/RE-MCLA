# M7-016: conditional coherent-memory readback invalidation

Status: source/model, baseline reproduction, binary, native and Deck reviews
passed. The completed comparison establishes no measured benefit. No default
promotion or physical driving/audio acceptance.

The candidate omits only the resolve-readback vkInvalidateMappedMemoryRanges call
when the same device reports a nonempty host-visible mask wholly contained in
its host-coherent mask. Empty or mixed catalogs retain the original call.
All allocator arguments, slot state/layout, waits, barriers, mapping, copy bytes
and order, returns and other invalidation calls are unchanged.

HOST_COHERENT memory does not need host cache-management commands for availability
and visibility; those commands do not replace device synchronization. Existing
fences and barriers remain. [Memory-property specification](https://docs.vulkan.org/refpages/latest/refpages/source/VkMemoryPropertyFlagBits.html),
[invalidation specification](https://docs.vulkan.org/refpages/latest/refpages/source/vkInvalidateMappedMemoryRanges.html).

The SDK builds its masks from actual physical-device memory properties and its
host allocator filters supported types by HOST_VISIBLE. Therefore this universal
catalog guard covers every legally mapped readback allocation without per-slot
metadata. A read-only native Vulkan query on Deck reported one RADV VANGOGH GPU,
visible/coherent masks1916/1916. It created no logical device or allocations.
This query is not a per-call observation inside the game.

Private root: coherent-invalidate-20260909. Retained source B6C6655D; candidate
E86F2B95CAF5C83CB2F71F9B900EA40B38523A0A24FB2AAE8D19672582215B50,
source manifestC39536E9894E9AFB5C8B9938AC9BEEC606D2F328FA71534307D2CE16A9AD8680.
Exactly one reversible replacement around original4756-4761. Source, manifest,
model and builder received independent review. No CVar or new instrumentation.

The witness compiles the exact candidate/original blocks. It passed66,688 cases
and400,130 checks covering empty/mixed catalogs, all32 bit positions, supported
type intersections, both slots and original arguments/return behavior.
ResultD62ECB79; EXEADDB3EAB; inputD85F05B5. Four hidden owned Jobs exited normally.
These are model checks, not Vulkan driver or synchronization execution.
Scoped ast-grep scan and three repository rule tests also passed.

Fresh baseline result0B781580066C29B896F8ABB51CB69B82A66B85F1EB1A0D0E3B369FEB83EC86CA
reproduces U8 whole COFF/PE with only permitted timestamps differing.
BuilderF2A5E79F preserves the52-object graph and51 retained objects, tool/library
resolution, normal-output absences and owned process guards.

Candidate GPUCA09C84C3780539A4CD7DA391816EA2FD9191E215BECF51AAB95D802A490EB12,
MAPF9707F92975C3E13F4B2873F3F425AD5877123D2E76447B44BA1B9F1CD78AEBE.
Build resultDCE32352D3BA9C71DD583CB86630F85407303727C1B74D824D81824495B141DE,
inputF13D95E17B48DB28702E201A1CCAA6D9C9FF9D581081D01580C54E9A1C4AA10B.
Binary reviewB9E0E749 confirms the guard atRVA B24CC reads offsets1340/1344 and
skips toB2525 only for the intended predicate. Only resolve-readback changes
among317 normalized Vulkan functions.56 static calls,51 retained objects,
360 imports/four exports/ABI2 and3659 normalized vtable pointers remain.
TLS and target unwind prologue match; no whole-binary or exhaustive EH claim.

Package58D45F88825E659A4835B25A3782CD5CE16525E1B835106A8D213165ECFB3A6A
retains ABF host/AEE runtime/U8 endpoint policy and replaces only GPU.
Configuration proofC3F5C6DD/reader7EA004A0 bind U8 and candidate to the same13B
scalar layout at6349688-6349700; no wider process read was introduced.

Native helperD1250B67; result
3D98C332D761D27342F877917798B9B5CAC06B4874E2181DB9C153C4F8CBE334:
PID40892/birth134334437736368279, normal0 after173.850974s.
Independent audit verified nine binaries, seven loaded images, two13B observations,
94 raw deltas, six frames and full unchanged3BBC profile. No forced cleanup or
survivors. Root viewed all six frames; no apparent corruption. The unpaused
camera rotates, so this run is only a native regression check. H/I/override
were not measured; cadence comparison remains ineligible.

Next: one matched paused Deck reference/candidate/reference cohort, H100,
all8CPUs, HUDoff, fixed owner profile/cache and existing strict lifecycle/config/
pacing/counter/visual gates. The normal bundle, source version and SDK outputs
remain unchanged. No commit, push or public distribution.


## Completed Deck comparison

The117-source/86,194,360B stage was checked on both hosts; eight extracted
prerequisite validators passed before launch. Three serial arms used the same
ABF/AEE/U8/H100/all8CPU/HUDoff/paused route, changing only the GPU guard variant.

| Arm | GPU | Outputs/s | Draws/output |
|---|---|---:|---:|
|A|Reference U8|21.899929|5230.966|
|B|Coherent guard|21.899995|5283.927|
|C|Reference U8|21.949996|5169.002|

B differs+0.0003%/-0.2278% from controls, with+1.01%/+2.22% draws/output.
This does not establish useful gain. Retain ABF/AEE/U8/H100/all8CPU reference;
the guard remains a private tested candidate, not a promoted optimization.

All three arms closed normally0, without forced cleanup or survivors; Mango
restored. Full profiles unchanged. Independent audit verified249 payloads,
117 source bindings,141 counter deltas, six13B configuration and six120B pacing
observations. Root viewed12 frames: matching categorical parked/pause cameras
without apparent corruption; pixel/world equality is not established.
The first offline analysis attempt used exact floating equality and rejected a
one-ULP derived-rate difference; the preserved final analyzer uses1e-10 tolerance
for floats while retaining exact integer checks. Raw evidence was never changed.

Return archive38720F40970BEE185AB6AF218E09C1E0FA9A76E0C1AA0FBBC7220FCF174766B0,
manifest8E0A0B41CCDE0FF5E500CD200073F0B77171559FB68FB131741BDCF5638764AE,
249files161875008B; sequence9835784B74965B42E85AB4069263E5DAA01B211D9C5198F73FB965DAC8209FF2.
Analysis35A2F689BFFB3CE890C249A9752A3054858DADC2861BE567E7BDFF753B57B05A,
visual4FDDBE2789C13269566225DE734F30565F2CEEDCA932D7F6A855DD4B60BFDEBC.
Guard-hit frequency remains unmeasured; no clean causal or moving-gameplay claim.

Only the three successful regenerable compat directories were removed after
verifying all249 preserved files before and after. Profiles and failed test
prefixes remain; free1175851008B afterward. No game remains active.
