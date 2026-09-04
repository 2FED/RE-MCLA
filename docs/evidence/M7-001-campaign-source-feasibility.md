# M7-001 native campaign-source feasibility

Date: 2026-09-05. Status: bounded source investigation, **not a catalog or
campaign PASS**. Primary inspection plus independent read-only inventory and
registration-flow review. No runtime, Ghidra, codegen or archive extractor ran.

## What was inspected

- Existing M6-003 private static reports; the constructor report from the
  failed M6-014 delivery run, rehashed to its recorded identity.
- Generated registration, query-wrapper and directly related accessor/loop
  excerpts, cross-checked against the historical name/string references.
  Generated code stays ignored/private.
- The outer 32 bytes of each of four original RPF files: 128 bytes total.
  Source manifest inventory/sizes passed (15 files, 6,569,586,392 bytes);
  this probe **did not freshly rehash the full game payload**.
- Existing tracked extraction/SDK CLI tools and primary format implementations.
  No new dependency was installed or adopted.

Existing extract-xiso tooling stops at the disc's outer archives. SDK VFS
dumping handles STFS, not the RPF campaign catalog. Generated registration
addresses alone cannot identify mission instances. The earlier 102-target
constructor audit proves callable coverage, not 102 campaign missions.

## Source handles recovered

Historical reports identify a race-description format with race type/subtype,
district, statistics index, lap count, grid/checkpoints, target time/damage and
opponent/car configuration. These are structural fields, not populated events
or prerequisites. Session reflection also names `RaceNameHash`; neither its
hash algorithm nor its identity with a campaign-save field is established.

The registry pairs below are confirmed by `r3` name-pointer / `r4` callback
materialization immediately before helper `0x82554798`. They identify
**registered wrappers**, not a proven host-callable ABI or validated live data.

| Registered name | Callback | Registration call |
|---|---|---|
| CityDescription_LookupRace | `0x822C9F58` | `0x822CA744` |
| CityDescription_LookupRaceByIndex | `0x822C8FE8` | `0x822CA758` |
| CityDescription_FindRaceByRacerPosition | `0x822C9028` | `0x822CA76C` |
| CityDescription_GetRaceIndexByName | `0x822C9F98` | `0x822CA780` |
| CityDescription_GetRaceNameByIndex | `0x822C9070` | `0x822CA794` |
| mcCareer_UnlockRace | `0x826C06E0` | `0x826C2438` |
| mcCareer_UnlockDistrictRaces | `0x826BE060` | `0x826C244C` |
| mcCareer_IsRaceUnlocked | `0x826C0700` | `0x826C2460` |

Registration bodies: `0x822CA238` and `0x826C2160`. The two unlock-mutating
callbacks must **not** be executed to enumerate content or alter owner progress.
The query ABI and array layout are now source-reviewed below; live mapping,
initialization, lifetime and snapshot consistency still need validation before
any runtime probe.

## Query ABI and catalog layout

Both `0x822C8FE8` and `0x822C9070` receive a script context in `r3`.
Context `+0x08` points to 32-bit argument slots `[city pointer, race index]`;
context `+0x00` points to a 32-bit return cell. They call their accessor and
write the returned pointer into that cell. These wrappers therefore **write
script-frame state** even though they query game data. Do not inject calls.

| Item | Source-derived behavior |
|---|---|
| Descriptor accessor `0x82388C48` | Reads city `+0x2C`; computes base plus `index * 0xA0` in effective 32-bit guest address arithmetic. No index, null or allocation check. |
| Name accessor `0x82388C60` | Negative signed index returns fixed pointer `0x8204DF9C` (contents not inspected). Otherwise reads one 4-byte pointer from the array at city `+0x24`; no upper-bound/null check. |
| Shared count at city `+0x20` | Independently established by the loop in `0x826BDAA8`: signed positive count guards entry, then count is reloaded and compared with the incremented index each iteration. |
| Paired arrays | That same loop advances descriptor offsets by `0xA0` and name-pointer offsets by 4, tying both arrays to the count at `+0x20`. Offset `+0x28` is not a proven count. |

