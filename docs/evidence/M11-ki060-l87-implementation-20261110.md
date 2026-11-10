# KI-060 Deck audio crackle, l87: engine-thread filter probe and native voice filter

Implementation draft, 2026-11-10 (Opus subagent; code written while the owner was playing, so nothing
was built or run in phase 1). Inputs: `docs/evidence/M11-ki060-audio-filter-analysis-20261110.md`
(the XAudio engine thread F8000534 spends most of its time in two scalar biquad effects). Spark not
used: deterministic reading of generated code, no bounded extraction worth offloading.

Status: **built and unit-tested (phase 2, after the owner closed the game), not run in the
game**: gate build 0 warnings, 56 suites with 0 failed executables
(`mcla_audio_filter_test: 115347 checks passed`, first build, no fixes needed). Owner build l87:
`docs/evidence/M11-l87-owner-build-20261110.md`. No physical PASS.

## Files

| File | Role |
| --- | --- |
| `src/mcla_audio_filter.h` | Pure parts: layout constants, the exact kernel (`StepBank`, `Process`), reset, flush, classification, bit comparison, MXCSR helpers, counters, line and F10 formats |
| `src/mcla_audio_filter.cpp` | Three cvars and three strong overrides (`sub_82146BE0`, `sub_82147738`, `sub_823EC248`) |
| `tests/mcla_audio_filter_test.cpp` | Kernel vs the generated loop body (interpreter of the recompiled statements) and vs a hand transliteration; offsets vs the generated text; counters and formats |
| `src/mcla_dev_overrides.h`, `src/mcla_m10_defaults.h` and their tests | Keys `audio_filter_probe`, `audio_filter_native`, `audio_filter_native_verify` (84 keys); the three cvars in the never-written list (71) |
| `src/mcla_diagnostics.{h,cpp}`, `src/mcla_app.cpp` | F10 block `"audio_filter"` |
| `CMakeLists.txt` | Source, test target `mcla_audio_filter_test` with `MCLA_AFILT_GENERATED_DIR` |

## Keys (all host init-only bools, startup, default off; `=false` writes nothing; no conflict pairs)

| Key | Cvar | Effect |
| --- | --- | --- |
| `audio_filter_probe` | `mcla_audio_filter_probe` | Times every engine frame (`sub_823EC248`), every `sub_82146BE0` and `sub_82147738` call; classifies `sub_82146BE0` inputs; one `MCLA_AFILT v=1` line per second of frames; F10 `"audio_filter"`. Reads only what the original reads; writes no guest memory |
| `audio_filter_native` | `mcla_audio_filter_native` | Runs `sub_82146BE0` natively (same arithmetic and rounding). Falls back to the original (counted) when the buffer argument or its sample pointer is 0 |
| `audio_filter_native_verify` | `mcla_audio_filter_native_verify` | One `sub_82146BE0` call in 64 runs the ORIGINAL on the real state after a native prediction from a host copy of its inputs; bit-for-bit comparison; first 8 mismatches logged as `MCLA_AFILT_VERIFY v=1`. Works with or without the native switch |

Arms planned for l87: `afprobe` = probe; `afnative` = probe + native + verify.

## Offsets (generated/default/mcla_recomp.0.cpp unless noted; M11 codegen)

