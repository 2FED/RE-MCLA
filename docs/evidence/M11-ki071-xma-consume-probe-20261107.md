# KI-071 step 2: guest XMA consume probe and the l74 dev keys (host)

Draft by an Opus implementation subagent, 2026-11-07. Host patch on main `ca64ff5` (scratch
`%TEMP%\claude\C--BDU-MCLA-Recomp\l74-host-patch`, copy-only `apply.ps1`). Nothing was launched;
no physical result is claimed. Inputs: `M11-ki071-sibling-contexts-re-20261107.md` (section 2, the
voice pump) and `M11-ki071-xma-sibling-trace-sdk-20261107.md` ("Guest-side consume hook", SDK
`8f949f7`).

## Purpose
The SDK sibling trace counts what the host decoded per hardware context (`frames_emitted`,
`frames_parsed`, drops). The consume probe adds what the game actually took out of each context, keyed
by the same hardware context id. Reading the two together:
- Siblings consume at the same rate but `lost` differs by k frames: the host dropped k frames (D2 or a
  decode failure). That sibling plays k x 512 source samples ahead.
- Siblings consume at the same rate, and the host counts are equal: the host is aligned.
- A sibling consumes less in some second: the guest underflowed there and zero-padded. The lag is in
  output samples, which is not the frame-exact doubling.

## Dev keys (src/mcla_dev_overrides.h, 39 -> 44)
| key | phase | writes |
|---|---|---|
| `xma_sibling_trace` | startup (rexruntime) | `=true` writes true, `=false` nothing |
| `xma_stall_on_invalid_next` | startup (rexruntime) | same |
| `xma_honor_start_skip` | startup (rexruntime) | same |
| `xma_reset_codec_on_clear` | startup (rexruntime) | same |
| `mcla_xma_consume_probe` | startup (host init-only) | plain bool, true/false |

- None of the five is in a conflict pair.
- All five cvar names are in `kNeverWrittenCvars` (src/mcla_m10_defaults.h, 31 -> 36), so the M10
  defaults module never touches them.
- An SDK without the four step-2 cvars (anything before `8f949f7`) logs the keys as unavailable.

## The hook (src/mcla_xma_consume_probe.{h,cpp})
- A pass-through strong override `REX_EXTERN(sub_823ED700)`, the same pattern as
  mcla_pursuit_probe.cpp.
  - With `mcla_xma_consume_probe` false (the default, init-only, read once), it only calls
    `__imp__sub_823ED700`.
  - With the probe on, it saves r3, r4 and r5, calls the original unchanged, and then reads the guest.
    All reads go through the checked reader (`mcla::guest::HookReader`). Guest memory is never written.

