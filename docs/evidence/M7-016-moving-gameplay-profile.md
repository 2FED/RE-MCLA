# M7-016: bounded moving-gameplay CPU profile

Date: 2026-09-06. **Attempt complete; scout selection failed. Manual title exit0; no12-second leaf capture.**

The [gather control](M7-016-inline-float-gather.md) reduced binding cost without
a cadence improvement. The [owner drive](M7-016-proton10-owner-driving.md)
still reports10–17 FPS and audio stutter/crackle. The next question is which
code the GPU command worker and the busiest other threads execute during
representative movement on the exact owner-tested candidate.

## Protocol

Use the existing nine-file Type0 candidate, batching ON, pinned official
Proton10/Sniper/FSync, Vulkan/readback fast and1280x720 windowed SDL.
Create a new isolated prefix and full copied seed/cache outside the entire
Syncthing root. Preserve the normal published bundle, pointer, Steam shortcut,
original prefix and owner profile. No SDK or game binary is changed for this
profile; the gather candidate remains a separate unpromoted experiment.

The owner controls START, actual driving and normal exit. Once the owner
confirms ongoing movement, primary invokes one separate bounded capture.
A3-second99Hz all-thread userspace scout is bracketed by lightweight CPU/maps
snapshots. Exact packet-function hits must uniquely identify the GPU thread;
select it plus the two busiest stable sampled other threads by positive user-time delta.
A12-second99Hz leaf capture covers those three threads concurrently.
Bind PID/TID birth, UID/Tgid, exact module/PE identities and record CLK_TCK.
Preserve raw/text equality, sample policy, clock-domain limits and any
new/departed threads. The capture helper owns only its direct perf children,
and never terminates the game or prefix services.

The title observer independently retains exact startup/runtime and terminal
profile evidence. Its preparation/launch must record expected external
profiling; actual profiling is accepted only through the validated capture
receipt. Collection occurs after normal exit, excludes prefix/cache and
retains complete private evidence without promoting experimental saves.

## Interpretation limits

Owner-confirmed movement is a phase attestation, not a machine video record.
No automatic driving/pause, F10 requirement or external graphics-counter
endpoint is introduced. This profile supplies CPU ticks and sampled executed
code. It does not provide ring-idle/wait-counter deltas, an exclusive producer
budget, caller/predicate attribution from a shared wait leaf, clean FPS,
GamingMode acceptance, or an audio-underflow count.

Vulkan F10 currently has no quantitative underrun counter. The existing
frame-audit path requires D3D12 timing; the route audit is a separate one-shot.
Audio remains an owner-observed symptom. Source review also notices disabled
audio-audit wrappers still doing classification/locking, but this is only an
unmeasured lead and no audio change is selected.

Stop after one valid capture and normal owner exit. Preserve an ambiguous or
failed capture rather than automatically repeating the drive. M7 stays0/16;
the real publisher/evolved-successor save gate remains separate.

## Preparation evidence

Primary and independent reviews pass. Review corrected acceptance of an
empty artifact inventory, unbound command/timing and reused stage data
before deployment. All16 artifacts and six direct child commands/outputs
are now bound; stage timing, raw/header metadata and distinct ordered
windows are checked. Perf and Python clocks remain separate.

Final helper SHA256 values:

- run-moving-profile.py:
  8754523651D60CB9A379A61386BE47F15A69DB019EEFF1F1F95A5CD12432545F
- capture-moving-profile.py:
  BA2D88F116028913308BBF98DF45E11A3D6AEE4C26060C358EA0466525799A58
- collect-moving-profile.py:
  55BC111F668FC85ED2B79BDF5D39C817204CC13A4FC8C77E4A6B5B54AF637D35

All ASTs parse. Capture16 parser/selection plus18 completed-receipt tests
pass on host and Deck. Independent negative cases reproduce intended
rejection. Actual retained625/887 and synthetic193-sample records pass
header/raw/record-stderr checks. Only reviewed helpers and the pinned
runtime manifest were deployed.

Remote root:
/home/deck/MCLA-Diagnostics-20260905/moving-profile-20260906.

Preparation completed without launching a title; a separate check found
no mcla.exe and an empty private prefix. Three receipts returned under
private/evidence/M7-016/moving-profile-preparation-return-20260906.

- Comparison fingerprint:
  42713C2A0DE24FBD0439596E1082B03FA08787C0A75E336B5B11B46AAA191D7A
- comparison-baseline.json SHA256:
  889B081F8553B7A3B7EB857E080B619F4A4EEFDF3FDDAFD725914D93827FA620
- preparation.json SHA256:
  307A76823F5C198BAC837542EBB677BA53EA488B201449488CB0A36B490BE572
- launch.json SHA256:
  9CD00567BB579069CE6AF47B54E9A38D566C70C94F443F75A546B0A43DD59DD5

Primary recomputes hashes/fingerprint and exact9 binaries, complete4-file
seed, two cache files, runtime manifest and manual-only flags.
Owner then confirmed readiness; manual session77098 launched wrapper129408,
loaded the exact runtime and published the active receipt. The owner then confirmed CURRENT driving and the attempt below was made. This preparation
and startup are not physical driving, FPS/audio or successor-save proof.

The one authorized attempt has now ended. Do not relaunch these helpers or
run the normal success-only collector. A separate reviewed failure collector freezes the failed receipts and
rechecks source/lifecycle state as recorded below.

