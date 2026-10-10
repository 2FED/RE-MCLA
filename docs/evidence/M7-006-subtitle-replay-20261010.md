# M7 subtitle scene replay and accepted result

2026-10-10 result: representative subtitle rendering PASS in the owner-driven
s2 replay. Owner completed Sunset Strip Race, saw the Lester scene with SUBTITLES
ON, and saw subtitles. Primary and independent review of the actual F10 image
confirm a readable English dialogue caption at Great Palace. No repeat required.
This is not all-scene/telephone subtitle coverage.

## Task closure and media limits

M7-006 is accepted for the representative route set after primary and independent
review. The owner also confirms startup promotional video playback and Y skip
for encountered cutscenes; the repaired Tournament-to-Pinkslip phone tutorial
passes its [blocker regression](M7-pinkslip-crash-20261010.md). Current sound/music
observations cover working audio, not every track or exact mix. KI-017 remains
an explicit unresolved music-continuity limitation, as the M7-006 criterion
permits: output counters do not identify music versus ambient sound, and no
decoder/mixer correction or reference parity is claimed. Exact Bink decoder
identity, unseen videos, phone-caption expectations and line synchronization
are not inferred. The complete physical title campaign is outside M7 scope.

The final owner-driven subtitle record, frame, session and full-profile checks
below pass primary and independent review. PS5/7 input-contract tests also rerun
with2parsed scripts/5cases/4invalid-input rejections; those are not rendering
tests. No new runtime implementation or repeat playthrough is needed for this
bounded media acceptance. Source0.18.6.0 records closure, not new binary provenance.

2026-10-10. First seed rejected by owner observation: Sunset Strip Race is already
completed in s1. Subtitle rendering was NOT tested; this is a route-precondition
failure, not a subtitle defect. Preserve s1 and do not replay it automatically.
Owner explicitly selects the existing Run M7 Clean Profile Test.cmd save for the
replacement, not a brand-new profile. Entry remains `Run M7 Subtitle Test.cmd`;
replacement root is `private/s2`.
The owner identifies the early Great Palace parking-lot scene with Lester after
Sunset Strip Race as a scene that should have subtitles. The subsequent s2 result
below confirms this route by owner observation and a subtitled Great Palace frame.

## Existing observation

Owner-provided screenshot `codex-clipboard-c2eacc3d-e39d-4f10-b6f8-60ed42e5fb10.png`
clearly shows GAME OPTIONS / SUBTITLES / ON in their running game. SHA256:
`99DD671FB69C0D26E91F1A407C03A064C2D9DA4048A492C34C0DA41EFD6342AC`.
This proves the visible option, not rendering during a cutscene, exact process
identity, or the setting in the separate historical seed. Telephone/incidental
speech is not used as an established subtitle control.

## First attempt isolation and identities

Accepted p1 immutable payload was fully materialized, not hardlinked. No p1 user,
cache, logs, metadata, results, diagnostics or update state was copied. All seven
writable roots belong to s1. Its user tree is an independent complete copy of
`private/baseline/M4-011/post-oobe-profile`, archived through the production
content-addressed profile API. Main profile EFF52498..., historical seed and
normal portable pointer were unchanged by staging; full main returned-state
before/after identity matched. No source removal, profile rollback or seed reset.

- Binary version remains0.18.0.0, not the newer documentation/source version.
- Host82A69B5E..., launcher67B8F527..., runtime542DD347..., WGC helper6E766EDF...
  and GPU E2EA29B7... are the accepted p1 bytes.
- New bundle ID: `mcla-0.18.0.0-win64-82A69B5E922DE807-67B8F5272016D686-E8B559E1F2D03341`.
- Full-profile hash: `6B84C0B7C42D2884E43CC330C78AF5D7C8083DCC70D76BFFCE446672C9CE4D77`.
- Save: `E8B559E1F2D03341B1147CAB4F5A3F3C778E6E9633B0B04B9908360FA2C67D68`.
- Header: `1DAEF55FA78BDD3F1AA75A36772B801C6C714B6D1A115A97904F3D1C957BC4C9`.
- Immutable manifest: `E233CA7E81BD8DAD42DAC7F72A04E258EDAB53B9DAF814B003E2A59CAB4197AB`.
- Bundle manifest: `7C8673DEB542224666B385EC37A88862931C8292AE0993CFDCC960E84E80A3B9`.

