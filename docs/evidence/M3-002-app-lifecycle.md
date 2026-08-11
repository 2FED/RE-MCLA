# M3-002 application lifecycle

Date: 2026-08-11

Result: PASS — PROJECT LIFECYCLE EXERCISED; WINDOWS UNICODE-PATH CRASH FIXED

## Lifecycle contract

`MclaApp` is the project-owned `rex::ReXApp` subclass. The native entry point
registers `MclaApp::Create`, while the subclass keeps only the hooks currently
needed by the port:

- `OnPostInitLogging()` records that host logging is available;
- `OnFinalizePaths()` provides an opt-in `--mcla_lifecycle_probe` path that
  requests a clean UI-thread quit before guest runtime construction;
- `OnShutdown()` records controlled host shutdown.

The probe is deliberately guest-free. It verifies host application ownership,
SDK initialization, path setup, deferred quit, and shutdown without claiming
module boot or game-code execution. Static regression coverage keeps the entry
point, hook set, generated-code boundary, and four ordered markers intact.

## Initial host failure and diagnosis

The first real RelWithDebInfo probe logged `MCLA lifecycle: logging ready` and
then terminated before `OnFinalizePaths` with Windows status `0xC0000409`.
Windows Error Reporting identified `ucrtbase.dll` fast-fail subcode 7. A CDB
second-chance exception trace localized the actual cause:

```text
std::_Throw_system_error_from_std_win_error
std::_Convert_wide_to_narrow (... code page _Acp)
std::filesystem::path::string
rex::ReXApp::SetupEnvironment (rex_app.cpp:189)
```

The SDK converted a Windows Documents-derived path with
`std::filesystem::path::string()`. On this host the resolved path is equivalent
to `%USERPROFILE%\OneDrive\<localized Documents>\mcla`; its Cyrillic component
is not representable by the active ANSI code page. The conversion throws, the
uncaught exception reaches `std::terminate`, and the runtime reports the
secondary fast-fail status. Raw logs and debugger transcripts remain under
ignored `private/evidence/M3-002/` and are not publication evidence.

## ReXGlue SDK correction

The project fork replaces all nine host-path `.string()` conversions in
`src/ui/rex_app.cpp` with the SDK's UTF-8 helper `rex::path_to_utf8()`. A unit
test round-trips a non-ASCII Windows path (`MCLA-Дані`) through UTF-8. The
reviewed SDK identity is:

- release: `v0.9.0.2`
- commit: `14c901c6c58dc5791985a116575a2cc59849d2fe`
- subject: `fix(ui): encode host paths as UTF-8`

Review and regression results:

- clang-format dry-run with `--Werror`: pass;
- focused non-ASCII filesystem test: 1 case / 2 assertions, pass;
- complete PPC suite: 1,459 cases / 5,733 assertions, pass;
- complete discovered unit suite: 9 failures among 251 cases, with 4 other
  cases skipped. All nine failures reproduce with the pre-hotfix Release test
  binary and are unrelated template/migration fixture drift; the new focused
  test passes. No regression was attributed to the UTF-8 change.

The SDK was installed in Debug, Release, and RelWithDebInfo configurations.
The installed CLI and CMake package both report `0.9.0.2`.

## Final verification

The final Release build used the exact `0.9.0.2` package and consumed the
accepted ignored generated corpus.

| Property | Result |
| --- | --- |
| Configure and build | pass, 67/67 Ninja actions |
| Generated C++ objects | 62/62 |
| Generated manifest | 64 files / 128,031,984 bytes |
| Executable size | 41,977,856 bytes |
| Executable SHA-256 | `CD436F75870F34AE7AB7FD926611CAB73716043A9DE50924F880C4B60223E46E` |
| Lifecycle exit code | 0 |
| Ordered markers | 4/4 |
| Guest runtime | skipped by design |
| Private log SHA-256 | `87EF7009B51953CBE80B09F5F4A559CCCA0E01E9EC0FD69E635BEAF2DE97CB35` |

The earlier RelWithDebInfo verification also exited 0 with the same four-marker
contract. Neither run entered guest code or modified private game data.

## Upstream duplicate audit

Before preparing an upstream report, open and closed issues and pull requests
in `rexglue/rexglue-sdk` were searched for `path.string`, `path_to_utf8`,
`SetupEnvironment`, filesystem conversion errors, Unicode/non-ASCII paths, and
user-data-root crashes. All 217 issues and 183 pull requests were included in
the audit. Issue #383 / PR #384 concerns configuration-load ordering, and issue
#318 / PR #332 concerns locale-dependent TOML numbers; neither is a duplicate.
Upstream `main` still contains the nine unsafe conversions and no equivalent
fix. A sanitized issue payload is prepared privately and must only be published
after explicit approval of that exact public payload.

## Acceptance decision

M3-002 is complete. The project owns a minimal documented ReXApp lifecycle,
the real host path reaches controlled shutdown on a non-ASCII Windows profile,
the original S1 failure is root-caused and removed, and the regression is
covered without claiming guest module boot. M3-003 owns guest memory, dispatch,
and executable-entry configuration.
