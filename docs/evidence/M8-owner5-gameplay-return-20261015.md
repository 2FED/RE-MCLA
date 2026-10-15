# Parked world capture and journal overflow

Owner5 successfully recorded the loaded parked-car scene in all 91 frames and
closed normally. The owner-readiness gate and console correction passed this
physical run. Exact frame-to-work timing remains blocked: the lifetime semaphore
journal stored its maximum 4096 operations and lost 794 more. M8-001 remains open;
there is no 60 FPS or simulation-timing acceptance.

## Collection integrity

Run `private/evidence/M8-001/game-work-owner5-20261015-1` uses token
`34a4fbc331ed45a2b16189b6f592b624`. Primary and independent audits verified:

- All 1854 input pins, using the retained armed archive only for the subsequently
  disarmed supervisor. Primary also checked the original root before disarming.
- Exact inventories of 10 build files, 15 game files and six historical seed
  files; nine MAIN/l12 before, after and current identities match.
- All 91 images and 11 sidecars match their hashes and sizes. Replay of all
  decoders reproduces the saved analysis after JSON normalization. Direct Python
  comparison initially differed on tuples versus JSON arrays, not data.
- PID 38624, birth 134365357923896960, received the normal window-close
  acknowledgment; exit 0, zero active job processes, no forced cleanup, errors
  or pending ownership. Seven package modules are recorded.

The READY signal precedes the fresh source observation by 5.3761 milliseconds.
The fixed slots remain at source observation plus 40 through 49 seconds. Freeze
is at plus 49.1152357 seconds, 73.0096137 seconds before the journal horizon.
The capture census has 91 collected slots, zero failed/missed/unattempted slots
and zero duplicate source identifiers. These are collection facts, not FPS.

| Artifact | SHA256 |
| --- | --- |
| Supervisor result | C0CC4329DE752C7BCBFDF0734132FAE3F0281077BBC10165D28603B1ADE94A21 |
| Source result | F49CAEDB3393A277125D21C31CE743338F1E48A07EEFCD0B23E1361A4372984E |
| Input manifest | ABE43392A925AA5A7F5148BC40B175C3A3453CEF752393BDDC548FAC12A7044C |
| Armed supervisor archive | DFC0417C92B9F120FEFC3890D5AFF8872CCDB4253D0F8B1BA19504CC9D0F4D41 |

## Scene review

Both reviewers inspected all 91 frames in six labeled contact sheets, with
additional original-frame inspection. Every frame shows the parked RX-8 on a
night street. Traffic passes behind the car. No title screen, loading overlay,
pause menu or black transition is present. HUD is absent throughout, consistent
with the owner's report of the idle camera hiding it. Slight view/vehicle pose
changes do not identify hydraulics specifically and are not a timing reference.

The accepted scene is loaded parked-world/idle view, not controlled driving,
fixed-camera benchmarking or proof of correct simulation speed. HUD visibility
was useful for the owner's READY decision but is not required throughout this
scene collection. No further replay is needed merely to restore the HUD.
Derived sheets reside in
`private/evidence/M8-001/game-work-owner5-review-20261015-1`; originals are intact.

## Timing remains blocked

The semaphore prefix reports 4890 observed operations, 4096 stored/completed,
zero pending and 794 lost. Its overflow range is operation 4097 through 4890,
with no reliable entry-time lower bound. Raw history is incomplete and the
handoff observer reports failure. Collector failure/mismatch/late-callback
counters are zero; the recorded capacity loss still prevents admission.

One worker fault has reason 10 (`kMissingOperation`), helper 2049 and operation
0 at QPC 1188684705492, about eight seconds before the first captured slot.
There are 233 untracked Update scopes. Scope/event arrays report no losses, so
their capacities must not be blamed for the observed semaphore-history overflow.
Independent review confirms this distinction and the derivative verdict below.

Strict exact-ID replay produces zero candidate frame-to-work relations and eight
blocking issues: handoff observer failure, semaphore admission loss, untracked
handoff scope, worker observer fault, incomplete prefix admission inventory,
missing fresh-lifetime coverage, loss without a reliable lower bound and an
unbounded coverage gap. Do not weaken these guards, reset history at READY,
select only convenient frames or infer relations from nearby timestamps.

The raw work journal separately contains 466 completed normal bodies: 380 with
vector count 4 and 86 with vector count 5, totaling 1950 vector emissions.
These are raw observations, not source-linked work. The derivative is
`private/evidence/M8-001/game-frame-work-owner5-20261015-1`, analysis SHA256
`5791A5BCA88FFD7367F6DE4A61EF13EE99B369D79F2F3436F887A24F232EFA0A`.
Its verdict remains `WORK_EMISSION_JOIN_DIAGNOSTIC_INCONCLUSIVE`.

## Next action

Inspect native journal capacity and bounded memory/serialization requirements
offline, with overflow regression coverage before preparing another package.
The owner need not repeat this consumed test. The v5 supervisor is disarmed,
its armed archive is preserved, and the AFTER baseline already exists. Never
rerun the attempt, overwrite its evidence or merge its disposable save into MAIN.
