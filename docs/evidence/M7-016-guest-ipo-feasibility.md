# M7-016: current-macro two-TU ThinLTO feasibility

2026-09-10. Compile-only private experiment; M7-016 remains active.
Private root: `private/evidence/M7-016/guest-ipo-feasibility-20260910`.

## Outcome

The baseline passed static review. ThinLTO compiled and linked normally, but
failed the agreed zero-import gate by retaining `calloc` from
`api-ms-win-crt-heap-l1-1-0.dll`. Its 99-byte caller is identical to baseline
except the displacement of the call to the relocated strong hook. Both public
calls and the r6 store/reload remain. No game build, DLL loading, runtime test,
performance gain or general semantic-equivalence claim follows from this witness.
Do not proceed to a whole-game LTO build on this evidence.

This is a narrow negative result for the current weak-alias/breadcrumb-fence
macros and this recipe, not proof that all LTO or generated-code optimization
is ineffective. The exact reason cross-module optimization did not happen and
the retained import's linker provenance have not been fully attributed.

## Source and bounded recipe

Two synthetic files use current PPCContext (2688 bytes, alignment64), generated
declarations, actual forced scope override3157F9BC and installed function/context
headers. No generated game function bodies are compiled. Public weak aliases,
strong-hook override, fallback entry, mutable address table and entry/restore
publication fences are retained. The callee modifies r5; the caller stores r6,
calls across TUs, reads r6, replaces r6 and calls the strong hook.

An independent precompile review found missing DECLARE_REX_FUNC declarations
and root corrected them before either compile. Final source manifest
C46603AA6C92CA9EBBE6EDD125E6BA94F909D07F700C18D05E604AA24A75264D.
Caller C2BFCBD6D1C4DE17F35F3147EF1FBAEBC3DA6F2C03780BDC5C7F2DE60EF38F33;
callee D62449535E88762ADAB7EA4569497C42B0345706A9B4CA56F38691501E2C6534.
Current TU27 compilation flags are retained; ThinLTO adds `-flto=thin`.
This does not cover the distinct strict-FP flags of full-game TU0 or the other
game translation units, guest ABI boundaries, hook population or runtime behavior.

Baseline inputF43C7B1C349C86F1BBA2231850F90078DA77754767542F3AE787010238F5B358;
ThinLTO inputE1619977F309FDD6DB773134343B7BED7C756DD947DD734439E7D91394401A9C.
Each preparation froze440inputs/427dependencies and only two .cpp files.
Compilation dependency output is checked against its preflight closure.
ThinLTO controllerF848BCD465334540C8C950FED4D59431775224B84283DF24F728B1E9DD835F11
is a separate root-resolved-v2.py, preserving the exact baseline helper.
It adds postcompile dependency checking and assigned-before-resume validation.
Compiler jobs use the existing owned Windows Job launcher and fixed working
directory; all completed normally, without forced cleanup or surviving children.

## Link diagnostics retained

