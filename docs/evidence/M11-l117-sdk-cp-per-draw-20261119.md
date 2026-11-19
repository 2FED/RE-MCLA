# l117 SDK: the per-draw CP budget of the daylight dips and three default-off levers (constant buffer reuse, index buffer view cache, narrow texture rebind) with verifies (2026-11-19)

Implementation by an Opus subagent for the primary agent, from the l115 main session
([M11-l115-main-session-20261119.md](M11-l115-main-session-20261119.md): day CP = 2.54 ms +
2.38 us x draws, night 1.26 + 3.04 us; GPU 7.9-8.1 ms + 1.45-1.65 us; CP overtakes GPU at about
5-5.75 k draws) and the l116 day / night run
([M11-l116-pc-gpu-cat-day-night-20261119.md](M11-l116-pc-gpu-cat-day-night-20261119.md): the two
longest dips CP-bound at up to 6.2 k draws; daylight cost is CP-side). The lever pattern follows
[l101](M11-l101-sdk-per-draw-levers-20261114.md) (default off, verify does the full work and
compares, counters by reason). SDK commit `e6eeeb9` (`e6eeeb90931b73b7a27c190cf28036739ccc2ed9`) on the local branch
`mcla/m11-l117`, parent `d889a16` (l116); worktree `private/l117-sdk-wt`. Not pushed, no tag.
Installed to `private/l117-sdk-install` (CMake version `0.20.0.23-dev.ge6eeeb9`). **Built and unit-tested only:
nothing ran in the game, no physical PASS, no number below is measured with l117.** Nothing was
taken from GPL forks or LARecomp; no code was ported from Xenia Canary. Spark not used: the budget
is a deterministic parse of four session logs and the levers are source work checked by unit tests;
no bounded extraction or triage task suited it.

## 1. Phase A: the per-draw CP budget

Sources: every `MCLA_PERF_SPLIT v=1` window of the l115 MAIN session (29865940,
`private/owner-l115-20261119/session`) and the two l116 sessions (c46e0301 = pc/gpu-cat-race, day
and night; 13d779cf = pure defaults with the alt-tab test). Clean play minutes = minute windows
with under 5 % of the wall time paused (the `MCLA menu state` lines) or unfocused
(`MCLA_WINDOW_EVENT`) and more than 1,800 draws per frame; day / night from the last
`MCLA_TOD ... night=` before the window. Dense = F10 windows above 5,000 draws per frame (l115: 2
windows, 6.1 k draws; l116 c46e0301: 1 window, 5.1 k) or play minutes above 4,300 (l116 13d779cf).
The `p_*` phases are sampled (1 in 16) and scaled, exclusive of the regions charged inside them;
`draw_ms` = their sum. Script: scratch `l117-work/budget.py` (output `budget2.log`).

### 1.1 Draw-path phases, us per draw

