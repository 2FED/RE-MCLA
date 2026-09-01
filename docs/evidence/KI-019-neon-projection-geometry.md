# KI-019 exposed underbody-neon projection geometry

KI-019 is an open S2 rendering defect. Enabled underbody neon can render its
projected primitive as a visibly bounded translucent diamond or quad beneath
the vehicle instead of only a soft glow conforming to the ground and car.

## Current evidence

During the canonical M6-014 gameplay progression, the owner captured the garage
`NEON/WINDOW TINT` screen with `NEON` set to `ON`. The private 2377x1333 PNG is
retained under suite `20260826-100036-1b80ac82` as
`owner-rendering-defects-20260827/neon-projection-garage.png`, SHA-256
`57D52762DD162D6EAEAD5CD2A11AC0E5189BCC12D8CA4C791427CFBFF380D4EF`.
The bright ground contribution has a readable polygonal boundary extending
around the Camaro. The owner reports that the diamond/quad presentation is
intermittent in normal play.

The repaired Photo Mode route later captured the same defect in the album:
the otherwise valid non-black 2026-08-31 image shows a hard triangular/diamond
cyan footprint projected from the side sill rather than only soft underglow.
The private 2411x1327 PNG is retained beside the original garage capture as
`neon-projection-photo-album-20260831.png`, SHA-256
`4C9DC91CB6F9EDF7AFF1F3484A723D2953B23BDAEF171E16BAEE99540A3622BA`.
This second scene confirms KI-019 is independent of the former black-JPEG
Photo Mode defect (KI-021), which is closed.

This screenshot establishes the incorrect projected shape but not its temporal
trigger. It is separate from KI-013 saturated local-light transfer because the
artifact is on the ground projection rather than vehicle paint.

## Diagnosis boundary

Candidate stages are projection geometry/UV generation, depth intersection,
texture border or clamp sampling, source/destination blend factors, target
format, and EDRAM resolve/compositing. The first controlled repro should hold
car, camera, garage, and exposure fixed; toggle neon off/on, cycle one color,
and capture the responsible draw's shader hash, texture addressing, blend/depth
state, color-target format, ownership mode, and resolve path. A global bloom or
brightness adjustment is not an acceptable fix for visible projection geometry.
