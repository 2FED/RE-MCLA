# M7-016: current H100 first-hop caller capture

Qualified offline first-hop diagnosis is complete. The original game run remains
FAILED after a parser cardinality rejection and forced cleanup. A separately
reviewed offline correction recovers bounded caller/size evidence, not normal
runtime, clean FPS, physical acceptance or a new optimization.

The accepted3303-leaf profile identifies a shared guest wait and GPU-thread CRT
copies, but cannot identify their callers. This single12s99Hz all-title userspace
capture records20 registers and at most128 valid stack bytes per sample.
It retains ABF/U8/AEE, flagH100,1x/pre-masktrue, full copied profile/cache,
HUD/MangoOFF and the exact clean profiler environment. No automatic DWARF
callchain is trusted, no arbitrary stack word is scanned and attribution stays
offline after closure. Raw private profile.data contains registers/stacks;
summaries do not disclose their values.

Parser16D9CB1BC504594687E3A0B61BA64DFD69F8B5B4053ACACA244ABA0497FF3C56;
proof17F40966FA3FB5AB0CBA3211167D87D2F854FE62BDFAF9ED01ECB9A354009950;
tests739963FB7612EAD760787B33D30F22C84F5E903FA155FA57F2F4F0655E77EF96.
Independent source/proof review and35 offline tests PASS0.175s, result
0D11B8AF94766EC8C39AB2347E113528A038DC5CD93FB9A8D6FA9FD3F44ED60F.
Wait admission is restricted to eight proven PAUSE IPs,96 valid bytes and
stack+0x58, with six exact current E8 returns. CRT admission uses stack[0:8],
42 proven IPs/four known GPU callsites; only38 stable IPs yield in-flight sizes.
Unknown/transient states remain unresolved. This is not call counts or traffic.

Real helper-format witness: owned arithmetic target216690/start187132309,
normal3s exit; perf1s record normal0, no force/survivor or game.
97 samples have ABI2,20 registers and all128 valid bytes.
ArchiveB80C5A44FC4E1B68B4DE5239FD8601BC9933F1BC740383B0BA302D1478814A34;
manifestCD715641749D69FDFF93AC08F0D884FCD6606C98EE736C801525A68B21B96F12,
4payloads50974B independently verified. Raw5D6500786F1E5804FAA191B7566B8DCFCAC89DBE59C7C3F5262EB9BE9C3850A2.
Parser receipt5BC30AE31DA5C2093FDF78DD2BAD8E3A965997B2B19BCD596494D97FBD74D476.
Actual perfscript-G/header rendering and extracted parent header validator pass;
receipt6978559070C10BECE6B17F15A7B19447B45B405E33FD8EB3D04C1A1C55030131.
Only the exact known optional DWARF/register warning plus two valid completion
lines are accepted.

Runner71CFD1C773188729623CCF256142B5E56F0BCD920175A3604A4A4490CEFDC161;
supervisorA1E8A54C5C998C5ADD3124167033DD13CAA790EECE74217A41C3E07D50140F9B;
plan14E9C05B2BE6D170A73B2A221848269FC3E057333010EBDD2D9A8C2D4B41A75D;
stage map5B117EC31D5D6BA654B04710069BB400755272E2F25DC3C630020C27C27154F9.
Independent final source/map review PASS75files82177710B.
Actual archive3A0296956A63AAFCB6AAD993AD5C4AECA8652B37E7F0528E2119B78FE54CFA75;
manifest00B248F884530958D773292AFF93A51CFAB983286E7C398206EF2FB500AAECA7.
Strict transfer/unpack verified. Remote .../pacing-caller-stacks-20260909/title-callers,
singleh100-callers. SDK launcher437FD6FDE536E5AEAEBA67EEA740A15F5199332FCD50B8E88CCFE4D4F17C3C8A
preserves previous launcher; runner216899/wrapper216910 active, firstH100 checkpassed.
Wait for profile and normal closure/Mango restoration before collection/acceptance.


## Preserved failed run and bounded offline correction

The three profiler children exited0; recording lasted12.295s. Original reader
rejected the event-ID table before decoding:4352bytes/544IDs versus its256-ID cap.
THREAD_MAP has68 threads and CPU_MAP records0..7, so544=68×8 is legitimate.
The source run remains FAILED, with forced title cleanup at132.692s, only initial
H100 and no second pacing snapshot/full counter pair. No survivors; Mango restored.

Return118payloads117569066B independently preserved/verified:
archive9BE5509DF28875E020B4C7B5B341D245FAC8266732855F13C166A508399481B3;
manifest8D0B67207D7B2A3335C8489691B7FE3D5A162CD687A6610BE552C3FF91EE0B8F.
ResultBF05BF2E2445D39836318E35E43F1490B5D8FFA198937CA6A7ED3C65C5D31258;
profile4B7501C0F4DD8749D3F028BB1A32E9FFD10869A3775A76F41F0BE4FD4CDB29AD;
rawCE63180D97EC7622FC7892E2DE300A2A1F26581E368E5AA5565941D0120E4F6A.

New offline-only stack-parser-multicpu.py
5C47E34C366B981E07D2D59B0F4B1ED66245577B4EEAB73499BFCFB7EFF2783D
admits at most256threads×8CPUs/2048 unique IDs with existing section bounds.
Tables above256 require exact bounded THREAD_MAP/CPU_MAP dimensions and product;
other stack/register/PID/caller guards are unchanged. Independent review PASS.
50 tests (35 legacy+15 topology cases) pass0.321s, result
81C9C8D8DFFC7D2EA90C5BD44C7FAAFB57199774458B09423BF8BD19ABB9C9EE.
Original reader, harness and rejected receipts remain immutable.

Root qualified raw/header/text/metadata replay
2641684B467D41012FA83456AACB557A7627CE7EE04EA453048A4120BD90DBBC
decodes3406samples, all ABI64/full128bytes, spanning11.989272356s.
All22 sampled TIDs retain births across68-thread snapshots. No lost/throttled
records;103419MMAP2 records remain explicit, not silently discarded.

## Independently confirmed first hops

Root analysisFCCA864482AD2A9D5D09D40F7C9C8C57D83C020A1B81F384330A1B26E54E6D1A
matches independent raw/current-MMAP2/stable-executable-map and birth replay.

| Proven caller | Samples | In-flight size observations |
|---|---:|---|
| Guest82411E98 at hostcall1294D59 |381|Not a byte-copy measurement|
| IssueCopy_ReadbackResolvePath at GPUcallB2555 |98|64KiB–5MiB; median1.875MiB|
| VulkanSharedMemory::UploadRanges at GPUcallF66E9 |63|4–444KiB; median28KiB|

All381 approved wait sites occur on TID217164. All161 proven copies occur on
packet-leaf-identified GPU TID217128. Two whitelisted copy leaves have unproven
returns. No attributed sample uses a transient-size IP.2254 other leaves are
outside the exact instruction whitelist,608 outside the four bound modules.

These are sampled observations, not distinct calls or transfer-volume weights.
No second hop or automatic DWARF chain is inferred. The failed title/capture is
not retroactively accepted; only the bounded diagnostic result is accepted.
The published H100 clean control remains the independent FPS evidence.

Current evidence supports investigating actual readback allocation/cache behavior
and consumer requirements before any copy suppression or kernel replacement.
Source inspection still shows kReadback prefers cached memory; accepted INFO logs
do not record its selected memory type. Do not claim the allocation is uncached.
Earlier caller yielding and generic copy-kernel replacements remain rejected.
No further optimization or normal package promotion was made in this step.
