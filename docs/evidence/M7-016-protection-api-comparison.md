# M7-016: same-Deck protection API diagnostic

A guest-free Linux/Proton/Linux sequence completed normally on the same Steam
Deck. This measures repeated protection changes on a small private allocation,
not the game's fault pipeline or potential native-Linux FPS.

| Pages (4 KiB each) | Linux A median us/call | Proton median us/call | Linux C median us/call |
|---|---:|---:|---:|
| 1 | 1.254 | 2.726 | 1.250 |
| 8 | 2.036 | 3.410 | 2.014 |
| 16 | 2.883 | 3.013 | 3.015 |
| 64 | 3.768 | 3.876 | 2.715 |

Each cell contains five fresh-allocation trials, each with 2,000 warmup and
20,000 timed read-only/read-write roundtrips (40,000 timed calls). All bytes
are checked before warmup, after warmup and after timing; release precedes
output. Every benchmark arm reports 800,000 timed calls, 80,000 warmup calls,
5,468,160 checked bytes and successful release. No deliberate protection fault,
executable scratch mapping, game assets or owner profile was involved.

Spread matters: Proton's eight-page trials range 2.372-3.416 us/call, and Linux
C's 64-page trials range 2.709-3.919. Reported CPU frequency is sampled only
before and after each arm; affinity, thermal state, scheduling and effective
frequency are not controlled. Do not present the medians as universal constants.

The one-page Proton median is about 2.18 times the native median; the 16-page
results are close. This experiment does not reproduce a universal 7-12 times
penalty from protection APIs alone. The game's previously observed per-event
PC/Deck ratios include different hardware and memory layouts, alias handling,
locks, faults, callbacks and contention. Those differences remain unresolved.
VMA/TLB behavior and platform API implementation also differ here. These timers
cannot establish how much a native MCLA port would improve FPS.

## Identity, execution and verification

One original freestanding C source produced an ELF with direct Linux syscalls
and a PE importing ten Kernel32 functions. Independent source and binary review
verified syscall operands, entry ABI, loop counts, non-executable Linux stack
and absence of CRT or hidden runtime dependencies.

Source SHA256: 06FA0F18C1D240F772B08ACEC2EE4D7FCCB8D54065478F0FE601792762D8EBFA.
ELF SHA256: B7C5920A2E77617F2379CE2AFAD5973FE56171C3A1E13B1F75A06F6F97CE5264.
PE SHA256: 3EAE5E22EDC6DE6A8181409874905ACB1B453A2B0AE5AAA67D1123D19C01066D.
Runtime manifest: 8222680D3B6ABB3422B1EFB32CFBB5122FE17F178B9E370A8F784B5EA0F746C3.
Kernel: 6.16.12-valve24.5-1-neptune-616-gb2f7cfe85e45.
Proton: 1785139158 proton-10.0-4b.

The first attempt used normal Proton game dispatch and returned no benchmark
stdout despite wrapper exit0. The controller rejected it and preserved the
failed receipt, with no forced cleanup or survivors. The accepted fresh retry
initializes its own prefix using getcompatpath, verifies the exact path, then
uses runinprefix for console handle inheritance. Both Linux arms bracket that
Proton benchmark; prefix initialization occurs before Linux A.

Controller: run-deck-2.py,
SHA256 3C695C03DFC8BB87BF5CEC9B06087033633F92D4605142344A09CFC54FF702FB.
All four controlled steps exited0. The sequence receipt reports no failure,
forced cleanup, cleanup errors or survivors. Every arm has bounded output,
180-second deadline, owned process identities and private-prefix cleanup.
Proton stderr retains the container's missing dependency warning for 32-bit
libMangoHud and the fsync startup notice; it is not an empty-stderr PASS.

Private evidence root: protection-api-comparison-20260908.
Return archive SHA256: C8A9D94FAC48ADB98950AEA60CEDF7E8443BB907EF73B8F0D250D829001006D7.
Root checked all 13 payload hashes and the three complete 21-row benchmark
streams against the embedded receipts. analysis-2.json SHA256:
7EA30033B05CC085F65203DACE794FF60263D945A42CE43E9981F2EA1C48EE12.
Independent returned-data review passed: 60 trials, all integer clock conversions,
2,400,000 timed calls, 240,000 warmup calls and 16,404,480 checked bytes agree.
Provenance limit: returned receipts do not directly embed binary/runner hashes;
root verified the staged controller hash before execution, and that controller
verified exact ELF/PE/runtime pins before each arm. This staging record is part of
acceptance, rather than a claim that the receipts alone prove all loaded identities.
U16 remains the title reference.
