# l113 SDK: deferred claims for the draw extent bound (KI-084) and the verify query budget (KI-085) (2026-11-19)

**SDK READY: `private/l113-sdk-install`** (CMake version `0.20.0.20-dev.g9595fc4`). Local commit `9595fc4`
(`9595fc40a138e5ac69b1aad49a0ce1365963c27b`) on the branch `mcla/m11-l113` (parent `3173c2e` = l112), worktree
`private/l113-sdk-wt`; not pushed, no tag. Implemented by an Opus subagent for the primary agent.
**Built and unit-tested only: nothing has run in the game, there is no physical PASS.** Spark not
used: the work was source reading, a stack symbolisation and deterministic code checked by unit
tests; a model answer would cost more to check than to write.

Licence: no code was copied; the new headers and tests are our own. Nothing was taken from GPL or
unlicensed sources.

## 0. Result

| Item | Status |
|---|---|
| F1 KI-084 fix | **shipped** as part of `d3d12_draw_extent_from_rect`: deferred claims (section 1); escape `d3d12_draw_extent_deferred_claims` (default true, hot) |
| F2 resolve audit | **shipped**: `MCLA_XFER_BELOW ... kind=resolve ... resolve_top=` (16 rows, own table of 32) and the first 32 `MCLA_XFER_BELOW_RES v=1` events with the resolve's surface, address, length, rectangle and destination format |
| F3 KI-085 TDR | **cause not found** in the log or the source; no concrete defect in the verify queries (section 3). Guards: `d3d12_verify_query_every` (1), `d3d12_verify_query_frame_budget` (64), the ring check made explicit; `MCLA_CP_FREEZE v=1 kind=gpu` |
| F4 tests | 6 new `[l113]` cases (the deferred-claim state machine) and 4 query-budget cases; the 4 `[l112]` cases updated for the separate resolve tables |
| Default of the bound | the SDK default stays false; the l113 host keeps it OFF (owner build doc, section 4 here) |

## 1. F1: the rule and why it restores the unbounded semantics

**What the bound changes** (l112, section 1 of the l112 document): a bounded clear by render target
K claims only the rows down to its rectangle. The tiles of the old extent below the bound keep their
owner A. Without the bound, K claimed them too: the transfer A -> K, then K owned them. The l112 audit
records exactly these tiles as "left" ranges (K, A).

**The invariant.** While a range (K, A) is alive, A still holds the data it had at the bounded draw:
A writes those tiles only through a claim (its own `Update` or a resolve clear), and every claim of
the tiles ends the range (`OnClaim` cuts it). Every ownership change goes through `ChangeOwnership`;
the only other writers of the ownership map are the EDRAM snapshot restore and
`DestroyAllRenderTargets` (handled below) and the pixel-shader-interlock barrier (never with the
bound). So "complete the claim now" (transfer A -> K, K becomes the owner) produces exactly the state
the unbounded draw produced, as long as it happens before anything reads the tiles in a way that
differs.

**Real and audit ranges.** A range is *real* (a deferred claim is owed) when the bound was applied
and `d3d12_draw_extent_deferred_claims` was on at that draw; with the verify (old extent applied) the
ranges are *audit* ranges: the same classes are counted, nothing is completed.

**The rule** (`include/rex/graphics/util/extent_below_audit.h`):

| Reader of a real range (K, A) | Unbounded | Bound (l111) | l113 |
|---|---|---|---|
| resolve copy | dumps K's copy (A -> K) | dumps A | **complete first** (A -> K), then the resolve; the read part converges (removed) |
| K claims the tiles (`draw`) | nothing (K had them) | A -> K now | as l111 (A unchanged: the same data) |
| another target R claims (`transfer`), a resolve clear (`rclear`), A takes them back (`owner`, R = A) | A -> K -> R | A -> R | **exact chain**: as l111; otherwise **complete first** (A -> K), then R claims from K |
| a newer bound K2 leaves the same tiles | A -> K -> K2 later | (K2, A) | exact chain (R = K2): (K2, A); otherwise complete A -> K, then (K2, K) |
| capacity (512 ranges) | - | oldest dropped | oldest real range completed |
| full EDRAM dump (trace) | - | - | every real range completed |
| EDRAM snapshot restore | every tile overwritten | - | ranges dropped, queued completions discarded |
| `ClearCache` | K owns the tiles: kept | K may be deleted | owed clearers and the render targets of queued completions kept |

