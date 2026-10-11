# Render target acceptance and graphics defects

October 11, 2026. The owner accepts l7 target selection across all four offered
resolutions, unclipped HUD, Alt+Enter, persisted selection and continued save
progress after restart. Those functional checks do not need replay. M8-004 stays
open because the returned 2160p images show substantial building-sign corruption;
the existing acceptance criterion also requires correct world rendering.

## Identity and session evidence

Launcher `private/l7/MCLA-Launcher.exe` still hashes to
62A0A3AB3839D3273AC1872B54E24CE4A966D259FAE8A45468D87D49C0ADDE18.
Host/build/game identities remain those in the
[implementation evidence](M8-render-targets-20261011.md).
All following sessions belong to MAIN
`C:/BDU/MCLA-Saves/MCLA-f5d835a679843f1e95de84646d2a8646`, with RTX5060 adapter1,
stock timing and VSync on. Their receipts specify windowed startup; the owner
tested switching, and the returned 2160p F10 state records fullscreen.

| Session | Requested target | Guest mode and scale | Final state |
| --- | --- | --- | --- |
| 0a27fcd3fb628aadeb0d4b9872fc3dba | 1920x1080 | 960x540, 2 | complete, exit0 |
| ea509a687abbb571d08b736c93c4bf72 | 1920x1080 | 960x540, 2 | complete, exit0 |
| a77101732ba666eab05f5a123db5949b | 3840x2160 | 1280x720, 3 | complete, exit0 |
| 87d6929560b8ddcc875d68cf9167f8f9 | 2560x1440 | 1280x720, 2 | complete, exit0 |

The all-resolution and 2160p selection-persistence claims are owner observations,
not invented extra sessions. The second1080p receipt independently confirms that
mode was reused. No matching new720p session was identified in this bounded return.

All eight before/after snapshots passed their declared file sizes and SHA256
checks, six files each, with no extra files. First1080p before receipt is
164A8DC42C64FEBBD8EDB0E6EE41D74FA10094775007B46FCC3729245F8F73DA;
the remaining seven boundary receipts (starting with its after snapshot)
are D682CB13F81E3035F0EFBB0B894AECDFB7093410A473045D15AEA4FD6A4B1665.
Current campaign SHA256 is
AD934778072A7C8360EBD451DD3B4CB05F3CE87F2D4C80EBD887CE89A4D03110;
display-name SHA256 remains
257870D8ACCCF4D44E97758A0DA2920C489C0F03938E3FE4FFD2B2047F5BDD00.
These are read-only observations, not a restore or merge. No mcla process remained
at the end of inspection. Exit0 is not a general SDK graceful-teardown claim.

## F10 integrity and appearance

Packages live under `MAIN/diagnostics/<session>/live/<package>`.
All four actual F10s contain frames and six verified manifest artifacts each.
The four copied game-save files in each package also match their inventory.

| Package | Image dimensions | Image SHA256 | Visual observation |
| --- | --- | --- | --- |
| live-20261011T081002Z-42360-1 | 1920x1080 | 720BF475F4F8D3142A3B31B1AB782CA2838F2ABA565514C653B53DDF1D48C758 | Road, Mazda vinyl visibly coarse, HUD present |
| live-20261011T081347Z-41152-1 | 3840x2160 | 0A1982DE0EBD2E93F5FB5BE1B494CDF8E275E11BF63CC27A339D99A1276957EC | Selma/Highland; FOOD MART and adjacent billboard locally legible |
| live-20261011T081400Z-41152-2 | 3840x2160 | 8DED0376E108DE28D98D9A0D565484C9B76DDBC277FE0CFDD2787DC370032368 | Large sign surfaces replaced by flat red, yellow, blue and dark blocks |
| live-20261011T081410Z-41152-3 | 3840x2160 | 32DCFE851D8434DBC4BC4606A414FC85AB55A23E634E0A6C18F508E6CEEFCC27 | Pause menu intact; large billboard remains partly solid red/dark |

These are window-only WGC images, with admitted helper and completed cleanup.
Their protocol explicitly says freshness is not established. They support the
visible corruption matching the owner report, not exact capture timing, temporal
cadence, FPS or an independently measured gameplay internal render target.

A fifth package, live-20261011T081011Z-42360-2, has reason
`unexpected-guest-exit`, not `f10`, and no frame. Its five manifest artifacts and
four copied save files verify. Keep this existing shutdown/report-classification
limitation separate from successful F10 capture and from a claimed gameplay crash.

## New global issues

**KI-032, S2, open:** building-sign corruption observed during the2160p session.
Two images clearly capture the symptom; an earlier image in the same session has
legible signs. This does not prove that resolution alone causes it, nor that the
problem cannot occur at other targets. No workaround or common cause with KI-015
is established. M8-017 owns diagnosis; M8-004 world-rendering acceptance remains
blocked by the symptom. Keep the working target/HUD/persistence checks accepted.

**KI-033, S2, open:** vehicle vinyl becomes visibly coarse/pixelated outside the
Vinyl Editor. The owner's two attachments are retained, byte-identically, in
`private/evidence/M8-l7-owner-20261011/`:

- vinyl-editor-owner.png:
  0436B9A1E360F469343AAF096AB5F0935A04D0D889854FCD945316A393F20AC3
- vinyl-outside-owner.png:
  AAE3F7DB5866526A50FF5D456D0F3B50C73F62AC51C748F6A0A9CDF14E65FE98

The editor image shows comparatively smooth edges and readable lettering; the
outside image has large stair-steps and degraded lettering. The1080p road F10 also
shows coarse vinyl. The supplied pair differs in camera, car side and money, so
it is visual symptom evidence, not a controlled same-view comparison. Console
parity, affected target range and root cause remain unverified; this is not proven
4K-only and is separate from paint shimmer or sign corruption.

## Disposition and next diagnostic boundary

No game, launcher or SDK code changed for this return; no saves/settings were
written, no binaries replaced, and no artifacts uploaded. The two attached images
were copied to a new private evidence directory; the originals remain intact.
Independent read-only review confirms the localized sign corruption and vinyl
quality difference, and agrees that M8-004 cannot receive an unconditional PASS.

Under the diagnosing-bugs workflow, this is evidence triage, not a completed
diagnosis. No unattended, red-capable replay of either actual rendering defect
exists yet; therefore no ranked cause or speculative renderer fix is promoted.
The next investigation needs a bounded same-scene sign repro and a matched
editor-to-garage vinyl comparison before causal changes. Do not repeat the
already accepted general resolution, HUD, persistence or F10 tests.
