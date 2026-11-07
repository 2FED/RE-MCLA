# KI-071 step 4: XMA frame-walk defect W1 (split 15-bit header at a packet end skips a frame), continuity trace, fix, input dump (SDK)

Implementation by an Opus subagent on 2026-11-07 as SDK commit `b6d361e` on local branch `mcla/m11-xma-walk` (parent `bd9314f`); accepted by the primary agent as code: ctest 2110/2110 (4 skipped), no new warnings; the defect is established by static reasoning and bit-exact synthetic tests, the owner l76 trace run decides whether MCLA's packets trigger it (`walk_split_lost > 0`). Installed as `private/l76-sdk-install` (b6d361e): rexruntime.dll 75E3CA910041421AE66CAA6422168E9125264B519C4AA9DBE1A354E0B28A6BDA, rexgpu-xenos.dll D096A23628A5D688ADE62732B507D89BAD67B975C86205306F80DAEE885B7DAD, TracyClient.dll A63DC7A08868301A60EEC2F5DF66F667589E3CFB3F2F475DDC1056EEB7C0663D (to be superseded by the l76 merge with M11-007c).

---

# KI-071 step 4 (l76 SDK): XMA frame-walk defect, continuity trace, fix, input dump

Draft by an Opus implementation subagent on 2026-11-07; for review by the primary agent. Nothing was launched;
no physical result is claimed. SDK worktree `private/l76-sdk-xma4-wt`, branch `mcla/m11-xma-walk` from
`mcla/m11-steady2` bd9314f. Build `private/l76-sdk-xma4-b` (output `l76-sdk-xma4-o`), install `l76-sdk-install`.
Commit, hashes and test results: see the end of this file (filled in after the build).

## 1. The frame-walk state machine (xma_context.cpp at bd9314f)

