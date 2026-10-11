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
   -OutputPath <new-ticket-path>`. Place the resulting ticket beside the final
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
payloads contain host hash and creation seconds. Lease payloads contain issue
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
