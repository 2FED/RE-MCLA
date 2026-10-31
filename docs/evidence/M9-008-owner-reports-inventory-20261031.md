# Owner F10 packages of 2026-10-30/31: triage and known-issue mapping

Read-only triage by an Opus subagent on 2026-10-31 of every owner diagnostic package from 2026-10-30/31 (packages named by id, no personal data); accepted by the primary agent and applied to docs/known-issues.md.

Read-only triage of every owner F10 package written on 2026-10-30 and 2026-10-31.
Sources: the MAIN profile diagnostics tree (`<MAIN>/diagnostics/<session>/live/`),
the direct-run folders `private/ki047-ab/*/diagnostics/live/`, and the Deck package
under `private/owner-deck-20261030/extracted/`. Only `manifest.json`, `state.json`,
`report.txt`, `log-tail.txt` and `frame.bmp` were read (frames as temporary downscaled
PNGs, deleted afterwards). Saves, `save-private` and minidumps were not opened.
Session-level `report.txt` files written by the launcher were also read; all are blank.

Build identity: report `build` line or, where missing, the per-build cache root hash in
`log-tail.txt`, compared with `build_manifest_sha256` in the candidate receipts:
`2E4BB9F3` = l49, `AD988012` = l50, `E784EB11` = l51, `C928FEDA` = l52, `B2831FAF` = l48.

## Counts

- 39 package directories: MAIN 22 (12 complete, 10 `.partial`), direct runs 16
  (8 complete, 8 `.partial`), Deck 1.
- No `crash-*` package exists for either day (all `crash/` folders are empty).
- Owner text: 2 MAIN packages (session `edbd39cf`, l52, Ukrainian) plus the Deck
  package (English). Every other `report.txt` (package and session level) is blank.
- No report contains the word "pre-existing" or an equivalent; any such owner remark
  was made outside the packages.
- Out of scope but noted: a tester's 2026-10-30 wheel packages (`...-10724-*`) live in
  `private/tester-reports/` and are not owner packages; the Clean profile has one
  2026-10-31 exit `.partial` (`live-20261031T144713Z-17764-1.partial`).

## Package table

Times are UTC (package id). "Exit partial" = automatic `unexpected-guest-exit`
snapshot queued at "Window closing, shutting down..." and cut off by process exit
(state `window` 0x0); "empty partial" = the same, but the directory is empty.

