# M9-005 signed ticket capabilities (2026-10-29)

Status: implemented and tested with synthetic fixtures only. No game,
`mcla.exe` or `MCLA-Launcher.exe` was started, nothing was signed with the
real key, and no volunteer build was produced. Physical acceptance is pending.

## 1. Problem

The tester-guide review ([M9-005-tester-guide-review-20261029.md](M9-005-tester-guide-review-20261029.md))
found that `src/mcla_launcher_capabilities.h` admits 1080p/2160p only for the
two fingerprints `kRenderTargetTestedBuild` and `kCleanRenderTargetTestedBuild`
and Unlocked only for `kUnlockedTestedBuilds`. The only other route is the owner admission file
(KI-045), which exists only on the owner PC. The build fingerprint is the
SHA-256 of `mcla-build.manifest`, and the manifest lists the signed ticket, so
a new volunteer build can never be listed in its own source. Every refreshed
volunteer build therefore offered only 720p/1440p at 30 FPS, against the M9
goal "stock 30 recommended, unlocked experimental" for volunteers.

## 2. Design

The admitted capabilities travel in the owner-signed ticket.

- Ticket payload (V1 or V2): an optional line after the host row,
  `capabilities <token>[,<token>...]`, tokens from `1080p`, `1440p`, `2160p`,
  `unlocked`. 720p and stock 30 FPS have no token. The line is signed with
  the rest of the payload.
- Parser (`src/mcla_build_lease_policy.cpp`): `ParseTicket` accepts at most
  this one extra line; `ParseTicketCapabilities` accepts the tokens in any
  order, each once; an empty list, unknown token (also different case),
  repeated token, empty token, stray spaces or any other extra line make the
  ticket invalid. `Ticket::capabilities` holds `kGrant*` bits (0 without the
  line). `TicketCapabilitiesText` gives the canonical list for audits. The
  host uses the same parser, so a malformed grant also refuses host start.
- Launcher (`src/mcla_launcher_controller_win.cpp`): `ReadTicketGrant` finds
  `mcla-test-build.ticket` in the verified build inventory, reads it, checks
  its SHA-256 against the inventory entry, authenticates it under the
  embedded build-ticket key (a test seam supplies a fixture key in the
  synthetic controller build only) and checks the row names the inventory's
  `mcla.exe`. No ticket in the inventory grants nothing; a present but invalid
  ticket returns an error, so CHECK SETUP and START GAME refuse instead of
  ignoring it. In production `CheckBuildLease` also verifies the same ticket.
- Policy (`src/mcla_launcher_capabilities.h`): `CapabilityRequest` gains
  `ticket_grant`; `ResolveCapabilities` admits a gated option when the
  fingerprint table, the owner admission file or the grant admits it. Each
  token admits only its own option (`1080p` never implies `2160p`, KI-048;
  `unlocked` implies no target). 1440p is not gated today, so the `1440p`
  token is accepted but changes nothing.
- Records: `CapabilityPlan` gains `ticket_granted` and `ticket_grant`.
  `capabilities.txt` ends with `admission ticket-grant` when the grant (not
  the table, not the owner file) admitted an option; `admission
  owner-allowlist` is unchanged and takes precedence when the build is also
  owner-listed. Records without admission stay byte-identical to policy V1.
  CHECK SETUP adds "Admitted by the signed build ticket."; refusal texts for
  ungranted options are unchanged.
- Export (`src/mcla_diagnostic_export_policy.h`): the byte-for-byte rule is
  kept. The export has no ticket, so a record ending in `admission
  ticket-grant` is re-resolved with every token granted and must
  re-serialize to the same bytes: an ungated record claiming the grant, a
  gated one without it, or one with two admission keys is rejected.
- Signing (`scripts/Mcla-TestBuildSigning.ps1`,
  `scripts/New-MclaTestBuildTicket.ps1`): `-Capabilities <string[]>`
  (`ValidateSet`, then a case-sensitive check) writes the line in the
  canonical order `1080p,1440p,2160p,unlocked`; omitted, no line. The signing
  boundary accepts a 4-line ticket payload only with a known, unique,
  canonical-order list.
- Staging (`scripts/Stage-MclaInternalLauncher.ps1`): the ticket shape gate
  allows the optional line, refuses unknown or repeated tokens, prints the
  grant (`Write-Host`, not pipeline output) and records
  `test_build_ticket_capabilities` (array, `[]` for none) in
  `candidate-receipt.json`.
- Audit (`scripts/Test-MclaVolunteerStaging.ps1 -RequireVolunteer`): prints
  "Ticket capabilities (informational): ..."; a finding when the ticket does
  not parse, has an unknown/repeated token, or the receipt does not record
  the same grant. A receipt staged before this change has no field and is
  therefore a finding under `-RequireVolunteer`.
