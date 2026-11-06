# KI-071 "dialogue plays twice with a small delay": where the host could double a voice (read-only RE)

Read-only RE by an Opus subagent on 2026-11-06 (SDK d08813b XMA/APU path, SDL driver, the game's XMA playback library in the generated code); accepted by the primary agent. Decision: implement the three fixes behind cvars (default off) plus a swap trace, pending the echo measurement for confirmation.

Read-only analysis, 2026-11-06. SDK worktree `private/l71-sdk-wt` at `d08813b`; guest code in
`generated/default`. Nothing was built or run. No physical result is claimed.

## Summary
- The render path (AudioSystem plus the SDL driver) cannot play a guest frame twice. Each submitted
  frame is copied once and popped once. An underrun produces silence, not a repeat.
- The game drives the XMA hardware itself. It uses a statically linked XMA-playback library (sub_823ED440..sub_823EE050)
  with lock, snapshot, modify, write-back and kick. Its source voice can carry **N XMA streams that share
  one input buffer**, with stream i starting at packet i. The host decoder has three places where it
  can hand the guest audio that it already played, or the wrong stream's audio:
  - **C1 (ranked first): multi-stream packet carry-over across an input-buffer swap.** At every buffer
    boundary the host restarts every stream at packet 0. The correct start is packet `next_packet_index - packet_count`.
    From the first buffer swap on, stream 1 and above decode stream 0's packets, so a second channel
    group carries a time-shifted copy of stream 0.
  - C2: the host clears `output_buffer_valid` when the output ring is empty and nothing was written.
    The guest reads "not valid" as "ring full" and replays the whole ring.
  - C3: a Work/lock race reverts guest-owned offsets, so the host re-decodes frames or the guest
    re-reads output.
- What remains is game-side (C4): a deliberate dry voice plus an environment/reverb send whose timing
  differs on the host. This analysis did not reach it.

## 1. Render path (no duplication possible)
- `src/audio/audio_system.cpp:106-157`: the worker waits on the per-client semaphore and runs the guest
  callback once per signal. The semaphore starts at `queued_frames_` (default 8, `:28-30`, `:230`).
- `src/audio/sdl/sdl_audio_driver.cpp:198-228` SubmitFrame: `memcpy` of the guest frame into a
  private buffer (`:213`), then push to `frames_queued_` (`:224`). Later guest reuse of its buffer
  cannot alias.
- `sdl_audio_driver.cpp:341-391` SDLCallback: `front(); pop()` (`:358-359`). A frame is consumed
  exactly once. The semaphore is released once per played frame (`:388`). On an empty queue it
  outputs **silence** (`:344-356`) and does not release the semaphore. Nothing repeats the last frame.
- "Frame consumed" timing: the semaphore is released when a frame is handed to SDL, about 8 frames
  ahead. The guest callback produces exactly one new mix per release, so it never mixes the same voice
  region twice because of host timing. The game's mixer advances its own XMA read pointers (see 3).
  The log evidence (`audio_underruns=0`) also rules out starvation artefacts.
- Verdict: no candidate in the render or SDL path.

## 2. XMA decoder and context path

Constants (`include/rex/audio/xma/context.h:185-191`): an XMA frame is 512 samples per channel.
An output block is 256 bytes, which is 128 mono samples or 64 stereo sample pairs (the "subframe"
the code consumes: `4 << is_stereo` per frame, `xma_context.cpp:661`). The output ring has at most
31 blocks. Packet = 2048 bytes.

### C1: multi-stream carry-over is wrong at an input-buffer swap (deterministic, continuous)
- Guest facts:
  - `sub_823FB960` (`generated/default/mcla_recomp.26.cpp` ~3600-3720) loops `r30` over the voice's
    stream count (byte +56). It calls `sub_823ED450` with the **same** buffer address/size
    `*(r31+8)` / `*(r31+12)` for every stream.
  - Only when a stream's context is fresh (both input buffers invalid, `sub_823ED610`) does it call
    `sub_823EDC80` with read offset `(i << 14) + 32` bits, which is packet i
    (`rlwinm r11,r30,14,0,17; addi r5,r11,32`).
  - For normal double-buffered refills nothing sets the offset. The hardware must carry stream i into
    the new buffer through the packet skip count.
- Host:
  - `src/audio/xma_context.cpp:593-594` computes `next_packet_index = packet_index + skip + 1`.
  - But `:693-708`: when `GetNextPacketReadOffset` returns the header sentinel (next index >= count,
    `:302-315`), it swaps and sets the read offset to **packet 0's** first-frame offset (`:699-700`).
  - `GetNextPacket` (`:281-300`) likewise returns packet 0 of the other buffer for frames that span
    the boundary (`:288-299`).
  - For stream 0 the carry is 0, so this is correct. For stream i>0 the context jumps onto stream 0's
    packets and then follows stream 0's skip chain for the rest of the wave.
- Secondary: `GetNextPacketReadOffset` steps `+1` packet (`:304-312`), not by that packet's skip, when
  a packet has no frame start. This crosses streams inside one buffer (rare: needs a frame longer than
  one packet).
- Audible result: after the first buffer swap of a multi-stream wave (a few seconds in), the channels
  of stream 1 and above play stream 0's audio.
  - Lag: the boundary time mismatch of the two streams plus at most about one packet. If XMA2 blocks
    are frame-aligned across streams, the lag is near 0 to about 1 frame (512 samples: 21.3 ms at
    24 kHz, 10.7 ms at 48 kHz). That is a comb filter, which sounds "hollow, in a barrel". Otherwise
    the lag is tens to a few hundred ms (slap-back).
  - Fingerprint: **the first seconds of a cut-scene (first input buffer) are clean, and the doubling
    starts at a fixed point and persists**. A first-frame glitch at the jump is also likely, because the
    MDCT state belongs to the other stream.
  - If the cut-scene voice is a 3-stream 5.1 wave (FL/FR, FC/LFE, BL/BR), FC would turn into an FL
    copy. Dialogue in FC would then be replaced, not doubled, unless the voice is also in stream 0.
    So which stream holds the voice matters (see E2/E4).
- Xenia canary has the same swap logic, so the absence of this report elsewhere does not clear it.
  The hardware carry semantics (`next - count`) are inferred from the guest's behaviour (the shared
  buffer, with packet i set only on a fresh start), not from a hardware document.

