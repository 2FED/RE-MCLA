# Windows render targets

October 11, 2026. M8-004 remains open. Four isolated stock-timing startup probes
produced the requested guest-output dimensions. New launcher l7 exposes those
targets. Subsequent owner return accepts target/HUD/window/restart functionality and four
F10 images verify;2160p sign corruption prevents full world-rendering acceptance.
See [owner return and new defects](M8-l7-owner-return-20261011.md). The following
sections retain the implementation-stage evidence and its original limits.
Output dimensions alone do not establish internal quality, performance or correct
rendering of every pass. F10 window dimensions are not render-target proof.

## Source findings

The generated guest `sub_8217AC30` in `generated/default/mcla_recomp.2.cpp`
caps automatic dimensions above 921600 pixels to 1280x720. Explicit width/height
options can bypass this branch, but their retail command-line parsing path is
not proven operational. The option registration is present; the adjacent
initialization function is a return stub. No guest patch or SDK edit was made.
Xbox video-output capability therefore does not establish this game's native
internal target. SDK draw-resolution scaling uses integers, not a supported 1.5x.

The bounded 1080p route uses 960x540 guest mode and 2x draw scaling. SDK video-mode
metadata may classify that guest mode as non-HD; HUD, media or other low-resolution
branches remain a specific acceptance risk. Direct guest 1920x1080 remains a
possible separate implementation, not a working existing option.

## Startup evidence

All probes used RTX 5060 Laptop adapter 1, stock 60 Hz video-mode declaration,
VSync on, a 1280x720 window and fresh empty temporary user/cache/metadata roots.
The video-mode declaration is not a 60 FPS claim. No input injection, owner-save
import or desktop screenshot occurred. Each owned process closed through WM_CLOSE,
exited normally with code 0, needed no forced cleanup and preserved its sentinel.
The host's normal lease check may update its separate local lease cache.

| Target | Guest mode | Draw scale | Observed guest output | Temporary run suffix |
| --- | --- | --- | --- | --- |
| 720p | 1280x720 | 1 | 1280x720 | 12af6bfa5a79f1d34de20efedb1fa81f |
| 1080p | 960x540 | 2 | 1920x1080 | 69529e466c7c265a762bf4043be87870 |
| 1440p | 1280x720 | 2 | 2560x1440 | 187fdd475156cb07c6e1ae7df8f02b00 |
| 2160p | 1280x720 | 3 | 3840x2160 | ef0f34173a99094bc239ced0f038fb94 |

Evidence roots are `%LOCALAPPDATA%/Temp/mcla-render-target-<suffix>`.
Each retains result.txt, mcla.log and user/mcla-first-frame.bmp. The primary
visually inspected all four legal-text images. BMP dimensions, exact payload size
and the host's matching nontrivial-frame marker agree. These runs are consumed;
do not automatically repeat them.

BMP SHA256 in table order:

- 5F55F7CEAD3ACE323517F1A745238FFBA0A847D4901529086AA227116D1038EA
- 4C1E9BCBE235EC50775F73919698CDB91931AA925C5EC094D72587AA0920A1E7
- 6DD3F7649A3DD3D590B53548DFD52126A88362018A41589831A7D6FD196305C3
- 38B6C04CC9A1DC1BDF444A65EFBD7369FBEB8C180AF9E23ABCE6AD161A616B85

Passive probe executable SHA256:
7EACA596E1D8E1D1A1C03E8F4EE4C828F9FEB536AE5DD8607F8BD901C06C99D8.
Its strict parser/header/process guards pass 88 self-tests. An initial 720p
attempt stopped before temporary-tree creation or launch because the TEMP path
had a trailing separator. The helper now removes only that empty final component;
root guards were not relaxed. The accepted four runs used the corrected helper.

## Launcher implementation and tests

The shared policy holds explicit target IDs and guest-mode/scale tuples. Both
read-only readiness and real Start require the full tested build fingerprint
for 1080p/2160p; unknown targets or other builds fail closed. The controller sends
video-mode dimensions and both draw-scale axes explicitly and records target,
guest mode and scale in each session receipt. Legacy 720p/1440p admission remains
unchanged. Invalid UI selections cannot silently become 720p or gain readiness.

V1 preferences decode scale 1/2 as 720p/1440p. Saving either old target produces
the original V1 byte format; saving 1080p/2160p uses V2. Loading never rewrites
preferences. Atomic previous-settings backups remain unchanged. Old launchers
cannot open V2: switch to 720p/1440p and save in l7 before returning to l2-l6.
Do not delete preferences or reimport saves as a workaround.

Fresh native tests: preferences 1064/0, store 125/0 with one Windows 1314 symlink
SKIP, UI 902/0, controller 161/0, probe 88/0, staging 63/0. Production worker-thread
preflight passed 16/0: all four targets accepted and unknown 999 rejected, with
unchanged synthetic profile/settings inventory and no host/session launch.
AST scan and all three rule fixtures pass; diff whitespace check passes.
Independent source review: GO for experimental staging, not task closure.
Positive fake-child controller arguments cover 1440p; the other startup probes
construct their arguments independently. Source review checked the production
mapping, but this is not an end-to-end gameplay matrix.

Retained fixtures under TEMP:

- MclaStoreTest-89b2d84cee927336efc8eb0ae475f1d4
- mcla-controller-test-62dee0f39fbdadbbbefb33c4b5cd7eca
- mcla-host-probe-f8a7bd14b9cc7c65337d44507e2847bd
- MclaStagingTest-dcba46072a744584b7330d1aa492209d

Three hidden 120 DPI page renders under private/evidence/m8-ui-l7-*.png were
inspected: 1125x875, no outer scrollbars or clipped labels. Hidden WM_PRINT omits
native edit/combo text, so native value assertions do not replace visible testing.

## Exact candidate

`private/l7/MCLA-Launcher.exe`:
62A0A3AB3839D3273AC1872B54E24CE4A966D259FAE8A45468D87D49C0ADDE18.
All 17 staged receipt files match their recorded hashes and sizes.

Unchanged host/build/game identities:

- Host: 9195117A71A69F42B505838447C350EB8B0A6A2CC9FA14C535DCB0630240AAEE
- Build manifest: 9C6BEA718046F1B22D7FB9C4BEB8E03CC52DD3F466CCFB926CC585059F580BAA
- Game: 95DA270942B4BDDBC8DC5EB45AA742AC6BE191D74E430022070D9B9B5AF25048
- Ticket: 6C2FA6EAE358A2167ADA346B8E424BFC179957F072EC500CA800CFB5F360325A

l6 remains A665EB7D12B46C70C5F8AD8E089E477829F26092E9D4662CE1A2731B5108763E.
No game-host rebuild, personal-save/settings write, reimport, ticket/key reissue
or external distribution occurred. Owner functionality acceptance is now recorded
in the [l7 return](M8-l7-owner-return-20261011.md). Shared admission/preferences/UI
source is included with [M8-003 at0.20.3.0](M8-capabilities-20261011.md);
M8-004 remains open because KI-032 fails world-rendering correctness. The owner
check is consumed, not a request to repeat accepted tests.
