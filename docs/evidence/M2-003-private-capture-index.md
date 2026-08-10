# M2-003 private stock-render capture index

Date: 2026-08-11
Result: PASS

## Scope and handling

All source images are native Xenia F12 guest-output captures at 1280x720 from the verified stock session or its verified isolated-profile resume. The PNG files remain ignored and private; this tracked index stores only a state label, capture basename, SHA-256, and visual-review result. It does not redistribute copyrighted game imagery.

## Required state matrix

| Required state | Private capture basename | SHA-256 | Visual evidence |
| --- | --- | --- | --- |
| Intro | `545407F8 - 2026-08-11T00-22-57.png` | `5EF41E811D622DB43EB09E2EB264EFBC1A3DA4E1BA1322B0A53441515BF8E402` | Opening vehicle sequence rendered correctly. |
| Frontend / main title | `545407F8 - 2026-08-11T00-59-52.png` | `7F0293842A6AA30EF0B0EA7C7954FF5130A03ECF6E3A112EEFCAA4A6B11C613E` | Complete Edition title/frontend and input prompt rendered after clean resume. |
| Garage | `545407F8 - 2026-08-11T00-54-45.png` | `0DBF0505CCC82E7643746597A3457CDFE5F160C48116556AE5F90BC4B85F97B2` | Vehicle, garage environment, menu, money, and button prompts rendered. |
| Daytime/dusk race | `545407F8 - 2026-08-11T00-26-20.png` | `391C42BAAD919B6884AF34F3039B28126F3F88465EF14257855EC1F67B5EE345` | Active opening race with street scene, position, timer, minimap, and speed HUD. |
| Nighttime race | `545407F8 - 2026-08-11T01-02-28.png` | `C5EC83D8A7DFB1AFD6E64CECAD3CA785E339CD9A0FF7ABC1978D189CDE7E67CB` | Active lit night course with opponent, route markers, position `2/2`, minimap, and speed HUD. |
| HUD | Covered by both race captures above | See race hashes | Timer/position, minimap, tachometer, gear, speed, route, and street/music areas are represented. |
| Pause menu | `545407F8 - 2026-08-11T00-52-04.png` | `EA85BD3AECFAD647819682D19E1951097A22F59360500ACD70A0DAF4DC80BFE2` | Pause overlay, continue action, tabs, and A/B prompts rendered over the night scene. |

Supplemental private captures cover the Carney's introduction cutscene, a moving daylight vehicle cutscene, night free roam, garage vehicle management, shop confirmation, and tutorial/map UI. They are useful for later regression comparison but are not required to close this task.

## Verification

- Every indexed PNG existed and matched its recorded SHA-256 during review.
- Each image was visually inspected at original or high detail; labels are based on visible game state rather than filename assumptions.
- The matrix includes all seven required states and both daytime/dusk and nighttime active-race HUDs.
- `git status --ignored` confirmed that source captures remain ignored; no PNG was staged.
- The tracked index contains no absolute private host path or profile identifier.

This is a visual reference baseline, not a claim of pixel-perfect native-port parity. Later milestones must compare native output against these state-labeled private originals.