| Package | Session | Build | Mode / output | Kind | Owner text (EN) | Frame | Log-tail facts |
|---|---|---|---|---|---|---|---|
| `live-20261030T133307Z-26500-1.partial` | 560b1a33 | l48 (cache B2831FAF) | unlocked (vblank 60/240), 1920x1080 capture | exit partial | none | Free roam, parked by a billboard; normal | 22-min PC session, no launcher session report. VRAM near budget from 13:18Z, texture hard limit 5339 -> 3565 MB, later 6830 MB of a 6781 MB budget; minute 13:22Z 21.8 fps with 502 frames > 50 ms |
| `live-20261030T172128Z-676-1` (Deck) | 3621f6b8 | l48 (B2831FAF) | stock, 1280x720 windowed | F10 | "launched the game on steam deck / launch the game / it did not respond to the controller / it should" | Title screen, PRESS START | Paths under `Z:\home\deck\MCLA`; F10 written on Z:; backend d3d12 on RADV VANGOGH (vkd3d); "Wine/Proton detected; keeping SDK texture cache limits"; SDL initialized, no controller ever added |
| `live-20261031T080356Z-5656-1.partial` | 0aa0237e | unknown | - | empty partial | none | - | Session 08:03-08:03Z, under one minute |
| `live-20261031T080622Z-23812-1.partial` | c7f1eca5 | unknown | - | empty partial | none | - | Session 08:05-08:06Z, under one minute |
| `live-20261031T092156Z-42608-1` | e614faa6 | l49 | unlocked 3840x2160 (scale 3) | F10 | none | Photo Mode, police car close (broken livery) | Analysed in KI-047 police-livery evidence |
| `live-20261031T092230Z-42608-2` | e614faa6 | l49 | unlocked 3840x2160 | F10 | none | Photo Mode, police car rear | Analysed (KI-047) |
| `live-20261031T092239Z-42608-3.partial` | e614faa6 | l49 | - | empty partial | none | - | - |
| `live-20261031T092516Z-29760-1` | 7d57f31c | l49 | unlocked 2560x1440 | F10 | none | Photo Mode, police car side (dense streaks) | Analysed (KI-047) |
| `live-20261031T092522Z-29760-2` | 7d57f31c | l49 | unlocked 2560x1440 | F10 | none | Same shot | Analysed (KI-047) |
| `live-20261031T092528Z-29760-3` | 7d57f31c | l49 | unlocked 2560x1440 | F10 | none | Gameplay, police car at right edge | Analysed (KI-047) |
| `live-20261031T092541Z-29760-4.partial` | 7d57f31c | l49 | - | exit partial | none | - | - |
| `live-20261031T093637Z-24104-1` | 4a4deac6 | l49 | unlocked 1920x1080 (scale 2) | F10 | none | Photo Mode, police car (clean) | Analysed (KI-047 part 2) |
| `live-20261031T093728Z-24104-2` | 4a4deac6 | l49 | unlocked 1920x1080 | F10 | none | Photo Mode, police car (clean) | Analysed (KI-047 part 2) |
| `live-20261031T093733Z-24104-3.partial` | 4a4deac6 | l49 | - | empty partial | none | - | - |
| `live-20261031T093928Z-30476-1` | cd50d18b | l49 | unlocked 1280x720 | F10 | none | Photo Mode, police car (clean) | Analysed (KI-047 part 2) |
| `live-20261031T093936Z-30476-2.partial` | cd50d18b | l49 | - | empty partial | none | - | - |
| `live-20261031T105908Z-27376-1` | 8ade55ee | l49 | unlocked 2560x1440, launcher, warm cache | F10 | none | Driving out of the garage parking; Boutique / STINCHCO / INSIGHT signs normal | Taken 56 s after session start (10:58:12Z); this is the KI-038 transient sign-flicker package, captured after recovery |
| `live-20261031T105947Z-27376-2` | 8ade55ee | l49 | unlocked 2560x1440 | F10 | none | Photo Mode, police car 19350, livery clean | Warm-cache launcher 1440p check (KI-047) |
| `live-20261031T105952Z-27376-3.partial` | 8ade55ee | l49 | - | empty partial | none | - | - |
| `live-20261031T142517Z-21084-1` | edbd39cf | l52 | stock (30 fps), 1920x1080 (scale 2), launcher | F10 | see quote A | Photo Album "Choose a slot" screen: preview panel shows the grey MIDNIGHT CLUB placeholder, not the photo; several older album thumbnails show the KI-037 black-top/row-band corruption | First session of the new fingerprint (cold store): pipeline wait first minute 344 waits / 1799 ms, longest 50 ms, 0 timeouts; first-minute frame max 1.28 s. Photo window open 14:24:57Z, closed by timeout after 120 frames (4 s); only photo of the session |
| `live-20261031T143712Z-21084-2` | edbd39cf | l52 | stock, 1920x1080 | F10 | see quote B | Race on the elevated freeway (lap 2/2), NPC car ahead; nothing visibly wrong in a single frame | VRAM near budget from 14:31Z (6012 of 6436 MB), texture hard limit 4059 -> 3565 MB, then 6694-6792 MB of 6721 MB; 14:25-14:34Z clean 30.0 fps (> 50 ms: 0-13/min), 14:35-14:40Z 28.6-29.4 fps with 30-71 frames > 50 ms and 1-9 > 100 ms per minute (max 402 ms); finish/crash slow motion (timer scale 0.58 -> 0.1 -> 1) at 14:37:31Z; F10 audit PASS (300 samples, 0 stream stalls) |
| `live-20261031T144152Z-21084-3.partial` | edbd39cf | l52 | stock | exit partial | none | Free-roam frame | Window closed via double-click toggle then close |
| `live-20261031T144610Z-3972-1.partial` | b610b3dd | l52 (cache C928FEDA) | stock, 1920x1080 | exit partial | none (no session report) | Pause menu (CONTINUE / GO TO GARAGE) | 80-s session; Photo Mode window opened at 14:45:57Z and timed out after 120 frames; then closed. Plausibly a retest of quote A, unconfirmed |
| `private/ki047-ab/20261031T100752Z-1440p-full` `...-43848-1` | direct | l49 | unlocked 2560x1440, `--readback_resolve=full` | F10 | none | Police car clean | Analysed (KI-047 part 3) |
| `...-43848-2.partial` | direct | l49 | - | exit partial | none | - | - |
| `private/ki047-ab/20261031T101007Z-1440p-some` `...-280-1` | direct | l49 | unlocked 2560x1440, `some` | F10 | none | Police car clean | Analysed (KI-047) |
| `...-280-2.partial` | direct | l49 | - | empty partial | none | - | - |
| `private/ki047-ab/20261031T103042Z-1440p-freshcache-some` `...-13340-1` | direct | l49 | 1440p, fresh cache | F10 | none | Police car clean | Analysed (fresh-cache run A) |
| `...-13340-2.partial` | direct | l49 | - | empty partial | none | - | - |
| `private/ki047-ab/20261031T103814Z-1440p-freshcache-some` `...-42820-1` | direct | l49 | 1440p, fresh cache | F10 | none | Police car clean | Analysed (second fresh-cache run) |
| `...-42820-2.partial` | direct | l49 | - | exit partial | none | - | - |
| `private/ki047-ab/20261031T121925Z-l50-diag-...` `...-9832-1` | direct | l50 | 1440p, cold store | F10 | none | Police car clean | Analysed (l50 run 1) |
| `...-9832-2.partial` | direct | l50 | - | empty partial | none | - | - |
| `private/ki047-ab/20261031T122422Z-l50-diag-...` `...-3764-1` | direct | l50 | 1440p, warm | F10 | none | Police car clean | Analysed (l50 run 2) |
| `...-3764-2.partial` | direct | l50 | - | empty partial | none | - | - |
| `private/ki047-ab/20261031T125542Z-l51-wait-...` `...-13712-1` | direct | l51 | 1440p, cold store, wait on | F10 | none | Police car clean | Analysed (l51 cold run) |
| `...-13712-2.partial` | direct | l51 | - | empty partial | none | - | - |
| `private/ki047-ab/20261031T125856Z-l51-wait-...` `...-6576-1.partial` | direct | l51 | 1440p, warm, wait on | exit partial | none | - | l51 warm run (only an exit partial) |
| `private/ki047-ab/20261031T130043Z-l51-nowait-...` `...-3844-1` | direct | l51 | 1440p, warm, wait off | F10 | none | Police car clean | Analysed (no-wait control) |

