# Internal test build expiry

Internal Windows test candidates admit new launches for 14 days from their signed
UTC creation time. A signed public grant can extend that deadline. Expiration
never stops an already running game, deletes data or prevents using saves with a
new eligible build. This is a test-build freshness policy, not DRM or anti-cheat.

## Launch policy

Both the internal launcher and the expiry-enabled host check
`mcla-test-build.ticket`. It binds the exact SHA256 of `mcla.exe` to its creation
time using RSA2048 PKCS1 SHA256. Renaming/copying/restaging files does not renew it.
The launcher also verifies its immutable build inventory. A missing, malformed,
incorrectly signed or wrong-host ticket rejects startup before SDK save setup.

The online source is the fixed public HTTPS resource
`https://raw.githubusercontent.com/2FED/RE-MCLA/DebugBundle/active-builds.txt`.
Requests send no save, profile, hardware identifier, local path, API key or build
hash; the client downloads the common manifest. GitHub still receives ordinary
network metadata such as the IP address. TLS verification stays enabled, and
redirects, cookies, authentication and proxies are disabled. A five-second total
network deadline and 64 KiB response limit bound the check.

Each signed entry grants one host hash an explicit `valid_until` UTC timestamp.
A verified extension is cached outside builds/saves in
`%LOCALAPPDATA%\MCLA-R-TestLeaseCache-v1`, and remains usable offline until that
deadline. Missing/shorter entries and untrusted responses cannot reduce a cached
grant. Removing a hash is therefore not immediate revocation. An unavailable or
busy cache can still allow the current launch using a valid online grant, but
offline persistence is not promised. The managed host rechecks the proof and
normally avoids a second request; if the launcher's extension could not be
cached, an expired host may need its own bounded request.

At the deadline, startup reports `BUILD EXPIRED - GET THE NEW ONE`. A network
failure neither expires a still-valid local allowance early nor unlocks an
expired build. UTC is checked again after the request. A future creation time or
backwards clock movement during admission is rejected.

## Kill-switch builds (V2 tickets, M9-005)

Volunteer builds are signed with `New-MclaTestBuildTicket.ps1 -KillSwitch`,
which writes a `MCLA-TEST-BUILD-V2` ticket with the same row as V1. The schema
alone selects these rules, and a V2 ticket never falls back to the 14-day V1
allowance. Owner decisions 2026-10-28 ([M9 priorities](M9-priorities.md)):

- **Online check is authoritative.** If `active-builds.txt` downloads and its
  signature verifies, a host listed with `valid_until` in the future may start.
  A host that is missing or past its `valid_until` is refused with "This test
  build was withdrawn". Its cached check is replaced by a local `revoked`
  record, so the build also stays refused offline, even inside its first
  3 days. A later list that names the host again restores it.
- **Offline start:** allowed while the build is less than 3 days old (signed
  creation time), or within 24 hours of the last online check that listed it.
  The 24-hour window never exceeds that grant's `valid_until`.
- **Network failure, timeout, a malformed list, a list signed by another key or
  one issued more than 300 seconds in the future** never revoke and never
  extend: only the offline rule applies.
- Every listing check rewrites the cache. The record is
  `%LOCALAPPDATA%\MCLA-R-TestLeaseCache-v1\<host>.killswitch` and holds the
  local check time plus the exact signed list. The managed host follows a
  check younger than 24 hours instead of sending a second request.
- Expiry or withdrawal never stops a running game and never touches saves.
  Refusal messages say saves are unchanged.

Limits as before: the local clock and the unsigned check time in the cache are
trusted; clock rollback and patched binaries are not defended. This is a
freshness and withdrawal switch, not DRM.

Publishing: every volunteer host hash must be listed in `active-builds.txt` on
`DebugBundle` before testers go online, or the build is refused. Publishing that
file needs the owner's explicit approval each time; nothing is pushed from
the agent without it.

## Signed launcher capabilities (ticket line, M9-005)

