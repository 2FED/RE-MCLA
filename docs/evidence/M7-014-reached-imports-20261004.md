# M7-014 current stock reached import audit

## Final task acceptance

October10 primary and independent review accept M7-014 within the observed-route
scope. Both PowerShell5.1 and7 reruns pass8source pins,11facts,8drift rejections,
one direct import/wrapper call each and9synthetic branch cases. Primary inspected
the actual wrapper/caller branch and independently confirmed the fallback below.
No runtime return semantics changed; unobserved surfaces remain unavailable or
unknown, not support claims. Source version0.18.5.0 is an audit checkpoint, not
a relabeling of accepted0.18.0.0 binaries. Structural rule tests pass3/3; the
PowerShell-only audit adds no C++ code requiring a new native build or AST scan.

The final s2 subtitle-route log adds one UNAVAILABLE private-key marker, one
COMPAT dismount marker and one cross-thread critical-section compatibility marker.
Its104189bytes hash to `030A1DEDAFD83CE2695337A6D1B44E4F0739B21A079172D47E5816E10A3CC91A`.
No new unsupported/stub/invalid-target/fatal/device-loss category is observed;
INFO silence does not establish DEBUG-only call absence. The separate exit-time
diagnostic remains a lifecycle limitation, not a newly reached unsupported import.
The primary and independent reviewer agree; further observed calls reopen this
incremental audit. No game launch, save edit or upstream report was needed.

## October 10 successor audit disposition

The observed Windows route set retains explicit unavailable/compatibility results.
The filtered signature-verification export now has a source-bound caller disposition:
verification is unimplemented, and its current constant-zero result forces the
reviewed direct guest caller's fallback, not positive verification acceptance.
Invocation remains unknown. This supports bounded M7-014 closure as an audit of
observed routes with unsupported surfaces documented, not complete export support,
cryptographic correctness or proof that every indirect route is safe. README-AI
owns the final task decision; any newly observed unsupported call reopens review.

### Preserved successor returns

Only evidence copies were read; the live p1 profile was not locked or inspected.
The earlier log is an exact byte prefix of the later log, so count three gameplay
sessions, not five. A preceding guest-free diagnostic probe is not another gameplay
route. Both receipts and all payloads were independently rehashed: first return
180 entries/38,375,514 bytes, later return220 entries/55,314,850 bytes. The complete
profile/archive remains EFF52498..., main save EE824CF5....

| Return under private/evidence/M7-002 | Log bytes | Log SHA256 | Gameplay observations |
|---|---:|---|---|
| profile-owner-return-20261010-1/logs/mcla.log | 24916 | 3456D96FB0670ECF6E865FA0BC3EA25CD4877395FF404F7B35DC9544C768C459 | Two sessions; name/progress reload, pause/audio/Alt+Enter accepted. Owner corrected that F10 was not pressed here. |
| profile-f10-return-20261010-2/logs/mcla.log | 127687 | A0D8626E28493D4C26E0860D398C47FDBE86D69BAF67133BC2D3AFC93E443C61 | Same two sessions plus one explicit F10 follow-up; completed package, coherent street image and 300-sample audit accepted separately from timing/performance. |

The three gameplay sessions are 20261010T123005Z-47616, 123631Z-40908 and
124849Z-26276. Each exits0 with zero save-watcher errors; snapshot counts are
3/2/1. The first final save equals the second launcher-start; latest profile and
archive hashes agree. Three UNAVAILABLE-key, three COMPAT-dismount and three
cross-thread compatibility markers occur, with 30 ordinal-resolution lines.
These are once-per-process observations, not invocation totals. No new stub,
fatal, invalid-target, device-removal or DRED marker appears. DEBUG/TRACE absence
still cannot prove filtered imports unreached. Two invalid texture-fetch warnings
remain diagnostic limitations; no suggested renderer bypass was enabled.

### Signature verification direct caller bound

`xboxkrnl_crypt.cpp:661` implements `XeKeysConsoleSignatureVerification_entry`
as a DEBUG message and return0, with no pointer writes. Its comment says success,
but the guest's actual branches, not that comment, determine this disposition.
Ordinal0x257 is registered at guest thunk0x827BD8E4. Generated TU29 contains one
explicit invocation, in sub_8244D9C0; there is one explicit call to that wrapper,
from sub_8244E778. Dispatcher registration means indirect entry remains possible;
the direct-call count does not establish whole-program reachability.

The wrapper initializes its stack output word to0 and returns1 only when both
the export result and that word are nonzero. With the current export it returns0.
Its reviewed caller branches on0 to0x8244E8E0, writes the expected magic/version,
clears two entries and sets low flag bits before continuing. It does not take the
existing-state shortcut based on a positive wrapper result. This is a bounded
fallback disposition, not console-signature verification, an export-wide safe
success policy or proof of a physical invocation. No runtime return was changed.

