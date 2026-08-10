# M2-007 conservative codegen policy

Date: 2026-08-11

The first MCLA analysis uses pinned ReXGlue SDK commit `3eb9b511b4140d2769e27be63eae57d41bfa2afa`. Its manifest loader passes the inline `[entrypoint]` table to `RecompilerConfig::LoadFromTable`, whose parser recognizes nine optional code-generation boolean keys. All nine are now explicit in `mcla_manifest.toml` and set to `false`:

- `skip_lr`, `skip_msr`
- `ctr_as_local`, `xer_as_local`, `reserved_as_local`, `cr_as_local`
- `non_argument_as_local`, `non_volatile_as_local`
- `generate_exception_handlers`

No `[analysis]`, manual function, switch-table, mid-assembly hook, or `rexcrt` override is present. The includes list remains empty. Therefore M2-008 will measure the analyzer with its stock pinned thresholds and without register-localization, skipped state handling, generated exception wrappers, layered configuration, or manual recovery hints.

`scripts/verify-first-analysis-policy.ps1` checks the exact SDK commit, explicit manifest values, corresponding parser keys, false C++ defaults, and absence of tuning/hint sections. `scripts/verify-rexglue-manifest.ps1` independently enforces the complete public manifest schema and exact target identity.

Closure evidence:

- conservative policy gate: PASS (9/9 flags explicit and disabled)
- exact manifest gate: PASS
- independent Python `tomllib` values: PASS
- negative enabled-flag fixture: PASS (rejected)
- negative analysis-section fixture: PASS (rejected)
- structural review and rule tests: PASS
- repository bootstrap: PASS (12/12)

M2-008 may now perform the first non-force codegen. It must not add `--force`, optimization flags, analysis tuning, includes, or manual hints.
