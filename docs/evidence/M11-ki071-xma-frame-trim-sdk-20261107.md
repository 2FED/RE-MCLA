# KI-071 step 3: XMA per-frame trim (parse, counters, honour), per-context PCM dump, burst budgets (SDK)

Implementation by an Opus subagent on 2026-11-07 as SDK commit `c526cb3` on local branch `mcla/m11-xma-trim` (parent `8f949f7`); accepted by the primary agent as code: ctest 2090/2090 (4 skipped), no new warnings. Installed as `private/l75-sdk-install` 0.20.0.5-dev.gc526cb3: rexruntime.dll c8df5cca5c7928dc57c2058e7eb9af48c393f6d002fbd7f51535e63b283b73c4 (11,410,944 B), rexgpu-xenos.dll 0fcb9a808acf3be87632c30b84396e2a258a68e2242e5db81d637dd68f0bc292, TracyClient.dll 2387d4b6161ad02666666dda861478bdb6d4ab5d7e31504b650584b86207b4c4. Format change: the `[xma] group:` `id=` is now a stable serial per scene (not the lowest ctx id); `MCLA_XMA_SIB` is v=2.

**Follow-up 2026-11-07 (SDK `bd9314f` on `mcla/m11-steady2`):** the owner's trim-trace run exhausted the 64-file cap with menu SFX before the cut-scene, so no dialogue was dumped. `xma_dump_burst_only` (default true) now dumps only contexts whose first decode falls in a burst of 3+ same-rate starts within 50 ms (plus siblings joining by start-kick id when the trace is on), holding back the first siblings' blocks until the 4th starts so every file begins at its first sample; caps 128 files / 90 s; file names `ctx<id>-g<burst serial>-<rate>Hz-...`. ctest 2100/2100. Installed over `private/l75-sdk-install` (0.20.0.7-dev.gbd9314f: rexruntime.dll 30a31cc44203f3ee9391fb5547c55d72ee3a8a9b3bcddfb7045625396ff60377, rexgpu-xenos.dll 01aecc557bafd3d7d0e4f5f26cb22f59499bc7cde54d76b6aef56d32b153ec6d). NOTE: the staged l75 candidate still carries 4bb8c02; the dump fix needs a re-stage (l76).

---

# KI-071 step 3: XMA per-frame trim counters, `xma_honor_frame_trim`, per-context PCM dump, trace budgets (SDK notes, draft)

Draft by an Opus implementation subagent, 2026-11-07. SDK worktree `private/l75-sdk-xma3-wt`, branch
`mcla/m11-xma-trim` from `mcla/m11-xma-sibling` 8f949f7. Commit: c526cb3 (version string
0.20.0.5-dev.gc526cb3). Nothing was launched; no physical result is claimed. All new cvars default to off/empty.
With them off the build decodes and writes the ring exactly like 8f949f7. The additions that run anyway
are a read-only re-parse of each frame header (a few dozen bit reads per frame), relaxed-atomic counters,
the reworked sibling grouping (trace only) and the reworked first-decode budget (trace only).

Inputs: `docs/evidence/M11-ki071-l74-sibling-analysis-20261107.md` (7 siblings in lock-step, candidate =
ignored per-frame trim), `docs/evidence/M11-ki071-xma-sibling-trace-sdk-20261107.md` (8f949f7).

## 1. The trim fields (what FFmpeg does)
Source: `thirdparty/FFmpeg/libavcodec/wmaprodec.c` in the SDK tree.
- `ff_xmaframes_decoder` (line 2146) uses `xmaframes_decode_init` -> `decode_init(s, avctx, 0)`.
  - Line 328: `block_align = 2048` for XMA1/XMA2/XMAFRAMES, so `log2_frame_size = av_log2(2048) + 4 = 15`
    (line 386): the 15-bit XMA frame length.
  - XMAFRAMES branch (line 375): `decode_flags = 0x10d6`, `nb_channels = avctx->channels`.
  - From 0x10d6: `len_prefix` (0x40) on; `log2_max_num_subframes = (0xd6 & 0x38) >> 3 = 2`, so
    `max_num_subframes = 4`, `max_subframe_len_bit = 1`, `subframe_len_bits = 2`,
    `min_samples_per_subframe = 128`; DRC (0x80) on; `samples_per_frame = 512` (line 410) and
    `skip_frame = 0` for every non-WMAPRO codec (lines 393-396).
- `xmaframes_decode_packet` (lines 2045-2085): byte 0 of the packet holds padding_start (3 bits) and
  padding_end (3 bits); the frame starts after `padding_start` bits; `decode_frame` is called once and its
  `*got_frame_ptr` result is used as is.
