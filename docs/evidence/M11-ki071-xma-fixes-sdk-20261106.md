# KI-071: XMA doubling fixes in the SDK (swap carry, valid-only-when-written, lock recheck, swap trace)

Implementation by an Opus subagent on 2026-11-06 as SDK commit `4a3aea1` on local branch `mcla/m11-xma-doubling` (parent `e412c5c`); accepted by the primary agent as code: ctest 2074/2074 (4 skipped; one parallel-run manifest flake passed alone), no new warnings. All four cvars default false; owner A/B arms voice-c1 / voice-fix vs base on l73. Installed as `private/l73-sdk-install` 0.20.0.3-dev.g4a3aea1: rexruntime.dll ee1173e1d2be7853ed30ab25b4c16ed6e4da5b9728be36897420215faa3af676 (11,275,776 B), rexgpu-xenos.dll 9ede9314fd29897c1c3fc6dfe498f542fc07cffb970d33604424c76cd44f26d2 (3,253,248 B), TracyClient.dll 01cf28a803fcf6dd078ef68a2fea8f2eff53235f856c7a57e1a99935822328b6.

---

# KI-071 XMA doubled-voice fixes (C1 carry, C2 valid, C3 lock) - SDK implementation notes (draft)

Draft by an Opus implementation subagent, 2026-11-06. SDK worktree `private/l73-sdk-xma-wt`, branch
`mcla/m11-xma-doubling` from `mcla/m11-steady-key` e412c5c. Commit: 4a3aea1 (version string 0.20.0.3-dev.g4a3aea1). Nothing was launched;
no physical result is claimed. All four cvars default to false, so a build with no overrides is
behaviourally upstream (plus one INFO line per minute while XMA swaps happen, see "Counters").

## Measurement folded in (from the coordinator)
Our capture has delayed copies at 557.3-557.7 samples @ 48 kHz (11.61 ms) and 1114.5 (23.22 ms):
exactly 512 and 1024 samples of a 44.1 kHz stream (one and two XMA frames), constant on all cut-scene
lines, 10-15 dB below the direct sound, different per-channel gains, no bulk L/R skew; present in
cut-scene dialogue, the phone call and some driving stretches (probably radio music); absent in the
console reference. This matches C1: stream i of a multi-stream 44.1 kHz wave restarted at packet 0
after a swap decodes stream 0's packets, about i frames behind. C1 is therefore the primary fix; the
trace logs rate and the per-stream packet index, and the minute counter splits out 44.1 kHz contexts.

## Changes (citations are to src/audio/xma_context.cpp at d08813b/e412c5c unless noted)

### C1 `xma_swap_carry_packet` (primary)
- Upstream: at the end-of-buffer step (`:693-708`) `GetNextPacketReadOffset` returns the header
  sentinel when `next_packet_index >= packet_count` (`:302-315`); the code swaps and continues at
  packet 0's first frame (`:699-700`). `GetNextPacket` (`:281-300`) likewise returns packet 0 of the
  other buffer for a frame that spans the boundary (`:288-299`).
- Fix (cvar on):
  - New pure helper `xma::ComputeSwapCarry(next_packet_index, old_count, new_count)` in
    `include/rex/audio/xma/doubling_fixes.h`: carry = next - old_count when next > old_count, start at
    that packet of the new buffer; carry >= new_count (or new_count 0) falls back to packet 0 with a
    paced WARN `[xma] swap carry N out of range (packets=M) on ctx=K; packet 0` and a
    `carry_fallbacks` count. A next index inside the old buffer (swap forced because no later packet
    starts a frame) carries 0.
  - Decode end-of-buffer swap: new `XmaContext::OnInputBufferSwap` returns the start packet; the read
    offset becomes `start_packet * kBitsPerPacket + GetPacketFrameOffset(packet start_packet)`. A
    start packet with no frame start keeps upstream's handling (swap back, return). start_packet 0
    reproduces upstream byte for byte.
  - `GetNextPacket`: the spanning frame continues in packet `carry` of the next buffer.
  - Next buffer not yet valid at the swap (guest refills late): the carry is stored in
    `pending_swap_carry_packet_` and applied at the next Decode that starts at the buffer start
    (read offset == 32 bits), range-checked against the then-known packet count (fallback 0 + WARN).
    Cleared by ResetDecoderState (Clear/Release) and after one use.
- Not changed: the 0xFF full-skip path (`:582-590`) and the +1 step inside `GetNextPacketReadOffset`
  (`:304-312`, the "secondary" crossing in the RE note); their correct per-stream step is unknown.
- Hardware-behaviour inference and its uncertainty: the guest library (sub_823FB960) gives every
  stream of a voice the same input buffer and sets the read offset to packet i only on a fresh context
  (`(i << 14) + 32`); for double-buffered refills it sets nothing, so the hardware must carry stream i
  into the next buffer itself. The natural carry is the packet skip chain continued across the
  boundary (`next_packet_index - packet_count`). This is inferred from guest behaviour and now from the
  measured 512/1024-sample taps, not from a hardware document. Risks if wrong: a carry that points at
  the wrong packet decodes another stream (garble) or a packet without a frame start (silence or a
  dropped buffer via the upstream swap-back path); a too-large carry falls back to packet 0 (upstream
  behaviour) and is counted. The trace shows the actual carry values to check this.

### C2 `xma_valid_only_when_written`
- Upstream (`:136-149`): after decoding, `if (output_rb.empty()) output_buffer_valid = 0`.
  `RingBuffer::empty()` is read == write, which is both "drained" and "full". The guest
  (sub_823ED670) reads valid clear as "whole ring readable" and replays it.