**Exact chain** (`BelowChainExact`): A -> K -> R equals A -> R when one hop is the identity: K has
A's kind or R's kind, kind = depth or colour, resource format and MSAA sample count (the key bits
19-25). Pitch and base do not change a tile's bits (the transfers address EDRAM tiles). Any other
combination is treated as lossy: a format change (colour <-> depth, different depth encodings, 7e3 or
16-bit host formats) or a sample-count change on both hops. This is conservative: a 4x <-> 1x
transfer at the same format may well be bit-exact (it maps EDRAM samples one to one), but that is not
proven here, so such chains are completed.

**Why the resolve case is exact.** The resolve reads the EDRAM layout of the owner of each range.
After the completion that owner is K holding A -> K of A's unchanged data, i.e. what the unbounded
draw left; the resolve is then byte-identical to the unbounded one, whatever the formats.

**Implementation.**
- Common `RenderTargetCache` (`cache.cpp`):
  - `Update`: the bound's deferred flag latched at the top; after the draw's own claims every
    pending left range is recorded with `Leave(..., real, &completions)` and the completions
    performed.
  - `ChangeOwnership`: before a claim, `OnClaim` returns the lossy chains of real ranges;
    `PerformBelowCompletions` claims each for its clearer (a nested `ChangeOwnership` with the
    audit bypassed) and queues the transfers per clearer in `deferred_claim_transfers_`; then the
    claim proceeds and takes the tiles from K.
  - `AuditResolveRead` completes the real ranges a resolve span reads (from the first to the last
    tile read in each range); `CompleteAllDeferredClaims`; the snapshot restore drops them;
    `ClearCache` keeps the owed clearers and the queued transfers' targets;
    `DestroyAllRenderTargets` drops everything.
- `D3D12RenderTargetCache::FlushDeferredClaimTransfers` performs the queued transfers, one
  `PerformTransfersAndResolveClears` call per clearer, outside the draw's own transfers
  (`xfer_in_update_` false: no l108 elision cut, no extent counters; the completion transfers are full
  because the bounded draw never wrote those rows). It runs before an `Update`'s transfers, after the
  completions of a resolve copy and before the copy (direct or dumped), after the resolve clear's
  claims and before its transfers, and before the trace dump.
- Readers enumerated: draw claims (`Update`), resolve clears (`PrepareHostRenderTargetsResolveClear`),
  resolve copies (`Resolve`, both `TryResolveCopyDirectly` and `DumpRenderTargets` read the same
  span), the trace dump (`InitializeTraceSubmitDownloads`), the snapshot restore (a writer), host
  depth stores (inside transfers, which read only what the source owns). Textures cannot read the
  EDRAM directly on Xenos; draws only write inside their claimed extent; the l101 memo never skips a
  claim (it bumps on every ownership change).

**Counters.** `MCLA_XFER_EXTENT` gains, after the l112 fields:
`deferred=on:,resolve:,claim:,exact:,leave:,overflow:,flush:,restore:,missing:,xfers: deferred_tiles=...`
(`on` = the bound applied with deferred claims; with the verify the same classes count what would
be done; `missing` = a clearer whose render target is gone, expected 0; `xfers` = the transfers the
completions recorded), and with the verify ` vq=every:,budget:,cand:,granted:,sampled_out:,over_budget:,ring_full:`.

**Expected saving with the fix (from the l112 audit, session 75035b51, minute 2).** Of the 12 most
frequent reads of left tiles (`MCLA_XFER_BELOW top=`), the 4 `draw` rows (8,685 reads, 2.1 M tiles)
are exact and the 8 others (19,466 reads, 7.75 M tiles) are lossy under the rule:

