# M7-016 private published-scope candidate

2026-09-06. M7 remains 0/16 and KI-027 remains open. This experiment follows
the [overhead attribution](M7-016-guest-scope-overhead.md),
[ABI controls](M7-016-guest-scope-abi.md) and
[nonvolatile vector controls](M7-016-guest-scope-vector.md).

## Change and isolation

A private forced-include header replaces generated function-scope bookkeeping
with a normal-completion wrapper. It saves the caller's PC/function, invokes a
private body overload, and restores the fields after body-local destruction.
Exceptional escape bypasses restoration. Published entry, block-PC and normal
restoration use compiler signal fences; no private CRT layout or CPU fence is
used. The legacy SDK class remains untouched.

The original two-argument entries, weak aliases, typed function pointers and
shared setjmp registry remain. Only the63 recompilation TUs receive the header
and enable definition; init/register, host, tests and SDK sources do not.
The actual corpus has29,788 bodies and107,107 qualified literal PC setters.
Both unchanged mapping tables contain30,034 entries. Every body's name/address
is checked against both tables, including237 functions with meaningful names.

All67 generated files and SDK inputs remain unchanged. TU0 retains its existing
strict hardware-FMA options; all other source flags and link libraries match
the retained hardware44E8 baseline. Guest Tracy profiling is rejected by the
private header. Large bodies may stay out of line, requiring fresh symbol
attribution and performance measurement.

## Exact-header ABI acceptance

The previous twelve-case ABI fixture now uses the actual override and generated
header/setjmp helpers. Its three case translation units and runtime preamble
remain byte-identical. A separate adapter removes the old copied macro/helper
definitions, and every compile selects strategy1.

The build freezes1,200 inputs /211,674,362bytes and completes eight commands.
Independent static review confirms aliases, distinct strong hook, one shared
TLS registry, returns-twice setjmp ordering, local cleanup before restoration,
and exceptional cleanup bypassing it. No Rex runtime import is required.

Native Windows and exact Proton10 both finish normally with five verified
images, no forced cleanup or survivors. Each emits187 rows:12 cases,88 checks
and65 observation points. All180 normalized semantic rows match each other and
the previously accepted wrapper, including platform-longjmp destructor traces.
The eleven-file Proton return is retained outside the entire sync root.

- Header: `3157F9BCBE2D52651986587B57B297E411C45FEE26031B06DB1474F09242300A`
- Fixture PE: `4CA3E19D009F73C2C7F2141160609B30839F4C4EF61D7FD82E52C72A766A0FEA`
- Native result: `C43A53E8BD558A136A1D220E07F2634A1383DF89160A0C4BA9F0F78D8E351E32`
- Proton result: `D03E3FC10736AEA3914C4A9DEA4F33D4039AF83C0EDF42566CB9C24464A997C7`
- Verification: `74D7D02CA12CA98C5C32DF47A15E37B39533762A9AA67511555D46A26A2B512B`

Final independent return review passes, including all83,449 returned bytes.
The fixture does not establish
general asynchronous-fault safety or performance. Its observer queries affect
code generation; the separate vector fixture addresses representative faults.

## Game build gate

Private root: `private/evidence/M7-016/guest-scope-candidate-20260906`.
Build root: `out/build/win-amd64-guest-scope-published-20260906`.
ControllerCF883357... and hookA113E379... freeze2,654 files /784,423,760bytes.
Input manifest110DFF04... binds the unchanged baseline plus new helpers and
accepted vector/header evidence. Inherited generation hashes and success
predicates are checked, not merely copied into a new manifest.

Configure exits zero. All78 CMake source commands and72 actual host Ninja
commands compare against the hardware baseline; only the explicit include and
enable options differ on exactly63 sources. Link fields match after build-path
normalization. All six generated configuration files are pinned before build.
The first two pre-freeze census assumptions were corrected against actual
named-function and registry data; neither attempt configured or built anything.

The full77-step game build exits zero in131.297seconds with no forced cleanup.
PEE54C42E53F1D1ACC063170D3491BD69A50979C48E5539F67B9A49D17505D3154
is45,299,712bytes; mapCCD1B326... is specific to this build. All nine unchanged
non-recompilation objects match the hardware baseline after zeroing only COFF
timestamp bytes4–7. The experiment retains the old accompanying eight
binaries, including the existing crash helper; fresh build-helper hashes are
not substituted into the comparison package.

