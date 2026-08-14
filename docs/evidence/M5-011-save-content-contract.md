# M5-011 save/content persistence contract

Date: 2026-08-14

Decision: `save-content-prerequisite-pass`

## Result

The exact MCLA content surface is implemented rather than stubbed:
`XamContentGetDeviceData`, `XamContentGetDeviceState`,
`XamContentCreateEnumerator`, `XamContentCreateEx`, and `XamContentClose`.
The title also has twelve direct `NtWriteFile` call sites backed by the concrete
kernel write implementation.

Immutable M5-002 evidence supplies the physical read-side prerequisite. The
pinned save enumerates, mounts as writable `save0:`, opens three times, reads
successfully, and unmounts without an archive or path failure.

ReXGlue v0.9.0.21 closes the creation-side false-success gap. A failed content
header write is now returned to the guest; the new mount and package are rolled
back and the disposition is reset. Header serialization checks both writes and
close and removes a partial header before returning access denied.

The focused `[system][xam][content]` suite passes 2 cases and 16 assertions. It
proves that saved-game metadata survives reconstruction of `ContentManager`, is
still enumerable, and that a truncated header is rejected.

## Scope

This is implementation and prerequisite evidence. The accepted gameplay routes
have loaded the pinned save but have not reached a native race-result write.
Consequently this report does not claim that an actual result has survived a
process restart. M5-012 owns that physical write, results transition, restart,
and return-path proof.

Accepted private result:
`private/evidence/M5-011/20260814-181120-64104b37/result.json`, SHA-256
`11B9F82B92130575EA082A088357EA0D4E73D760F2A37633337815CEAD99A07D`.
Raw build and test logs remain ignored under `private/evidence/M5-011/`.