### C2: valid flag cleared on an empty ring with nothing written (full-ring replay)
- `xma_context.cpp:136-149`: after the decode loop, `if (output_rb.empty()) output_buffer_valid = 0`.
  `RingBuffer::empty()` is `read == write` (`include/rex/memory/ring_buffer.h:34`), and
  `write_count()` treats read==write as all free (`:61-62`). The upstream comment admits that this
  case is ambiguous.
- The guest's available-output helper `sub_823ED670` (`mcla_recomp.25.cpp` 40227-40310) returns
  `write - read` when valid is set, and **the whole ring (`output_buffer_block_count`) when valid is
  clear** (`loc_823ED6DC`). The consume helper `sub_823ED700` (40311-40555) does the same with the
  remainder to the ring end, then sets valid again (`oris r7,r8,32768`).
- Trigger: a kick (the guest kicks every pass in `sub_823EE050`) or a worker Work while the guest has
  drained the ring (read == write) and the decoder produces nothing:
  - no valid input: end of the wave, or the next stream chunk is not yet submitted
  - `error_status == 4`
  - only the 0xFF-skip or split-header packets that return without output (`:582-590`, `:597-602`).
- Result: the guest replays the stale ring content.
  - Lag/length: `block_count * 128` samples mono (`* 64` stereo) at the source rate. At 31 blocks
    that is 3968 samples (165 ms at 24 kHz, 83 ms at 48 kHz).
  - Character: a burst repeat at line ends or stalls, not a steady echo. On hardware, valid is cleared
    only when a write fills the ring.

### C3: Work/lock race reverts guest-owned fields (sporadic)
- `XmaContext::Work` checks `is_enabled()` **before** taking the lock and does not re-check it after
  (`xma_context.cpp:97-102`). `XmaDecoder::WriteRegister` Lock does Disable plus `Block(false)`
  (`src/audio/xma_decoder.cpp:310-325`).
- If the worker (`xma_decoder.cpp:140-163`) passes the check just before the guest's lock, Work runs
  after the guest's `sub_823EDF00` snapshot. That snapshot copies 48 bytes of the hardware context into
  the shadow (`mcla_recomp.25.cpp:41471`). The guest's unlock `sub_823EE050` (`:41668`) then writes
  those 48 bytes back with `stvx128` (dwords 0-11), which **rolls back our write offset and
  input_buffer_read_offset**. The next Work re-decodes the same frames with an advanced FFmpeg state,
  and any leftover subframes of the racing frame (`current_frame_remaining_subframes_`) are emitted
  first. Result: a repeat of n frames (n x 512 samples).
- Separately, `StoreContextMerged` writes the stale `output_buffer_read_offset` back (`:412`). That
  field is guest-owned and should not be stored. It can undo a guest consume made during an unlocked
  Work.
- The guest's current-context check reads register 0x606 (`0x7FEA1818`) xor 0x200. The host returns a
  rotating index without bit 9 (`xma_decoder.cpp:242-253`), so the guest never waits for "busy".
