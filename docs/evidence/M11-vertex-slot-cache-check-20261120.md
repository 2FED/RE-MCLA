# M11 vertex-slot cache check (2026-11-20)

Static read-only check. No builds, tests or game runs. Sources: SDK worktrees at e6eeeb9 (l117)
and d889a16 (published v0.20.0.12), plus upstream Xenia Canary at f9431eb (latest commit touching
`src/xenia/gpu/d3d12/d3d12_command_processor.cc`, dated 2026-11-17, branch canary_experimental).

## 1. What the code does

Two caches sit in front of the vertex `RequestRange` in `D3D12CommandProcessor::IssueDraw`:

- `vertex_buffers_in_sync_[2]`: 96 bits, one per fetch slot (`include/rex/graphics/d3d12/command_processor.h:1434`).
- `vertex_buffer_states_[96]`: `{address, size}` of the slot's last request (`command_processor.h:1412-1416`).

e6eeeb9 `src/graphics/d3d12/command_processor.cpp:6252-6291`
(d889a16: the same text at `:6112-6151`; the diff of the two loops is empty, and so is the diff of the helpers at `:609-649` / `:564-604`):

```cpp
      uint64_t vfetch_bit = uint64_t(1) << (vfetch_index & 63);
      if (vertex_buffers_in_sync_[vfetch_index >> 6] & vfetch_bit) {      // 6253 / 6113
        continue;                                                         // skip A
      }
      ...
      VertexBufferState& state = vertex_buffer_states_[vfetch_index];     // 6275 / 6135
      if (state.address == vfetch_constant.address && state.size == vfetch_constant.size) {
        vertex_buffers_in_sync_[vfetch_index >> 6] |= vfetch_bit;         // 6277 / 6137
        continue;                                                         // skip B
      }
      ... shared_memory_->RequestRange(vfetch_constant.address << 2, vfetch_constant.size << 2) ... // 6282 / 6142
      state.address = vfetch_constant.address;
      state.size = vfetch_constant.size;
      vertex_buffers_in_sync_[vfetch_index >> 6] |= vfetch_bit;           // 6291 / 6151
```

Every writer and every reset:

| What | Where (e6eeeb9 / d889a16) | Effect |
|---|---|---|
| set in-sync bit + state | `:6277`, `:6289-6291` / `:6137`, `:6149-6151` | after a request or after skip B |
| clear one in-sync bit | `InvalidateFetchDwords` -> `InvalidateVertexBufferResidencyRange` `:2734` / `:2685` (a fetch-constant register write) | bit only; the state is kept |
| clear all in-sync bits | `IssueSwap` `:3092-3093` / `:2960-2961` | bits only; the state is kept |
| clear bits and states | `InvalidateAllVertexBufferResidency` `:621-628` / `:576-583`, called from `ClearCaches` `:611`, `SetupContext` `:1407`, shutdown `:2266` (d889a16 `:566`, `:1362`, `:2221`) | the only state resets |

So "valid" at skip B means only "same address and size as this slot's previous request". No
generation, epoch, dirty flag or validity check is involved.

## 2. Life cycle of a buffer rewritten in place

1. Frame N: draw with slot s = (A, S), state differs, `RequestRange` uploads the invalid pages and
   arms the write watch. The state becomes (A, S).
2. Guest CPU writes into [A, A+S). The fault calls `SharedMemory::MemoryInvalidationCallback`
   (`shared_memory.cpp:903-996`, on the faulting guest thread, under the global critical region).
   It clears `valid_flags` and `system_page_flags_valid_and_gpu_written_` for the (widened) pages
   (`:961-974`), sets `gpu_written_data_dirty_` / `dirty_blocks_` (only read inside SharedMemory:
   `:267-301`), and calls `FireWatches` (`:992`): per-range watches and the global watches. The
   only global watch is the texture cache's scaled-resolve watch (`pipeline/texture/cache.cpp:234`).
   The command processor registers none, and nothing touches `vertex_buffers_in_sync_` or
   `vertex_buffer_states_`. The page now stays unprotected until something requests it again.
3. Frame N+1 (after `IssueSwap` cleared the bits) or the same frame (after a fetch-constant write
   cleared the bit): slot s still has (A, S), so skip B runs and `RequestRange` is not called. The
   pages stay invalid and the host shared-memory buffer still holds the frame-N bytes. The draw
   reads stale vertices.
4. Same frame with no fetch-constant write in between: skip A runs. This is also a skip without a
   validity check (Canary would re-request here too).

The staleness ends only when another request covers those pages (another slot or an index buffer
over the same range, a memexport full-range request) or when slot s is next requested with a
different (address, size), which misses skip B and uploads.

Why the study is right: there is no per-frame or per-invalidation reset of the state. The
`IssueSwap` reset (`:3092`) clears only the bits, and skip B sets them again straight away.

