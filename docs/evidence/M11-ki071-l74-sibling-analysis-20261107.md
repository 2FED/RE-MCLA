# KI-071 l74 owner sessions: sibling alignment analysis (read-only)

Read-only analysis by an Opus subagent on 2026-11-07 of the four l74 voice sessions (trace / d2 / d1 / all; parking-lot cut-scene); accepted by the primary agent.

Read-only analysis by an Opus subagent on 2026-11-07. Inputs: the four l74 owner session logs (profile 7d6ff1dd...,
sessions 5f37aed2 = trace, bd5284b0 = d2, 5fe79684 = d1, bf78b0ba = all), the KI-071 row, the sibling RE, the SDK
sibling-trace notes and the consume-probe notes, the SDK worktree at 8f949f7 (xma_context.cpp, wmaprodec.c) and
generated/default/mcla_recomp.26.cpp. Nothing was built or launched. No physical result is claimed.

## Verdict
- The host decodes the cut-scene siblings in exact lock-step, and the guest consumes them in lock-step:
  - same kick burst at the start
  - identical frames_emitted, frames_parsed and consumed_blocks every second for all 7 contexts, in all 4 arms
  - zero drops on every path, start_skip 0 on every sibling
- The 512/1024-sample doubling is therefore not produced by any of these:
  - frame loss (D2)
  - start-skip handling (D1)
  - decode timing or order
  - guest zero-pads
  - output-ring start offsets
- The offset must already be inside the decoded PCM content of some siblings, or be added by the guest mix.
- Most likely mechanism: the xmaframes decoder parses the per-frame start-skip and end-skip (trim) fields of the XMA/WMA
  Pro frame header and throws them away (`int av_unused skip`). Real hardware trims those samples. Siblings whose
  streams carry a different trim (0, 512 or 1024 samples) then play exactly 1 or 2 frames late on the host, even though
  the frame counts stay equal.
- Next experiment: log the parsed trim fields per context. Then decide whether to add a `xma_honor_frame_trim` cvar,
  with a per-context PCM dump as a cross-check (section 6).

## 1. The cut-scene voice siblings per session
- The first-decode records of the siblings are **missing in all four sessions**.
  - The shared token bucket (8 lines/s) is exhausted during gameplay. For example, the trace session has 3062
    first_decodes in the minute but only about 8 logged per second.
  - So read offsets and buffer pointers of the siblings are not available.
- Instead, the siblings were identified from the `[xma] group:` lines:
  - 44.1 kHz contexts (about 86 frames/s)
  - first seen in one kick burst
  - identical counters for about 40 s, up to the end of the session
- The groups are **7 contexts**, not 5. In every arm they split into a contiguous pair plus a contiguous five. This is
  two separate reads, or two stems, started in the same kick burst. The group heuristic reports them as 2 + 5, and as
  changing subsets once buffers swap. Kick ids are equal across all 7.
- All 7 contexts are mono with start_skip = 0 and skipped_blocks = 0, including in the d1 and all arms.

| arm (session) | span | pair | five | first group line (frames_emitted / consumed_blocks / kick) | last line |
|---|---|---|---|---|---|
| trace (5f37aed2) | 11:21:59.5 - 11:22:39 (exit) | 82,83 | 84-88 | 35 / 113 / 8513 for all 7 | 3393 / 13546 |
| d2 (bd5284b0) | 11:41:48 - 11:42:29 | 61,78 | 79-83 | 40 / 134 / 11257 for all 7 | 3485 / 13913 |
| d1 (5fe79684) | 11:44:27 - 11:45:07 | 7,22 | 23,54,73,75,76 | 6 / 0 / 8002 for all 7 | 3451 / 13777 |
| all (bf78b0ba) | 11:47:08 - 11:47:48 | 14,38 | 39,40,52,62,63 | 10 / 16 / 8486 for all 7 | 3456 / 13797 |

- A 48 kHz contiguous pair of 31-packet buffers runs alongside, in the same region as the radio stream. It is probably
  the scene's music bed. It cannot make a 44.1 kHz-frame lag.
  - trace: 1/7 at 03FA6000 + 03FB5800
  - d2: 30/45
  - d1: 40/41
  - all: 4/17