### Owner text, original and translation

**A** `live-20261031T142517Z-21084-1` (l52):
> 1. робив фото 2. запустить білд / зробить фото / натиснути ЗБЕРЕГТИ В АЛЬБОМ
> 3. прев'ю в альбомі на етапі вибору слота ЧОРНЕ, замість прев'ю самого фото
> 4. буде прев'ю ФОТО яке я щойно зняв

English: took a photo and pressed SAVE TO ALBUM; on the slot-choice screen the album
preview is black instead of the photo just taken (expected: preview of that photo).

**B** `live-20261031T143712Z-21084-2` (l52):
> 1. їхав 2. гнать через місто по інтерстрейт трасі, чи як вона там називається
> 3. під час просадок фпс машини нпц трохи тарабанило в просторі вперед-назад + камера стрибала
> 4. щоб машини НЕ стрибали через просадки фпс

English: driving fast through the city on the interstate/freeway; during FPS drops NPC
cars shook back and forth in space and the camera jumped (expected: no car jumps on
FPS drops).

**Deck** `live-20261030T172128Z-676-1` (l48), written in English: "launched the game on
steam deck; it did not respond to the controller; it should".

## Mapping

| Package(s) | Class | Maps to | Note |
|---|---|---|---|
| `21084-1` (quote A) | (a) existing, split proposed | KI-037 (preview sub-symptom "preview while choosing the slot is the previous photo") | First photo of the session, so there is no previous photo and the album shows its empty placeholder. The KI-037 first-photo corruption is fixed; this residual symptom deserves its own row (proposed KI-056) |
| `21084-2` (quote B) | (b) new | KI-054 | Plus KI-048 evidence (VRAM pressure at 1080p drives the drops) |
| All exit/empty `.partial` (19 across MAIN, direct runs, Clean) | (b) new | KI-055 | Known as a teardown caveat only in M7 evidence; no KI row exists |
| `27376-1` | (a) existing | KI-038 | Confirms the package of the transient 1440p Unlocked sign flicker (taken after recovery; frame shows normal signs) |
| `26500-1.partial` (l48, PC) | (a) existing | KI-048 | Long 1080p session over the VRAM budget |
| Deck `676-1` | (a) existing | KI-050 (closed), KI-049, KI-053; KI-025 note | See the Deck section |
| `3972-1.partial` | (c)-like, undescribed | KI-037 / proposed KI-056 retest (unconfirmed) | Ask the owner whether the preview was black again |
| `42608-1/-2`, `29760-1/-2/-3`, `24104-1/-2`, `30476-1`, `27376-2` | (c) | KI-047 analysed runs | No description; already in the KI-047 police-livery, part 2 and part 3 evidence |
| Direct `43848-1`, `280-1`, `13340-1`, `42820-1`, `9832-1`, `3764-1`, `13712-1`, `3844-1`, `6576-1.partial` | (c) | KI-047 analysed runs (part 3, diag runs, pipeline-wait-always) | No description |