The two sites: "6113" is the in-sync check (skip A) in d889a16 numbering, the same as e6eeeb9
`:6253`. It is not a separate path. Both skips are in one loop. Skip A lasts until the next
fetch-constant write or swap. Skip B is the one that persists across frames.

Origin: `git log -S vertex_buffer_states_` / `-S vertex_buffers_in_sync_` gives only `e6407f3`
(2026-03-01, "graphics/d3d12: port backend pipeline and command runtime updates"). Its parent
requested every slot on every draw and still had Triang3l's TODO. The Vulkan backend has the same
code (`vulkan/command_processor.cpp:4048`, `:4071-4085`, swap reset `:2337-2338`, fetch reset
`:2220`, `:2303`).

## 3. Upstream Xenia Canary

Canary has no resident bitmap and no per-slot cache in its D3D12 command processor (f9431eb,
`d3d12_command_processor.cc:2957-3030`). It keeps the TODO "Cache residency for ranges in a way
similar to how texture validity is tracked". It collects the address and size of every used slot,
takes the global critical region once, and calls `shared_memory_->RequestRange` for every slot on
every draw (`:3021`). So it re-uploads invalidated pages before every draw. The resident bitmap
remembered in the brief is not in current upstream. Our skip-A/skip-B pair is only in the port.

## 4. Verdict

- The code defect: (b), high confidence (about 95%). Skip B does not check the shared-memory valid
  bitmap. A rewrite in place of [A, A+S) is never uploaded while slot s keeps being requested with
  (A, S). This reverses the upstream guarantee.
- The trigger is narrower than "any buffer rewritten at the same address". The slot's previous
  request must have been the same (A, S): no other buffer was requested on that slot in between.
  Typical cases:
  - a dynamic buffer at a fixed address drawn alone on its slot every frame (particles, HUD or
    text, trails, a CPU-animated mesh);
  - consecutive draws on the same slot with the same buffer and a CPU rewrite between them.

  Ring or double-buffered dynamic data (A, B, A, ...) and static data are not affected.
- Whether MCLA hits it cannot be decided statically. That would take the game's per-slot access
  pattern, so the counter is needed: (c) for the game.

Counter (diagnostic build): at e6eeeb9 `command_processor.cpp:6276`, inside the skip-B branch
before `continue`, count `vb_skip_invalid` when any page of
[address<<2, (address+size)<<2) is clear in `active_valid_flags_`. Use the same lock-free scan as
`SharedMemory::RequestRanges` `shared_memory.cpp:743-772`, moved into a public
`SharedMemory::IsRangeValidLockFree(start, length)`. Add `vb_insync_skip_invalid` with the same
condition at the skip-A `continue` (`:6254`). Any non-zero value is a confirmed stale draw.

## 5. Fix

Minimal and safe: in skip B (and in skip A, for upstream equivalence) skip only if
`IsRangeValidLockFree(...)` is true; otherwise fall through to `RequestRange`. This does not change
behaviour for valid data and restores re-upload for invalidated pages. Alternative: delete
`:6275-6279`. `RequestRange` has its own lock-free fast path (`:743-784`), but that path also
builds a thread-local vector, sorts and runs `EnsureHostGpuMemoryAllocated`.

Cost: the bitmap check is 1-2 relaxed 64-bit loads for buffers of up to 256 KiB (one block of 64
pages), about 5-15 ns per slot. At about 6k draws and 1-2 slots per draw that is roughly 0.05-0.2
ms of CP per frame. Not measured. Mirror the fix in the Vulkan backend.

Possible symptoms. Stale vertices would show as geometry frozen at old positions or UVs, or, when
other draws on the slot sometimes force an upload, flicking between old and new. Frame rate does not
matter.

| Issue | Could stale vertices produce it? |
|---|---|
| KI-082 (traffic trembles near the player, also at 30 fps) | Low to moderate. Fresh/stale alternation would look like tremble and does not depend on fps. But car meshes are normally static buffers moved by constants, so it would need CPU-written car vertices (LOD morph, deformation). |
| KI-079 (trees blink at dawn) | Low. A fixed-address CPU-rebuilt impostor quad buffer could show old quads for a few frames, but the RE points at the impostor render-target refresh. |
| KI-080 (ocean shimmer) | Low. A CPU-animated water grid at a fixed address would mostly freeze; it would shimmer only with partial refreshes. |
| KI-081 (white world textures on the integrated Radeon, only there) | Very unlikely. The symptom is colour and depends on the driver; stale vertices would be the same on every GPU. |

Measure: run the `vb_skip_invalid` and `vb_insync_skip_invalid` counters in the planned owner run
(PC daylight race plus a near-traffic stretch). If either is non-zero, record the slot index and
range of the first 16 hits, then A/B the fix against KI-082.