- Owner/volunteer rule (primary decision): volunteer V2 tickets carry
  `-Capabilities unlocked,1080p,1440p` by default (signed line
  `capabilities 1080p,1440p,unlocked`); `2160p` only on the owner's explicit
  request. See [test-build-expiry.md](../test-build-expiry.md).

## 3. Files

Source: `src/mcla_build_lease_policy.h`, `src/mcla_build_lease_policy.cpp`,
`src/mcla_launcher_capabilities.h`, `src/mcla_launcher_controller_win.h`,
`src/mcla_launcher_controller_win.cpp`, `src/mcla_diagnostic_export_policy.h`.

Tests: `tests/mcla_build_lease_policy_test.cpp`,
`tests/mcla_build_lease_runtime_win_test.cpp`,
`tests/mcla_launcher_capabilities_test.cpp`,
`tests/mcla_launcher_controller_win_test.cpp`,
`tests/mcla_diagnostic_export_policy_test.cpp`.

Scripts: `scripts/Mcla-TestBuildSigning.ps1`,
`scripts/New-MclaTestBuildTicket.ps1`, `scripts/Test-MclaTestBuildSigning.ps1`,
`scripts/Stage-MclaInternalLauncher.ps1`,
`scripts/Test-MclaInternalLauncherStaging.ps1`,
`scripts/Test-MclaVolunteerStaging.ps1`.

Docs: `docs/test-build-expiry.md` (new section "Signed launcher capabilities",
owner/volunteer rule, maintainer step 2, envelope paragraph),
`docs/volunteer-tester-guide.txt` (section 2 EN+UK: 720p/1440p at 30 FPS on
every build; 1080p and Unlocked only when the build's signed ticket allows
them, with the CHECK SETUP text; 2160p not offered; the Unlocked limits kept
as a plain line instead of "if a later build offers Unlocked"; other text
unchanged, longest line still 82 characters).

`CMakeLists.txt`, `docs/rexglue-sdk.md` and `docs/build.md` are unchanged.

## 4. Tests added

- Lease policy: missing line = 0 for V1 and V2; `unlocked,1440p` parses with
  the row unchanged; order-insensitive; all four; single token; 22 malformed
  lines rejected (empty list, duplicates, `720p`, `60fps`, wrong case, empty
  token, separators other than comma, extra spaces, misspelt key, missing
  final LF, blank line, two capabilities lines, other extra line); a line
  added or edited after signing fails; the volunteer default on V2; the
  exposed token parser and canonical text.
- Lease runtime: a V2 ticket with the line is admitted exactly like V2; an
  unknown token makes the host refuse.
- Capabilities: for 1080p, 2160p and Unlocked, refused without the grant and
  with every other token, admitted with its own token, record ends in
  `admission ticket-grant` (18/19 keys), stripped/revoked plans cannot
  serialize, owner list keeps its record when both apply; 720p/1440p stay
  unrecorded with any grant and a forged claim cannot serialize; Unlocked at
  1440p with `unlocked,1440p`; Unlocked+2160p refused with `unlocked,1080p`;
  table-admitted builds never record the grant.
- Controller (new `TicketGrantMatrix`, fixture key, V2 ticket
  `capabilities unlocked,1440p` in the manifest): without the key the check
  is Blocked ("Build ticket refused"); stock 720p/1440p Ready without the
  notice; 1080p/2160p Blocked with the unchanged text; Unlocked 720p/1440p
  Ready with "Admitted by the signed build ticket." and no owner claim;
  Unlocked 1080p Blocked; a ticket signed by another key is Blocked even for
  stock; an unknown token is Blocked; START GAME with Unlocked 1440p runs the
  synthetic child, which checks the vblank argument, and `capabilities.txt`
  has 19 keys ending in `admission ticket-grant`; the session completes with
  exit 0. The synthetic child accepts the Unlocked arguments only together
  with that record.
- Export policy: a ticket-granted Unlocked record exports; without the key it
  is rejected; an ungated record claiming the grant is rejected; a record
  with both admission keys is rejected.
- Signing test (`Test-MclaTestBuildSigning.ps1`): canonical V2 line from
  unordered input, no line without capabilities, all four tokens, unknown/
  case/duplicate/empty/comma-joined tokens rejected, and 7 direct-primitive
  payloads with a bad capabilities line rejected. Not run: the script needs
  PowerShell 7 (.NET 5 `Convert.ToHexString`) and this PC has only Windows
  PowerShell 5.1. Substitute check in 5.1 by dot-sourcing the signing
  library without its `#requires` line and an ephemeral `RSACng`: the token
  function and the payload validator gave the expected canonical output and
  all 5 token and 5 payload rejections; the two valid payloads reached the
  RSA signing call and stopped only at `Convert.ToHexString`. All six changed
  scripts parse with no errors in the 5.1 parser.