Binary review passes: all665 imports remain identical; all29,788 scoped alias
pairs and their object owners remain intact, with171 retained private bodies.
The full decoded scan finds8 direct calls to the exception-count thunk, compared
with83,559 in the baseline. This does not count dynamic calls or every possible
indirect call. The selected96-FMA pipeline and all narrow/widen dependencies
remain intact. TU27's reviewed polling loop retains23 instructions, eight PAUSE
instructions and one PC store in both builds; no wait bypass was introduced.

The Windows Vulkan control passes with exit0 in173.5979224seconds, no forced
cleanup, survivors or recorded errors. Seven exact images,100-module snapshot,
six BMPs and unchanged original/test four-file profiles independently verify.
Primary inspected saved gameplay, unpaused endpoints and both pause frames.
Unpaused595/20.000165s and paused600/20.000002s are this native control's counters;
the native after-window idle camera varies, and no comparative/moving/audio/Deck
performance result follows. Native resultE2DD083D...; package09BAE976... retains
eight old companions and changes only the host.

## Deck comparison ready; owner authentication required

Reviewed controller988045B1... runs clean baseline/published/baseline controls,
with no profiler. Preparer1F20F0F9... accepts only the reviewed executable and
small helpers/manifests, copying the eight companions locally on Deck. Stage
manifest5F72968D... binds eight files /45,421,309bytes. The fresh package/run
directories must be inspected after reconnecting because incoming-file state is
currently unverified.

Initial transfer59193 stopped at Tailscale's additional SSH authentication check.
Its exit-code guard prevented preparation. After owner reauthentication, read-only
inspection found only the old ABI directory; reviewed transfer and preparation
B5AE6CAA... then completed. All three clean controls have now finished normally.

| Phase | Baseline44E8 | PublishedE54 | Baseline repeat44E8 |
|---|---:|---:|---:|
| Unpaused outputs/s |18.45|18.80|18.55|
| Unpaused draws/output |3279.12|3357.95|3286.24|
| Paused outputs/s |14.80|14.90|14.45|
| Paused draws/output |5181.45|5330.80|5404.51|

Published unpaused rate is1.90%/1.35% above the two baselines, with2.40%/2.18%
more draw work per output. Baseline unpaused drift is0.54%; paused drift is−2.36%.
This is a modest bounded signal, not stable30 or established repeatable speedup.
Primary inspected all fifteen gameplay/unpaused/pause frames: unpaused endpoints
retain rear camera/HUD0mph, while traffic, pedestrians, music and lighting vary.
Identical full-window workload is not established.

All85 files /89,016,177bytes are hash-verified and safely extracted. Both counter
phases, twelve mapped images in each of two snapshots, launch normalization,
complete unchanged terminal seed and clean lifecycle pass the adapted analyzer
and primary reproduction. Final independent adaptation review passes, including
all six raw counter pairs and work/timer calculations.

- Archive: `E2C34C800382D1D8FC24AF4FA5800B3262DCCCE27B6B722FB65839FE6456FFE9`
- Inventory: `F45CE67B45B784557BDA6DF22139F12F197EE43640AEA59F9594DA1673C85625`
- Analyzer: `3DD11E9F6B83331D00EA0A2F237894268CF7A0928E6917C6B4146073A15C9D8F`
- Analysis: `7DCD3F1037D36084E567BA18ED5C73859828154D6A0098D928E14D0B410B74E1`
- Verification: `4790F5AB366145FC354D1CB8A6570EBC647E55A3D3E286F4BBE60FF04D8740D1`

The owner explicitly approved changing the existing Tailscale SSH rule from
check to accept for users accessing their own devices. After all game controls
closed, the rule and its comment were saved; reload confirmed persistence, and
SSH collection succeeded. Tags, ownership, network grants and expiry were not
changed. The owner then went to sleep and set a new20% weekly-remaining stop
threshold, replacing the earlier3%; latest live allowance is67% remaining.

Next is a separate profile of this exact candidate to identify remaining work.
No identical clean series is planned, and no manual owner drive is requested yet.

The normal0.16.0.1 bundle, shortcut and full accepted save are unchanged. No SDK
promotion, version bump, task closure, commit or push has occurred.
