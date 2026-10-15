# Interval1 selector replay

October 15, 2026. The presentation-selector candidate passes an exact-body
synthetic comparison at O2 and O3. The candidate changes the selector-2 payload
from 2 to 1 while preserving the stock timer. It has not yet established correct
gameplay speed, display cadence or 60 FPS.

## Candidate boundary

The private generated TU27 source is 2086797 bytes, SHA256
`4A78050817638AFB10543505DC09E1549FD075855300360CBBECFCD6E90F14A1`.
Only source byte 720947 changes from ASCII 2 to 1, at guest instruction
`82419AA0`. Original guest-PC writes and comments remain intact. Candidate SHA256:
`4F60A2F380E821E314621A7EFD99066E25228D55E5203CFB1F57FAAA1C4D6734`.

The original object reproduces exactly except COFF timestamp bytes 4, 5 and 6.
The candidate object has the same 954875-byte size, SHA256
`1A83DE7426E2A27894AD03FD2386051565962C6C5C46779BC7AFC6B3AB7C4E3C`.
Independent inspection accounts for all 185 changed bytes: 174 in the target
function, one unwind-range endpoint byte, eight section checksum bytes, one
internal label byte and one timestamp byte. All 8045 relocations match, as do
205 of 207 section payloads. Other functions, imports and unwind programs are
unchanged apart from the stated range endpoint. The compiler reuses the existing immediate-1 block, shortens the
target tail by five bytes and retains all eleven guest-PC writes.

These are fresh identities and an explicit optimizer-difference review, not
reuse of the historical September binary or its allowlist.

## Synthetic comparison

The private fixture contains the exact 8086-byte function twice, with only
function-definition renames and the single literal change. The public harness
contains no proprietary function body. Memory accesses are bounded synthetic
big-endian reads/writes; every other callee is an explicit mock.

Each optimized executable compares 32768 original/candidate cases:

- Eight selector values, all 128 packed low-bit values, eight incoming R4
  overlays and four buffer/refill states.
- 28672 non-selector-2 cases remain identical.
- 3072 selector-2 payloads change; 1024 are masked by incoming OR bits.
- Each variant executes 12288 expected refill calls.

The comparison covers the entire 64 KiB synthetic memory, call order and
arguments, guest breadcrumbs and every named PPCContext field. An independent
SDK enumeration confirms all 211 declared members: 2648 semantic bytes, with
only 40 padding bytes excluded. Every semantic byte has a corruption control;
padding-only mutations must compare equal.

Both variants' final R7 and every trace R7 from the unique submission onward
must match their independently expected payload before normalization. Four
negative controls corrupt late R7 or post-submit trace R7 while leaving the
submission and memory intact. Both validators reject them. Six memory-boundary
controls reject, and an actual mocked-submit mutation returns exit 1 after the
full clean matrix passes.

## Results and preserved failed attempts

Accepted output roots:

- `private/interval1-selector-replay-o3-20261015-3`
- `private/interval1-selector-replay-o2-20261015-1`

Both baseline JSON files are byte-identical, SHA256
`6B7FF62401FFA8A18E50482D1B37A6B2187811C0B4BF418A090E923D4AFCBC07`.
Both mutation reports are byte-identical, SHA256
`48306BE79340878FFBAAAA219F612815FB70A3F4601FFA0E7A26915A9F8987CC`.
Reviewed public test source SHA256:
`DF365688CFF6DDA4EC1D533883DEFEB5695F8A85EABCB6641AE3BDB5B2A2C366`.
Independent review verifies all 19796 pins and 1713923829 bytes per build;
the SDK dependencies are frozen, while system-library hermeticity is not claimed.

The first O3 output is not accepted: its PowerShell mutation argument handling
returned exit 2, and independent review found that normalization could hide
late register corruption. The second O3 output passed the full matrix but
failed the newly added late-payload test. Its generic error did not distinguish
the clean precondition from corruption-control rejection. Replacing raw
context-object comparison with complete named-state comparison removes a
padding-dependent test risk; the precise historical failure cause is not
claimed. Both failed output roots remain preserved.

AST scan and all three rule tests pass. Owner9 supervisor regression tests also
pass all 66 cases; they are separate from the selector experiment.

## Runtime boundary

The isolated linked candidate is
`private/host-short31-interval1-20261015-1/bin/mcla.exe`, 53488640 bytes, SHA256
`2F77716137F1F0BC4AC926D7227ADDF413DDF8E032E01132BACC33677689F915`.
Independent final-link review verifies all 103 pins (321265386 bytes), 129 response
entries and exactly one TU27 replacement among 85 objects. The final PE differs
from stock by exactly 177 bytes: the same 174 target-code changes, one runtime
function endpoint byte and two timestamp bytes. All seven section headers,
21 import descriptors and 702 imports match; every other runtime-function entry
and the complete rdata/data/tls/resource/relocation sections match. The crash
helper is unchanged. This candidate has not been run.

Owner9 is the fresh stock A1 control using the separately reviewed fixed
`short31_10hz` profile. It does not contain this interval1 candidate. All 31
requested frames and sidecars must pass integrity and scene review before a
matched candidate comparison. The normal l12 build and MAIN save remain
unchanged. M8 stays 4 complete, 6 deferred, 19 total, YELLOW; M8-001 remains open.
