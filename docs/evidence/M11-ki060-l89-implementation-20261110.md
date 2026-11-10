# KI-060 Deck audio crackle, l89: both voice filters native without the library fma

Implementation, 2026-11-10 (Opus subagent for the primary agent). Inputs:
`docs/evidence/M11-l87-deck-analysis-20261110.md` (ranked levers),
`docs/evidence/M11-ki060-l87-implementation-20261110.md` and the M7-016 FMA notes
(`M7-016-software-fma.md`, `M7-016-fma-tu-candidate.md`, `M7-016-residual-fma.md`,
`M7-016-fma-runtime-matrix.md`, `M7-016-unpaused-fma-window.md`, `M7-016-tune-znver2.md`,
`M7-016-audio-mixer-cost.md`). Spark not used: deterministic reading of generated code,
compiler output and logs, nothing worth offloading.

Status: **built and unit-tested, not run in the game.** No physical PASS. Owner build l89:
`docs/evidence/M11-l89-owner-build-20261110.md`. `private/tools/wait-for-idle-pc.ps1` ran before
every build, test and benchmark command (all IDLE; no "hold" received).

## Part 0: audit of the M7-016 "strict hardware FMA" setting

**(a) It is not in effect, and it never was on main.**
- `git log -S` over all refs: the only CMake code that put `-ffp-model=strict -ffp-contract=off
  -mfma` on `generated/default/mcla_recomp.0.cpp` is commit `e477649` (2026-09-11, "feat: prepare
  Zen+ wheel performance comparison", option `MCLA_WHEEL_CPU_CANDIDATE`, TU0 pinned by sha256
  `2d6028a7...`), extended by `45861f4` (2026-09-13). Both are on branch `codex/wheel-tester` only.
  That branch is not an ancestor of main (merge base `201c3a6`, 2026-09-06), so main never had it.
- The 2026-09-06 M7-016 candidate itself used a private CMake project-include hook in private
  experiment builds only (`M7-016-fma-tu-candidate.md`). "Only generated TU0 currently uses strict
  hardware FMA" (`M7-016-residual-fma.md`) described those experiment builds, not main.
- Main `CMakeLists.txt` has no `fma`, `-ffp-model`, `-ffp-contract`, `-march` or `-mtune` text. The
  owner-build scripts (`l88_host_build.ps1` configure, same options as l81+) add none either.
- `private/l88-host-build/build.ninja`, TU0 and `src/mcla_audio_filter.cpp`: `FLAGS = <prefix maps>
  -O3 -DNDEBUG -std=gnu++23 -D_DLL -D_MT -Xclang --dependent-lib=msvcrt -msse4.1`. No `-mfma`, no
  `-ffp-model` (Clang 20.1.8 default `precise`, which allows contraction only where the target has
  FMA; `-msse4.1` has none, so nothing is fused).
- Object code of l88 (llvm-objdump of `mcla_recomp.0.cpp.obj`):
  - `__imp__sub_82146BE0`: 96 `call fma` (REL32 relocations to `fma`), 0 `vfmadd`;
  - `__imp__sub_82147738`: 72 `call fma`, 0 `vfmadd`;
  - the l87/l88 native kernel (`mcla_audio_filter.cpp.obj`): 24 static `fma` call sites, 0 `vfm*`.
- `mcla.exe` (l88) imports `fma` (ordinal hint 192) from `api-ms-win-crt-math-l1-1-0.dll`; 62 of
  the 63 generated TU objects reference `fma`.
- `M7-016-audio-mixer-cost.md`'s "96 hardware FMA instructions" was measured on the experiment
  host (ABF); it does not hold for any main build.

**(b) std::fma sites per generated TU (count of `std::fma` occurrences, M11 codegen):**
TU0 185, 1 107, 2 163, 3 4, 4 51, 5 258, 6 40, 7 26, 8 137, 9 103, 10 82, 11 11, 12 51, 13 99,
14 108, 15 266, 16 151, 17 298, 18 176, 19 77, 20 322, 21 185, 22 45, 23 97, 24 115, 25 169,
26 26, 27 41, 28 266, 29 135, 30 56, 31 57, 32 77, 33 249, 34 37, 35 166, 36 1023, 37 126,
38 304, 39 296, 40 249, 41 143, 42 204, 43 233, 44 160, 45 46, 46 182, 47 171, 48 215, 49 4,
50 0, 51 27, 52 41, 53 24, 54 130, 55 171, 56 155, 57 164, 58 190, 59 92, 60 98, 61 120,
62 114; total 9118. Hot threads (M7-016 notes, no new measurement): the XAudio engine thread
(M7-016 "XThread0240" / title TID 224120; M11 F8000534) through `sub_82146BE0` (96) and
`sub_82147738` (72), both TU0, the only sites that this change removes; the residual UCRT fma leaves
of `M7-016-residual-fma.md` (1.94 % of one capture, six TIDs) return into TUs 2, 5, 24, 29, 36, 38,
47, 54 and 61, of threads whose roles are not established.