## Proposed new rows (copy-paste ready)

| KI-054 | S3 | Open (triage) | During frame-time drops, NPC traffic cars visibly shake back and forth along their path and the chase camera jumps. Owner, 2026-10-31, l52 stock 30 FPS at 1920x1080 (scale 2), racing on the elevated freeway; F10 `live-20261031T143712Z-21084-2`: "під час просадок фпс машини нпц трохи тарабанило в просторі вперед-назад + камера стрибала". The log shows a clean first ten minutes (30.0 fps, 0-13 frames over 50 ms per minute), then, once video memory reached the DXGI budget (6012 of 6436 MB at 14:31Z, texture-cache hard limit stepped 4059 to 3565 MB, later 6792 of 6721 MB), 28.6-29.4 fps with 30-71 frames over 50 ms and up to 9 over 100 ms per minute (max 402 ms). F10 audit PASS (no stream stalls). Leads: (1) the raw measured-time timer (`src/mcla_raw_timer.cpp`: smoothing, whole-millisecond steps, hitch debt) feeds the guest a long step and then a catch-up/slow-down sequence, which the game's traffic interpolation and camera smoothing (tuned for a fixed 1/30 s) render as back-and-forth motion; (2) the hitches themselves come from KI-048 texture eviction under the budget. Next: the same route with the raw timer off versus on at the same budget, and correlate `mcla_raw_timer` step/debt lines with the over-50-ms frames. | Close other GPU-heavy applications; restart the game after long sessions to reset video-memory pressure. | M10 backlog |
| KI-055 | S3 | Open | Every normal window close leaves an unfinished F10 directory: the guest-thread exit hook (`MclaApp::OnGuestThreadExit`) requests an `unexpected-guest-exit` snapshot at "Window closing, shutting down...", and process exit cuts it off, so each session gains a `live-...-N.partial` folder (half of them empty, the rest with frame, state, log tail and minidump but no manifest) labelled as an unexpected exit despite exit code 0. Seen on 2026-10-30/31 in all 19 owner sessions checked (MAIN, direct runs, Clean; l48, l49, l50, l51, l52); recorded since M7 as a teardown caveat in evidence only. Impact: clutter and a misleading "unexpected exit" label in volunteer folders, extra items for owners sorting reports, and the KI-029 history where such a folder blocked the next launch on 0.16.0.1. Fix direction: skip the guest-exit snapshot once a window close is requested, or finish/remove the partial during shutdown. | Ignore `.partial` folders; send only completed F10 folders. | M10 backlog |
| KI-056 | S3 | Open | Photo Album: on the "Choose a slot to save your photo" screen the preview panel shows the album's empty placeholder (grey MIDNIGHT CLUB logo; the owner calls it black) instead of the photo just taken; earlier sessions showed the previous photo instead (recorded under KI-037, whose first-photo corruption is fixed). Owner, 2026-10-31, l52 stock at 1920x1080, first and only photo of the session (cold per-build store, always-on pipeline wait); F10 `live-20261031T142517Z-21084-1`; the photo window closed by its 120-frame timeout. Leading hypothesis (from KI-037): the slot-choice preview reads the photo's guest buffer before the delayed `some` CPU readback of the capture has landed, so it shows the previous content (or nothing for the first photo). Not yet checked whether the saved album image itself is correct. Next: save the photo and reopen the album to check the stored image; repeat once with `--readback_resolve=full` (slow, test only). | Check the photo in the album after saving; take a second photo if the stored image is wrong. | M10 backlog |

