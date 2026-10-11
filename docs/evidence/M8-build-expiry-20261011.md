# M8 signed test build expiry

October 11, 2026 Kyiv. Owner requested the 14-day test-build limit and online
extension while launcher gameplay acceptance waits for morning. Implementation
and bounded native tests pass. The owner subsequently approved publishing only
the signed manifest: live delivery and native extension/cache verification now
pass. Real-title owner acceptance remains pending; no M8 task is closed or
source implementation committed/pushed by this work.

## Exact candidate

Fresh `private/l2` excludes game assets and personal data. Original p1 and l1
were not modified and no owner profile was imported or selected automatically.

- Host: `9195117A71A69F42B505838447C350EB8B0A6A2CC9FA14C535DCB0630240AAEE`
- Launcher: `BE79E0C3269CC80F9736D0066AC01177C4D73B34AE597964DB84259FDE9BB2DB`
- Build manifest: `9C6BEA718046F1B22D7FB9C4BEB8E03CC52DD3F466CCFB926CC585059F580BAA`
- Ticket: `6C2FA6EAE358A2167ADA346B8E424BFC179957F072EC500CA800CFB5F360325A`
- Read-only game revision: `95DA270942B4BDDBC8DC5EB45AA742AC6BE191D74E430022070D9B9B5AF25048`

Host compiled with `MCLA_TEST_BUILD_EXPIRY=ON` in `out/build/m7-final-sdk20`.
Signed creation1791667418, local expiry1792877018; expiration is exclusive.
Receipt inventories17 files, plus the receipt itself; manifest has11 rows.
Version remains0.19.0.0 while task acceptance is open.

## Executed checks

- Canonical policy and BCrypt signatures:1417 checks,0 failures.
- Pure HTTP response validation:26 checks,0 failures.
- Runtime cache/extension/clock fixture:37 checks,0 failures; ephemeral signing
  keys and temporary cache, no live game or owner saves.
- PowerShell signing fixture:66 checks passed; ephemeral keys. Production CNG
  key was separately created once, nonexportable, then used for the real ticket.
- Native controller regression:56 checks passed with synthetic children.
- Rebuilt build-verifier regression:109 checks passed; symlink case SKIP with
  Windows1314 (no privilege), not a symlink PASS.
- Allowlisted staging:63 checks,0 failures/skips under both PowerShell7 and5.
- Structural scan with ast-grep0.45.0 and all3 rule tests pass; tracked diff
  whitespace check passes. Unrelated pre-existing WIP remains uncommitted.
- Locally prepared one-entry grant passed the native production-public-key
  signature/parser check; this verifies the exact signed file, not its deployment.
- Actual copied host admission: missing, forged-signature and valid-signed but
  expired tickets each rejected with the expected message and nonzero exit.
  All three preserved exact case inventories and pinned original hashes. No
  game assets, SDK profile/cache/log directories, network or known-folder lease
  cache were used by these lifecycle-only negative probes.
- Exact staged managed guest-free host:14 checks,0 failures. Production ticket
  verified and managed SDK lifecycle completed using only a temporary empty
  profile. This is not gameplay, save reload, F10 or visible UI acceptance.
- Public endpoint smoke returned non-200 in203ms. No redirect was followed.
  HTTP status code was not exported by that probe; do not label it404. This is
  bounded unavailable-source behavior, not live signed-extension PASS.

Private retained fixtures under `%TEMP%`:

- `mcla-lease-runtime-e607d5e1565a77ec03dffc0d22e9b5f0`
- `mcla-test-build-signing-cf05ee38ea30489a8a0de831b87519cf`
- `mcla-controller-test-566e9e59a0ecda5114f9f467c46625e9` (final rebuild)
- `mcla-build-verifier-test-c1b80208e20bf607e5ecd3c046fdef9d`
- `MclaStagingTest-5a87ea0badc541d1b428ba49339f86c1`
- `MclaStagingTest-8fb5742742304f1f889b8b0fd89e8545` (PowerShell5)
- `mcla-host-admission-d8a1f0df33ff46af8ce0bca44f55809e`
- `mcla-host-probe-527025b9147efbd4dd7c7ef7cc299a0b`

## Review and boundaries

Scoped independent policy, transport and signer work was integrated by the
primary agent. Independent privacy review returned GO for this private candidate
and the final stricter lifecycle-probe delta. Review caught the generic reader's
64-MiB cap, unnecessary duplicate network checks and the fact that environment
redirection cannot isolate Windows known-folder cache access. Final native tests
ran after these corrections, not against an earlier artifact.

RSA verification uses the digest contract documented by
[Microsoft BCryptVerifySignature](https://learn.microsoft.com/en-us/windows/win32/api/bcrypt/nf-bcrypt-bcryptverifysignature).
Async WinHTTP keeps request context/buffers alive through closing callbacks per
[Microsoft concurrency guidance](https://learn.microsoft.com/windows/win32/winhttp/concurrency-in-winhttp).
The fetcher has one outstanding operation, a bounded total deadline and no
unbounded worker join. Cache failure does not replace stronger signed evidence.

The exact public-manifest source remains in
`private/evidence/M8-build-expiry-20261011/active-builds.txt`. M8-018 remains in progress: volunteer export,
tester acceptance and content gates are not completed by expiry implementation.
See [maintainer procedure and limitations](../test-build-expiry.md).

## Authorized allowlist publication

On October11 the owner explicitly approved creating/updating DebugBundle and
pushing only the signed active-builds.txt. Independent read-only publication
review returned GO: the634-byte canonical ASCII payload contains only schema,
issue time, one host hash/expiry and a public signature. Native signature
verification passed again immediately before publication; l2 host hash matches.

Commit `7878d4877fcae1a9d2baf73414c2fe58aa09b3eb` has no parent and exactly one
100644 file named active-builds.txt. Tree `ec3c89a0390cf145705ddf8ab105fdfece869b2c`,
blob `30c52abd1140fd1a13f49778293a849948cb5d51`. Git plumbing avoided the working
index and checkout. The exact commit was pushed with a non-force DebugBundle
refspec after confirming the branch was absent; remote branch identity matches.
No source history, build, key, game data, save or diagnostic file was included.

The fixed public endpoint returned HTTPS200 with payload SHA256
`F8252FEF91FF13C0118681F173AD5B5234065FF06D18D2915BB9EBB7FC7F9872`, matching the
reviewed local file. The production native admission probe against l2 returned
`SIGNED_BUILD_ADMITTED expires_utc=1794086618 extended=1` (November7 at21:23:38 UTC).
The per-host local lease cache has the exact same SHA256. This executed the live
fetch, signature verification, host matching, extension and cache-write path;
offline behavior remains covered by the existing injected-fetch runtime fixture.
No game process was launched and no owner save was used. Main HEAD remains
`69d52284dcfba5d61bcc3dda24a5b2f266366768`; source WIP is still uncommitted.
