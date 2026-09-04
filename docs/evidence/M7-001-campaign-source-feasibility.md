# M7-001 native campaign-source feasibility

Date: 2026-09-05. Status: bounded source investigation, **not a catalog or
campaign PASS**. Primary inspection plus independent read-only inventory and
registration-flow review. No runtime, Ghidra, codegen or archive extractor ran.

## What was inspected

- Existing M6-003 private static reports; the constructor report from the
  failed M6-014 delivery run, rehashed to its recorded identity.
- Two generated registration-function excerpts, cross-checked against the
  historical name/string references. Generated code stays ignored/private.
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
The query wrappers' bodies, lifetime/thread ownership, ABI and bounds still
require review before any runtime probe.

Other bounded leads:

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

Inspect only wrappers `0x822C9070` and `0x822C8FE8` for read-only catalog/name
access, then `0x826C0700` for unlock-state querying. Establish storage, bounds,
and safe snapshot semantics before instrumenting. Reconcile eventual names
with actual Mission Log and persisted progress; do not call unlock mutators.
No row in the historical coverage seed receives campaign credit from this
investigation. The pending 0.13.0.0 Deck pair remains the first physical gate;
no new physical session, bundle, or executable is created by this report.