## Failed scout disposition

Owner readiness and current-driving confirmations preceded the run.
The3-second scout records822 samples across13 TIDs, raw span2.985836531s.
Exactly one sample hits Type0 RVA0x1D1BF on TID129618; Type3 has zero hits.
The selector requires one unique TID with at least two packet-function hits,
so it raises "GPU packet selection is not unique" before the12-second leaf.
This error means insufficient packet evidence here, not multiple GPU threads.

Capture result SHA256:
AC01B8693BD22F97663977948CC652D3D1D3961BD5A5450DD872A3A1F532F812.
scout.data SHA256:
BF3DD898FDEC3F2EFF1FAD7E51EEC9B7AD1C64871C8C52B03B06E858A08D53EC.

Primary and independent analysis agree on all raw counts and exact candidate
packet ranges. Post-failure decoding independently matches raw/text/header/
record stderr; this is auxiliary analysis, not the unexecuted stage decoders.
No lost/throttle records appear. The perf child exits0 and is reaped without
force. The failed scout does not become an accepted full capture.

The selection exception precedes scout.json writing, so before/after CPU
snapshots and scout thread births were not retained. Startup maps and raw
mapping records can support qualified address investigation, not complete
stable-lifetime/CPU-delta proof.

The owner exits normally. Title129585/birth162729650 exits0; no forced cleanup,
survivors, cleanup errors or source/prefix/artifact drift. Complete terminal
seed is unchanged. Harness elapsed168.801503842s is not exact title lifetime.
Manual harness status fails as designed because the external capture failed;
it is not a title crash. The bounded failure return is now verified.
No automatic repeat, SDK promotion, FPS/audio PASS or save progression claim.


### Installed perf multi-thread probe

Before deployment, an isolated one-second synthetic two-thread CPU probe on
the Deck uses the same task-clock:u/99Hz, no-inherit, no-buildid-cache and
comma-separated TID selection. No game is launched. The five payload hashes
and raw/text samples independently reproduce:193 samples,98 and95 on the two
requested stable worker lifetimes,0.989016436s raw span. Both workers join
and all direct perf children are reaped. There are no lost/throttle records.

Private return: private/evidence/M7-016/moving-profile-synthetic-return-20260906.
Result SHA256:
F567F693FDF80331FB94197013BF8BCC88625ED365A3DBD7A77187FA4ADB227F.
Raw SHA256:
9F51E078981306C8CA623A1265813DD861835DDC61E6E39D249D29C9FABE052C.

This tests installed multi-thread recording and parsing only. It supplies no
gameplay, movement, FPS or audio evidence.


## Frozen failure return and qualified triage

The failure-only collector52A9C1B2D62C832AEC1E20CAAE75964C65C494452AB7B24E421587489F2B6FE5
passes author AST/actual-receipt/19 negative tests and primary review,
actual-receipt reproduction/four independent rejection checks. It imports
only pinned prior primitives, requires the exact failed receipts/three
scout artifacts/one normal perf child, signals no process and never accepts
the failed recording. It rechecks source bundle/prefix/cache/runtime/
candidate/shortcut identity before freezing the isolated return.

- Frozen inventory:32 files,8,592,576 uncompressed bytes.
- Archive SHA256:
  A6D15E666E8C01477D838187648DA7DF72A159916588DA994A6E88EC126EFBE4
- Inventory SHA256:
  2951FC5E56BF54EC41B044E7F872EE1E7AF039B7ED51AD6A3DF20F221A5AB0B5

Primary verifies every regular member/hash and preserves exact bytes under
private/evidence/M7-016/moving-profile-failed-return-20260906/verified.
The normal prefix/cache are excluded, not deleted; test saves are not promoted.

Raw evidence contains141 title MMAP2 records before the first sample and
68 title COMM records plus one auxiliary perf sleep COMM. All822 sampled
IPs have one executable raw-map match. Main/GPU executable maps are anonymous;
address attribution combines their geometry with exact frozen startup image
headers and on-disk hashes. Missing bracket snapshots still limit conclusions.

The busiest recorded thread has241 samples. Exact host974392DD... registry
matches all30,034 generated keys and binds sub_82412F98 to pdata
[0x147CE70,0x147D3A5). On that thread76 samples are in the function,
68 in its loop/comparison and54 directly on eight decoded PAUSE instructions.
Primary and independent checks agree. This corroborates the already known
shared wait in M7-016-deck-cpu-attribution.md; it is not a new cause or a
reason to remove synchronization. Six callers remain, and one separately
observed completion-caller leaf cannot attribute every helper sample to it.

The corroborated GPU Commands thread has191 samples, including77 exact GPU
image leaves. Independent exact-symbol investigation finds UpdateBindings16,
FindOrCreateTexture6 and ExecutePacket4. The short failed scout supplies
leads only, no clean timing/call-count/CPU-delta/FPS or audio budget.

Next preparation uses one12-second99Hz all-title userspace recording.
It removes sampling-dependent GPU preselection, saves bracket snapshots
before classification and keeps new/departed/reused thread qualifications.
All-thread recording with no-inherit covers threads present at attachment;
new threads are reported, not silently considered captured. Any new drive
requires fresh owner readiness and current-moving confirmation. No source
optimization is selected from the failed scout.