## 5. Build and runs

- New directory `private/l44-build`, configured by `private/l44-build.ps1` (a
  copy of `private/l39-work/l42_build.ps1` with only the comment and build
  directory changed): Release, Ninja, clang 20.1.8, the l42 flags and prefix
  maps, `rexglue_DIR` at `private/l41-sdk-install` (reports ReXGlue SDK
  0.20.0.6-dev.g8b10d62), `-DREXSDK_VERSION:STRING=` empty as in l42, so the
  new 0.20.0.3 pin check did not block configure. Log
  `private/l44-configure.log`.
- Targets: `mcla`, `mcla_crash_handler`, `mcla_diagnostic_image_helper`,
  `mcla_diagnostic_image_fake_helper`, `mcla_internal_launcher` and the 35
  test targets of `private/l39-work/l41_targets.txt`: 219/219 steps, rc 0,
  0 warnings (`private/l44-build.log`); after the runtime-test edit 2/2
  steps, 0 warnings (`private/l44-build2.log`); a final full build reported
  no work to do.
- Artifacts (SHA-256): `mcla.exe`
  `62885B4E8FB2C7E34EDB29523507854D14A3B161D51A077FED5FF56461E5DB3A`,
  `MCLA-Launcher.exe`
  `A79FB2D45779CFE5CDA3B4E458B623180CF133912CBAEFC993FFEB68B2DF3118`,
  `mcla_crash_handler.exe`
  `55F819AB81FFE53B378C16B03EC072BF2A0A6BED163394FC91A92EACACC3FFE3`,
  `mcla_diagnostic_image_helper.exe`
  `552210A7E1804352733553D66F160870AF87F02497CB644DFC2471D36DAE3FFC`.
  These are verification builds against the l41 SDK install, not candidates.
- Tests (`private/l44-tests.log`): 35/35 exit 0. Changed counts versus l42:
  lease policy 1438 -> 1499, lease runtime 69 -> 71, export policy 585 -> 597,
  capabilities 262 -> 293, controller 375 -> 401. All other suites equal l42
  (export native 1 skip and store 1 skip as before; image client run with the
  fake helper and its SHA-256; adapter policy with `config/mcla.toml.example`).
- Staging (Windows PowerShell 5.1, CRT from `private/l12`): input
  `private/l44-staging-synthetic-input` = the l44 binaries plus a shape-only
  V2 ticket for the l44 host with `capabilities unlocked,1080p,1440p` and a
  zero signature (staging checks shape and host binding, not signatures; this
  ticket authenticates nowhere). `Test-MclaInternalLauncherStaging.ps1`:
  74 checks, 0 failures, 0 skips (11 new: receipt grant, staged grant
  receipt, four malformed-grant refusals before writes); the same with the
  line removed (`private/l44-staging-synthetic-input-plain`): 74/0, grant
  `none`. `Test-MclaVolunteerStaging.ps1 -RequireVolunteer` on both staged
  fixture candidates: 185 checks, 0 findings, grant printed; on a copy with
  `capabilities unlocked,4k`: 2 findings (unknown token, receipt mismatch),
  exit 1. Fixtures are preserved under the user temp folder.
- `ast-grep scan` clean, `ast-grep test` 3 passed, `git diff --check` clean;
  all changed files ASCII except the Ukrainian guide section, CRLF.
- DGX Spark: no job sent. The task was one bounded code change checked by
  deterministic native and script tests; no extraction or log triage of a
  size worth offloading.

## 6. Limits and open points

- Not physically tested: no real signed ticket with the line exists yet, and
  the launcher's production `kPublicKey` path was exercised only through the
  same parser and `CheckBuildLease` (the controller test uses the fixture-key
  seam). The first volunteer candidate signed with `-Capabilities` needs the
  usual owner checks.
- The signing test needs PowerShell 7; it was not run here (see section 4).
- A ticket grant is a policy decision, not a quality certificate: Unlocked
  keeps its known limits (KI-041, KI-043) and 1080p keeps KI-033.
- Older launchers and hosts reject a ticket with the line (they require
  exactly one row). This is harmless because a ticket is bound to one host
  hash, but an old build must never be re-ticketed with `-Capabilities`.
- `1440p` is a no-op token while 1440p is ungated; if 1440p is gated later,
  tickets signed without it lose 1440p, so the volunteer default includes it.
