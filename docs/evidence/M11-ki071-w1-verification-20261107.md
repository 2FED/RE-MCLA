# KI-071 W1 verification on the l76 owner sessions (read-only)

Read-only verification by an Opus subagent on 2026-11-07 of the l76 walk-trace / walk-fix sessions (logs, walk.csv, WAV + input dumps, independent offline frame walk); accepted by the primary agent.

Read-only analysis by an Opus subagent on 2026-11-07. Nothing was built or launched; no physical result is claimed.
The owner's verdict ("the doubling seems gone" on walk-fix) is the physical result; this note checks the data.

Inputs (profile 254a4ba8..., same parking-lot cut-scene after about 30 s at the menu):
- **walk-trace:** session 7002b4fc..., dump folder `xma-dump/20261107-135040`, 128 WAV files.
- **walk-fix:** session f4b4500c..., dump folder `xma-dump/20261107-135439`, 128 WAV files.

Tools: scratch Python scripts (`w1verify/`) using numpy/scipy for cross-correlation and the owner Python for log and
CSV parsing. The offline frame walker (`walker.py` + `cont.py`) was written independently for this check; it does
not reuse the SDK code or the earlier scratch walker. matplotlib is not installed, so there are no PNG plots; the
lag series are given as tables.

## Verdict
- **W1 is confirmed directly from the decoded data and the raw input packets.**
  - Offline frame walk: every frame the trace host skipped is a frame whose 15-bit length header straddles a packet
    end. There are 7 skips in the trace dumps and 0 in the fix dumps. Neither arm duplicated a frame or skipped any
    other kind of frame.
  - In the fix arm, every straddling frame the host reached was decoded, in order.
- **Fix music pair (ctx74/75): lag 0 throughout the 62 s.** The fix claimed 4 split frames in each context, at
  different positions. Without the fix, the pair would have been up to 1536 samples apart.
- **Cut-scene voice siblings: 7 contexts in both arms.**
  - Trace: they lost 2/2/2/4/4/5/5 frames. All 7 have 3630 true frames; the host decoded 3628/3628/3628/3626/3626/
    3625/3625. During the scene they were up to 5 frames (2560 samples) apart.
  - Fix: all 7 decoded 3630 frames, with the end trim on frame 3629 for every sibling.
  - The voice siblings were not in either dump (see Limits), so this comes from the logs, not from cross-correlation.
- **The fix arm's single `walk_split_lost=1` is not a real loss.** It is a tail that the next packet does not
  continue (padding), the same class as the 2 non-real tails in the trace arm. walk_gap stayed 0, and it did not
  occur in any dumped context.

## Limits of the dumps (affects questions 2 and 3)
- **The 128-file dump cap was reached about 46 s before the cut-scene in both arms.**
  - Trace: cap reached at 15:52:45, scene burst at 15:53:31.
  - Fix: cap reached at 15:56:23, scene burst at 15:57:09.
  - No voice sibling was dumped in either session.
- **Dumping began late.** The first dump opened at 15:52:10 (trace) and 15:55:57 (fix), when the sibling-group
  tracking began. The intro-music pair (ctx0/1, first decode at 15:51:13 in trace and 15:55:03 in fix) had started
  before that and is not in either dump.
- **The trace arm's other long music pair (ctx21/22, group 7, kick 6487) was not dumped.** It had no first-decode
  record.
- **No dumped trace group has correlated channels.** The checks below were all at or below the noise level:
  - g24, six contexts, 14.9 s: an ambience loop; pairwise r < 0.3, and no correlation at the loop period either.
  - g13: r ≤ 0.05.
  - g32: r ≤ 0.29.
