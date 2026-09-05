# M7-016: accepted cache return and resolve-readback attribution

2026-09-05. M7-016/KI-027 remain open; M7 remains **0/16**. This follows
[the primitive-cache correction](M7-016-deck-primitive-cache.md).

## Accepted physical return

MCLA 0.14.2.0 / SDK 0.16.1.0 returned two complete F10 packages:
`live-20260905T104122Z-340-1` and `live-20260905T104227Z-340-2`.
Manifest capture times are 10:41:23Z and 10:42:28Z: **65 seconds**.
Owner explicitly confirms BOTH were in gameplay; pause was only afterward.
`live-20260905T104311Z-340-3.partial` is shutdown residue, not a third capture.

The completed launcher session `session-20260905T104009Z-300` spans
10:40:09Z--10:43:11Z, exits 0, verifies 25 immutable files, one full-profile
snapshot and zero save-watcher errors. This independently bounds this old
PID-only pair to one recorded session; old F10s lack process-creation identity.
The verifier's DiagnosticsVerified=false means the guest-free probe was not
requested, not that the owner's F10 packages failed their hash checks.

| Measurement | 0.14.2 gameplay interval |
|---|---:|
| New guest outputs / vblanks | 947 / 3,895 |
| Actual output rate / vblank rate | **14.57 / 59.92 per second** |
| Closed frames / draw packets | 957 / 3,360,844 |
| Frame wall / inclusive draw-packet time | 64.132 / 40.758 s |
| Actual Vulkan fence calls / call wall | 5,748 / 0.413 s |
| Watch arm calls / aggregate wall | 501,436 / 9.192 s |
| Watch disarm calls / aggregate wall | 235,499 / 4.064 s |
| Handled write faults / aggregate wall | 60,426 / 5.015 s |
| Protection failures / safe arm-gap splits | 0 / 202 |

Arm/disarm still average about 18.33/17.26 us. GPU Commands consumed 52.04
CPU-seconds (80.06% of one core), and one guest thread consumed 58.86
CPU-seconds (90.55% of one core). Total CPU utilization is not evidence of
idle critical threads. All spans overlap/nest or aggregate threads: do not sum
them into exclusive frame shares. Old submission counters cover about 10.1%
of frame wall but do not isolate a GPU-busy or exclusive CPU remainder.

Owner perceives steadier 10--15 FPS and 5--8 FPS in pause. The preceding
0.14.1 return measured 12.13 outputs/s, but route/draw load changed; this is
NOT a controlled 20% speedup claim. Pause cost is not measured by this pair.
Owner also reports traffic-light dithering now absent. Preserve that narrow
observation; it does not close KI-015 or prove a primitive-cache root cause.

Complete profile/album remains unchanged: profile SHA-256
`3BBC44D0E0CBE03633DFA3BFFBC776B0C5E62CFDC5AA34BEFB346DE022322B5A`,
save `58B3385C89D9E4999B6791CCB95B6F7ED7963CE07F9C47618850841236784585`,
header `2FAEFBC7FF8CDEADBCE025D9BE914436ED05E27D041F72C8C88798612CFB0D60`,
album `8EE49926E1DFE804DB923EF2512219FC7A078FEB50944C3D2BD4E9AC39F81588`.
Four files / 3,815,088 bytes. Full mutable return archived outside sync at
`private/evidence/M7-016/returned-bundles/<0.14.2 bundle id>/7C95E74A68B90D5507C262239ED3FE7A7F9B362DB9296495B13D44063E0A7392`.
Derived measurements and all test logs:
`private/evidence/M7-016/deck-return-20260905-0.14.2.0`.

## Hypothesis and bounded instrumentation

Code review found no demonstrated host pause-specific FPS cap. Guest START
pause is not GraphicsSystem::Pause(), which suspends the GPU command worker.
MCLA globally defaults `readback_resolve=fast` for working Photo Mode. Vulkan
IssueCopy can consume delayed readback through memcpy into guest physical RAM,
including write-watch faults; resource replacement and cold/full paths can
wait for outstanding queue operations. Extra work in pause is a hypothesis,
not a causal finding from the current two gameplay samples.

SDK **v0.17.0.0 / e11b30af9e20c36d19ef4ee328473b7fb5627478** adds seven
address-free cumulative counters: IssueCopy calls/inclusive time, actual
readback memcpy count/bytes/time, and explicit resolve readback waits/time.
All four wait sites are covered; memexport waits remain excluded. Copy time
includes handled faults but not vkInvalidateMappedMemoryRanges. Relaxed totals
are nontransactional and nested; bytes increment after the completed copy.
No renderer, readback, coherency, cache, protection or scheduling policy changes.

