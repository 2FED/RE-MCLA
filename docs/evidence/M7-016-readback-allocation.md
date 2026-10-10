# M7-016: resolve-readback allocation metadata

Status: native and Deck metadata runs completed normally; independent full
evidence audits passed. Observed readback allocations are cached on both.
No allocation-policy change or performance gain is claimed.

The qualified caller capture identifies sampled copies in resolve readback and
Vulkan shared-memory upload. Existing allocation policy prefers host-cached
readback memory. This default-off/init-only probe records actual scalar metadata
after successful slot installation without changing policy, reuse, mapping,
cleanup, waits, copies or synchronization.

Source4EB952F906199AC25732B1AB2F0B9DBF36067BD7881FBF022F8AC174ABFB7066,
pins7439A33ACA7EEBB0171CD8D32F7F5C2BA8F63B9972DAC6436F8CD65516CF510F.
Three insertion blocks and one optional-output change reverse exactly to
count-only Vulkan sourceB6C6655D. Up to32 best-effort records contain slot,
event, selected type/property flags and requested/allocated bytes; no addresses
or handles. Tickets may be lost/reordered. AEE allocator object provenance binds
the optional output API; current unfrozen util.cpp is not its source proof.

BuilderB7D2E3F4BA512D067821FF352F709619ACFBC263DB98A1E12F1CF7A88FDAF27F
and plan547060EE9827EFC638977FE3B7687A10BD95C4A62EF3A9DFB4469BA0178238F0
passed independent review after fixing /implib output classification before
execution. Exact timer compile and combined U8 52-object link retain51 other
objects and4096 alignment. All outputs/VFS/TEMP remain private.

Baseline inputEEBDCBDA99FF85DEB8BE976A872F61CF3AE91DE088BA9E7EF176D4F8D4428531
freezes1517records/687dependencies. Result
C4E2BD597264609952D2E81093B8A7F84C66A0D5F6C1DE0C72EDFF05ADE581E0
is BASELINE_IDENTITY_PASS: compile7.141s/link0.234s normal0, inputs unchanged.
Root compared whole object/DLL excluding only COFFtimestamp4–7/PEtimestamp128–131.
Reproduced DLL E61931F34D81C6BD98F569A3B1404BC4C4DB7696C0B87DF81C234352826943F6.

Candidate input3F3E5AF113222B1AE68DC2C7CADBB8F6E3D95C5B9481D7832099F0AF7DB1322A
freezes1537records/687dependencies. Result
E8EC6E44A9A842529D4F44FBF5A6D77CE812C1ED56F38703B7D6CA4DE0E2475E:
compile7.078s/link0.203s normal0/no survivors, inputs unchanged. Compiler stderr
matches baseline exactly. GPU
F6594076B6356BFFCD936C447128396EA158C57251BA78AAF45EA67060F56624
is6524928B, SizeOfImage6582272; ABI2, four exports and360 imports retained.
Binary review1372C5B32DE372EA29A5122C1577C78ACD71E38BE11C04745DED661F1CA66EE9
binds actual property masks to AEE and verifies default-off null outputs, stores
before logging,32-ticket cap and guarded type shift. Targeted, not exhaustive.

Package403C01CC2C666318D24CD8E917513C0174818DB7F35FDFB1EAA394DCCC09CCFE
contains9files64799264B; onlyGPU differs from accepted setter package.
Config proof26B7D8E5B3CB56D8FFF0DF721EFEAD31DEB01A95955AB0F0723862BC153016E5
and readerCAEC04B7CD6F1818D7C6614BE1AF090C3DE0796D31618883071169A8B6579B0C
add this GPU while retaining13-byte reads/historical entries;25 pure checks pass.
Allocation parser70230848F9844E6FA90FF33F357AE9F4DC73310EA1C7FC8D60A392EA1D4D9EA3
and native helper5508124AAAB2CA05A7B9ED5FE0357CC2766CBCA1F2E648083A4BBB0A038EF0DB
passed independent review;23 parser tests and PowerShell syntax pass.
Zero records are UNOBSERVED; invalid memory types UNRESOLVED. Gaps/reordering are
permitted; this is not a full allocation census.

