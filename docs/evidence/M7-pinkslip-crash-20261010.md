# Championship followup Pinkslip tutorial crash

2026-10-10. The owner reports winning a championship, receiving the Pinkslip
Races tutorial phone call, then crashing. The preserved runtime journal ends
with an invalid/unregistered call to guest address `0x822B88B8`. The original
XEX contains a legitimate callback at that address; the crashing host omits it.
A narrow function-boundary correction passes fresh native build, corrected
lookup and owner physical regression on DCDE2F12. The owner confirms that the
championship win persisted, the same tutorial call repeated after garage to
free roam and completed without the crash, and a subsequent Pinkslip race was
won. Accept the specific KI-031 crash repair; this is not a full M7 task closure.

The owner subsequently identifies the championship as the game's Tournament
category; its displayed route name/native ID remain unverified. Subsequent
owner confirmation accepts fresh-process persistence of the Pinkslip win/reward.
The Pinkslip win is a distinct owner
observation, not inferred from the tutorial or runtime log.

## Preserved crash and save

Source is `private/bundles/M7-Windows-Stock-F10-WGC-20261004-1`, crash package
`diagnostics-stock/crash/crash-20261010T074039Z-41108`. Host SHA256 is
`3380E9E8880F66D154CA7B57DC0E11175112DF59378CFB61B6BB229F2B81E0F0`;
runtime SHA256 is
`570A88C7FAB538312BC35091183EE890A10DF71D26906C24062067B779663EC6`.

The one-time collector
`private/evidence/M7-012/collect-pinkslip-crash-20261010.ps1` preserved 53 files /
38,870,751 bytes plus receipt under `pinkslip-call-crash-20261010-1` in the same
evidence directory. It verified copied/source hashes and the complete profile
before/after. Review caught that the copied collector retained the older pilot's
mutex rather than the WGC pilot's mutex. A subsequent read-only check held the
actual WGC launcher mutex `46B78699...`, confirmed no active game, and rehashed
all 53 source/copy files plus the source, copied and archived complete profiles:
all match. The original collector is retained as executed, not silently corrected.
New staging must derive the mutex from its exact source root.
The full profile is archived without fabricated portable ancestry:

- Profile `1ED6760789A66FFA6770E7E57FDC3AC02AC6214E62DA6E6574E16A1C7DF629AD`.
- Main save `4B67097512765B7215560DBC80C894D602128BC1FF6ABA0CF8FE579320667D0B`.
- Header `1D717009B10A84525F468AD826A7DC016B744FA78274865A7DAB8A1BB65A4957`.

All four manifest artifacts, four save files, package topology and reviewed
minidump header/flags pass the existing diagnostic verifier functions. Dump
SHA256 is `C4978E9E3D56BA8FF30B341D51C95E84A3D5F9146C1C15F27D41CB45E380AF6B`.
Journal SHA256 is
`87010788DCD81721A81E1EE85F6244276E752433FBC03F6155D1E124D12333FF`.
No game launch, save write, desktop capture or upload was used for collection.

## Failure and deterministic lookup reproduction

Local CDB shows the abort path through `ResolveIndirectFunction`; the first
host return into generated code is RVA `0x1CFA740`. Its code contains the
indirect call with guest LR `0x82555CCC`, consistent with the script interpreter
call at `0x82555CC8`. The minimal dump does not retain the suspected PPCContext
heap contents; do not invent its register values or a complete script stack.

`tests/mcla_guest_dispatch_test.cpp` links the original generated objects and
uses the real SDK Memory/FunctionDispatcher plus generated `REX_LOOKUP_FUNC`.
It replays production registration, never invokes a guest callback, and does
not initialize the application, graphics or profiles. The private build command
`private/evidence/M7-012/build-dispatch-probe.ps1 -Run` and a direct second run
both returned 1: all 30,034 entries registered, A8/C8 positive controls passed,
but B8 was absent from mappings, dispatcher and memory slot. This reproduces
the missing lookup, not the entire gameplay sequence.

The read-only `private/evidence/M7-012/audit-pe-registry.py` independently found
the complete 30,034-entry table in the hash-pinned crashing PE at file offset
`0x3109FB0`; all guest keys match the pre-fix source, with B8 absent. This rules
out a source-only mismatch as the explanation for that omission.

## Original instruction proof and correction

Fresh XEX SHA256:
`C386F4001FA569E6AD4B982F441F67412F00B3F47C166134555CD4B59854A432`.
The migrated Ghidra project refused access because its recorded owner is the
old machine account. It was not modified. A separate `pinkslip-20261010` project
imports the same XEX; subsequent audits are read-only/no-analysis. Raw exports
remain private, including loader diagnostics. The loader reports guest image
base `0x82000000`; Ghidra's generic program image-base field is zero, so it is
not used as a guest-address subtraction base.

