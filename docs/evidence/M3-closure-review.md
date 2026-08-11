# M3 closure review

Date: 2026-08-11

Decision: **GO M4**

Release version: `0.3.0.0`

## Scope review

All 15 M3 tasks and all 15 acceptance-register entries are complete. The
ignored locally generated corpus now clean-builds into the native host in
Debug, RelWithDebInfo, and Release; at runtime the host separately validates
and loads the exact user-supplied Complete Edition image, mounts source data
read-only, enters guest startup, loads the Xenos plugin, creates a graphics
pipeline, and reaches audio callbacks under bounded diagnostics.

No criterion was weakened. M3 does not claim a rendered frontend, menu
usability, save compatibility, gameplay, correct graphics/audio output, or a
public consumer package. Those remain M4 and later scope.

## Final verification

| Gate | Result |
| --- | --- |
| M3 task ledger | 15 complete, 0 open |
| M3 acceptance register | 15 entries |
| M3 task commits | 15 distinct task commits through `ce1f0fe` |
| Sanitized M3 evidence | 15 task reports plus this closure review |
| Project PowerShell tests | 29/29 passed |
| `ast-grep scan` | clean |
| `ast-grep test --skip-snapshot-tests` | 3 passed, 0 failed |
| Fresh bootstrap | 12 passed, 0 failed |
| Native build matrix | final run `20260811-172749-d2732624`; clean Debug, RelWithDebInfo, and Release; 65/65 generated objects in each; 32,756 / 49,934 / 44,299 ms |
| Generated integration | 67 files, 65 C++ sources, 65 generated objects, 0 tracked guest-derived files |
| Generated-manifest SHA-256 | `85BAAEE37F082804CBF1B969E95F7185463BAB5A7DE1408E59562D303681BF8D` |
| Canonical startup smoke | all 15 ordered identity/VFS/GPU/module/graphics/audio markers |
| Repeated stability | clean build, 10/10 crash probes and 10/10 normal `WM_CLOSE` cycles |
| Process cleanup | exit 0 for all 20 invocations; 0 force cleanups; 0 exact-path orphans |
| Source-data integrity | 15 files, 6,569,586,392 bytes, 15/15 hashes unchanged |
| Runtime integrity | executable and three staged runtime/plugin DLLs unchanged across cycles |
| Accepted executable SHA-256 | `1F06BC1EBEB79CA9CCA0E89C9320286484990FB42C4D9D09C7E393C7357FB0AD` |
| ReXGlue fork | v0.9.0.7 / `efac376998cbb0520295d308be4703574a12a995` |
| Fork publication | branch `mcla/mcla-r-hotfixes` and tag `v0.9.0.7` both resolve remotely to the pinned SHA |
| Prohibited tracked game/generated artifacts | 0; `generated/rexglue.cmake` is tracked non-proprietary SDK boilerplate |
| Public evidence privacy | hashes, counts, relative run IDs, and bounded metadata only |
| Git integrity | no object-integrity error; dangling local objects only |

The accepted repeated-cycle run is
`20260811-165807-b340ca91`. Its physical verifier re-hashes the sibling build
log, all 20 runtime logs, ten crash reports, and every user/cache tree; reruns
the lower startup and crash-report verifiers; rejects missing, extra, mutated,
or reparse-point evidence; and proves prior-cycle immutability.

After the M3-014 and M3-015 host-source changes, the closure review reran the
complete current-source matrix as `20260811-172749-d2732624`. All three
configurations clean-built 65/65 generated objects and staged only their exact
configuration-matched runtime, Tracy, and Xenos DLL variants. The sanitized
table above records durations; raw build logs and full artifact hashes remain
under ignored private evidence.

The SDK publication gate was independently read from
`https://github.com/2FED/rexglue-sdk.git` after publication. Both required refs
reported exact commit `efac376998cbb0520295d308be4703574a12a995`, so a fresh
clone is not dependent on an unpublished local branch or tag.

## Exit-criteria assessment

- Native Debug, RelWithDebInfo, and Release configurations compile and link.
- Exact Title ID, Media ID, loaded image range, entry point, dispatch map, and
  source-XEX hash are fail-closed before guest execution.
- The canonical startup route reaches the registered guest module, Xenos GPU
  plugin, graphics interrupt/pipeline creation, and audio callback without a
  fatal, invalid-function, `PPC_UNIMPLEMENTED`, guest-crash, or post-launch
  Bink marker in the accepted observation window.
- Ten consecutive normal window-close cycles leave no owned process and do not
  modify source game data or runtime binaries.
- No S0/S1 defect remains in M3 setup, image-loading, or controlled-shutdown
  scope.
- Diagnostics localize guest C++ exceptions without publishing guest memory or
  private host paths by default.

Decision: `GO M4`.

## Residual risks entering M4

- No valid presented frontend frame or menu-navigation route is yet proven.
- Render-target, EDRAM, depth, shader, gamma, and frontend audio correctness are
  unverified even though pipeline/audio callbacks are reached.
- Profile, controller-slot, reconnect, locale, and frontend offline-service
  behavior remain M4 work.
- KI-010 remains S2/contained: ReXGlue can force-terminate the Audio Worker
  while it owns a guest-heap lock. The synthetic guest-free crash probe avoids
  that teardown after safely flushing its report; this is not a general SDK
  teardown fix.
- Hardware SEH/fatal-abort coverage and generic return-bearing import stubs
  remain outside the M3 crash-report claim.
- Consumer packaging still requires the M9 asset-free launcher flow. The
  current developer pipeline already performs exact validation, contained
  extraction, local code generation/compilation, and native launch, but it is
  not yet a turnkey user experience.

No residual M3 risk requires manual user action before M4-001 analysis. M4
begins with the first valid D3D12 Xenos frame and frontend render-path audit.
