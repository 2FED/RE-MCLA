# M4-012 frontend frame and audio comparison

Status: PASS

The canonical private run `20260814-003029-963c60bd` compares the supported
saved frontend at native draw scales 1 and 2 against the pinned stock-Xenia
baseline. Its sanitized `result.json` SHA-256 is
`C5063DA3482AA819BB430684ADBE1DF3AC78473FEAFA16E582D0DA307617CE0B`.

The scale-1 side re-verifies all three accepted M4-011 cycles and their four
1280x720 captures. The new clean-build scale-2 cycle captured the same title,
saved free-roam gameplay, pause, and Settings/Options route at 2560x1440. It
used a 45-second pre-input settle and a separate 45-second gameplay wait so a
loading logo cannot be accepted as the title or gameplay state. The process
then closed externally through exact-window `WM_CLOSE`, exited 0 in 715 ms,
required no force cleanup, and left no canonical process.

Three immutable 1280x720 Xenia frames are indexed by hash: title
`7F029384...C613E`, free roam `A4905530...4F8B`, and pause
`EA85BD3A...BFE2`. Stable edge-region comparisons for the 2560x1440 cycle
were:

- title logo 0.925287 and `PRESS` 0.946633;
- gameplay HUD 0.810976;
- pause footer 0.861960 after a bounded -5/+1-pixel translation only;
- native Options scale-1 versus scale-2 0.751092.

The pause registration searches no more than +/-8 horizontal and +/-2
vertical pixels and cannot compensate for scale, rotation, deformation, or
different content. Animated world/camera regions are deliberately excluded,
so this is not a whole-frame equivalence claim. The private 1280x800 contact
sheet (`AF29FAA1...B444`) was reviewed: orientation, logo/prompt, HUD,
pause compositing, and Options structure are recognizable and coherent at both
native resolutions. The previously reported green vehicle-shadow tint remains
a minor S3 deviation rather than a parity blocker.

The audio comparison binds the pinned Xenia log
`0A7E5418...C4D0D` and its one XMA decoder, one audio worker, and one successful
client-registration lifecycle event to the accepted M4-007 five-minute native
route: 60,594 nonzero guest submits, 60,587 nonzero device frames, 319,325
nonzero XMA frames, maximum queue depth 8, and zero starvation/failure/drop
counters. Stock Xenia does not expose event-level identity in this baseline,
so individual UI/music event identity is recorded as `not-observed`, not
inferred from lifecycle parity.

The final verifier rehashes the three Xenia images and log, all M4-011 and
M4-007 evidence, the scale-2 rotated-log set and captures, the clean-build log,
contact sheet, source-game tree, save seed, and four runtime artifacts. Raw
frames, logs, save data, and JSON remain ignored under `private/`.

