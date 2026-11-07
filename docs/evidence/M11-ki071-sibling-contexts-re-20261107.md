# KI-071 C5 "contexts out of lock-step" (read-only RE, 2026-11-07)

Read-only RE by an Opus subagent on 2026-11-07 (l73 swap traces, the game's XMA playback library, SDK 4a3aea1); accepted by the primary agent.

Scope: SDK `private/l73-sdk-xma-wt` at 4a3aea1 (src/audio/xma_decoder.cpp, xma_context.cpp,
include/rex/audio/xma/context.h, thirdparty/FFmpeg/libavcodec/wmaprodec.c), guest
`generated/default/mcla_recomp.25/26.cpp`, l73 session logs (base d148c414, voice-c1 1054e9d0,
voice-fix dc6b9c88; profile 7d6ff1dd). Nothing was built or run. No physical result is claimed.

## Verdict
- **C5 is refuted as stated.** The host does not let sibling contexts drift in content.
  - Each kicked context is decoded inline until its ring is full.
  - The guest consumes every stream FIFO, at the same SRC rate.
  - A fill-level difference between siblings cannot turn into a permanent time offset.
  - A fill shortfall becomes a guest zero-pad. The resulting lag is in output samples, of any
    length, and it would be audible as a gap. It is not exactly 512 source samples.
- **Structural part of C5 confirmed.** The cut-scene soundtrack really is several sibling
  contexts.
  - It plays through 5 mono 44.1 kHz contexts, one per channel.
  - Each context's sub-buffer is carved from one streaming read.
  - Radio and music are 2 mono contexts (L and R) carved the same way.
  - Engine and SFX are single mono contexts, often 48 kHz. They have no sibling, so they cannot
    double. This explains "44.1 kHz only".
- **What the measurement requires:** a frame-exact discontinuity or start offset in one sibling's
  sample sequence.
  - The lag is 557.3-557.7 samples at 48 kHz, which is 512.0 +/- 0.3 source samples.
  - That is fractional at 48 kHz, so it is not a mixer delay line. This also excludes C6.
  - The current counters do not see this event.
  - The decisive experiment is a per-context content-position ledger (section 5).

## 1. Contexts active during the cut-scene (l73 swap trace)
- 1385 `[xma] swap:` lines in the two traced sessions. Every line has `stereo=0` and `skip=0`,
  `last_packet = packets-1` and `carry=0`. Every buffer is single-stream (no interleaved streams).
- Cut-scene group, voice-c1: ctx 67, 70, 38, 72, 69.
  - All five start within 50 ms (09:54:41.640-41.690).
  - They run about 40 s and end together.
  - Per-buffer packet sequences: 67 19,18,16,13...; 69 19,18,15,12...; 70 10,10,11...;
    72 11,10,11...; 38 3,2,5,11,8...
  - Read as channels: an L/R-like pair, a second pair, and a centre-like channel whose first
    buffers are silence-sized and then about 8-12 packets of speech.
- Voice-fix has the same group as ctx 47, 48, 50, 51, 52, with identical packet sequences.
- The buffers are contiguous slices of one read:
  - 03F5F800 (67, 19 pk) + 0x9800 = 03F69000 (70, 10 pk)
  - 03F69000 + 0x5000 = 03F6E000 (38, 3 pk)
  - 03F6E000 + 0x1800 = 03F6F800 (72, 11 pk)
  - 03F6F800 + 0x5800 = 03F75000 (69, 19 pk)
  - The next read starts at 03F80000.
  - In the second block one packet (03F8E000) is unaccounted for. It may be a sixth (LFE-like)
    context that never swapped while traced; this is unverified.
- `shared_buffer_ctxs` is always "-" inside the group. The non-empty values elsewhere belong to
  one-shot SFX that reuse the same asset or a stale pointer.
- Radio and music pairs are built the same way:
  - ctx 0 at 03F18000 (31 pk) + 0xF800 = ctx 1 at 03F27800.
  - Their swaps fall on the same millisecond.
  - Other pairs: 17/18 and 17/20, 30-32-packet buffers.
- Swap times are quantised to about 50 ms. Decoding happens only in kick bursts (see 3), so the
  trace cannot resolve offsets of 11.6 ms. Swap time also depends on each channel's bitrate.

## 2. Guest: how the voice pump consumes several streams (question 1)
- Pump `sub_823FC9F8` (mcla_recomp.26.cpp 5297-6936):
  - It takes the lock once (`sub_823EDDA0`, :5376) and processes ONE output block per call.
  - It kicks only if some stream is "starving" (r22). That is the `sub_823EE050` call at :6826,
    after the check at loc_823FD3E4, :6805.
- Per stream i (loop :5622-5838), each stream is handled independently:
  - available = `sub_823ED670(xma,i)`
  - contiguous chunk = `sub_823ED8B8`
  - resampler `sub_823FF5F8` (ratio = src rate / out rate, stored at +44; per-stream state is
    88 bytes at voice+200)
  - consume = `sub_823ED700(xma,i,n)`
  - The loop repeats while that stream's 256-sample SRC output is not full.
- There is no min() across streams. Nothing reads stream i at an offset computed from stream 0.
  The streams stay aligned because they consume FIFO at the same ratio.
- Short stream:
  - The remainder of the output block is zero-filled (`sub_823D9890` loop, loc_823FCD34 :5785;
    r26 = 1).
  - The stream then lags permanently by the padded number of output samples.
  - This is the only place where a fill difference turns into a time offset. Its signature is
    an audible gap plus a lag in 48 kHz samples of any length, not 557.28.
- Kick (`sub_823EE050`, mcla_recomp.25.cpp 41668-41745):
  - It writes the 48-byte shadow back, then writes one kick register per stream in stream order.
  - Starving test: remaining = avail - consumed <= halfword[voice + 2*(i+138)].
- Buffer submit `sub_823FB960` (26.cpp 2780+):
  - All streams of one voice get the same buffer address (`sub_823ED450`, :3649).
  - The cut-scene contexts have separate sub-buffers, so they are 5 separate one-stream voices,
    or a caller outside this path. Either way they are 5 FIFOs that the game keeps aligned only
    by starting them together.
  - Mode byte +292 == 2 gives a seek start: `sub_823FA988` (:3672) computes a per-stream bit
    offset from the buffer's start sample (+28).
  - `sub_823EDC80` then writes that offset plus a start subframe skip ((+28 >> 7) & 3) into
    dword1 bits 24-26. On the host that field is `output_buffer_padding` (context.h:75), used
    only as ring headroom (xma_context.cpp:255, :610). The host never uses it as a skip. See D1.

## 3. Host: per-Work budget, order, fill difference (question 2)
- Inline Work at kick (xma_decoder.cpp 298-307): the guest writes one kick register per stream.
  Each write enables that context and runs `Work()` inline. Order = stream order, sequential,
  inside the guest's `sub_823EE050`.
- Worker loop (xma_decoder.cpp:142-163): round-robin over all 320 contexts. Disabled (locked)
  contexts return false.
  - The guest keeps a voice locked from pump start until its next kick, so between kicks
    nothing decodes. This explains the 50 ms bursts.
  - A context can get a worker pass right after its inline pass. The second pass only tops up
    the ring; it never replays.
- Budget per Work (xma_context.cpp:257-268): loop Decode + Consume while free blocks >=
  max(1, sdc) + padding, that is, until the ring is full or the input runs out. There is no
  fixed frame count.
- Maximum fill difference between two siblings after the same kick: fewer than (sdc + padding)
  blocks, which is at most 3-6 blocks x 128 samples (mono).
  - This difference is harmless. Content is FIFO, and each pump needs about 2 blocks.
  - It cannot create an offset unless a stream underflows (section 2).
- Hardware model: the APU decodes about `subframe_decode_count` subframes per enabled context,
  round-robin, asynchronously.
  - This is the Xenia understanding, not a hardware document.
  - It is also FIFO per context. "Lock-step" matters only for start position and for having no
    discontinuities, not for decode order.

## 4. Alternatives (questions 3 and 4)
- **C6 (game mixer delay, mode 1/3)** is excluded on numbers alone.
  - The tap is 557.28 samples at 48 kHz, which equals 512 x 48000/44100.
  - A mix-bus delay line works in integer 48 kHz samples, and it would not track the source frame.
  - The tap persisted in both mode 3 (l70/l71) and mode 1 (l72/l73). The shared element is the
    44.1 kHz source domain.
  - `sub_8213A600` was not traced further.
- **C7a (stereo ConvertFrame):** not involved. No stereo context exists in any trace
  (`stereo=0` on all 1385 lines).
- **C7b (packet-boundary repeat):** possible, but it must happen in only one sibling and at a
  frame-exact point. It is covered by D2 below. The traced normal swaps all had `new_valid=1`.
- Why only 44.1 kHz content doubles:
  - Dialogue and radio are multichannel 44.1 kHz streams split into sibling mono contexts.
  - Engine and SFX are single contexts, so there is no correlated sibling to comb against.
  - Packet counts 2/3 are one-shots. Counts 11-19 are the cut-scene channels. Counts 30-31 are
    music pairs.

### Frame-exact candidates left (one sibling must gain or lose whole frames)
- **D1, start skip ignored or seek warm-up mismatch:**
  - Mode-2 starts set a per-stream read offset plus a 0-3 subframe skip in dword1 bits 24-26.
  - The host treats that field as padding.
  - If the hardware also discards a warm-up frame after a guest-set offset (the host's loop path
    already models skip == 4 as a whole warm-up frame, xma_context.cpp ~849-855), then streams
    whose seek frame is chosen differently by `sub_823FA988` would start one frame apart.
  - Unverified: it is not known whether these voices use mode 2.
- **D2, silent frame drop at a buffer boundary** (not visible in `xma_swap_trace`):
  - A frame header split across the last packet while the next buffer is not yet valid takes
    `SwapInputBuffer(); return;` (xma_context.cpp:775-780). When decoding resumes at packet 0's
    first frame, the straddling frame is lost and that channel is now one frame ahead.
  - The same happens on the 0xFF path (:761-767).
  - Related: `error_status = 4` (:791, :814) is never cleared by the host. After it is set, the
    loop at :266 stops every Work after one frame, which leads to starvation and zero-pads.
- **D3, a 4-block (one mono frame) stale or skip through the valid/whole-ring rule:**
  - Excluded for C2 by counters (stale_ring_replays = 0).
  - Remains open for any ring that is exactly full but whose write offset field wraps.
- Side finding: the `xmaframes` codec has no `.flush` (wmaprodec.c:2146-2158), so
  `avcodec_flush_buffers` in `ResetDecoderState` (xma_context.cpp:332-334) leaves the IMDCT
  overlap buffer intact.
  - A reused context adds the previous wave's tail to the first frame of the next wave. That is
    a click, not a shift.
  - The codec is re-created only when the rate or channel count changes (`PrepareDecoder` :618).

## 5. Decisive experiment
Proposed cvar `xma_sibling_trace` (numbers only, paced).
- **Host, per context, from Clear or first valid input until Release:**
  - Fields: `frames_parsed` (read-offset advances to a new frame), `frames_emitted`
    (`DecodePacket` true), `subframes_written`, `subframes_discarded` (loop skip).
  - Drop counters by path: split_header_swap (:779), ffskip_swap (:765), err4 (:791/:814),
    decode_fail.
  - At the first Decode: `read_offset`, `first_frame_offset`, `dword1[24:26]`,
    `codec_reinit` (0/1), rate, buffer pointer.
- **Sibling grouping:** contexts kicked in one guest `sub_823EE050` sequence, or whose input
  buffer starts at another context's `ptr + packets*2048` within the same read.
- **Guest hook:** a `REX_EXTERN` wrapper on `sub_823ED700`. It accumulates samples consumed per
  hardware context id (stream record +80 halfword) and records the pad count per stream (+24 - +28
  after the pump loop, via a wrapper on `sub_823FC9F8` that reads the stream structs).
- **Per group, once per second during playback, one line:**
  `ctx: emitted_samples consumed_samples pads frames_parsed-frames_emitted drops{...}`.
- **Reading it:**
  - Siblings with equal consumption but `emitted` differing by exactly 512 (or 1024) samples
    confirm a 1-frame (2-frame) decode discontinuity. The drop-path counter names it (D2), or the
    first-Decode record names it (D1).
  - Equal emitted counts with non-zero pads on one sibling point to guest underflow. Lag in that
    case is output samples, which is not this bug.
  - All equal and no pads means the host is aligned. KI-071 is then game-side or content.
- **Cheapest first step:** only the first-Decode record plus the drop-path counters (no guest
  hook). One cut-scene run with the swap trace on.

## 6. Candidate fixes (by outcome; not lock-step decoding, which is not needed)
- **D1:**
  - Treat dword1 bits 24-26 as the start subframe skip for the first frame after a guest-set read
    offset (`sub_823EDC80`), and discard that many subframes.
  - Verify the hardware warm-up semantics before changing anything. Keep it behind a cvar.
- **D2:**
  - At a split header or spanning frame when the next buffer is invalid, stall: keep the read
    offset and current buffer, return, and resume when the guest validates the next buffer.
    This is the hardware behaviour.
  - Do not swap and drop.
  - Clear `error_status` once decoding succeeds again.
- **Independent hygiene:** reset the codec state on Clear or Release, for example by re-creating
  `av_context_` or zeroing the overlap. This removes first-frame cross-wave contamination.