- Fix (cvar on): `xma::ShouldClearOutputValid(read==write, blocks_written, true)`: clear only when this
  Work wrote at least one block (read == write after writing = full). `blocks_written_this_work_` is
  reset at Work start and summed in `Consume`.
- Counter `stale_ring_replays` (always counted): Work started with valid set, ended with read == write
  and wrote nothing, i.e. the case upstream clears wrongly.

### C3 `xma_work_lock_recheck`
- Upstream (`:96-102`): `Work` checks `is_enabled()` without the lock, then locks and runs. A guest
  lock (register 0x690 -> `Disable()` + `Block(false)`, `xma_decoder.cpp:310-325`) landing between the
  check and the lock lets Work run while the guest edits the context; the guest's unlock write-back
  then reverts our offsets.
- Fix (cvar on): re-read `is_enabled()` after taking the lock; if it is false, return without work
  (`xma::ShouldRunWorkAfterLock`). A `disable_count_` (incremented in `Disable()`) separates a real
  guest-lock race (counted as `lock_races`) from a concurrent second Work (worker plus inline kick),
  which is skipped but not counted.
- Also (cvar on): `StoreContextMerged` keeps the guest's current `output_buffer_read_offset` instead
  of writing back the snapshot (`:412`) via `xma::MergeOutputReadOffset`. Ownership: dword 9 bits 0-4
  (XMAGetOutputBufferReadOffset) are written only by the guest (consume helper sub_823ED700 writes
  dword 9; ClearLocked writes it via a direct Store, not the merge); Work only reads it
  (PrepareOutputRingBuffer `:319`). So storing the snapshot can only undo a guest consume.
  `read_offset_reverts` (always counted) is the number of merges where the guest value differed.

### Trace `xma_swap_trace`
- Paced INFO at each end-of-buffer swap (token bucket: burst 64, 8 lines/s; suppressed lines counted):
  `[xma] swap: ctx=N stereo=0|1 rate=44100 packets=<old count> last_packet=<index of last decoded
  packet> skip=<its skip> next_packet_index=N carry=N new_valid=0|1 new_packets=N start_packet=N
  fix=0|1 buffer=XXXXXXXX shared_buffer_ctxs=<ids of other contexts with this input buffer, max 8, or
  ->`. For stream i of a shared buffer, expect carry == i if the hypothesis holds.

### Counters (`MCLA_XMA v=1 kind=minute`)
`MCLA_XMA v=1 kind=minute swaps= carry_nonzero= stale_ring_replays= lock_races= swaps_44k=
carry_nonzero_44k= carry_fallbacks= read_offset_reverts= trace_suppressed= carry_fix= valid_fix=
lock_fix=`, emitted from Work at most once per 60 s when any counter is non-zero or the trace is on.
Counters accumulate with the fixes off too, so the base arm also shows how often each defect fires.
Note: swaps count on every XMA wave, so in practice the line appears once per minute whenever XMA
audio is playing, also in the default configuration.

## Tests
- New `tests/unit/audio/xma_doubling_fixes_test.cpp` (7 cases, tag `[ki071]`): carry for 3
  interleaved streams (carry == stream index), no carry inside the old buffer, clamping/fallback
  (last fitting packet, carry == count, empty new buffer, large skip), valid rule with fix off/on,
  stale-replay event, lock re-check decision, read-offset merge.
- ctest (l73-sdk-xma-b, Release, -j 8): 2078 tests, 2073 passed + 4 skipped + 1 failed: "Manifest: WriteSdkVersionStamp overwrites existing" (remove_all of %TEMP%manifest_test in use: it ran concurrently with "WriteSdkVersionStamp inserts when missing", which uses the same directory; unrelated to XMA). Re-run of the Manifest tests alone: 10/10 passed. Net 2074 passed, 4 skipped = baseline 2067 + 7 new. No new compiler warnings in the changed files.
- No XMA decode test existed before (only the route audit's XMA PCM classification); it stays green.
- clang-format (git-clang-format against e412c5c) clean; sources ASCII.

## Owner A/B proposal
- Arm "voice-fix": `xma_swap_carry_packet=true xma_valid_only_when_written=true
  xma_work_lock_recheck=true xma_swap_trace=true`.
- Arm "base": all four false (or only `xma_swap_trace=true` for comparable logs; the trace does not
  change decoding).
- Same Pier cut-scene, the phone call and a radio-music driving stretch; record and re-run the
  autocorrelation: the 11.61/23.22 ms taps should disappear in the voice-fix arm. Check logs for
  `carry_fallbacks` and WARN lines; if voice-fix garbles or silences multi-stream audio, retest with
  only `xma_swap_carry_packet` toggled.

## Build and install
- Build: `private/l73-sdk-xma-b` (output `private/l73-sdk-xma-o`), same configuration as l73-sdk-steady-b
  (VS 2022 Build Tools, LLVM clang, Ninja Multi-Config, -march=x86-64-v3, D3D12, Tracy, perf counters).
  Scripts: `%TEMP%\claude\C--BDU-MCLA-Recomp\l73-xma-scripts\{build,test,install}.ps1`.
- Installed into `private/l73-sdk-install` (overwrote the e412c5c install), version.h
  `0.20.0.3-dev.g4a3aea1`. SHA-256:
  - bin/rexruntime.dll   ee1173e1d2be7853ed30ab25b4c16ed6e4da5b9728be36897420215faa3af676 (11275776 B)
  - bin/rexgpu-xenos.dll 9ede9314fd29897c1c3fc6dfe498f542fc07cffb970d33604424c76cd44f26d2 (3253248 B)
  - bin/TracyClient.dll  01cf28a803fcf6dd078ef68a2fea8f2eff53235f856c7a57e1a99935822328b6 (246784 B)
