# M7-016: bounded FMA numerical and runtime controls

2026-09-06. Continues [software FMA attribution](M7-016-software-fma.md).
The owner renewed usage limits and resumed the investigation. This report
covers guest-free witnesses; no game candidate has been built or accepted yet.

## Accepted numerical matrix

The same 17,408-byte x64 console executable ran on native Windows and the
exact installed Proton10.0-4b. Its SHA256 is
21B2B4F1E05CDF6F14C6ADCB4DBB3E2DB6D1C2042FF8C1CEC2AA6BD826E9AD2B.
Each run emits17,280 case rows:45 operand sets, all8 emitted FMA forms,
four rounding modes, four independent FTZ/DAZ settings and three compiled
arms. Twelve additional rows exercise identical expressions across MXCSR
rounding/flush changes. Complete normal exit restores the saved environment
through successful fesetenv and verifies exact MXCSR readback; this is not
independent readback of every x87 environment field.

The arms are the ordinary host flags, strict software FMA and strict hardware
FMA. Strict arms disable implicit contraction. The baseline under nondefault
rounding is observational, not a numerical oracle. The independent Python
oracle uses exact rational arithmetic for round64 FMA, optional outside
negation, optional round32 and exact widening. Its normative arithmetic scope
is FTZ=DAZ=0; other flush combinations are recorded comparisons.

Primary and independent review accept the following evidence:

- All960 hardware finite-input checks,80 infinity-result checks and48
  invalid-infinity classification checks pass in each environment. All352
  NaN-input rows with flushing disabled remain NaN. Payload selection is not
  required to match the CRT.
- All5,760 hardware case records are identical across Windows and Proton,
  including result bits, CSR before/after, fenv and errno. All four hardware
  transition records also match. Both strict arms preserve the two operations
  in each transition; the ordinary baseline reuses a result in all four cases.
- Native strict software has no finite-oracle mismatch. Native hardware versus
  strict software differs in576 NaN bit patterns and no other recorded field.
- Exact Proton strict software has20 finite-oracle mismatches, all only the
  sign of a rounded zero. The hardware agrees with the exact oracle.

| Proton software case | Rounding | Forms | Rows |
|---|---|---|---:|
| Negative tiny product with opposite-signed zero addend | Nearest, up, toward zero | 1/3/5/7 | 12 |
| Half minimum subnormal with negative-zero addend | Down | 1/3/5/7 | 4 |
| Normal operands producing the same tiny positive result | Down | 1/3/5/7 | 4 |

The exact nonzero magnitudes in those cases are2^-1075. The software behavior
is consistent with rounding a product to zero before the opposite-signed zero
addition, but that branch explanation is an inference, not an exact-instruction
proof. Flags, fenv and errno agree between software/hardware in all20 rows.
No witness proves that game code reaches these boundary operands.

Other Proton differences are preserved. Its software double forms can return
an unquieted signaling NaN in the addend without invalid status (64 rows across
flush settings); hardware quiets it. Invalid infinity operations set software
errno33 in192 rows while hardware leaves the sentinel unchanged. NaN payloads,
flags and FTZ/DAZ-on finite behavior also differ. Thus the candidate direction
is explicitly **not exact CRT bit/flags/errno equivalence**. Existing SDK
FPSCR methods expose rounding rather than a complete host exception register;
this does not establish full PPC semantics or make every host difference
irrelevant to guest code.

## Build and runtime identity

The frozen build records12 successful compiler/linker/assembly/IR commands,
five exact source pins and36 artifact entries. Compiler Clang20.1.8 remains
57E32B1EA2456F12F97BF802453171BBFF12D56B2C486B4ECA0C75343C0CDA2C.
Primary and independent binary review verify eight double fused operations,
four outside sign flips, four narrow/widen pairs and two ordered operations
in each strict transition. The hardware harness checks CPU/OS support before
using FMA. GetModuleHandleEx receives the resolved __imp_fma pointer, not the
host thunk. Build receipt:
FBB84EA579902E7ED9A869E8BE18EB70DD30BEB2AF2A82C9601B09248DE96897.

