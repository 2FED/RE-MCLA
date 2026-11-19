# l114 SDK: the resolve-only deferred mode of the draw extent bound (KI-084) (2026-11-19)

**SDK READY: `private/l114-sdk-install`** (CMake version `0.20.0.21-dev.g389d3bd`). Local commit `389d3bd`
(`389d3bda76d371ac77b9c0bd62df06c1e8cb3005`) on the branch `mcla/m11-l114` (parent `9595fc4` = l113), worktree
`private/l114-sdk-wt`; not pushed, no tag. Implemented by an Opus subagent for the primary agent.
**Built and unit-tested only: nothing has run in the game, there is no physical PASS.** Spark not
used: a small deterministic change checked by unit tests, plus reading the transfer shader source.

Licence: no code was copied; the new header and tests are our own.

## 0. Result

| Item | Status |
|---|---|
| Deferred mode | **shipped**: new int `d3d12_draw_extent_deferred_mode` (default **1**, hot); `d3d12_draw_extent_deferred_claims` (bool, default true) stays as the master switch (false = mode 0) |
| Mode 1, resolve-only | an owed claim is completed only before a resolve copy reads the tiles, and before a resolve clear whose chain is lossy; every later Update claim and a newer leave keep the direct path (as l111); what mode 2 would complete is counted (`would_claim`, `would_leave`) |
| Mode 2, exact | the l113 rule, unchanged |
| MSAA-hop question | **sample addressing exact** (1x <-> 4x is a bijection, the chains return every sample, unit-tested); **value path for depth not proven**, so mode 2 is unchanged (section 2) |
| Tests | 3 new `[l114]` cases in `extent_below_audit_test.cpp`, 1 in `transfer_sample_map_test.cpp`; the l113 field-string test updated |

## 1. The mode

- Why: on l111 (bound on, nothing deferred) the every-frame readers (draw 11,314, transfer 28,591,
  owner 7,667 reads of left tiles per minute in session 75035b51) showed no visible defect in an
  hour of play; the only intermittent reader in the audit is the resolve (44-54 per minute), which
  matches the intermittent white square. The exact rule of l113 would redo 7.75 M of the 9.85 M
  left tiles of the top table, i.e. give back the gain. Mode 1 targets the observed defect and keeps
  the gain; mode 2 stays available as the fallback.
- Cvars: the effective mode is `d3d12_draw_extent_deferred_claims ? clamp(d3d12_draw_extent_deferred_mode, 0, 2) : 0`,
  read at every bounded draw and latched into each left range (`BelowEntry::exact`). A bool cannot
  become an int cleanly (the l113 owner key writes `false` to it), so the mode is a new int; the
  l113 key `xfer_extent_deferred_off` still gives mode 0.
- `extent_below_audit.h`:
  - `Leave(..., real, complete, exact_claims)`; mode 1 ranges have `exact = false`.
  - `OnClaim`: a lossy chain of a real range completes only when `exact` (mode 2) or the claim is a
    resolve clear (it copies the tiles outside the cleared rectangle into the clearing target);
    otherwise it is counted `would_claim` (with tiles) and the claim takes the tiles directly. The
    range still ends on any claim.
  - `Leave` over a real range of another clearer with a lossy chain: mode 2 completes A -> K1 and
    owes the new range from K1 (l113); mode 1 counts `would_leave` and owes the new range from A.
  - `OnResolve`, capacity, full dump and snapshot restore: as in l113 in both modes.
- Line: `MCLA_XFER_EXTENT ... deferred=on:,resolve:,claim:,exact:,leave:,overflow:,flush:,restore:,missing:,would_claim:,would_leave:,xfers:,mode: deferred_tiles=...`
  (two classes and `mode:` appended; `claim:` counts the completions made, i.e. lossy resolve
  clears in mode 1; with the verify the classes count what mode 2 would do, as in l113).
- What mode 1 restores: every resolve copy reads exactly what the unbounded path would dump (the
  l113 argument). What it does not: a later draw-time reader R of a lossy chain gets A -> R instead
  of A -> K -> R, exactly as in l111.

## 2. The MSAA-hop question

The ownership transfer pixel shader (`render_target_cache.cpp`, the DXBC built for each transfer
key; "Same BPP, 4x -> 1x" and the common "1x -> 4x" source Y / sample code) addresses, for the same
bits per sample:
- 4x -> 1x: destination pixel (x, y) loads source pixel (x >> 1, y >> 1), sample
  (x & 1) | ((y & 1) << 1);
