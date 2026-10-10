# M7-016: bounded byte-copy witness

Date: 2026-09-07. M7 remains 0/16; stock Deck performance is open.

## Decision

Do not replace the two profiled CRT copies globally. Four byte-copy kernels
pass native Windows and exact Proton correctness gates, but their speed depends
strongly on size and cache reuse. REP MOVSB is often substantially worse on Deck;
temporal AVX2 has no broad gain. Streaming stores help selected cases, with no
material advantage in the rotating 1.875–5 MiB groups. These ordinary owned-memory
results do not establish Vulkan allocation behavior or game performance.

The earlier [caller capture](M7-016-gpu-copy-callers.md) identifies sampled
readback/upload residency, not copy invocation counts or traffic weights. Do not
turn the timing table into an estimated FPS gain. No SDK copy change is selected.

## Source, build and static review

Private evidence root: `private/evidence/M7-016/resolve-copy-witness-20260906`.
Four noinline C entry points implement CRT memcpy, REP MOVSB, AVX2 temporal
stores, and aligned AVX2 streaming stores with a bounded prefix/tail and SFENCE
before return after streaming. All accept distinct nonoverlapping byte ranges;
zero length touches neither pointer. No SDK, game, Vulkan, affinity or clock API.

The first build rejects the combined kernel TU because Clang's always-inline
`__movsb` inherits AVX2 features incompatible with the baseline target attribute.
Failed receipts and original sources remain in `build/source-attempt-1`. The final
build moves the unchanged baseline/REP bodies into their own non-AVX TU. No
benchmark case, vector kernel, timing rule or result was changed to fix compilation.

Clang20.1.8, O3, dynamic CRT, three separate TUs, no LTO/unity/PCH/fast-math.
All twelve compile/assembly/IR/link/inspection commands exit0. Independent static
review verifies source/artifact pins, real imported `VCRUNTIME140!memcpy`, REP
MOVSB, bounded vector accesses and tails, conditional SFENCE and VZEROUPPER.
Main/baseline contain no AVX; CPUID/OSXSAVE/XCR0 gate vector calls. SSE4.1 remains
a host prerequisite. Both AVX kernels preserve RSI/RDI and XMM6–15 with a168-byte
stack allocation; this cost is included. The counted batch retains indirect calls.

- PE: `8DC30294499C1C9A7431BE46F9EC2B03476E6B12252EC892C7AFAC2659A04D7D`
- Frozen build inputs/commands: `FF4F3637D2972FBE25249FB10F807A53335FAE1AD01726888BE8A2F29CF20E0E`
- Build result: `122ADF69EE1899F7C92B046F7802094E4D997B699698D5E0C9DDC86B926934CE`
- Native helper: `FC1440788F8282F94969E6632C05384634088FD4B9BC9DD32DD5BC85A334315A`
- Proton helper: `29432011A86F653AD03FC10DE4F8FA3634061FC0696D51BFEA154074869041C1`

## Runtime and correctness

Both runs use seed `0x4D37434F5059` and the same reviewed four app-local CPP DLLs.
Actual loaded images are the witness, VCRUNTIME140 and the platform UCRT;
MSVCP, atomic-wait and VCRUNTIME140_1 are absent as permitted by the imports.
The flushed module preamble and two-second identity hold precede the work timer.
Proton10 is exactly `1785139158 proton-10.0-4b`, with the existing runtime/container
pins and no DLL overrides. Only the small executable/helpers transfer; CPP DLLs
are copied locally on Deck. The experiment is outside the entire game sync root.

Each runtime passes all728 aggregate correctness cases: thirteen lengths,
fourteen placements and four kernels. Lengths include zero, vector/page edges,
2 MiB and5 MiB. Independent allocations have inaccessible boundary pages;
source is read-only. Full payload comparisons and all destination sentinel bytes
are checked. SEH rejects faults without repair/retry. The aggregate covers
411,778,360 payload bytes; individual case events are not emitted.