| Item | Value | Proof (line: instruction) |
| --- | --- | --- |
| Object, buffer argument | r3, r4 | :55202 `mr r31,r3`; :55206 `mr r3,r4`; :55244 `addi r4,r1,80`, `bl 0x823eaf00` |
| Sample pointer | `[arg + 20]` | mcla_recomp.25.cpp:34234 `addi r11,r3,-8`, :34252 `lwz r8,28(r11)`, :34254 `stw r8,8(r4)`; :55456 `lwz r6,88(r1)`, :55464 `addi r11,r6,8`, first sample `lfs f0,-8(r11)` :55495 |
| Bank A switch | `+80 < 23900` unsigned | :55204 `li r10,23900`, :55208 `lwz r11,80(r31)`, :55211-55222 `subfc`/`subfe`/`clrlwi r11,r7,31` |
| Bank B switch | `+76 != 0` | :55210 `lwz r9,76(r31)`, :55218-55230 `subfic r6,r9,0`/`subfe`/`clrlwi r27,r5,31` |
| Neither on | return r3 = 0, no object access | :55234-55240 branches to `loc_821475EC` (:56881-56896) |
| Bank A words | `+4..+36` (c0..c4, h0..h3) | :55248-55258 `addi r29,r31,4`, `li r5,36`, `addi r3,r1,144`, `bl 0x8244d5c0` |
| Bank B words | `+40..+72` | :55259-55269 `addi r28,r31,40`, `li r5,36`, `addi r3,r1,96` |
| f31 (reset/flush value) | float at `0x82000ED4` | :55270-55277 `lis r11,-32256`, `lfs f31,3796(r11)` |
| History reset | bank on and `+100` (A) / `+101` (B) != 0 | :55280-55297 `lbz r11,100(r31)`, `stfs f31,176/168/172/164(r1)`; :55306-55324 `lbz r11,101(r31)`, `stfs f31,128/120/124/116(r1)` |
| Bank A refresh | `+88 != +80`: `sub_82146AD0(r3=obj, r4=1, r5=+80, r6=r1+144)`, return 0x82146CE0, copy to `+4..+20` | :55327-55374 |
| Bank B refresh | `+84 != +76`: `sub_82146AD0(r3=obj, r4=0, r5=+76, r6=r1+96)`, return 0x82146D28, copy to `+40..+56` | :55377-55424 |
| `sub_82146AD0` | reads r4 low byte, r5, r6 only; writes five words at r6 | :55035 `clrldi r11,r5,32`, :55045 `mr r30,r4`, :55052 `mr r31,r6`, :55076 `clrlwi r8,r30,24`, :55140-55162 `stfs f6..f2,0..16(r31)` |
| Flags and caches | `+101 = (B off)`, `+88 = +80`, `+84 = +76` (re-read), `+100 = (A off)` | :55447-55472 |
| Loop | 32 x 8 samples in place | :55467 `li r10,32`; :56681-56688 `addic. r10,r10,-1`, `addi r11,r11,32`, `bne 0x82146da0` |
| Flush and write-back | A: h0->+24, h2->+32, h1->+28, h3->+36; B: h0->+60, h2->+68, h1->+64, h3->+72; f31 when `bits & 0x7F800000 == 0` | :56689-56880 |
| Return | r3 = 0; r1, f29-f31, r27-r31, lr restored | :56883-56896 |
| Engine frame | `sub_823EC248`, engine thread call returns to 0x823EC96C | mcla_recomp.25.cpp:38273-38282 (`bl 0x823ec160`, `bl 0x823ec248`), then `KeSetEvent` :38293 |

Every row is also checked by `tests/mcla_audio_filter_test.cpp` (TestGenerated) against the
generated text.

## Exactness argument

1. **Arithmetic.** The recompiled loop keeps every value as a double that is exactly a float: each
   `fmuls`/`fadds` is `double(float(a op b))`, each `fmadds`/`fmsubs` is
   `double(float(std::fma(a, b, +-c)))` (fma in double, then rounded to float), and each `lfs`/`stfs`
   converts float <-> double. `StepBank` performs the same ten operations per bank and sample in the
   same operand order with the same `Round(std::fma(...))` and `Round(a * b)` expressions. Every
   operation is correctly rounded IEEE (std::fma is exact whether it is the FMA instruction or the
   library), so equal inputs and equal MXCSR rounding give equal bits. Each multiply result is
   rounded to float before it feeds an add, so no contraction can fuse anything differently.
2. **Same per-sample formula everywhere.** The unrolled loop has 16 blocks (8 samples x 2 banks). A
   symbolic interpretation of all 1206 loop statements (phase 1, scratch script) and the unit test's
   numeric interpreter of the same statements both show that every block is the same formula as
   `StepBank` on its bank's stack slots, for the three bank combinations, with exact operand order.
   The unit test compares 660 cases of 256 samples (random, silence with -0, denormal, impulse,
   +-30000, 1e-36 inputs; zero, running and denormal history; two f31 values; RBJ designs and random
   small coefficients) bit for bit, history after the flush included.