The proof loop is itself an **unlock mutator**, calling `0x826BCEE8` after
district/type/subtype filters. Inspecting it does not authorize invoking it.
Its literal district filter `6` is a wildcard, not a six-district count.
Likewise, a valid catalog count would describe entries, not mandatory missions.

The loop passes the value at guest `0x82879330` to resolver `0x8238EC00`;
that slot is an opaque lookup key, not a direct city pointer. The resolver
searches a registry at `0x828CCE98` (32-bit array pointer at `+0x00`, unsigned
16-bit count at `+0x04`) of 80-byte records, using a comparison
of each record's first pointer against its argument. A match returns the
record address; no match reports the failure and returns null. The comparison
and reporting callees were not followed. No resolved object, current count,
allocation size, encoding or lifetime has been observed yet.

Source anchors: generated translation unit 14, wrappers `0x822C8FE8` and
`0x822C9070`; unit 21, accessors `0x82388C48`, `0x82388C60` and resolver
`0x8238EC00`; unit 53, loop `0x826BDAA8`. An independent read-only review
confirmed the wrapper/array layout; the primary re-read the proof loop.

## Current unlock state is not the dependency graph

`0x826C0700` uses the same script-context layout with one name-pointer
argument. Null produces zero. Otherwise it reads a career pointer from guest
`0x8286D8DC`, calls `0x826BF4B0`, and stores the low result byte in the
32-bit return cell. This is not a safe host-callable interface either.

The underlying query transforms the name through `0x821C9790` with seed zero,
then queries `0x826BDA38`. The transform lowercases ASCII A--Z, normalizes
backslash to slash and handles a leading quote before 32-bit mixing. This
identifies the transform used on this path; it does not yet bind the earlier
`RaceNameHash` reflection field or establish a public save-format contract.
There is no proven string-length bound; do not copy the guest loop as a
host-memory scanner.

`0x826BDA38` returns true for either of two nonzero override fields, whose
gameplay meaning is still unknown: guest word `0x8290951C` or byte at
the global career object's `+0x16C88`. Otherwise it scans at most 250
32-bit keys at the supplied career object's `+0x16844`, stopping at a
zero key or a match. **250 is a scan ceiling, not a proven allocation capacity
or mission count.**
The result describes current unlock membership/override state, not the
prerequisite edges, completion state or mandatory status of a race.

No save, override field or unlock state was changed or queried in a live
process. Raw generated code and game strings remain private. A future passive
snapshot must validate mapped ranges, reject overflow/truncation, bound names
and counts independently, and report inconsistent/loading state as unknown.

## Existing F10 is not a catalog snapshot

Both live and crash paths request exactly `MiniDumpNormal`,
`MiniDumpWithThreadInfo` and `MiniDumpWithUnloadedModules`, with no custom
stream or dump callback. Live capture supplies no exception information;
crash capture does. They do not explicitly request the guest catalog ranges,
full/private/indirect memory or guest-address translation metadata.
The IPC and runtime-state structures contain process/counter metadata, not
a guest memory map. Sources: `src/mcla_crash_handler_win.cpp` (`WriteDump`
and live event handling), `src/mcla_crash_ipc.h`, `src/mcla_diagnostics.h`.

SDK `Memory::TranslateVirtual` uses a live virtual base plus a heap-specific
host offset when applicable (`include/rex/system/xmemory.h` in the pinned SDK).
Do not assume a fixed host base plus guest address when parsing a dump.
The two completed 0.12.0.0 Desktop F10 manifests confirm successful minidumps,
but do not certify city/array coverage. On-disk profile snapshots are not
snapshots of the live catalog either.

A particular address might incidentally be captured: the format review does
**not** prove that all guest bytes are absent. Actual range-descriptor inspection
would be needed for a particular address, with validated translation metadata.
The current packages cannot be treated as supported arbitrary guest-memory
snapshots. No raw dump payload was read during this review, and no attempt was
made to enlarge F10 or the pending portable artifact.

## Other bounded source leads

