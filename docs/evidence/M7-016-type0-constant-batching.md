# M7-016: Type-0 constant batching experiment

Date: 2026-09-06. Primary plus independent parser/backend, lifecycle and
offline-attribution reviews. M7 remains 0/16; KI-027 stock cadence/audio open.
This is a default-off, uncommitted SDK experiment, not a published FPS fix.

## Decision and scope

The targeted GPU-command work decreases in the leaf profiles, but this single
A-B-A does not establish a repeatable cadence improvement. Keep the option
off in normal use. Do not replace the Syncthing bundle or promote test saves.
Residual profiles now identify bulk memory movement and Proton protection
metadata scanning below. Next bind their current callers and volume before
selecting another change. No repeated owner START/F10 test or long soak is requested.

The source caches `gpu_type0_constant_batch` once at command-processor creation;
the CVar is InitOnly and defaults false. Enabled Type-0 contiguous spans use
the existing wrap-safe bulk reader/writers only for float4000-47FF,
fetch4800-48BF, and bool/loop4900-4927. Single/repeated writes, holes, mixed
families, scratch/gamma/coherency/other side-effect registers retain scalar
semantics. Packet length checking remains before mutation; no equal-value
skipping, guest clock, readback or native protection change.

## Exact candidate versus normal publication

- Parent source0.18.0.0 /201c3a63895a8431cc68c3217c3eb1da5631e93b.
- SDK base0.19.0.0 /c2e67161f1207ddcee609507c399efe7a1981b72 plus five
  identified experimental files. No new SDK tag/version/install promotion.
- Host974392DD9C7D60578AC2BCA50E24317FD4F1F2E2EE19CD989219B72888B64164.
- Runtime2953D368F57EDE6D5EC8C5378F14129D0E0178FE7A5DD918A4639D4EC0A18E6A.
- GPU18A33A9974C9D6015F810BCC70F6D9580B76C81D5B2C1E9673CD5ACE67A8BCF3.
- Private prefix: private/evidence/M7-016/type0-sdk-install-20260906.
  The diagnostic host tree out/build/win-amd64-readback-cost is configured
  against THIS prefix; it is no longer the clean installed SDK19 host tree.
- Exact nine-file closure, dirty-source hashes and official CRT pins:
  private/evidence/M7-016/type0-batch-package-20260906/binary-pins.json.
- Normal0.16.0.1 bundle/current.txt, Steam shortcut and installed clean SDK19
  remain unchanged. Only binaries/hash metadata/helper went outbound; Deck
  copies its existing complete profile and cache locally for each arm.

Remote experiment root is /home/deck/MCLA-Diagnostics-20260905/type0-batch-20260906,
outside the entire Syncthing root. Windowed1280x720 Desktop Vulkan, fast
readback, same Proton/prefix/official CRT, no clock/affinity/global overrides.

## Review and tests

- Independent code review compared BOTH Vulkan and D3D12 dirty masks, partial
  fetch groups, float vertex/pixel boundary, bool/loop boundary, ring wrapping,
  exact-end offsets and late CVar registration. No blocking finding remained.
- Nine CPU-only tests exercise the actual parser/base writer:11,968 assertions.
  They do not execute renderer SIMD or GPU dirty masks; native controls below
  supplement source review, not exhaustive backend equivalence proof.
- SDK AST scan and1 rule fixture pass. Full all-target RelWithDebInfo and
  Release each pass1,865 executed tests plus4 known BitStream skips; zero fail.
  Existing unused-parameter warnings from direct CPU fixture compilation remain.
- Host rebuilt against the private matching SDK; serializer133/0 passes.
- Final primary structural scans pass in both repositories; root3 and SDK1
  rule fixtures pass again after the source/evidence review. Source delta
  hashes still equal the five recorded experimental files; no version bump.
- Both Windows Vulkan and D3D12 enabled controls load the complete progressed
  profile, reach gameplay/PAUSE/Options, exit0 with WM_CLOSE and no force,
  source profile unchanged. Primary inspected gameplay and PAUSE for BOTH.
- Harness review fixed early-exit child ownership/cleanup, cross-arm identity,
  atomic sample publication and phase triggering before execution. Exact
  executable+current-arm arguments, PID/start/UID plus pidfds bind ownership;
  created sessions remain discoverable after leader exit. No name/group kills.

## Deck A-B-A results

All three use the SAME candidate binaries and copied seed/cache/settings.
The cached flag marker is verified once per arm. Each exits0 without forced
cleanup, errors or surviving owned processes; source and published hashes match.
Primary inspected all3 gameplay and all6 pause endpoint images: actual saved
Eclipse/HUD and PAUSE, not PRESS START. Dynamic traffic/lighting differs; the
viewpoint is comparable, not a bit-identical or moving-gameplay workload.

