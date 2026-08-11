# M2-016 ReXGlue vector-pack regression

Date: 2026-08-11

Result: PASS — PROJECT SDK FORK REQUIRED

## Pinned implementation

- Fork: `https://github.com/2FED/rexglue-sdk.git`
- Branch: `mcla/float16-mask3`
- Release tag: `v0.9.0.1`
- Commit: `583c8ff35cde3818992fc78d936c635bca092a6b`
- Upstream base: `rexglue/rexglue-sdk` v0.9.0 at `3eb9b511b4140d2769e27be63eae57d41bfa2afa`
- Upstream `main` checked at `cb58065c793429aa92895d778af58d12e9d26d8f`; it retained the same validation gap and lacked mask-3 FLOAT16_4 coverage.

The fork changes only `build_vpkd3d128` operand validation for format 5. Mask 3 is accepted alongside mask 2 while the existing `shift <= 2` bound is preserved. A PPC assembly case encodes `vpkd3d128 v4, v3, 5, 3, 0` as `0x18971E10` and requires the same four half-float results as the existing mask-2 case.

## Test results

| Gate | Before | Fork v0.9.0.1 |
| --- | ---: | ---: |
| Focused `*vpkd3d128*` | 16 cases, 128 assertions passed | 17 cases, 136 assertions passed |
| Complete ReXGlue PPC suite | 1,458 cases, 5,725 assertions passed | 1,459 cases, 5,733 assertions passed |
| MCLA non-force codegen exit | 0 | 0 |
| MCLA FLOAT16_4 warnings | 20 | 0 |
| Generated files / bytes | 64 / 128,031,984 | 64 / 128,031,984 |

The final Release CLI reported `0.9.0.1`. Its SHA-256 is `E9D60CF784B7D954358629E4B9B4B16EC84A8E4B0E732BFF156468A78FBD6410`; the final PPC test binary SHA-256 is `A28DDD454EEE62D6A6A0CF5C8BC6585EDABF9F83EBD96A39738B84F2B7369378`.

## Generated-code parity

`scripts/run-rexglue-vector-regression.ps1` performs a clean non-force generation into ignored private evidence, rejects any FLOAT16_4 warning, and compares every output path, size, and SHA-256 against the accepted M2-012 clean manifest.

- Result: 64/64 files byte-identical
- Total: 128,031,984 bytes
- Generated-manifest SHA-256: `F8A97618215848CA3B2279725DE77D8AF556E6B2E02AF1CA66D51A812E60F933`
- Accepted-manifest SHA-256: `F8A97618215848CA3B2279725DE77D8AF556E6B2E02AF1CA66D51A812E60F933`
- Private run metadata SHA-256: `E7D3FBC631E2E9AED0A2418176C177A3C589179E08642A4A7CCB27E7663F51E7`
- Private stderr SHA-256: `AF50904309C336F467C35C5391BF8DB92353049EEA488A78FD83B0708C04EE23`

This proves the fork removes a false-positive diagnostic without changing generated MCLA semantics.

## Review correction

An initial local predicate restricted FLOAT16_4 to shift 0 and the project gate immediately exposed 47 new warnings. The failed ignored run was retained privately, the predicate was corrected to preserve the upstream `shift <= 2` behavior, and all SDK and MCLA regressions were rerun. No failed predicate was published or pinned.

## Acceptance decision

M2-016 is complete. The twenty MCLA sites have direct semantic coverage, the complete applicable SDK suite passes, generated output is unchanged, and the exact fork commit/tag is publicly fetchable. M2-017 must therefore evaluate feasibility as `GO WITH SDK FORK` unless the same fix lands upstream before milestone closure.
