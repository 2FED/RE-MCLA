# M7-016: per-game Steam Deck affinity investigation

Status: capability and corrected8/4/8 comparison reviewed and complete.
No stable benefit established in this parked scene; keep all8CPU reference.
Moving assessment awaits owner availability. First two failed attempts remain
failed and are preserved below.

Owner explicitly requests per-game affinity to one logical CPU per physical core.
Live topology pairs0-1,2-3,4-5,6-7; chosen CPUs0,2,4,6. Owned scratch self-affinity
8/4/8 and restoration passed without sudo. taskset2.41.1 binary SHA
E7EC2AA036D06473EF694C1B700D8FAD39060ED613EFA62258D4AF43062BED37.
No system SMT, governor, priority or clocks changed.

AEE Runtime::Setup resets affinity to Wine SYSTEM mask. The initial protocol
therefore combines taskset with PROTON_CPU_TOPOLOGY:
controls4s:0,1,2,3,4,5,6,7 and candidate4:0,2,4,6.
ignore_thread_affinities=true is explicit in CLI/REX environment. The override
also changes Wine-visible CPU counts and possible worker-pool sizes; it cannot
isolate pure SMT contention. Actual child Wine/Proton variables are checked.

## First8/4/8 attempt

Private affinity-control-20260909. Same ABF/AEE/U8/H100/HUDoff/paused-only scene
and source/profile/lifecycle gates. RunnerD28389CE, originalsupervisorDAF628A1,
actualsupervisorBAA3345D after one constant-only currentMango config amendment.
Currentpreset3 config38B6AB95 was never modified. Initialprelaunch rejected its
oldconfigpin beforemutation; a prematurelyissued SDKdispatch stillpointed to
oldone-shotlauncher. No title observed; oldsequence577BD911 unchanged.
Originalstage, source and amendment retained; correctedprelaunch thenpassed.

Source+independentreview and owned scratch tests passed. A8 title225609/
start189702880 closednormally0 in158.519936s; threeobservations47/68/68threads
all0-7. Its20-secondpausedsample gave21.699995outputs/s.
B4 title226103/start189723949 rejected exactmask atreadiness, before any row was
saved. Ownedforcedcleanup, no survivors; C8notrun. Mango restored.
Do not label candidate slow or fast from this rejectedlaunch.

Return160files54016479B, archive
8531697626CAB6C80B4EA143C13DCF320CD3DF09D04F90BC145628265BD07588,
manifestD01ACE4816542319837284E7CFF040B43ED7FB8926C38F883F434D22B55BF7C8.
Root and independent full preservation audit passed, bothterminalprofiles equal
the full seed. Sourceamendment only changes the exact Mango configuration hash.

## Single candidate admission diagnostic

Fresh affinity-admission2-20260909 retains exactmask rejection, adds bounded
offending TID/API/status/identity and namedtopology receipt before failing.
Root fixed two independentreviewfindings: keep successcomparisoneligibilityfalse,
and preserve core mismatch if optionalenvironment/comm reads fail.
RunnerC17F187C, supervisorC3716D49, source map7801B51A.
Reviewed; owned scratch tests verify8/4/8, wrongmaskreceipt, invalidCLI and TIDreuse
rejection, restoration. Scratch test is not game admission.

Actual title226597/start189770652, TID226608/start189770680:
main and earlierthread had0,2,4,6, but mcla.ex:disk$0 had0-7 in both
sched_getaffinity and Cpus_allowed_list. Actual PROTON_CPU_TOPOLOGY and
WINE_CPU_TOPOLOGY each4:0,2,4,6. No reapply occurred.
This is a genuine observed escape by a native worker, not harmless narrowing.
The failedtitle was cleaned up, no survivors, Mango restored.
Return116files28649134B, archive
E2DCACD2335980536216DCE97C2CF4383FA83D75056F134C1F58232D8C33D910,
manifest928D3934FBA7022EB4E5B3DA928AE131F9B13EB6A184EB17B47018DBA7767A2A.
Root strict unpack passed; no candidate FPS acceptance.