| Arm | New outputs /20s | Outputs/s | Draw packets/output | Target GPU leaf samples /total |
|---|---:|---:|---:|---:|
| A, off |251|12.55|5197.05|90/1054 (8.54%)|
| B, on |258|12.90|5360.90|24/1045 (2.30%)|
| A, off repeat |237|11.85|5317.37|89/1066 (8.35%)|

B is2.79% above the first A; the two off arms differ by5.58%. Neither the
small apparent gain nor unchanged total GPU-thread sampling establishes a
usable FPS recovery. Resolve waits are zero in all intervals; copy cost is
4.76/4.73/4.77ms per output. Timers overlap and cannot be summed as a budget.

Each CPU recording is15 seconds, userspace task-clock99Hz, no inheritance,
no kernel samples or call graph. It starts after the pause-before frame and
is observed complete before pause-after, but exact containment within the
20-second counter interval is deliberately UNPROVEN. Do not normalize these
two windows as identical work or infer motion/audio/GamingMode acceptance.

Raw/text/header consistency validates3174/3154/3221 samples. Stable process,
thread and image identity is required; two truncated GPU thread names exist,
so selection additionally requires unique packet-handler samples on the
identified command thread. Other same-name thread totals are reported.

Exact own PE unwind bounds plus code/pdata/layout-identical private relinks
bind names ONLY at exact MAP starts; unresolved/aliased functions stay honest.
The six target ranges are disjoint before summation. Scalar base/Vulkan/Type0
samples are45/31/14,14/2/5,41/25/21. Bulk adds0/3/2; shared byte-swap helpers
have4/12/3 samples without caller attribution. Thus the targeted sampled work
decreases, not necessarily every cost of the replacement route.

Remaining B leaves include UpdateBindings65, FindOrCreateTexture28 and
GetView23. Original RADV180, anonymous executable162 and Unix ntdll158 buckets
are refined below without changing the frozen analysis or raw receipts.

## Residual attribution from the same retained recordings

The private CRT extension reruns the complete hash-bound baseline analysis,
compares its full output to the frozen JSON, then matches the exact official
vcruntime140.dll export starts and non-overlapping own pdata ranges against
stable header/maps and executable PE layout. Anonymous code ownership is a
layout/header inference, not a fresh live code-byte attestation. Independent
review reproduced all counts. Optimized Python is explicitly rejected; neutral
RVA bins avoid pretending the script independently disassembles copy loops.

| Exact leaf | A off /1054 | B on /1045 | A repeat /1066 |
|---|---:|---:|---:|
| Official CRT memmove |146|152|151|
| All official vcruntime140 |150|154|155|
| Unix ntdll get_vprot_range_size |78|83|75|
| Unix ntdll syscall dispatcher |63|55|66|
| All Unix ntdll |153|158|156|

Memmove is13.85%/14.55%/14.17% of the identified GPU-thread leaf samples,
not the same fraction of recoverable frame time. Its exported RVA120D0 and
pdata end1273D are exact; primary also inspected the pinned AVX disassembly.
The old75-sample SSE2 caller proof belongs to an older builtin CRT/SDK and
must not be relabeled as current-candidate caller evidence. Current leaf
profiles do not distinguish readback, upload, constant copies or call counts.

Independent Unix ELF review verified SHA2C533727F0D4B1A1F3349D624CABC7A933C27EB79F62FAF505F5A2BDB47E3D53,
stable mapped inode7241718, process/thread lifetime and raw/text equality.
File offsets translate through executable PT_LOAD to nonzero-sized STT_FUNC
bounds, not nearest symbols. All153/158/156 Unix ntdll samples resolve uniquely.
Other leaves include NtQueryPerformanceCounter7/4/4 and Unix-call dispatcher3/12/7.
The protection scan is real sampled work, but without callers it cannot all
be charged to watch disarm; that inclusive cost remains separately bounded.
An IP after xsavec64 in the syscall dispatcher is not proof that one
instruction consumes the sampled time: skid and absent caller stacks remain.

Primary static disassembly additionally finds a tail-call via get_committed_size;
its callers include fill_basic_memory_info, get_working_set_ex and
NtProtectVirtualMemory. Thus the scan name alone cannot distinguish query
from protection changes. Existing returned `.partial/state.json` forensic
scalars provide a useful bound without requesting another F10:

| Whole-run scalar, not the 15s CPU interval | A off | B on | A repeat |
|---|---:|---:|---:|
| Successful-write fault query, seconds |0.193494|0.212374|0.196908|
| Counted successful-write faults |111571|132181|121740|
| Native watch arm, seconds |21.828194|23.322938|20.811128|
| Native watch disarm, seconds |7.626858|8.774257|7.869933|

