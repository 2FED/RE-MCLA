# Hardware FMA for the recompiled game: design and bound (2026-11-11)

Read-only design by an Opus subagent, accepted by the primary agent as a design; nothing built. Spark not used: source reading.

**Headline.** The recompiler writes `std::fma` text into the generated code (SDK codegen floating_point.cpp:200-262); 63 objects reference the plain `fma` symbol and mcla.exe imports it from the UCRT. Cheapest safe mechanism: link-time interposition (our own `fma` in mcla.exe with a CPUID-selected hardware vfmadd target and the UCRT function as fallback; generated code unchanged). The bundle already requires an x86-64-v3 CPU (rexruntime.dll is built -march=x86-64-v3 with AVX2), so FMA3 excludes nobody. Bound on the gain: on the Deck, with the audio filters already native (l89), the remaining fma residency is 64 of 3,303 samples = about 2 % of process CPU spread over six threads; this is not a fix for 16 fps. Plan: stage 0 start-up benchmark line and per-thread call census behind a key; stage 1 interposition behind a dispatch switch with an exhaustive bit-identity test.

---

# fma design notes (read-only, 2026-11-11)
- Emission: third_party/rexglue-sdk/src/codegen/builders/floating_point.cpp:200-262 (std::fma text, singles as double(float(std::fma(...)))).
- l91 objects: 63 reference `fma` (62 generated TUs + mcla_audio_filter_test), 0 reference `__imp_fma`; mcla.exe imports fma (hint 192) from api-ms-win-crt-math; rexruntime.dll imports no fma.
- rexruntime.dll built -march=x86-64-v3 (private/l91-sdk-b/CMakeCache.txt:224); AVX2 (vpbroadcast*, vpermd) present in .text sample => bundle already requires v3-class CPU.
- Residual 1.94% (M7-016-residual-fma.md:8-10) are Proton UCRT RVAs (fma range 0x53C50-0x5433A, M7-016-software-fma.md:24) => Deck data, not Windows.
- Moving all-thread Deck profile: fma 409 + scalbn 25 + _dclass 8 of 4094 = 10.8%; audio thread 365 fma of 994.
- Matrix: Proton software 20 sign-of-zero mismatches (hardware matches exact oracle); Windows hw vs UCRT: 576 NaN-pattern diffs only.