Host **0.15.0.0** wires those counters to F10. The analyzer accepts an optional
complete seven-field group, rejects partial/regressing data and never subtracts
these spans as exclusive stages. `-FirstF10`/`-SecondF10` choose exact complete
direct package names, allowing separate gameplay and pause intervals. New
state records GetProcessTimes creation ticks: equal PID AND positive creation
identity are required, so Proton PID reuse cannot splice two launches. Old
packages require `-AllowLegacyPidOnly`, explicit unverified lifetime output,
and independent session verification; the override never accepts mixed or
invalid creation fields. Same-PID monotonic counts alone are not proof.

## Black F10 frame limitation

Both returned 1280x800 BMPs are all-black, visually inspected. F10 currently
captures the client using Win32 GDI BitBlt/GetDIBits, not Vulkan readback.
Successful GDI bytes/file writing do not establish renderer visibility or
freshness. New optional frame_signal metadata records source, dimensions and
RGB class (unavailable/all-black/uniform-nonblack/nonuniform), ignoring alpha
and padding. Black bytes remain useful evidence; they do not block dump READY.
The verifier independently recomputes the bounded bitmap classification.
**This does not repair black F10 screenshots**, nor regress the separate,
physically accepted Photo Mode fix. No image is promoted to gameplay proof.

A separate source-review follow-up: VulkanPresenter::CaptureGuestOutput maps
a possibly noncoherent kReadback allocation without invalidation before CPU
reading. This is NOT the current F10 producer. Its synchronous tracker wait
may be unbounded, so do not simply call it from the dump worker. Any future
renderer capture needs bounded waits, visibility handling and explicit output
sequence/resource ownership; no such rewrite is included here.

## Review and pre-publication tests

Primary integration plus independent SDK-path and analyzer/fixture reviews.
Review found and corrected PID-lifetime ambiguity. Full all-target SDK tests
pass on RelWithDebInfo, Release and tagged Release: 1,834 executed, four known
BitStream skips, zero failures (1,838 registered); SDK AST scan / 1 fixture.
Seven save fixtures pass five repeated runs in each configuration.

The first parallel SDK run failed two XAM fixtures because separate processes
shared/deleted a temp directory. The test helper now atomically owns a random
new directory and deletes only that owned path. No production saves changed.
Keep the initial failure logs; do not portray the first run as green.
Coverage of new GPU counters is default-zero + timer early return/unwind/
nesting, not injected execution of the four Vulkan branches. Both backends
compile. The real pre-version Release host compiles against the changed ABI;
pure nonshipping frame classifier passes 24 checks. Full parent/post-version
publication receipts are recorded below after completion.

## Final parent gates and publication

AST scan/3 fixtures and both PowerShell 7/5.1: analyzer **12 positive/37
negative**, diagnostics **5/20/146 source checks**, bundle 1/13/41, save lineage
46, session 1/10, build matrix 1/6, alpha 1/31 plus actual alpha verification.
Bootstrap 12/12. An initial alpha check exposed one escaped old SDK regex;
fixed before both complete host suites were rerun. Historical alpha identity
and broad false claims remain unchanged. Native launcher verification is hidden.
Independent final review also corrected stale active test instructions and
current SDK references, without rewriting dated evidence.

Clean publication receipt:
`private/evidence/M7-016/bundle-20260905-113820-20fbffe4/result.json`.
Current Syncthing child:
`mcla-0.15.0.0-win64-185AB35EEEB7532F-8D6BF5EF8BA689E3-58B3385C89D9E499`.
Manifest SHA-256 `C097FA96E9A38541643C201A96C13C01901B647848478174356DF6949AEA2142`;
lock SHA-256 `BDAE69ABE0B6DEED5282BE6147825AD766B3FC295E4D086275593093287D6238`.
25 immutable files, clean relocated launcher/session/guest-free diagnostics,
exit 0. Real diagnostic state verifies positive process creation and all seven
default-zero resolve fields; its 1920x1200 GDI bitmap is labelled all-black.
This is serialization/packaging evidence, not gameplay or GPU branch coverage.
The complete returned profile/album was selected unchanged, archive reverified,
then the one obsolete sync child pruned. Final payload/launcher rehash and
24-check pure frame classifier pass again after clean publication.

First publication failed at staging rename with access denied (holder unknown),
leaving the old current pointer intact. Exact failed staging is preserved at
`private/evidence/M7-016/bundle-20260905-113140-07d1bccd/failed-staging`, outside
sync. A fresh clean build/retry passed unchanged publication/relocation gates;
do not classify the first attempt as successful or blame Syncthing definitively.

## Next physical check (about two minutes, not another soak)

Same Proton 9.0-4 / Desktop settings. First pair: F10 READY in gameplay,
drive about 30 seconds, second F10 READY. Then open pause, settle two seconds,
third F10 READY, leave it paused about 30 seconds, fourth F10 READY. Exit and
finish sync. Compare the two homogeneous intervals explicitly; do not include
the pause transition in either. Visible dump status may temporarily raise
Steam's presented FPS, not guest-output counters. Report audio/motion/geometry;
no wheel testing required. No stock cadence/audio recovery PASS yet.