| row | reads | tiles | class |
|---|---|---|---|
| transfer d1m4p12b0 > d1m1p12b0 > d0m4p16b0 | 3,612 | 2,167,200 | lossy (MSAA, format) |
| transfer d0m4p6b168 > c0m4p6b168 > d1m1p12b0 | 2,283 | 1,873,056 | lossy |
| owner d1m4p12b0 > d1m1p12b0 | 2,130 | 173,180 | lossy (MSAA) |
| transfer d1m1p12b0 > c0m1p12b408 > d1m4p12b0 | 2,009 | 819,672 | lossy |
| transfer d1m4p12b0 > d1m1p12b0 > c0m1p12b408 | 2,009 | 819,672 | lossy |
| owner d0m4p6b168 > c0m4p6b168 | 1,915 | 11,490 | lossy |
| transfer d1m1p12b0 > d0m4p6b168 > d1m4p12b0 | 1,889 | 1,825,776 | lossy |
| transfer d1m4p12b0 > d1m1p12b0 > c1m1p2b977 / c3m4p13b416 | 3,619 | 57,904 | lossy |

So with the conservative rule the completions copy almost every tile the bound skipped, at the same
cost as without the bound: the l111 saving (GPU 13.4 against 18.7 ms per frame) is expected to be
mostly lost with the fix. If the MSAA hops were proven exact (only the depth/colour kind and format
counted), 5.5 M of those 7.75 M tiles would become exact again (all rows except the two
`d0m4p6b168 > c0m4p6b168` ones). That proof (the transfer shaders' sample mapping at resolution
scale 2) is the next lever, not done here.

## 2. F2: the resolve-class audit

- Resolve and resolve-clear signatures go to their own table (32 signatures, `resolve_top=` lists
  16) and no longer compete with the draw/transfer/owner rows (in l112 the shared table of 64
  overflowed by 80 and the resolve rows never showed).
- The first 32 resolve-class events of the session are kept apart from the first 32 claim events:
  `MCLA_XFER_BELOW_RES v=1 read=resolve|rclear clearer= owner= reader=res@<source key> tiles=<start>+<n>
  age= deferred=done|owed|exact|skip src=<source key> addr=0x<copy dest base> len=<bytes> rect=x,y,wxh dest_fmt=<copy dest format>`.
  `deferred=done`: completed before the read (bound on); `owed`: an audit range whose claim the fix
  would complete; `exact`/`skip`: a resolve clear with an exact chain.
- The resolve reader key is the resolve's source surface as a render-target key (base, 32bpp pitch,
  MSAA, depth, resource format), so the minimap targets read as `res@c0m1p2b945`, `res@c1m1p2b977`.
- New line after `MCLA_XFER_BELOW ... top=`:
  `MCLA_XFER_BELOW v=1 reason= seq= kind=resolve sigs= overflow= resolve_top=...`.

## 3. F3: KI-085, the device hang in the verify arm

**The log** (session 75035b51, arm pc/extent-verify2, the tail read line by line from 11:53:10): only
ordinary periodic lines (time of day, pursuit state, livery diagnostics) until
- 11:53:17.192 `MCLA_HITCH frame=4844 ms=114.7 cause_hint=gpu cp_busy_ms=113.45 strict_wait_ms=100.22
  gpu_busy_ms=10.29 draws=4005 subm=3 lead=2`: the CP waited 100 ms on the GPU (the strict lead
  bound) with only 10 ms of measured GPU work: the GPU had stopped completing submissions;
- 11:53:18.204 `MCLA_CP_FREEZE kind=begin episode=1 stall_ms=1012 cp_state=ring_idle copy_active=0
  log_queue=0 log_dropped=0`: the CP had no ring data and was waiting;
