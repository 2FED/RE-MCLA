# Isolated rendering inspection

October 11, 2026. M8-004 remains open for KI-032 building signs; M8-017 also
tracks KI-033 coarse vehicle vinyl. A bounded native helper now loads a copied
historical MAIN snapshot and accepts guest-local navigation requests. This is
inspection tooling, not a symptom-specific regression test or rendering fix.

## Isolation and identity

`tests/mcla_seeded_render_probe_win.cpp` reuses production build/game verification,
signed lease admission, guarded directories, filtered child environment and an
owned non-breakaway process job. It admits only the historical session
`a77101732ba666eab05f5a123db5949b/before-user` and its pinned complete receipt
`D682CB13F81E3035F0EFBB0B894AECDFB7093410A473045D15AEA4FD6A4B1665`.
The game receives a fresh TEMP copy, never the live MAIN or archived source.
All six seed files are copied and checked against the receipt. A second complete
source copy after process cleanup checks source membership and bytes again.

Runtime remains host9195117A, build9C6BEA71 and game95DA2709 from the
[accepted l7 evidence](M8-l7-owner-return-20261011.md). No host/SDK change is
required. The inspection uses stock timing, VSync, named RTX5060, a1280x720
window and requested3840x2160 guest output. A managed sentinel suppresses
inherited persistent configuration. There is no desktop capture or Windows
keyboard injection. Only the owned host's existing guest-input protocol is used.

The helper requests normal close after300 seconds or an explicit close file;
after10 seconds it can clean up only its owned job. Evidence is retained.
Success means inspection closure, not gameplay, capture freshness, FPS or visual
correctness. Old garage protocol phase names do not prove purchases or states.

## First manual inspection

TEMP evidence root: `mcla-seeded-inspection-1899307cffbb3cc642bab3ee2ba23b1a`.
Helper SHA256 was
`F063A0C6A4F3C6789D4AB1C6E4CCA3F493897774229DBB1FF3D0CAB456494AFC`.
Native self-tests passed69 seeded-policy and88 passive-policy checks. Independent
source safety review admitted the bounded run.

Primary inspected the actual guest frames before each new navigation step:
title, loaded parked Mazda, pause menu, garage, customization and vinyl access
purchase prompt. Nine guest commands were acknowledged, including release.
The garage view reproduces visibly coarse vinyl. The displayed world signs in
the initial parked view are readable; this is a different view from the owner's
Selma/Highland defect and does not exclude KI-032.

The300-second limit closed the game normally, exit0, without forced job cleanup.
The complete source and managed sentinel were unchanged. Nine independently
hashed owner artifacts also remain unchanged: MAIN's six user files, historical
receipt, launcher preferences and l7 launcher. No purchase occurred in this run.

## Follow-up route

The optional `--route=vinyl-entry-v1` reproduces the nine observed navigation
commands, checks source/guest press and release acknowledgments plus consumed
requests, captures the resulting frame, then stops all automatic input. It does
not assume that an acknowledgment establishes the menu state. Primary must
inspect the capture before further input. The owner confirms that the editor's
$1,000 access purchase is normal; any purchase here remains in the test copy.

The updated helper passes146 seeded-policy plus88 passive-policy checks, with
independent review GO. Its SHA256 is
`EC6A1201441778E840540F7758D24247C9F4A57FD4908A3AE6CCDED3821019D5`.
The route completed in a second fresh TEMP root,
`mcla-seeded-inspection-93eca52689e7b8ff8e1a6487bdaf93c2`. After inspecting the
purchase prompt, the primary sent four further acknowledged guest commands:
buy access, open the exit confirmation, confirm exit, and re-enter the editor.
No vinyl was edited or removed. The $1,000 purchase affected only this test copy.

Primary and independent visual review agree on coarse -> finer -> coarse ->
finer vinyl appearance. The two outside-editor views match in pose, visible side
and coarse pattern; the two editor views also match each other. Editor versus
outside uses opposite car sides and different framing/scale, so this is a
state-associated reproduction, not a same-camera comparison or causal finding.

All following guest-output frames are3840x2160. The historical phase names are
mapped to their observed meaning here; they do not establish other purchases.

| Phase filename suffix | Actual visible state | SHA256 |
| --- | --- | --- |
| garage-main-before | $1,000 access prompt, coarse vinyl, $22,645 | B0B67CB48D6062AA03EF436F4071BD717B6B45EF9821126621D27548C7882D40 |
| showroom-purchase-confirmed | Vinyl Editor, finer vinyl, $21,645 | 658D507DD878A344E8CB24B2380F5C999EC315FDC1A01F7662446CA80F67AB4C |
| visual-purchase-confirmed | Exit Vinyls confirmation | 83BC1F88755EBECAEB5073D5D81465105FA695E04C21A10D598DFD037357FE63 |
| performance-purchase-confirmed | Customization, coarse vinyl again | 79E8430BA26179DA974ECA2CD01F1D91212C15CC9EA35F48EAEE495416C2F401 |
| paint-purchase-confirmed | Vinyl Editor re-entry, finer vinyl again | 731D5081440C2FF83985BA9C444ECECFB60F9357CC3FBA7D1E248036A05E84B3 |

The helper reached its300-second deadline and closed normally, exit0, without
forced cleanup. All13 commands have source/guest press-release acknowledgments;
all five phase captures have increasing guest-output sequence numbers. The full
source copy and sentinel verify unchanged. The same nine owner artifacts were
rehashed after closure and remain identical, including all six MAIN user files.

The exact-symptom automated oracle remains open. Whole-frame sharpness across
opposing cameras would be misleading. Next is an unattended captured transition
route and matched outside-state comparison, not a renderer hypothesis or fix.

## Automatic baseline cycle

The distinct `--route=vinyl-cycle-v1` uses15 fixed commands and captures after
commands9 through15. Each command requires source/guest press-release and final
control acknowledgment; each capture requires the correct slot, dimensions and
consumed request. It buys access only in the exact seed's TEMP copy and closes
normally immediately after the seventh capture. Manual and entry-only modes
remain unchanged.229 seeded plus88 passive self-tests and independent safety
review passed before the one720p run. Helper SHA256:
`F166F69E602C81D779AF3DFBA225FDE27AC919BD94A973529626562351545E89`.

TEMP root `mcla-seeded-inspection-74a30578627b2a491619cda96e4a8563` completed
all15 commands/seven captures, then exited0 without forced cleanup. Source and
sentinel stayed unchanged. Primary and independent review inspected all seven
1280x720 images: purchase prompt, editor, exit confirmation, customization,
editor again, exit confirmation again, customization again. Coarse side-decal
steps are already visible outside the editor at720p. The editor appears finer,
but opposite car sides/framing still prevent a same-surface quality comparison.
The result is not an automatic defect verdict or proof of console behavior.

All60 source/guest records,15 control acknowledgments and seven frame records
are present. The host still reports its existing hard-exit path after receiving
normal close; exit0/no external force is not graceful SDK teardown acceptance.
No renderer/SDK change was made. The same immutable host and seed are retained.

Hash-verified copies preserve the two useful returns under
`private/evidence/M8-vinyl-repro-20261011/manual-cycle` (nine files) and
`automatic-720` (11 files). These contain images, log, result and seed receipt,
not a promoted gameplay profile. The next graphics step remains a sharp
same-state symptom oracle; captured route completion does not satisfy it.
