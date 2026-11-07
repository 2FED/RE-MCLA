# KI-071 step 2: XMA sibling trace, stall-on-invalid-next (D2), start skip (D1), codec reset (SDK)

Implementation by an Opus subagent on 2026-11-07 as SDK commit `8f949f7` on local branch `mcla/m11-xma-sibling` (parent `4a3aea1`); accepted by the primary agent as code: ctest 2082/2082 (4 skipped), no new warnings; all four cvars default false (counters always on). Installed as `private/l74-sdk-install` 0.20.0.4-dev.g8f949f7: rexruntime.dll 3c4fec5a16d1d9e7706b9c17f45208d7fbf1cb06b0938070119a5ffd9bbcd8dd (11,339,264 B), rexgpu-xenos.dll 150e2b6f40d87f9dc47460acf90a60316e01804745303927a3849c3a75c8b54b, TracyClient.dll bdfde78135dba8739297e096118826f7fa71094642889de39e8324a589457ef4. Note for later worktrees: the copied thirdparty `.git` files point to a missing module path, so git commands there need `--ignore-submodules=all`.

---

# KI-071 step 2: XMA sibling-context trace and frame-exact fixes (D1/D2/codec reset) - SDK notes (draft)

Draft by an Opus implementation subagent, 2026-11-07. SDK worktree `private/l74-sdk-xma2-wt`, branch
`mcla/m11-xma-sibling` from `mcla/m11-xma-doubling` 4a3aea1. Commit: 8f949f7 (version string
0.20.0.4-dev.g8f949f7). Nothing was launched; no physical result is claimed. All four new cvars default to
false. With every cvar off the build decodes exactly like 4a3aea1; the only additions are counters
(relaxed atomics) and a few extra branches on the error paths.

Inputs: `docs/evidence/M11-ki071-sibling-contexts-re-20261107.md` (C5 refuted; 5 mono 44.1 kHz
sibling contexts; candidates D1/D2), `docs/evidence/M11-ki071-xma-fixes-sdk-20261106.md` (4a3aea1).

## Files
- New `include/rex/audio/xma/sibling_trace.h`. It holds the pure helpers:
  - `DecideBoundary` and `ErrorStatusAfterDecode` (D2)
  - `StartSkipBlocks` (D1)
  - `GroupContiguousSlices` and `MaxPairwiseDelta` / `ComputeGroupDeltas` (grouping)
  - `FormatSiblingGroupLine` and `FormatSiblingMinuteLine`
  - the four cvar declarations
- `include/rex/audio/xma/context.h`: per-context counters, the stall state, the start-pending flag,
  `SiblingSnapshot()`, `TakeSiblingMinuteTotals()`, `OnBoundary()`, `LogFirstDecode()` and
  `set_kick_group()`.
- `src/audio/xma_context.cpp`: the cvar definitions, the counting, and the D1, D2 and reset paths.
- `include/rex/audio/xma/decoder.h` and `src/audio/xma_decoder.cpp`:
  - kick burst ids
  - the per-second group scan and the per-minute line (`MaybeEmitSiblingTrace`), called after each
    kick burst and once per worker sweep.
- New `tests/unit/audio/xma_sibling_trace_test.cpp`: 8 cases, tag `[ki071]`.

Line citations are to `src/audio/xma_context.cpp` at 4a3aea1 unless stated otherwise.

## Cvars
| cvar | default | effect |
|---|---|---|
| `xma_sibling_trace` | false | Logging only: the first-decode record, the group lines and the MCLA_XMA_SIB minute line. The counters run regardless of this cvar. Decoding is unchanged. |
| `xma_stall_on_invalid_next` | false | D2 stall. Also clears error_status 4 after a successful decode. |
| `xma_honor_start_skip` | false | D1: applies the start subframe skip at the first decode after a Clear. |
| `xma_reset_codec_on_clear` | false | Re-creates the FFmpeg codec context on Clear and Release. |

Suggested arms, all with `xma_sibling_trace=true`:
- **trace-only:** the remaining three cvars false. This is the decisive measurement.
- **d2:** `xma_stall_on_invalid_next=true`.
- **d1:** `xma_honor_start_skip=true`.
- **all:** all three fixes on, plus `xma_reset_codec_on_clear=true`.