## Part A1: exactness proof (sub_82146BE0 and sub_82147738 kernels)

The recompiled fmadds/fmsubs are `double(float(std::fma(a, b, +-c)))`. In both functions every
FP register is written only by `double(temp.f32)` (lfs/lfsx), by `double(float(...))` or by fmr of
such a register, and is written before its first read (checked for every statement of both
generated bodies by `CheckFloatExact` in the unit test). So a, b and c hold floats exactly, and:

1. a * b is exact in double: at most 24 x 24 = 48 significant bits (<= 53), magnitude 0 or in
   [2^-298, 2^256), inside the normal double range: no rounding, overflow or underflow.
2. fma(a, b, c) = round64(a*b + c). The double expression a * b + c is round64(round64(a*b) + c)
   = round64(a*b + c) by 1. This holds in every rounding mode, and the sign of an exact zero sum
   comes from the same operands (the rounded product is the exact product).
3. Both then go through the same round32 (`double(float(...))`), so the float bits are equal.
4. A fused instruction would give the same result (by 1), but none is formed: `MulAdd` keeps
   product and sum in separate statements under `#pragma clang fp contract(off)`, and
   `src/mcla_audio_filter.cpp` is compiled with `-ffp-contract=off` (and `-fno-slp-vectorize`,
   below). l89 object: 0 `fma` relocations, 0 `vfm*`, 0 packed `mulpd`/`addpd`.
5. The M7-016 Proton software-fma differences (sign of a rounded zero) need |result| = 2^-1075;
   float operands cannot reach that (2^-298 minimum product). The software fma's unquieted
   signalling-NaN addend cannot occur either (lfs widening quiets NaNs).
6. Residual (as l87): NaN payload selection may differ; unreachable for finite audio.

Tests (`tests/mcla_audio_filter_test.cpp`): the l87 660-case comparison of sub_82146BE0's
generated loop now runs the MulAdd kernel and also the l87 library form (all bit-identical);
`TestMulAddExact` checks, for 1,000,000 random finite float triples (all exponents, denormals,
both signs), 1,000,000 audio-range triples, 16,000 edge triples (full 24-bit significands,
2^-149 x 2^-149, max x max, signed zeros, one ulp around 1) and addends that cancel the product
exactly or within one ulp, that the double product is exact (`fma(a, b, -a*b) == 0`) and that
`MulAdd` equals `std::fma` bit for bit before and after the round to float (round to nearest; the
other modes follow from the proof, not tested).

`MulAdd` replaces `std::fma` in `StepBank` (sub_82146BE0, both banks and the sub_82147738 cascade)
and `StepSection` (sub_82147738 single section). The kernels are templates on the multiply-add, so
the l87 library form (`LibraryFma`) stays available to the test and the benchmark only.

## Part A2: native sub_82147738

Layout (generated/default/mcla_recomp.0.cpp, M11 codegen), also in `src/mcla_audio_filter.h`:

| Item | Value | Proof (line: instruction) |
| --- | --- | --- |
| Object, buffer argument | r3, r4 | :57135 `mr r31,r3`; :57137 `mr r3,r4` |
| Bypass | byte +68 != 0: return r3 = 0, nothing touched | :57138-57143 `lbz r11,68(r31)`, `bne` to loc_82147BFC |
| Descriptor | sub_823EAF00 with r4 = r1+96: r1+96/100/104 = [arg+12]/[arg+16]/[arg+20] | :57144-57148; mcla_recomp.25.cpp:34229 (`lwz r10,20(r11)`, r11 = arg-8) |
| Channel count | byte [arg+13] (`lbz r10,97(r1)`) | :57149-57150, re-read :57820 |
| Coefficients | floats +12..+28 -> f0, f13, f12, f11, f10 (c0..c4), read once | :57151-57170 |
| Count 0 | return | :57171-57172 |
| f5 (flush value) | float at 0x82000ED4 | :57173-57183 `lis r11,-32256`, `lfs f5,3796(r11)` |
| Channel loop | r4 = 0..count-1; block = [arg+20] + 1024 * r4 | :57184 loc_821477A8, :57206-57221 `lwz r11,104(r1)`, `add r11,r5,r11`; :57822-57829 `addi r4,r4,1`, `addi r5,r5,1024`, `cmplw`, `blt` |
| Channel bit | (1 slw r4) & byte +74 (re-read per channel); shift amount r4 & 0x3F, 32..63 give 0 | :57186-57195 |
| History arrays | h0 [[+32]+r6], h1 [[+36]+r6], h2 [[+40]+r6], h3 [[+44]+r6]; r6 += 4 only for channels that run | :57196-57224 (load order h2, h3, h0, h1); :57817 |
| Variant | byte +75 (per channel) != 0: cascade loop loc_821477FC (StepBank); == 0: single loop loc_82147A20 (StepSection) | :57204-57234; :57235-57531; :57540-57734 |
| Gain | each output `fmuls` by the float +60, re-read before every sample's output (8 loads per 8 samples) | e.g. :57245-57247, :57281-57283 |
| Flush | h0, h1, h2, h3 stored in that order (pointers re-read), f5 when exponent bits are 0 | :57735-57817 |
| Return | r3 = 0; f25-f31, r30, r31, lr restored; the object is never written | :57830-57850 |

The native path (`NativeChannel`) follows the same control flow and guest access order: bypass
byte, buffer argument (fallback to the original when it or the sample pointer is 0 while the
original would read through it), count, coefficients, per channel the mask byte, the four history
pointers, the cascade byte, the four history words, the 256 samples in place with the gain re-read
per sample, then the four flushed history words through re-read pointers. It writes only the
samples and the history words, as the original; it does not reproduce the original's stack
scratch below r1 or the volatile registers it leaves behind (the function is reached through the
effect vtable from ABI-compliant title code, as for sub_82146BE0 in l87).

The unit test runs both generated loops (every statement, through a small interpreter of the
recompiled forms with std::fma) against `ProcessChannel` (MulAdd) and against the l87 library form:
360 cases of 256 samples (5 designs x 6 inputs x 2 variants x 2 flush values x 3 history fills,
six gains including 0, -0 and a denormal) plus 80 random-coefficient cases, history after the flush
included; plus loop shapes (cascade 48 fma / 40 mul / 16 add, single 24 / 24 / 8, 8 sample loads,
8 gain loads and 8 sample stores each) and the offsets above against the generated text.

## Self-check (verify) and the l87 race

- One call in 64 of each function runs the ORIGINAL on the real state after a host prediction
  (separate counters per function).
- sub_82146BE0: a difference is now `raced` when `FilterRaced(before, after)`: a cache the
  original wrote differs from the copied +80/+76, or +80/+76 changed. The single l87 mismatch
  (a.c0 after a refresh the copy did not predict) is that case.
- sub_82147738: a difference is `ch_raced` when any object input (bypass, mask, cascade, the five
  coefficients, gain, the four history pointers) or the call's count or sample pointer changed
  across the original's run. Otherwise `ch_mismatch`, first 8 logged as
  `MCLA_AFILT_CH_VERIFY v=1 n= call= channels= mask=0x.. cascade= diffs= field=ch<k>.sample<i>|ch<k>.h<j> want= got=`.
- Bypassed calls, count 0 or no sample pointer are not compared (`ch_vskip`).

## Keys

`audio_filter_native` now covers both functions; `audio_filter_native_verify` both self-checks.
New: `audio_filter_native_ch_off` -> host init-only bool `mcla_audio_filter_native_ch_off`
(startup, default off, `=false` writes nothing, no conflict pair; never written by the M10
defaults): sub_82147738 stays recompiled (native and verify) while the other switches are on. 86
keys; `kNeverWrittenCvars` 73.

## Part A3: probe of the remaining leaves

