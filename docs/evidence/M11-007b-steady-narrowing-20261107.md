# M11-007b: steady-key narrowing (sticky a/b exclusion, mip-chain shape filter, single-key cap) and fast delivery (SDK)

Implementation by an Opus subagent on 2026-11-07 as SDK commit `4bb8c02` on local branch `mcla/m11-steady2` (parent `c526cb3`); accepted by the primary agent as code: ctest 2099/2099 (4 skipped), no new warnings. Installed as `private/l75-sdk-install` 0.20.0.6-dev.g4bb8c02: rexruntime.dll 594e60b79804b3cbc16a4ba3bc9b2b92a0def10f283869b64bcf268a01f3e376, rexgpu-xenos.dll be9725c933c50e741e7bf7d45ea66e5ed876604cf9aa735c1128a359c16aee09, TracyClient.dll 93650999b88d7faefa807bdc8921e09491e2e84dd39164e274a0d4a5e280a318. Parser note: the old steady_key end `reason=shape` is now `reason=not_c`; `shape` means the new chain filter.

---

# M11-007b: narrowed steady-key exemption + fast delivery (l75 SDK, draft)

Draft by the implementation subagent, 2026-11-07. Not accepted, not owner-tested.

- SDK worktree `private/l75-sdk-steady2-wt`, branch `mcla/m11-steady2`, parent `c526cb3`
  (`mcla/m11-xma-trim`, KI-071 step 3), commit `4bb8c02` (local only, not pushed).
- Build `private/l75-sdk-steady2-b` (Ninja Multi-Config, Release, same flags as l75-sdk-xma3-b,
  `-ffile-prefix-map` to `./third_party/rexglue-sdk`), output `private/l75-sdk-steady2-o`.
- Installed over `private/l75-sdk-install` (replaces the c526cb3 install; reconfigured first so the
  version reads the new commit). DLL hashes below.
- Inputs: `docs/evidence/M11-007a-steady-ab-l74-20261107.md` (section 5 recommended rule and the
  flicker plan), `docs/evidence/M11-007a-steady-key-exemption-20261106.md`, `docs/M11-priorities.md`
  "M11-007a analysis".

## Rule (d3d12_readback_sync_steady_exempt on)

A resolve is a candidate exactly as in M11-007a (rule c alone, run >= 30). A candidate is exempt
only if it passes, in this order:

1. **sticky**: its key (address, length, destination format, width x height) never matched rule a
   or rule b in this session. Every evaluated resolve that matches a or b records its key
   (`ReadbackSyncStickySet`, bounded at 4096 keys; a saturated set excludes every key). The set
   and the cap state are cleared only in `ShutdownContext`, together with `readback_buffers_`
   (the read-back cache is never torn down elsewhere; eviction of a key after 60 idle frames does
   NOT clear its sticky flag). Recording happens whenever the rules are evaluated (also with the
   switch off) so that turning it on later sees the history; this is internal state only.
2. **shape**: no mip-chain neighbour among the live read-back keys (`readback_buffers_`, whose
   entries now carry the shape of their last evaluated resolve). `IsReadbackSyncChainNeighbour`:
   same destination format, and either (i) the ranges overlap (any size or EDRAM base; the
   1024 + 512 same-address livery layout), or, with the same EDRAM source base, (ii) the next
   SMALLER level after the key: starts exactly at base+len with half width and half height, or
   (iii) the next LARGER level before the key: ends exactly at base with double width and height.
3. **cap**: at most one distinct key passed 1-2 in this frame and in the previous frame
   (`ReadbackSyncSteadyCap`). Otherwise none is exempt; one `kind=steady_cap event=start` line per
   episode.

Each refusal leaves the resolve forced by rule c (the l74 base behaviour for that key).

### Why 039CA000 stays exempt and the 05368000 chain does not

The literal symmetric definition "half size AND adjacent AND same format AND same src_base" would
make `039BA000` 128x128 (len 64 KB, ends exactly at `039CA000`) a neighbour of `039CA000` 256x256
and remove the whole gain. A mip chain is laid out from the largest level down, each level directly
after the previous (larger) one; so relative to a key, the half-size level is on the base+len side
and the double-size level on the base side. A half-size key that ENDS at the key is not a level of
its chain (the level before a 256 level would be a 512). Hence the direction rule:
- `039CA000` 256: `039BA000` 128 ends at it but is half size (not double) -> no neighbour; nothing
  at `039CA000+0x40000 = 03A0A000` of fmt 6 (the round-robin src_base 224 tiles are fmt 2) -> exempt.
