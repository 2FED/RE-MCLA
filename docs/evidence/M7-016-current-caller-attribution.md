# M7-016: current GPU callers and Proton protection scan

Date: 2026-09-06. Primary plus independent capture/provenance and exact
CRT/ELF/stack reviews. M7 remains 0/16; KI-027 stock cadence/audio stays open.

## Result and next decision

The current candidate's sampled large copies come from resolve readback and
GPU uploads. All 36 protection-scan samples resolve to NtProtectVirtualMemory.
Retained caller frames distinguish actual 4–128 KiB PAGE_READONLY requests
from the scan's much larger remaining-view bound, 53,010,432–437,051,392 bytes.
These bounds are not bytes actually scanned or native protection lengths.

The exact old Proton implementation passes an unlimited maximum to its
commitment check, then compares the result with the requested length. This
matches an already-merged upstream correction:
[Wine commit d9172b0](https://github.com/wine-mirror/wine/commit/d9172b0acf052d0f3c087a9f5f5ece29cd917c55),
[Wine MR 8151](https://gitlab.winehq.org/wine/wine/-/merge_requests/8151).
No new upstream report is needed. The fix bounds the check by the actual
request without removing commitment validation.

Deck already has official Proton 10.0-4b installed. Primary retrieved its
exact Unix ntdll and inspected the call: it passes the requested size instead
of -1. This is static evidence, not proof of improved title cadence. Next is
a reviewed isolated Proton 9/10/GE11 comparison with fresh private prefixes, unchanged
candidate/seed/cache/settings and no normal shortcut or bundle change.
Do not change SDK watch policy, split mappings or patch Proton speculatively.

## Exact artifact and bounded execution

Same parent 0.18.0.0/201c3a6 plus five-file default-off Type-0 SDK19 experiment
as [the preceding report](M7-016-type0-constant-batching.md). Exact manifest
FBEB08FBD1756D6A9E2452002455C20D81E4958B56FFD96CB9532004BAFB31FE;
host974392DD9C7D60578AC2BCA50E24317FD4F1F2E2EE19CD989219B72888B64164.
All nine candidate binaries are verified; no rebuild or source promotion.

Owner supplied a stable Tailscale route and renewed autonomous work to 3%
weekly remaining. Paired SSH verifies against the prior pinned Deck host key.
Private connection details: private/evidence/M7-016/deck-connection.json.
Syncthing is running, HTTP200 via local Deck access, but GUI listens on
127.0.0.1:8384 rather than the Tailscale interface. No network settings changed.

Fresh experiment /home/deck/MCLA-Diagnostics-20260905/current-caller-20260906
stays outside sync. Same original candidate binaries, batching OFF, fast
readback, Desktop Vulkan1280x720, same locally copied complete profile/cache.
No external game/save transfer. Existing normal bundle/current.txt, shortcut,
source profile/cache and measured binary hashes remain unchanged.

The reviewed helper first takes a three-second,99Hz userspace leaf scout.
Exact own pdata/MAP Type-0/Type-3 ranges select only one current GPU TID,
with six hits among601 raw scout samples/nine TIDs. It then records eight
seconds of that TID only,99Hz userspace, full GP registers,4096-byte maximum
stack, no inheritance, kernel sampling, build-ID cache or global changes.

PID119461/start161286410 and GPU119549/start161286566 stay stable across
all four snapshots. Exact mapped host/runtime/GPU/CRT/Unix ntdll hashes,
headers, devices and inodes match; raw MMAP2 headers independently agree.
All556 stack samples have ABI64, mask0xff0fff, actual valid stack2016–4096
bytes, zero lost/throttle records. Raw callchains are empty; the manual
register/stack proofs below do not rely on successful general unwinding.

Pause-before observed133.464s; scout135.639–138.902s;
stack139.285–147.628s, before pause-after observation. Primary inspected saved
Eclipse/HUD gameplay and both PAUSE images. Normal exit0 at154.596s; no forced
cleanup, errors or surviving owned processes. This is a paused-scene proxy:
clock domains and exact containment within the separate20s counter interval
remain unproven. No motion/controller/audio/GamingMode or speedup PASS.

## Verified sampled callers

| Observation | Samples |
|---|---:|
| All GPU stack samples |556|
| Official CRT memmove leaves |67|
| Exact forward AVX loop samples with proven SDK callers |56|
| Vulkan resolve readback memcpy |34|
| Vulkan shared-memory upload memcpy |22|
| Memmove outside the narrow loop proof |11|
| Unix protection-scan leaves |36|
| Proven NtProtectVirtualMemory callers |36|

Current official memmove has no stack adjustment in these loops: return is
[RSP], not the old builtin CRT's [RSP+16]. Original PE pdata plus code-identical
MAP names, decoded instruction boundaries, actual call/thunk/IAT offsets and
the official memcpy-to-memmove tail jump establish the two current SDK routes.
Copied sizes in these sampled in-flight calls are64KiB–3.59MiB for resolves
and4–212KiB for uploads. They are not call counts, total volume or recoverable
frame-time fractions. The analyzer excludes transient size states and never
searches arbitrary stack words for plausible addresses.

Exact Unix scan-body frame gives the return at[RSP+56], agreeing with[RBP+8].
All36 returns follow the checked NtProtectVirtualMemory call through the
frame-restoring get_committed_size tail jump. Its saved caller RBP and exact
local slots also recover the actual rounded request and new protection:

| Requested protection | Rounded request | Samples |
|---|---:|---:|
| PAGE_READONLY |4KiB|27|
| PAGE_READONLY |8KiB|6|
| PAGE_READONLY |16KiB|1|
| PAGE_READONLY |56KiB|1|
| PAGE_READONLY |128KiB|1|

The separate R13 scan bound is the remaining mapped view, capped by an
unlimited input in this exact old path. No writes/queries are bypassed.
The saved Windows stack lies outside the bounded Unix stack capture, so the
exact SDK caller for these protection samples remains unproven. Do not label
every sample as physical-watch arming solely from its PAGE_READONLY value.

## Evidence, review and preservation

- Private return: private/evidence/M7-016/current-caller-return-20260906;
 40files/35,577,584bytes, all bounded regular archive members rehashed.
 Archive76EEB2222578B96F5693A99C521374F482E0F762B3ADB2BC7F46B39208BA502A;
 inventoryDC3E4887459C1DE07E278BFC8A3A1E8518E375CFA5D7363FEF77DC791ED54B7F.
- Helper957B3559451FFCAFE8376B044D3A4D576BA5D53F84FDE389A6E3190D2E480828;
 raw stackDEA8EAB44D8BBAEA0EE1A0F10A7CB6C955F3115FDA80E48147BF2B888FC5CA0A.
- Original perf callgraph text omits leaf IPs. Separate immutable-raw rendering
 with `perf script --hide-call-graph` matches every raw sample tuple:
 supplement36E4742EE1388C978FF6F7C0A9C385F6B64CB76EFF4433E53D3A843D48C2688E.
 Initial unsupported `--no-call-graph` was a read-only render failure, corrected
 without repeating the runtime. Original return/archive remains immutable.
- Reproducer: private/evidence/M7-016/analyze-current-callers.py,
 output current-caller-analysis.json. It first revalidates the frozen Type-0
 analysis and current symbol provenance; then validates raw/header/text,
 lifetimes, instructions, bounded stacks and exact caller chains.
 Final analyzerA728F05DF66D74FBCB61724D515B8B548DD15525806E0092CF1B589B3D6708C4;
 outputBBDC3E9E3C66FD9869F316D251EF945265028F3235DD8409000473EF2E384D53.
- Independent review reproduced archive/identity/raw sample counts and
 current caller analysis. No private stacks/maps/frames/saves/binaries in Git.
 Disk/map provenance is not live anonymous-code or IAT-byte attestation.
- Installed Proton10 static evidence: version1785139158/proton-10.0-4b;
 Unix ntdllFD63E771B1C4BBE65CEA6BB5A0C672C0C686CCF1EFA5AC01CFF40319651FD7A3;
 scriptF07E19D289A4C7A1A9047BB123A2F630B54CEEF1EE2BA0D96D1990A68055635E.
 This capture selected the subsequent runtime comparison. That comparison is
 now [completed separately](M7-016-proton-runtime-comparison.md): newer runtimes
 reduce protection cost, without establishing stock playability or a GE-only
 advantage. Residual profiling on fixed official10 is the next investigation.
- No task closed, version bump, SDK promotion, normal publication or push.