The 4a3aea1 cvars (`xma_swap_carry_packet` and the others) are independent. Keep them as in the
voice-fix arm, or keep them off, but use the same setting in every arm.

## (1) Trace `xma_sibling_trace`

### Per-context counters (since the last Clear/Release; reset in ResetDecoderState)
- `frames_parsed`: counts each frame the read offset moves past. That is every decode attempt
  (incremented before `DecodePacket`), plus each split-header frame dropped by a swap.
- `frames_emitted`: `DecodePacket` returned true. Each counts as one 512-sample frame per channel.
- `blocks_written`: 128-sample blocks (256 bytes mono) that `Consume` wrote to the output ring.
- `consumed_blocks`: blocks the guest has read. It is computed at the start of each Work as
  `blocks_written` minus the blocks still in the ring (`output_buffer_block_count` - free blocks).
- `blocks_skipped`: blocks discarded by the D1 start skip.
- Drop counters, by path:
  - `split`: split-header swap with the next buffer not valid (:775-780). It loses the straddling
    frame.
  - `ff`: 0xFF full-skip path that swaps while the next buffer is not valid (:761-767).
  - `err4`: `error_status = 4` set, at :791 (frame-length 0x7FFF in a split header) and :814 (a
    spanning last frame with the next buffer not valid).
  - `fail`: `DecodePacket` false. The read offset still advances, so the frame is lost.
- `start_skip`: dword1 bits 24-26 at the first decode.
- `kick`: kick burst id of the last kick.

### Global event totals (minute line)
- The four drop counters
- `stalls`, `stall_timeouts` and `err4_cleared` (D2)
- `swapbacks`: the start packet has no frame start, so the code swaps back (:885-888). This can drop
  a whole buffer.
- `no_bits`: the "There are no bits to copy" swap (:803-806)
- `start_skips`: first decodes with a non-zero field
- `start_skips_applied`
- `codec_resets`
- `first_decodes`

### First-decode record
One paced INFO line, using the shared token bucket of the 4a3aea1 swap trace (burst 64, 8 lines/s).
It is written at the first successful decode after Clear/Release:

`[xma] first-decode: ctx= rate= stereo= entry_read_offset= read_offset= packet= first_frame_offset= start_skip= sdc= codec_reinit= buffer= packets= current_buffer= kick_group= skip_fix=`

- `entry_read_offset` is the guest-set offset as Decode saw it, before the host clamped it to the
  first frame.
- `first_frame_offset` is the packet header's first-frame bit offset of the packet being decoded.
- `codec_reinit=1` means PrepareDecoder re-created the codec for this frame (the rate or channel
  count changed, or the codec was reset on Clear).

### Grouping
- **Kick burst:** `XmaDecoder::WriteRegister`, kick registers (xma_decoder.cpp 282-309).
  - Kick-register writes less than 1 ms after the previous kick write share one burst id.
  - Every context kicked by that write gets the id before its inline Work.
  - The guest helper `sub_823EE050` writes one kick register per stream in stream order. Separate
    one-stream voices (the cut-scene case per the RE) get one id each, unless the guest pumps them
    back-to-back.
  - Kick ids are therefore informative only. The ids appear in the first-decode record and in
    `kick=[..]`.
- **Sibling group (used for the lines):** contexts whose input buffers are contiguous slices of one
  read.
  - Slice b starts at a.ptr + a.packets*2048. Both input buffers of every allocated context that
    has decoded since its Clear are considered, and the relation is transitive (union-find).
  - Group id = lowest context id in the group.
  - With the l73 data (67/70/38/72/69 from 03F5F800) this reproduces the cut-scene group. The
    test "contiguous slices of one read" uses exactly those pointers.

### Lines
- **Group line.** Once per second for every group with at least one member that emitted a frame in
  that second. At most 16 lines per second; the rest are counted as `suppressed_group_lines` in
  the minute line.

  `[xma] group: id=38 ctxs=[38,67,69,70,72] frames_emitted=[..] frames_parsed=[..] consumed_blocks=[..] drops=[split=a/b/c/d/e,ff=..,err4=..,fail=..] start_skip=[..] skipped_blocks=[..] kick=[..] frame_delta=N lost_delta=N`

  - Lists are in `ctxs` order.
  - Drop lists are per context, separated by `/`.
  - `frame_delta` = max - min of frames_emitted.
  - `lost_delta` = max - min of (frames_parsed - frames_emitted).