- **What replaces the trace-arm cross-correlation:**
  - the per-context `[xma] walk:` gap records and `walk-summary` lines (complete for the contexts involved: each
    context's logged gaps equal its walk-summary gap count);
  - the `frame-trim` end records;
  - the offline walk of the dumped packets (question 5).
  - The l75 dump already showed the music-pair +512 step with cross-correlation (see the l75 analysis).
- **For a WAV-level sibling check, a rerun would need the dump aimed at the scene.** Options: a start delay, a larger
  file cap, or a filter for groups of 5 or more contexts.

## 1. Identification
| | trace (7002b4fc) | fix (f4b4500c) |
|---|---|---|
| intro music pair (2 x 44.1 kHz mono, one burst, 31-packet buffers, start read offset 32) | ctx0/ctx1, first decode 15:51:13 (buffers 03C58000/03C67800), not dumped | ctx0/ctx1, first decode 15:55:03 (03F18000/03F27800), not dumped |
| second long music pair (62 s) | ctx21/22, group 7, start_kick 6487, 15:52:24-15:53:27, not dumped | **ctx74/75, group 6, start_kick 5802, dumped as `ctx74-g20-...-24.wav` / `ctx75-g20-...-25.wav`** (62.07 s each, 5346 frames) |
| cut-scene voice siblings (one kick burst of 7, ~40 s) | group 60: ctx 4,13,14,22,33,37,38, start_kick 13441, 15:53:31-15:54:13 | group 44: ctx 53,54,59,60,61,62,63, start_kick 12663, 15:57:09-15:57:51 |

- **Sibling mapping between the arms.** The `frame-trim` end record (end=119) has a byte-identical read offset in
  each arm, and the first-decode packet counts (30/32/3/19/19/10/11) agree:

  | trace ctx | fix ctx | read offset |
  |---|---|---|
  | 4 | 53 | 179385 |
  | 13 | 54 | 163263 |
  | 14 | 59 | 13869 |
  | 22 | 60 | 166143 |
  | 33 | 61 | 149193 |
  | 37 | 62 | 84633 |
  | 38 | 63 | 80623 |

## 2. Music pair
- **Fix, ctx74 against ctx75 (WAV dump).** The WAVs were high-passed at 1.5 kHz and cross-correlated in 2 s
  windows with a 1 s hop, searching ±2048 samples.
  - **Lag 0 in all 60 windows** (0-61 s). r is 0.51-0.97, with a median of 0.92; the lowest values (0.51-0.73)
    fall at 14-21 s, a quieter, less correlated passage.
  - The host's walk.csv lists the claimed split-header frames:
    - ctx74 at frames 2587, 3001, 3446, 4605;
    - ctx75 at frames 3701, 4508, 5100, 5114.
  - 0.5 s windows within ±3 s of each of these 8 frames all give lag 0 (r_min 0.67-0.94).
  - Without the fix (upstream walk), the offset of ctx74 against ctx75 would have gone
    -512 / -1024 / -1536 / -1024 / -512 / -1024 / -512 / 0 samples at 30.0 / 34.8 / 40.0 / 43.0 / 52.3 / 53.5 /
    59.2 / 59.4 s.
  - Both contexts decoded 5346 frames, with walk verdicts all `ok`/`first` and no gap.
- **Trace, intro pair ctx0/ctx1 (from the log, not dumped).**
  - ctx1 has W1 gaps at host frames 1004, 2132, 3260, 4388 and 5516: one per pass, 1128 host frames apart, so
    1129 true frames per pass. Each gap is a straddling header with 10 bits left in packet 16.
  - ctx0 has no gap. So ctx0 is late by +512 from frame ~1004 and gains 512 more on every pass: +2560 after the
    5th pass.
  - This is exactly the l75 signature (step at frame 1004-1005, +1024 on the second pass).
- **Trace, music pair ctx21/22.** ctx21 has gaps at 571, 1533, 1647, 2548 and 4965. ctx22 has gaps at 578 and 1786.
  Both decoded 5395 frames, but their content ends 3 frames apart.
- **Fix: no `[xma] walk:` anomaly for any context in the whole session** (walk_gap=0).

## 3. Voice siblings
- **Trace frame counts.** Decoded = walk-summary at Clear; gaps = `[xma] walk:` kind=gap; true = decoded + gaps.
  End-trim frame = `frame-trim` end=119 record.

  | trace ctx | gaps at host frame | gaps | decoded | true | end-trim frame |
  |---|---|---|---|---|---|
  | 4 | 54, 400 | 2 | 3628 | 3630 | 3627 |
  | 13 | 1528, 1873, 1958, 2286 | 4 | 3626 | 3630 | 3625 |
  | 14 | 85, 169, 1318, 1343, 1905 | 5 | 3625 | 3630 | 3624 |
  | 22 | 1483, 3221 | 2 | 3628 | 3630 | 3627 |
  | 33 | 2246, 2389, 2728, 2811, 3002 | 5 | 3625 | 3630 | 3624 |
  | 37 | 111, 1838, 2677, 3426 | 4 | 3626 | 3630 | 3625 |
  | 38 | 2398, 2619 | 2 | 3628 | 3630 | 3627 |

  - Every gap is `path=in-packet` after a trailer-1 frame with 1-14 bits left (`left_bits`) in the previous packet.
    All records carry `hdr_count=0 xma2=0`.
  - The end-trim frames 3624/3625/3627 match the l75 values byte for byte. The true length is 3630, not 3627, so
    even the "3627" siblings had lost 2 frames.
  - ctx14's first two gaps (frames 85 and 169) are 192-bit (silent) frames in a 3-packet lead-in buffer.
- **Fix.** The group line has `frames_emitted=[3630 x 7]`, `frame_delta=0` and `trim_end=[1:119 x 7]`. All 7
  `frame-trim` end records are on frame 3629, and the session logged no walk anomaly.
- **Trace offsets relative to the first sibling (ctx4).** This timeline is derived from the gap records (a lost
  frame makes that context 512 samples early); + means the sibling is late. Selected rows:

  | host frame | t (s) | 13 | 14 | 22 | 33 | 37 | 38 |
  |---|---|---|---|---|---|---|---|
  | 0 | 0.0 | 0 | 0 | 0 | 0 | 0 | 0 |
  | 54 | 0.6 | +512 | +512 | +512 | +512 | +512 | +512 |
  | 400 | 4.6 | +1024 | 0 | +1024 | +1024 | +512 | +1024 |
  | 1528 | 17.7 | +512 | -1024 | +512 | +1024 | +512 | +1024 |
  | 1958 | 22.7 | -512 | -1536 | +512 | +1024 | 0 | +1024 |
  | 2619 | 30.4 | -1024 | -1536 | +512 | 0 | 0 | 0 |
  | 3426 (to end) | 39.8 | -1024 | -1536 | 0 | -1536 | -1024 | 0 |

  - Each sibling's offset changes by exactly 0, ±512, ±1024 or ±1536 at its own gap frames.
  - The pairwise spread peaks at 5 frames (2560 samples, 22-28 s) and ends at 3 frames.
  - In the fix arm all 7 stay at 0 (equal decode, no gaps). This is from the logs only: the scene was not in the WAV
    dump.

## 4. The fix arm's `walk_split_lost=1`
- **The counter appeared in the minute ending 15:56:40 and stayed at 1.** Final values: walk_split_tail=151,
  walk_split_claimed=150, walk_split_real=150, walk_split_lost=1, walk_split_padding=0, walk_gap=0, walk_dup=0.
  - So 151 tails = 150 confirmed and claimed + 1 tail that `CheckSplitTail` did not confirm (the next packet's
    first-frame offset does not continue a frame there). The fix leaves such a tail to the upstream walk, which is
    correct for padding.
  - The continuity classifier saw no gap (walk_gap=0), so no frame was skipped.
- **The trace arm has the same class.** It had 170 tails, of which 168 were real, and 168 gaps: the 2 non-real tails
  produced no gap.
- **So the 1 is a padding tail, not a real loss.** `walk_split_lost` counts unclaimed tails, real or not; the real
  losses are `walk_split_real - walk_split_claimed` (fix: 0).
- **Which context.** It cannot be named from the log, because the fix arm prints `[xma] walk:` lines only for
  anomalies and there were none.
  - None of the 128 fix walk.csv files has an unclaimed tail row, and the offline walk found no skipped frame in any
    fix dump. So it did not affect a dumped context.
  - The 1-bit tails at the end of a buffer in fix ctx7-g35 and ctx56-g65 (and trace ctx44-g54) are in the last
    packet of a 23-packet one-shot buffer. The host never reached that packet (the sound was cleared after 0.43 s),
    so they are not this event.
- **Side note.** The SDK walk-fix doc proposes `walk_split_lost` as the loss metric. It should be read as
  "unconfirmed tails", or the cleanup should report `real - claimed`.

## 5. Independent offline frame walk (input.bin / input.csv)
- **Method.** For each dumped buffer, the walk parses the 32-bit packet headers:
  - first-frame offset = 15 bits (+32);
  - metadata = 3 bits;
  - skip = 8 bits;
  - the top 6 bits hold `4*seq+2` (2, 6, 10, 14, ...): an XMA1-style sequence counter, not a frame count, with
    metadata = 0.

  It then follows the 15-bit frame lengths over the concatenated packet payloads:
  - trailer 1: the next frame starts immediately, even when its header straddles into the next packet;
  - trailer 0: the next frame starts at the first later packet whose first-frame offset is at or after the frame end;
  - length 0 or 0x7FFF: padding, continue at the next packet's first-frame offset.

  Validation:
  - In every walked packet, the first frame start found by the walk equals the header's first-frame offset. There
    were no mismatches across all 256 dumps.
  - Each consecutive pair of host-decoded frames (walk.csv: buffer epoch, packet, start bit) is classified against
    the true successor.
- **The voice siblings were not dumped**, so the walk ran on every dumped trace context instead. Two of them carry
  W1 events:
  - **ctx52-g24 (a looping 15-packet ambience buffer).**
    - 195 true frames per pass; the host decoded 194 per pass.
    - The one straddling header is at packet 13, bit 16378: 6 header bits in packet 13 and 9 in packet 14, L=1254.
    - The host skipped exactly that frame on each of the 6 passes that reached it, at host frames 188, 382, 576,
      770, 964 and 1158 (= the walk.csv `gap` rows). In total, 1291 true frames over the host's span against 1285
      decoded.
  - **ctx36-g32.**
    - One straddling header at packet 2, bit 16371: 13 bits left, L=1370.
    - The host skipped it once (walk.csv gap at frame 30).
- **Trace totals, 128 dumps.**
  - 23505 transitions follow the true successor.
  - 150 are loop/seek jumps back.
  - **7 skips, all of a straddling-header frame. 0 duplicates and 0 other skips.**
  - Straddling headers found in dumped buffers: 3. Two were reached and skipped every time (7 skips). The third
    (ctx44-g54, 1 bit at the end of a buffer) was never reached.
- **Fix totals, 128 dumps.**
  - 32534 transitions follow the true successor.
  - 150 are loop jumps back.
  - **0 skips, 0 duplicates.**
  - 18 straddling headers in dumped buffers. Every one the host reached was decoded in order, including the 4 in
    ctx74 and the 4 in ctx75.
  - The only host frames not matched to a walked frame are buffer-boundary frames: walk.csv labels a frame that
    spans into the next buffer with the next epoch (13 each in ctx74/75), plus the final frame of short one-shots.
- **This confirms the W1 mechanism.** The game's packets never count frames in the header (`hdr_count=0 xma2=0` in
  all 284 trace walk lines; `walk_split_counted=0` in both arms). The upstream walk therefore always treated the
  frame before a straddling header as the packet's last frame and jumped past the straddling frame. The fix decodes
  it.

## Follow-ups (for the primary agent)
- The data support flipping `xma_frame_walk_fix` to default on.
- Treat `walk_split_lost` as "unconfirmed tails". Real loss = real - claimed, which was 0 in the fix arm.
- Optional: a WAV-level check of the 7 voice siblings needs a dump aimed at the scene (cap or start-delay change).
  The log-level evidence above is already conclusive for equal frame counts.