| phase (IssueDraw section) | l115 day | l115 night | l116a day | l116a night | l116b day | l116b night | l115 dense 6.1 k | l116a dense 5.1 k |
|---|---|---|---|---|---|---|---|---|
| setup (entry, fence poll, BeginSubmission) | 0.090 | 0.097 | 0.095 | 0.107 | 0.086 | 0.103 | 0.094 | 0.082 |
| prim (primitive processor, IB request) | 0.110 | 0.095 | 0.102 | 0.085 | 0.096 | 0.089 | 0.129 | 0.105 |
| rt (modifications, RT memo / Update) | 0.195 | 0.215 | 0.209 | 0.231 | 0.188 | 0.224 | 0.169 | 0.202 |
| pso (translations, ConfigurePipeline) | 0.123 | 0.124 | 0.130 | 0.129 | 0.117 | 0.129 | 0.111 | 0.128 |
| **tex** (RequestTextures) | **0.289** | **0.440** | **0.294** | **0.469** | **0.288** | **0.440** | **0.389** | **0.295** |
| **bind** (PSO set, viewport, system constants, UpdateBindings) | **0.359** | **0.414** | **0.365** | **0.418** | **0.344** | **0.411** | **0.332** | **0.377** |
| vb (vertex residency, memexport) | 0.129 | 0.124 | 0.123 | 0.118 | 0.118 | 0.116 | 0.157 | 0.143 |
| record (IB set, barriers, draw, audits) | 0.076 | 0.082 | 0.078 | 0.083 | 0.073 | 0.080 | 0.082 | 0.082 |
| **phases total** | 1.371 | 1.593 | 1.396 | 1.640 | 1.311 | 1.591 | 1.463 | 1.414 |
| upload region (shared-memory UploadRanges) | 0.491 | 0.555 | 0.491 | 0.554 | 0.470 | 0.564 | 0.640 | 0.501 |
| watch arm | 0.157 | 0.177 | 0.170 | 0.192 | 0.148 | 0.182 | 0.202 | 0.180 |
| write fault | 0.189 | 0.208 | 0.205 | 0.227 | 0.178 | 0.214 | 0.270 | 0.221 |
| watch disarm | 0.050 | 0.060 | 0.053 | 0.059 | 0.046 | 0.058 | 0.076 | 0.060 |
| draws per frame | 3,871 | 2,728 | 3,587 | 2,408 | 3,638 | 2,366 | 6,099 | 5,131 |
| fps | 67.4 | 66.9 | 66.9 | 70.3 | 66.9 | 73.1 | 46.6 | 50.2 |
| CP busy / GPU busy, ms per frame | 11.8 / 13.5 | 9.6 / 12.6 | 11.3 / 13.2 | 8.7 / 12.4 | 10.5 / 12.9 | 8.4 / 12.1 | 19.1 / 16.9 | 15.0 / 15.1 |

Dip seconds (`MCLA_DIP kind=second cause_hint=cp_draws`): l115 178 s (2.9-12.2 k draws, the
highest are menu seconds), draw phases 1.29 us and upload 0.44 us per draw, CP work 18.0 ms, GPU
16.5 ms per frame; l116 c46e0301 37 s: 1.35 / 0.53 us, 14.4 / 15.4 ms; l116 13d779cf 64 s: 1.41 /
0.59 us, 13.1 / 15.5 ms. The dip seconds cost the same per draw as play: the dips are draw count,
not a per-draw anomaly.

### 1.2 Non-draw CP time, ms per frame

| item | l115 day | l115 night | l116a day | l116b day | l115 dense 6.1 k |
|---|---|---|---|---|---|
| type-0 register writes (6-7 packets per draw: about 0.23 us per draw) | 0.90 | 0.70 | 0.88 | 0.82 | 1.26 |
| IM_LOAD (+ hash) | 0.18 | 0.15 | 0.19 | 0.16 | 0.24 |
| SET_CONSTANT / LOAD_ALU_CONSTANT | 0.14 | 0.11 | 0.14 | 0.13 | 0.18 |
| event writes / other type-3 | 0.13 | 0.12 | 0.12 | 0.12 | 0.22 |
| WAIT_REG_MEM (with its waits) | 0.98 | 1.61 | 0.98 | 1.81 | 1.19 |
| copies (resolves) + resolve | 1.38 | 1.08 | 1.41 | 1.19 | 1.45 |
| read-back memcpy | 0.53 | 0.29 | 0.53 | 0.42 | 0.56 |
| present | 0.76 | 0.70 | 0.76 | 0.72 | 0.86 |
| submit + execute + stream finish wait | 0.98 | 0.73 | 1.00 | 0.86 | 1.65 |
| fence wait | 0.12 | 0.32 | 0.15 | 0.09 | 0.07 |
| packets unattributed | 0.39 | 0.26 | 0.32 | 0.31 | 0.60 |

### 1.3 Counters per draw (same windows)

