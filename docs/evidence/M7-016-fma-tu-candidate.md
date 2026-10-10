# M7-016: isolated single-TU strict/FMA candidate

2026-09-06. Continues the accepted [numerical/state gate](M7-016-fma-runtime-matrix.md).
Windows and the five-arm Deck comparison are complete. Owner driving/audio
assessment is the active follow-up.

## Isolation and review

A private CMake project-include hook uses a deferred function to apply
`-ffp-model=strict -ffp-contract=off` to only generated/default/mcla_recomp.0.cpp.
The strict control adds `-mno-fma`; the hardware candidate adds `-mfma`.
The file contains185 static FMA expressions, including the previously
identified96-call function. This changes the entire translation unit.

Root CMake/presets, generated sources, SDK source/install and VERSION remain
unchanged. Fresh build trees use the existing Type0 SDK and exact baseline
dependency paths. Frozen inputs cover2587files/516,316,784bytes: project and
generated sources, SDK includes/configuration/shared app sources, explicitly
linked SDK libraries/DLLs and build tools. System headers/libraries remain the
existing installed toolchain, so this is not a fully hermetic build claim.

Eight incorrect configurations reject for their intended guards before the
hook applies options. The final comparison checks78 CMake command pairs and
72 baseline/strict/hardware actual Ninja commands, including dependency flags.
Only the selected source gains the three FP flags. Link settings match the
baseline except an output map requested in both arms. Ten generated command
files are hash-bound before/after each build.

Earlier unbuilt configurations remain retained: fresh dependency lookup chose
copied private libraries instead of the baseline's clean-install paths.
Explicit baseline package paths resolved that difference. A subsequent
CMake-versus-Ninja representation mismatch was resolved by comparing Ninja
commands with Ninja commands; dependency flags were retained.

Both77-step builds exit0 in approximately123seconds. Source and binary
reviews independently reproduce:

| Property | Strict control | Hardware candidate |
|---|---|---|
| Host SHA256 | 9C2663D280E925234FDD9F8F078FC58850B8107D79C6ECF5F89FE815A251E510 | B0C90E33FF4D10700002B00EC7E39CB41674CB122043A771830EAB6C2E306A5F |
| Known function range, RVA | [193ED0,19AC43) | [1939E0,199BCD) |
| Static imported FMA calls there | 96 | 0 |
| Hardware double FMA instructions there | 0 | 96 |
| Checked FMA/narrow/widen dependencies | 96 | 96 |

All665 DLL/symbol imports match the measured baseline. After normalizing only
COFF timestamp bytes4–7,71 of72 host objects are byte-identical; the selected TU
is the sole remaining difference. Both generated-integration gates pass:
67files/133,916,451bytes,65generated C++ sources and65built objects.
Root ast-grep scan and all three rule fixtures pass.

## Windows and package gate

Two private nine-file packages reuse eight files from the measured Type0
package. Only mcla.exe changes. Manifest hashes:

- Strict: E6508429CBC224BFADCEC744C6DDC651872D6FCB964634CAEDB554091E484FBB
- Hardware: 3C62711010D8048647F8DD774832858F883CE9EE2B4042E6CCCADEF9E4CD6DF3

Fresh v2 Windows Vulkan controls load the complete accepted four-file seed,
enter saved gameplay, PAUSE and Settings, and verify seven required loaded
images. Primary inspected all six accepted phase frames: the saved Eclipse,
HUD and menus render. Phone animation and music-label timing vary; this is
not pixel-level parity or physical controller/audio acceptance.

Both titles exit0; the separately tracked crash helpers are observed exited.
Helper exit codes were not recorded. No forced cleanup, survivors or inspection
errors occur. Source and both test profiles retain the full3BBC44D0... hash.
All18 copied binaries and eight BMP headers/hashes verify.

- Strict result: 3C09A676A203CC68604DFB2502851DCA2E8802BF81F4F330543562986C6F75F6
- Hardware result: C17955E0391F2B4B55929D90823EB5FB34E8D686DEEA160EFB78728364987B52
- v2 helper: 81A81307D6D6D06EFA68D99C9AFC3597DBA8670AEFC67EE14C073800B45CFE6F

The original strict attempt remains FAILED because its observer incorrectly
required TracyClient.dll. Normal and delay imports confirm these Release
binaries do not import Tracy, although CMake stages the file. v2 records its
absence and validates it if loaded. The old attempt's three recorded images
do not prove its four unrecorded C++ images; fresh v2 supplies that evidence.

## Completed Deck comparison

The owner temporarily took Deck offline, then announced its return. Paired SSH
and the exact CEC72E71... UCRT were rechecked. Eight outgoing files contain only
the two reviewed host executables and small manifests/helpers; no assets,
saves or full bundle were transferred. The other eight package files were
copied on Deck from the retained baseline. All work remains outside the entire
Syncthing root; the normal published bundle is still0.16.0.1.

The five arms are old/strict/hardware/strict/old, with fixed Type0 ON, fast
readback, Proton10, full historical seed/cache and a fresh private prefix per
arm. The helper pins UCRT source identity and12 mapped images in the two
startup/pause-adjacent snapshots. It has no profiler and captures a20-second
paused-scene counter interval.

