# M6-013 reached unsupported-operation coverage

## Decision

M6-013 passes as `reached-unsupported-surface-fixed-or-bounded-nonblocking`.
Accepted broad-play evidence and a fresh autonomous title-route probe found no
reached `PPC_UNIMPLEMENTED` instruction. Two imported operations had emitted
stub diagnostics during broad play; both now have explicit, tested behavior.

This is a reached-surface report, not a claim that every ReXGlue export is
implemented. Twelve remaining imported xboxkrnl stubs are
not-observed/deferred. At M6-013 closure, eight advanced `XInputdFF*` exports
were deliberately unadvertised under the M5-007 degradation policy; M6-015
subsequently implemented that bounded surface and advertises it only for
eligible wheel devices.

## Immutable result

- Accepted run: `20260819-200712-a4cc8715`.
- Result SHA-256: `279DA2FDEDF20D8C69D3DE6B5993A26AC625FBA9B624BF52BC3F800F8F1BC4ED`.
- ReXGlue SDK: `v0.9.0.29`, commit `5a7fc75713d1d43188b7574349f44a7e7923033d`.
- Focused tests: 2 cases / 5 assertions.
- Parser fixtures: 1 positive, 14 fail-closed negatives, 18 source checks.

The result re-hashes six prior accepted reports covering force-feedback
degradation, five consecutive races, all major city regions, the representative
garage lifecycle, the representative race-system matrix, and the two-hour
audio/device-recovery session. Raw logs, captures, saves, and host paths remain
private.

## Reached imports

`IoDismountVolumeByFileHandle` is reached after MCLA's cache-volume work. The
SDK now validates that the handle resolves to an `XFile`; invalid or wrong-type
handles return `X_STATUS_INVALID_HANDLE`. A valid call succeeds without
removing the host-owned VFS mount and emits one bounded compatibility marker.

`XeKeysConsolePrivateKeySign` is reached by a cache-signature path. ReXGlue has
no physical console private key, so valid inputs return
`X_STATUS_NOT_SUPPORTED`, null inputs return `X_STATUS_INVALID_PARAMETER`, and
the caller buffer is preserved. The observed caller ignores the return and
does not depend on fabricated signature bytes. This is explicit
unavailability, not console-private-key cryptography.

The accepted trace has exactly one module launch, unavailable-key marker,
compatible-dismount marker, and third-successful-present marker in that order,
followed by controlled external close, `Execution complete`, and hard exit. It
contains no legacy reached-stub text, generic stub call, `PPC_UNIMPLEMENTED`,
invalid target, guest crash, fatal, device-removal, or DRED marker.

## Repaired runtime targets

Six runtime-discovered indirect targets from earlier broad play are present as
exact non-force function boundaries, generated bodies, initialization entries,
and dispatcher registrations:

| Start | Exclusive end |
|---|---|
| `0x8220B810` | `0x8220B834` |
| `0x82262320` | `0x8226233C` |
| `0x82264760` | `0x82264770` |
| `0x82264770` | `0x82264780` |
| `0x822C9FE8` | `0x822CA04C` |
| `0x82554080` | `0x8255409C` |

## Deferred not-observed surface

The report keeps twelve imported xboxkrnl stubs explicit and deferred because
no call was observed in the accepted broad-route logs: `__C_specific_handler`,
`StfsControlDevice`, `StfsCreateDevice`,
`XeKeysConsoleSignatureVerification`, `IoDismountVolume`,
`IoInvalidDeviceRequest`, `IoCompleteRequest`, `ObIsTitleObject`,
`IoCheckShareAccess`, `IoSetShareAccess`, `IoRemoveShareAccess`, and
`RtlUnwind`. Some historical stub diagnostics are debug-level, so this is not a
claim that every route made every possible call observable. Static import or
direct-call presence is also not runtime reachability.

The immutable M6-013 result truthfully records that its eight resolved
`XInputdFF*` exports were stubs and the capability was withheld at that time.
Current-source closure review separately requires all eight explicit hooks,
zero matching stubs, one wheel-only `X_INPUT_CAPS_FFB_SUPPORTED` advertisement,
and the 64-slot bound introduced and physically scoped by M6-015. This does not
retroactively add advanced-force evidence to the historical M6-013 run.

M7-014 still owns the final unsupported-operation audit after full campaign and
Complete Edition content coverage. Any newly reached stub, unsupported PPC
instruction, or invalid target must reopen this classification.
