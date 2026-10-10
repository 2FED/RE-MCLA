# M7-016 nonvolatile vector breadcrumb control

2026-09-06. M7 remains 0/16; this is a private, guest-free compiler/runtime
experiment, not a game performance result. It follows the
[scope-overhead](M7-016-guest-scope-overhead.md) and
[ABI controls](M7-016-guest-scope-abi.md).

## Purpose and scope

Removing the two exception-count queries from every guest function also removes
compiler-visible calls. The earlier scalar fault fixture uses Windows
atomic-volatile-acquire loads; it cannot establish how ordinary SIMD accesses
behave. This fixture uses the SDK's actual aligned SIMDe load, VectorMaskL byte
reversal and register store, plus the corresponding vector write. Guest pointers,
vectors and breadcrumb fields remain nonvolatile. Bodies and handler are separate
translation units without LTO or body observer calls.

Three arms compare the existing SDK scope, plain normal-completion continuation,
and continuation with `std::atomic_signal_fence(seq_cst)` before and after entry,
PC publication and normal restoration. Five cases cover entry read, a read before
a later PC publication, a later read, an accumulated same-PC loop, and a write.
Both reads in each pair and all twelve loop vectors contribute to checked output.

The handler accepts one continuable access violation at the exact owned page
start on the armed thread. It records breadcrumbs and changes only that page
from NOACCESS (read) or READONLY (write) to RW, then resumes the instruction.
No game memory or instruction-pointer modification is involved. Independent
scalar byte oracles check vector and memory results; normal caller restoration,
mechanical recovery, expected visibility and cross-arm equality are separate.

## Build and observed result

Private evidence is under `private/evidence/M7-016/guest-scope-vector-witness-20260906`.
The pinned Clang 20.1.8 build freezes 1,197 inputs / 210,687,122 bytes and completes
five compile/assembly/IR/link commands. SDK headers and generated game files are
unchanged. The VS environment helper printed a command-shell warning; all five
actual compiler/link receipts exit zero, and their argument lists are retained.

- PE: `6874720ED07003F69BE06C7F0279E144CE77FA0D43476BD235F2B2C91E7CA130`
- Input manifest: `9F0D9F163190A3AAC17CFC5AB0ACD3EB8F20898139C5AA1BAFDAFDD5480A528F`
- Build result: `9ACBCA038B6F3EA59F2F5DAD0AE13684C5490B1E26CF88FBDFAB2D8F548F3C72`
- Native stdout: `D347D0595757E51BC738EFB59B818D4F976D8E8B7E397BB795D1957C1A5ED65A`
- Proton result: `16B83CE5CA03C4FB66EC40BC3BB6C1A8496A8E85CEB5936569A6D6211D90B3C5`

Native Windows completes all fifteen faults with correct bytes and normal
restoration. Existing SDK visibility matches the intended site in four of five
cases: the first PC store disappears in the earlier-read case, leaving the
caller's PC and current callee function. Plain continuation loses both fields
in all five cases. Published continuation retains all five intended sites.
The same PE has completed exact Proton10 with app-local C++ runtime files copied
on Deck, no overrides, a fresh isolated prefix, exit zero and no forced cleanup
or survivors. Primary reproduction and independent source, binary, helper and
analyzer reviews pass. Both runtimes have identical normalized observations.
All eleven returned files / 63,474 bytes, 1,197 inputs and 21 build artifacts
verify. Five byte-oracle known answers and ten altered-output rejection cases
pass. Vector index eight identifies the fault address; it does not prove that
eight earlier reads had retired.

- Analyzer: `82530C1D56D9184D081532B79016A80F6970BAD43B4F1B1E6E6E8C0A399C5847`
- Native analysis: `D3EA405C94ECBC63F9405FA34E0F78D08ED83B5347FD4B391989C0E644C5DF55`
- Proton analysis: `AE437665CCFE03D2762D609D4750B60F8B34E333B881D250F9A32A108F40E2C5`
- Verification: `3FD114B8D5CF9889B82716701B6002CA9B1D451AA0AADD9CD8560BD16B5E4B2F`

## Compiler cost and limits

Published bodies have no exception-count calls or CPU-fence instructions. Their
compiler boundaries are not free: the loop unroll changes from four to two,
per-iteration PC and intermediate vector stores remain, and mask loads repeat.
The old SDK loop hoists its identical PC store. Actual game measurement must
decide whether removing CRT calls outweighs these costs.

This establishes no general SEH/signal guarantee, full generated-code safety,
unhandled-fault recovery or FPS improvement. The documented M3 contract remains
C++ guest unwinding and last tracked entry/block information. A reviewed private
game candidate and its control measurements are the next step after accepting
the returned evidence; no production SDK change or promotion is implied.
