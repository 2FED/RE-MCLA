# M7-016: controlled late-presentation threshold comparison

Changing only the guest phase threshold from8 to100 improved the matched
parked-scene cadence from22.55/21.85 to26.30 guest outputs/s. Paused cadence
improved from19.60 to21.45. Retain this as evidence for an optional private
implementation; stable30, moving-gameplay/audio acceptance and normal promotion
remain unproven.

| Window | A H8 | B H100 | C H8 |
|---|---:|---:|---:|
| Guest outputs/s, unpaused |22.549981|26.299993|21.849995|
| Guest outputs/s, paused |19.599994|21.449997|19.599996|
| WAIT_REG_MEM ms/output, unpaused |8.435|0.0466|9.610|
| WAIT_REG_MEM ms/output, paused |6.531|0|6.353|

H100 was16.63–20.37% faster unpaused and9.44% faster paused. Recorded unpaused
WAIT duration per output fell99.45–99.52%. Paused zero means zero accumulated
reported duration, not proof that no wait episode occurred.

B had4.60–5.66% fewer draws/output unpaused, but1.64–2.44% more paused.
Paused protection calls/output increased16–21%. Work and scheduling changed;
do not attribute the entire FPS difference to one timer or add overlapping
timings into an exclusive CPU budget. The primary-minus-WAIT elapsed proxy
was35.87/37.98/36.19ms per unpaused output, not a measured exclusive CPU cost.

## Scope and test mechanism

Every arm used the same host8E, runtimeAEE and private count-only U8 GPU8405,
same complete owner-profile seed,1x/pre-mask=true and HUDoff. Global vsync,
guest-vblank frequency, host presentation mode and binaries were not changed.

A separate controller briefly stopped only its pidfd-owned copied test title
after device initialization. It required two stable wholly stopped thread
censuses, proved current module/pointer/physical-variable identities and
I2/H8/rate60/override0, then performed one aligned4-byte process_vm_writev
(flags0) on the existing RW threshold DWORD:

    new = (old & 0xC07FFFFF) | (assigned_H << 23)

Only H8 and H100 were admitted. H8 controls performed the same-value write.
Other bits, other sampled fields, pointer chains, E offset and mappings matched
exact readback. Each intervention used112 target read bytes/22 reads and one
four-byte write; there was no retry or recovery write.

An independently running guardian required permanent write-path closure
acknowledgement or confirmed controller death before resuming the title.
A draft with SIGINT followed by immediate SIGCONT had a real race and was never
executed. The corrected main-thread/64-bit protocol has failure-only exact
controller/title cleanup; rescue or forced cleanup cannot pass. The parent
persists the intervention receipt before subsequent admission checks.

Final operatorDBF6CDD4E219BE3DAFAAEB8491DAF6F317C029158BD424F9A9FD196BCC10D6E8;
guard747D562C533BB02609DD615B83FADEF2B658309439B6E38EB6675691F451C3DB.
34 offline mocked tests passed in3.060s, followed by independent source review.

## Real kernel witness before game intervention

The guest-free witness used only owned scratch Python processes. One normal
four-byte write preserved all20 surrounding sentinel bytes and closed its guard
normally. Controller-death and timeout cases confirmed parent death before CONT,
then cleaned up the exact owned target. No witness process survived.

Witness6D4AA09CA0BA6A10305D673FD590667F756EFF1E34B9E8AF0328EC6E72A8092E;
result207FFD92FF5604F9029D91F808439669478327BE2CC0842FD17784F2AFA97ED2.
ArchiveA24968F119BE9BD17C37440F49B15AB103C54EC31A8DE93173163B7D64DD9ADA;
manifest452BC34AF21710EC95351A2D14F4E2C93B95C530B64408BDC6B4E78E42194791.
Independent audit passed13 files/51,824bytes. This validated the kernel mechanism,
not game performance or physical acceptance.

## Cohort validation and evidence

STOP spans were58.437/58.270/59.642ms. Conservative settling before the measured
window exceeded104.47s in every arm. Six later120-byte observations confirmed
assigned H8/100/8 with I2/rate60/override0; these snapshots cannot exclude every
temporary intervening state change.

All three arms closed normally, with normal guardian exits, no forced cleanup
or survivors and exact Mango service/configuration restoration. All protection
failures were zero. Unpaused metadata-lock misses were0/5/1, qualifying
fragmentation completeness only.

Final runnerEB042EF162733615025993CAC202E1D6C9AF6DE6DC8A32C69F9629E44FE8E09C;
supervisor1ECF4ADA33C8EB16AD4C057E9073735654FF0F87E3CBD961C813CBEB80C99372.
StageDAC02EB9D05327F77F41B8178C530A3C8F0E87CD6B05C863CAD45BCFFFE6D3D4
binds all60 source/proof files and53 prerequisite pins. Parent ancestry manifests
retain their earlier source identities and are not current-runner attestations.

Returned archiveFB3A8CB62794FEFE09BDFB2EF2155AC0F7DE71700E919831610F4BE108977A2D;
manifest806E7C1A4A91DBCD13A9DD52023914DFC877E2C3DB2138522F5719B8DF264C20;
analysis6F5E3BFF4B3D35987BB40D1DB857F2F554A76538B9DF6578F832D7FBA96B65D9.
Private root pacing-threshold-control-20260909/deck-return.
The analysis pending-review label is historical; independent audit now passed
214 payloads/112,400,982bytes, all intervention/guardian receipts, six pacing
observations,6x13-byte CVar reads,72 mappings, full profiles,18 frame hashes,
282 counter deltas,1260 CPU samples and focus.

Root viewed six representative scene/pause frames without obvious corruption;
passing traffic differs. Whole-arm CPU and reported frequencies are not
phase-specific title CPU or effective-clock measurements. No human driving/audio
PASS is inferred.

## Maintained candidate next

Prepare a private generated.27 overlay with an init-only, default-false diagnostic
CVar. When enabled, relax only the observed incoming threshold8 to100 while leaving
other incoming values unchanged. Preserve r4, original load/store/prologue and
other guest state. This catches the known direct setter call, unlike merely
replacing a dispatcher-table entry.

The implementation requires source/witness and linked-binary review plus new
native/Deck validation. It will replace the experimental external intervention
for further private testing. Normal SDK sources, bundle and owner profile remain
unchanged at this checkpoint.
