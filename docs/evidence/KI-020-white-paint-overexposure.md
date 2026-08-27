# KI-020 white vehicle-paint overexposure

KI-020 is an open S2 rendering defect. White vehicle paint can become
emissive-looking under ordinary lighting, clipping broad body regions to near
solid white and erasing the shading that should describe panel curvature and
material response.

## Current evidence

During the canonical M6-014 free-roam process, the owner captured a white
Mitsubishi Eclipse on Slauson Ave. The private 2380x1330 PNG is retained under
suite `20260826-100036-1b80ac82` as
`owner-rendering-defects-20260827/white-paint-overexposure-free-roam.png`,
SHA-256
`BC2F34D55E372D440C2F6441A934BCA4B591DC8BCC75D1981020F51BAA0DE8F5`.
The roof, pillars, rear deck, bumper, and broad body panels are substantially
brighter than the environment and read as self-lit; panel shading and paint
detail are lost. The same frame still has readable road, buildings, vegetation,
traffic, and HUD, so this is a material/lighting defect rather than global frame
washout.

## Diagnosis boundary

KI-013 already tracks excessive saturated red/green local-light contribution on
vehicle paint. Neutral-white overexposure may share HDR light accumulation,
specular/material response, packed-float conversion/blending, EDRAM resolve,
output gamma, or tone mapping, but the shared root cause is not yet proven.

The first controlled comparison should keep vehicle, camera, time, weather, and
host exposure fixed; compare white and neutral-gray paint against console/Xenia
reference, then record material/shader identity, pre/post-resolve HDR ranges,
blend state, target formats, selected gamma path, and tone-map inputs. Do not
apply a global brightness clamp: it could hide the white-paint symptom while
breaking legitimate lights, bloom, sky, and wet-road highlights.