The full-profile hash above uses the portable profile inventory format; it does
not replace the different historical M5 tree-format hash5F3B045D.... Individual
save/header hashes match the historical route contract. No ancestry from the
main progressed profile is invented.

Staging receipt: `private/evidence/M7-006/subtitle-candidate-20261010-1/staging.json`,
SHA256 `59A3AB2FFFA4A00622E917A66318048E04E522DC97551C5AB76939F12385C6C8`.
The unchanged portable manifest task/schema remain M7-016/v2; this external
receipt and report define the M7-006 experiment. Native path budget covers known
snapshot/rename/diagnostic nesting: maximum179file/164directory characters.

## First attempt preparation checks

Independent pre-stage and final isolation review GO. Sampled host/save/game
archive hardlink inventories show only their respective s1 path. Production
bundle verification accepts33immutable files and15game files. PS5.1 and PS7 entry
CheckOnly both pass and select NVIDIA GeForce RTX5060 Laptop GPU, index1.

One guest-free diagnostics probe, session `20261010T133227Z-26296`, exits0 with
33verified files, one recoverable save snapshot and zero watcher errors. Production
session/diagnostic validation and complete returned-profile validation pass;
seed remains6B84C0B7.... This is not a guest boot, cutscene or F10 gameplay result.
Diagnostic package: `private/s1/diagnostics/live/live-20261010T133228Z-32668-1`.

PS5/PS7 portable bundle regression:5positive,105negative,45source checks,
3path-budget positives,2capture-cache and2settings positives PASS.
New subtitle wrapper tests:2scripts parse,5input cases including4invalid input
rejections PASS on both shells. These test the input contract, not subtitle
rendering. The HITL workflow adapts the diagnosing-bugs template to PowerShell.

## Replacement using the existing Clean test save

At the owner's explicit request, `-CleanTestSeed` uses the preserved full-profile
archive that matches the live `M7-Windows-Stock-Clean-20261010-1/user-stock` tree.
It does not create an empty profile. The source matches before and after staging;
main p1 returned-state and normal pointer also remain unchanged. The replacement
has independent materialized files and fresh writable roots in `private/s2`.
The original Clean test and rejected s1 attempt are preserved, not reset.

- Profile: `5D07321954B57ED0D4383464B073587F7AD70DA87DFA81263659CE9F900457F0`.
- Save: `D8E9AE500E5F9AC38B26D8A7749A7CE8F78C5D6ABDC4B82A586674A997B6273F`.
- Header: `1D717009B10A84525F468AD826A7DC016B744FA78274865A7DAB8A1BB65A4957`.
- Bundle ID: `mcla-0.18.0.0-win64-82A69B5E922DE807-67B8F5272016D686-D8E9AE500E5F9AC3`.
- Immutable manifest: `D97B16B07CB30754680664DCD0535F23E6E5528E57F7C92A13CA84167183AB4C`.
- Bundle manifest: `4FF5FC00B563947EBE4E7374A60782D509A04B602092735FB93F2D4804922C94`.

Receipt: `private/evidence/M7-006/subtitle-candidate-20261010-2/staging.json`,
SHA256 `FF5853232E58512022EE5882B4044F49303D0F1B8E8751466C924405318197F0`.
Production staging verification passes33immutable/15game files; path budget
remains179file/164directory characters. PS5.1/PS7 CheckOnly passes with named
RTX5060; wrapper tests pass2parsed scripts/5input cases/4invalid rejections.

