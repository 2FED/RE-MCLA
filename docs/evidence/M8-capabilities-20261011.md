# Launcher capability contract

October 11, 2026. M8-003 accepts the stock-only capability contract and its
machine-readable launch record. This does not validate 60 FPS or close the
M8-004 world-rendering gate. KI-032 building signs and KI-033 vinyl remain open.

## Admission and recorded settings

`src/mcla_launcher_capabilities.h` owns the stock/VSync-on policy, the 60 Hz
guest-refresh argument, unsupported-mode errors and resolved target tuples.
Both Check Setup and Start use it after production build/game verification.
Start rechecks independently; a previous green button is not launch authority.
Unknown timing, unvalidated 60, VSync Off and unknown targets fail closed.
1080p/2160p require the exact reviewed 9C6BEA71 build fingerprint; the existing
720p/1440p compatibility rule is unchanged.

Before creating the child, Start writes `capabilities.txt` and binds it from
`launch-receipt.txt`. The 17-field record identifies policy version, verified
build/game fingerprints, resolved timing/VSync/target/guest-mode/scale arguments
and their limitations. Mutable plans are revalidated before serialization.
It explicitly says host state was not observed, FPS was not measured and world
correctness is not certified. It contains no personal paths, names or save data.

The UI's stock-only limitation comes from the same policy. Its checked-status
text names the target and guest refresh without presenting 60 Hz as measured
60 FPS. Native testing found that the old 45-DIP status box accommodated only
one line: all four target descriptions failed the new two-line layout check.
Raising that box to 50 DIP preserves the footer's position and leaves 4 DIP
between them. The test then passes using the real EDIT font, formatting
rectangle and line count. A hidden 120-DPI native render also visibly shows
both complete status lines and the footer. Single-line edit/combo text remains
absent from this rendering method; their values have separate native tests.

## Verification

Fresh native results, all with zero failures:

| Suite | Checks |
| --- | ---: |
| Capability policy | 112 |
| Controller and fake child | 164 |
| Native UI including clipping regression | 924 |
| Preferences and migration | 1064 |
| Profile policy and library codec | 117 / 315 |
| Managed configuration | 50 |
| Filesystem / process / store | 76 / 41 / 125 |
| Build / game / snapshot | 109 / 70 / 26 |
| Legacy import policy / production import | 72 / 31 |
| Production read-only preflight | 16 |
| Private staging fixture | 63 |

Six symlink/reparse fixtures remain SKIP because Windows returns 1314; they are
not passing coverage. AST scan, three rule fixtures and whitespace checks pass.
The controller's independent fake child compares the actual 1440p CLI against
the parsed 17-field record and launch receipt. Other target tuples have pure
policy and production no-write preflight coverage, plus the separately accepted
[l7 owner matrix](M8-l7-owner-return-20261011.md); no new gameplay claim is made.

Retained final fixtures under TEMP include controller
`mcla-controller-test-0b94b25b706f0fa9521ef5cc006051ae`, preflight
`mcla-host-probe-b9fa96a412af593ef7da5732021eca8b`, and staging
`MclaStagingTest-dcb5345b4e8d420083b4128a20629232`.
Private native renders are `private/evidence/m8-ui-capabilities-20261011-*.png`.
The source launcher executable in `out/build/m7-final-sdk20` has SHA256
`894240347206F055456A13B914BC05877F4FFF831293A22651FE129F30B8C820`;
it is not staged or substituted for the owner's l7.

Independent integration review and the final clipping review return GO.
They cover admission ordering, record/CLI agreement, failed-write behavior,
honest claims and native text bounds. No additional owner driving is needed
solely for this metadata integration.

Spark `qwen3.8-27b` completed a bounded explicit two-source audit. Primary
deterministically accepted the exact 17-field order, three source quotes and
their rejection errors. Its proposed missing scale-mutation test was rejected:
the existing per-field loop already checks that mutation and its error.
Private receipt: `private/evidence/M8-capability-spark-20261011`. No model output
was executed or treated as engineering approval.

## Closure boundary

Source 0.20.3.0 closes M8-003 only. The shared target-policy/preferences/UI
implementation is included as its admission prerequisite; M8-004 retains its
separate world-rendering criterion and remains open for KI-032. The verified
[dimension and preference matrix](M8-render-targets-20261011.md) is not silently
promoted to full visual acceptance. The private seeded vinyl experiment is
separate, unpromoted M8-017 work.

No host/SDK change, accepted-candidate replacement, real profile write, ticket
reissue, external distribution or push is part of this task. The owner continues
using the unchanged l7 and MAIN. A future candidate can carry this metadata
alongside a useful renderer fix; repeating accepted l7 gameplay is unnecessary.