- 11:53:21.026 `D3D12 device removed: HRESULT 0x887A0006 - DEVICE_HUNG`.
No line about queries, heaps, fences or submissions in between (the verify lines are per minute; the
last minute window, 11:52:59, had `check=n:65003,ok:65003,bad:0,lost:0`, `MCLA_XFER_ELIDE ...
check=n:42379,ok:42379,bad:0,lost:0`, `MCLA_GPU_CAT ... dropped_frames=0 ts_pf=2033.0`).

**The stack, symbolised** (`llvm-symbolizer` against `private/l112-sdk-o/Release`, the DLL hashes
equal to the staged l112 files; return addresses minus one):

| frame | function |
|---|---|
| ntdll.dll+166F34, KERNELBASE.dll+1AB53/+1AAD3, ntdll.dll+D3194 | the wait (WaitForMultipleObjectsEx) |
| rexgpu-xenos.dll+E42EA | `D3D12CommandProcessor::WaitIdleForDeferredMemoryWrites` (`command_processor.cpp:4337`, the bounded 5 ms `WaitForMultipleObjectsEx` on the fence event) |
| rexgpu-xenos.dll+1DEE5 | `CommandProcessor::WorkerThreadMain` (`command_processor.cpp:403`, the idle loop) |
| rexgpu-xenos.dll+25A4D | `CommandProcessor::Initialize` worker lambda (`command_processor.cpp:243`) |
| rexruntime.dll+2E24ED, +2E5303 | `XHostThread::Execute`, `XThread::Create` lambda |
| rexruntime.dll+C7557 | `rex::thread::ThreadStartRoutine` |
| rexruntime.dll+C69AA, +C68AE, MSVCP140.dll+129CC, ucrtbase.dll+50D9, KERNEL32.DLL+2DDA7, ntdll.dll+C9C5C | stale words of the stack scan (`MaybeYield`, a string destructor in `set_thread_name_impl`) and the thread start |

So the CP thread was idle, waiting for the GPU to pass the fence of a deferred guest memory write.
It is the victim: the GPU did not complete a submission for about 3.8 s (from about 11:53:17.1 to the
removal at 11:53:21.0; the Windows TDR limit is 2 s of a single GPU command plus detection).

**The verify arm checked against the source:**
- Load in a race: 65,003 bounded draws per minute over 2,293 frames = 28.3 per frame, each issued in
  two scissored parts with one occlusion query and one `ResolveQueryData` of one slot on part 1;
  42,379 elision checks = 18.5 per frame (one query on part 0); category timing every 4th frame,
  2,033 timestamps per sampled frame (budget 8,192, capacity 32,768, `dropped_frames=0`).
- Heaps: 1024 occlusion slots each for the extent and the elision verify, used as FIFO rings; a slot
  is allocated only while fewer than 1024 results are pending (`pending.size() < capacity`), so no
  slot is reused before its result was read: **no wrap onto unresolved entries** (now explicit in
  `VerifyQueryBudget::Request`, tested).
- Nesting: the elision query is opened and closed around part 0, the extent query around part 1;
  with a guest occlusion query open neither is issued (`lost`). **No nesting.**
- Command-list boundaries: Begin/End/Resolve are consecutive deferred commands of one draw; a
  submission cannot end between them. **None left open.**
- Read-back: persistently mapped readback buffers in `COPY_DEST`, read after the submission's fence:
  legal.
- GPU time: the two parts of a draw have disjoint scissors that add up to the original one, so no
  sample is shaded twice; the extra cost is a second rasteriser setup, one query and one small
  resolve copy per bounded draw (about 47 resolves of 8 bytes per frame) plus 2,033 timestamps on
  every 4th frame. A per-query `ResolveQueryData` can make a driver wait for the query inside the
  frame, which slows the frame, but it cannot by itself run for seconds.
- **Finding:** no defect in the verify queries; the cause of the hang is not known. It happened once,
  at a race start, under the heaviest verify load; the default arms never showed it, and a laptop
  GPU or driver fault cannot be excluded.

**Guards added (KI-085):**
- `d3d12_verify_query_every` (int, default 1, read every frame): the extent and elision verifies
  measure every Nth candidate. Key `verify_every4` (N = 4).
