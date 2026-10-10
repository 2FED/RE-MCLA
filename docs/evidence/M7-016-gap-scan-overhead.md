# M7-016: diagnostic disarm-gap scan overhead control

This private candidate removes diagnostic-only metadata inspection from
PhysicalHeap::TriggerCallbacks. It does not broaden protection ranges or remove
coherence checks. A fresh complete Deck retry found no reproducible gain. Retain runtimeAEE;
do not adopt the private B6 scan-only candidate.

The retained baseline is S precision-xmemory.cpp38A0AE80, backing runtimeAEEA10B5.
All seven precision changes remain intact. Candidate08339B84 preserves2193 lines;
ten edits remain inside TriggerCallbacks. It removes unwatched-page rights reads,
allocation-state reads and safe-gap bookkeeping. Watched-page rights, run boundaries,
all protection calls, callback consent/intersection, clipping, excess bounds and
final watch-bit clearing remain unchanged.

The original heap try-lock, miss accounting and complete lexical lifetime are
retained. Successful acquisition also stabilizes functional watched-page rights
reads; removing that lock would exceed this experiment's scope.

Only watch_disarm_safe_gap_splits becomes unavailable. Its zero storage must not
be treated as evidence of no gaps. Lock misses remain meaningful. The existing
disarm protection timer excludes this metadata loop, so that timer cannot by
itself quantify the omitted work.

## Equivalence witness

The full original and candidate method bodies are included byte-exact in two
fixture namespaces. The witness uses real recursive/global locks, an owned
bounded arena, recorded protection calls, explicit callback/protection span
oracles and independent page-by-page expected bit clearing. Peer try-locks verify
selected ownership points; a contention holder never mutates metadata.

37 scenarios /38 invocations per variant /890 checks passed. Cases cover fragmented,
safe/unsafe/uncommitted gaps; watched rights; aliases/page representations;
callback intersection, clipping and excess limits; early exits and repeated calls;
unprotect=false; contention and recursive ownership; protection false and dedicated
callback/protection exceptions. Ordered arguments, watch snapshots/final bits,
outcomes, misses and failure handling match. Eligible modeled metadata accesses
decrease, including2048-to2 in the no-callback excess-cap fixture. These are
modeled accesses, not measured CPU loads or saved time.

Witness source7348235E6E9C26B0C90E9779F673B1F6DC5E1ABBA40741D87967A8C74EA5BBB3;
corrected metadata pinsBA0A1C08638B28F7C6A2E3EAC487DEC1065D8E1D2C5340F2F271A4B6F0D56B3F.
The correction records that method includes omit the following blank separator;
the reviewed C++ and method bytes did not change.

Witness resultA3183B0E3AAD6AEF8063240ED800B5E0EF6E8B86F9F4CFC602ECEAF667E1A462;
inputsC33D6FCCC2EDD0422CD0FDB88134F037E3383A2CF140AF05F8477D5017072E79.
Compile/link/run exited0 with no forced cleanup or survivors; execution0.109s.
Exactly37 case lines plus37/890/0 summary, empty stderr and only two expected
original unused-lock-parameter compile warnings. Independent audit passed.

This is modeled protection and stable metadata during contention, not real host
protection, memory faults, unrestricted concurrency or physical gameplay proof.

## Runtime build and next gates

Private root: gap-scan-overhead-20260909.
Candidate source08339B8417D97DEFA6A379CAF4E3F218AB6BE25C4E9F42C2C03BE5B052D861D4;
source manifest6958E804465E36F3FB3C2527D116A62B71CAFAA68A51CF77B7DE7D78E5D1D3DE.
BuilderB8753DA20C9887F4F3588128793CBCC3A872D404C72EEA47C09FB6C5E78526E5
reuses exact S compile/link inputs, replacing only xmemory and private output paths.
It retains the old-stamp DebugOverlay object and222-object direct DEF link.
No SDK export-generation wrapper or normal SDK output is touched. Protected
absent MAP/PDB outputs remain explicitly absent.

Runtime inputs912B21B71AB35B31E852BCA33F5A3394D3A02E470EECB01F80A420356A1962DC
are frozen; both owned compile/link steps passed normally. Runtime result
E3956A9A0186CAFA33F1F1A954366760FD321C4E2AC1B1A2BAEF0E7A09C1B13D.
DLLB6E3EF75C4B751728C9B8FA2F53312A68DEA047290CEC9541A9A651078B5CCB0.
Independent bounded binary review confirms all11129 export identities and721
ordered imports,221 reused objects and only TriggerCallbacks plus its cleanup
fragments changing. Heap try-lock, protection calls, unlocks and miss counting
remain; TLS and image/section RVAs remain fixed.

Package6582D4E3 changes only runtime relative to the count-only GPU package.
Adapter4EF2691C marks safe-gap totals null/unavailable while retaining real misses;
two positive and six negative availability cases pass. Native validation passed; the first Deck cohort failed shutdown and the fresh,
unchanged retry completed normally. The fixed GPU is the prior count-only private candidate,
so its three elapsed fields are unavailable in both new arms.


## Native and first Deck control