Upstream Mesa24.1.5 disk_cache_init_queue names disk$ workers and enables
UTIL_QUEUE_INIT_SET_FULL_THREAD_AFFINITY, whose worker sets an all-CPU mask.
This supports the observed mechanism, but is not exact installedbinary
attribution. [Diskqueue](https://fuchsia.googlesource.com/third_party/mesa/+/refs/tags/gitlab/mesa-24.1.5/src/util/disk_cache.c),
[worker](https://fuchsia.googlesource.com/third_party/mesa/+/refs/tags/gitlab/mesa-24.1.5/src/util/u_queue.c).

Read-only device inspection shows cpuset present at cgroup root but not delegated
through user.slice/user-1000.slice/user@1000.service. A privileged global
delegation change is outside this experiment. No shadercache disablement.

Next source: affinity-repin3, one-time guarded per-title-TID affinity at readiness,
followed by read-only exactcensuses; reject later escapes, do not continuously
reapply or change affinity during measurement. This remains sampled confinement,
not a kernel-enforced ceiling. TIDbirth/UID checks around sched_setaffinity narrow
but cannot remove the syscall's numericTID reuse race.

## Timing and acceptance limits

Readiness/pause-before/+18s late-live observations do not prove continuous masks.
Python/Linux monotonic and guestWindows chrono absolutevalues are uncalibrated;
their raw difference cannot establish whether a census was before/after the guest
counter window. Sourceordering and within-clock intervals remain valid.
All future four-frame scene reviews and normal closures are required before any
conditional cadence comparison. No physical driving/audio or stable30 acceptance.


## Readiness correction cohort (closed)

Fresh affinity-repin3 runner06A595AAABF99F77B434063B487E03DA959BB48BF461045CEE698CFFE1959C8A,
supervisor31E3B7FCCDD695C8D16DF406099D6FD2428E97A3C0D5C581751C59B9AB6A8D96.
Source root/independent review and AST passed. Owned two-thread Linux scratch
recreated an escaping nativeworker: corrected once, verified boththreadmasks,
rejected repeatedcorrection and laterescape withoutreapply, restored ownmasks,
joined worker. Receipt6FAFC45915B788D933A802797E01D373E3CEA1AA6C5CBD22BD819A7140DA0DD3.
No game claim from this scratch.

Stage87files20181024B, archive3D3626B252291AD05D9AF78639D8BCD475701FCDA37CB062C25743F65C251245,
manifestA45D77C12B62ABD7C71EE44B148B7BB5B9F4F8D9B230D9547443F15A64C5E179.
All three arms closed normally0, without forced cleanup/survivors; Mango restored.
Nine exact censuses passed: A49/68/68threads, B46/65/65, C49/68/68.
Correction calls0/4/0 before gameplay; all later observations readonly.
No newly escaped TIDs appeared in the observed phases.

| Arm | Logical CPUs allowed | Outputs/s | Draws/output |
|---|---|---:|---:|
|A8|0-7|21.749984|5183.400|
|B4|0,2,4,6|21.649992|5152.804|
|C8|0-7|20.699945|5307.952|

B versus A is-0.460%; B versus C+4.590%; controls drift-4.828%.
B also has-0.590%/-2.923% draws/output versus controls. No stable benefit is
established; this is not a repeatable4.59% improvement.
All twelve frames directly reviewed: same categorical parked/side-rear pause
views, but no pixel/scene equality or frozen-world claim.

Full return234files102200998B, archive
11A4764E708346697117E07F3528FD9DCCC4673E153B73DAC62A711D3EDEA347,
manifest6E7235F27ACA7AF6258475FC2AB32015A04C91FFF1416AFB7CD00C655579914D,
sequence0805CE6E766C4051E48B845983727F60668EEA44AC8CDEF7416FED8ECAC80047.
Root and independent audit verified141raw deltas,6x13Bconfig/6x120Bpacing,
profiles, source bindings, normal closures, correction identities and CPU arithmetic.
Analyzer46BC9B8B includes reviewed exact-field/hash/identity/interval guards;
visual receiptE5BFB8CC binds all12frame hashes.

CPU telemetry concentrates load on even logical CPUs, but it is system-wide
parent-clock accounting, not a calibrated guest window or effective-frequency
measurement. It proves neither exclusively reserved physical cores, globally
disabled SMT nor continuous affinity during unobserved intervals.

Disposition: retain H100/U8/all8CPU reference. No default promotion, new game
binary, version bump, commit or push. The moving/traffic/spawn hypothesis
requires owner driving; availability question pending. No manual run started.

Only three closed successful compat prefixes were removed after verifying all
234preserved files before/after. Profiles live outside them; failed prefixes remain.
Free1360994304B afterward; cleanup receipt retained locally and remotely.
