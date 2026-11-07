# KI-071 l75 owner sessions: per-frame trim analysis and dump side check (read-only)

Read-only analysis by an Opus subagent on 2026-11-07 of the three l75 trim sessions and the trim-trace WAV dump (intro-music pair); accepted by the primary agent.

Read-only analysis by an Opus subagent on 2026-11-07. Inputs: the three l75 owner session logs on the parking-lot
cut-scene (profile 254a4ba8..., sessions f6e21d26 = trim-trace, 0355386b = trim-fix (`xma_honor_frame_trim`),
dc1f0181 = trim-block (+ `xma_frame_trim_block_granular`)), the trim-trace PCM dump folder (64 WAVs), the l74 sibling
analysis, the step-3 SDK notes and the SDK worktree at c526cb3 (`xma_context.cpp`). Nothing was built or launched.
No physical result is claimed. The owner heard no difference between the three arms.

## Verdict
- **The per-frame trim is not the mechanism.** All 7 cut-scene siblings carry identical trims in all three
  sessions: one start trim of 512 samples on frame 0 and one end trim of 119 samples on the last frame. There are no
  other trims and no parse errors. The fix was applied in trim-fix and trim-block, but it dropped the same 512
  samples from every sibling, so their relative alignment did not change. This matches the owner hearing nothing.
- **New, stronger finding: the host inserts (or drops) whole frames in some contexts, at fixed stream positions.**
  - The trace arm's dump captured the intro-music pair (ctx 0/1). It shows a mid-stream one-frame slip: lag 0 for
    the first ~11.6 s, then exactly +512 samples (ctx 0 late). On the second pass of the looped track the slip
    happens again at the same position, and the lag grows to +1024.
  - The frame-trim end records agree: ctx 0 reaches its end-trim frame at index 1128, ctx 1 at 1127, with the same
    end trim (356).
  - The cut-scene siblings show the same signature. The end trim is identical (119) on all 7, but the index of the
    frame that carries it differs: 3624 / 3624 / 3625 / 3625 / 3627 / 3627 / 3627. The same values come back, per
    buffer and per read offset, in the trace and trim-fix sessions.
  - So the siblings leave the host 1, 2 or 3 frames apart. This explains the measured echo taps at 1 and 2 frames
    (557 / 1115 samples at 48 kHz).
- The lock-step counters could not see this. frames_emitted, frames_parsed and consumed_blocks are bounded by ring
  space, which the guest drains in lock-step. A context that decodes one extra frame (or skips one) still writes
  the same number of blocks per second; only *which* content occupies them changes. The PCM dump records exactly
  what `Consume` wrote, so the slip happens on the host, before the guest pump. This rules out the guest resampler,
  latency compensation, the guest mix and the SDL output stage.

## 1. The cut-scene voice siblings per session
Sticky group (one serial per scene), 7 mono 44.1 kHz contexts, one start burst. First-decode records are all present
(`budget=burst` / `burst-late`), all with entry_read_offset 32, start_skip 0, `trim=512:0 trim_ok=1`.

| arm (session) | group id | start_kick | ctxs | first / last group line | last frames_emitted / consumed_blocks |
|---|---|---|---|---|---|
| trim-trace (f6e21d26) | 62 | 10674 | 72,73,74,77,78,80,83 | 13:55:55.3 - 13:56:37.3 (exit 13:56:37.9) | 3621 / 14457 |
| trim-fix (0355386b) | 54 | 10015 | 67,79,82,83,84,85,86 | 13:59:13.7 - 13:59:54.7 (exit 13:59:55.9) | 3569 / 14246 |
| trim-block (dc1f0181) | 47 | 7670 | 52,54,65,68,75,77,79 | 14:01:46.6 - 14:02:27.6 (exit 14:02:28.9) | 3617 / 14437 |

The first buffers are the same in all arms, so the identities are stable across sessions:
- pair 03FEC000 (32 packets) + 03FFC000 (30)
- five 03F5F800 (19) + 03F69000 (10) + 03F6E000 (3) + 03F6F800 (11) + 03F75000 (19)

Contiguous buffers confirm the l74 "pair + five". One sibling per arm had `codec_reinit=0`: trace ctx 74, fix ctx 84
(and none in block). Per the l74 RE, this is a click candidate on the first frame, not a shift.

## 2. Trims per sibling
- **Group line, every second, all 7 siblings, all 3 arms:**
  - `trim_start=[1:512 x7]`
  - `trim_end=[0:0 x7]` up to the last group line
  - `trim_first=[512:0 x7]`
