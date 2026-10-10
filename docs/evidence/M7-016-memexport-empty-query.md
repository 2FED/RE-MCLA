# M7-016 empty memexport query candidate

Private candidate built and root and independent binary reviews passed; runtime and performance
acceptance are pending. No normal source/package/version promotion.

The Vulkan draw path used to query the memexport readback setting before checking
whether its per-draw range vector was empty. The candidate moves the empty check
first. Empty draws skip the legacy read and registry query; nonempty draws retain
the original query and body. Preceding exact/conservative memory invalidation is
unchanged. Skipped empty-case locking/allocation exceptions intentionally disappear.
This does not establish the live empty-range frequency or recoverable CPU time.

Private root: private/evidence/M7-016/memexport-empty-query-20260910.
SourceD96951F2AF80E4732F6ED2A92A911BF60F40115B50CEF2FEFCA2C7CF65BC57B8.
BaselineB6C6655D/candidateB7F10503 differ only by the two-line condition order.
Root and independent source/build admission passed; ast-grep scan passed.
Witness2003D2C705F85EA91D073B2CC4997D31FA76A9817DF774CB928450BB0388B2AC
passed29cases222checks,3normal jobs and24verified artifacts.
This is a fixed query model, not a live registry/Vulkan test.

Builder7A892E8E reuses historical unchanged-U8 baseline0B781580 with its original
C395source provenance. Whole object/GPU byte reproduction differs only in timestamps.
Candidate input9293436B freezes1574inputs/687actual dependencies.
Build85052F28009045344088105E4A7AE034F9A9F730375FCC9BA5DC75F232A44FD7
completed2normal jobs, replacing one Vulkan object and retaining51.
Root verified19artifacts, unchanged15DLL/360imports,4exports/ABI2/alignment4096.

GPU C9053D2685CFA8C03D6D7894EEE6B029CB11973A14B06186AAE3F5652A78309C:
6516736bytes/SizeOfImage6574080.
MAP D5872E755530211E1C8A2C76123C32D4CD8B7F907BF94AFE8A271797B7617228.
Actual code loads vector atRVAACAAC, comparesACAB6, branchesACABD toACCF5 when
empty, before the getter callACAE9 targeting1ACF0. This proves the compiled skip,
not its measured runtime value.
Root binary review43AC10D8A6A5E9F8048A7F8A9DC11689E2F0D110648E2794CA67C5A22155EBCC.

Fresh package-empty-query manifest:
3D59E259A81434114DF872EB7953B843B2D42347D61066D1E379313A4B1CBDBA.
Nine files64826400bytes; only GPU differs from preset package337928.
Host0850/AEE/CRT stay fixed. Host source4505 is recorded separately from GPU D969.
Next: native preset4 regression, separate observer proof for the actual GPU,
then controlled Deck comparison. Existing observer7F03 admits only reference8405.

Independent audit confirmed19candidate/24witness artifacts,51retained objects,
45ordered library arguments and the unchanged presenter bridgeRVA18010.
Native helper7742AFD6 uses the unchanged80-case-tested motion protocol; syntax
and root diff review passed. Native preset4 regression completed normally; see below.

Native result683313DF28AA185E5BAA733B1C3FEE300795A6CE1C721DF9C3699221F6AC06D7;
log65FD16CF20C932A7CD17D81C6BD86F36840FBD584F40E735C0D4D7B6B46FAE42.
PID28444/birth2026-09-10T03:44:30.1985608Z,134.1379487s,normal0/no force/errors/
survivors. Root+independent audit passed9bins/7loaded images/five frames/v3protocol
and unchanged full four-file3BBCprofile. LoadedC905 actualSize6574080.
Root views:0mph before,about2afterturn,35afterdrive,18aftercoast nearintersection.
Traffic/trajectory vary from earlier runs; no continuous clean-route/gain proof.
No obvious broken texture/geometry observed in these five frames; audio untested.
Separate C905 external observer proofBAE960E3/reader76888A1F passed33fake-backend
tests5.543s. Existing baseline proof remains unchanged; live measurement pending.

