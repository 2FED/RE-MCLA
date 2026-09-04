# M7-016 Steam Deck performance diagnostic

## Decision

`causal-deck-performance-trace-ready-physical-rerun-required`

MCLA-R `0.10.0.0` pins ReXGlue `v0.11.0.0` commit
`b2afb8bc0ea06e62eea9d755a83772b0cdaad972`. The current private child is:

`mcla-0.10.0.0-win64-3A1EAE5DD2BCC311-C974C2030F5182B7-58B3385C89D9E499`

Its immutable manifest SHA-256 is
`78C3F126A54FC389CF714D686E830C625EB511C062C7EF99EA04B10681BA36D0`;
the 25-file hash lock SHA-256 is
`1A0BC3433A6736E5F2114266BC61A97D29AE2D3291CAE3D01581CC9851DDFAA5`.
Clean Windows build, relocation, diagnostics, launcher verification, and
current-child-only retention passed in private run
`bundle-20260904-142733-601f6fc9`.

## Instrumentation

The first successful F10 after D3D12 setup arms one bounded 300-frame audit.
Timestamp resources are created during graphics setup, but timestamp queries
and command-processor CPU measurement are submitted only while the audit is
active. Repeated F10 cannot reset an in-flight audit. A later F10 package
contains the completed per-frame trace in its hash-verified log tail.

`scripts/analyze-portable-deck-performance.ps1` selects a monotonic pair of
complete F10 packages from one process and reports:

- guest output and guest vblank deltas and rates;
- frame-interval p50/p95/max;
- renderer command-processor CPU and D3D12 GPU p50/p95/max;
- streaming reads/stalls, audio underruns, shader translations, and pipeline
  creations;
- a conservative GPU, renderer-CPU, outside-render/pacing, or inconclusive
  classification.

The renderer CPU measurement is not whole-guest CPU time. A low renderer CPU
and GPU result therefore classifies the remaining delay as outside render work
or pacing; it does not by itself prove a specific guest thread is responsible.

## Gates

- ReXGlue clean Release build/install: PASS.
- Focused ReXGlue frame-audit tests: 11 assertions / 3 cases PASS.
- Existing performance telemetry contract: 1 positive / 14 fail-closed
  negatives / 22 source checks PASS.
- Deck analyzer fixture: 1 positive / 2 fail-closed negatives PASS.
- Portable bundle fixture: 1 positive / 13 fail-closed negatives / 34 source
  checks PASS.
- Build-matrix fixture: 1 positive / 6 negatives PASS.

No MangoHUD correction or performance improvement is claimed yet. Physical
closure requires recording the exact Proton version, pressing F10 in saved
gameplay, driving actively for 30–45 seconds, pressing F10 again after the
audit completes, and returning the synchronized folder with the observed
MangoHUD and perceived frame rates.
