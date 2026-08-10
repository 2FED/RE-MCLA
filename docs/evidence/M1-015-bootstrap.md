# M1-015 aggregate bootstrap gate

Date: 2026-08-11
Result: PASS

## Implementation

`scripts/bootstrap.ps1` is a read-only prerequisite validator. It does not install, download, repair, delete, extract, or overwrite anything. It reports each required check independently and returns a failing process code if any check fails.

The default gate covers:

1. four-part public `VERSION`
2. Visual Studio C++ tools, CMake, Ninja, and clang-cl through the deterministic resolver
3. pinned ast-grep
4. exact ReXGlue commit/tag, clean tracked SDK tree, and complete recursive submodules
5. installed ReXGlue CLI version
6. pinned XDVDFS extractor executable hash
7. supported ISO, XEX, title ID, and media ID
8. exact extracted inventory and all 15 payload hashes
9. pinned Xenia Canary executable hash
10. pinned Temurin JDK version/hash
11. Ghidra/XEXLoaderWV versions and loader JAR hash
12. RenderDoc version/hash

Explicit path parameters allow nonstandard private layouts and safe negative testing. Defaults describe the verified MCLA-R host setup. PATH changes used to make Git's recursive submodule helper deterministic are process-local and disappear when the bootstrap process ends.

## Fresh-shell positive test

Command:

```powershell
powershell.exe -NoLogo -NoProfile -ExecutionPolicy Bypass `
  -File C:\BDU\MCLA-Recomp\scripts\bootstrap.ps1
```

Result:

```text
Bootstrap summary: 12 passed, 0 failed, 12 total.
MCLA-R prerequisite validation passed. No installation or host mutation was performed.
```

The run independently rehashed the 7.84 GB source ISO and all 6.57 GB of extracted payload. ReXGlue reported v0.9.0 at commit `3eb9b511b414` with 26 initialized recursive status entries.

## Missing-tool rejection test

The same fresh-shell command was run with `-AstGrepPath` pointing to a deliberately nonexistent ignored fixture. The bootstrap reported that specific check as `FAIL`, continued through and reported all other prerequisites as `PASS`, summarized `11 passed, 1 failed, 12 total`, and returned process exit code `1`.

The test did not create the missing fixture or attempt to install ast-grep. This proves both required behaviors: report every prerequisite and fail automation when a required tool is absent.

## Review notes

- No secret, proprietary file content, disc key, or decrypted data appears in the report.
- Overrides change only the path being validated; they cannot bypass version/hash requirements.
- Full content hashing is deliberately the default milestone gate. A fast mode is not provided because it could silently weaken source identity.