- **Minute line.** Every 60 s, written only while the trace is on:

  `MCLA_XMA_SIB v=1 kind=minute groups= max_frame_delta= drops_split= drops_ff= err4= decode_fail= max_lost_delta= stalls= stall_timeouts= err4_cleared= swapbacks= no_bits= start_skips= start_skips_applied= codec_resets= first_decodes= stall_fix= skip_fix= reset_fix= suppressed_group_lines=`

  - `groups` is the number of distinct group ids reported in the minute.
  - `max_frame_delta` and `max_lost_delta` are the largest per-second group deltas in the minute.

### How to read it (cut-scene run)
- **Content offset between siblings.** The guest consumes every sibling FIFO at the same rate, so
  `consumed_blocks` should be equal across a group, give or take a few blocks of sampling skew.
  Look first at `lost` (= frames_parsed - frames_emitted) and at `start_skip`.
  - A sibling whose `lost` is k frames higher, or whose `frames_emitted` differs by k at equal
    consumption, plays k x 512 source samples **ahead** of the others. This is the 11.61 ms and
    23.22 ms tap for k = 1 and k = 2.
  - The non-zero drop counter in that sibling's slot names the path:
    - `split` or `ff`: **D2**, a swap-and-drop at the buffer boundary.
    - `fail`: the codec rejected a frame (a corrupt frame, or a codec state problem).
    - `err4` without `split`: the sticky error throttle. That causes starvation and guest zero-pads,
      i.e. an output-sample lag, not a frame-exact one.
- **D1.** Every member has `lost` = 0, but `start_skip` differs between siblings. Also check the
  first-decode records:
  - `entry_read_offset` and `first_frame_offset` show where each sibling started.
  - Siblings whose start skip differs by s blocks are misaligned by s x 128 samples when the skip
    is not applied.
  - A whole-frame difference in the first frame chosen (`entry_read_offset` lands in different
    frames for the same start time) points to the seek warm-up.
- **Fill jitter.** `frame_delta` of 1-2 with equal `lost` and equal `consumed_blocks` is fill jitter:
  ring headroom of up to sdc+padding blocks (RE section 3). It is not the cause.
- **Host aligned.** Everything equal, all drops 0 and start_skip all 0 means the host decodes the
  siblings aligned. The guest consume hook below is then needed to rule out guest underflow
  (zero-pads).

## (2) D2 `xma_stall_on_invalid_next`

### Upstream
- **Split header** (:775-781): a frame header split across the last packet. If the next packet
  cannot be fetched (`GetNextPacket` null: the next index is past the buffer and the other buffer
  is invalid), the code runs `SwapInputBuffer(); return;`.
  - SwapInputBuffer invalidates the current buffer and sets the read offset to 32 in the other one.
  - When decoding resumes, :753 moves the offset to packet 0's first frame, so the straddling frame
    is never decoded. That sibling is now one frame ahead.
- **0xFF path** (:761-767): there is no later frame start in this buffer, so the code swaps
  immediately, whatever the state of the next buffer.
- **Spanning last frame** (:809-816): next buffer not valid, so `error_status = 4; return;`. The
  read offset is kept and the frame is retried at the next Work.
- **error_status in this decoder:**
  - Its only writers are the two `= 4` sites.
  - `StoreContextMerged` copies it back to the guest (:556).
  - `ClearLocked` does not reset it.
  - The Work loop breaks on `error_status == 4` (:266).
  - So once set, it stays set for the life of the context, and every later Work decodes at most one
    frame. That throttle leads to starvation and guest zero-pads.
  - I found no host code that ever clears it.
  - The field is named ErrorStatus/ErrorSet after xma2defs.h (context.h:84-85). The bit layout
    comes from Xenia; there is no hardware document, and the value 4 is a host choice.
  - The guest's use of the field was not traced. I found no consumer of it in the pump path.

### Fix (cvar on)
- At each of the three sites, when the next data is not available, `xma::DecideBoundary` returns
  `kStall`. The context then keeps its read offset and current buffer.
  - `stalled_this_work_` ends the Work loop (new break after :266).
  - The guest's next kick retries the same frame. Once the guest has validated the next buffer, the
    straddling frame decodes from both packets and the normal end-of-buffer swap follows.