A V1 or V2 ticket may carry one optional payload line after the host row:

    capabilities <token>[,<token>...]

Tokens come from the fixed set `1080p`, `1440p`, `2160p`, `unlocked`; 720p and
stock 30 FPS are always available and have no token. The line is part of the
signed payload, so it cannot be added or edited after signing. The native
parser (`ParseTicketCapabilities` in `src/mcla_build_lease_policy.cpp`)
accepts the tokens in any order, each once. An empty list, an unknown token
(including different case) or a repeated token makes the whole ticket
invalid, so the host and the launcher refuse it; it is never silently
ignored. A ticket without the line grants nothing and behaves as before.

The launcher reads the ticket listed in the verified build inventory, checks
its bytes against the inventory hash, its signature and its host row, and
admits a gated option when the fingerprint table, the owner admission file
(KI-045) or the ticket grant admits it. Each token admits only its own option:
`1080p` never implies `2160p`, and `unlocked` never implies a target.
`1440p` is accepted for forward compatibility; 1440p is not gated today, so
the token changes nothing. When the grant (not the table and not the owner
file) admitted an option, CHECK SETUP adds "Admitted by the signed build
ticket." and `capabilities.txt` ends with `admission ticket-grant`; the owner
file keeps `admission owner-allowlist` and takes precedence. Refusal texts for
ungranted options are unchanged. The diagnostic export rebuilds such a record
byte for byte.

`New-MclaTestBuildTicket.ps1 -Capabilities <tokens>` writes the line in the
canonical order `1080p,1440p,2160p,unlocked`; the signing boundary refuses
unknown, repeated or out-of-order tokens. Staging prints the grant, records
it as `test_build_ticket_capabilities` in `candidate-receipt.json` and refuses
a malformed line; `Test-MclaVolunteerStaging.ps1 -RequireVolunteer` prints it
and fails when the ticket does not parse or the receipt disagrees.

## Owner-internal versus volunteer tickets (owner rule 2026-10-29)

- **Owner-internal candidates use plain V1 tickets:** signed without
  `-KillSwitch`, 14 days from the signed creation time, no kill switch and no
  online requirement: the launcher still asks for the signed list, but only a
  listed grant can extend the 14 days; being offline or unlisted never refuses
  the build inside them. They carry no tester guide and nothing is published
  for them. Their owner-only prepare script signs the V1 ticket with
  `New-MclaTestBuildTicket.ps1`, asserts the first line `MCLA-TEST-BUILD-V1`
  and stages with `Stage-MclaInternalLauncher.ps1`.
- **Volunteer candidates use V2 (kill-switch) tickets** and the rules above.
  Their prepare script signs with `-KillSwitch`, signs the new list with
  `New-MclaTestBuildLeases.ps1`, stages with `-TesterGuide` (the guide becomes
  `TESTER-GUIDE.txt`), verifies the list with the lease probe and runs
  `Test-MclaVolunteerStaging.ps1 -RequireVolunteer`. Publishing the list stays
  a separate step that needs the owner's explicit approval.
- **Volunteer V2 tickets carry the launcher grant by default** (primary
  decision 2026-10-29): sign with `-KillSwitch -Capabilities unlocked,1080p,1440p`,
  which writes `capabilities 1080p,1440p,unlocked`. `2160p` is added only on the
  owner's explicit request for that build (KI-048: over the 8 GB video-memory
  budget); nothing implies it. Owner-internal V1 tickets may carry a grant
  too, but the owner admission file remains the owner-only route.
- The prepare scripts live in the ignored owner workspace, are run by the
  owner in PowerShell 7 and are not part of any staged folder. Volunteer
  packages never carry owner-only launcher settings: the owner admission file
  is never staged, and gated options reach volunteers only through the signed
  ticket grant above.

## Maintainer procedure

