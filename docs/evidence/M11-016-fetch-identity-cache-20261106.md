# M11-016: fetch-constant identity cache in the SDK

Implementation by an Opus subagent on 2026-11-06 as SDK commit `ae35ae3` on local branch `mcla/m11-fetch-identity` (parent `d08813b`); accepted by the primary agent as code: ctest 2060/2060 (4 skipped), no new warnings. Not run in the game. Ships in owner build l73 behind `d3d12_fetch_identity_cache` (default true in the SDK) with an owner A/B (off / on / on+verify) and the new perf-split counters; the premise (the guest re-sends unchanged fetch constants) is measured by `fetch_dw_unchanged / fetch_dw_written`.

---

# M11-016 fetch-constant identity cache (l72-sdk-fetch) - evidence draft

Status: implementation draft by an Opus subagent, 2026-11-06. Not yet accepted by the primary agent. No game or
launcher launched. Physical effect: NOT measured (needs an owner A/B run).

- SDK worktree: `private/l72-sdk-fetch-wt`, branch `mcla/m11-fetch-identity`, base `d08813b`
  (`mcla/m11-content-unmount`). Commit: `ae35ae3`.
- Build: `private/l72-sdk-fetch-b`, output `private/l72-sdk-fetch-o`, l71-sdk-b configuration (Ninja Multi-Config,
  clang 20.1.8, `-march=x86-64-v3`, D3D12 only, tests, Tracy, perf counters), no install step.
  Script: `%TEMP%\claude\C--BDU-MCLA-Recomp\l72-scripts\build.ps1` (copy of l71 build.ps1 with paths changed and the
  `cmake --install` lines removed), `test.ps1`.
- Design input: `docs/evidence/M11-cp-cost-plan-20261106.md` section 2.2 (T1-T3) and 2.3 (B2).
- Spark: not used (a bounded C++ change validated by exact source reading and unit tests; a model answer would need
  the same checks).

## What changed (paths relative to the SDK worktree)

| Part | Where | Change |
|---|---|---|
| Pure helpers | `include/rex/graphics/util/fetch_identity.h` (new) | `StoreFetchDwordsCompare` (store big-endian dwords, report first/last changed), `FetchIdentityGeneration` (atomic, starts at 1), `FetchIdentityMemo<Payload, log2>` (direct-mapped, 512 entries, keyed by the 6 raw fetch dwords, stamped generation), `SamplerMemoSlot` + `PackSamplerBindingWord`, `FetchIdentityCounters` |
| cvars | `src/graphics/d3d12/command_processor.cpp:90` `d3d12_fetch_identity_cache` (default true, hot reload, latched at setup `:1357` and at every frame start `:5145`); `:98` `d3d12_fetch_identity_verify` (default false) | false = pre-M11-016 behaviour |
| (a) compare-before-invalidate | `D3D12CommandProcessor::WriteRegister` `:1968` (reads the stored value before the base class writes), `WriteRegistersFromMem` fetch branch `:2091-2110`, shared `InvalidateFetchDwords` `:2009` | unchanged single write: no invalidation; bulk write: only the dwords whose value changes are invalidated (texture slot `d/6`, vertex fetch `d/2`, fetch cbuffer). Cache off: identical calls to before (`TextureFetchConstantsWritten(first/6,last/6)`, `InvalidateVertexBufferResidencyRange(first/2,last/2)`, `cbuffer_binding_fetch_.up_to_date = false`) |
| (b) raw-fetch memo | `TextureCache::RequestTextures` `src/graphics/pipeline/texture/cache.cpp:477-672`; state in `include/rex/graphics/pipeline/texture/cache.h` (`FetchMemoPayload`, `fetch_memo_`, `fetch_identity_generation_`) | for each used slot not in sync and with fetch type kTexture: lookup by the 6 raw dwords at the current generation. Hit: key, swizzled signs, host swizzle and the texture pointers come from the entry (skips `BindingInfoFromFetchConstant`, `GuestToHostSwizzle`, and on a key change `FindOrCreateTexture` = `GetGuestLayout`, two `IsRangeScaledResolved`, hash lookup). The binding-changed, load and pending-load logic is unchanged and runs on the hit result. Miss: original path, then store an entry stamped with the generation read BEFORE the lookup, only if every texture the sign configuration needs is non-null |
| (c) lock-free scaled-resolve check | `TextureCache::IsRangeScaledResolved` `cache.cpp:1227-1275`; writers `MarkRangeAsResolved` `:325-370`, `ScaledResolveGlobalWatchCallback` `:1285-1342` | with the cache on, no `global_critical_region_` on the read side; reads via `std::atomic_ref` (acquire on l2, relaxed on l1); writers keep the lock and use `atomic_ref` stores (formally race-free). Cache off: the lock is taken as before |
| (d) sampler memo | `D3D12CommandProcessor::GetSamplerParametersMemo` `command_processor.cpp:6262`, used at `:6497` (vertex) and `:6530` (pixel); slots `sampler_memo_vertex_/pixel_` in the header | per sampler binding position: if the 6 fetch dwords, the packed binding word (fetch constant, mag/min/mip filter, aniso override) and `anisotropic_override` equal the slot, reuse the stored `SamplerParameters` value; else call `GetSamplerParameters` and store |
| Counters | `include/rex/perf/perf_split.h` (`PerfSplitCounters`, `REX_PERF_SPLIT_FIELDS`, `ComputePerfSplitWindow`), collected in `PerfSplitCollectInputs` `command_processor.cpp:3218` | appended after `gpu_submissions` (still `v=1`, name=value, before `render_tid`): `fetch_dw_written fetch_dw_unchanged tex_slots tex_key_changed tex_memo_hit tex_memo_miss tex_memo_mismatch tex_gen_bumps smp_memo_hit smp_memo_miss` |
| Tests | `tests/unit/graphics/fetch_identity_test.cpp` (new, registered in `tests/unit/CMakeLists.txt`) | see below |