| counter | l115 day | l115 night | l116a day | l116b day | l115 dense |
|---|---|---|---|---|---|
| constant buffer uploads / KB | 1.59 / 1.54 | 1.77 / 1.57 | 1.61 / 1.55 | 1.61 / 1.54 | 1.47 / 1.38 |
| shared-memory requests / uploads / upload KB | 1.79 / 0.20 / 1.78 | 1.75 / 0.23 / 2.19 | 1.78 / 0.20 / 1.80 | 1.71 / 0.20 / 1.77 | 1.84 / 0.24 / 2.35 |
| fetch dwords written / changed | 11.8 / 4.1 | 15.3 / 5.2 | 11.9 / 4.1 | 11.6 / 4.0 | 11.6 / 3.9 |
| texture slots evaluated / key changed | 0.60 / 0.58 | 0.97 / 0.93 | 0.62 / 0.59 | 0.62 / 0.59 | 0.59 / 0.57 |
| float constant dwords written / unchanged | 56.5 / 37.0 | 57.5 / 43.6 | 56.8 / 37.1 | 55.6 / 35.7 | 46.1 / 31.1 |
| draws with no draw-state change / same PSO / same RT key | 0.57 / 0.64 / 0.99 | 0.64 / 0.71 / 0.99 | 0.55 / 0.63 / 0.99 | 0.55 / 0.66 / 0.99 | 0.64 / 0.71 / 0.99 |

The game really changes about 19-20 float constant dwords per draw (about 5 float4: a world
matrix and a few more); 37-44 more are rewritten with the same value and skipped by the l100
register skip (default on since l104: `const_dw_skipped` 408 M per minute in c46e0301).

### 1.4 What "bind" is (source at d889a16, `src/graphics/d3d12/command_processor.cpp`)

From the kTextures mark (5977) to kBindings (6044): the guest PSO handle set when it changed
(5981-5986, cached); the viewport key build and compare (5998-6021, cached); the scissor
(6023-6028); `UpdateFixedFunctionState` (8794-8857: viewport, scissor, blend factor, stencil
reference; every set compares with the last value); `UpdateSystemConstantValues` (8859-9278:
about 30 register reads, per-field compares, one loop over the used textures with two texture-cache
calls each; the dirty flag is exact: `cbuffer_binding_system_.up_to_date &= !dirty` at 9277);
`UpdateBindings` (9306-10100): root signature (9318-9328, cached), the float constant maps
(9361-9392: a map change marks the buffer out of date), the five constant buffer uploads (system
9395, float vertex 9408, float pixel 9447, bool / loop 9487, fetch 9509: each a constant-pool
`Request` at a NEW upload-heap address, the copy, and the root CBV bit cleared so 9998-10022 sets
the root CBV again), the bindless sampler memos and indices (9534-9802; descriptor-index buffers
are uploaded too but not counted in `cb_uploads`), and the root parameter sets (9997-10097, cached
per root parameter by `current_graphics_root_up_to_date_`). Shared-memory residency of vertex
buffers is in vb (6105-6153, cached per vertex fetch slot); the index buffer request is in prim
(`primitive_processor.cpp:931`); texture data requests in tex (`pipeline/texture/cache.cpp:757`).
`SharedMemory::RequestRanges` (`shared_memory.cpp:683`) takes a lock-free fast path when every page
is valid (773); only real uploads (880-889) are the upload region.

### 1.5 The top three costs and what is recomputed although its inputs did not change

1. **Shared-memory uploads and memory watches: 0.85-1.2 us per draw** (upload 0.47-0.64, watch
   arm 0.15-0.20, write faults 0.18-0.27, disarm 0.05-0.08). This is real data: 0.20-0.24 uploads
   per draw of about 9 KB each that the guest CPU rewrote (dynamic vertex and index data), and the
   page protection that detects the writes. The valid-range check itself is already lock-free
   (`shared_memory.cpp:745-785`) and vertex residency is cached per slot
   (`command_processor.cpp:6113`, `vertex_buffers_in_sync_`). Nothing is recomputed with unchanged
   inputs here; it is not an l117 lever (upload batching / the existing upload-ahead arm are the
   candidates; the GPU side is the 0.7-0.8 ms shared-memory upload category of l116).