- **End of scene** (`[xma] frame-trim:` lines, emitted after the last group line): every sibling has `end=119`.
  - The siblings differ in the index of that frame.
  - The final read offsets are byte-identical between trace and fix, so the streams are the same and the result is
    deterministic.

| buffer identity (final read_offset) | trace ctx | fix ctx | end-trim frame index | end trim |
|---|---|---|---|---|
| 179385 | 72 | 67 | 3627 | 119 |
| 166143 | 77 | 83 | 3627 | 119 |
| 80623 | 83 | 86 | 3627 | 119 |
| 163263 | 73 | 79 | 3625 | 119 |
| 84633 | 80 | 85 | 3625 | 119 |
| 13869 | 74 | 82 | 3624 | 119 |
| 149193 | 78 | 84 | 3624 | 119 |

- In trim-block, the frame-trim budget (the first 4 non-zero trims per context) logged only ctx 52 (3627). The
  other end lines were suppressed or not reached before exit.
- So the start trims are equal, the end trims are equal, and none of the per-sibling trim sums differ by 512 or
  1024. trim_parse_errors = 0 in every minute line of every arm.
- What does differ is the number of frames the host walked before reaching the frame that carries the shared end
  trim: 3624 / 3625 / 3627. An identical end trim (119) means the channel lengths are congruent modulo 512. Channels
  of one multichannel source have identical length. The 1-3 frame difference is therefore most likely host-made,
  unless the authored stems really differ in length by whole frames.

## 3. trim-fix / trim-block: was the fix applied?
- **trim-fix minute lines:**
  - `trim_fix=1 trim_block=0`
  - `trim_applied_samples` 737143 and 1601222, equal to trim_samples
  - `trim_unapplied=0`
- **trim-block minute lines:**
  - `trim_fix=1 trim_block=1`
  - `trim_applied_samples` 791808 and 1369856
  - `trim_unapplied` 7909 and 16511 (the non-multiple-of-128 end trims, as designed)
- **Siblings:** 4 x frames_emitted - consumed_blocks at the last line is 27 (trace), 30 (fix) and 31 (block).
  - That is 4 fewer blocks written per sibling: the 512-sample start trim, equal on all 7.
  - Spreads across siblings are equal, so emission and consumption relations are unchanged apart from that
    uniform block.
  - The end trim (119) falls on the last frame, at the scene end, after the owner had already heard the scene.
- **Why it was inaudible:** the fix removed the same 512 samples (one frame of decoder delay) from every sibling at
  frame 0. That is a uniform 11.6 ms latency change, with no relative shift. The real relative offset (1-3 whole
  frames) comes from the frame walk, not from the trim fields.

## 4. The dump
- The cap was exhausted before the cut-scene.
  - `[xma] dump: file limit 64 reached` came at 13:54:31.836 (the 64th file, ctx17 n=63, opened at 13:54:31.548 and
    closed at the 60 s cap at 13:55:01.7).
  - The cut-scene burst started at 13:55:55.3, about 84 s later. No sibling was dumped.
- **Files:**
  - n=0/1: ctx 0/1, 24.89 s each, opened 13:54:05.177/.179. This is the intro music: buffers 03F18000 + 03F27800,
    31 packets each, contiguous.
  - n=63: ctx17, 60 s.
  - The rest are menu SFX of 0.07 - 4.76 s, mostly pairs of adjacent contexts (2/4, 5/6, 9/10, 13/14, 43/44 ...).
- **Cross-correlation, music pair ctx0 (x) vs ctx1 (y):** high-passed at 1.5-2 kHz and normalised. A positive lag
  means x is late.
  - Overall: peak at lag +512 (0.46 high-passed; PHAT also +512). The broadband peak is at +545..548 (low-frequency
    smear).
  - Windowed:

    | window | lag | correlation |
    |---|---|---|
    | 0 - 11.5 s (frames 0 - 1003) | 0 | 0.86 - 0.98 |
    | from frame 1006 (11.73 s) | +512 | 0.88 - 0.97 |
    | from output frame ~2134 (24.84 s) | +1024 | 0.93 - 0.94 |

  - The transition is at ctx0 output frames 1004-1005. Neither channel has a click there, and there is no bit-exact
    repeat (an extra decode with a different MDCT overlap state, or a lost frame, is smooth in music).
  - The track loops: both contexts restart at read_offset 32 in kick group 914. ctx 0 parsed 1128 frames per pass,
    ctx 1 parsed 1127. Pass 2 starts at output frame 1129, so pass-2 frame 1004-1005 is output 2133-2134. That is
    where the second slip appears, so the slip is **deterministic at a fixed stream position**, and it accumulates
    across loops.
  - This validates the method. It shows that a host slip produces exactly a 512-sample offset with equal block
    counts, and it is the first direct evidence of a whole-frame content offset in decoded data.
