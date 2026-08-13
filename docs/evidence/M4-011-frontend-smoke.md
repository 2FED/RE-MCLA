# M4-011 saved frontend smoke

Status: PASS

The exact-release canonical private run `20260813-230502-1f0de720` completed
one clean ReXGlue v0.9.0.18 install, the focused VFS tests, one clean host
build, and three isolated frontend cycles. The sanitized result SHA-256 is
`FAD092EAF957BA727053859792040943D93FDFA54CCA5990F6E612F855FAA131`.

Each cycle started from the same pinned post-OOBE profile and followed the
same autonomous route: Complete Edition title, `START` to active nighttime
free roam, `START` to pause, `RB` to Modes, then `RB` to Settings with Options
highlighted. The input probe emitted exactly eight source and eight matching
guest records per cycle, using 200-ms holds and a two-second inter-tab wait.
Route completion took 73,906-74,424 ms.

All three cycles captured four distinct nontrivial 1280x720 frames. The title
capture passed the existing title/logo/prompt gate. Pause-region correlations
against the private reference were 0.637415, 0.810331, and 0.999636; Options
correlations were 0.999175, 0.999955, and 0.996711, above the documented 0.55
floor. Animated gameplay and backgrounds are intentionally not exact-hash
matched.

Every cycle exited 0 through the exact native window's external `WM_CLOSE` in
689-918 ms, required no force cleanup, and left no canonical process. The
source-game tree, four runtime artifacts, and the two-file seed remained
unchanged. ReXGlue's focused mounted-root regression passed 2 cases and 33
assertions. Private saves, captures, menu references, logs, and result JSON
remain ignored.

This evidence does not claim an internal Exit command—MCLA is closed externally
like the console title. It also does not prove first-run OOBE, race flow,
detailed gameplay correctness, save writes, or frontend frame/audio parity.