- The spanning path stalls without setting error 4.
- After a successful `DecodePacket`, `xma::ErrorStatusAfterDecode` clears a host-set
  `error_status` of 4. This also covers the :791 site, whose value is genuinely bad data.
- **Bound:** after `kMaxConsecutiveStalls` = 4 consecutive stalled Works (Works happen only on guest
  kicks) the upstream action is taken and counted as `stall_timeouts`. This covers the end of a
  stream and a guest that refills only after the release. The counter resets on any successful
  decode or non-stall boundary.

### Risks
- A guest that validates the next buffer only after the current one has been released would see
  the current buffer stay valid longer, for up to 4 kicks.
  - During that time the ring drains and the guest zero-pads that channel, which shifts it by an
    output-sample amount. That is audible as a gap.
  - Watch `stall_timeouts`. Non-zero means the fallback fired.
  - The l73 swap trace had `new_valid=1` on every normal swap, so the next buffer is usually ready.
- Clearing error 4 changes a guest-visible field. A guest that polls it for an end-of-stream
  decision could behave differently. No such consumer was found.
- The 0xFF stall gives the same final position as upstream, since packet 0's first frame is used
  either way. Only the timing of the buffer release changes.

## (3) D1 `xma_honor_start_skip`

### Guest
- Setter `sub_823EDC80(xma, stream, read_offset r5, skip r6)` (mcla_recomp.25.cpp 41098-41120):
  - Writes `read_offset` into dword2 bits 0-25 of the stream's shadow context.
  - Writes `skip & 7` into dword1 bits 24-26 (`rlwimi r9,r6,24,5,7`, mask 0x07000000).
- Caller `sub_823FB960` (mcla_recomp.26.cpp 3652-3677):
  - Seek start (mode byte +292 == 2): `read_offset = sub_823FA988(...)`, the bit offset of the
    frame that holds start sample +28. `skip = (+28 >> 7) & 3`, i.e. the 128-sample subframe of
    the start sample inside its 512-sample frame.
  - Otherwise: `read_offset = (stream << 14) + 32` and `skip = r20 = 0`.
- **Host:** the same bits are `output_buffer_padding` (context.h:75) and are used only as ring
  headroom (:253-255, :610-614).

### Fix (cvar on)
- At the first successful decode after a Clear/Release (`start_pending_`, armed in
  ResetDecoderState), discard `StartSkipBlocks(field, stereo, remaining)` blocks from that frame's
  output.
  - The count is min(field, 4) subframes, x2 for stereo, clamped to what is left after any loop
    skip.
  - 4 (whole frame) follows the loop-skip convention (:849-855). The guest never writes more than
    3.
- The field's use as padding is left unchanged, to keep the experiment one-variable.
- The field value is recorded in the first-decode record and the group line whether or not the cvar
  is on.

### Uncertainty
- Bits 24-26 are carried as "padding" from Xenia. The guest code shows the game writes a
  start-subframe index there. It is not known whether the hardware discards those subframes, or
  whether it also discards a whole warm-up frame after a guest-set offset (seek).
- If the hardware does neither, applying the skip would itself misalign siblings with different
  start subframes. Use the trace-only arm first: if every sibling has `start_skip=0`, D1 cannot be
  the cause and this cvar is a no-op.
- Arming happens only on Clear/Release. A guest seek without a Clear (only the offset rewritten)
  is not detected.

## (4) `xma_reset_codec_on_clear`
- Upstream `ResetDecoderState` (:328-340) calls `avcodec_flush_buffers`.
- The `xmaframes` codec has no `.flush` callback (thirdparty/FFmpeg/libavcodec/wmaprodec.c,
  `ff_xmaframes_decoder`, about 2146-2159; checked). The WMA Pro overlap/IMDCT state therefore
  survives, and a reused context adds the previous wave's tail to its first frame.
- With the cvar on, the context is freed and re-allocated with rate 0 and channels 0. The next
  decode's `PrepareDecoder` (:618) then re-opens it, and the first-decode record shows
  `codec_reinit=1`. Counted as `codec_resets`.
- Added null guards: PrepareDecoder handles a failed allocation.
- **Risk:** one codec open per wave start. Cost is small; it is the same code path as a rate change.
  The click it removes is not the frame-exact doubling.

