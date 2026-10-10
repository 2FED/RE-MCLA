# M7-001 representative category inventory checkpoint

October 10 reconciliation completes the identified-family and critical-lifecycle
inventory deliverable. The [current matrix](../campaign-coverage-matrix.md)
maps every source lead below to an observed route, historical-only evidence,
explicit unknown distinction or owner-authorized deferral. README-AI owns task
closure. Inventory completion is not M7-003 coverage PASS or milestone closure.

Later October10 follow-ups supersede the early pending summary: Unordered is
unavailable in the observed Arcade menu and deferred without PASS; race-to-start
works, without exact static Beat Me There label binding. Freeway5+ and targeted
Arcade Landmark are owner-confirmed. Payback and unconfirmed South Central access
are deferred without PASS by the owner's October10 no-grind decision. Series
Circuit/Sprint participation is not standalone completion or a native enum binding.
The p1 successor accepts owner-visible OrangeNoob and name/progress restart,
pause/audio/Alt+Enter. The owner corrected an initially missed F10, then completed
a genuine capture that passed independent visual/integrity review. See the
[latest event acceptance](M7-003-event-acceptance-20261010.md) and profile report.

The October 3 source investigation and its then-open discovery leads are preserved
below as historical evidence. It ran no game, archive extractor, Ghidra, generated
edit or save/unlock mutation and granted no current-build physical PASS.

## October 3 route families and discovery gaps

These overlap: race mechanism, session/container, wager condition and UI mode are
different axes. They must not be flattened into a disjoint seven-type enum.

| Family / axis | Source anchor | Representative route lead / remaining gap |
|---|---|---|
| Ordered mechanism, Arcade mode | [M6-004](M6-004-environment-effects.md) | Pause -> Modes -> Arcade -> Ordered Race -> Sunset and Vine. Rebind current artifact/profile and completion/persistence; historical environment run did not finish it. |
| Circuit / Unordered mechanism | Rehashed M6-003 static string references | Discover one currently labelled Arcade/career entry for each; exact present menu/course/event binding unknown. |
| Head-to-head participant route | [M6-003](M6-003-race-system-matrix.md) | Available Martin two-car event; current displayed name/ID/availability unknown. Not an exhaustive race enum. |
| Multi-event Series container | [M5-012](M5-012-race-results.md) | Available Ian series, all NEXT RACE transitions/final standings/return/reload; course names/count/availability unknown. |
| Tournament container lead | Static tournamentRace registration/string references | Discover a currently labelled tournament; exact format/menu/progression gate unknown. Not proven interchangeable with Series. |
| Red Light / Race Back recovery | [KI-026](KI-026-race-back-camera-softlock.md) | Available Red Light event, lose -> Race Back -> camera/pause recovery. Do not deliberately loop an uncontrolled softlock; KI-026 stays open. |
| Delivery objective | [M6-014](M6-014-delivery-constructor-registry.md) | Progressed-profile delivery entry; current identity/completion/reward pending. Historical entry/300s stability is not completion. |
| Freeway driver | Owner checkpoint README-AI 2026-08-27 | Two historically attested freeway events; current opponent/event/entry identity unknown. |
| Pink-slip / Wager condition | Same owner checkpoint; wagerRace/wagerHangout source leads | Historical best-of-three pink-slip win; current opponent/entry unknown. Other wager availability/semantics unknown. |
| Time Trial objective lead | Static timeTrialRace label | Discover currently labelled event; target/result/fail/retry/reload identity pending. |
| Payback objective lead | Static paybackRace / PaybackLegCore references | Discover currently labelled event; actual objective/transition/name/progression gate pending. |
| Beat Me There lead | Static beatMeThereRace label | Discover labelled challenge and classify whether distinct from existing participant/mechanism coverage; current route pending. |
| Goal Attack UI mode lead | Static goal-time/UI labels; [matrix persistence precaution](../campaign-coverage-matrix.md) | Discover labelled mode/objectives and fresh-process persisted credit; exact current entry unknown. |
| missionRace / localRace / wagerHangout labels | Historical static registration leads | Player-facing/offline distinctness remains unresolved; retain discovery gaps, do not invent extra required retail nodes. |

Multiplayer CTF/battle/editor/online-tournament strings do not automatically become
offline campaign requirements. Critical clean startup/tutorial/profile, expansion,
garage/shop/save/streaming, media/audio/input and AI lifecycle rows are separate
coverage dimensions. Trevor is still proposed/uncalibrated, not a current native ID.

## Source identity and interpretation boundary

Primary rehashed the four canonical reports under
`private/evidence/M6-003/diagnostic-static`; all match
[the prior feasibility report](M7-001-campaign-source-feasibility.md):

- parser-strings: `CEB2A6D20CF4382F0CC72F15210200559E2F35BE633C79E50930D623FEC41D40`
- race-desc-field-strings: `2A892C9768A246C944C33F2771DAAA753A284C8BF7FE0F9E7DEA195394A2B446`
- race-field-tables: `C1E4DEE618177F056FEDEA0ADF5E7255DD83EB3375E443FF11F5FA912F0FC819`
- race-strings-analyzed: `A9D5756DE5AE92ACA9DC017EAF9E452F69CEE83AC3F6F54467C9F30B0FED214F`

