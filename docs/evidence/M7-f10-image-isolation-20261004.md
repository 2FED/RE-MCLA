# F10 image-only isolation preparation

2026-10-04. Nonshipping component and isolated candidate preparation; **not game
acceptance**. Earlier candidate47F321E8 retains GDI; new3380E9E8 uses experimental
WGC. Both retain explicit IMAGE UNVERIFIED status. No normal bundle, game or owner
save is changed.

## Scope and contract

The proposed replacement moves one-shot HWND Windows Graphics Capture into a
separate process, avoiding the reviewed unbounded in-process GPU readback/join
risk. It is deliberately unrelated to deferred M8 precise temporal/cadence work.
There are no source timestamp, FPS, producer-clock or image-freshness claims.

- `src/mcla_diagnostic_image_protocol.h`: fixed-width request/result, bounded
  dimensions and64MiB payload/source footprint, checked strides/crop/identity.
- `src/mcla_diagnostic_image_helper_win.cpp`: first available HWND-only WGC frame,
  default notification border, no cursor; client-only BGRA rows. Repeated live
  PID/birth/thread/foreground/PMv2/geometry checks. Hardware D3D11, nonblocking
  readiness polling, soft3s deadline; native calls still need parent containment.
- `src/mcla_diagnostic_image_client_win.cpp`: current-process window only,
  absolute local non-reparse helper path, executable held against ordinary
  replacement. Anonymous mapping, explicit inherited-handle list, suspended
  child, dedicated kill-on-close Job with one-process limit before resume.
  Four-second child deadline plus bounded500ms termination wait or normal-exit
  Job-drain grace. No pixels admitted without normal exit, observed empty Job,
  matching live owner/surface and valid result. Ordinary Win32 calls are not a
  hard-real-time guarantee. Caller must enforce the reviewed helper artifact pin.

No desktop/monitor fallback, picker, input injection, activation, game launch,
capture persistence or settings changes exist in these components. The mapping
is temporarily inheritable: integration must audit concurrent legacy process
launches with unrestricted handle inheritance. This is not a hostile-process
sandbox; helper source/binary remains trusted and pinned by the eventual caller.

## Review and regression evidence

Independent read-only review found a missing external surface binding: shifted
surface metadata with a compensating crop could remain internally consistent.
`MatchesSurface` now binds the result to independently observed unchanged DWM
bounds before reading pixels. The coordinated mutation is explicitly tested.
Second review gives bounded GO for one owned-fixture attempt, not native PASS.

The first fake-child run was RED:43 checks/17 failures. Immediate empty-Job
accounting after process-handle signaling rejected otherwise completed children.
A separate bounded500ms drain observes zero active processes; it does not assume
cleanup or relax admission. The43-check version then passed three additional
independent repetitions. Protocol tests:119 checks PASS. Scoped ast-grep clean;
all three repository rule tests PASS. Later failure-header additions require the
final rerun: **49 checks PASS**, including validated failure-header retention and
rejection of an abnormal success-looking header. Final scoped ast-grep scan clean.

The fake child tests do not open a window or capture anything. They exercise
valid synthetic bytes, pending/malformed/foreign-identity results, abnormal exit,
a success-looking result from a hung live writer, cancellation, oversized output,
well-formed failure and coordinated surface mutation. Rejected pixels never
leave the client. After observed exit/drain only a validated failure header may
be retained for diagnostics, never rejected pixels.

## Build and remaining gates

Opt-in `MCLA_BUILD_IMAGE_CAPTURE_TESTS` defaults OFF; helper, client and tests are
EXCLUDE_FROM_ALL, with no application linkage or package publication. Builds use
the existing Release clang++20.1.8 tree
`out/build/win-amd64-f10-check-20261004` and explicitly selected installed SDK
`10.0.26100.0/cppwinrt` include directory. No dependencies were installed.

Production helper/client compile and link successfully. Changing/fullscreen
game image correctness remains pending. Subsequent opt-in integration and isolated
packaging are recorded below. Do not call these components repaired screenshots,
physical rearm, a measured performance improvement or an M7 task closure.

## One native fixture attempt — bounded PASS

At16:37:52–16:37:54 UTC, the single reviewed
`scripts/Invoke-MclaF10ImageFixture.ps1` attempt passed. Receipt/stdout/stderr:
`private/evidence/M7-016/f10-image-fixture-20261004-1`. Existing evidence consumes
the launch gate; the wrapper refuses a rerun. No pixels were saved.