`sub_823F4450` (gain-ramp mixer, called from sub_823F4570), `sub_823EFD30` (lwarx/stwcx. atomic
add under the SDK global lock, called from the frame's sub_823EBEC0 loop) and `sub_823EE050` (XMA
unlock/kick, called from sub_823FB5B8 / sub_823FC9F8) make no calls of their own and are
function-level hookable (weak `DEFINE_REX_FUNC` aliases; strong overrides). With
`audio_filter_probe` only: inside an engine frame of the calling thread each call is counted and
timed (two steady_clock reads); outside any frame only counted (`_out`). The call rates are not
knowable statically, so the guard is dynamic: a leaf above 50,000 in-frame calls per second in a
line interval is counted only from then on and logged once as
`MCLA_AFILT v=1 kind=untimed fn=<addr> calls_per_s= limit=50000`; its `_ns_per_frame` is then
scaled from the timed calls.

## Log format additions (all additive; `MCLA_AFILT v=1` kept)

- Start line: `... mxcsr=<0|1> ch_off=<0|1> kernel=muladd`.
- Per-second line, appended after `vskip=`: `raced= ch_nat= ch_fallback= ch_verified=
  ch_mismatch= ch_raced= ch_vskip=`, then for `mix` (823F4450), `add` (823EFD30), `kick`
  (823EE050): `<x>_calls= <x>_out= <x>_timed= <x>_ns= <x>_per_frame= <x>_ns_per_frame=`.
  Without the probe `ch_calls` now counts the sub_82147738 calls too.
- F10 `"audio_filter"`: the same fields (`"raced"`, `"ch_native_calls"`, ...,
  `"mix": {"calls", "outside", "timed", "ns", "ns_per_frame"}`, `"add"`, `"kick"`).

## Benchmark (this PC: AMD Ryzen AI 7 350; scratch program, not in the repository)

`afbench.cpp` (sha256 8B24C9F1...CF79) includes `src/mcla_audio_filter.h` and is built like the hook
TU: clang 20.1.8 `-O3 -DNDEBUG -std=gnu++23 -msse4.1 -fno-slp-vectorize -ffp-contract=off`
(static CRT, whose `fma` uses FMA3 like the system UCRT). Best of 15 x 20,000 blocks, three
runs, ns per 256-sample block:

| Kernel | l87 form (library fma) | l89 MulAdd | ratio |
| --- | --- | --- | --- |
| sub_82146BE0, both banks on | 2596-2607 | 1955-1956 | 1.33 |
| sub_82147738 cascade channel | 1782-1786 | 1616-1626 | 1.10 |
| sub_82147738 single channel | 1628-1629 | 1526-1527 | 1.07 |

Without `-fno-slp-vectorize` (the default -O3) the MulAdd kernels were 0.98x / 0.60x / 0.87x: the
SLP vectorizer packs the independent scalar operations of the recurrence with shuffles and
lengthens the dependency chain. Hence the per-file option in `CMakeLists.txt` (results do not
depend on it). This PC's fma is hardware-assisted, so the call costs little here; under Proton the
Wine builtin fma is software (M7-016: integer multiplies, `_dclass`, `scalbn`), about 25 ns per
call in the l87 Deck data (42.1 us per native call, ~1713 fma calls), so the Deck gain should be
much larger than this ratio. Not measured on the Deck yet.

## Gates (main checkout)

- `gate_host_build.ps1 build`: rc 0, 24/24 steps, 0 compiler warnings.
- `l57_host_tests.ps1`: `count 56`, `failed executables: 0`; `mcla_audio_filter_test: 2350517
  checks passed` (l88: 115347), `mcla_dev_overrides_test: 1378 checks passed` (1356),
  `mcla_m10_defaults_test: 5186 checks passed` (5111).
- `ast-grep scan -c sgconfig.yml` on the seven changed source/test files: no findings.
- `git diff --check`: clean; no new line over 100 columns.
- Mutation checks (scratch): swapping an operand in `StepSection`, perturbing the channel gain
  product, and rounding the MulAdd product to float each fail the test.

## Not done

- No game run: bit identity in the game, the Deck speed-up and its effect on crackle and music
  speed are for the owner runs (Deck afprobe vs afnative).
- No native replacement of the remaining leaves; the probe only measures them.
- No CPUID-dispatched hardware FMA for the other 9,000+ generated `std::fma` sites (whole-game
  fma cost under Proton unmeasured).
- `docs/known-issues.md`, `docs/M11-priorities.md`, `docs/current-state.md` and README-AI not
  updated (primary agent).