- `d3d12_verify_query_frame_budget` (int, default 64, read every frame; 0 = unlimited): at most 64
  queries per frame for each verify; further candidates are skipped and counted. The l112 race load
  (28 and 18 per frame) is under it, so the default arms measure as before.
- The counters `vq=every:,budget:,cand:,granted:,sampled_out:,over_budget:,ring_full:` on
  `MCLA_XFER_EXTENT` and `MCLA_XFER_ELIDE` (verify only).
- `MCLA_CP_FREEZE v=1 kind=gpu episode= signaled= completed= behind= removed=0x...` after the
  freeze stack: the last submission signalled on the queue, the fence's completed value and the
  device removed reason (free-threaded calls). `behind>0` with the CP idle means the GPU, not the CP,
  is stuck. The last GPU category cannot be printed cheaply (it would need GPU breadcrumbs); the
  CPU-side category is not where the GPU is, so it is not printed.
- The heavy verify arms (pc/extent-verify2, pc/verify-gpu, pc/extent-verify and the amd
  verify-gpu / extent-verify arms) stay out of races until the hang is understood; the safe audit is
  the new pc/extent-audit (extent verify, trigger profile, every 4th draw; no elision verify, no
  category timing).

## 4. Default of the bound

The SDK default of `d3d12_draw_extent_from_rect` stays false. The l113 host keeps the l112 entry
(false). Reasons:
- The fix restores the unbounded semantics for resolves by construction and is unit-tested, but it
  has not run in the game: whether the white square is gone is Run 3 of the checklist.
- By the l112 audit (section 1) the conservative rule completes almost every tile the bound skips,
  so the expected saving with the fix is small: turning the bound on by default would add risk for
  little measured gain.
- What is missing for ON: Run 3 without a white square and with `deferred=...missing:0`, and either
  a measured gain from the `deferred_tiles=exact:` share or the proof that MSAA-only hops are exact
  (which would keep most of the saving).

## 5. Tests, build, install

- `tests/unit/graphics/extent_below_audit_test.cpp`: the 4 `[l112]` cases (top now without the
  resolve rows; the resolve row in `resolve_top`; the claim and resolve events apart; audit ranges
  not completed) and 6 new `[l113]` cases:
  - the exact-chain rule (same kind as A, as R; pitch and base ignored; MSAA or format on both hops
    lossy; the owner taking back through another sample count lossy)
  - a real range completed before a resolve of the 977 minimap surface (the completion, the converged
    part removed, a second resolve reads nothing owed, the event line with `deferred=done` and the
    resolve's surface, address, length, rectangle, format; the same as an audit range: counted, not
    completed)
  - lossy chains complete and exact chains keep the saving (transfer exact, transfer lossy, owner
    lossy, the clearer itself, a resolve clear; the remaining tiles; an audit range)
  - a newer leave over a real range of another clearer (lossy: A -> K1 completed, the new range owed
    by K1; exact: one range owed by A; the same clearer renewed)
  - capacity (the oldest real ranges completed), snapshot restore (dropped), full dump (completed;
    audit ranges stay), the exact `deferred=` fields
  - resolve-class signatures and events kept apart (80 transfer signatures do not hide a resolve)
- `tests/unit/graphics/verify_query_budget_test.cpp` (4 cases `[l113]`): every candidate and the
  full ring, every Nth, the per-frame budget and the next frame, the race load of l112 with the
  light arm (7 of 28) and the default budget (28 of 28).
- Build: fresh `private/l113-sdk-b` with the l112 options (Ninja Multi-Config Release, clang 20.1.8,
  VS 2022 build tools, FFX presenter shaders, Tracy and perf counters on, Vulkan off), output
  `private/l113-sdk-o`; third-party trees copied from `private/l112-sdk-wt`. Normalized warning set:
  the first full build had one new warning (`extent_below_audit.h`: missing field `detail` initializer, -Wmissing-field-initializers); fixed, and the rebuild of every translation unit that includes the header had none. Union of the full, post-fix, post-format and install logs without that fixed entry: 211 entries, **0 new, 0 gone against l112**.
