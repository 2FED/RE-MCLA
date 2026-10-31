# M10-001 owner route protocol (l53 measurement build)

Owner-facing protocol for the M10-001 measurement runs. Design:
`docs/evidence/M10-001-harness-design-20261031.md` (sections 3-6); build:
`docs/evidence/M10-001-instrumentation-build-20261031.md`.

The l53 build is **owner-internal** (V1 ticket, no online check). It writes one
extra `MCLA_PERF_SPLIT v=1` log line per minute and one per F10, plus one
`MCLA_THREAD_CPU v=1` line after each of them. Nothing else changes in play.
MAIN is never played for these runs.

## 1. One-time setup: the bench copies (PC)

All copies are made in the launcher with **Copy selected profile...**; this
copies only the save tree (`user/`, SHA-256 verified) and never the
shader/pipeline store, so the first launch of a fresh copy is always a cold
store.

1. Close the game. Select **MAIN**, press **Copy selected profile...**, label
   `BENCH-SEED`. **Never start BENCH-SEED.** It is the frozen start state.
2. Select **BENCH-SEED**, **Copy selected profile...**, label `BENCH-A`; again
   from BENCH-SEED, label `BENCH-B`; again, label `BENCH-C` (720p arm).
3. Run setup for BENCH-A and BENCH-B: Build folder `private\l53-internal\build`,
   **Unlocked**, RTX 5060, **2560x1440 (scale 2)**, vsync on. CHECK SETUP must
   say "Setup saved."
4. Run setup for BENCH-C: same build, **Unlocked**, RTX 5060, **1280x720
   (scale 1)**, vsync on.
5. On the first run, note where the save puts the car after loading (garage or
   last position) and the in-game time and weather; send that with the first
   logs.

Do not save, buy, or enter events in any bench copy. After every run the agent
compares `before-snapshot.txt` with `after-snapshot.txt` of the session; a copy
whose save changed is retired and replaced by a new copy of BENCH-SEED.

## 2. Cold and warm runs

- The **first** launch of a fresh bench copy is **cold** (empty l53 store:
  shaders and pipelines are created while you drive).
- The **second** launch of the same copy is **warm**.
- Run order for the 1440p arm: BENCH-A run 1 (cold), BENCH-A run 2 (warm),
  BENCH-B run 1 (cold), BENCH-B run 2 (warm).
- 720p arm: BENCH-C run 1 (cold, counts as a warm-up), then BENCH-C runs 2 and
  3 (the two warm runs that are measured).
- **30 FPS control** (optional, one run): a fourth copy `BENCH-D` with the same
  setup as BENCH-A but **Stock** timing; one warm run (start it twice and use
  the second).

## 3. The route (every run, PC)

Before the first run, pick and name to the agent:

- **S**, the start point: a landmark 150-200 m before the chosen intersection,
  car stopped at the kerb facing it.
- **E**, the exit point: about 90 s of slow driving after the intersection.
- The intersection: C1 (downtown high-rise turnaround) or C2 (the big daily
  intersection where the l19 F10 was taken); C2 is preferred.

Per run (about 9-10 minutes):

1. Launch from the bench copy, load the save, drive to **S**. Keep about the
   same time from loading to S on every run (about 2 minutes), because the
   in-game clock advances with play time.
2. Wait at S until at least 45 s have passed since loading (the unlock) and no
   menu is open. Default chase camera, no input.
3. **Lap 1:** press **F10** at S (mark 1). Stay **stationary for 90 s**, no
   input. Press **F10** (mark 2). Drive **slowly through the intersection on
   the same path at a steady low speed** to E (about 90 s). Press **F10** at E
   (mark 3). Return to S (not measured).
4. **Lap 2:** the same again (marks 4-6).
5. Exit the game normally from the menu.

The F10 status dialog says when a capture was taken. If an F10 is refused as
busy, wait two seconds and press it again; say so in the run note.

Segments are cut at the F10 marks. A segment is used only if it is 90 s +- 5 s
long, its draws per frame are within 10% of lap 1 and of run 1 for the same
segment, no menu or pause and no `UI cap: on` / `guest vblank: 60 Hz` falls
inside it, and the save snapshots of the run are identical. A failing segment
is excluded, never corrected.

## 4. What to send back

After a normal exit, for each run, from the bench profile's
`sessions\<session id>\` folder:

- `mcla.log`, `launch-receipt.txt`, `capabilities.txt`, `state.txt`,
  `before-snapshot.txt`, `after-snapshot.txt` (not `before-user` or
  `after-user`).
- The session's F10 packages (`diagnostics\live\...`), as for earlier tests.
- A one-line note per run: copy name, run number, cold/warm, arm (1440p, 720p,
  30 FPS control), anything unusual (traffic jam, police, F10 refused).

The volunteer diagnostics export is not enough for these runs: it does not
carry the `MCLA_PERF_SPLIT` lines.

## 5. Steam Deck variant

- Same l53 build (V1 ticket), Deck launcher l49 or later, **Stock**,
  **1280x720 windowed** (KI-057: fullscreen still open), D3D12 through
  vkd3d-proton as the launcher sets it.
- Make the Deck's own `BENCH-SEED` from the Deck's MAIN copy and then
  `DECK-A` from it, as in section 1 (type labels with STEAM+X because of
  KI-052).
- **Run (a) title:** boot to PRESS START, idle 3 minutes, F10 at about 60 s and
  150 s, then exit.
- **Run (b) short drive:** one lap of the same route as on the PC (or the
  nearest equivalent if streaming makes it impractical): F10 at S, 90 s
  stationary, F10, slow drive to E, F10.
- First launch of `DECK-A` is cold, the second warm; do (a) and (b) warm, and
  say which launch each was.
- Send the session's text files (as in section 4) and F10 packages to the PC
  the same way as the l48 package, into `private\owner-deck-<date>\`.

## 6. Overhead check (agent request, optional for the owner)

To confirm the split costs under 1%, one extra warm 1440p run on BENCH-A with
the split **off**, started with the agent's script (it passes
`--d3d12_perf_split=0`), is compared with a normal warm run on fps and CP
busy time; one run with `--d3d12_perf_split=2` checks the sampling. The agent
prepares these scripts only if the owner agrees to the two extra runs.