## Guest-side consume hook (HOST change; not implemented here)
Purpose: per-hardware-context sum of samples the guest consumed, compared with the host's
`frames_emitted * 512` and `consumed_blocks * 128`.

- **Function:** `sub_823ED700` (mcla_recomp.25.cpp 40311-), called from the voice pump
  `sub_823FC9F8` at mcla_recomp.26.cpp 5692 after the resampler `sub_823FF5F8`.
- **Arguments:**
  - r3 = XMA voice object (the pump passes `[voice+184]`)
  - r4 = stream index i
  - r5 = samples consumed from this stream (per channel; the resampler's return value)
  - r6 = pointer to a u32 that receives the next read address
- **Body:**
  - The stream record is `rec = [r3+8] + i*96`. It holds a shadow of the hardware context at
    rec+0..63 (dword1 at +4, dword9 at +36).
  - `+68` is the output buffer base. `+80` (u16) is the hardware context id, written at create by
    `sub_823EDCD8` as `(phys - array_base) >> 6` (the host's context index). `+82` (u16) is the byte
    offset inside the current 256-byte block.
  - Bytes = r5 << (stereo + 1), with stereo = dword1 bit 29. The consume advances +82 and, at a
    block end, the dword9 read offset (mod block count), and sets dword1 bit 31 (valid).
- **Hook:**
  - `REX_EXTERN` wrapper that, before calling the original, reads `rec = load32(r3+8) + r4*96`,
    `hw = load16(rec+80)` and `stereo = (load32(rec+4) >> 29) & 1`, and adds r5 to a 320-entry
    atomic array `consumed_samples[hw]`.
  - Optionally a wrapper on `sub_823FC9F8` that reads the per-stream pad count (+24 - +28 after the
    pump loop, per the RE) and records the zero-pad counts.
  - Log once per second next to the SDK group line, keyed by the same context ids.
  - Equal `consumed_samples` with `lost` differing by k means k frames of host drop. Unequal pads
    means guest underflow.

## Tests
New `tests/unit/audio/xma_sibling_trace_test.cpp` (8 cases, `[ki071]`):
- boundary decisions with the fix off (upstream actions)
- stall with the fix on, and the stall limit
- the error_status-4 clear rule
- start-skip accounting (mono, stereo, 4 = whole frame, clamp)
- grouping with the l73 cut-scene pointers plus a radio pair
- grouping when both buffers link and a self link is ignored
- the group delta computation
- exact group and minute line formatting

Results:
- ctest (l74-sdk-xma2-b, Release, -j 8): 2086 tests. 2082 passed, 4 skipped (the BitStream Write cases, as at baseline), 0 failed. That is the baseline of 2074 passed plus 8 new.
- No new compiler warnings: the only warnings in the XMA files are the pre-existing ones in decoder.h (ppc_context) and xma_decoder.cpp (avcl, sign-compare). Their line numbers shifted; the warnings are identical to the l73 build.
- git-clang-format against 4a3aea1 is clean, and the sources are ASCII.

## Build and install
- Build: `private/l74-sdk-xma2-b` (output `private/l74-sdk-xma2-o`). Same configuration as l73-sdk-xma-b:
  - VS 2022 Build Tools, LLVM clang, Ninja Multi-Config, Release
  - -march=x86-64-v3, D3D12, Tracy, perf counters
  - Scripts: `%TEMP%\claude\C--BDU-MCLA-Recomp\l74-xma2-scripts\{build,test,install}.ps1` (adapted from l73-xma-scripts).
- Worktree third-party folders were copied from `l73-sdk-xma-wt` as before.
- Installed into `private/l74-sdk-install` (new folder) after a reconfigure. version.h reads `0.20.0.4-dev.g8f949f7`. SHA-256:
  - bin/rexruntime.dll   3c4fec5a16d1d9e7706b9c17f45208d7fbf1cb06b0938070119a5ffd9bbcd8dd (11339264 B)
  - bin/rexgpu-xenos.dll 150e2b6f40d87f9dc47460acf90a60316e01804745303927a3849c3a75c8b54b (3253248 B)
  - bin/TracyClient.dll  bdfde78135dba8739297e096118826f7fa71094642889de39e8324a589457ef4 (246784 B)