Original no-library baseline link failed on9CRT symbols in unused STL locale
COMDATs. An explicit /OPT:REF-only retry failed identically. No game dependencies
were added to resolve those failures. LLVM20.1.8 diagnoses remaining undefined
symbols before markLive; GC is already enabled by default.
See [the exact LLVM linker source](https://raw.githubusercontent.com/llvm/llvm-project/llvmorg-20.1.8/lld/COFF/Driver.cpp).

The reviewed matched retry retained /DLL /NOENTRY /NODEFAULTLIB and supplied
the same three existing, hash-pinned x64 import libraries to both variants:
msvcprt59D3AB10, vcruntime1ACBD98B, ucrt7EF4EAC9. No /FORCE, stubs or SDK libraries.
All failed attempts and original objects remain. The libraries resolve symbols
before GC; success was conditional on absence of final runtime imports.

## Static binary evidence

| Artifact | Baseline | ThinLTO |
|---|---|---|
| Result |6AF5BCEE|1526A435|
| DLL, both3584bytes |4FFFF64A82BC1942D05A647D53A548711BB6C4632FA30203E1942EF5BBE83CB7|A12A64CF48C61179370DAFD47E674AEFE637736CF0D326A4CED9952F75F4E306|
| MAP |E08901D6|8144478F|
| Imports |none|one calloc import|
| Caller |99bytes,2calls,r6spill/reload|same except hook rel32|
| .text virtual bytes |296|248|

Both have entrypoint0, no TLS/delay imports/.CRT section, exactly five synthetic
exports and the four-entry address table. The public strong hook writes0x77;
the separately exported weak fallback writes0x11. Table slots resolve to the
caller, cross callee, strong public hook and weak fallback respectively.
The smaller ThinLTO text removes a duplicate standalone body; it does not shorten
the observed caller. Baseline locale bodies are absent; ThinLTO retains the import
without a callable locale body in final text. That still fails the strict gate.

Root machine-code-review.json:
1AF1A5A6B025385CC18F106DB576F3AF4B7210B1D43A502A4C5261DEC27A2350.
Pinned readobj/disassembly outputs live beside each DLL. Root verified artifact
hashes, normal jobs, PE directories, exports, table targets, hook constants and
caller comparison. Independent final comparison review PASS: the reviewer separately verified both
artifact sets, normal jobs, exact libraries, exports/table and unchanged caller;
ThinLTO remains rejected by the zero-import gate. Structural ast-grep scan of the
two synthetic C++ sources and git diff --check passed (existing LF/CRLF warnings).

## Existing-bitcode IR inspection

Root inspected the existing two ThinLTO objects as text with clang -cc1 -x ir
-emit-llvm -disable-llvm-passes; no source rebuild or runtime. Two normal owned
jobs preserved the object hashes. Receipt9110A1B78E904BCED3BE571EF0BE54C402E882D74D2DE3429E9F35C15C7DAD92,
private ir-inspection/. Clang emitted a target-triple override warning for both;
these outputs are inspection views, not replacements for original bitcode or a
target-exact transformation-equivalence proof.

Source context.h53 declares REX_WEAK_FUNC as weak,noinline, but the inspected
IR contains weak aliases and **no noinline attribute**. Therefore a proposed
"remove noinline" flag/edit has no demonstrated target in this witness. Caller
IR still calls the public weak cross alias and public hooked symbol; synthetic
bodies retain singlethread seq_cst publication/restoration fences (8caller-TU,
12callee-TU). The strong override has already absorbed its same-TU private body.
The input locale helper IR calls calloc, consistent with its provenance, but the
full linker/import-retention cause remains unresolved. This narrows subsequent
investigation to alias/import analysis and fenced boundaries, without claiming
that either one alone caused the final unchanged caller.

## Saved ThinLTO import boundary

A subsequent **link-only diagnostic**, using fresh identical copies of the two
existing bitcode objects and the same link recipe plus /lldsavetemps, finished
normally. Original artifacts remained unchanged; no source rebuild or DLL load.
Private thinlto-link-trace/result.json C787B147387628F2ECAFE8F6BC891ED4FD24138F3A716DA12C2C1D53C5EA197C.
Its entire248-byte .text matches the original ThinLTO DLL exactly.

Four saved post-import/post-optimization modules were read with LLVM passes
disabled and the exact observed target x86_64-pc-windows-msvc19.44.35228;
all four owned jobs normal, empty stderr. Receipt link-trace-ir/result.json
BEC32665FA5ED95355678A2118CDC03F3198D5A24E394F65ECEDE05E110BF577.
These supersede the earlier warning-bearing view for the saved link phases.

The caller module after both import and optimization has no imported cross
callee or winning strong-hook body. It still calls the public cross declaration
and public hooked alias, which locally names the weak fallback. The strong
override remains in the callee module; final link selects it correctly. This
establishes a missing cross-module-body opportunity at the observed import
boundary, rather than evidence that deleting source noinline will help.
The local weak fallback must never be substituted for the winning strong hook.

Exact LLVM20.1.8 source independently provides a relevant restriction:
[FunctionImport.cpp](https://github.com/llvm/llvm-project/blob/llvmorg-20.1.8/llvm/lib/Transforms/IPO/FunctionImport.cpp#L234)
rejects interposable linkage; [GlobalValue.h](https://github.com/llvm/llvm-project/blob/llvmorg-20.1.8/llvm/include/llvm/IR/GlobalValue.h#L426)
classifies WeakAny as interposable. Local copies35BBF6ED/86C1663D preserve the
exact source. This is a supported lead, **not an observed exact import-failure
reason**: saved import decisions and alias resolution may change linkage before
that check. Import remarks/decision attribution would be needed to establish it.
The preserved index and resolution text are available for such a bounded follow-up.
Independent saved-link/IR receipt and narrow attribution review PASS.
No blanket alias removal, direct fallback calls or full-game rewrite is admitted.

## Matched ordinary full-LTO control

To distinguish the ThinLTO import boundary from monolithic visibility, a final
two-TU control changed only -flto=thin to -flto=full and fresh labels/output paths.
No game bodies, aliases, fences, hooks, headers, optimization/FP flags or libraries
changed. Independent controller review passed; prepared440inputs/427dependencies
were root-verified against the exact ThinLTO job delta and actual file hashes.

Prepare3FD61DBCFF22F06F29F15CE037C3919B973A72DDB5C81AD687F7DE1E614683D6;
resolved-controllerFD7CD79A9DD6E46BF1B30ABBE10A6FB32A5DEDBEE142532E7E120A945A6022DD;
input7D6B7D4912A042FEBFADC9E876F0A753BCF933F118C2667BE3D3F042A00467CE.
Result832DEE1CE2636D96BC217967E78C36BBB9AD4AD33EBF65657BB582B651CA60AB:
two compiles and link normal, dependencies checked and inputs unchanged.
DLL605E7AB8674E22E523405D6990ACFD604913B68CEEF537B8C4C440A8D224BFEC (3584bytes);
MAPECBA77B7427EC32040EC8A31B97F1821C8952069D44377FFED151ED8C17C1291.
Root full-machine-code-review.json83EBD40A7E650C89977693254EB509B5BF8107CA911F885D0B37277DA2F34A5B.

Full LTO inlines the winning strong hook, retaining its0x77 write and scope
publication/restoration, but the public weak cross call and r6 spill/reload remain.
Caller122bytes versus99baseline/ThinLTO; code size is not a runtime-cost estimate.
Exact five exports and four table slots retain strong77/fallback11 separation.
Entrypoint/TLS/delay imports/.CRT remain absent, but calloc remains imported:
the strict zero-import gate still FAILS. No game build or DLL execution followed.
This demonstrates partial cross-TU optimization, not the desired weak-call/context
traffic elimination or a game-rate gain. Independent final binary review PASS:7artifacts/3normaljobs, sole calloc import,
exact exports/table and partial strong inlining independently confirmed.

## Next boundary

Retain ABF/AEE/U8/H100/all8 and default FSync. No new game-rate gain or promotion.
A future experiment needs a concrete hotspot and an optimization opportunity
that survives current generated-code/hook/fence constraints. Do not repeat this
unchanged synthetic recipe or spend a broad guest rebuild on LTO alone.

Weekly cutoff reached after this control:97%used/3%remaining. No further experiment
started. M7-016 remains open; no new game-rate gain, commit or promotion.