### Verified against the generated code (generated/default, M11 codegen)
Each item below is checked by the unit test.
- `sub_823ED700` is defined at mcla_recomp.25.cpp:40311. Its only direct caller is the voice pump
  `sub_823FC9F8`, at mcla_recomp.26.cpp:5692. A grep of all 63 generated files finds no other
  `bl 0x823ed700`. The call sequence is:
  - `bl 0x823ff5f8` (the resampler), then `mr r5,r3`: r5 is the resampler's return value, the samples
    consumed per channel
  - `addi r6,r1,80` (the out pointer)
  - `lwz r3,184(r31)` (the voice's XMA object), `mr r4,r27` (the stream index)
  - `bl 0x823ed700`
- **Stream record:** `rlwinm r11,r4,1,0,30` / `add r10,r4,r11` (3i), `lwz r11,8(r3)`,
  `rlwinm r31,r10,5,0,26` (96i), `add r11,r31,r11`. So rec = [r3+8] + i*96.
- **Stereo:** `lwz r7,4(r11)` / `rlwinm r10,r7,3,31,31`, so stereo = (dword1 >> 29) & 1. Then
  `addi r6,r10,1` / `slw r10,r5,r6`: bytes = r5 << (stereo + 1). This confirms that r5 counts samples
  per channel.
- **r4:** cleared by `li r4,0` only after it has been used for the offset, so the hook saves it before
  the call.
- **Hardware context id:** rec+80 (u16) is written once at create, in `sub_823EDCD8`
  (mcla_recomp.25.cpp:41146):
  - `XMACreateContext`, then `MmGetPhysicalAddress`
  - `subf r11,r11,r3` (phys - array base), `srawi r10,r11,6`, `clrlwi r9,r10,16`
  - `sth r9,80(r30)`

  The id is therefore the 64-byte context index, the same index as the SDK's
  `XmaDecoder::contexts_[320]`. The ids in `MCLA_XMA_CONSUME` lines match the SDK's `[xma] group:` and
  `first-decode` lines.

### Hot path
Per call, with the probe on:
- three checked reads: [r3+8], rec+80 and rec+4
- one relaxed atomic add into a 320-entry per-id sum, and a relaxed store of the id's stereo flag
- one relaxed load of the next-emission time

Records that cannot be used are counted as `bad` and are not summed:
- a stream index >= 64 (plausibility bound)
- a null stream array
- an unreadable record
- an id >= 320

### Lines (paced by construction: at most one second line and one minute line per second)
At most one caller per second wins a compare-exchange on the next-emission time (plus a try-lock). That
caller swaps every per-id sum to 0 and logs:

`MCLA_XMA_CONSUME v=1 kind=second ms=<interval> ids=<n> ctxs=[id:samples,...] run=[id:samples,...] stereo=[ids] more=<n> bad=<n>`

- `ctxs` holds the samples per channel consumed in this interval. The list is sorted by id and holds
  at most 32 entries; `more` counts the remaining ids.
- `run` holds the samples per channel since the id was last idle for a whole interval. Siblings that
  start together have comparable run totals.
- `stereo` lists the ids whose record had the stereo bit.
- There is no line for an interval with nothing consumed and no bad records.
- `ms` is the real interval length. After a quiet period it is longer than 1000.

Once 60 s have passed since the previous minute line, the winning caller also logs:

`MCLA_XMA_CONSUME v=1 kind=minute ms=<interval> ids=<distinct ids> samples=<total> calls=<hook calls> bad=<n> lines=<second lines>`

### Reading (cut-scene run, with the SDK group line for the same second)
- Per second, a mono 44.1 kHz voice consumes about 44100 samples.
- Compare `ctxs`/`run` of the cut-scene ids (5 mono contexts) with the group line's
  `frames_emitted` x 512 and `consumed_blocks` x 128.
  - Equal consumption with a `lost` difference of k means a host frame drop. The drop counter names
    the path.
  - A short second on one sibling means a guest underflow (zero pads).
- The pad count per stream (a wrapper on `sub_823FC9F8`, RE section 2) was not implemented. A short
  `ctxs` value is the indirect signature.

## Tests
- New `tests/mcla_xma_consume_probe_test.cpp` (suite `mcla_xma_consume_probe_test`; CMake definition
  `MCLA_XMA_CONSUME_GENERATED_DIR`) covers:
  - the layout helpers
  - the exact second and minute line formats
  - the ledger: the first call starts the clock; one line per second; sorted ids; interval sums; run
    totals and the idle reset; bad ids; no line for silent intervals; the minute summary and its
    restart; the 32-entry cap and `more`
  - the generated-code checks above
- `tests/mcla_dev_overrides_test.cpp`:
  - the 44-key count; lookups, phases and texts of the five keys
  - no conflict pairs; near-miss names
  - the l73 voice arm kept at 39 values
  - the five l74 arms (base, trace, d2, d1, all) at 44 values within 2 KiB
  - l74 near misses (non-bool values, prefix, repeats)
- `tests/mcla_m10_defaults_test.cpp`: the five cvars are never written by the defaults module.

## Arms (private/l74-arms, LF, 44 values each)
- pc:
  - `base` = l73 base + the five keys false
  - `trace` = base + `xma_sibling_trace` + `mcla_xma_consume_probe`
  - `d2` = trace + `xma_stall_on_invalid_next`
  - `d1` = trace + `xma_honor_start_skip`
  - `all` = trace + D2 + D1 + `xma_reset_codec_on_clear`
  - `steady`, `fetch-off`, `fetch-verify` and `voice-fix` = the l73 arms + the five keys false
- deck: `saves-fix` and `steady` = the l73 arms + the five keys false.
- The 4a3aea1 XMA cvars (`xma_swap_carry_packet` etc.) are off in base/trace/d2/d1/all, the same
  setting in every voice arm, as the SDK notes require.

## Build check (2026-11-07, after the idle guard reported IDLE)
- Scratch tree `%TEMP%\claude\C--BDU-MCLA-Recomp\l74-host-src`: tracked main `ca64ff5` with the patch
  overlaid. Built against `private/l74-sdk-install` (SDK 0.20.0.4-dev.g8f949f7) into
  `private/l74-host-check`, using the l73 owner-internal configuration (dev overrides, M10 defaults,
  expiry, D3D12).
- The build ran 270/270 steps with 0 compiler warnings. It produced mcla.exe (SHA-256 a46beda4...),
  which contains the `MCLA_XMA_CONSUME` and `mcla_xma_consume_probe` strings.
- 52 test suites ran: the 51 from l73 plus the new `mcla_xma_consume_probe_test`. Results:
  - `mcla_dev_overrides_test`: 446 checks
  - `mcla_m10_defaults_test`: 3048 checks
  - `mcla_xma_consume_probe_test`: 26 checks
  - `mcla_diagnostic_image_client_test` failed once with "bounded observed return". This is the
    timing-sensitive case also seen in l72. It passed 3 of 3 times when rerun alone. Unrelated to
    this patch.
- ast-grep 0.50.0: scan rc=0, test 3/3 passed.
- Arm files: all 11 parsed by the production parser, each with 44 values, 0 ignored and 0 issues,
  1313-1321 bytes. Expected values are taken from the l73 arms plus the arm definitions; 0
  mismatches.
- Sources are ASCII. Line endings follow the checkout: the dev-overrides header and test and the new
  probe sources are CRLF; the m10 defaults files, CMakeLists, the evidence, the checklist and the arms
  are LF.