`address-audit-20261010.tsv` proves an independent 16-byte wrapper at B8:
the preceding A8 wrapper ends in an unconditional tail branch at B4; B8 moves
the original r3 argument to r4, materializes r3=`0x822B7888`, and tail-branches
to `0x822C97B0` at C4. C8 starts different instructions. Both downstream entries
already exist. This is not an alias to A8 or an interior fallthrough entry.

`callback-audit-20261010.tsv` independently confirms original instructions at
`0x822B8F7C/84/8C` construct `0x822B88B8` and pass it in r4 to registration
routine `0x82554798`, alongside the neighboring A8 callback. Generated code
preserves and forwards that value into the callback table. This proves a valid
callable target, not the exact timing of the owner's phone-call event.

Independent boundary review approved only `[0x822B88B8,0x822B88C8)`.
`config/mcla_functions.toml` adds that exact explicit function using the existing
SDK `functions`/`end` schema. ReXGlue 0.19.0.0 non-force codegen exited 0.
The complete pre-fix corpus is retained under `generated-before-20261010`.
The generated diff is only one 13-line wrapper, one declaration and one entry
in each registration table; all other files and neighboring bodies are unchanged.
No SDK patch, opcode substitution, empty stub, broad gap or renderer change.

## Native and packaging verification

Fresh Release build `out/build/win-amd64-pinkslip-20261010` completed all 82
actions with exit 0. The same lookup test, relinked to its real objects via
`build-dispatch-probe.ps1 -Run -Repaired`, passes twice with 30,035 registrations
and no null, duplicate or rejected entries. The original probe still fails at
B8 as a negative control. No guest function was invoked in these tests.

Second non-force regeneration is byte-identical across all 67 files. The
manifest exporter initially omitted its byte sum because Measure-Object was
applied to ordered dictionaries; the generated corpus itself was unchanged.
After correcting the metadata sum, existing integration verification passes:
67 files / 133,916,828 bytes, 65 C++ sources and 65 compiled guest objects, zero
tracked generated files. Manifest SHA256 is
`34505E790324D8857BE10D41FDB3ABE841BE02A8EA9DFAB770817F43DB70B3FC`.
Scoped ast-grep scan and all three rule fixtures pass.

Candidate: `private/bundles/M7-Windows-Stock-Pinkslip-20261010-1`.

- Host `DCDE2F12014C29277BB76B919ED539119C88BC6D49B816F12F28290EF97E2F70`.
- Helper `D4894275DC0311B905C8E666E8F585FC2D81883A638194537674C7208AA0C98D`;
  this exact pin appears once in the compiled host, not merely in its header.
- Receipt `930BD14DEEDF56B4A402AAD55287CE19948D85221BDE91F7186FEFC9BD4EBF3F`.
- Independent full profile `1ED6760789A66FFA6770E7E57FDC3AC02AC6214E62DA6E6574E16A1C7DF629AD`.

One-time `prepare-pinkslip-stock-20261010.ps1` holds the actual source-derived
WGC launcher mutex, verifies source/archive before and after, and retains any
failed destination instead of deleting/retrying. Its 30 unique receipt entries
comprise 15 immutable game hardlinks and 15 independently copied/configured
non-game files; all non-game files have one hardlink. Source helper is verified
then replaced only in the new destination, with one new receipt entry. Runtime
DLLs match the freshly linked host's build; the previously tested crash helper
is preserved. At staging, new cache/log/diagnostic/metadata/update roots were empty.

`Run Pinkslip STOCK Candidate.cmd` calls the new hash-pinned wrapper. Actual
CheckOnly passes on PowerShell 7 and Windows PowerShell 5; both expose contained
five-root CLI arguments and select the RTX 5060 by name. The pure config suite
passes 23 checks on each shell. Settings remain stock timing, 2x, VSync,
fullscreen. Staging did not launch a game or overwrite an old pilot/save; the normal
portable pointer remains the prior 0.16.0.1 package. Prior F10 physical evidence
is not silently reclassified as physical evidence for this new host.

Independent final read-only review rehashed host/helper/receipt and all 15
non-game inputs, compared all three full profiles, normalized the actual config,
reviewed the four-file generated delta and wrapper/CLI roots, and approved an
owner-started launch only. Primary accepts that bounded readiness result.

## Accepted owner return

Owner report on Oct10 explicitly confirms the championship victory survived
loading the new candidate. The Pinkslip tutorial phone call actually repeated
after garage to free roam, completed successfully, and the owner then entered
and won a Pinkslip race. This closes the specific physical KI-031 crash gate;
no replay of the championship or tutorial is required merely for more evidence.

The exact host/helper/receipt remain DCDE2F12 / D4894275 / 930BD14D. One logged
session runs 11:35:19–12:02:26 Kyiv (PID21636), with RTX5060 D3D12, 30,035
mappings and isolated roots. The complete log has no invalid-function fatal,
PPC_UNIMPLEMENTED or device-loss markers. Its end records window closing and
the existing hard-exit path, not independently verified exit code zero or
graceful diagnostic teardown. An empty shutdown `.partial` directory remains;
there is no new crash package or accepted F10 capture in this return.