One guest-free diagnostics probe, `session-20261010T134443Z-47748`, exits0,
creates one recoverable snapshot, and records zero watcher errors. Production
session validation and full returned-profile validation pass with the same
5D073219 profile. Diagnostic package: `private/s2/diagnostics/live/live-20261010T134444Z-17332-1`.
This preparation probe is not a guest boot or subtitle rendering result. Scene
availability was unproven at preparation time; the owner result below supersedes it.
Independent final static isolation review GO: actual pins and archived profile
match; no empty-profile fallback, source replacement or reseeding operation.
The reviewer did not inspect the concurrently running probe's mutable results;
the probe validation above was performed by the primary agent.

## Accepted owner return

Owner record `private/evidence/M7-006/subtitle-owner-20261010T134905Z-7adeb856/observation.json`
contains y/y/y for SUBTITLES ON, target scene seen and readable subtitles seen,
with exit0. Its original independent-review-pending marker is retained; this
report records the completed primary and independent review without rewriting it.

F10 package `private/s2/diagnostics/live/live-20261010T135446Z-15948-1` contains
six manifest-bound artifacts, all sizes and hashes verified. The1920x1080 frame
shows the Great Palace sign and the caption “Seriously homie, 5-0 was all over
my ass.” It contains real cinematic content, not a black frame or foreign window.
Frame SHA256: `0CADE93203728862262859453F23FD756CD35707D18628C61ADD19652C5CC4E4`.
The image alone does not identify Lester; that identification is the owner's.
WGC helper admitted/cleanup complete, exit0 and all failure fields0.

Gameplay session `session-20261010T134913Z-46096` passes production session
validation: exit0,33immutable files,3save snapshots,0watcher errors. Production
full returned-profile validation passes. Resulting profile:
`EBBDB1842D605DF555182FE2469D762C9E4EEAEAD6FDBF3625244877437B028B`;
save `482840D946A66F5A0760CC278C7719C3D7D93A848F5D850218CA4CDA31B87DC8`;
header remains1D717009.... Returned-state hash:
`79646FA4C00633005E1604E1BDFE3F05A453973E44782A83183E1AB74769B779`.
This remains a separate Clean-derived branch; do not merge it into the main save.

The full log contains exactly300sequential audit samples and a complete summary,
with0audio underruns,0stream stalls and0shader/PSO failures in that bounded window.
No exact image/sample/audio synchronization or performance acceptance is inferred.
WGC image freshness remains explicitly not-established.

The second package `live-20261010T135459Z-15948-2` is NOT a second F10: its reason
is `unexpected-guest-exit`, with no frame. All5artifact hashes and both save files
verify. Production validator functions for frame metadata, save inventory,
topology and minidump headers pass for both packages. Log order is audit completion,
then “Window closing, shutting down...” at16:54:59.508, then snapshot queued.
The current OnGuestThreadExit callback labels exits this way without a shutdown
guard. Preserve the signal separately: exit0 and this log order neither establish
a gameplay crash nor prove graceful guest teardown. No runtime fix was made.

Independent read-only review agrees with bounded subtitle acceptance and the
teardown limitation. No game was relaunched, no save reset and no upload performed.

## Original owner test instructions

1. Close other MCLA sessions, run the CMD and press Enter.
2. In this test's GAME OPTIONS, check SUBTITLES and turn ON if needed. Do not
   change F4 host settings or create a new save if the supplied seed will not load.
3. First inspect Sunset Strip Race in the mission log. If already completed,
   stop without replaying. Otherwise follow its objective and look for the named
   Great Palace/Lester scene. If unavailable or blocked, stop; no mission grind.
4. If it appears, do not skip with Y. Watch several spoken lines and press F10
   once during speech. Allow the recording to complete; F10 alone does not
   guarantee a frame containing the relevant line.
5. Quit normally. Answer the console's three y/n/u questions: option ON, target
   scene seen, readable subtitles seen. Report unusual behavior in chat.

The console writes a unique private observation record with launcher exit status;
it never classifies an automatic PASS. Missing scene means the subtitle test was
not performed, not a rendering failure. Reopening retains this test's progress;
neither launcher nor staging silently restores the seed. Any repeat/reset needs
a new separately reviewed test copy. The completed observation is recorded above;
these original instructions do not request another run.
