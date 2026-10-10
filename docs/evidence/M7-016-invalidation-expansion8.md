# M7-016: private16-versus8-page endpoint expansion

The retained16-page candidate improves bounded Deck cadence. This adjacent8-page
experiment changes only two endpoint masks at SharedMemory lines614/620, retaining
full requested coverage, GPU-written barriers, exact-range behavior, watch order,
and mandatory1x/pre-mask=true scope. More retained protection can increase faults;
a performance gain is not assumed. Normal SDK/bundle remain unchanged.

Source manifest27364CC57E1E6FF84672D758DF8F49F86C4BBB7A6AD33B5EF93CAD33E5E5D218
binds U16baselineA605ACD8 and8-page64D7A23E. Exact method includes587–647 and
785line preservation independently verified. Witness adds7/8,23/24boundary cases:
832cases/17474checks PASS, normal0, no stderr/force/survivors. It models range/
flag/watch/consent behavior; it is not actual memory-protection/concurrency proof.
Result25ECD50FBBD780A39411DC8379A0AEF122B3E1D8DDDF6CADE5FF80F3DD26C5AE.

Builder4E6BD001 retains the accepted52-object link with clean Vulkan plus50other
objects and one new SharedMemory compile. Clean SDK01085 and clean-reference18A
are kept distinct from the U16 comparison baseline. GPUbuild result
B13C74E9A5DAE391AD32B80E3ADD14F71B457141EF461BB99712FE0FB3919177.
Independent binary review:104code records/144sections; only2immediates change in
MemoryInvalidationCallback(RVA44580). All relocations, symbols and100unwind records
match. EntireDLLdiff is two code bytes plus3COFFtimestampbytes;4exports/ABI2/
360imports unchanged. Three warnings are unchanged unused baseline code.
GPU07A29A954512E65CCB40C6D77DFFE7A76E07BA21E04F630CC8A38A366E3CC5EB.

Private packageABEBC64F96AF37E50C5AE96961B0362DD743AE1502F161C3DBC263D50670D3BA
has9files64773664B; onlyGPU differs from V44, preserving8Ehost/AEEruntime.
Configproof721402FB0105D747FA1C9F88FEA74626D2F56D0A8AC00A139F510BC8CBBE5E83
binds actual U16/U8 MAP accessors/LEA targets/defaults. Reader6CD5E444 differs only
in proof admission hash;13-byte process-read scope unchanged.11puretests PASS.

Native gameplay/effective configuration, FrameViewbackend calibration and eventual
Deck16/8/16 comparisons are pending. Use preserved terminal manual profile and
HUDoff. No native/Deck performance acceptance, promotion, version bump or commit.


Native U8 PASS B539BB624DF6D86C5D79E6B31F68ED37106E97BD6ECBDFBB668908345CE117C9:
173.7947s/exit0/no force/survivors,94counterdeltas,6frames,7loadedimages/9binaries,
2x13Bconfig records and full4filemanualprofile independentlychecked.29.74909/29.99954
unpaused/paused outputs/s; near30cap, no performancegainclaim. Rootviews include
idlecameraorbit, so not a fixed-scene throughput proof. FrameViewbackend attempt
separatelyfailedexit1in0.07437s withnoCSV/emptyout+err; noETWsessionleft. Causeunproven.

First SDKcohort collected onlya16 (20.00/18.74994); b8 readiness failed twicebefore
anygame. OriginalA andbothfailurespreserved. RestartingonlyMangoservice andthen a
Steam-onlyautostarttrialdidnotyieldidleCSV. Supervisorstoppedcleanbetweenarms.
Freshrepeat usesconfiguration-ready toallowgameboot, butrequiresactualCSVbefore
measurementBEGIN andbeforethecontrollerlast-begin-absent observation. Everyarm uses
thissameprotocol; originalbefore-bootCSVcohort isnotmixedintoitscomparisons.

## Completed counter-only comparison and disposition

The uniform Mango-off 16/8/16 cohort completed normally. All three arms use the
same host/runtime, 1x scales, pre-mask=true, HUD off and full terminal owner seed.
The retained reference remains U16: this single cohort does not establish a
robust gain for U8. No normal bundle or SDK promotion was made.

| Arm | Unpaused guest outputs/s | Paused guest outputs/s |
|---|---:|---:|
| A16 | 19.949995 | 18.799995 |
| B8 | 20.099994 | 18.799997 |
| C16 | 19.949994 | 18.549996 |

U8 is about 0.75% faster unpaused, matches A16 paused and exceeds C16 by 1.35%.
Work differs: unpaused draws/output are 1.68% below A16 and 0.38% below C16.
Arm protection calls/output fall 26.44-29.77%, disarm calls 20.94-22.79%.
Fault count/output rises 12.50-15.77%, while fault time/output falls 5.32-12.58%.
No protection failures occurred. These overlapping timers are not additive.

Root verified 132 returned payloads totaling 97,827,786 bytes. Independent review
checked 282 counter deltas, six configuration records, mappings, 18 frame hashes,
full profiles and closure. Root viewed nine representative frames across all arms:
parked rear view and pause camera orbit, with traffic/minimap differences and no
obvious corruption. This is not physical driving/audio acceptance. CPU collection
had 420 samples/arm, eight initial invalid values, no subsequent errors or misses.
All arms and CPU helpers exited normally, with no forced cleanup or survivors;
the original Mango configuration and active service were restored.

Sequence SHA256: 2A9F8B324DB27F0CF928C4375293FD27F83B1B93DEFAFF2E4D7CEE3BBD906D87.
Analysis SHA256: 75275EF3DC3F401E8BCF2A15AADA9016DB7DCBBBE9AE9F8E52164E416D1C1725.
Private analysis: invalidation-expansion8-20260908/counter-return/analysis.json.

The preceding Mango-enabled attempts remain incomplete and are excluded. In the
second attempt B8 booted but never produced CSV, so the supervisor stopped before
C16. Repeated logger arming remains unreliable; an IPC success is insufficient.

## Next diagnostic lead

Unpaused draw-packet time/output is 29.100/27.545/29.254 ms for A16/B8/C16,
while WAIT_REG_MEM time/output is 10.507/11.981/10.430 ms. Shorter draw processing
coincides with longer dependency waits. This suggests reaching an outstanding
condition earlier, but does not establish which condition or an exclusive budget.
Existing WAIT_REG_MEM timing includes polling, coherency, yielding and configured
sleeps; it lacks the waited-predicate identity and sleep/yield census. Neither
1200 vblanks nor near-20 output cadence proves fixed 20 Hz quantization. A bounded
predicate and duration census is the next useful title measurement.