3. **State in registers.** Keeping the history in doubles across samples equals the stack round trip
   because every stored value is already float-exact.
4. **FPU environment.** The native path calls `ctx.fpscr.disableFlushMode()` first, as the original
   does before its first FP instruction (FTZ and DAZ cleared, the PPC scalar IEEE mode); the rounding
   mode is the thread's, the same for both.
5. **Control and memory.** The native path reads the object and switch words at the points the
   original reads them (+80/+76 at entry, re-read before each refresh and before the flag stores),
   calls the guest `sub_82146AD0` exactly as the original does (same registers, return addresses
   and frame `r1 - 256`, back chain stored) when a cache differs, and writes only: the refreshed
   coefficient words, +101/+88/+84/+100, the 256 samples, the eight history words in the original's
   order, and, on a refresh, the back chain and the coefficient scratch inside the original's own
   frame region. It returns r3 = 0 with r1, lr, r27-r31 and f29-f31 unchanged. The volatile
   registers the original leaves behind (r4-r12, f0-f13, cr6, xer) are not reproduced: the function
   is reached only through the effect vtable from ABI-compliant title code.
6. **Known residuals (counted by the self-check, expected unreachable).** A signalling NaN copied
   unchanged (a disabled bank's history word, a coefficient copy) is written as its raw bits, whereas
   `lfs`/`stfs` may quiet it; NaN payload selection when two operands are NaN may differ between
   the original's and the native code's instruction forms. Audio samples and filter states are
   finite in practice.
7. **Self-check.** One call in 64 runs the original after the native prediction was made from a
   host copy; calls that would refresh coefficients (a guest call) are not compared (counted
   `verify_skipped`). A mismatch is counted and logged; it does not change what the game gets on
   that call (the original's output).

## Log formats

- Start (once): `MCLA_AFILT v=1 kind=start probe=<0|1> native=<0|1> verify=<0|1> mxcsr=<0|1>`.
- Per second of engine frames (probe on):
  `MCLA_AFILT v=1 ms= native= verify= frames= other= fps= need=187.5 fps_ratio= frame_ns_avg=
  frame_ns_max= busy= bq_calls= bq_ns= bq_ns_avg= bq_ns_max= bq_per_frame= bq_per_frame_max= bq_a=
  bq_b= bq_ab= bq_none= bq_zero= bq_denorm= bq_de= bq_ue= outside= ch_calls= ch_ns= ch_ns_max=
  ch_per_frame= ch_per_frame_max= ch_de= ch_ue= nat= refresh= fallback= verified= mismatch= vskip=`.
  `bq_*` = `sub_82146BE0` (a/b/ab/none = bank mix; zero = all 256 inputs +-0; denorm = any denormal
  input sample or history word; de/ue = calls after which MXCSR DE / UE were set, the sticky bits
  being cleared before the call and the caller's bits put back after), `ch_*` = `sub_82147738`,
  `other` = frame calls not from the engine thread loop, `busy` = frame time / wall time,
  `fps_ratio` = frames per second / 187.5 (48 kHz / 256). Shares are the counts over `bq_calls`.
- Mismatch (first 8): `MCLA_AFILT_VERIFY v=1 n= call= a= b= diffs= field= want=0x........ got=0x........`.
- F10: `"audio_filter": {"version": 1, "probe", "native", "verify", "seconds", "frames", ...,
  "filter": {...}, "channel": {...}, "native_calls", "refresh", "fallback", "verified",
  "mismatches", "verify_skipped"}` (session totals).

Timing uses `std::chrono::steady_clock` (QueryPerformanceCounter with the MSVC STL, the clock of the
host's other timing probes). The probe's per-call classification reads 22 object words and 256
samples outside the timed window.

## Not done

- No native replacement of `sub_82147738` (per-channel biquad with gain): probe timing only.
- The probe does not classify `sub_82147738` inputs (zero/denormal) and does not measure the XAudio
  queue; KI-060 acceptance stays with the owner's ears on the Deck.
- No game run: whether the native filter is bit-identical in the game, and whether it helps
  the Deck crackle, is for the owner runs (afnative arm: `mismatch=` / `MCLA_AFILT_VERIFY`).
