# Swap emission and captured source binding

The diagnostic carries an explicit token from the SDK's swap emission to the
captured mailbox resource. It is a prerequisite for M8-001, not a simulation,
display or FPS result. Primary and independent reviews accept one synthetic
GPU attempt within that scope; the owner build and saves remain unchanged.

## Transport contract

The SDK extends only its existing XE_SWAP packet inside its 64-word command
reservation. The original four swap arguments and six texture-fetch words stay
unchanged. The extension contains magic, version, exact length, process-run epoch
and a nonwrapping serial. A fixed 1024-entry host ledger binds each issued token
to those arguments, fetch state and an optional bracketed CPU snapshot. Entries
are never recycled. Unknown, changed or replayed tokens cannot bind a resource.

This changes SDK-owned bytes in guest-mapped command memory. It does not modify
simulation state or save files; it is not accurately described as zero guest
memory writes. The diagnostic is compiled off by default and requires explicit
one-time runtime configuration with a maximum 12-second window.

The export adapter retains the actual PPC context through nested TLS scopes,
calls the existing typed marshaler once and restores LastError around diagnostic
work. Snapshot callbacks execute outside the short ledger lock. Diagnostic
rejection preserves ordinary swap execution with the original arguments.

The command processor reads the extension through the real ring reader and
compares fetch register0 as well as packet arguments. D3D12 rechecks that state
at source lookup. A value-owned binding is copied into the writable mailbox
slot before publication, then acquired with that slot's resource and retained
through readback. Untagged or failed refreshes never inherit an older token.

## Engineering checks

Fresh private SDK roots are `private/sdk-swap-s`, `sdk-swap-b2` and `sdk-swap-o`.
They derive from the accepted capture SDK, with parent receipt
`private/sdk-swap-parent-receipt.json` SHA256
082EA597F0840507ADFFADFFBB4485DF40868A1A962E017736252C1EB6736DBF.
The first configure directory `sdk-swap-b` is preserved: it lacked dependency
Git metadata and never compiled. The replacement copied genuine parent Git
metadata and verified HEADs; no fabricated metadata or default installation.

Clang20 Release full and incremental builds pass. Tests:

- Numerical token/ledger core: 192 checks.
- Native service: 8 default-off and 94 enabled checks, including actual PPC
  context, nested callbacks, concurrent issuance and failure clearing.
- Actual SDK RingBuffer seam: 100 checks for wrap, endian, truncation and bounds.
- Actual new provenance header: 27 checks. Contradictory snapshot status/flags
  produced four failures before the validator correction and zero afterward.
- Earlier capture header: 135 checks; these do not cover the new binding fields.
- SDK: 401 cases, 3364 assertions, four known skips across 41 isolated suites;
  PPC: 1462 cases and 5753 assertions.
- Python supervisor, prior pure validator and process owner: 58 test methods.
  The new supervisor exercises 18 mocked orchestration outcomes. Created-owner
  cleanup receipts, coherent profile directories and late completion rejection
  have targeted negative checks.

A monolithic SDK unit invocation failed memory-initialization cases because
existing suites retain fixed guest mappings for process lifetime. The established
source-isolated runner covers every discovered case without relaxing those
tests. Its receipt is
`private/evidence/M7-002/sdk-unit-isolated-01ca230a0b3b44829379ecf19ec9e50b`.

## Native scope and remaining work

The new fixture uses only synthetic D3D12 textures, without a window, game,
desktop capture or save access. It exercises Emit/Consume and the actual
presenter/readback path, including duplicate acquisition, retained-resource
concurrent publication, failure, resize and presenter recreation. It does not
execute the actual VdSwap export or command-processor dispatch.

The separate supervisor requires exact source/binary pins, one fresh output
directory, a creation-owned process Job and a 60-second deadline. Every outcome
is preserved; no automatic repetition. The one attempt is consumed and its
admission flag is disabled.

The accepted run is `private/evidence/M8-001/swap-source-binding-20261013-1`.
All 36 input pins were verified by primary and independent review before disarm.
PID47872, birth134363662915872149, exited0 with no forced cleanup, errors,
retained handles or pending ownership. This self-exiting headless fixture has
no WM_CLOSE handshake. Nine report rows have source IDs
`[1,1,1,3,4,12,20,28,36]` and swap serials `[1,1,1,4,5,13,21,29,37]`.
Duplicate IDs retain the exact token/snapshot; the full pixel oracle passed
inside the pinned native fixture. No images were persisted, so this is not an
independent replay of pixels.

| Artifact | SHA256 |
| --- | --- |
| Native fixture | A67AF253BBA4CEA27F26C8C24FAF1E238BBBA5D9195D5DBCEF86C906EEC4D1AD |
| Result | A9EF40F081412C6E86F4067199FB01D70840F623C859F537874D2557846F8283 |
| 2180-byte report | ED3D1837DF60D87FE68CCBA52B529DDB58BF3C5BD09400786F186149FF5E3C23 |
| Runtime DLL | F67F7C7E7142F797C8CA82EB2967AC664C04905DAE29FD565E90BEEDC75BCA05 |
| GPU DLL | DBCC22472CDBB1480B2BFABD4B1351E156333605F87D859103AA40688B4F303D |

## Durable prerequisite boundary

The scoped prerequisite contains only the standalone numerical token/ledger
header and its 192-check regression, this report, version and checkpoint.
Private SDK/native integration and supervisors remain separate pending work;
the commit does not promote those changes or close M8-001. The core test uses
only C++23 standard-library headers and can be built without SDK or game data:

```powershell
clang++ -std=c++23 tests/mcla_swap_emission_core_test.cpp -o <fresh-output>/swap-core.exe
& <fresh-output>/swap-core.exe
```

## Remaining causal witness

Even successful transport establishes contemporaneous emission metadata, not
which simulation update generated earlier draw commands. Main-loop update,
coordinator handoff and actual callback execution require explicit observations.
The counting semaphore also carries control wakes, so a latest-global ID or
nearest timestamp is not a valid cross-thread association. M8-001 remains open.
