# Testing

Owner: MCLA-R maintainers

Purpose: define test layers, required evidence, regression routes, supported environments, and milestone-specific quality gates.

Current structural checks:

```powershell
ast-grep scan
ast-grep test --skip-snapshot-tests
```

Current rule fixtures cover unbounded `strcpy`, `sprintf`/`vsprintf`, and `strcat` calls.

Current M1 environment/source gate:

```powershell
powershell.exe -NoLogo -NoProfile -ExecutionPolicy Bypass `
  -File C:\BDU\MCLA-Recomp\scripts\bootstrap.ps1
```

The gate must report all 12 prerequisites. Its normal route must pass 12/12, and a deliberately missing required-tool override must return nonzero after reporting the remaining checks. It includes full ISO and extracted-payload hashes.

Planned test layers:

1. host utility unit tests
2. ReXGlue/PPC tests when SDK code changes
3. structural analysis
4. startup smoke tests
5. frontend and first-race routes
6. save/load and soak tests
7. full campaign matrix
8. clean package tests

Update this document when any command, fixture, baseline, supported hardware configuration, canonical route, or closure gate changes.
