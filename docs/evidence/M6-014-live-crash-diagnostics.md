# M6-014 live and native-crash diagnostics

Status: implemented and machine-verified on 2026-09-03.

MCLA-R `0.8.0.0` adds two local-only diagnostic paths. The rebindable `F10`
action queues a non-overlapping background snapshot for a running title. An
out-of-process Windows helper captures unhandled SEH, terminate, and abort
failures without asking the crashing thread to perform filesystem or logging
work. A successful real crash displays the completed local package path; the
automated crash probe explicitly suppresses that dialog.

The accepted formal run `20260903-203341-44f977ce` clean-built the 75-action
Release graph, including `mcla.exe` and `mcla_crash_handler.exe`. It produced a
real 72,912-byte live minidump and a real 230,614-byte post-runtime crash
minidump. Both advertised
flags `0x201020` (`MiniDumpNormal`, thread information, and unloaded modules)
and contained no full-memory, private-read/write-memory, or handle-data flags.
The result decision was `live-snapshot-and-native-crash-package-pass`.

A separate clean RelWithDebInfo build and probe
`20260903-203711-e9b1522b` produced and verified both packages after runtime
and Tracy initialization. This specifically verifies that the post-setup
last-chance filter refresh survives the instrumented configuration.

Package verification requires:

- atomic `.partial` to completed-directory publication and an atomic latest
  pointer;
- bounded rotating runtime logs, package retention, 128 MiB dump bounds, and
  64 MiB/4,096-entry save bounds;
- reparse-point rejection and per-file SHA-256 inventory for private save
  snapshots;
- exact hashes and byte lengths for every manifest-listed artifact;
- an already-presented Win32 client screenshot rather than a potentially
  blocked GPU readback;
- an explicit `automatic_upload=false` and whole-package
  `package_safe_to_share=false` privacy contract;
- post-runtime reinstallation of the last-chance crash filter and exact helper
  handle inheritance.

Focused validation passed one positive fixture, seven fail-closed negative
fixtures, and 35 source-contract checks. Negatives cover unsafe publication,
full/private-write-copy dump flags, automatic upload, an unlisted crash
artifact, a missing crash journal, and an unsafe latest-package pointer. The
fixture also verifies one nested private save file against its declared size
and SHA-256.

The seven directly affected lifecycle/crash/log/generated-integration fixture
scripts pass, as do a clean ast-grep scan, all 3 rule tests, clang-format for
the new C++ sources, and the read-only 12/12 prerequisite bootstrap. The older
repository-wide fixture sweep is not claimed here: its historical
`test-analysis-config.ps1` rejects the already-present `[[midasm_hook]]`
syntax independently of this change.

The formal probe invokes the same snapshot manager directly so it does not
depend on desktop focus. The F10 registration/default and callback route are
source-checked; the next naturally occurring softlock should use a physical F10
capture before restart to add real incident evidence. This task does not claim
that KI-026 or any gameplay crash is fixed.
