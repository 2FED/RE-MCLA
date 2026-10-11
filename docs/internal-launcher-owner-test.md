# Internal launcher first gameplay test

The owner completed this test on October11. F10 image/package, exact session and
save lineage have been reviewed and accepted. No repeat is needed. The procedure
below remains a reference for future candidates, not a new test request.
[Acceptance and shutdown limits](evidence/M8-launcher-owner-acceptance-20261011.md).

## First setup

1. Close the old MCLA game and its launcher. Open
   `C:\BDU\MCLA-Recomp\private\l2\MCLA-Launcher.exe`.
2. Set **Build folder** to `C:\BDU\MCLA-Recomp\private\l2\build` and
   **Game-data folder** to `C:\BDU\MCLA-Recomp\private\p1\game`.
   The existing game assets are read-only; they are not the selected save.
3. Select **NVIDIA GeForce RTX 5060 Laptop GPU**, **2x**, and **Fullscreen**.
   VSync remains on for the stock preset. Click **Verify build / game**, then
   **Save settings**. Full hashing can briefly make the window unresponsive.
4. Create your preferred permanent saves parent in Explorer, for example
   `C:\BDU\MCLA-Saves`. Choose that existing folder in **Destination / parent**.
   Do not choose `p1`, `p1\user`, the build folder, or a cloud/network folder.
5. Enter **MAIN** in **Library label**. Set **Import/snapshot source** to
   `C:\BDU\MCLA-Recomp\private\p1\user`. Click **Import legacy (copy)** once.
   This creates a complete independent `MCLA-<id>` child under your chosen parent.
   It does not move, overwrite, merge or select the original save.
6. Choose **MAIN** in **Save profile**. Check **Selected full path** points into
   the new parent, and the in-game name matches your existing profile.
   Click **Launch selected profile**.

Do not progress both p1 and MAIN independently: there is no automatic branch merge.
If MAIN was already imported using l1, keep that existing managed profile selected;
do not import p1 a second time. Only switch the launcher/build paths to l2.
If anything fails, preserve the new folder and status text; do not delete locks,
edit markers, replace binaries or repeatedly import the same source.

## Short physical check

No race replay or new progression is required. Check existing progress/name,
drive briefly, pause, listen to sound/music and toggle Alt+Enter out/back once.
Press F10 on the road and wait for **PACKAGE READY**. Quit normally and wait for
the launcher to report the completed session. Reopen this launcher and launch
the same MAIN entry; check progress and name persist.

On that second launch, close only the launcher window. The game should remain
running. Quit the game normally; the hidden launcher owner should then finish
its snapshots and exit. Report any error or unexpected game termination.

Report the chosen profile parent, whether the controls/paths were readable, and
the results above. Actual F10 image and session integrity are reviewed afterwards;
the key press alone is not capture acceptance.

## Storage and limits

The local library/settings are in `%LOCALAPPDATA%\MCLA-R-Launcher-v1`, outside
all builds. The selected managed root contains `user`, build-specific `cache`,
`diagnostics`, session snapshots and recovery history. **Open save folder** and
**Open diagnostics** open only the selected profile's directories.

Library labels and in-game names are separate. Existing managed roots use
**Register managed folder**; old bundles use **Import legacy (copy)**. Copy and
restore always create a new independent profile and require explicit selection.
Only stock timing is enabled; validated 60 FPS remains M8 work. F4 host settings
and the console are disabled for managed sessions to protect configuration precedence.

This candidate excludes game assets and personal data, but has not been approved
for external distribution. Do not send it or p1 to volunteers yet.

The signed base allowance ends October25 at00:23:38 Kyiv. The published online
grant extends l2 to November7 at21:23:38 UTC; this PC has already verified and
cached it. Expiry only blocks new launches and never removes saves.
[Policy and maintainer workflow](test-build-expiry.md).
The current UI still offers1x/2x. Explicit720p/1080p/1440p/2160p render targets
are recorded follow-up M8 work, not an implemented option in l2.
