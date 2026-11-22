# M11 volunteer hotfix candidate l120v (KI-088): preparation up to signing (2026-11-22)

Preparation by an Opus subagent for the primary agent. l120v = the published volunteer build l117v plus
ONE fix: **KI-088** (vertex-buffer request skips do not check range validity: stale vertex data), measured
live on the Deck in l119 (`M11-l119-deck-analysis-20261122.md` section 3: 0.15-0.32 invalid in-sync skips
per frame at two tiles, every `MCLA_VB_STALE` line slot 95). Built, unit-tested, scanned and staged as input;
**not signed, not staged as a package, not published, not launched.** No `mcla.exe`, launcher or game process
was started; no key, ticket, save, profile or MAIN was touched; no git write in the main checkout (no
`git add`, no commit, no push); the SDK fork was not touched (it stays at v0.20.0.12 = `d889a16`).
`wait-for-idle-pc.ps1` ran before every build and test command and returned idle each time. Spark not used:
a cherry-pick, deterministic builds, tests and scans.

## a. Identities

| item | value |
|---|---|
| SDK branch / commit | `mcla/vol-l120v` (worktree `private/l120v-sdk-wt`), one local commit **`aadda84` (`aadda8493ac59480a7883ca6b0f053d462f68a10`)**, parent `d889a16` = v0.20.0.12 (the SDK of the published l117v) |
| SDK build | tree `private/l120v-sdk-b`, output `private/l120v-sdk-o`, install `private/l120v-sdk-install`; the l117v volunteer configuration (Ninja Multi-Config, clang 20.1.8, C++23, `-march=x86-64-v3`, D3D12 only, Tracy and perf counters ON, tests ON, `REXGLUE_ENABLE_FFX_PRESENTER_SHADERS=OFF`, `REXGLUE_ENABLE_FIDELITYFX=OFF`, prefix map `l120v-sdk-wt=./third_party/rexglue-sdk`); `Version: 0.20.0.1-dev.gaadda84` (the version is derived from the nearest tag: one commit after v0.20.0.12; once the commit is tagged v0.20.0.13 a rebuild reports 0.20.0.13) |
| Host source | main `ecfc182` (the l117v host commit), copied from the l117v scratch tree and checked file by file against the commit's blobs (`git hash-object --path`: 632 tracked files, 0 mismatches, 0 missing); `mcla_m10_defaults.h` `3CC1ECFB...067F`, `mcla_dev_overrides.h` `9DB010A2...7185`, `CMakeLists.txt` `9309F5BF...20AE` |
| Host build | `private/l120v-host-build`, the l117v volunteer options (`MCLA_DEV_OVERRIDES=OFF`, `MCLA_EXPERIMENTAL_F10_WGC=OFF`, expiry, raw timer, perf split, livery diag2, read-back sync composite, prewarm seed, m10 speed-up defaults, image and diagnostic tests ON), `Found ReXGlue SDK 0.20.0.1-dev.gaadda84` |

**Staged input `private/l120v-volunteer-input`** (no ticket; `private/l120v-volunteer-input.SHA256SUMS.txt`,
SHA-256 `A84FEA88E65B7E97AF2A7AA62059D0445DFFAB2B89375491BF991A0F9EBB1A3B`; the three DLLs equal
`private/l120v-sdk-install/bin` and the build-directory copies (asserted); every file differs from l117v):

| File | Bytes | SHA-256 |
|---|---|---|
| MCLA-Launcher.exe | 1452544 | 6793C8F7FF430B25030752639C8603BCEDFE3C9296D167D8B476FB9D1BFAF68D |
| mcla.exe | 53969408 | **CD59A0FB4762394AC4EC1D6D976C450BCAC5898A35BC0AA4E57238786C7E6B54** |
| mcla_crash_handler.exe | 211968 | A34270D7DD200ED98B70151D74436D64536FEBF45FF3F6EB7582F4A7887273FA |
| mcla_diagnostic_image_helper.exe | 60928 | 1B11DA89EF5E713894B2AD339484305A3DB0A396BF51A596B01A1C83F54D13C4 |
| rexgpu-xenos.dll | 3906048 | A1AC5A8B88B4B866322B533F59ECF88D1F4D4388FF70A23E9D7DD54029626E08 |
| rexruntime.dll | 11702272 | 425A85E9981096BCE003057C0A3346512892C54C1DDF5DDDE410DCAAD64F1652 |
| TracyClient.dll | 246784 | 40671016F634633242C507C5E4E90512E5D023B1A9D65602BF783AFE45618083 |

The host executables have the same source as l117v; their bytes differ only because they were linked in a
new build tree (as every fresh tree). `rexruntime.dll` has the l117v size; `rexgpu-xenos.dll` grew by
7,168 bytes (the fix lives in the graphics DLL).