The read-only regression `scripts/test-m7-import-disposition.ps1` pins eight
inspected files, checks11 source facts and one direct import/wrapper call each,
rejects eight in-memory source-drift fixtures and models nine return/output
combinations. The model is NOT execution of generated guest code. Changed source
fails before acceptance and requires review before repinning. It does not scan
private logs or write source, generated files, saves or temporary files.
Windows PowerShell5.1 and PowerShell7 both PASS with those counts. An initial
PowerShell5 parameter-default resolution failure was corrected by resolving the
script-relative repository root inside the body; the final source passes both
shells. Parser and scoped whitespace checks pass.

- Crypt implementation SHA256: `6EF4032AA9BD9CD10721CA1503C9A0B2C7F7415EC55D5F2DAEF9DBF9C9F9C880`.
- Generated TU29 SHA256: `2473F38749D70C6EBD2D5820E6ADFC756E0A2A906747E8D92EC836978F833675`.
- Other pins cover import initialization/registration, export ordinal table and
  IO/RTL/object source files for the deferred inventory below.

### Retained unsupported inventory

These are the twelve surfaces retained by
[M6-013](M6-013-reached-unsupported-coverage.md), not twelve newly observed calls.
They remain unimplemented or bounded as stated; no silent support expansion.

| Surface | Current source disposition and limit |
|---|---|
| XeKeysConsoleSignatureVerification | Constant0/no writes, DEBUG-only; direct caller fallback bound above. Invocation unknown; verification unavailable. |
| StfsCreateDevice | WARN stub returning success without creating a device/output. Not observed in retained logs; unsafe to advertise as implemented. |
| StfsControlDevice | WARN stub returning success without performing control. Not observed in retained logs; functionality unavailable. |
| __C_specific_handler | WARN-only empty entry; exception handling not implemented by this export. Not observed, not a general exception-recovery PASS. |
| RtlUnwind | WARN-only empty entry; unwind operation unavailable. Not observed, not an unwind-correctness PASS. |
| IoDismountVolume | Generic stub; distinct from the reached, handle-validated IoDismountVolumeByFileHandle compatibility implementation. |
| IoInvalidDeviceRequest | Generic stub; unsupported disposition retained, invocation not established. |
| IoCompleteRequest | Generic stub; request completion unavailable, invocation not established. |
| ObIsTitleObject | Generic stub; title-object classification unavailable, invocation not established. |
| IoCheckShareAccess | Generic stub; share-access checking unavailable, invocation not established. |
| IoSetShareAccess | Generic stub; share-access state operation unavailable, invocation not established. |
| IoRemoveShareAccess | Generic stub; share-access state operation unavailable, invocation not established. |

Generic stubs WARN and leave r3 unchanged; they are not safe deterministic success
implementations. No call marker was observed for these remaining surfaces, but
that statement is not proof of non-invocation. Export-wide cryptography, exception,
STFS or sharing-policy support would require implementation/caller-specific tests.
If actual signature-call counts are needed later, add bounded observability and
collect a new artifact-bound route; do not reconstruct calls from INFO silence.

## October 10 incremental return review

Independent read-only review of the crash, repaired Pinkslip, Wager/Goal Attack,
Clean and Sandbox returns found no newly observed unsupported import or stub.
Primary rehashed the four deduplicated main logs and confirmed their existing
UNAVAILABLE/COMPAT counts and the old branch's single fatal. The INFO blindspot
below remains; that incremental log-only review did not close M7-014. The later
caller-bound disposition above adds source evidence without changing those logs.

Twenty-eight main/runtime/tail files have 24 distinct byte hashes. Six cumulative
main logs contain 14 startup entries but only 11 unique sessions: three old WGC
(two October 5, one October 10 crash), four repaired Pinkslip, two Clean and two
Sandbox. The 12,157-byte Pinkslip return is an exact prefix of its 24,343-byte
reload return, itself an exact prefix of the 48,789-byte Wager/Goal Attack return.
Removing only the thread field from timestamped supplemental lines yields zero
events absent from the selected main logs. Do not count mirrored crash journals
as additional crashes.

| Main log under private/evidence, followed by logs-stock/mcla.log | Bytes | SHA256 |
|---|---:|---|
| M7-012/pinkslip-call-crash-20261010-1 | 222004 | A856B19E7A23DE0529F82AA304F89834F8534D6835BE7739E36739C4B1127539 |
| M7-003/wager-goal-attack-owner-20261010-1 | 48789 | 91986E72A026FF634FB0CADAFE51EB22BBD8404B17DA483B97C346E4EB77E67F |
| M7-002/clean-owner-return-20261010-1 | 24334 | FAA0387F3C03EBA51D3CC362BD09CF133037E8BF04EE5206FFEA7B4F6DB740E0 |
| M7-004/sandbox-owner-return-20261010-1 | 23884 | 291208009FDB179B59EFC3FD08E0BAABEEED5C1AB89252745BD842BFF0B334B5 |

Each of the 11 sessions has the same private-key UNAVAILABLE, dismount COMPAT
and cross-thread critical-section warning described below. The 110 ordinal
resolution lines do not prove invocation. There are no DEBUG/TRACE lines or
device-loss/DRED markers. The sole fatal is the already diagnosed `822B88B8`
callback in the old WGC branch; repaired and isolated-test branches have none.