- `05368000` 512 (len 1 MB): `05468000` 256 starts at base+len, half size -> neighbour.
- `05468000` 256: `054A8000` 128 after it (and `05368000` 512 before it) -> neighbour.
- `054A8000` 128: `054B8000` 64 after, `05468000` 256 before -> neighbour.
- `054B8000` 64: `054A8000` 128 ends at it with double size -> neighbour.
- With the 1024 key at `05368000` live, the 512 level also overlaps it (same format) -> neighbour.
Overlap is checked for the same format only (as specified); rule a already forces any overlap of a
different length in the composite family.

Limit: the cap is decided per resolve, so in the first frame of an episode the first key's earlier
resolves of that frame are already exempt; from the second key on, every resolve of that frame and
of every frame that follows a two-key frame is refused.

## Fast delivery (d3d12_readback_sync_steady_fast, default true)

Only with the exemption on, `readback_resolve=some`, and for an exempt (not forced, not miss)
resolve: `PlanReadbackResolve` gets `steady_fast` and `previous_slot_complete`. If the previous
slot of the key is known complete (written, not elided, `written_size == len`, its submission <=
the completed fence value; `submission_completed_` or a non-blocking
`submission_fence_->GetCompletedValue()` poll when that is not enough; never a wait), the resolve
copies that slot into guest memory now (`steady_fast_copies`, `steady_fast_kb`), so the guest copy is
at most one resolve old instead of 0-3 frames. Otherwise it keeps the normal `some` cadence (every
4th resolve, l74 behaviour, no wait) and counts `steady_fast_skipped`. Its own GPU copy is always
recorded (not elided by `d3d12_readback_elide_unread`) because the next exempt resolve reads it.
Ignored in `fast` (already copies every resolve) and `full`.

## Logs

- Minute/mark line, after the M11-007a steady fields, while the exemption is on (or any of these
  counted): ` steady_fast=<0|1> steady_sticky_excluded= steady_shape_excluded= steady_cap_skipped=
  steady_fast_copies= steady_fast_kb= steady_fast_skipped=` (counts of candidate resolves refused per
  filter; fast copies, KB, and exempt resolves whose previous slot was not complete).
- `kind=steady_key event=end reason=` now also `sticky|shape|cap`. The former `reason=shape`
  (rule c no longer matched) is renamed `reason=not_c` to keep `shape` for the new filter.
- `MCLA_READBACK_SYNC v=1 kind=steady_cap event=start frame= draw= base= len= keys=2 suppressed=`
  on the first refusal of a cap episode (paced 2/s, burst 16).
- With `d3d12_readback_sync_steady_exempt` off: no candidate, no filter, no plan change, no new
  field or line (byte-identical output; only the sticky set and the per-key shape are updated).

## Expected on the owner A/B (PC, South Central Tour)

- One `event=exempt` line for `039CA000` ~30 frames after the title/menu start; the livery chain
  keys never get an `event=exempt` line; in the showcase minute `steady_shape_excluded` ~ (4 keys
  x their runs past 30) and/or `steady_sticky_excluded` > 0, `steady_exempt_keys=1`.
- `steady_fast_copies` ~ steady_exempt minus `steady_fast_skipped`; `steady_fast_kb` ~ 256 per
  copy; memcpy cost ~256 KB/frame. Owner question: building/sign flicker yes/no.

## Tests

`[readback-steady]` new cases: chain-neighbour predicate (direction, size, format, src_base,
overlap, wrap); shape filter on the l74 key sets (039CA000 with 039BA000 + src_base 224 tiles stays
exempt; 05368000/05468000/054A8000/054B8000 all refused; 1024 overlap; unknown shape ignored);
sticky set (identity, saturation, clear) and a frame sequence (rule b once, then 40 frames of rule c
alone: never exempt, sticky from run 30); filter order (sticky before shape before cap, refused
candidates do not reach the cap); single-key cap episodes; end reasons and the new report/line
formats; planner fast delivery (complete -> copy every resolve, incomplete -> cadence, own copy
always recorded, miss/forced/fast/full untouched, elided previous slot -> guard as before) and an
exhaustive check that `previous_slot_complete` alone changes nothing.

ctest: 2099 passed, 4 skipped, 0 failed of 2103 (baseline c526cb3: 2090 passed, 4 skipped; +9 new cases).
Warnings: none new (normalized unique warning set identical to the l75-sdk-xma3-b build log, 222 entries).
clang-format: changed lines clean (clang-format-diff empty).

## DLL hashes (l75-sdk-install)

version.h REXGLUE_VERSION_STRING "0.20.0.6-dev.g4bb8c02"
- rexgpu-xenos.dll sha256 be9725c933c50e741e7bf7d45ea66e5ed876604cf9aa735c1128a359c16aee09
- rexruntime.dll sha256 594e60b79804b3cbc16a4ba3bc9b2b92a0def10f283869b64bcf268a01f3e376
- TracyClient.dll sha256 93650999b88d7faefa807bdc8921e09491e2e84dd39164e274a0d4a5e280a318