- `git-clang-format` against `3173c2e`: applied (`-f`, three files), `clang-format -i` on the new
  files; then "clang-format did not modify any files". `git diff --check`: clean.
- ctest (full suite, `-j 8`): **2376 of 2376 passed** (l112 2366 + 10 new) three times in a row after the warning fix (`ctest-b1..3`) and three times in a row after formatting and the rebuild (`ctest-c1..3`). The unrelated `physical_mapping_test.cpp:184` race did not occur.
- Commit `9595fc4` on `mcla/m11-l113`, parent `3173c2e`; 11 files, 1144 insertions, 86 deletions (`extent_below_audit.h` +410 net changes, `verify_query_budget.h` 100 new, common `cache.cpp` +117 / `cache.h` +40, D3D12 `render_target_cache.cpp` +158 / `.h` +12, `command_processor.cpp` +21 / `.h` +3, the tests +275 and 93 new, tests CMakeLists +1). Not pushed, no tag.
- Install `private/l113-sdk-install`, CMake `0.20.0.20-dev.g9595fc4`; adds
  `include/rex/graphics/util/verify_query_budget.h`.

| bin file | Bytes | SHA-256 |
|---|---|---|
| rexruntime.dll | 11776000 | 1AC2AC54AA90C9A901D0EA34F9CDF16CE66628621B54222560EBE87903373E5A |
| rexgpu-xenos.dll | 3858944 | BDDC7963070E462A2B20BDECD14D476DAFB87599AB9CDED8B7B6F86EE39CE194 |
| TracyClient.dll | 246784 | EA96AA2E69E19FA418C6D23D51D4366061E1860DA7DCE21947108AFD09724B1B |
| rexglue.exe | 4113920 | E2F5AB2C0B0ED189A974100C2F3F2BE7EAC75CB8C453E46B79E30287C445C1B8 |

rexruntime.dll and TracyClient.dll have no source change but new hashes (fresh build tree, version
string).

**ABI:** `RenderTargetCache`, `D3D12RenderTargetCache` and `D3D12CommandProcessor` gained members:
the host must be rebuilt against this SDK. New SDK cvars: `d3d12_draw_extent_deferred_claims`
(bool, true, hot), `d3d12_verify_query_every` (int, 1, hot), `d3d12_verify_query_frame_budget`
(int, 64, hot).

## 6. Expected performance

- l113 defaults = l112 defaults (the bound off): no change; nothing is recorded while the bound and
  its verify are off (no range is ever left; `OnClaim` / `OnResolve` return at once).
- pc/extent-new (the bound with the fix): between l111 (13.4 ms GPU per frame) and l112 (18.7 ms),
  expected close to l112 by the classification above; the `deferred_tiles=claim:` against `exact:`
  share of Run 3 is the measurement.
- pc/extent-l111: as l111 (the flicker included).
- Verify arms: as before with the default budget; with `verify_every4` about a quarter of the queries.

## 7. Risks

- No game run. The fix's correctness rests on the hook coverage (every ownership change through
  `ChangeOwnership`, every resolve through the hooked span) and on the invariant that A is unchanged
  while a range lives; both are argued from the source above, not measured.
- Host-depth precision: under the bound the host-depth owner of a left tile may differ from the
  unbounded one; transfers pick the host depth only when it matches the EDRAM value, so the
  difference is below the 24-bit EDRAM precision. Not counted.
- Tiles owned by nobody (only before the first draws after `DestroyAllRenderTargets`) are not owed:
  both ways hold undefined data there.
- The completion transfers appear in `MCLA_XFER` with the resolve-clear trigger (they are performed
  outside a draw); `deferred=...xfers:` counts them.
- KI-085 is not explained; the guards only limit the verify load.
- Scratch: `%LocalAppData%\Temp\claude\C--BDU-MCLA-Recomp\l113-work`.