With no running MCLA and the exact candidate launcher mutex held,
`private/evidence/M7-012/collect-pinkslip-return-20261010.ps1` preserved eight
files / 3,846,667 bytes plus receipt under `pinkslip-owner-return-20261010-1`.
All source/copy hashes and full profiles were checked; the complete returned
profile is independently archived under `private/save-archive/M7-009`:

- Profile `E2A7822E75ADC2AF594645B5E3D125099774914603BE848897FDE36727A95FED`.
- Main save `D3FDCEF9A08682657D7D122AD31C34604602E85FD08A1AF19F55BC9635093BB9`.
- Header `1D717009B10A84525F468AD826A7DC016B744FA78274865A7DAB8A1BB65A4957`.
- Receipt `2801D844B12CF50387FB83E3A06A38671D58E6145CE0AFFA9CA1567D1FDE5FF3`.
- Runtime log `E4BE8384503CCC9524C5C48943E8C679D2C20D90515ECE978C7CC8047511F887`.

Independent read-only review accepted this exact crash regression and its
limits; primary accepts the result. The changed on-disk save alone did not prove
that the new Pinkslip win survives reload; the subsequent confirmation below
supplies that bounded owner evidence.
No reward amount, won vehicle identity, course or opponent is inferred. Keep
progression on this candidate branch; all older branches and archives remain.
M7 stays 0/16 closed; no game launch by the agent, version bump, normal
publication, commit or push occurred during return collection.

## Same candidate reload confirmed

Owner replied "все ок" to the explicit request to restart the same Pinkslip CMD
and check persistence of the new win and reward. Accept that bounded confirmation.
The same DCDE2F12 host has a second logged launch at 12:34:37 Kyiv, RTX5060, and
window-close/hard-exit markers at 12:39:32. No invalid-function fatal or device-loss
marker appears. This does not prove graceful diagnostic teardown. A nonfatal
invalid texture-fetch warning remains a rendering observation, not a new
owner-reported blocker or permission to enable the suggested bypass.

After exit, the source-derived launcher mutex and no-running-game guard protected
`collect-pinkslip-reload-20261010.ps1`. The separate cumulative return
`private/evidence/M7-012/pinkslip-reload-return-20261010-1` preserves nine files /
3,870,185 bytes plus receipt; source/copy hashes and full profiles match. This is
one additional session, not two additional runs inferred from a cumulative log.

- Receipt `CACD2203F5A567CF06521D66FAAB55D203A6F003CA2F2052F67D47DD3BE7AE66`.
- Full archived profile `10D80A049A6F6B24FF445DBEA9B65D4DE4978E1FEE7FF88CE032D689138B2880`.
- Main save `C963D60BBD87254BA962B2974531514576C51BC736BBE92E0EB66240FCB9936C`.
- Header remains `1D717009B10A84525F468AD826A7DC016B744FA78274865A7DAB8A1BB65A4957`.

The specific repaired tutorial, championship persistence, Pinkslip win and
same-candidate win/reward reload are accepted. No numeric reward or won-car
identity is inferred. Do not request another replay of these accepted checks.
Other category/lifecycle gates keep the full M7 tasks open; no normal publication
or promotion of this profile into another branch occurred.

## Task closure review

October10 final review closes M7-012 for the reproduced blocker on the claimed
Tournament/Pinkslip route. The accepted physical callback/tutorial/win/reload
regression above is sufficient; unknown story routes and open S2 defects remain
separate. Latest renamed-profile host82A69B5E also boots with30035 mappings and
the owner's saved progress, but does not replace the targeted DCDE2F12 regression.
F10 was not pressed in that later smoke and remains an M7-016 gate.

Final integration audit found the old analysis-config verifier omitted the new
B8 tuple and rejected the six already-committed Race Back observation hooks.
The real configuration failed before repair. The verifier now requires all35
exact function tuples and all six exact existing hook blocks; no arbitrary
section skipping or broader callable boundary was introduced. Both PS5.1 and
PS7 pass one positive and16 negative cases, including missing/overlapping/
truncated callback and missing/changed/extra hook fixtures. Scoped ast-grep is
clean and all3 rule tests pass. Real repaired SDK dispatch passes30035 unique
registrations and all three B8/A8/C8 mapping, dispatcher and slot checks.
Generated integration revalidation passes67 files/65 sources/65 objects and
the exact DCDE2F12 host hash. No guest was launched for these final checks.

Independent narrow code/config/test review: GO. The dispatch test's build recipe
is still the private artifact-bound runner described above, not a public CMake
target or an advertised automatic clean-checkout test. The source version is
0.18.0.1; accepted binaries retain their original identities and versions.
Only this task closes in its commit. No SDK renderer experiments, generated game
code, saves or diagnostic payloads belong in the commit; M7 remains open.