## b. The SDK change against the published v0.20.0.12

`aadda84` is a cherry-pick of `8d351cc` (l118, "KI-088 vertex request skips validated against shared-memory
validity") onto `d889a16` and nothing else. Three conflicts (the l117 / l118 context of `8d351cc`'s hunks:
the l117 lever cvars and the l118 RT memo 2 code around the insertion points, which `d889a16` lacks) were
resolved by keeping only the lines `8d351cc` adds (scripted, `l120v-work/resolve_l120v.py`). Proof: the
changed (+/-) lines of `git diff d889a16 aadda84` are **identical** to those of `git show 8d351cc`
(`diff` of the two line lists empty); only the context differs. `git diff --check` clean,
`git-clang-format --diff d889a16`: "clang-format did not modify any files".

`git diff --numstat d889a16 aadda84` (8 files, +384 / -29):

| file | + | - | content |
|---|---|---|---|
| include/rex/graphics/util/vb_slot_check.h | 125 | 0 | new: `IsPageRangeValidInBitmap` (the bitmap scan moved out of `RequestRanges`), `DecideVbSkip`, `VbSlotCounters`, the `MCLA_VB` / `MCLA_VB_STALE` / config line formatters |
| include/rex/graphics/shared_memory.h | 8 | 0 | `SharedMemory::IsRangeValidLockFree(start, length)` (public, const, lock-free) |
| src/graphics/shared_memory.cpp | 18 | 23 | `RequestRanges` fast path calls the shared scan; `IsRangeValidLockFree` |
| include/rex/graphics/d3d12/command_processor.h | 12 | 0 | the include; `vb_validate_`, the counters, `NoteVbStale` |
| src/graphics/d3d12/command_processor.cpp | 59 | 4 | cvar `d3d12_vb_skip_validate` (bool, default **true**, hot); latch in `LatchL101Options`; `NoteVbStale` (first 16 lines per session); `MCLA_VB v=1` in `PerfSplitEmit` after `MCLA_CP_L101`; `IssueDraw`: skip A (in-sync bit) and skip B (same address / size) only while the range is valid, a slot refused at A goes straight to `RequestRange` |
| src/graphics/vulkan/command_processor.cpp | 13 | 2 | the same check (not compiled in MCLA builds) |
| tests/unit/CMakeLists.txt | 1 | 0 | the test source |
| tests/unit/graphics/vb_slot_check_test.cpp | 148 | 0 | 5 cases |