The224 timing rows balance kernel positions over four rounds, reverse size order
and alternate reuse regimens. Repeated pairs and nearly64 MiB rotating arenas
use the same offsets per comparison group. Every active destination is poisoned
before timing and fully compared afterward. Rows target10ms, cap copying at
256 MiB and require a full rotation. Full verification/warmup/poisoning are outside
row timing. The9s work budget is cooperative, supplemented by parent deadlines.

| Runtime | Work time | Parent elapsed | Outcome |
|---|---:|---:|---|
| Native Windows |3817.529ms|5.894s|exit0,728 cases,224 rows|
| Exact Proton on Deck |5498.217ms|16.934s, includes prefix setup|exit0,728 cases,224 rows|

Peak owned reservations134,234,112bytes stay below192 MiB. Both free everything,
report no partial/budget/cleanup failure and leave no surviving owned process.
Native PID42984 and remote wrapper148142 completed; exec49781 is closed.
Retained Proton stderr contains the existing optional32-bit MangoHud dependency
warning and normal fsync/prefix initialization messages; no title was launched.

## Deck timing result

Each value is the median of four paired time-per-byte ratios to CRT; below1 is
faster for this specific regimen. No statistical population or call weighting
is implied. Native results are retained separately and also have no uniform winner.

| Bytes | Regimen | REP | Temporal AVX2 | Streaming AVX2 |
|---:|---|---:|---:|---:|
|32768|Repeated pair|1.042|1.335|1.885|
|524288|Repeated pair|15.294|1.081|1.139|
|1048576|Repeated pair|3.341|1.056|1.096|
|1966080|Repeated pair|8.098|0.993|0.731|
|2621440|Repeated pair|5.309|1.930|0.811|
|3768320|Repeated pair|7.753|1.999|0.772|
|5242880|Repeated pair|2.252|1.808|0.980|
|32768|Rotating|1.041|0.967|0.650|
|524288|Rotating|4.564|0.990|0.603|
|1048576|Rotating|1.244|0.973|0.598|
|1966080|Rotating|6.318|1.621|0.992|
|2621440|Rotating|4.712|1.607|0.992|
|3768320|Rotating|5.373|1.630|0.993|
|5242880|Rotating|2.124|1.605|1.008|

Streaming's lower copy time may shift costs to later consumers. Readback already
prefers HOST_CACHED, while upload allocation behavior differs. Neither is modeled
by assuming that these owned WB arenas reproduce actual mapped Vulkan memory.

## Verification and disposition

All11 returned files/84,695bytes match the fixed ten-entry SHA inventory plus
the inventory itself. Root reproduces both analyses, compares loaded-module
preambles and maps, checks frozen build artifacts and lifecycle receipts, and
rejects twelve deliberately malformed output cases. A review found missing
row/total-time consistency checks; those and reconstructed source-pattern
checksums are now enforced. Original analyses remain in analysis-v1; runtime
evidence and timing ratios are unchanged. Independent source/build, runner,
returned-result and strengthened-analyzer reproduction reviews pass.
Mapping/on-disk identity is not live IAT/code attestation.

- Native result: `E3EEB8FE434C71C1716C05853E48FFB2197F8A47B25A1776AE405C5A858729F3`
- Deck result: `3E062FE092346A9C3E2F4825BA11F19DDE852FB572EFEA0B10F8C1024907ED97`
- Native analysis: `C81320859D4EB1D96171D7F2AFAD33BC43A7FB2ADD27419B91685AC233E83414`
- Deck analysis: `715CCF8585B8C56E28633CD6338A4EB58338806EC064275BE3933BA0399319C8`
- Analyzer: `842A05701883503FEB85D28A621DD6148D1E4E1118511A35F8D93F1818816D30`
- Return inventory: `7E16708A9B5B9C48008F1D06C271191FA0C371B07DB3C5E9397EC1732DAC4685`
- Verification: `DF5824332B426A8594A0D7FA8826E94605207FFB4DA7BC4BBE2164DDC742BD99`

No repeat of the same microbenchmark is planned. Next is read-only semantics
review of conservative host scheduling in the known shared active-wait helper,
preserving every guest predicate, callback, threshold and return. No wait bypass,
copy suppression, coherency change, SDK promotion, version bump, task closure,
commit or push occurred.