Signing scripts require Windows PowerShell 7.1 or later with .NET 5 or later.
The existing current-user CNG key is named `MCLA-R-TestBuild-Leases-v1`, in the
Microsoft Software Key Storage Provider. Its private part is nonexportable and
must never enter the repository or a candidate. The public key is embedded in
`src/mcla_build_lease_public_key.h`. Do not rerun the key-creation script during
ordinary builds. Losing this Windows user's key storage requires a new signing
key and newly built hosts; the repository is not a private-key backup.

1. Build the final Windows host with `MCLA_TEST_BUILD_EXPIRY=ON`. The option is
   OFF for ordinary developer builds; a signed ticket alone cannot enable it.
2. Run `scripts/New-MclaTestBuildTicket.ps1 -HostPath <final-mcla.exe>
   -OutputPath <new-ticket-path>` (volunteer: add `-KillSwitch -Capabilities
   unlocked,1080p,1440p`). Place the resulting ticket beside the final
   host before staging. Outputs use create-new semantics. Keep old signed
   artifacts immutable; never refresh a ticket merely to restage the same build.
3. Stage a fresh destination with `scripts/Stage-MclaInternalLauncher.ps1`.
   Staging checks ticket shape and host binding, not its cryptographic signature.
   Run native policy/runtime gates, `Test-MclaTestBuildHostAdmission.ps1` against
   the staged inner build, and the exact staged guest-free host probe. These
   establish signature verification and compiled-in rejection, not gameplay.
4. To extend a build, prepare JSON containing only approved records, for example
   `[{"host_sha256":"<64 uppercase hex characters>","valid_until":1794086618}]`.
   Run `scripts/New-MclaTestBuildLeases.ps1 -EntriesPath <json>
   -OutputPath <new-active-builds.txt>`. Review every hash and expiry first.
5. Check that exact file using `mcla_build_lease_probe_win.exe --verify-leases
   <absolute-file-path>` from the developer build. This uses the host's public
   key and native parser without contacting the server or touching saves.
6. Publish only the signed `active-builds.txt` to `DebugBundle` after the owner's
   separate early-publication approval. Never include binaries, game data,
   signatures' private key, JSON with private fields, saves or diagnostic files.
   Then verify live delivery, exact public bytes and native signed-extension
   acceptance. Do not infer successful deployment from a push alone.

The file envelope uses canonical ASCII with LF, a versioned payload, the literal
`--SIGNATURE--` line and a 512-character uppercase hexadecimal signature. Ticket
payloads contain host hash and creation seconds, optionally followed by the
signed `capabilities` line. Lease payloads contain issue
seconds followed by up to 256 unique, sorted hash/expiry records. The signing
tools enforce the canonical format; manual edits invalidate the signature.

## Limits and current candidate

The policy relies on the local clock. Clock rollback between runs, patched
binaries and replay of an unexpired signed grant are outside its protection.
The host identity does not authenticate all DLLs; the launcher's separate
inventory is an integrity check, not a DRM trust boundary. Host hashing currently
rejects files larger than 64 MiB. Proxy-only networks fall back to local allowance.

The lifecycle-only diagnostic mode is stricter: it verifies the ticket and
14-day allowance without cache/network extensions and suppresses error dialogs.
It does not start the guest. It is not an expiry bypass or a normal gameplay path.

Candidate `private/l2` was issued October 10 at 21:23:38 UTC (October 11 at
00:23:38 Kyiv). Its local allowance ends October 24 at 21:23:38 UTC (October 25
at 00:23:38 Kyiv). On October11 the owner authorized publication of the signed
allowlist. The one-file DebugBundle root commit
`7878d4877fcae1a9d2baf73414c2fe58aa09b3eb` now extends this host to November7
at21:23:38 UTC. Public HTTPS delivery, exact bytes, native signature/admission
and local cache persistence passed; no actual game was launched for this check. See
[exact evidence](evidence/M8-build-expiry-20261011.md) and
[morning launcher test](internal-launcher-owner-test.md).