Historical opportunity evidence was found without a new instrumented build:
O draw-request-probe-20260907/deck-return/verified/run/request/mcla.log(E5A16853)
and request-analysis.json(2DB5C499); P draw-request-path-probe-20260907/deck-return/
verified/run/path/mcla.log(A2EA441F) and request-analysis.json(193227A0).
All5publications per run report q3_calls=q4_calls=0 and invalid=0.
Selected unpaused/paused F13 visits:O1158202/1329105;P1160026/1378806,unmarked0.
q3counts every iteration of the unconditional memexport residency loop before
the readback getter. With no intervening vector mutation, these completed
published draws support an empty-vector inference; a false getter cannot explain
zeroq3. F13 visits are broader than draw-body executions and are not a draw count.
These are historical instrumented parked/menu intervals, not current H100driving/
pursuit frequencies, caller-key attribution or a measured FPS gain.


## Prepared-observer Deck A/B/A outcome

No meaningful game-rate gain established; retain referenceGPU8405 and do not
promote C905. This does not exclude a sub-percent CPU saving in empty-range draws.

| Arm | GPU | Counter delta / local interval | Active refreshes/s |
|---|---|---|---|
| A1 baseline |8405|144 /8.047868–8.048558s|17.8914–17.8929|
| B candidate |C905|144 /7.996212–7.996494s|18.0079–18.0085|
| A2 repeat |8405|137 /7.628486–7.628948s|17.9579–17.9590|

These are prepublication active-refresh rates, not scanoutFPS or exact whole8s
drive intervals. B is only about0.05refresh/s above the repeat. Each run used the
same prepared observation algorithm, original complete seed, preset4, H100requested,
all8CPUs and Proton10; onlyB's GPU/its matching proof/package differed.
No method/read-count/finalization/cleanup change was added in B.

Root saw A1 traffic collision(12mphafterdrive->0coast), B38->31mph with no endpoint
collision, and A2 38->32mph similarly clear. No red pursuitHUD in B/A2. Traffic,
lights, sampled trajectory and endpointpoll timing differ; this is not a fully
matched deterministic workload or a statistical proof of a tiny gain.
No unchanged-repeat request is justified solely by these sub-percent differences.

Candidate B root:
motion-active-output-prepared-empty-query-20260910/deck-p4-a1-return.
44files23225988B, archive61AB4B833D92FD893593DFFAC02661B7D46F693AD8ECF620B13900EFAF17D63E,
manifest9991570BD6F99805779BFA587BA8600FB50B69FB6F32317C709EBA127103E0E6.
Result1E597FE064C890EFFB29F3B46C5CFC1CA6D408C4896D08B2344DE6CDB15EDC56;
pairDC27FA047AF2AB934BBF67EE131029CFC8DD7F1D971B3E3DC88000FD77037C49.
PID245118/start195323740,135.339975s,normalwrapper0/no force/survivors/Mango restored.
Root+independent44-file/canonicalprepare-endpoint-finalization/module/profile auditPASS.
One partialshutdown state file retained; no physical/audio/continuousroute claim.

Repeat A2 root motion-active-output-prepared-20260910/deck-p4-a2-return.
44files23084558B, archiveECB3385163E0A3E5BD67B3A2B0C0D5CD9C6B971302DA478E02675099BB4DB4F0,
manifest65DD4CA0F346550FE566606B39B37AF0EB8A2C2F54C13380EBD5A1FF21AE92AA.
Result13FAD9CE4CFF4832D5FD13FD43DB0FDEFEA3A47BBBD394BCD9D821A0CC841983;
pair3455BBCBBD3744B9B0C64688968BD20C8FBF0A8677EF3B83A8E3193B0BF15FAE.
PID245875/start195411587,135.319970s,normalwrapper0/no force/survivors/Mango restored.
Rootfull44-file/profile/5frame/preparedfinalization/protocol auditPASS; independent
repeat review also passed. All original four save files remained unchanged in all arms.