- The resource locator at `0x8204E164` is `$/tune/race`; this is not yet a
  resolved archive entry/path. A `printracenames` string at `0x8204E1B8` has
  a generic registration reference, not an identified safe callable handler.
- The partially decoded race parser is in `0x82388E48..0x823895F8`; enum
  converter candidates are `0x8238E6D8`, `0x8238E748`, `0x8238E818`.
  Later raw/undecoded output prevents treating it as complete semantic proof.
- Name-based unlock validation appears at `0x826BCEE8` and `0x826BF4B0`.
  This does not enumerate dependency edges, REP thresholds or mandatory nodes.

## Archive boundary

All four inspected files start with ASCII `RPF3`; their header word at offset
`0x10` is `FFFFFFFF`. The MCLA-specific CodeX reader interprets that header
word as encrypted-TOC state, reads the TOC at `0x800`, and uses hashed entry
names. This is a **format-research lead**, not validation of that reader on
our payload. [CodeX Rpf3File.cs](https://github.com/Foxxyyy/CodeX.Games.MCLA/blob/master/RPF3/Rpf3File.cs)

Swage likewise distinguishes hashed RPF3 from named RPF2.
[Swage format notes](https://github.com/0x1F9F1/Swage/blob/master/src/games/rage/rpf.md)
The CodeX project also requires its separate engine; it was not installed,
licensed for integration, or made a project dependency in this work.
[CodeX MCLA project](https://github.com/Foxxyyy/CodeX.Games.MCLA)

Do not infer clear-text paths, a complete directory listing, mission counts,
or `.xsc` compatibility from the four-byte magic. No TOC, script, key or
asset was extracted. README-AI's no-broad-unpacking rule still applies.
Prefer the narrow native catalog/query lead before considering an extractor.

## Reproducible private provenance

The first four reports below live under
`private/evidence/M6-003/diagnostic-static`; the constructor reference lives
under the failed suite's `scenarios/free-roam/crash-analysis`. These are
historical report hashes, not new current-XEX validation or public raw data.

| Report | SHA-256 |
|---|---|
| parser-strings.txt | `CEB2A6D20CF4382F0CC72F15210200559E2F35BE633C79E50930D623FEC41D40` |
| race-desc-field-strings.txt | `2A892C9768A246C944C33F2771DAAA753A284C8BF7FE0F9E7DEA195394A2B446` |
| race-field-tables.txt | `C1E4DEE618177F056FEDEA0ADF5E7255DD83EB3375E443FF11F5FA912F0FC819` |
| race-strings-analyzed.tsv | `A9D5756DE5AE92ACA9DC017EAF9E452F69CEE83AC3F6F54467C9F30B0FED214F` |
| da40-references.txt | `CA6AA54F6CC4F4B828B217E5EA6037F2BA82FB58992896294D47348BA10BDF80` |

The constructor suite is `20260831-133236-2cecb67b`; see the existing
[delivery audit](M6-014-delivery-constructor-registry.md). The full raw reports
and generated bodies must not be committed or uploaded.

## Next bounded action and non-claims

The bounded wrapper/count/unlock-body and diagnostic-format reviews are
complete. Collecting catalog instances needs a separate, opt-in bounded
snapshot on an isolated copied profile; current F10 has no explicit range or
translation contract for it. Do not reuse the owner's live bundle
as a probe or rebuild the pending Deck comparison just for catalog discovery.
Passive range validation and consistent snapshot semantics remain mandatory;
do not call the unchecked accessors or unlock mutators. Reconcile eventual
names with actual Mission Log and persisted progress.
No row in the historical coverage seed receives campaign credit from this
investigation. The pending 0.13.0.0 Deck pair remains the first physical gate;
no new physical session, bundle, or executable is created by this report.

Independent read-only review found no privacy/claim-promotion blocker. Its
scan-ceiling and opaque-key wording corrections are incorporated. AST scan
is clean and all three rule fixtures pass; the unchanged historical seed
still receives no campaign credit. This source-only investigation does not
establish current runtime values or exhaustively audit the callees' effects.
