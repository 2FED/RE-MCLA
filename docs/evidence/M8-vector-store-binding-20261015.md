# Vector state store witness and exact parent association

October 15, 2026. The portable journal, strict decoder and exact-ID join extend
M8-001 to the short `8259E768` load, computation and store sequence. Offline and
native synthetic gates pass. The new observer has not collected gameplay;
physical units, vehicle identity, correct simulation speed and 60 FPS remain
unproved. M8 remains 4 complete, 6 deferred, 19 total, YELLOW.

## Boundary selected from owner7

[Owner7](M8-owner7-gameplay-return-20261015.md) supplies 91 parked-world source
records associated with 558 unique actual target bodies. Their slot28 calls
request this short function. The new witness stops at its state store rather
than extending instrumentation through another broad call chain.

The original body loads vectors at object offsets 256 and 272, computes the
second multiplied by incoming V1 plus the first, loads an auxiliary vector at
288, scales V1, stores the computed vector at 256, and invokes its original
tail function. These are observed offsets and operations, not established
position, velocity or seconds. A zero-change store remains valid evidence.

## Observation contract

Entry retains the exact active slot28 CallPre, lane-global event index and full
saved parent ancestry. Subsequent events copy already-loaded or computed
register bits after the original operation. The witness adds no guest-memory
read or floating-point calculation. The original tail runs once and propagates
its original exception. LastError, MXCSR and PPCContext remain unchanged by
observation.

The ordered grammar is entry, old load, rate load, computed value, auxiliary
load, scaled value, successful store, tail entry/return and exit. The store must
contain the exact captured computed bits; the tail must receive the captured
scaled bits. Raw vector lanes retain host storage order. Effective addresses
preserve original low32 wrap and 16-byte alignment.

Eight thread lifetimes each have 4096 entries, 65536 events and depth eight.
Rejected entries are bounded to 256 metadata headers; JSON is capped at 128 MiB.
The store journal freezes before its parent target. Pending and failed states
remain explicit; later completion never backfills the cut.

Calls outside this selected parent/site/target are counted separately as
unselected, not treated as observed selected work. The exact-ID join still
requires one completed child for every applicable selected slot28 call.
Missing bodies exclude the entire source group; orphan, duplicate and
contradictory associations block globally. All records must fit the exact
parent call bracket, both cuts and the original source's closed frontier.
Other inner calls remain unknown. No nearest-time matching or arithmetic,
physical, total-work or FPS proof is inferred.

## Tests and independent review

- Portable core: 220 checks; native adapter: 177 checks.
- Extended actual-target accessor: 269 checks. Real target/store adapter
  integration: 41 checks, with only the upstream work association synthetic.
- Exact frozen body differential: 2832 checks each at O2 and O3. Full context,
  entry/tail/final flags, all mapped memory, tail count and exceptions agree
  between original and instrumented bodies. Entry, computed-register and
  post-store mutations fail 576, 288 and 144 checks respectively at each level.
- Strict decoder: 24 tests; full-chain join: 16 tests; exact transformer: 3
  tests. Combined target/store suite: 78 tests. All three AST rule tests and
  the AST scan pass. Independent reviews accept core, native adapter,
  differential, decoder and join without a blocking finding.
- All 25 actual native serializer fixtures replay. The first fixture set has
  one invalid construction: it labels a saved nonzero parent as an invalid
  accessor result. The real accessor clears that payload. The decoder correctly
  rejects it; corrected fresh fixtures pass without relaxing the reader. The
  original set remains preserved.
- An impossible empty claimed lane with an invented model fault reached
  evaluation in a new regression. RED/GREEN hardening now rejects it before
  evaluation. A genuinely empty inventory remains inconclusive.

Private test directories use `vector-store-*-20261015-*`; the accepted
serializer set is `vector-store-serializer-20261015-2`. Native source and the
exact eight-seam generated-code transformation remain separate experimental
integration, outside the portable prerequisite commit. No game body, assets,
save data or binaries are published.

## Build identity and next gate

`private/host-store-overlay-20261015-1` links 85 objects: 81 reused, three
replaced and one added. All four compiled objects reproduce except their COFF
timestamp; 587 dependencies and two tool binaries are pinned. System-library
resolution is not claimed hermetic.

Host: 53479936 bytes, SHA256
`527AFF591B0B2268130D3756EA52BD1A761D182DFE84EA3472A5F06FEEBE4BF4`.
Unchanged helper: 207360 bytes, SHA256
`293D4AC754A83B32252540E3E3A464F321C20758121F96192A0CA5199EAEC343`.
The inherited native version remains 0.20.21.0; repository version 0.20.30.0
identifies this portable prerequisite, not the executable. Stock timing and
VSync remain unchanged.

Next is separately reviewed owner8 preparation using the same historical save
copy and READY boundary. MAIN and normal l12 remain untouched. No game run,
60 FPS promotion or milestone closure follows from these synthetic gates.