Seven existing texture-fetch warnings (six October 10), 11 missing-controller-DB
warnings and 77 developer-city missing-path opens remain classified observations,
not evidence for a renderer bypass. In particular, no invalid-fetch override is
admitted. `XeKeysConsoleSignatureVerification` has a DEBUG-only constant-zero
path without verification, so INFO logs cannot establish that it was unreached;
the guest interpretation is now bounded separately above.

## October 4 baseline

2026-10-04. Incremental review of the RTX5060 owner delivery, Ian Red Light,
Time Trial and Kenneth Hahn State Park Series returns. No newly reached stub is
demonstrated by these INFO-level logs. This does not establish absence of unsafe
success paths: DEBUG-only messages are filtered. M7-014 remained open at this checkpoint.

## Evidence and deduplication

Under `private/evidence/M7-003`, each earlier `logs-stock/mcla.log` is an exact
byte prefix of the last one. Primary rehashed and compared all bytes. There are
six distinct sessions, not seventeen independent runs obtained by summing the
cumulative returns.

| Return directory | Main log bytes | SHA256 |
|---|---:|---|
| delivery-owner-20261004-1 | 24227 | 80B96EB1ACF7A96A68F273AA9C230EECC5FDC6A148AF96CAEF8C4B664D14122E |
| delivery-owner-20261004-2 | 49085 | 2E68E8DFDCDD1272EE1C1818E723353D031AFA475C2B994F68A40920B29AB1F8 |
| ian-red-light-owner-20261004-1 | 61531 | 00CD077D1E7025341ABBC6CBA7001D40489ECFB868728353133D76BC04BC17FE |
| time-trial-series-owner-20261004-1 | 166859 | 4CEC1683AE0207BBA6B4ABFE98EF562DE0B1AD03A739ADF51C8C7FA920B68F45 |

Independent review searched 30 main/journal/tail files across these roots
(4/6/7/13 respectively); these contain duplicate sessions and snapshot excerpts.
Current-source dispositions below are consistent with the retained runtime
markers, not an independently reconstructed source-to-binary build proof.

## Actually reached behavior

Line numbers refer to the final cumulative main log.

- `XeKeysConsolePrivateKeySign`: six UNAVAILABLE markers at70/180/287/398/505/614.
  Current `xboxkrnl_crypt.cpp` returns the compatibility helper's NOT_SUPPORTED
  for valid pointers, preserves caller buffers and does not claim cryptographic
  success. Invalid pointers receive INVALID_PARAMETER.
- `IoDismountVolumeByFileHandle`: six COMPAT markers at71/181/288/399/506/615.
  Current `xboxkrnl_io.cpp` validates the XFile handle; a valid handle receives
  compatibility success while the host VFS keeps its runtime-owned mount.
- `RtlLeaveCriticalSection`: six explicit cross-thread compatibility warnings
  at102/210/316/428/537/646. This is an enabled exception with actual decrement,
  unlock and wakeup logic, not a stub or proof arbitrary cross-thread use is safe.

These markers are once per process, so they establish at least one occurrence
per session, not total invocation counts. Existing focused helper tests were
inspected, not rerun for this read-only audit. No return semantics were changed.

## Limits of the negative result

The independent search found no matches in those30 files for
`(?i)stub|PPC_UNIMPLEMENTED|unimplemented|invalid target|unsupported|no native implementation|fatal|device.remov|device.lost|DRED`.
No DEBUG/TRACE or XINPUTD_FF_AUDIT lines were present. The copied configurations
all specify INFO,16MB rotation and12 files; config SHA256 is
`E3CFC68317CA10DD16F7A85C2D3C2B7F180CA40423EBFAB4AEEBF1BEABDEC579`.
Adding another sink does not recover messages already filtered by the logger.

Sixty ordinal-thunk resolution lines represent ten exports per session, not
sixty calls. `kernel_module.cpp` emits these while allocating a thunk. In
particular, `XamInputGetUserVibrationLevel` still resolves to a generic stub;
resolution is not implementation or invocation evidence. Generic REX_STUB logs
WARN and leaves r3 unchanged, not a deterministic safe success result.

`XeKeysConsoleSignatureVerification` logs DEBUG and returns0 without validation;
its success comment did not establish the caller's interpretation. Its absence
from INFO logs cannot establish non-reachability.
STFS create/control retain WARN-level success stubs, but no reached marker is
observed. The historical deferred import list remains unproven. Force-feedback
thunk resolution likewise grants no physical wheel behavior credit.

These logs are not error-free: missing test-city files, texture-fetch warnings
and deliberate contract rejections exist. None alone proves a newly reached stub
or explains the owner's busy-area slowdown.

## Earlier observability follow-up and retained limit

Keep the explicit unavailable/compatibility dispositions. A runtime invocation audit needs
bounded once-per-process observability for filtered unsafe-success paths and an
artifact-bound route return. Do not enable unbounded debug logging or change
return semantics without a caller reproduction. No game launch, profile write,
SDK mutation, commit or milestone closure was performed by this audit.