Bounded TSV565–580 contains the registration/leg labels,695–712 the known
mechanism/container references. Whole raw reports remain private; only an explicit,
owner-authorized bounded instruction/label excerpt packet was later sent to Spark.
Main also re-read historical race/delivery reports and the two converter bodies
in generated TU21: `0x8238E6D8` searches7 pointers at `0x827E92F0`,
`0x8238E748` searches14 at `0x827E930C`. These are enum lookup spans, **not mission
counts**, not an exhaustive validated category list. Series/Tournament references
also occur outside those spans. Complete table contents and retail instances were
not dumped or proven. No unchecked accessor/unlock wrapper was invoked.

## Source-bound descriptor axes (AFK preparation)

Independent read-only source review and primary inspection bind the two lookup
spans to separate descriptor fields, rather than an inferred seven-type catalog:

| Field / descriptor offset | Converter / pointer span | Historical named slots |
|---|---|---|
| `RaceType`, `+0x00` | `0x8238E6D8`, 7 pointers at `0x827E92F0` | Ordered=1, Unordered=2 |
| `SubType`, `+0x04` | `0x8238E748`, 14 pointers at `0x827E930C` | Ordered=1, Circuit=2, Unordered=3 |
| `DevOnly`, byte `+0x60` | Parser `0x82388F64..0x82388FA8` | If field is present, stores 1 exactly when parsed low32-bit value equals1; otherwise0. Missing-field default is not established here. |

The privately retained TU21 parser (`generated/default/mcla_recomp.21.cpp`, line41915)
constructs the exact field-label pointers, calls each converter and stores its
result at the corresponding offset. Converter bodies in that private file (line55236)
return the matched table index, or -1 on exhaustion. TU21 SHA256
`5F27BF0AD3D69ECA9ED7675512BC5661986BAA5553F9E828053C0E8FE69B7887`.
Historical `parser-strings.txt` lines1..8 bind DevOnly/RaceType/SubType labels;
`race-strings-analyzed.tsv` lines697,701..707 bind five table slots above. Their
hashes match the provenance section. Independently consistent historical
`race-parser-full.txt` lines73..118 SHA256
`2839BEDE634039B697657C48A6502D1AF6B530F71BC5E916FD91CA796E17739C`.
Primary checked label/table pointer arithmetic, call/store chains and low32-bit
equality idiom; no guest accessor or archive extraction executed.

Equal numeric values across RaceType/SubType are not equal semantics. Circuit is
demonstrated in a historical SubType slot, not proven a separate top-level
RaceType. DevOnly is an availability-investigation lead, not permission to count
or exclude a retail event. Remaining slots, live initialization/table contents,
current event instances, displayed names, unlock/REP gates and the effect of
DevOnly on selection remain unknown. No physical coverage/task closure credit.

Primary formatted-hex arithmetic check PASS (avoids PowerShell's signed32-bit
interpretation of high-bit hex literals); unchanged historical seed validator
PASS:11 entries,1 positive,13 negatives,GameplayCredit=false. Independent final
document review GO; generated source/seed flags unchanged.

## Bounded Spark descriptor experiment

An explicit two-excerpt packet (2711B canonical serialized copy,
SHA256 `A853D815876202AE160B7F498C388051C68E5C1CCBFE310AA2769EE362DBE877`)
was sent only to the owner's Spark,short aliases/normal TLS,2048-token output,
600s shared deadline budget,no automatic retries/tools/local inference. Separate
Qwen and Gemma jobs used the same packet/budgets. Primary rehashed all five
recorded artifacts for each; strict whole-response JSON parsing rejected both.

- Qwen `qwen3.8-27b`,24.406s: preface violates exact JSON; axes include DevOnly
  instead of SubType,wrong table address/names. Final846B SHA256
  `2301A857EB0A22E26F5B6D6ECC46959F81F60A36FBD74FAED03DE123B39A39DA`.
- Gemma `gemma4-26b`,16.656s: fenced JSON plus extra prose violates schema;
  converter entrypoints mistaken for table bases,RaceType count14 instead of7,
  missing historical named slots. Final SHA256
  `A08C8CA70A6CF9EC234871A473F9846B8D44EF86231D390031C7AFA5C18973C4`.

Both **REJECTED_EXTRACTION**. Three generic non-claim labels agree with the task,
but grant no useful descriptor extraction/code/physical acceptance. Neither
result changed docs or source. This is a narrow negative result on arithmetic/
axis binding,not a general model ranking or retroactive rejection of earlier
verified exact-quote jobs. No semantic/transport retry or unreviewed repair.
Receipts/raw results remain in ignored `private/evidence/M7-001-spark-descriptor-20261003`
and `M7-001-spark-descriptor-gemma-20261003`; keys excluded from packets/output/Git.

## October 3 acceptance plan

The original plan was to keep the unchanged historical seed validator and add
actual current route bindings/dispositions as local play exposes them: displayed
name/type/mode, exact build/GPU/config/profile lineage, applicable objective/result/
failure/recovery/reward and persisted reload. A missing route remains unknown or
explicitly unavailable with evidence/review, never guessed PASS. That October 3
checkpoint did not complete the matrix or grant coverage percentages; the October 10
inventory reconciliation above supersedes its planning status, not its test limits.