2. **Bind: 0.33-0.42 us per draw.** Recomputed although unchanged:
   - **The fetch constant buffer** (`command_processor.cpp:9509-9522`): any changed fetch dword
     clears `cbuffer_binding_fetch_.up_to_date` (`InvalidateFetchDwords`, 2677-2686, from
     `WriteRegister` 2424-2435 and the bulk path 2881-2900), and the next draw uploads all 768
     bytes to a new address and sets the root CBV again. 4-5 fetch dwords change per draw, almost
     all of them TEXTURE fetch constants (0.58-0.96 texture key changes per draw), but the
     translated shaders read the fetch constant buffer only through vertex fetches (the DXBC
     translator allocates the fetch cbuffer only in its vfetch path,
     `pipeline/shader/dxbc_translator_fetch.cpp:56-62`). A dirty flag set by ANY write to the
     block: exactly the coarse case.
   - The other buffers are already exact: system constants by per-field compare (9001-9277),
     float and bool / loop constants by the l100 register skip (only a changed dword the current
     map uses marks the buffer, 2790-2840). They are re-uploaded for real changes or a map change
     (a shader switch: 0.29-0.36 per draw have another PSO); content that repeats inside a frame
     (A, B, A shader or material toggles) is uploaded again at a new address each time: that is
     what a content memo can catch (unknown rate; the L1 verify measures it).
   - `IASetIndexBuffer` (6333) is issued at every indexed draw (about 0.8 per draw) without a
     cache; every other binding call (viewport 1282, scissor 1296, topology 1317, blend factor
     and stencil reference 8831-8855, pipeline 5981, root signature 9319, root CBVs and tables by
     bit 9997-10097) is already cached per command list. On the CP thread a redundant call costs
     one deferred-list append (24-32 bytes): the replay of the call runs on the D3D12 replay
     thread. Small (section 3.2).
3. **Textures: 0.29-0.47 us per draw**, 0.60-1.0 slot evaluations per draw of which 95 % have a
   new texture key (real changes, already served by the l101 memo at 90 %+). Recomputed although
   unchanged: when ANY texture becomes outdated (a CPU write fault or a resolve into a watched
   range: `TextureCache::WatchCallback`, `pipeline/texture/cache.cpp:931-944`), the next
   `RequestTextures` resets ALL 32 bindings (538-543 -> `ResetTextureBindings`, 1281-1295): every
   bound slot is decoded again, its textures looked up again, its descriptors written again (and
   the bindless descriptor-index buffers of both stages uploaded again), although only the
   bindings of the outdated textures need a reload. The rate is not in the logs; l117 counts it
   always (`tex_events`, `tex_slots_valid`, `tex_slots_outdated`).

Fourth, not addressed: the render-target phase (0.17-0.23 us) with the l101 memo on (67 % hits; 29 %
miss on inputs although 99 % of the draws keep the same render-target key: the memo inputs include
scissor and viewport registers that change without changing the result).

## 2. What shipped

| Lever | Switch (default) | Verify (wins) | Host keys |
|---|---|---|---|
| L1 constant buffer reuse | `d3d12_cb_reuse` (false) | `d3d12_cb_reuse_verify` (false) | `cb_reuse`, `cb_reuse_verify` |
| L2 index buffer view cache | `d3d12_ib_view_cache` (false) | `d3d12_ib_view_cache_verify` (false) | `ib_cache`, `ib_cache_verify` |
| L3 narrow texture rebind | `d3d12_tex_outdated_narrow` (false) | `d3d12_tex_outdated_narrow_verify` (false) | `tex_narrow`, `tex_narrow_verify` |
| MCLA_THIN: a GPU time of exactly 0 is not thin | always | - | - |