These are final incomplete diagnostic states with reason unexpected-guest-exit,
not accepted completed F10 packages or a new crash verdict. The independently
verified probe receipts still report intentional exit0/no force. Whole-lifetime
scalars cannot be normalized to the CPU/counter windows; read/failed faults are
excluded, each completed query truncates microseconds and nested spans overlap.
They favor investigating protection changes before the measured successful-write
query path, not bypassing VirtualQuery or claiming dynamic caller proof.
The three already-returned state hashes are A74982A24CAFCAEE3AB0AE65BDC4BF3F2B0FC7FD5C56E5BC6F22BEA99008A18C,
7AF48F35E6A1F584EEDC649216BCD53DAE6F89AEF5136411BBEE45B3D0254B07,
8ADF37EB2A6F04D63B11FF839EC1044187066A9D8247D4401D67150CE885B300.

RADV still has only a mapping-level bucket157/180/157. The current installed
driver is stripped except three public ICD exports; bounded local debug-file
checks found no matching internal-symbol file. Its current build ID is
4e8b447161847824beb1ccd29bf2c496b62b4137, but retained perf buildid-list exposes
only vdso. Do not silently turn a current disk identity into capture-time
code attestation. No driver install, global override or new owner run occurred.

Private reproducible CRT extension: analyze-type0-crt-leaves.py,
SHA87FF685EB187A774689F706BF80461BFD9C1E7DEA8AAF78047B6FD7F5A72C766.
Run with Python -B (not -O); JSON goes to stdout and original evidence is read-only.

## Follow-up protocol and later results

The protocol below was subsequently executed and accepted in
[current caller attribution](M7-016-current-caller-attribution.md). The
[runtime comparison](M7-016-proton-runtime-comparison.md) then confirmed lower
protection cost on newer Proton, followed by the accepted
[Proton10 residual profile](M7-016-proton10-residual-profile.md). These later
results supersede pending wording in this historical protocol; the original
Proton9 Type0 cadence verdict above remains inconclusive.

Reuse the exact private candidate with batching OFF and the same copied seed/cache.
The next control should reach visually verified saved gameplay and PAUSE by the
existing automatic probe; it is not an owner test or a benchmark of a new build.
First identify the current command thread by exact packet-handler samples and
stable PID/TID births, not the ambiguous truncated thread name. Then take one
short userspace-only caller recording (existing 8s/99Hz/4096-byte stack ceiling),
with exact binaries, maps and before/after lifetime checks. Keep raw registers,
stacks and maps private; no kernel scope, full-memory dump or global settings.

Review this new collector before execution, preserving identity-bound cleanup
and source/save/cache hashes. Treat it as caller attribution, not an FPS test:
sampling overhead and a longer phase differ from the accepted A-B-A. For copy
loops, validate unwind/return location and exact SDK call/import chain; never
scan arbitrary stack words for a plausible return address. For Proton scans,
prove a caller chain if available, otherwise retain unknown rather than guess.
For the exact retained Unix ntdll, primary verified the stable scan-body frame:
six pushes plus8 local bytes leave the return at RSP+56 (also RBP+8).
get_committed_size restores its own frame before tail-jumping into that scan.
Exact call returns70226 and76E0D distinguish fill_basic_memory_info from
NtProtectVirtualMemory only when the sample is in the verified stable body,
register/stack bounds agree and the return/call bytes are checked. Keep other
callers separate. This planned discriminator is not a captured caller result,
and protection-route evidence would still not distinguish arming from disarming.
Do not reuse old CRT stack offsets or old GPU TIDs. Existing SDK query/arm/disarm
counters can supplement this; do not add redundant generic counters first.
If driver symbols become necessary, require exact build-ID/debuglink provenance
before assigning internal RADV names. No new library or runtime promotion yet.

## Private evidence and preservation

- Verified return: private/evidence/M7-016/type0-return-20260906,
 93 files /73,118,882 bytes. Archive704C932DFE6D11A802FB42856D851D702D3BEFBB67F6B78957EA5E6A74D9F745;
 inventory0B9D0431762C4A6BA72BDD2916CCB1649624881760B8E7A514214B3AA66B2FD2.
- All bounded regular archive members were hash-checked before and after
  extraction. Original visual_validation_pending receipts are immutable;
  primary visual review is recorded here, not retroactively in raw evidence.
- Private analyzer44AFCEB31A4DAB1DC47463A09209C371AF06B6216D1A376FC2DCEBFF31AD57F7,
  result1059605986E59A181C39D1CE77E5FC510F5A2F5C556485000CD08859754CC7BE.
  Primary independently reran and matched the complete frozen result.
- Full owner profile/album remains3BBC44D0E0CBE03633DFA3BFFBC776B0C5E62CFDC5AA34BEFB346DE022322B5A;
  no experimental profile promotion. Existing owner-return archive remains.
- No game/guest-generated data, frames, stacks, maps, profiles or binaries in
  Git. No task closure, no normal-bundle publication and no push.