Native-probe PID31364 completed normally, exit0,173.7512535s; no force, survivors
or cleanup errors. Source/full seed/binaries unchanged; two GPU13-byte observations
pass.32 valid records all report type4, host-visible/cached/coherent, not
device-local. These are actual properties of logged PC allocations, not Deck
properties, allocation frequency or recoverable frame time.
LogA524D6FC3DCA346F299BD2FFE5397280DD83B350500675CABF91174596523C28.
Root inspected six frames: game/pause render, idle-camera rotation is present.
Native guestH was not read; no clean FPS or owner driving/audio acceptance.
Independent native-evidence audit PASS: nine binaries, seven mappings, two13-byte
observations, six frame hashes,94 raw deltas, full profile and closure reproduce.
ResultA3EBC19F147F5C873B91C7E56B62A0AAA5E6140BDE10311DDAB44B096461D4FD;
analysisA08F2954DE755768FDB9C3CC3553AD0EBBAD5605E8C8EA1ED59B06EF40715345.
Sizes:23x256KiB,6x4MiB,2x1MiB,1x512KiB; the32-event cap exhausts early.
Protection failures and gap-lock misses are zero.

M7-016/KI-027 remains active; no normal-package promotion, version bump, commit
or push. The diagnostic allocation question is resolved within sampled coverage.

Independent frozen-source reuse review: slots already reuse mapped capacity for
size <= stored size (baseline4508–4511). Exact address+length keys mean different
lengths can select separate entries even when rounded allocation size is equal.
Each entry has two ping-pong slots. Entries older than60frames or while map size
exceeds256 are evicted; count eviction follows unordered-map order, not LRU.
Thus repeated slot0/256KiB logs do not prove shrinking/reallocation of one buffer.
No retained-capacity change is justified. Allocator padding is not extra VkBuffer
capacity. Address-only keys or shared slots would need preservation of delayed
payload and GPU completion semantics. If churn becomes a measured lead, use
settled-window installation/reuse/eviction counts before changing policy.


## Completed Deck capture

RunnerCE5BF566C88068ACACA3060C123D65B7E5B1D149441901636E1B9CD11E103A15
and supervisorEFDE0733D21D676F797079F9B3AEC0BA00B597D481B8C45C49B0AA77C5F54715
passed independent review.15 pure staged contract gates replayed historical/new
native/package proofs and rejected invalid metadata CLI forms without live access.
Stage88files83303601B, manifestCF43491B64A895650A8E85FB35FB5DE3677137C9A554BBF93077F857E11F2D0F.

Official SDK run: title218181/start188011460, runner217985, supervisor217976.
Title normal0/178.23533290997148s, no force/survivors/cleanup errors. Sequence
COMPLETE, CPU helper normal, Mango restored with original config9AEFDC69.
ResultFA8D86F1B78521CE3F3AF01BCBC9A66436C692FBB5E323CC0DBD778B50D6221F.
Allocation analysisA6CB2A8D4422A2C382CD260231416FAEBFD5CCAF64454865EC3FA03735792772;
log822F3B3B88ABC233C07908627D1EECD0644256D73805769F6C70C37CAE9E6EE4.

All32 records: type5, host-visible/cached/coherent, not device-local;26slot0/6slot1.
Sizes:15x256KiB,2x512KiB,2x1MiB,3x2MiB,3x3MiB,6x4MiB,1x16MiB.
Requested and allocated sizes match. All records precede the unpaused window;
the cap exhausts, so this does not prove every subsequent allocation or a census.
Do not compare native/Deck size histograms as allocation-frequency measurements.

Root strict return and independent audit passed142files116264433B:
archive3897C6F8B2D5C82E29874C9BB6BFEF8A77E812B8C1F8812175D8F3EB26511C8B;
manifest410EB25A15289C2BE2AE275EF2CB40A8A8699C28F7755623AC562C8E5656C06D.
88staged files, nine binaries,24mapped-image observations, two13-byte GPU reads,
two120-byte H100/I2/rate60/override0 reads, full four-file profile, six frame hashes
and94counter deltas verify. Root viewed all six frames; game/pause render and
idle camera changes angle.420CPU samples/3352valid intervals plus8baselines,
no missed deadlines; sidecar CPU0.512s/210s. Recorded focus is consistent.

Diagnostic windows:524/441 outputs,26.199995/22.049994outputs/s, zero protection
failures and lock misses. Readback copy spans3.677/4.529ms per output; these
existing timers overlap and do not establish exclusive/recoverable cost.
No comparison/FPS gain, continuous H, physical driving/audio or stable30 claim.

Disposition: do not change allocation policy based on an uncached-memory theory.
Existing retained-capacity behavior also rules out a simple shrink/reallocate fix.
Next read-only lead is residual FMA scope/caller feasibility in the accepted H100
profile; do not repeat generic copy kernels or enlarge cache without a measured
mechanism.
