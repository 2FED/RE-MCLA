# Short capture profile for the timing comparison

October 15, 2026. A separately identified 31-slot profile passes native scheduler,
serialization and Python source-to-store tests. It reduces the detailed journal
window from 10.1 to 4.1 seconds without changing observer capacities or guest
hooks. Gameplay collection with this profile remains pending.

## Fixed contract

Only two profiles exist. `legacy91_10hz` remains the default and retains the
`mcla-guest-source-scout-v1` schema. `short31_10hz` uses the distinct schema
`mcla-guest-source-scout-short31-v1`, with the same exact field set. There are no
arbitrary slot-count, frequency or buffer-size arguments.

Both wait 40 seconds after source readiness following owner READY, then sample
at 10 Hz. For short31, relative to readiness R:

- Captures are due at R+40.0 through R+43.0, inclusive.
- The last slot closes at R+43.1.
- Detailed observer and swap windows are R+39.0 through R+43.1, end exclusive.
- Maximum raw image bytes are 114278400, versus 335462400 for legacy91.

The 91-slot backing layout is unchanged; only the selected slots are collected
and serialized. The diagnostic buffer ceiling remains 512 MiB, not a bound on
whole-process memory. Every requested slot, failure and terminal suffix stays
in the receipt. No search, retry or post-hoc subset is permitted.

The init-only native `mcla_source_scout_profile` is parsed once for the run and
rechecked alongside the token and enable flag. The same profile determines
collection, serialization and all detailed observer endpoints. The separate
legacy timer-observer build rejects short31; that older decoder is not expanded.

## Admission and decoding

Public metadata admission, pixel decoding and publication analysis default to
legacy91. A fresh short supervisor must explicitly request short31 at both its
terminal-result and post-exit read gates, before reading images. Consumed owner8
still rejects a short receipt before inventory, sidecars or pixels.

Pure dependent joins use a detached schedule resolved from fully validated
finite-schema metadata. Swap, phase, owner-readiness and deeper source-to-store
joins derive counts and endpoints from it. Owner READY retains its original
180-second journal and 120-second deadline; shortening capture does not shorten
the owner's loading allowance.

## Verification

Primary rerun: 299 Python tests pass, including all legacy cases and 13 new short
profile cases. The full synthetic source-to-store chain returns 31 candidates,
no blockers or exclusions, and unchanged false proof flags. Wrong schema,
profile, count, raw budget, final slot, image inventory and sidecar endpoints
reject. Callback mutation cannot alter the detached schedule. AST scan and all
three AST rule tests pass.

Native injected tests, with no game, GPU or window:

- Scout scheduler, serializer and configuration: 1086 checks, zero failures.
- Swap: 133 legacy and 73 short checks, zero failures.
- Updated phase/swap integration: 194 legacy and 134 short checks, zero failures.

Four actual native wire serializations also pass independent Python decoding:
91 and 31 complete slots, plus incomplete variants with two collected images,
one failed call and 88 or 28 unattempted slots. The synthetic BMP is uniform
RGB (17,34,51), not a game image. Sixteen metadata mutations reject before any
pixel callback. Compiler warnings are limited to existing NOMINMAX and
WIN32_LEAN_AND_MEAN macro redefinitions in the filesystem helper.

Native outputs are frozen in `private/short-profile-tests-20261015-1` and
`private/short-profile-wire-20261015-1`. The initial phase/swap test had only the
legacy case; the second root contains the extended two-profile integration.
Wire final receipt SHA256:
`081AFCB44227ABC7249DD8AE701E46F8DF1F8C53D7BC2DB7E440CB8795737E99`.
Short complete wire SHA256:
`9FA0B3BAFFD6EC5CE7CA0CD1C3D5D40FDCF802C3EC13C93FF63FA78292DE256D`.

## Isolated native build

`private/host-short31-overlay-20261015-1` derives from the accepted owner8 store
overlay. Both original changed objects were reproduced first, equal except
COFF timestamp bytes. The 129 linker rows change only two object paths:
83 of 85 objects are reused, scout and swap are replaced, and none are added.
An independent audit verifies all 20250 pinned inputs, totaling 1866237012 bytes.

Host: 53488640 bytes, SHA256
`345D98BE09C5BBA3F3CDBBB494B7EA2A7C95EEE5989AA98183A0BFB8D5105E08`.
Crash helper remains 207360 bytes, SHA256
`293D4AC754A83B32252540E3E3A464F321C20758121F96192A0CA5199EAEC343`.
Native version remains the inherited 0.20.21.0. The toolchain resolver was not
pre-pinned by these builders, and system dependencies are explicitly
nonhermetic. No game was launched; build writers are consumed.

This checkpoint commits the portable decoder contract and its tests. Native
integration remains in the existing private diagnostic worktree/staging, not
the normal l12 package. MAIN and its save lineage are untouched.

## Next discriminator

At a hypothetical 60 Updates/s and the observed 13 timing rows per Update,
4.1 seconds would require about 3198 of 4096 timing entries. This is headroom
estimation, not an overflow guarantee. Every runtime overflow still rejects.

Next prepare a fresh matched stock/interval1/stock comparison using the same
short collector and unchanged stock timer. The candidate must change only the
reviewed presentation selector, not reuse September's binary or its optimizer
allowlist. All 31 frames require integrity and scene review. Neither this
short-profile gate nor a later cadence difference proves correct simulation
speed or stable 60 FPS. M8 remains 4 complete, 6 deferred, 19 total, YELLOW;
M8-001 stays open.