Units: `R` = `input_buffer_read_offset`, absolute bit offset in the current input buffer (packet `k = R / 16384`,
packet-relative `r = R % 16384`; the 32-bit packet header occupies r 0..31). Packet header: frame count `C`
(6 bits, XMA2 only), first-frame offset `F` (15 bits, stored +32 as an absolute packet bit), metadata (3 bits,
1 = XMA2), skip count `s` (8 bits). Frame: 15-bit length `L` (bits, length field included), payload, trailer bit
(the frame's last bit; 1 = the next frame starts right after this one in the same packet). Frames are packed back
to back across packet payloads (16352 payload bits per packet).

One `Decode` = one frame:
1. **Entry**: swap if the current buffer is invalid; clamp `R >= 32`; `k`, `r`.
2. **Resync to F**: `if (r < F) R = k*16384 + F` (start of buffer, after a swap, after a spanning frame).
3. **0xFF skip packet**: `R = first F of packets k+1..` (`GetNextPacketReadOffset`); none -> swap; no decode.
4. **Packet analysis** (`GetPacketInfo`, now `xma::AnalyzePacketFrames`): walk the 15-bit lengths from `F`:
   stop when fewer than 15 bits remain, on `L == 0 / 0x7FFF`, after a frame that spans (`L > bits left`, counted)
   or after a trailer 0. `walked` = frames found; the frame at `r` gets `current_frame` / `current_frame_size`.
   XMA2 fix-up: if `C > walked`: `frame_count = C` and, when the frame at `r` was not found (`size == 0`),
   `current_frame = walked`. `isLast = (frame_count == 0 || current_frame == frame_count - 1)`.
5. **Split-header path** (`current_frame_size == 0`): read `L` from the two payloads joined (next packet =
   `k+s+1`, or the other buffer's packet 0); next packet missing -> swap (frame lost, `drops_split`).
6. **Bits**: `bits_to_copy = min(16384 - r, L)`; if `isLast` and the frame spans, the next packet's payload is
   appended (missing -> `error_status = 4`, retried later). Copy `L` bits, decode one frame (FFmpeg xmaframes:
   exactly one 512-sample frame per packet sent).
7. **Next offset**:
   - loop end -> `loop_start`;
   - `!isLast` -> `R = k*16384 + (R + bits_to_copy) % 16384` (same packet);
   - `isLast` -> `R = first F of packets k+s+1..`; none -> swap and `R = F` of the new buffer's packet 0 (C1 carry
     optional), swap-back if that packet has no frame start.

Transitions and their arithmetic, checked for re-decode (dup) and pass-over (gap):

| transition | arithmetic | dup? | gap? |
|---|---|---|---|
| in-packet frame, more follow | `R + L`, frame ends inside packet | no | no |
| frame ends exactly on packet end, last | trailer 0 (or 1 with 0 bits left): walk counts it as last -> `F(k+1) = 32` | no | no |
| same, but `!isLast` (C counts one more) | `(R + L) % 16384 = 0` -> `R = k*16384` -> step 2 -> `F(k)`: whole packet re-decoded, endless loop | would be, but an audible endless loop, not seen | - |
| spanning last frame | `F(k+s+1)` = right after the continuation | no | no |
| split header, C counts it (spec) | P not last -> `R = end of P`; S: `current_frame = walked`, last, joined read, `F(k+1)` | no | no |
| **split header, C does not count it** | **walk breaks with < 15 bits left before counting S; `frame_count = walked`; P is last -> `R = F(k+1)` = after S** | no | **yes: S never decoded** |
| split header, S is the packet's first frame | `walked = 0` -> `frame_count = 0` -> last -> split path | no | no |
| first-frame offset in (16352, 16383] | packet treated as "no frame start" | no | yes, but needs a > 16321-bit frame (impossible here) |
| buffer swap (both cases) | `F` of new packet 0 | no | no |
| split / spanning with next buffer not valid | drop+swap / error 4 (counted; all 0 in l74/l75) | no | counted |
| 0xFF packet | skips to the next `F` | no | only if R pointed at a real frame there (not reachable by the walk itself) |

## 2. The defect (W1): a frame whose 15-bit header straddles a packet end is skipped

**Trigger:** frame P ends with `1 <= b <= 14` payload bits left in its packet and has trailer 1, so the next frame S
starts there; its 15-bit length straddles the boundary (`b` bits in packet k, `15-b` in packet k+s+1).

**Arithmetic:** the walk (`while`, `BitsRemaining() < 15 -> break`) stops before counting S, so `walked = n`
(P = index n-1). Upstream counts S only through the header: `if (C > walked) frame_count = C`. If `C = n` (the
header does not count S: an encoder that counts frames whose length field is complete, or a non-XMA2 metadata
value), P is the last frame: `isLast` -> `R = F(k+1) = 32 + (L_S - b)`, which is the first bit after S.
S is never decoded. No counter sees it (it is not a drop, an error or a decode failure), `frames_parsed` simply
grows one less, and from then on the context's content is 512 samples EARLY relative to its siblings.

**Direction and evidence fit:** a skip, so the context with fewer walked frames is the defective one:
- music pair: ctx 1 walked 1127 frames per pass, ctx 0 1128 -> ctx 1 lost one frame near frame 1004 on every pass,
  so ctx 0 appears late by +512, then +1024 after the next pass (deterministic: same packet layout each pass).
- cut-scene voice: true length 3627 (3 siblings, no event); 3625 lost 2, 3624 lost 3.
- rate: a boundary hits the first 14 bits of a frame with probability about 14 / mean frame bits per packet; for
  ~1500-bit music frames (~11 frames/packet, ~103 packets/pass) about 1 event per pass; for the voice streams
  1-3 per scene. This matches the observed 0-3 frames per stream and one slip per music pass.
- it also predicts the 3-frame tap for the 3624-vs-3627 siblings (side check on the l72 recording).

**Not established statically:** whether MCLA's packet headers really leave S out of `C` (the trigger requires
it; with `C` including S the upstream walk is correct). The new counters answer this directly in one run
(`walk_split_lost` vs `walk_split_counted`, and `walk_gap`), and the input dump lets an offline walker verify
it frame by frame. No duplicate (re-decode) mechanism was found: the only one (`!isLast` with an exact packet
end) would loop endlessly on one packet, which is not what is heard.

## 3. Trace (always counting; lines behind `xma_frame_walk_trace`, default false)