Helper SHA256:
346D0705182285EFB2261A62D1CE585A598B3A88C59559486B5E214AF26395A0.

| Arm | Outputs in approximately20seconds | Outputs/s | Draws/output |
|---|---:|---:|---:|
| Old | 290 | 14.50 | 5341.20 |
| Strict | 295 | 14.75 | 5209.09 |
| Hardware | 293 | 14.65 | 5388.41 |
| Strict repeat | 294 | 14.70 | 5203.19 |
| Old repeat | 295 | 14.75 | 5240.63 |

Hardware is0.678%/0.340% below the adjacent strict controls and inside the
old endpoints' range. It performs3.442%/3.560% more draws/output. Draw
time/output is essentially flat; binding time rises broadly with binding work.
There is no demonstrated paused cadence gain or defensible regression
attribution. Timers overlap; none establishes an exclusive CPU budget or a
new bottleneck by subtraction from wall time.

All five arms exit0 without force, survivors, cleanup errors or source drift.
Primary and independent reviewers verify123files/119,966,644bytes, all raw
deltas, normalized commands and both12-image snapshots per arm. All twenty
returned seed-file copies match the four-file baseline. Primary inspected
all15 gameplay/pause-before/pause-after frames: the same saved Eclipse/HUD
and PAUSE are visible, with traffic and sky variation between runs.

- Archive: FA2FB2241498D7F41B708DFC8D0F27C8EE1C5122D18B6DB20ED07292FFADE09A
- Inventory: 3C1E0F17356430B7DF8847CBC5949C48B148C1F3975DCB4ACBC7FC81B843DC8E
- Analysis: E8162D995F55C50AAE8E2AC5D6B666077DA12BE1F99A1A7A634CCC2F0B15E1E3
- Analyzer: D0241E6CE807FACC7DBA39B64BBD6AE415AE87F6A95C9C5C738E80550FB9E889
- Collector: D81DD92B10B649ED5CAAAA4D39E93CB91492FCA89276A0A65C432806FF0368F3

These are paused-scene outputs per second. The known FMA caller's activity
while paused remains unproven, so this null result does not rule out moving
or audio benefit.

## Completed owner driving session

The owner drove the exact hardware B0C90E33... package under the same pinned
Proton10, without profiling or automated controls, then closed the window.
Session97556 is closed: title136788/birth164055126, wrapper136610,
elapsed223.221525553seconds, exit0, no force, survivors, cleanup errors or
source drift. All12 startup images match, including exact CEC72E71... UCRT.
Here exit0 is the existing SDK window-close path using `std::_Exit(0)`, not
proof of graceful teardown. The log records Window closing, a queued
`unexpected-guest-exit` snapshot, then the SDK hard-exit marker. Source review
shows the guest-exit callback runs during intentional shutdown too; the same
sequence occurs in the older owner session. The returned partial diagnostic
is preserved. No native crash dump/package is present in the24-file return;
the diagnostic label alone does not establish a crash.
The complete returned four-file profile remains the accepted seed; this is
not a new persisted progression marker or successor-save acceptance.

The owner's assessment is substantially better driving responsiveness:
approximately15–20 FPS by feel, possibly25–30 while parked, and approximately15
in menus. There was no FPS monitor. The owner explicitly says stable30 is
not achieved. Their audio observation was: "звук НЕ тріщав-лагав весь час".
This supports improvement over the earlier continuous problem, not a claim
that every audio artifact disappeared. These observations are subjective;
they must not be merged with measured paused outputs/s or treated as a
strict-versus-hardware causal comparison. The recalled older menu rate also
refers to an uncontrolled earlier experience.

Primary and independent review verified all24 returned files/8,410,399bytes against the exact archive
and inventory. The independently reviewed collector retains helper,
preparation, launch, result, full profile and startup-image linkage.

- Helper: E62E3F728CADBCB589986F4F706F61F1309B9BA4E07743A21DDF85BDB28418AD
- Preparation: E282210C8430A6FE4DB00284664B0D4E4297FAD9C6A4207D09F42F74CA6CF181
- Launch: 45F6DD01F2C0312B11DA3FED4E3CB85D881532EFB76D50E54E527329AE28BAE1
- Result: B7EEACA8FCDFB7656D5C6E66F941CA79E9932C57C3B01A2CC4A1E3CA067DDBB1
- Collector: EFF4E71C043224ADDCF169A3E93259032FB86105A466BA56310187C13F5D3961
- Archive: F1BD47BB82445EF43C10ABC4177A798A59D3E5D1C09C11BEBC1F82548F6EF164
- Inventory: 3442F6844339343075A246E0C458941F81C049967A08628BF7CA8CCB654E67D1

The owner is now AFK; no further immediate physical test is needed. The next
autonomous diagnostic adds a default-off neutral unpaused window before the
existing pause probe, with separate counters and phase frames. Its edited
source and future binaries are distinct from the tested B0C90E33... artifact.
No version bump, task closure, SDK promotion, commit or push has occurred;
M7-016/KI-027 remains open.