- The earlier start_skip lists such as [3,1,2,2,0,2,0] belong to one-shot SFX contexts that the group heuristic lumps
  together (for example the 04DC1000/04DC6000 seek starts). They are not voice siblings.

## 2. Emission and consumption over the cut-scene
- **Host, all 7 siblings, every second the group lines show them (40-42 s per arm):** max spread of frames_emitted,
  frames_parsed and consumed_blocks is 0 in d2, d1 and all.
  - In trace there is one transient: at 11:22:24, ctx 82 was snapshotted 7 kicks later than 83 (5 frames, 20 blocks
    ahead). It is equal again the next second.
  - lost (parsed - emitted) is 0 everywhere.
  - All drop counters (split, ff, err4, fail) are 0.
  - Over 39 s, each sibling emitted about 3360 frames, which is 86.1 frames/s = 44.1 kHz.
- **Consume probe:** the `ctxs=` list keeps only the 32 lowest ids, so it never shows the siblings with ids above 31.
  - trace (82-88) and d2 (61, 78-83): no consume data at all.
  - all: only ctx 14 is visible, so there is nothing to compare it with.
  - d1: ctx 7, 22 and 23 are visible.
    - From 11:44:29 to 11:45:07 they consume **exactly the same number of samples every second**: 43983 / 44217
      alternating, 39 of 39 seconds.
    - 22 and 23 are also equal in the first seconds (31744, then 41159).
    - ctx 7 differs only at 11:44:27-28, by the tail of its previous one-shot wave.
  - `run=` totals cannot be compared, because these ids were reused without an idle second, so the runs include the
    previous waves.
- **No sibling starts or ends one or two frames apart, and none is short by 512 or 1024 samples:**
  - Consumption equal to the sample (3 siblings, d1) and consumed_blocks equal to the block (all 7, all arms) exclude:
    - a late start
    - a zero-pad lag
    - a stale-prefix ring start. If the guest's read offset started k blocks before the host's write offset,
      consumed_blocks (= blocks_written - (write - read)) of that sibling would be k lower for the whole scene.
  - The end of the scene is not observed: the owner quit during the scene.

## 3. Timing at the start and the pad path
- **Same kick burst for all 7 (less than 1 ms between kick writes) in every arm.** In d1 the first group line shows 6
  frames emitted and 0 blocks consumed on every sibling, so every ring was filled before the guest's first read.
- The first-decode spacing cannot be shown (records suppressed, section 1).
- **Pad path** (loc_823FCD34, mcla_recomp.26.cpp:5785):
  - It zero-fills `[rec+24] - [rec+28]` floats per channel. That is the rest of the resampler's 256-sample **48 kHz
    output block**: shifted by 2 into bytes, with a 1024-byte channel stride and the channel count from byte +13. It
    sets r26 = 1.
  - So a pad is measured in output samples, at most 256 per call. It cannot produce 557.28 (= 512 x 48000/44100).
  - The equal consumption above shows that no sibling padded more than the others.

## 4. D1 (and the other fixes) versus trace
- The siblings' consumption and emission pattern is the same in all arms: equal, no drops, start_skip 0.
- **d1 / all:** start_skips_applied is 853-1705 per minute, but only on one-shot SFX. The siblings' field is 0, so
  skipped_blocks = 0. D1 does not change the cut-scene and **D1 is not the mechanism.**
- **d2 / all:** stalls = 0, stall_timeouts = 0 and err4_cleared = 0. D2 never fired.
- **all:** codec_resets = 779 / 2967 with no audible change. Cross-wave IMDCT residue is excluded.
- 4a3aea1 counters: carry_nonzero = 0, stale_ring_replays = 0 and lock_races = 0 in every arm.

## 5. Radio and music pairs
- **44.1 kHz radio:**
  - Pairs: trace 12/14 (03F61800 + 03F71000, 31 packets each, 72 s), d2 84/85 (74 s), d1 33/34 (49 s), all 1/3
    (03F60800..., 59 s).
  - Host counters are equal every second, with drops 0 and start_skip 0. The trace pair has one 5-frame snapshot skew
    at its first line.
  - Consume, where visible:
    - trace 12/14: 61 of 72 seconds equal; the rest are ±235 / ±47 / ±78 / ±95 pairs that cancel out. Those are one
      pump block (256 x 44.1/48 = 235.2) falling on the other side of the second boundary. The constant run offset
      comes from the earlier one-shot on these ids.
    - all 1/3: 59 of 59 seconds equal.