- Character: rare, irregular repeats. It cannot explain a constant doubling.

### Checked and not a duplication source
- Stereo `ConvertFrame` SSE interleave: the shuffle mask gives L0 R0 L1 R1 ..., which is correct
  (`xma_context.cpp:727-746`). Mono path correct. There is no two-mono-streams decode of one stereo
  context: `PrepareDecoder` opens 1 or 2 channels from `is_stereo` (`:463-485`).
- Loop handling (`:231-249`, `:422-446`, `:663-677`): limits and skips. A wrong limit gives a
  click or gap. Dialogue streams normally have `loop_count == 0`.
- Flush on Clear/Release (`:193-204`), so there is no stale MDCT tail across waves.
- Double Work (an inline kick at `xma_decoder.cpp:298-307` plus the worker): state lives in guest memory
  plus members, so a second pass continues from where the first stopped. No repeat.
- No `xma_*`/`apu_*` decode cvars exist (only `ffmpeg_verbose`, `audio_maxqframes`).

## 3. Guest side
- XMA library (`mcla_recomp.25.cpp` 39902-41700):
  - context set-up `sub_823EDCD8`, which calls `XMACreateContext`, stores hw id = (phys-base)>>6
    at +80, and writes the Clear register 0x6A0
  - lock `sub_823EDDA0` (register 0x690)
  - wait plus snapshot `sub_823EDE38`/`sub_823EDF00` (on a timeout it resets register 0x601 and
    zero-fills via `sub_823ED9E8`)
  - available `sub_823ED670`/`sub_823ED8B8`, consume `sub_823ED700` (block-granular read offset in
    dword 9 plus a byte remainder in halfword +82, sample count = bytes >> (1+stereo))
  - set input buffer `sub_823ED450`, set loop `sub_823EDB18`
  - unlock, write-back and kick `sub_823EE050` (register 0x650)
- Voice pump `sub_823FC9F8` (`mcla_recomp.26.cpp` 5297-6936): per pass, lock, then for each stream
  read available and consume into a 256-sample (+24 = 256) frame (`sub_823FF5F8`). It then raises
  "starving" when the remaining amount is at or below a per-stream threshold (halfword at 2*(i+138))
  and unlocks/kicks. Offsets compare block-granular with wrap. With the host's semantics (C2) the
  "valid clear = full" rule is the only place where host timing makes it re-read a region.
- Intentional two-voice playback (dry plus environment send) and the mode 1 / mode 3 matrix
  (`sub_8213A600`) were not traced. They remain C4 and need the RMS probe or a recording to separate.
- Which waves are multi-stream (stream count byte +56 > 1) is data-dependent and was not determined.

## 4. Discriminating experiments
- **E1 (cheapest, no build):** run autocorrelation or a cepstrum on the owner's 20-30 s Game Bar
  recording of the Pier cut-scene.
  - A discrete peak at a steady lag points to a duplicate:
    - about 10.7/21.3 ms (one frame at 48/24 kHz), or a few frames: C1 with aligned blocks
    - a fixed `block_count*128/rate`: C2
  - Watch when it starts: clean first seconds that then turn doubled means C1.
  - Sporadic repeats only means C2/C3.
  - A diffuse tail with no peak means C4 (game reverb).
  - Also check L/R cross-correlation: C1 on a 2-stream voice makes channel groups near copies with an
    offset.
- **E2 (small probe build, cvar-gated, numbers only):**
  - Log at each input swap in `XmaContext::Decode` (`:693-708`): `ctx id, is_stereo, rate,
    packet_count, next_packet_index, carry = next_packet_index - count`.
  - Log once per second: contexts sharing the same `input_buffer_0/1_ptr` (multi-stream voices).
  - Any `carry != 0` on a shared-buffer context during the cut-scene confirms that C1 is active on
    this title.
  - Add counters for C2 ("valid cleared with 0 blocks written") and C3 ("Work ran while disabled" /
    "store reverted a guest field").
- **E3 (fix A/B):** cvar-gated fixes:
  - C1: on a swap, start at `next_packet_index - count` in the new buffer, for both
    `GetNextPacket` and the read offset
  - C2: clear valid only if this Work wrote at least one block and write == read
  - C3: re-check `is_enabled()` after taking the lock in `Work`, and drop the
    `output_buffer_read_offset` store at `:412`
  - Owner A/B on the Pier cut-scene.
- **E4:** `audio_channel_rms_probe=true` on the same scene, to see which channels carry the voice in
  mode 1 versus mode 3. This decides C4, and which stream index the voice sits in for C1.