- 1x -> 4x: destination pixel (x, y), sample s loads source pixel ((x << 1) | (s & 1),
  (y << 1) | (s >> 1)).
Coordinates are host pixels in the EDRAM tile (resolution scale included); one sample is loaded
(`Load` / `Texture2DMS.Load`), no averaging and no replication. `transfer_sample_map.h` transcribes
the instructions; `transfer_sample_map_test.cpp` checks at scales 1 to 3 that both maps are
bijections over a tile and inverse to each other, so 4x -> 1x -> 4x and 1x -> 4x -> 1x return every
sample to its place.

**Answer:** the sample addressing of an MSAA-only chain is exact. The value is exact only if the
same-format copy is: colour formats with 16- and 32-bit channels go through UINT views
(`GetColorOwnershipTransferDXGIFormat`), 8-bit formats through the draw format; depth (the frequent
rows are D24FS8, `d1m4p12b0 <-> d1m1p12b0`) goes through the depth output and the 24-bit
float conversions and host-depth sources, which were not proven here. So mode 2 is unchanged.
Proving the depth path would make the frequent rows `d1m4p12b0 > d1m1p12b0 > *` exact even in
mode 2.

## 3. Tests, build, install

- `extent_below_audit_test.cpp` `[l114]`: a resolve completes while a lossy transfer and a lossy
  owner claim do not (counted `would_claim` with tiles), an exact claim, a lossy resolve clear
  completed, the event `deferred=done`; a newer lossy leave not completed (`would_leave`, the new
  range owed from A, a resolve of it completes A -> K2); the exact field string with `mode:1` and
  the exact mode completing the same claim.
- `transfer_sample_map_test.cpp` `[l114]`: the bijection and both round trips at scales 1-3.
- Build: fresh `private/l114-sdk-b` (the l113 options), output `private/l114-sdk-o`; third-party
  trees from `private/l113-sdk-wt`. Normalized warning set (full build): 211, **0 new, 0 gone
  against l113** (the l113 baseline without its fixed entry).
- `git-clang-format` against `9595fc4` and `clang-format -i` on the new files: no changes;
  `git diff --check`: clean.
- ctest: **2380 of 2380 passed** (l113 2376 + 4 new) three times in a row after the full build and formatting (`ctest-a1..3`); the `physical_mapping_test.cpp:184` race did not occur.
- Commit `389d3bd`, parent `9595fc4`; 9 files, 267 insertions, 17 deletions (`extent_below_audit.h`, `transfer_sample_map.h` new, common `cache.h` / `cache.cpp`, D3D12 `render_target_cache.cpp` / `.h`, the tests and their CMakeLists). Not pushed, no tag.

| bin file | Bytes | SHA-256 |
|---|---|---|
| rexruntime.dll | 11776000 | DBCACC5D5E8FCD23CA8260B5CC027712035AE703B7683C8F031E3712D9C0C4C2 |
| rexgpu-xenos.dll | 3861504 | 0ECBF770446EC1BF1573C853145998F8D3A5ED04021F884E495E5A7D208CBD84 |
| TracyClient.dll | 246784 | A88F18ECF13A11A8804CD2016ADADCD8AD8B8F900AE68D0F6C7EA3518280B886 |
| rexglue.exe | 4113920 | AE0E29236D69E5FCA54B31EB2A7E56945671A96E33F9D1CA271C43EAB53DAD17 |

rexruntime.dll and TracyClient.dll have no source change but new hashes (fresh build tree, version
string).

**ABI:** `RenderTargetCache` and `D3D12RenderTargetCache` gained members; the host must be rebuilt.
New SDK cvar `d3d12_draw_extent_deferred_mode` (int, 1, hot).

## 4. Expected performance

- Defaults (bound off): unchanged.
- pc/extent-new (bound on, mode 1): l111 transfers plus the resolve completions (44-54 resolve
  reads per minute in the l112 audit, about 40 k tiles per minute) and the lossy resolve clears
  (12 per minute): expected at the l111 level (13.4 ms GPU per frame, 65-75 fps in races).
- pc/extent-exact (mode 2): close to the bound off (l112 level), by the l113 classification.

## 5. Risks

- Mode 1 accepts the l111 behaviour for draw-time readers; if a white square still appears, a
  draw-time reader is involved and mode 2 (pc/extent-exact) is the test.
- No game run; KI-085 unchanged (the heavy verify arms stay out of races).
- Scratch: `%LocalAppData%\Temp\claude\C--BDU-MCLA-Recomp\l114-work`.