Not taken (all of them are in `8d351cc`'s ancestors `e6eeeb9` / `4807ea1` or in later commits): the l117
per-draw levers (`d3d12_cb_reuse`, `d3d12_ib_view_cache`, `d3d12_tex_outdated_narrow` and their verifies,
`MCLA_CP_L117`), the MCLA_THIN zero-GPU-time change, the l118 RT memo 2 (`d3d12_rt_update_memo2`,
`MCLA_CP_L118`), and all of l119 (MCLA_SHM, the fault probe, the Canary `aed81ca9` invalidation port, the
single-range fast path). The binary scan below confirms their strings are absent.

Host: no change since `ecfc182` is required. `git log ecfc182..HEAD -- src CMakeLists.txt tests scripts cmake
config` lists only the l118 / l119 owner-key commits `ac693cf` and `53f209f`; their `src` changes are dev
override keys (compiled out in a volunteer build) and the `kNeverWrittenCvars` bookkeeping list of
`mcla_m10_defaults.h` (a unit-test list; the m10 module writes only `kDefaults` cvars, all present in
`aadda84`). The new cvar needs no host key: its SDK default is the fix.

## c. Gates (verbatim counts)

| # | step | result |
|---|---|---|
| 1 | SDK configure | rc 0, `Version: 0.20.0.1-dev.gaadda84`, `REXGLUE_ENABLE_FFX_PRESENTER_SHADERS:BOOL=OFF` |
| 2 | SDK build + install | rc 0, `INSTALL OK` |
| 3 | SDK warnings against the l117v volunteer SDK build (`l117v-work/sdk-install.log`, the same configuration) | `new normalized 213 reference 213`, `new vs reference: 0`, `gone vs reference: 0` |
| 4 | SDK ctest, three runs | `100% tests passed, 0 tests failed out of 2403` (x3; l117v 2398 + the 5 `vb_slot_check` cases) |
| 5 | Host build | 295/295 steps, rc 0, **0 warnings** |
| 6 | Host tests (`l120v_host_tests.ps1`) | `count 60`, `failed executables: 0` (`mcla_dev_overrides_test: 7388 checks passed`, `mcla_m10_defaults_test: 18145 checks passed`) |
| 7 | Volunteer binary scan (`l120v_scan.py`, 36 checks) | `failures: 0`: no dev-override strings in mcla.exe or the launcher; UTF-16 `MCLA test-build admission` and `MCLA m10 default` present; no build / user / temp / repo paths in any of the 7 binaries (ASCII and UTF-16, the Windows user name in path form included); no FidelityFX presenter shader strings; `nvapi64.dll`, `amdxc64.dll`, `MCLA_LOWLAT` present; **`MCLA_VB v=`, `d3d12_vb_skip_validate`, `MCLA_VB_STALE` present in rexgpu-xenos.dll**; absent from the SDK DLLs: `d3d12_cb_reuse`, **`d3d12_tex_outdated_narrow`**, `MCLA_CP_L117`, `d3d12_ib_view_cache`, `d3d12_rt_update_memo2`, `MCLA_CP_L118`, `MCLA_SHM`, `MCLA_FAULT_PROBE`, `memory_invalidate_unwatched`, `shared_memory_single_range_fast`, `shared_memory_hot_compare` |
| 8 | Prepare script parse | `parse errors: 0` in PowerShell 7.4.22 and 5.1.26100.9818 (not run, no dry run) |

The volunteer audit (`Test-MclaVolunteerStaging.ps1 -RequireVolunteer`) needs the signed ticket; it runs
inside the prepare script.

## d. The allowance list: l120v + l117v for a 7-day overlap

**Decision: overlap (two entries), implemented.** Reasons:
- The published list names l117v only (`private/l117v-entries.json`: `A65B6E7E...9DF2` valid until
  1797778646). An l120v-only list would refuse every tester's l117v at its next online check the moment it
  is published, before they have the new package: testers in the middle of a session or without the
  package yet would be locked out.
- A V2 build does not start online until its list is published, and the pre-share check needs the build to
  start. With an overlap the list can be published **before** the pre-share check without any effect on
  l117v testers (l120v itself is handed out only after the check); with an l120v-only list the check would
  have to run offline (as l117v's) or the list publication would withdraw l117v before l120v is verified.
- The pipeline allows it: `New-MclaTestBuildLeaseEnvelope` accepts a JSON array of 0-256 entries with
  unique hosts (`scripts/Mcla-TestBuildSigning.ps1:141-171`); the client checks each entry's own
  `valid_until`.
- The fix is a correctness fix of a rare artefact, not a safety or data-loss issue, so a week of l117v
  coexisting is acceptable; after it l117v is refused online (owner decision 2026-11-05: older builds are
  withdrawn).

`private/l120v-volunteer-prepare.ps1` writes `l120v-entries.json` = `[{l120v host, now + 30 days},
{A65B6E7E...9DF2, min(now + 7 days, 1797778646)}]`, checks both entries on read-back and that the two hosts
differ, then signs `l120v-active-builds.txt`. If the overlap must be longer, a newly signed list is needed
(the client has no rollback protection beyond rejecting lists issued in the future).

## e. Files

| file | bytes | SHA-256 |
|---|---|---|
| `private/l120v-volunteer-prepare.ps1` (ASCII CRLF; from the l117v script by `l120v-work/mk_prepare.py`) | 11898 | 85A7E1AF87E17799FF92CBB9CE5CEE9D49F5164264DA58CF1D0B1053047D5C93 |
| `private/l120v-THIRD-PARTY-NOTICES.txt` (a byte copy of `l117v-THIRD-PARTY-NOTICES.txt`: no new third-party code) | 6790 | 3EAA8C437AB88B1C2A72AFB883EAB01701C4D2F286690C10B8C1CCE23DF876EB |
| `private/l120v-CHECKLIST.txt` (ASCII CRLF) | 3768 | A1C351CFEDB68D593858373A36D00FFC3B8E9B575E3572035F08D80DB2CCFEF3 |
| `private/l120v-volunteer-input.SHA256SUMS.txt` | | A84FEA88E65B7E97AF2A7AA62059D0445DFFAB2B89375491BF991A0F9EBB1A3B |
| `docs/volunteer-changelog-l120v.txt` (UTF-8, CRLF, EN + UK: one fix, nothing else changed, the 7-day l117v overlap) | 2166 | D00D5A75D95F129474F54C099FAC362270F886E9E2F7FE6F3F78F0CE77B77603 |

The prepare script uses `docs/volunteer-tester-guide.txt` as it is at signing (the l117v guide; no change
proposed: the guide lists known issues, and KI-088 was never in it). Scratch:
`%LocalAppData%\Temp\claude\C--BDU-MCLA-Recomp\l120v-work` (scripts and logs: `resolve_l120v.py`,
`msg_l120v.txt`, `l120v_sdk.ps1`, `copy_tp.ps1`, `make_src.py`, `l120v_host_build.ps1`,
`l120v_host_tests.ps1`, `stage_input.py`, `l120v_scan.py`, `mk_prepare.py`, `warndiff.py`, `crlf.py`,
`sdk-configure.log`, `sdk-install.log`, `sdk-warndiff.log`, `sdk-ctest1..3.log`, `host-configure.log`,
`host-build.log`, `host-tests.log`, `stage_input.log`, `l120v-scan.log`, `make_src.log`).

## f. SDK publication plan: v0.20.0.13 = aadda84 (commands NOT run; only on the owner's "publish SDK")

The fork branch `mcla/mcla-r-hotfixes` is at `d889a16` (v0.20.0.12; `fork/mcla/mcla-r-hotfixes` in
`private/l39-sdk-src` = `d889a169703a...`); `aadda84`'s parent is `d889a16`, so the publication is a
fast-forward of one commit. `v0.20.0.13` does not exist yet.

```
cd C:\BDU\MCLA-Recomp\private\l39-sdk-src
git fetch fork
git rev-parse fork/mcla/mcla-r-hotfixes            # expect d889a169703a1ce9e4701b7e856028cf04d57123
git merge-base --is-ancestor fork/mcla/mcla-r-hotfixes aadda8493ac59480a7883ca6b0f053d462f68a10
git tag -a v0.20.0.13 aadda8493ac59480a7883ca6b0f053d462f68a10 -m "MCLA M11 KI-088: vertex-buffer request skips validated against shared-memory validity (d3d12_vb_skip_validate, default true; MCLA_VB counters); cherry-pick of 8d351cc on top of v0.20.0.12. ctest 2403/2403 (4 skipped). Source of the l120v volunteer DLLs."
git push fork aadda8493ac59480a7883ca6b0f053d462f68a10:refs/heads/mcla/mcla-r-hotfixes
git push fork refs/tags/v0.20.0.13
git ls-remote fork refs/heads/mcla/mcla-r-hotfixes refs/tags/v0.20.0.13
```

**Consequence for the owner line.** `mcla/m11-l117` .. `mcla/m11-l119` (and l120) descend from `d889a16`, not
from `aadda84`: after the publication the fork tip is no longer an ancestor of the owner line, so a later
publication of the owner line is not a fast-forward. Two ways:
- **merge (recommended):** on the owner-line tip, `git merge aadda84` with the conflicts (the same three
  places) resolved to the owner-line side, then check `git diff <owner tip> <merge commit>` is empty (the
  owner line already contains the same changed lines through `8d351cc`, proved above). The fork branch then
  fast-forwards from `aadda84` to the merge commit; every owner-line commit id cited in evidence and build
  receipts (`e6eeeb9`, `4807ea1`, `8d351cc`, `c503e32`, ...) stays valid.
- re-pick: rebase the owner line onto `aadda84` dropping `8d351cc`; linear history, but every owner-line
  commit gets a new id (the recorded identities of l117-l120 owner builds would no longer name published
  commits) and the conflicts of the rebase are the same three. Not recommended.

## g. What the owner must do

1. Run `private/l120v-volunteer-prepare.ps1` (signs the V2 ticket and the l120v + l117v list, stages
   `private/l120v-volunteer`).
2. Say the word to publish the list (`l120v-active-builds.txt` as `active-builds.txt` on `DebugBundle`): safe
   before the pre-share check because l117v stays listed for 7 days.
3. Pre-share check (`private/l120v-CHECKLIST.txt`): a race and a pursuit on the PC, the usual Deck loop;
   logs: no dev-override lines, the m10 defaults line, `MCLA_VB v=1 kind=config validate=1` and the
   per-window `MCLA_VB ... vb_insync_skip_invalid=` (above 0 is expected: the skips the fix refuses).
4. "publish l120v" (package) and, if agreed, "publish SDK" (v0.20.0.13 above).

The primary: review; commit `docs/volunteer-changelog-l120v.txt` and this file.

## Risks

- The three SDK DLLs and the four host executables are a new build: not byte-identical to any build the owner
  has run. The source difference against l117v is exactly the KI-088 change (b); the l118 / l119 owner
  builds ran the same change on PC and Deck (the Deck numbers of section a's reference).
- The fix adds a lock-free bitmap scan per in-sync / same-address vertex slot (cost class: the
  `RequestRanges` fast path, a few ns per slot) and a re-request per refused skip (0.15-0.32 per frame on
  the Deck): no measurable cost expected; not measured in l120v itself.
- The overlap keeps l117v (with KI-088) startable for 7 days after the list is signed.
- The SDK install registered `rexglue` in the CMake user package registry (as every SDK install); host
  builds pass `CMAKE_FIND_USE_PACKAGE_REGISTRY=OFF`.
- The SDK reports `0.20.0.1-dev.gaadda84` (one commit after the nearest tag) in its version string until the
  tag v0.20.0.13 exists; cosmetic (the host requires no SDK version for dev SDKs, `REXSDK_VERSION` empty).