## Per-KI evidence additions

- **KI-037** (first Photo Mode picture corrupted): the preview-shows-previous-photo
  part is still present on l52 (`live-20261031T142517Z-21084-1`, empty placeholder for
  the first photo of the session). Suggest moving that sub-symptom to the proposed
  KI-056. The same frame shows several old album photos with the black-top/row-band
  corruption saved on earlier builds; that is old album content, not a new failure.
  Possible undescribed retest on l52: `live-20261031T144610Z-3972-1.partial`.
- **KI-038** (sign placeholder texture): the 2026-10-31 transient flicker at 1440p
  Unlocked (l49, launcher, warm cache) is package `live-20261031T105908Z-27376-1`,
  taken 56 s after the session start; the frame shows normal signs (Boutique,
  STINCHCO, INSIGHT) near the garage parking, consistent with "F10 taken after
  recovery".
- **KI-047** (police livery): add that the first l52 session (cold store of the new
  fingerprint, stock 1080p) ran the always-on wait with 344 waits / 1,799 ms in the
  first minute, longest 50 ms, no timeouts, and a single first-minute stall of 1.28 s;
  no police car was checked in that session. The undescribed packages listed in the
  mapping table are all already in the KI-047 evidence; the l51 warm run left only an
  exit partial (`live-20261031T130017Z-6576-1.partial`).
- **KI-048** (VRAM over budget): not only 2160p. l48 at 1080p unlocked
  (`live-20261030T133307Z-26500-1.partial`, 22 min): near budget from 13:18Z, hard
  limit 5339 to 3565 MB, 6830 MB of a 6781 MB budget, a minute at 21.8 fps with 502
  frames over 50 ms. l52 at 1080p stock (`live-20261031T143712Z-21084-2`, 18 min):
  near budget from 14:31Z, 6792 of 6721 MB, 30-71 frames over 50 ms per minute once
  the pressure began (0 before), and the drops caused the KI-054 report. The DXGI
  budget itself moved between 7131, 5936, 6436 and 6721 MB within one session (other
  applications), so "prefer 1440p on 8 GB GPUs" does not prevent pressure in long
  sessions; the workaround text should say so.
- **KI-029** (unfinished F10 directory after shutdown): link to the proposed KI-055;
  the current builds still create such a folder on every close but do not block the
  next launch.
- **KI-049** (Proton Z: paths): Deck package `live-20261030T172128Z-676-1` (l48) shows
  game, saves, cache and metadata under `Z:\home\deck\MCLA` and the F10 itself written
  to Z:, so both launch and F10-on-Z: are confirmed; the folder-choice dialog is not
  visible in a package.
- **KI-050** (Deck controls ignored, closed): the l48 package tail has SDL 3.5.0
  initialized and never adds a controller, matching the recorded cause; its frame is
  the PRESS START title. No l49 Deck package exists in the sources, so the closure
  rests on the owner's report.
- **KI-051** (empty GPU choice on the Deck) and **KI-052** (Steam keyboard in the
  launcher): nothing in the package (launcher-side); no new evidence.
- **KI-053** (D3D12 through vkd3d): the Deck package logs `backend='d3d12'` on
  "AMD Custom GPU 0932 (RADV VANGOGH)" and "Wine/Proton detected; keeping SDK texture
  cache limits".
- **KI-025** (fullscreen on the Deck unverified): the Deck session ran 1280x720
  windowed (`fullscreen: false`) on the 1280x800 panel.
- **M10 Deck performance note:** the Deck package covers only about 40 s on the title
  screen (no per-minute pacing). The 10-14 fps with audio tearing is the owner's l49
  report recorded under KI-050; there is no l49 Deck package to measure it from.

## Hazards

- No save hazard or progression blocker is described by any owner text or seen in the
  logs (no crash package, no device loss, exit code 0 everywhere, F10 audits PASS).
- To check, not proven: the Deck uses a profile folder with the same id as MAIN
  (`saves/MCLA-5b71c3e9...` under `Z:\home\deck\MCLA`). If the owner progresses on
  both the PC and the Deck copy, the two lineages diverge (the KI-028 external-writer
  and divergent-lineage limit). Confirm with the owner which copy is canonical before
  any sync back.
