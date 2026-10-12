# Accepted 32bpp readback packing correction

October 12, 2026. The bounded SDK packing defect behind the reproduced coarse
vinyl is corrected and accepted on l12. The owner reports markedly better vinyl
in the garage and while driving, and confirms name/progress after a fresh
restart. Additional sharpness relative to Vinyl Editor is explicitly deferred.
This accepts the correction, not editor parity, every rendering defect or M8.

## Cause and correction

The scaled 32bpp writers expand groups of four dwords. The old downscale reader
treated each physical tiled dword as if its scaled samples were independently
contiguous. The correction inverts the uint4 group/lane layout for32bpp only.
Other format paths are unchanged. The actual compiled old shader fails the
synthetic layout oracle; the corrected compiled shader passes it.

The two-file [patch](../../patches/rexglue/resolve-downscale-32bpp.patch) and
[reconstruction manifest](../../patches/rexglue/resolve-downscale-32bpp.json)
preserve HLSL and its compiled SDK header. This is not title-generated code.
The original SDK BSD notices remain applicable. The manifest records the exact
base, before/after hashes, LF-to-CRLF header normalization and bytecode compiler
reproduction; the original historical full compiler invocation is not invented.
Narrow Git attributes preserve exact patch/manifest LF bytes on Windows checkout.

The SDK pin remains5dd58f894bf34b9ea85e59d3903c7f0e8044d853 (v0.20.0.0).
A normal main-project build therefore still uses that pinned source unless the
patch is explicitly reconstructed and built in a separate clean SDK checkout.
No default install, submodule content or existing package is overwritten.

## Verification

Fresh reconstruction from the exact Git base matches both authoritative source
hashes, including the required CRLF generated header. Fresh Release builds use
Clang20.1.8 and Windows SDK26100. The expanded fixture covers all49 scale-axis
pairs1..7, corner/center selection, two tiles and SRV offsets0/16/4096.

| Check | Result |
| --- | --- |
| Baseline actual shader, expanded32bpp oracle | 4,116,542 checks;353,280 failures; expected exit1 |
| Candidate CPU mapping | 3,213,374 checks;0failures |
| Candidate actual WARP shader | 4,116,542 checks;0failures;294dispatches |
| Other-format baseline/candidate comparison | 3,637,310 checks;0failures;108pairs |
| Exact-build capability policy | 168 checks;0failures |

The isolated launcher source set (without pending export/UI changes) also builds
and passes controller286/0, with one Windows1314 symlink SKIP, plus production
no-game preflight20+16 against exact FED35/game95DA2709. These checks preserve
their temporary profile inventories; neither launches the real game. AST scan
and all3rule fixtures pass. Independent source/index review gates the commit.

Preserved controller TEMP313bd33b7afc2a7acd7baee00617323f; missing-support
TEMPbe8d52f7fac195eb1f801d2493af656d; complete TEMP0aee6a41e62324c6dacf002e1307f0d3.

Other-format equivalence does not establish correctness of8/16/64bpp formats.
The reader fixture uses a reviewed writer-layout oracle, not execution of the
actual writer or validation of CPU resolve extent/origin handling. No game,
window, owner profile or hardware-GPU fallback is involved in these tests.
Fresh local logs are under `private/ki033-accepted-20261012`.
The committed standalone CMake recipe passes all3 CTest cases. A separate
private negative fixture changes a previously admitted header after configure;
the next incremental build reconfigures and rejects its hash before compilation.
Fresh Windows checkout preserves the patch and manifest byte-for-byte. Only
the patch's hash-significant compiler/context trailing spaces are exempted from
trailing-space lint; source and documentation checks remain enabled.

Earlier clean SDK suites passed401cases/3364assertions across41isolated suites
(four explicit skips) and PPC1462cases/5753assertions. The clean source inventory
contained exactly the two shader changes across20113files,25pinned dependencies
and15 documented link materializations. Four normal-host720/1080/1440/2160
garage/editor scene pairs passed independent review without changing MAIN.
The failed2160 RenderDoc allocation attempt remains failed and was not rerun.

The observed corrected actual car draw used a1024 BC3 atlas: contour RMS
0.127–0.148 versus the old0.452–0.461. The editor used2048 RGBA8. Zero guest
fetch bias at one reviewed draw does not measure hardware-selected mip or prove
the cause of all residual softness. No further quality override is included.

## Reproduce the synthetic regression

Use two private, separate shader trees reconstructed from the manifest. Never
patch an unrelated dirty SDK checkout. Supply the exact absolute baseline and
candidate `resolve_downscale_cs.h` paths, and a new private build directory.
From a process-local x64 Developer PowerShell with CMake/Ninja and Clang:

```powershell
cmake -S tests/resolve-downscale -B <private-build> -G Ninja `
  -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_COMPILER=clang++ `
  -DMCLA_BASELINE_HEADER=<absolute-base-header> `
  -DMCLA_CANDIDATE_HEADER=<absolute-candidate-header>
cmake --build <private-build>
ctest --test-dir <private-build> --output-on-failure
& <private-build>/baseline_test.exe --run-warp
# Require exit1 and the expected mismatch totals above; exit2 is setup failure.
```

The standalone project checks both header hashes and reconfigures if a header
or manifest changes. It is deliberately separate from the main game CMake build.
CTest runs the three positive checks serially, each with a60-second timeout.
The baseline negative is explicit because treating every nonzero exit as an
expected failure would wrongly accept a setup error. Fixture bounds are16MiB,
294dispatches, a55-second internal deadline and a5-second fence timeout.

## Artifact and owner acceptance

The accepted build is FED35A361F4A4A10BB03A6BC45072C82924085A0D3ADB8A3298243066DB4A850.
l12 launcher is DB0D84A5C7A2F61A27EA1C68859420F54E95BC4EC23FA4BA46F30AB4A297B4C8.
The capability change admits that exact build for1080/2160 while retaining old
9C6BEA71 admission for historical records and rollback. Mutated/other hashes,
VSync-off and unvalidated60 stay rejected. Policy does not certify world quality
or measured FPS. No export/observer feature is included in this change.

The FED35 drive completed with a real outdoor F10. The subsequent fresh-process
reload completes with exit0;24 rename/reload snapshot files and the intervening
completed rename verify the save lineage. The owner confirms persisted name
and progress. [Reload and support-folder evidence](M8-profile-support-repair-20261012.md).

KI-033's packing correction is accepted; residual quality is deferred without
PASS. KI-032 building signs, other baseline defects, stock/60 validation and
volunteer workflow gates remain open. No public SDK release or M8 closure follows.
