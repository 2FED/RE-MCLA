# M6-016 alpha compatibility matrix

## Decision

M6-016 records a `bounded-single-host-alpha-compatibility-matrix`. The public
matrix names the exact tested title/runtime build, reference hardware, verified
paths, representative-only paths, open limitations, and every current known
issue ID. It deliberately does not turn untested systems into compatibility
claims.

## Public artifacts

- `config/alpha-compatibility-matrix.json`: authoritative structured matrix;
- `docs/alpha-compatibility.md`: user-facing route and hardware summary;
- `docs/known-issues.md`: canonical issue descriptions, workarounds, owners,
  severity, status, and target;
- `scripts/run-alpha-compatibility-matrix.ps1`: one-command public report gate;
- `scripts/verify-alpha-compatibility-matrix.ps1`: fail-closed consistency gate;
- `scripts/test-alpha-compatibility-matrix.ps1`: positive and negative fixture
  coverage.

## Bounded scope

The reference system is Windows 11 Pro x86-64 build 26200, Ryzen 9 5900X, RTX
3090, and D3D12. Physical input coverage names the exact gamepad paths and the
T300RS wheel; other SDL wheel models are configuration-compatible only.

The matrix separates verified, verified-with-limitations,
representative-only, bounded-offline, in-progress, and not-verified rows. It
keeps M6-014 soak completion false, full-campaign and online-service claims
false, and alternate-OS/GPU, cross-model wheel, graphics-parity, and exact
audio-mix claims false.

All current `KI-001` through `KI-025` entries are mirrored by ID, severity, and
status from `docs/known-issues.md`; the initial M6-016 closure covered the first
twenty, and later M6-014 findings extend the same fail-closed inventory. The
full descriptions remain in one canonical register rather than being
duplicated into the matrix.

## Acceptance

The task is accepted only when the verifier proves:

- exact project, SDK, title, and source-image identity;
- one named reference OS/CPU/GPU/D3D12 path;
- exact route IDs and bounded status vocabulary;
- every cited public evidence file exists inside the repository;
- every route has at least one explicit limitation;
- every known-issue row matches the canonical register exactly;
- all broad unsupported claims remain false;
- the user-facing document contains the tested build, hardware, route table,
  known-issue link, and status definitions.

Private evidence is not published. The matrix provides public traceability to
the already reviewed milestone reports without exposing saves, logs, user
paths, or controller identifiers.