Per context: `walk_prev_` = guest address of the packet holding the previous frame's last bit, the bit after it,
and its trailer. Each decoded frame is classified (`xma::ClassifyFrameStart`): same packet and start < end ->
**dup**; same packet, start > end with trailer 1 -> **gap**; other packet while the previous frame had trailer 1
and bits left -> **gap** (W1 signature); other packet -> must start at that packet's first-frame offset. The chain
restarts on Clear/Release, loop jumps and guest seeks (read offset or buffer changed between Works ->
`walk_resyncs`). Split-tail census at the frame P: `walk_split_tail` (P ends with 1-14 bits left, trailer 1),
`walk_split_counted` (header counts S; upstream fine), `walk_split_lost` (upstream skips S),
`walk_split_claimed` (fix), `walk_split_real` (the next packet's first-frame offset matches a frame of S's length
starting at the tail: `CheckSplitTail`).

`MCLA_XMA_SIB v=3` appends: `walk_dup walk_gap walk_ctxs walk_boundary walk_split_tail walk_split_counted
walk_split_lost walk_split_real walk_split_claimed walk_split_padding walk_dup_prevented walk_resyncs walk_fix
input_dump_files`.

`[xma] walk:` line (first 8 anomalies per context, budget 96 burst / 8 per s): ctx, kind, frame index, path
(in-packet / last / spanning / split-header), rate, packet index, buffer, packets, packet address, header frame
count, xma2, walked, frame_count, current, first-frame offset, skip, start bit, frame bits, bits in packet,
previous end (packet:bit), previous trailer, left bits, split tail (pos:bits), counted, claimed, read offset
before/after, after_swap, after_ff, dup_prevented, running dup/gap, fix. `[xma] walk-summary:` per context at
Clear/Release when it had anomalies.

## 4. Fix (`xma_frame_walk_fix`, default false)

- **W1:** when the walk ends with 1-14 bits left after a trailer-1 frame and the header does not count that frame,
  `ClaimSplitTail` counts it (`frame_count = walked + 1`; at the tail offset `current_frame = walked`, size 0), so
  P is not last, `R` moves to P's end and the existing split-header path decodes S. Claimed only when the next
  packet confirms it (`CheckSplitTail`: its first-frame offset leaves room for exactly, or at least, S's
  continuation) or is not available yet (then the split path stalls/drops as for any split frame). If a claimed
  tail later turns out to be padding, the walk continues as upstream would after P (`walk_split_padding`).
- **Defensive dup guard:** never decode from a bit before the previous frame's end in the same packet (same chain,
  trailer 1): `R` moves to that end (`walk_dup_prevented`).

## 5. Input dump (`xma_dump_input`, default false; needs `xma_dump_contexts`)

Opened together with a context's PCM dump (same burst-only / 44.1 kHz / 128-file rules, data held until the
burst decision, dropped if the stream is not dumped). Files next to `<wav>`: `<wav>-input.bin` (each input
buffer, all packets, 2 KiB each, written once per buffer swap in decode order; 32 MiB cap), `<wav>-input.csv`
(seq, buffer_epoch, buffer index, buffer pointer, packet count, packet, file offset, first-frame offset, header
frame count, skip, read offset), `<wav>-walk.csv` (one row per decoded frame: buffer epoch/pointer, packet, start
bit, frame bits, path, verdict, header count, walked, split tail, read offsets, swap/0xFF flags). Offline walker
(scratch, not in the repo): `l76-scripts/xma_walk_check.py <stem>` walks the true frame chain over the dumped
packets and lists missed/duplicated frames and every straddling header with its packet's header count; self-test
on a synthetic dump reproduces the W1 loss (`missing=[4]`, header_count 4 of 5 frames).

## 6. Tests (`tests/unit/audio/xma_frame_walk_test.cpp`, no listening)

Synthetic XMA2 buffers built bit-exactly (frames back to back over packet payloads, headers derived):
- W1 trigger (frame ends 10 bits before the packet end, 3000-bit straddling frame, header excluding it): upstream
  walk decodes 11 of 12 frames, the straddling frame missing and flagged as a gap; the fix decodes 12/12 once
  each, in order, with no dup/gap.