Native result8614659D2FD29E7B29CA57FF85B15162FB36C27C675F273A1378E260254D85CB:
PID41572/birth134333858514039693,173.9116604s, normal exit0, no forced cleanup
or survivors. Guest cadence29.599997/29.999909 is near the cap and establishes
no gain. Independent binary/map/configuration/profile/frame/counter replay passed.

First Deck order a-scan / b-scan-only / c-scan, fixed host8E6E/GPU3D1B/U16,
1x/pre-mask=true/HUDoff, uniformly stopped Mango service:

| Arm | Unpaused guest/s | Paused guest/s | Closure |
|---|---:|---:|---|
| a-scan |20.749991|19.249996|normal179.008s|
| b-scan-only |20.149996|18.799995|normal178.596s|
| c-scan |excluded|excluded|timeout243.312s, forced owned cleanup|

C captured both phases and logged hard-exit intent, but that log precedes
FlushLogging and std::_Exit. Game focus persisted until cleanup; no saved
pre-cleanup liveness census establishes whether the title or wrapper stalled.
Treat this as post-capture shutdown failure, not proven wrapper-only failure.
Mango service/configuration restored; no owned survivors.

ArchiveF01A241712DED87141B3B8CFD6A567D9C9719C72C60471634B2D203992D467ED;
manifestA0976ABE578DAA6FC51A758E79B651CBB7ECCB208694ADA72CE839BE4E716FC4.
All142 payloads/94,216,540 bytes rehashed. Independent A/B audit passed188 raw
deltas, runtime-only binary difference,48 mapping records,4x13-byte observations,
12 frame hashes, complete four-file profiles,840 CPU samples and phase focus.
Root inspected six representative parked/pause frames including failed C; no
obvious corruption. This is not physical driving/audio acceptance.

A/B draws per output3307.33/3331.07 unpaused and5146.41/5400.42 paused differ.
Both accepted arms have zero protection failures and zero interval heap misses.
Three GPU timing totals are unavailable in all arms; safe-gap total additionally
unavailable in B. Missing return baseline and different work prevent a performance
decision. Failed C's raw records are retained for diagnosis only, never accepted.

Derived incomplete-cohort record595A306AA9DC4155446DB5D320E440FCA043569D415A3698BF13E130520CF54A
in private root/cohort1-analysis.json. No normal package promotion.

## Unchanged retry

Fresh remote title-control-retry1, same A/B/A and all240s/normal-exit gates.
RunnerAF6965BCBADC9F9744EFC02C3BEDE66656049E62EC6B1215816E7ACF08B406DD;
supervisor7DD74D9CC602970587C30685A12B08E1B95FE669072809158119A082EE549860.
Independent diff review: only output root and corresponding runner pin changed;
26 support files unchanged. Stage archive205019FC7C472861AA52B59F8D57C46598CA99EC53B362FAD5AFF03C47D32974,
manifest449D27F99D94416B14BBC7AD01129827731D38FD2271D90D29399E8E46E10581.
SDK launcherB1B8F8B6E34BAB9558A58B378758B4B100BA7ADE3FFC6DC5EBFE7C8B0B5078DB.
Official SDK retry completed with all three normal closures. The first failure
remains unexplained and preserved. If shutdown failure recurs, record owned
title/wrapper/child liveness before cleanup before changing shutdown behavior.


## Accepted retry outcome

| Arm | Unpaused guest/s | Paused guest/s |
|---|---:|---:|
| a-scan |21.049996|19.349995|
| b-scan-only |20.449995|19.049996|
| c-scan |20.249996|19.199988|

B is2.85% below A and0.99% above C unpaused; baseline drift is-3.80%.
Paused B is0.78–1.55% below both controls. Unpaused draws/output nearly match;
paused B performs0.80–2.34% more draws/output. Fault/protection populations vary
and disarm time/output has no consistent reduction. The disarm timer excludes
the removed loop, so these totals are not direct scan-cost measurements.

Retain AEE runtime and U16 as the reference for the next GPU interaction test.
No reproducible benefit justifies adopting B6 scan-only; this is not proof that
the diagnostic scan is free. No normal bundle or SDK source was changed.

ArchiveF249001BB3B3E0AB1B361FAEB4BB8ED4AA09CC740091B1DE2669C5E9825B990B;
manifestDC8FD627F4FAD673316179249CF0803E58623407EA6023FC80D7B29D3C2F4B8A;
derived analysisC6979295440D4851793FBB83BFC112E15DC8AB52FE93B871E2189896E9A182F4.
Local retained directory: private/evidence/M7-016/gap-scan-overhead-20260909/deck-retry1-return.
The derived file's pending-review label records creation time; independent audit
is now PASS:152 payloads/98,097,587 bytes,282 deltas,6x13-byte observations,
72 mappings,18 frame hashes, complete profiles,1260 CPU samples and focus.
All arms closed normally, no forced helpers/survivors; stock Mango service/config
restored. Protection failures all zero; A/B unpaused each have one heap-lock miss.
Gap total stays unavailable for B. Root viewed six representative parked/pause
frames with no obvious corruption; different passing traffic remains visible.
Neither the captures nor automation establish physical driving/audio acceptance.
