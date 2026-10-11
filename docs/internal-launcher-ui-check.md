# Accepted launcher fixed window check

Completed October11: the owner confirms no outer scrollbars, no clipping and
working Cancel in l6. M8-019 is accepted; the checklist below is retained only
for regression reference, not another requested owner test.

The owner accepted l5 automatic startup checking, green START GAME,
collection-parent prefill, Cancel preservation and fullscreen edit/recheck.
This follow-up checks only the requested window geometry changes in l6.
Open `C:\BDU\MCLA-Recomp\private\l6\MCLA-Launcher.exe`.
[Verified candidate and tests](evidence/M8-launcher-ui-20261011.md).

Close the old launcher first. The successor uses the existing local library and
settings; keep the already imported MAIN profile. Do not import p1 again, create
another profile or restore a snapshot for this check.
The stored build path can remain l2/build: its verified game host is identical
to l6/build. You are testing the new launcher UI, not changing the game build.

1. Confirm the main window has no maximize button and cannot be resized by
   dragging its edges. Moving, minimizing and closing should remain available.
2. Check Play, Profiles and Setup: no outer horizontal/vertical scrollbars when
   the content fits, and the bottom controls remain visible. The separate Status
   text box still supports scrolling long diagnostic messages.
3. Open Create empty profile, check that its fields and Cancel/Confirm buttons
   fit without unnecessary scrollbars, then Cancel. Do not create a profile.

On a small work area or sufficiently high Windows scaling, overflow scrollbars
are intentional so controls remain reachable. Do not change display settings
for this check; report any clipping at your normal settings.
Render options remain1280x720 and2560x1440. Genuine1080p/2160p remain M8-004.

Report any clipped text, missing fields or confusing labels. No actual import,
name change, save migration, game launch or repeated F10 test is required here.
The exact import-clears-drafts regression is covered by the native synthetic test,
which invokes the real UI handlers without touching personal saves.

Play contains normal launch controls. Profiles contains explicit import,
registration, copy, restore and naming operations; navigation alone does none of
those operations. Setup contains build/game folders and read-only verification.
Save settings remains explicit, while Play retains the existing behavior of
validating and saving the current settings before launch.