- header counting the straddling frame: 12/12 with and without the fix (no regression).
- exact packet end and exactly 15 bits left: 1:1 either way.
- 400 random layouts x {header counts / does not count}: the fix is 1:1 always; upstream is 1:1 when the header
  counts and loses exactly the straddling frames otherwise; never a dup.
- trailer-1 frame followed by padding: not claimed, walk continues at the next packet.
- packet analysis equals the upstream walk (frame indices, header fix-up, continuation quirk), offset helpers,
  continuity classifier, minute fields; the sibling minute-line test updated to v=3.
`SimulateBufferWalk` drives the same pure functions that `Decode` now calls (`AnalyzePacketFrames`,
`ClaimSplitTail`, `CheckSplitTail`, `ReadSplitFrameSize`, `NextReadOffsetInPacket`, `FindNextPacketReadOffset`,
`ClassifyFrameStart`) in Decode's order for one buffer; the control flow itself is mirrored, not shared.

## 7. Risks

- If MCLA's headers do count the straddling frame, W1 is not the KI-071 mechanism; then `walk_split_lost` stays 0
  and `walk_dup`/`walk_gap` plus the input dump must point at the real one. The fix is then a no-op.
- The fix trusts the trailer bit; `CheckSplitTail` guards against padding when the next packet is present.
- Counters run always (a few integer ops per frame; `GetNextPacket` + a 15-bit read only for packets with a split
  tail). Dump I/O only with `xma_dump_input`.
- The 6-bit header count cannot express more than 63 frames per packet; very low-bitrate (near-silent) streams
  could hit that limit too, which the same claim covers.

## 8. Suggested owner run (l76)
Arms: `walk-trace` (`xma_frame_walk_trace=true`, `xma_sibling_trace=true`, `xma_dump_contexts=<dir>`,
`xma_dump_input=true`), `walk-fix` (+ `xma_frame_walk_fix=true`). Wait ~30 s at the menu (two music passes),
then the parking-lot cut-scene. Expected with W1: trace arm `walk_split_lost` > 0 and one gap per music pass on
ctx 1; fix arm `walk_split_claimed` > 0, `walk_gap` 0, the music pair at lag 0 for the whole dump and the 7
voice siblings with equal end-trim frame indices.

## 9. Results (2026-11-07)
- Commit `b6d361e` on `mcla/m11-xma-walk` (parent bd9314f), local only, not pushed. Files: new
  `include/rex/audio/xma/frame_walk.h`, `tests/unit/audio/xma_frame_walk_test.cpp`; changed `xma_context.cpp`
  (Decode uses the pure helpers; `GetPacketInfo` removed, replaced by `AnalyzePacketFrames`; next-offset tail moved
  into `AdvanceAfterLastFrame`; `SwapInputBuffer` no longer static), `context.h`, `sibling_trace.h` (v=3),
  `xma_decoder.cpp`, tests CMake + sibling test string.
- Build `private/l76-sdk-xma4-b` (fresh configure; Release): no new warnings (only the pre-existing decoder.h /
  xma_decoder.cpp / -gcodeview ones). clang-format 20.1.8 clean on all changed files.
- ctest: 2114 tests, 2110 passed, 4 skipped (BitStream Write, as at baseline 2100 + 4); +10 new XMA walk tests,
  including a differential test showing `AnalyzePacketFrames` equals the verbatim upstream `GetPacketInfo` on
  > 1000 packet/offset cases (fix-off behaviour unchanged).
- Install `private/l76-sdk-install` (reconfigured, version.h carries b6d361e), SHA-256:
  - rexruntime.dll 75E3CA910041421AE66CAA6422168E9125264B519C4AA9DBE1A354E0B28A6BDA (11457536 bytes)
  - rexgpu-xenos.dll D096A23628A5D688ADE62732B507D89BAD67B975C86205306F80DAEE885B7DAD (3263488 bytes)
  - TracyClient.dll A63DC7A08868301A60EEC2F5DF66F667589E3CFB3F2F475DDC1056EEB7C0663D (246784 bytes)
- Scratch tools: `l76-scripts/xma_walk_check.py` (offline walker), `l76-xma4-scripts/` (build/install/test).
- Spark: not used for this task (a single-file state-machine RE plus implementation with deterministic tests; no
  bounded extraction or log triage to offload).