- **Menu music 0/1 (trace):** ±1 sample alternating, plus straddle pairs.
  - So the two resampler phases differ by less than one source sample. That is harmless and nowhere near 512.
- **48 kHz scene pairs (1/7, 30/45, 40/41, 4/17):** equal. They show 128/256-sample straddles only.

## 6. Conclusion, mechanism and next experiment
- **The host decoding is aligned, and so is the guest consumption.** All 7 siblings start in one kick burst, decode and
  consume identical amounts at block resolution all the time (sample resolution where the probe sees them), and lose
  nothing.
  - So the doubling is not produced by frame loss, decode timing, zero-pads or ring start offsets.
  - Frame k of every sibling reaches the guest at the same moment. A 1- or 2-frame offset must therefore be in **what
    frame k contains**: the decoded PCM of one sibling is shifted relative to another.
- The other proposals are excluded by this data:
  - Resampler phase or initial fill: at most one source sample, see the menu pair.
  - Different ring read start: consumed_blocks would differ by 4 or 8.
  - 512 versus 128 in the first "available" poll: FIFO with identical consumption; no pad.
  - Latency compensation from output_buffer_write_offset: it would show up as unequal consumption or a pad.

### Most likely mechanism: the per-frame trim is ignored
- In `wmaprodec.c` `decode_frame` (around lines 1462-1479; it is used by `ff_xmaframes_decoder`), the frame header
  carries an optional start skip and end skip ("usually true for the first frame", "sometimes true for the last
  frame"). Each is 10 bits for 512-sample frames.
- The decoder reads both fields into `int av_unused skip` and discards them. Every frame is output in full (512 samples),
  and `skip_frame` is 0 for non-WMAPRO codecs.
- Hardware trims those samples (this is the WMA Pro encoder-delay mechanism; FFmpeg compensates for WMAPRO only, by
  dropping the first frame).
- If the cut-scene channel streams carry different trims, the host plays some siblings 512 or 1024 samples late while
  the counts stay equal. This fits every observation:
  - the lag is exact source frames (557.28 / 1114.56 at 48 kHz) and constant over the scene
  - only some channels carry the copy, and the pattern differs per line (per-channel offsets combined with per-line
    panning)
  - only multi-context 44.1 kHz content is affected
  - the console is clean
  - none of D1, D2, C1-C3 or the codec reset changed anything
- **Unverified:**
  - the values of the fields in this game's streams
  - whether the hardware trims whole-frame values. A 1024 lag needs either two frames with a 512 trim or a trim that
    spans frames, because one 10-bit field tops out at 1023.
- **Second candidate:** the two stems (the pair and the five) both carry the dialogue with an authored offset, and the
  console mixes only one of them (a game-side selection we answer differently). The trim log below separates the two:
  zero trims everywhere means the offset is not decoder-side.

### Next experiment (trace-only arm, same scene)
1. SDK: in `decode_frame`, store the parsed start and end skip into the decoder context instead of discarding them, and
   read them after `DecodePacket` in `XmaContext::Decode`.
   - Count frames with a non-zero start or end skip per context, as sum and maximum, in the group line, for example
     `trim_start=[..] trim_end=[..]`.
   - Log the first 4 non-zero values per context, paced on their own budget.
   - Then add `xma_honor_frame_trim`, default false, that discards start-skip samples from the head and end-skip
     samples from the tail of that frame's output. These are 128-sample blocks when the values are multiples of 128.
   - Expected result if the hypothesis holds: the siblings' summed trims differ by 512 or 1024.
2. Fix the instrumentation gaps that hid the siblings in l74:
   - give the consume probe a sibling-aware or larger cap (for example all ids, or 96 entries, or the ids in groups
     of 3 or more)
   - give first-decode records of contexts kicked in a 3-or-more 44.1 kHz burst their own budget
3. Cross-check without owner listening: `xma_pcm_dump` for the contexts of that burst (mono 44.1 kHz WAV per context,
   first 45 s). Then cross-correlate the siblings offline.
   - Peaks at 512 or 1024 confirm a content offset in the decoded data (trim or authoring).
   - Lag 0 everywhere means the guest mix adds the copy. The next RE target is then the voice's output-matrix and
     environment path.