- The other dumped pairs (2/4 n=32/33, 2/4 n=57/58, 9/10, 13/14, 43/44) are different SFX: correlation 0.06-0.22,
  no 512/1024 peaks. The first 0.3 s of ctx2/4 n=32/33 correlate at lag 0 (0.93) and then diverge as content.

## 5. Conclusion and next experiment
- **Trim hypothesis rejected:**
  - 7/7 siblings have trim_start = 1 frame : 512 and trim_end = 1 frame : 119, identical.
  - 0 parse errors.
  - The fix was applied (about 0.74-1.6 M samples per minute overall, 512 per sibling) and changed no relative
    alignment.
- **Mechanism now indicated:** a deterministic host frame-walk defect. At certain stream positions (packet or buffer
  layouts) a context decodes one frame more, or skips one frame without counting it. Each such position shifts that
  context's content by 512 samples from then on.
  - Siblings meet these positions differently because their packet layouts differ, so they leave the host 1-3
    frames apart, exactly as measured.
  - The case list in the brief, revisited:
    - (a) Authored per-channel delay: contradicted for the music pair, since the slip is mid-stream and repeats per
      loop.
    - (b) XMA2 start/loop fields: the start skip is 0 and the start trim is equal.
    - (c) Guest latency from the write offset, and (d) SDL 6-channel output: both excluded, because the offset is
      already in the PCM that `Consume` writes.
- **Candidate code paths** for the RE step (not proven):
  - the header-split path (`current_frame_size_ == 0`) together with the `isLastFrameInPacket()` and frame-count
    logic of `GetPacketInfo` (the walked count versus the XMA2 header count). When the frame is not seen as last,
    `!isLastFrameInPacket` computes the next offset modulo the packet inside the *same* packet index. That leads to
    a re-decode, or to the `relative_offset < first_frame_offset` jump.
  - the spanning-frame and swap paths that resume at packet 0's first-frame offset
  - the 0xFF skip

### Next experiment (one session, no listening; the menu music is enough)
1. **SDK, frame-continuity trace (per context, always counted, logged with `xma_sibling_trace`):**
   - For every frame handed to FFmpeg, record its absolute stream position: buffer serial (swap count since Clear),
     packet index, bit offset and size.
   - Check that it starts exactly where the previous frame ended. This accounts for packet headers, packet skip
     counts and swaps.
   - Count `re_decodes` (a start at or before the previous start) and `gaps` (bits skipped without a decode).
   - Log the first 8 anomalies per context with: frame index, path (normal, split-header, spanning, last-in-packet,
     swap, 0xFF), the packet header fields (first-frame offset, skip count, XMA2 frame count, walked frame_count_,
     current_frame_) and the read offsets before and after.
   - Expected: ctx 0 of the intro music shows exactly one anomaly near frame 1004 on every pass, and the cut-scene
     siblings show 0-3 anomalies that differ per sibling.
2. **SDK, input-packet dump** (`xma_dump_input`, bounded, 44.1 kHz only): write the raw input buffers of
   selected contexts at each swap. A Python XMA2 packet/frame walker can then count the true frames per stream
   offline and identify which side (insert or skip) is wrong.
3. **Dump selection, so the cut-scene is captured:** only dump contexts from a start burst of 3 or more 44.1 kHz
   contexts, or add `xma_dump_start_after_s` or a per-group filter. A 64-file cap spent on menu SFX cannot reach
   the scene.
4. **Owner run:** a trace-only arm (dump + continuity trace). Wait about 30 s at the menu (two music passes), then
   play the parking-lot cut-scene.
   - With (1) and (3), the offline cross-correlation of the 7 siblings should show their pairwise lags equal to the
     differences in their anomaly counts.
   - After that, a fix behind a cvar (correct walk at that path), with the music pair's +512 slip as a regression
     test that needs no listening.
5. **Side check on the l72 Game Bar recording:** look for a 3-frame tap (1671.8 samples at 48 kHz). The l72
   measurement tested only d and 2d. The 3624-versus-3627 siblings predict a 3-frame tap whenever both carry voice.

### Do not
- Do not make `xma_honor_frame_trim` a default. It is equal across siblings and only shifts global latency by
  11.6 ms (hardware probably honours the 512 start trim, but nothing depends on it).
