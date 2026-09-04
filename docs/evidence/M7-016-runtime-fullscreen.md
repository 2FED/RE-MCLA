# M7-016 runtime fullscreen shortcuts

## Decision

`runtime-fullscreen-two-shortcuts-pass`

ReXGlue v0.10.0.2 (`492614eec92c31f11d75dd8fa0f09785cbae4a66`)
restores both requested runtime window-mode routes in the SDL window layer:
non-repeated Alt+Enter and left-button double-click.

## Accepted evidence

Real-title Windows run `fullscreen-20260904-093559-ab89fbd3` exercised one
Release executable with deterministic geometry checks:

- initial 960x540 client area was windowed;
- Alt+Enter changed the same process to exact monitor fullscreen geometry;
- one LMB double-click returned the same process to windowed geometry;
- runtime logs contained exactly one source-specific toggle marker for each
  route;
- external `WM_CLOSE` produced exit code 0 without force cleanup.

The source verifier additionally passed one canonical case and eight
fail-closed mutations. The M7-016 portable bundle defaults to `fullscreen =
true` and records all three window requirements in its immutable manifest.

## Limits

This closes the Windows runtime-toggle defect KI-025. It does not prove Proton
or Steam Deck presentation; the exact synchronized M7-016 bundle must still
pass its physical Deck matrix.
