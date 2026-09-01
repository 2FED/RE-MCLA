# M6-014 delivery-transition constructor-registry repair

Date: 2026-09-01

Status: CODEGEN AND STATIC REGISTRY PASS; PHYSICAL DELIVERY RETEST PENDING

## Runtime failure

The successor free-roam process in suite `20260831-133236-2cecb67b`
terminated when the owner started a delivery mission. The final runtime marker
at 14:41:24 local time was:

`Call to invalid or unregistered function at guest address 0x8220DA40`

No contemporaneous NVIDIA display-driver or Sunshine application error was
present. This is therefore a guest callable-boundary failure, not the host GPU
failure previously classified as KI-023. The run is not a soak PASS.

The save watcher retained a complete profile snapshot at 14:40:45, 39 seconds
before the failure. Its save SHA-256 is
`A575F88F4FDFA19B084BA3C5DBA3B4A15EBFBDCCBD6CCD09628201A9B84A6F82`;
the embedded counters report 37 completed races and 31 wins.

## Boundary proof

Private Ghidra instruction and reference audits are retained below the failed
scenario root. Their sanitized identities are:

- address audit SHA-256:
  `83AA2CABDEDA2D5760F0E12DBFA84DED52DC0FC00175264D8C39CBFAD5A0C15D`;
- constructor-reference audit SHA-256:
  `CA6AA54F6CC4F4B828B217E5EA6037F2BA82FB58992896294D47348BA10BDF80`;
- failed runtime log SHA-256:
  `0025483A060BF8C52DFD6FACB3490D5E3AAEFDC02703265750D8EFBDB1A35D38`.

`0x8220DA40` begins the independent instruction sequence `lis`, `mr`, `addi`,
then a terminal branch to `0x8220BF08` at `0x8220DA4C`. `0x8220DA50` begins a
different already generated function. The title's constructor registry passes
`0x8220DA40` as an explicit callback alongside the neighboring generated
entries, proving that the crash target is a legitimate function start.

The same 102-target registry audit exposed two other legitimate callbacks that
were absent from generated registration:

| Entry | Exclusive end | Body classification |
| --- | --- | --- |
| `0x8220B7D0` | `0x8220B810` | bounded vtable tail ending in `bctr` |
| `0x8220DA40` | `0x8220DA50` | four-instruction dispatch thunk |
| `0x8220DAA0` | `0x8220DAB8` | five-instruction argument-adapter tail |

Each interval ends immediately before an independently recovered function. No
switch table, invalid range, parent chunk, or broad gap override is introduced.

## Generated result and harness correction

ReXGlue v0.10.0.1 non-force codegen completed all analysis/write phases with
exit 0. The generated corpus contains each exact function body and dispatcher
registration. `scripts/verify-constructor-registry-coverage.ps1` reports 102
constructor targets, 30,034 generated registrations, and zero missing targets.
The resulting 67-file / 65-C++-source corpus contains 133,915,274 bytes and is
bound by private manifest SHA-256
`5CC73DBA886ACD41F4B1AE26DC5D3272E22AFF4DA3E345AE4839243A1C8520D4`.
A clean 71-action Release build produced executable SHA-256
`669D9389CD9414958CF7631848281B5BE4A940DE03BF99D108672FCCFC330BE0`.

The failed run also exposed that synchronous operator prompts let the scheduler
rapidly fabricate delayed resource samples and captures after a long wait.
The runner now polls process exit while reading console input, immediately
reports a title crash at a prompt, resynchronizes future schedule boundaries,
and fails rather than backfilling missed canonical evidence.

Static/codegen acceptance does not replace the physical criterion: KI-024 stays
open until the rebuilt artifact continues through the same delivery transition.
