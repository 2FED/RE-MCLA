# M9-004 Setup categories (2026-10-28)

**Status:** implemented, with tests passing. Staged as `private/l37`. Pending: the
owner's two-minute UI review. Carries M8-007 ("compatibility, accessibility
and cheat categories are visibly separated").

## Before

The l12 launcher (ported in M9-002) already had three read-only category
lines on the Setup page. They were run-on sentences in one block
("Compatibility: ...", "Accessibility: ...", "Cheats: ...") across the full
width. They read as one paragraph, not three separate categories.

## Change

Each category is now a label|value row, in the same two-column layout as the
build and game-data folder rows above:

| Label | Value |
|---|---|
| Compatibility | Fixes come with the selected build. Frame rate (30 / Unlocked) is on the Play page. |
| Accessibility | No launcher overrides; use the game's own options. |
| Cheats | No launcher controls or cheat arguments. |

- The compatibility row points to the one real compatibility choice (the M9-003
  Frame rate option) instead of saying there are no switches.
- The rows stay read-only, Setup-only and without focus stops, as before. No
  new options were added (M9 scope).

## Tests

- The native UI test checks the exact wording, Setup-only visibility,
  read-only style, fit without clipping, no overlap with each other or the
  footer, unchanged window geometry, no backend dispatch, and (new) alignment
  of each value with the form's value column. 1096 checks, 0 failures.
- All 19 launcher, export and lease suites pass; ast-grep clean; staging
  tests 63/0/0.
- The synthetic UI renderer (`mcla_launcher_ui_render_win`) did not link after
  the M9-002 port (missing export dialog source); its CMake target is fixed and
  it renders the Setup page (`private/l37-setup-page-render.png`).

## Staging

`private/l37`: the l36 build (`0789DBB5...`) with launcher `FBA7AB49...`.