## Invalidation / generation rules

The memo maps raw dwords -> {decoded TextureKey, swizzled signs, host swizzle, Texture*, Texture* signed}. The
decode part is a pure function of the dwords (the invalid fetch type, which depends on
`gpu_allow_invalid_fetch_constants` and logs, is never memoized; `GetHostFormatSwizzle`,
`IsSignedVersionSeparateForFormat`, host size limits and the draw resolution scale are constant for the cache's
lifetime). The texture part is what `FindOrCreateTexture(key)` returns, which depends only on (1) the contents of
`textures_` and (2) the scaled-resolve page bits. Every site that changes either bumps the generation:

| # | Bump site | Line (`cache.cpp`) | Why |
|---|---|---|---|
| 1 | `FindOrCreateTexture` after `textures_.emplace` (texture creation) | 1074 | conservative: a new texture may change what a lookup returns |
| 2 | `CompletedSubmissionUpdated` after each `textures_.erase` (soft/hard-limit eviction) | 304 | entries may point at the destroyed texture |
| 3 | `DestroyAllTextures` (ClearCache, destructors) | 954 | all textures destroyed |
| 4 | `MarkRangeAsResolved` when any scaled-resolve page bit goes 0 -> 1 | 363 | key -> scaled texture |
| 5 | `ScaledResolveGlobalWatchCallback` when any page bit goes 1 -> 0 (CPU write over resolved memory; any thread, under the global lock; the generation is atomic) | 1340 | key -> unscaled texture |

Not bump sites, with the reason:
- `ResetTextureBindings` (every `BeginFrame`, every `texture_became_outdated_`, eviction): it invalidates the
  per-slot bindings, not the key -> texture mapping. After a reset the slot's key differs from the stored one, so
  the request takes the key-changed path, and the memo hit feeds the same `load_*_data = true` ->
  `queue_pending_texture_load` -> `PrepareTextureLoad` (checks `outdated_mask`) as the slow path. Outdated data is
  reloaded exactly as before. The BeginFrame reset exists to retry failed creations; failures (nullptr) are never
  memoized.
- Texture outdated / reload (`WatchCallback`, `MakeUpToDateAndWatch`): same Texture object, same mapping.
- Scaled-resolve page bits only matter with draw resolution scaling > 1 (otherwise `FindOrCreateTexture` never calls
  `IsRangeScaledResolved`).

Race rule: the entry is stamped with the generation read before the lookup that produced it. A bump in between
(including the creation of one of these textures, rule 1) makes the entry stale immediately; the next miss stores a
fresh one. So every newly created texture costs one extra miss.

Compare-before-invalidate (a) is exact: every cache the invalidation feeds (texture in-sync bits, vertex buffer
residency bits, the 768 B fetch cbuffer) is derived from the register values, so a write that leaves the value
unchanged leaves them valid. Pre-existing paths that write the register file without the D3D12 hooks
(`CommandProcessor::RestoreRegisters(execute_callbacks=false)`, no callers in the SDK; `GraphicsSystem::WriteRegister`
MMIO default case) did not invalidate before either; unchanged.

Sampler memo (d) is a pure function cache (no generation needed): `GetSamplerParameters` reads only the 6 fetch
dwords, the binding's filter fields and `anisotropic_override` (plus constant device state).

(b) note: sampler state does not participate in the texture binding (the TextureKey and textures are independent of
filters/clamps), so the texture memo key is the 6 dwords only.

## Counters (MCLA_PERF_SPLIT, appended, per window)

- `fetch_dw_written`, `fetch_dw_unchanged`: fetch-constant dwords written by PM4 (both D3D12 paths) and how many left
  the value unchanged. Counted only with the cache on. `unchanged/written` is the T1 hit rate.
- `tex_slots`: slots decoded in `RequestTextures` (used and not in sync). With T1 working it should fall sharply
  against `draws`.