All six cvars are hot (GPU/D3D12) and latched at the next frame (`LatchL117Options`, called from
`LatchFetchIdentityOptions`). A verify turns its lever off in the SDK. Any switch invalidates the
memos, the fetch tracker and the index buffer cache. Pure logic in
`include/rex/graphics/util/cp_l117_levers.h`.

### 2.1 L1: constant buffers (`UpdateBindings`)

- **Fetch buffer, exact dirty tracking.** `FetchCbTracker` keeps the vertex fetch slots (2 dwords
  each, 96 slots) changed since the last fetch upload of the current frame; `InvalidateFetchDwords`
  feeds it with the changed dword range (only while L1 or its verify is on). At an out-of-date fetch
  buffer, the shaders' read slots are the vertex and pixel shaders' `vertex_fetch_bitmap`
  (`VfetchMaskFromBitmaps`). Reuse when: an upload happened in this frame and no read slot changed
  since. The bound buffer then holds every dword the draw reads; the buffer stays out of date, so
  every later draw checks again (cheap: two 64-bit ANDs). Refusals: `frame` (no upload yet in this
  frame) and `used` (a read slot changed).
- **System, float vertex, float pixel, bool / loop: content memo.** At an out-of-date buffer the
  bytes are packed (floats into a scratch buffer first), hashed (XXH3-64) and looked up with the
  size in a 4-way, round-robin, per-kind memo of the uploads of this frame. A hit reuses that
  upload's GPU address: no `Request`, no copy; the root CBV bit is cleared only if the address
  differs from the bound one (`cb_root_saved` counts the reuses of the bound address). A float map
  without constants (a placeholder buffer of undefined bytes) is never memoised (`empty`). Refusals:
  `new` (no entry with these bytes) and `frame` (only an older frame's).
- **Invalidation rules.** Entries and the tracker are tagged with `frame_current_` and are used only
  in that frame: the constant pool reclaims a frame's pages only after that frame completed
  (`Reclaim(frame_completed_)`, which is always below the current frame), so an upload of the
  current frame stays alive while it is recorded, across submissions. A new command list (every
  submission) resets the root arguments (`current_graphics_root_up_to_date_ = 0`), and the next
  draw sets the root CBV again from the binding's address (reused or new); a root signature change
  does the same (root arguments do not survive `SetGraphicsRootSignature`); the descriptor heaps do
  not hold constant buffers. Explicit invalidation at the frame open (with the existing reset of
  every `up_to_date`), at `ClearCaches` (with the pool's `ClearCache`) and at any switch of the
  lever or its verify.
- **Verify.** Every buffer is uploaded exactly as without the lever. Fetch: the tracker models the
  lever (its "upload" is the real upload after a refusal) and a copy of the fetch block at that
  upload; at a would-reuse the read slots' dwords in the registers are compared with the copy
  (`UsedVfetchDwordsEqual`). Memo kinds: every upload is stored with a copy of its bytes; at a
  would-hit the packed bytes are compared with the entry's bytes (a hash collision or a stale
  entry). Counters `cb_ver` and `cb_mm` per kind.
- **Counters** (MCLA_CP_L117, per kind sys / vf / pf / bl / fe): `cb_up` real uploads (ALWAYS
  counted, also with the lever off: the per-kind upload split Phase A could not get from the logs),
  `cb_cons` out-of-date buffers evaluated, `cb_reused`, `cb_would` (would reuse; = reused with the
  lever on), `cb_ver`, `cb_mm`, `cb_refuse=new,frame,empty,used` (summed), `cb_root_saved`.

### 2.2 L2: the index buffer view (`IssueDraw`)

`IbViewCache` holds the view last set on the current command list (location, size, format); it is
reset in `BeginSubmission` next to the existing command-list state resets (a new deferred command
list, a new D3D12 command list). The IA index buffer survives `SetPipelineState` and
`SetGraphicsRootSignature` (only root arguments are lost on a root signature change); nothing else
in the SDK sets an index buffer (the only `D3DIASetIndexBuffer` call site is this one) and
`ClearState` is never used. A matching view is not set again. Verify: the view is always set; at a
would-skip the cache is compared with the deferred command list's own record of the last index
buffer view written since its `Reset` (new `last_index_buffer()`): a difference means a missed
invalidation (`ib_mm`). Counters `ib_cons`, `ib_skipped`, `ib_would`, `ib_ver`, `ib_mm`,
`ib_refuse=first,changed`.

### 2.3 L3: narrow texture rebind (`TextureCache::RequestTextures`)

The outdated-texture reset goes through `OutdatedResetL117`: it always builds the narrow rule's
view of the 32 bindings and counts the event (`tex_events`), the valid bindings (`tex_slots_valid`,
what the full reset resets) and the narrow reset set (`tex_slots_outdated`). The narrow rule
(`TexNarrowResetMask`) resets a valid binding when its unsigned or signed texture has a non-zero
`outdated_mask()` or when it misses a texture it needs (a failed creation: the full reset retries
those). With L3 only that set is reset (`ResetTextureBindingsMaskL117`, which also refreshes the
D3D12 descriptor indices of exactly those slots); the other bindings keep key, swizzle, signs,
textures and descriptors, which are what the full path computes again for them: same fetch
constant (any fetch change clears the slot's in-sync bit independently), same textures (destruction
resets all bindings through the unchanged eviction and frame paths), same descriptor keys. A texture
that becomes outdated after the scan sets the flag again and is handled at the next draw, as in the
full path. Verify: all bindings are reset as before; the bindings the narrow rule would have kept
are snapshotted (key, host swizzle, signs, texture pointers, D3D12 descriptor indices) and compared
at their next rebuild by the full path (`tex_ver`, `tex_mm=key,swizzle,signs,texture,descriptor`);
a fetch write to a pending slot or another reset (eviction, frame start, a new event) drops the
comparison (`tex_dropped`). Counter `tex_kept` with the lever on.

### 2.4 MCLA_CP_L117 v=1

One line per perf-split window (after MCLA_CP_L101, same reason and seq), and
`MCLA_CP_L117 v=1 kind=config cb_reuse= cb_reuse_verify= ib_cache= ib_cache_verify= tex_narrow=
tex_narrow_verify= frame=` at start and at every change:

`MCLA_CP_L117 v=1 reason= seq= cb_reuse= cb_reuse_verify= ib_cache= ib_cache_verify= tex_narrow=
tex_narrow_verify= cb_up=sys:N,vf:N,pf:N,bl:N,fe:N cb_cons=... cb_reused=... cb_would=...
cb_ver=... cb_mm=... cb_refuse=new:N,frame:N,empty:N,used:N cb_root_saved= ib_cons= ib_skipped=
ib_would= ib_ver= ib_mm= ib_refuse=first:N,changed:N tex_events= tex_slots_valid=
tex_slots_outdated= tex_kept= tex_ver= tex_dropped=
tex_mm=key:N,swizzle:N,signs:N,texture:N,descriptor:N`

With every switch off the cost is one branch per constant buffer and per indexed draw, the per-kind
upload increment, and the 32-binding scan at each outdated-texture event.

### 2.5 MCLA_THIN

`ThinFrameDetector::OnFrame`: a frame whose GPU time is available but exactly 0 is no longer
`by=gpu` (l116 13d779cf: 87 of 189 `by=gpu` lines were settled play frames with normal draw counts
and `gpu_ms=0.00`: missing timing); it is counted as `gpu_zero=` appended to the minute line. A
frame thin by draws is still reported.

## 3. Expected saving (from the Phase A numbers; to be measured)

1. **L1 fetch:** the fetch buffer is out of date on most draws with a texture change (0.58 per draw
   by day); with L1 the draws whose vertex fetch slots did not change keep the buffer: up to about
   0.5-0.6 fewer 768-byte uploads and root CBV sets per draw. At an estimated 0.08-0.15 us per
   avoided upload (pool request, 768-byte copy to write-combined memory, root CBV record): 0.04-0.09
   us per draw, **0.25-0.55 ms per frame at 6 k draws**; plus up to one root CBV set less per draw
   on the GPU front end and the replay thread (12-15 k root CBV sets per frame today). The memo
   kinds: unknown rate (`cb_would` in the verify run decides whether they are worth keeping).
2. **L2:** about 0.8 index buffer sets per draw; the CP saving per skipped set is one deferred-list
   append (about 10-20 ns): at most **0.05-0.1 ms per frame at 6 k draws** on the CP thread, the
   same number of D3D12 calls less on the replay thread. L2 is negligible on the CP thread and is
   kept because it is cheap and exact; it is not the third CP cost (that is L3).
3. **L3:** each outdated event currently rebuilds every other bound slot (about 0.3-0.5 us each,
   including descriptors) and re-uploads both descriptor-index buffers; with E events and K kept
   slots per event the saving is about E x K x 0.4 us (60 events x 4 slots = 0.1 ms per frame). E is
   measured by the always-on `tex_events`.

Together **about 0.3-0.7 ms of CP per frame at 6 k draws (2-4 % of the 15-20 ms CP frame of a
dip)**, i.e. 46 -> about 47-48 fps in a CP-bound second if nothing else limits; above about 5.7 k
draws the GPU (16-17 ms) becomes the limit next. The big per-draw blocks (shared-memory uploads and
watches about 1 us, texture evaluation of real changes, render-target inputs) are real work or need
deeper changes; the Deck (CP busy 51-56 ms per frame) gains the same fraction.

## 4. Tests, build, install

- New `tests/unit/graphics/cp_l117_levers_test.cpp` (10 cases, `[l117]`): the memo (find / store /
  frames / ways / round-robin / invalidate); a model (20 seeds x 20,000 steps: a hit always returns
  an upload of the same bytes made in the current frame); vertex fetch slot masks from dwords and
  bitmaps; the fetch tracker rules; a fetch model (30 seeds x 20,000 steps, random register writes
  including same-value writes: a reuse never leaves a changed read dword); the fetch verify compare;
  the index buffer cache (first, changed by location / size / format, reset); the narrow reset set
  (invalid, outdated unsigned / signed, missing texture, unneeded outdated texture, kept); the
  snapshot compare (each kind alone); the MCLA_CP_L117 lines. `play_state_test.cpp`: a GPU time of
  exactly 0 is not thin (and is counted), 1 us is, thin by draws still counts; the minute line with
  `gpu_zero=`.
- SDK build: fresh `private/l117-sdk-b` (the l116 options), output `private/l117-sdk-o`; compiled
  at the first attempt. Normalized warning set of the full build plus the incremental rebuild: 210
  entries, **0 new** against the l116 full build (211; 1 gone: an unused-parameter warning whose
  line no longer prints).
- `git-clang-format` against `d889a16` (include, src, tests): applied (cp_l117_levers.h,
  command_processor.cpp, the test), rebuilt, then clean; `git diff --check`: clean.
- ctest: **100 % passed, 0 failed of 2408 (2404 + 4 skipped BitStream; l116 2394 + 4; +10 new)**
  three times before the format, three times after it, and three times after the install. The
  known flaky `physical_mapping_test.cpp:184` did not fail in any of these runs.
- Install `private/l117-sdk-install`, CMake `0.20.0.23-dev.ge6eeeb9`:

| bin file | Bytes | SHA-256 |
|---|---|---|
| rexruntime.dll | 11776000 | D2EBF2721F4F898B0771E122E4936927CBC8FC194748692FAF96E43799EA08F6 |
| rexgpu-xenos.dll | 3937280 | 4C3918E950B809A5297D96D7A9935A62FB42648D0189CB3009309DD899AE9DE2 |
| TracyClient.dll | 246784 | F431EB5CF80579C3158B6CB296C741221BC046E54C7FD549C87D1321A87323DE |
| rexglue.exe | 4113920 | 8EBF0A7875414B1E4A901ECD9070584FC1080DCD3E592B589F9F4C075A57833A |

File set: 1249 (l116) -> 1250 files; added `include/rex/graphics/util/cp_l117_levers.h`. rexgpu-xenos.dll grows by 38400 bytes (l116 3898880); rexruntime.dll keeps its size; TracyClient.dll and rexglue.exe have no source change (fresh build tree).

**ABI:** `D3D12CommandProcessor`, `TextureCache`, `D3D12TextureCache` and `DeferredCommandList`
grew (installed headers): a host must be rebuilt against this SDK (it is, for l117). The host change
uses no new SDK symbol (keys only), so the main checkout also builds against the gate SDK (l67).

SDK diff (`git show --numstat`): 12 files, 1549 insertions, 89 deletions:
`include/rex/graphics/d3d12/command_processor.h` +30, `include/rex/graphics/d3d12/deferred_command_list.h`
+9, `include/rex/graphics/d3d12/texture_cache.h` +5, `include/rex/graphics/pipeline/texture/cache.h`
+37, `include/rex/graphics/util/cp_l117_levers.h` +507 (new), `include/rex/perf/play_state.h` +11 -3,
`src/graphics/d3d12/command_processor.cpp` +411 -77, `src/graphics/d3d12/deferred_command_list.cpp`
+2, `src/graphics/pipeline/texture/cache.cpp` +112 -2, `tests/unit/CMakeLists.txt` +1,
`tests/unit/core/play_state_test.cpp` +24 -7, `tests/unit/graphics/cp_l117_levers_test.cpp` +400
(new).

## 5. Risks

- Nothing ran: hit rates, the verify counters and the saving are unobserved. cp-fast2 must follow a
  clean cp-verify2 (every `cb_mm`, `ib_mm`, `tex_mm` 0 with `cb_ver`, `ib_ver`, `tex_ver` above 0).
- L1 fetch relies on the translated shaders reading the fetch constant buffer only through vertex
  fetches listed in `vertex_fetch_bitmap` (true for the DXBC translator at d889a16; the same bitmap
  drives vertex residency) and on every fetch register change going through `InvalidateFetchDwords`.
  The verify compares exactly the read dwords at every would-reuse, but only at draws where the
  buffer was out of date.
- L1 memo kinds use a 64-bit hash without a byte compare when the lever is on (the verify compares
  bytes). Their hashing costs about as much as a small upload, so with a low hit rate they cost
  more than they save; the verify run's `cb_would` decides.
- L3 relies on `outdated_mask()` being set before the outdated flag (it is, in
  `Texture::WatchCallback` then `TextureCache::WatchCallback`) and on every texture destruction
  resetting all bindings (eviction, `DestroyAllTextures`, every frame start through `BeginFrame`).
- L2 saves little on the CP thread (section 3.2).
- MCLA_THIN: a frame thin by GPU time with timing below 1 us but not 0 is still reported; a missing
  timing that reads as a small non-zero value would still be noisy.
- Scratch folder `%LocalAppData%\Temp\claude\C--BDU-MCLA-Recomp\l117-work`: `budget.py`,
  `budget2.log`, the SDK edit scripts (`e_ub.py`, `e_tex.py`, `e_dcl.py`, `e_thin.py`,
  `e_thin_t.py`), `sdk-build*.ps1`, `ctest3.ps1`, `warndiff.py`, `w117all.txt`, `cf-diff.txt`,
  `sdk-commit-msg.txt`; logs `sdk-configure.log`, `sdk-build0.log`, `sdk-build1.log`,
  `ctest-a1..3.log`, `ctest-b1..3.log`, `ctest-c1..3.log`, `sdk-install.log`.
