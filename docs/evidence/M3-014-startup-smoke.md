# M3-014 canonical startup smoke

Date: 2026-08-11

Status: accepted

## Scope

M3-014 promotes the one-off post-GPU trace into the reusable native startup
gate. It proves that the exact supported Complete Edition image reaches active
graphics and audio work within a bounded deadline. It does not claim graceful
guest shutdown, repeated-process stability, frontend usability, or gameplay;
M3-015 and M4 own those boundaries.

## Fail-closed image identity

`MclaApp::ValidateLoadedImageContract` now requires all four runtime values
before guest-thread creation:

| Field | Expected |
|---|---|
| Title ID | `545407F8` |
| Media ID | `5940C9DB` |
| Loaded image | `82000000-829E0000` |
| Entry point | `821322B8` |

The project emits these values in one canonical identity marker. A mismatch
uses the existing loaded-image rejection path and blocks guest launch. The
runner independently requires `private/game/default.xex` size `9,252,864` and
SHA-256
`C386F4001FA569E6AD4B982F441F67412F00B3F47C166134555CD4B59854A432`
before process creation.

## Automated contract

`scripts/verify-startup-smoke.ps1` requires 15 first-occurrence markers in
order: logging lifecycle; project Xenos selection and plugin load; static image
contract; runtime initialization; XEX load; exact identity; dispatch entry;
disc mount and read-only enforcement; module launch; title shader storage;
graphics interrupt and pipeline; and audio callback.

The verifier rejects no-GPU fallback, critical/fatal logs, GPU-load failure,
invalid/unregistered guest targets, `PPC_UNIMPLEMENTED`, structured guest
crashes, loaded-image or disc-root rejection, and post-launch Bink evidence.
Generic VFS error messages are not banned because the read-only contract
deliberately probes rejected traversal/write operations.

The fixture suite verifies the source identity contract, passes a full log and
matching result, rejects thirteen identity/ordering/subsystem/failure
mutations, rejects eight result-integrity/termination mutations, and rejects
one corrupt-XEX preflight before launch: two positive plus 22 negative cases.

## Accepted live run

Private run: `20260811-161327-e19594a9`.

The rebuilt RelWithDebInfo executable reached all 15 markers in 8,180 ms under
the 20-second deadline, signalled cleanup in 161 ms, and completed its harness
work in 8,361 ms. The runner
then force-stopped only its owned PID,
required the process handle to signal within a separate five-second cleanup
deadline, confirmed no process survived, and re-verified the immutable final
log before writing and validating the result. This controlled marker-driven
stop is not a claim of graceful title exit.

| Artifact | SHA-256 / value |
|---|---|
| `mcla.exe` | `5B7688A5A8BB8E40DFCCC39422AD6B16FD7D97D3D98D675789A45ABDC2C2EAA7` |
| runtime log | `EEF4AD9B206A2C49DF792A3FE349BE8141D5ACF0373C8ABF4AA319A1D0C6D587` |
| runtime log size | `56,131` bytes |
| expected markers | `15/15` |
| fatal/post-launch Bink markers | `0/0` |
| process cleanup | confirmed |

An initial harness-only run exposed that Windows PowerShell lacks the newer
two-argument `String.Contains` overload. The runner's `finally` block still
cleaned its owned process; replacing that call with ordinal `IndexOf` restored
compatibility, and the accepted run above passed.

Independent final-diff review then found three contract/race issues: loaded
image size was logged but not rejected, result bytes were captured before the
log became immutable, and a self-exit between marker detection and cleanup
could be misclassified as a controlled stop. The accepted run above includes
all three fixes and an exact post-exit result verifier.

Final repository gates also passed: 28/28 PowerShell test scripts, clean
ast-grep scan with 3/3 rule tests, bootstrap 12/12, and generated integration
with 67 files, 65 C++ sources/objects, exact manifest hash, and zero tracked
generated files.

## Re-evaluation

The supported native route now has an exact, reusable, bounded startup gate.
M3-015 remains the sole M3 task: it must exercise controlled exit repeatedly,
prove no persistent process or cross-cycle/source-data mutation, and resolve or
classify KI-010 before the milestone can close.