The fixture creates one own borderless640x360 PMv2 square-corner window, paints
four RGB quadrants, and invokes the production client once after natural initial
ShowWindow display. It neither forces foreground nor retries. All230400RGB
pixels matched exactly, helper exit0, cleanup_complete1, no setup/paint/deadline
failure. No MCLA, fixture, helper or fake-helper survivors observed afterward.

Reviewed binary SHA256s (verified unchanged before/after):

- Fixture `35A6D9CEF3D08B5C6CCA18BE8451BAFDF85D2BB19011F4B6EF0C9851E526DA07`.
- Helper `2E45A6857C21565673A0E70D6949D3A8348F3F7E16DBB67E807B08F374E6C1D2`.

The receipt also pins all component sources, fixture, CMake and runner. Independent
preflight review and primary full-source review preceded execution. This is an
exact known-pattern client-pixel test, not a saved image/independent visual game
review. No gameplay/fullscreen/freshness/timestamp or F10 integration claim.

## Subsequent opt-in F10 integration and owner-ready candidate

Default-OFF `MCLA_EXPERIMENTAL_F10_WGC` changes only the diagnostics translation
unit in the experimental host. The helper is built before a generated SHA256 pin
header; the client hashes the held deny-write/delete EXE handle before spawning.
Only the fixed module-sibling helper path is used. A failed helper gives no image,
never a GDI fallback. Source-level process-launch audit finds only explicit handle
lists in both image/crash helpers and inheritanceFALSE in the portable launcher;
external libraries are not exhaustively audited.

The worker stop token now reaches capture, with additional checks before save
copying, minidump and publication. Existing minidump work can take20s once begun;
neither total package time nor shutdown is claimed bounded to4s. A partial BMP
write is removed only from its own request directory; cancelled partial packages
are not published. Expected helper pin/protocol/admission/cleanup/failure fields
are serialized, with actual selected frame source and freshness not-established.
The hash on failure is the expected identity, not proof the helper executed.

Final tests:51 fake-child/hash checks PASS,119 pure protocol PASS,8 hotkey checks
PASS,1809 actual-SDK checks PASS. PS5.1 and7.6.5 diagnostics:9positive,76negative,
177source checks PASS. Scoped ast-grep clean/three rule tests PASS. Default-OFF and
ON host builds both pass; final pre-rename cancellation check rebuilt successfully.
Earlier native fixture predates the additional host pin enforcement; its exact
executables/sources were preserved under `accepted-inputs` before later rebuilding.
The helper binary remains identical, but final host/game integration is unrun.

Owner entry: `Run F10 WGC STOCK Candidate.cmd`. Separate directory:
`private/bundles/M7-Windows-Stock-F10-WGC-20261004-1`.

- Host52667904B SHA256 `3380E9E8880F66D154CA7B57DC0E11175112DF59378CFB61B6BB229F2B81E0F0`.
- Helper SHA256 `2E45A6857C21565673A0E70D6949D3A8348F3F7E16DBB67E807B08F374E6C1D2`.
- Staging receipt SHA256 `A3266952C05BFF9BD32C33DBDBA55ECC7C17C5DECA9B2FCAE9230315D0B25272`.
- Full independent profile `3763C20A15399248814E10EC4CE1B1422B8D01FD01AAFBC4C6D5AA9CE3D019F0`.

One-time staging script:
`private/evidence/M7-016/prepare-f10-wgc-stock-20261004.ps1`; refuses existing
destination, verifies source/archive before/after, no launch/publication/prune.
Receipt pins30inputs;15immutable game hardlinks only,15non-game single-link files
checked with fsutil. Original/earlier/new profiles remain equal. Source/SDK/normal
portable bundle/current pointer are not promoted or replaced. Both PS editions
pass actual wrapper CheckOnly. Independent final reviewer rehashed host/helper/
receipt, inspected contained config/five CLI roots and gave bounded GO for owner
testing. No version bump, task closure, commit or push.

Owner needs to confirm progressed profile on the new CMD, capture two genuinely
different foreground game scenes with F10 around90s apart while the game advances,
wait30s then quit, and restart this SAME WGC CMD for reload. Scene correspondence,
fullscreen behavior and actual repeated audit remain physical gates. No repeated
races or already accepted audio/input tests required; do not create conflicting
progress in different pilots. Keep failures rather than blindly repeating F10.
