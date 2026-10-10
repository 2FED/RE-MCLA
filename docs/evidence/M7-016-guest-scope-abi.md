# M7-016: guest-scope ABI and platform-longjmp witness

Status2026-09-06: compiled/runtime and independent final return reviews pass.
This is a guest-free compatibility witness, not a game or FPS test.

## Scope

Three separately compiled strategies use the actual installed PPCContext,
PPCFunc, hook and explicit-context import APIs:

- Original SDK RAII scope.
- Normal-completion wrapper preserving the two-argument C entry and weak alias,
  with a private three-argument C++ body overload.
- An emitter-shaped completion label after an inner body-local lifetime.

Three source translation units exercise six standard-control and six explicitly
platform-specific longjmp cases. Coverage includes weak fallback, strong hook
override, typed original calls, pointer tables in both declaration/definition
TUs, recursion, early return, fallthrough and destructor ordering. Guest-register
effects and sticky import metadata are checked independently of PC/function
restoration.

The jump registry retains external-inline linkage and one shared thread-local
map across all three TUs. The fixture follows the exact emitter order:
save context, assign setjmp's result into a separate temporary, restore the
saved context on nonzero return, then write that result into r3. Values0,7 and−7
cover normalization and signed propagation. Thirty-two logical canaries sample
register/vector/special-state regions without comparing uninitialized padding.

Skipped-frame journal destructors record transient breadcrumbs and exception
counts. Final context equality alone is insufficient. These longjmp cases,
including the assignment-context expression and nontrivial skipped destructors,
are pinned platform characterization, not an ISO-C++ portability claim.

## Build and linked review

Private directory: `guest-scope-abi-witness-20260906`.
The first configuration failure is retained under `rejected-config-v1`: importing
the real hook header required the consumer's existing external-fmt/spdlog
definitions. Adding the three recorded host definitions corrected configuration;
no dependency or SDK installation changed.

1198 inputs/210,691,180bytes are frozen. All24 compile/assembly/IR/link commands
pass; independent review verifies99 recorded artifacts. No LTO or forced body
inlining was added, and no ReXGlue runtime DLL is required.

| Strategy | PE SHA256 |
|---|---|
| SDK | 211E762E963A99D48F6942788BB4B32A64B2B0DB30B7DA0FD447768F110BABE3 |
| Wrapper | A47EF4E2CEC49024F4E765DE602C65D8107D7A65AF41F4A15D67F75DDA31C2A0 |
| Completion | 19B969776C82B5B17D1DA821C11D5B2F202ADCDA89D86554AEB97FFB8ECA30A3 |

All strategies preserve five weak/original alias pairs; the strong hook remains
distinct. The wrapper inlines ordinary bodies but retains a real extra body frame
for the setjmp target. Completion retains the original single-function shape.
IR marks `_setjmp(ptr, frameaddress)` as returns-twice; linked imports resolve
`__intrinsic_setjmp` and `longjmp` through VCRUNTIME140. Saved context is restored
before the separate return value is written into r3.

## Native and exact-Proton results

All six executions exit0 without force, survivors, cleanup errors or recorded
input drift. Five exact mapped images are checked per run, including matching
app-local C++ runtimes. No DLL override is used. Native startup sets the parent
process error mode for launch; the child's inherited mode is not directly observed.

Each output has187 rows:12 cases,88 named checks and65 observation points,
plus preamble/intent/characterization/completion records. All180 semantic rows,
after normalizing the strategy label, match across all three strategies and both
runtimes. In skipped-frame jumps, both journal destructors execute with an
observed uncaught count of1 and preserve the leaf breadcrumbs until target
context restoration. This is observed behavior of these exact runtimes.

- Input manifest:8A4FB9AAE04FCBD2A2C8BB35070A5AC86A708F10AE94D744221636FEF5954096
- Native helper:6522D17A26AA3BC124AF4DD60CDF07C1DF099B4113694639616CB89AFDF18E9C
- Proton helper:68CFD6A4B2E87A2A4D9A8626288AB5C604CE00E92B446D3BEF31A942F9C2F1DA
- Proton SDK result:696CEDECE5B79727D93AB269AE19060AC64F9926A9FB72A53E5AB9B68DA2EE4C
- Proton wrapper result:A3C9536253D9E0CB1930D690B3FB30759BFDB106A78DEED437933EFC83637CF5
- Proton completion result:F8874CE577CE7C168A2F3388FF6D933D6FA15FF250281B6041B567AF026C8060
- Return verification:68CE2E9D0AA9843B840F5B0FE11B80565AD46CD17FA7B168F28DC86FFD57B28C

The33 copied return files total236,388bytes. Original machine receipts keep their
capture/pending labels; the separate comparison records subsequent acceptance.
Independent review checks the expected ordered tuples as well as cross-run
equality; it finds no evidence requiring a speculative catch/rethrow variant.

## Limits and next gate

Observation calls intentionally affect optimization. Completion already eliminates
some transient current-function stores while retaining constant journal values;
these passing observations do not establish asynchronous publication.
The [separate scope investigation](M7-016-guest-scope-overhead.md) records that
nonvolatile vector operations need their own publication/control tests.

This fixture does not cover every generator return/SEH form, all context fields,
Tracy integration or gameplay. No performance gain, new game candidate, SDK
promotion, version bump, task closure, commit or push is claimed. M7 remains0/16.