Native Windows resolves fma to system UCRT
6AE500081442C38E0CC8D2E3DEA30D3AB7971688A60FF5D73DAA7F854974804D,
unchanged before/after. Native receipt:
DFB3D37C93525F87C40534848145AADF36B4E34DCDB56DCCEAF40555E61E0564.
The executed native runner is retained. A later failure-path review fixed
future timeout output retention and added runner self-pinning; it does not
retroactively claim a pre-execution runner hash or require repetition of the
completed exit0 numerical run. The private runner-review note records this.

Proton v4 uses supported getcompatpath to initialize a fresh isolated prefix,
then runinprefix with a pinned batch that captures the actual Windows child
exit code. This tests console numerical behavior, not normal Steam launcher
or gameplay performance. Wrapper and child both exit0; no forced cleanup,
cleanup errors or surviving owned processes; pinned sources remain unchanged.
The initial map during an explicit two-second pre-matrix wait verifies exact
PE and UCRT hashes and file identities. It is not an end-of-run mapping bracket.
The UCRT is the same CEC72E71... binary sampled in the gameplay profile.

Earlier failures remain frozen: v1 lacked the new prefix's parent directory;
v2 and v3 returned wrapper0 but no console output through Proton's Steam
launcher path. They are not numerical runs or successful child-exit evidence.
No cause beyond the observed launcher/output distinction is asserted.

The accepted return has6 files/2,709,179bytes, all independently verified:

- Result: DB582565E636B4A5D04540CEC00D8A0E372BC2B2E47C45748C0F5CA43A256414
- Inventory: 16F6877A5DA2F356C8425BD6E257AB5C051C26CAC5A357173C8BF81D7ADA298B
- Analysis: E8B166E654823AC99F003AC3480FF3B27EFE2594FC561BF5E89DA8FD1D769F09
- Analyzer: 461D9C37AD384C8C94E8513C884E4A354B1254CF4D125E319B6702FDD0A45FEE
- Remote helper: 910543F79B2DC6DAEFD00E230F31363689981C0B7EED55916C02077236E9BF43

Private evidence is under fma-runtime-matrix-20260906 in M7-016. The analyzer's
60 pure tests and byte-for-byte analysis reproduction pass. No private raw
output, map, executable or source artifact belongs in Git.

## Remaining candidate gate

The bounded matrix supports review of an isolated strict-hardware candidate.
A separate SDK-shaped cached-CSR four-operation sequence now passes source,
binary, native and exact-Proton review. It uses conditional flush enable,
then unconditional off/on/off, two entry states and double/narrow forms.
All12 rows match between runtimes;16 hardware results and all12 restorations
pass. The strict arms alternate zero and the expected2^-51 or2^-127 value;
the ordinary baseline's four all-zero sequences remain observations. This
contains no actual VMX instruction. Nine pure verifier tests pass.

The separate PE is0E67EC3A0612E1179E8F32B35DB97221B32C76EEE7D9624605177091DB065B92.
Private fma-csr-sequence-20260906 retains6 returned files/20,749bytes:

- Result: CE72051EC671ADC20360F32AD9F82EFF6CCD44B3FC16E48ED570D1562EAA1E84
- Inventory: 2D5D5BF4DBFBD959BC0555B22EDC8ED1DE2609A4E82A995A1888815F36CE75CE
- Analysis: 8984262F23B594E7E4FFA7090AE5D7E3041E19FAC5E482AD7AA55D04B747A0B5

Next is a narrowly scoped strict-only versus strict+FMA
translation-unit experiment, with unchanged generated inputs, exact compile
command/binary checks and isolated native/Deck validation. No plain global
FMA/v3 flag, SDK promotion, version bump, task closure, commit or push follows
from these witnesses. M7-016/KI-027 remains open and stock gameplay/audio
remains the previously measured NO-GO.