- `decode_frame` (lines 1430-1519), in order:
  1. `len = get_bits(gb, 15)` (len_prefix).
  2. `decode_tilehdr` (lines 654-738): one fixed-layout bit (max_num_subframes != 1), then per subframe
     either per-channel "contains" bits (stereo, variable layout) and `decode_subframe_length`
     (lines 603-633: no bits at offset 384; else 1 bit "not max length" and, if set, 1 bit shift).
  3. Postproc transform (stereo only): 1 bit; if set, 1 bit; if set, 4 x nb_channels^2 bits.
  4. DRC gain: 8 bits.
  5. Lines 1462-1479: 1 bit "trim present"; then 1 bit + `av_log2(samples_per_frame * 2)` = 10 bits start
     skip ("usually true for the first frame"), and 1 bit + 10 bits end skip ("sometimes true for the last
     frame"). Both go into `int av_unused skip` and are discarded.
  6. Lines 1497-1519: every channel's 512 samples are copied to the output frame and `*got_frame_ptr = 1`.
- So the host always emits 512 samples per frame per channel regardless of the trim fields.

## 2. What hardware is believed to do (unverified)
- The fields have the WMA Pro meaning of encoder-delay / end-padding trims: samples to drop at the start of
  the stream and at its end. FFmpeg's WMAPRO path compensates the encoder delay only by dropping the whole
  first frame (`skip_frame = 1`), and never for XMA.
- The Xbox 360 XMA hardware decoder is believed to honour them (a 10-bit field allows 0-1023, i.e. up to
  two 512-sample frames). This is not documented in any source available here, and was not verified in
  this session; it is the hypothesis the counters test.
- Unverified details that the experiment has to settle:
  - whether a start skip counts from the frame that carries it (as implemented here) or from the decoder
    output after its one-frame overlap delay;
  - whether a start skip above 512 spans the next frame (implemented: it does);
  - whether an end skip applies to the frame that carries it (implemented) or to the stream end.

## 3. Counters (always on) and lines (with `xma_sibling_trace=true`)
- Parse: `xma::ParseFrameTrim` (new `include/rex/audio/xma/frame_trim.h`) replicates steps 1-5 read-only on
  `xma_frame_` (the exact bits handed to FFmpeg, from bit `padding_start` of byte 1, bounded by the frame
  size). It is called in `XmaContext::Decode` right after `PreparePacket`, before `DecodePacket`.
  Parse failures (a broken tile header, truncated frame) count as `trim_parse_errors`; FFmpeg would fail
  the same frame.
- Per context (since Clear/Release): frames with a non-zero start trim and their sum, the same for end, and
  the first 4 non-zero (start,end) pairs.
- `[xma] group:` line, appended at the end (existing fields unchanged):
  `start_kick=[..] trim_start=[frames:samples,..] trim_end=[frames:samples,..] trim_first=[s:e;s:e/..]`
  (`-` for a context with no non-zero trim; `/` separates contexts).
- `MCLA_XMA_SIB` minute line is now `v=2`, with new fields at the end:
  `trim_frames= trim_samples= trim_ctxs= trim_applied_samples= trim_unapplied= trim_parse_errors=
  burst_first_decodes= trim_fix= trim_block=`.
  - `trim_frames`: frames with a non-zero start or end trim; `trim_samples`: sum of start + end;
    `trim_ctxs`: distinct contexts with a non-zero trim in the minute.
  - `trim_applied_samples`: samples per channel dropped by the fix; `trim_unapplied`: end trim beyond the
    frame plus (block mode) unaligned remainders.
- `[xma] frame-trim: ctx= rate= stereo= frame= start= end= start_flag= end_flag= header_bits= read_offset=
  kick_group= trim_fix=` for each of the first 4 non-zero frames per context, on its own budget
  (burst 64, 16 lines/s).
- The first-decode record gains `trim=start:end trim_ok=` (the first frame's fields) and `budget=`.

How to read it (cut-scene, trace-only arm): if the hypothesis holds, the 7 voice siblings show non-zero
`trim_start` and their sums differ by 512 or 1024 between siblings (for example 0 versus 512). All zero
trims on every sibling means the offset is not decoder-side; then the dump (section 5) decides between an
authored content offset and a guest-mix copy.

## 4. Fix `xma_honor_frame_trim` (default false) and the granularity decision
- Applied in `Decode` after `ConvertFrame`, before the loop-start / D1 start-skip adjustments, by
  `xma::ApplyFrameTrim` on `raw_frame_` (big-endian int16, interleaved for stereo).
- Ring granularity: the guest-visible write offset is in 256-byte blocks (128 mono samples, 64 stereo
  pairs); `Consume` writes whole blocks, `output_buffer_write_offset` is a block index. A partial block
  cannot be written without corrupting the guest's accounting.
- **Decision: sample-exact shift (default).** The kept samples are appended to a carry of less than one
  block left by the previous frame; whole blocks are emitted, the remainder is carried to the next frame.
  - Every later sample of the stream moves by exactly the trim, so a 512 trim removes exactly one frame
    and a 100-sample trim is not rounded.
  - A frame now yields 0..4 (mono) or 0..8 (stereo) blocks instead of always 4/8. The blocks are placed
    at the end of `raw_frame_`, so `Consume` (which reads block `total - remaining`) needs no change.
  - At most one block minus one sample pair stays in the carry at the end of a stream; it is dropped on
    Clear/Release (end of stream; less than 2.9 ms at 44.1 kHz).
- **Alternative `xma_frame_trim_block_granular=true`:** head and tail trims rounded down to whole
  blocks, remainder counted in `trim_unapplied`, no carry. Exact for trims that are multiples of 128
  (mono) / 64 (stereo).
- Start trim above the samples left in the frame continues into the next frames (`pending_start`); an end
  trim above what is left is counted as unapplied.
- Risks:
  - If hardware does not trim (or trims relative to the decoder delay), honouring the fields shifts the
    trimmed siblings and could create or worsen a misalignment. The trace-only arm decides first.
  - Fewer blocks per frame means more frames decoded per Work for the same ring space; the Work loop
    already handles a frame that yields 0 blocks (it decodes the next one).
  - Loop end frames: `loop_frame_output_limit_` counts consumed subframes as `total - remaining`; on a
    trimmed loop-end frame the limit is reached earlier by the trimmed blocks. No such combination is
    known in this title; not handled specially.
  - Toggling the cvar mid-stream drops a non-empty carry (less than one block).

## 5. Per-context PCM dump
- `xma_dump_contexts` (string, default empty) = directory; created if missing.
  `xma_dump_all_rates` (bool, default false) lifts the 44.1 kHz filter.
- File per context stream (from the first block written after a Clear/Release):
  `<dir>/ctx<id>-<rate>Hz-<mono|stereo>-<n>.wav`, n = session-wide file number (0..63).
- Content: exactly the blocks `Consume` wrote to the guest output ring (post-trim, post D1/loop skips),
  byte-swapped to little-endian 16-bit PCM; 44-byte canonical WAV header with the context's rate and
  channel count.
- Caps: 60 s per stream (the file is closed at the cap, reason=cap), 64 files per session (one INFO when
  reached). The header is rewritten about once per second of audio, on close (Clear/Release: reason=clear)
  and in `XmaDecoder::Shutdown` (reason=exit), so a killed process still leaves a valid file with at most
  one second missing from the header count.
- INFO lines: `[xma] dump: open ctx= rate= stereo= n= start_kick= file=` and
  `[xma] dump: close ctx= reason= bytes= seconds= file=`. Bounded by the 64-file cap (at most 129 lines).
- Offline use: cross-correlate the 7 voice files of one start burst (same `start_kick`); peaks at 512 or
  1024 samples confirm a content offset in the decoded data; lag 0 everywhere points at the guest mix.
- Risk: file I/O on the audio thread (buffered `std::ofstream`, 256-byte writes). Dump arm only.

## 6. Trace budget and grouping fixes
- First-decode records: a 44.1 kHz context whose first decode comes within 50 ms of first decodes of 3 or
  more other 44.1 kHz contexts is logged on its own budget (`budget=burst`, burst 128, 32 lines/s); the
  earlier records of the same window that the shared budget suppressed are flushed with it
  (`budget=burst-late`). Other records keep the shared budget (`budget=shared`). Counted as
  `burst_first_decodes`.
- Sibling grouping (`xma::StickySiblingGrouper`, replaces the plain contiguous-slice grouping in the group
  scan):
  1. Contiguous buffers link only contexts with the same start burst (`start_kick` = kick burst id at the
     first decode since Clear), the same rate and channel count.
  2. Linked components that share (start_kick, rate, channels) merge, so the l74 pair + five become one
     7-context group.
  3. Membership is sticky while a context keeps its start_kick (no Clear), so the group survives buffer
     swaps; the group `id=` is now a serial number given when the group forms (lower id wins on merge),
     stable over the scene. It is no longer the lowest context id; the `ctxs=[..]` list is unchanged.
- The host consume-probe cap is a host change and is not part of this commit.

## Files
- New `include/rex/audio/xma/frame_trim.h`: bit reader, `ParseFrameTrim`, `ApplyFrameTrim`, WAV header,
  `SwapPcm16`, `DumpFileName`, `StartBurstWindow`, cvar declarations.
- `include/rex/audio/xma/sibling_trace.h`: `GroupContiguousSlices` optional link predicate, trim fields in
  `SiblingCtxSample`, `FormatTrimFirst`, line formats, `StickySiblingGrouper`.
- `include/rex/audio/xma/context.h`, `src/audio/xma_context.cpp`: cvars, parse/apply, counters,
  `XmaPcmDump`, burst budget, `FinalizeDump`.
- `include/rex/audio/xma/decoder.h`, `src/audio/xma_decoder.cpp`: sticky grouper, minute flags,
  dump finalisation in Shutdown.
- New `tests/unit/audio/xma_frame_trim_test.cpp` (8 cases, `[ki071]`); updated
  `tests/unit/audio/xma_sibling_trace_test.cpp` (line formats).

## Suggested arms (all with `xma_sibling_trace=true`, same cut-scene)
- **trim-trace:** everything else off, plus `xma_dump_contexts=<dir>` (decisive and needs no listening).
- **trim-fix:** `xma_honor_frame_trim=true` (+ the dump to compare).
- **trim-block:** `xma_honor_frame_trim=true`, `xma_frame_trim_block_granular=true`.

## Tests
New `tests/unit/audio/xma_frame_trim_test.cpp` (8 cases, `[ki071]`):
- parser, mono: no trim; flag with a zero start; 10-bit maximum start (1023) at a non-byte-aligned frame
  start; both flags (512/300); end only (1023); header bit counts
- parser, stereo: a variable tile layout hand-traced through decode_tilehdr (ch0 256+256, ch1 512) with a
  postproc matrix; a fixed layout without postproc
- parser errors: 128 + 512 subframes (channel length > 512); frame truncated by one bit inside the trim
  fields; 3 channels
- sample-exact application: identity; start 100 (3 blocks + 56-byte carry, carried samples lead the next
  frame); end beyond the frame (unapplied 188); start 700 spanning two frames; stereo end 32 pairs
- block-granular application: stereo start 100 (1 block dropped, 36 unaligned); mono start 256 + end 128
- WAV header fields (mono 44.1 kHz, stereo 48 kHz), big- to little-endian swap, file names
- burst window: no burst below 3 others, burst on the 4th with flush of suppressed records, same context
  not counted twice, 50 ms expiry
- sticky groups: l74 pair + five merge into one 7-context group, a contiguous neighbour from another start
  stays out, a radio pair is its own group; ids survive non-contiguous buffers; a restarted context
  leaves; a vanished group disappears
Updated `xma_sibling_trace_test.cpp` line-format case for the appended group fields and the v=2 minute line.

Results:
- ctest (l75-sdk-xma3-b, Release, -j 8): 2094 tests; 2090 passed, 4 skipped (the BitStream Write cases, as
  at baseline), 0 failed. Baseline 8f949f7: 2082 passed + 4 skipped; +8 new.
- No new compiler warnings: the warnings in the XMA files are the pre-existing ones in decoder.h
  (ppc_context) and xma_decoder.cpp (avcl, sign-compare); none in the new or changed code.
- git-clang-format against 8f949f7 is clean; all changed sources are ASCII.

## Build and install
- Build: `private/l75-sdk-xma3-b` (output `private/l75-sdk-xma3-o`), same configuration as l74-sdk-xma2-b
  (VS 2022 Build Tools, LLVM clang, Ninja Multi-Config, Release, -march=x86-64-v3, D3D12, Tracy, perf
  counters). Scripts: `%TEMP%\claude\C--BDU-MCLA-Recomp\l75-xma3-scripts\{build,test,install}.ps1`
  (adapted from l74-xma2-scripts). Third-party folders copied from `l74-sdk-xma2-wt`; git there needs
  `--ignore-submodules=all`.
- Installed into `private/l75-sdk-install` (new) after a reconfigure. version.h reads
  `0.20.0.5-dev.gc526cb3`. SHA-256:
  - bin/rexruntime.dll   c8df5cca5c7928dc57c2058e7eb9af48c393f6d002fbd7f51535e63b283b73c4 (11410944 B)
  - bin/rexgpu-xenos.dll 0fcb9a808acf3be87632c30b84396e2a258a68e2242e5db81d637dd68f0bc292 (3253248 B)
  - bin/TracyClient.dll  2387d4b6161ad02666666dda861478bdb6d4ab5d7e31504b650584b86207b4c4 (246784 B)