- `tex_key_changed`: slots whose key changed (each one is a texture lookup).
- `tex_memo_hit`, `tex_memo_miss`: raw-fetch memo lookups (cache on).
- `tex_memo_mismatch`: verify mode only; must be 0.
- `tex_gen_bumps`: generation bumps (always counted).
- `smp_memo_hit`, `smp_memo_miss`: sampler memo lookups (cache on).

## Tests

`tests/unit/graphics/fetch_identity_test.cpp` (tag `[fetch-identity]`): store/compare with byte swap; a model of
the write path proving that an identical 192-dword re-send keeps every texture in-sync bit, vertex residency bit and
the fetch cbuffer valid, that one changed dword invalidates only its slot (2) and vertex fetch (6), and that a
change spanning two fetch constants invalidates both; generation start/bump; memo hit only with equal dwords and
generation, staleness after a bump, an entry stamped before a racing bump is stale, collisions replace and never
alias; sampler slot matching on every input; counter addition; the perf-split window deltas and the line (new fields
once each, after `gpu_submissions`, before `render_tid`). The GPU paths are covered by the existing tests (build) and
need the owner run.

Build and test results (2026-11-06, after wait-for-idle-pc exit 0 / IDLE):
- Commit `ae35ae3` on `mcla/m11-fetch-identity` (parent `d08813b`), 8 files, not pushed.
- Configure + full Release build into `private/l72-sdk-fetch-b` / `private/l72-sdk-fetch-o`: rc 0, "SDK OK", no install.
  Logs: `l72-scripts/configure.log`, `l72-scripts/build1.log`.
- Warnings: none in new or changed lines; the hits in the touched files are pre-existing lines
  (cache.h TextureKey memcpy/memset, unused global_lock parameters, D3D12 CP unused parameters).
- ctest (`l72-scripts/ctest.log`): 100% passed, 0 failed of 2064 = 2060 passed + 4 skipped (BitStream, as baseline);
  baseline at d08813b was 2054 passed + 4 skipped; the 10 new `[fetch-identity]` cases all pass.
- clang-format 20.1.8: new files clean; `git clang-format --diff d08813b` reports no changes on the touched lines (the
  D3D12 command_processor.cpp/.h were not format-clean at the base; unrelated lines left untouched).

## Expected saving (from the design, not measured)

Design 2.2: T1-T3 -1.6 to -2.9 ms/frame of CP CPU on the PC at 1080p late content (texture phase 4.6 ms/frame,
0.66-0.75 us/draw), plus part of `p_vb` and `packets_other`; with B2 -0.3 to -0.6 ms of `p_bind`. Deck -3 to -5
ms/frame. The real saving depends on `fetch_dw_unchanged/fetch_dw_written` (unmeasured premise: the XDK re-sends
unchanged fetch blocks per draw) and on the memo hit rate.

## Risks

- A missed generation bump binds a stale texture (wrong texture or a destroyed Texture* -> visual corruption or a
  crash). Mitigations: conservative bump on every creation; failures never memoized; verify cvar.
- Compare-before-invalidate would be wrong only if some cache keyed on fetch constants also depended on something
  else that the per-write invalidation used to refresh as a side effect. Reviewed users: texture in-sync bits
  (`TextureCache::RequestTextures` re-decode), vertex residency (`vertex_buffers_in_sync_`, cleared separately by
  `InvalidateAllVertexBufferResidency`), fetch cbuffer (also cleared at `:5111`). No other consumer found.
- Lock-free scaled check: a concurrent clear can be missed for one lookup; identical to before (the lock was released
  before the result was used); the clear bumps the generation.
- Memo table 512 x ~72 B = ~36 KB of cache footprint on the CP.

## How the owner A/B detects problems

- Arms: `d3d12_fetch_identity_cache=false` (old behaviour) vs `true` (default), same route; the cvar hot-reloads at
  the next frame, so both arms can share a session. Optional third arm: `true` + `d3d12_fetch_identity_verify=true`
  (slower; `tex_memo_mismatch` must stay 0; any non-zero value is a missed bump, the slow result is used so the
  image stays correct).
- Visual: texture artefacts (wrong/stale textures, flicker on streamed textures, liveries KI-047, vinyls KI-033,
  album KI-056), especially after heavy streaming/eviction (long drives) and with resolution scaling if used.
- Counters: `fetch_dw_unchanged/fetch_dw_written`, `tex_slots/draws`, `tex_memo_hit/(hit+miss)`, `tex_gen_bumps`
  per frame; perf: `p_tex_ms`, `p_bind_ms`, `p_vb_ms`, `packets_other_ms`, `cp_cpu_ms` per frame.
- Cvar off restores the previous behaviour exactly (same invalidation calls, lock taken, no memo lookups); only the
  `atomic_ref` loads/stores on the scaled-resolve bit vectors and the generation increments remain.
