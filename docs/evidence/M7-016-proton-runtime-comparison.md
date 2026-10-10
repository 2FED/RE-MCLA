# M7-016: Proton and GE runtime comparison

Date:2026-09-06. Reviewed private Desktop comparison; stock gameplay/audio
acceptance remains open.

## Why these runtimes

The [current caller capture](M7-016-current-caller-attribution.md) identifies
the old Proton's excessive commitment scan in NtProtectVirtualMemory. Wine
[d9172b0](https://github.com/wine-mirror/wine/commit/d9172b0acf052d0f3c087a9f5f5ece29cd917c55)
limits the scan to the actual request; [MR8151](https://gitlab.winehq.org/wine/wine/-/merge_requests/8151)
was merged2025-05-27. This preserves commitment checking and protection changes.

Independent official-source/submodule checks:

| Official release source | Wine revision | Scan limit | Required container |
|---|---|---|---|
| Proton9.0-4 |117daf8d508860a8552f59ebdd6ee16361431136|Unlimited|SLR3/Sniper|
| Proton10.0-4 |b8fdff8e1f855b5276ec4ddca0f31b2792554322|Request size|SLR3/Sniper|
| Proton11.0-2 |dc26e61847081a1b5cb0733dc30feba6ee575482|Request size|SLR4|
| GE-Proton11-6 |9358696fe9a2261329f4a83aa6a65fd436106154|Request size|SLR4|

Immutable code:
[Proton9](https://github.com/ValveSoftware/wine/blob/117daf8d508860a8552f59ebdd6ee16361431136/dlls/ntdll/unix/virtual.c#L5518),
[Proton10](https://github.com/ValveSoftware/wine/blob/b8fdff8e1f855b5276ec4ddca0f31b2792554322/dlls/ntdll/unix/virtual.c#L5671),
[Proton11](https://github.com/ValveSoftware/wine/blob/dc26e61847081a1b5cb0733dc30feba6ee575482/dlls/ntdll/unix/virtual.c#L5897),
[GE base](https://github.com/ValveSoftware/wine/blob/9358696fe9a2261329f4a83aa6a65fd436106154/dlls/ntdll/unix/virtual.c#L5897).
Primary also retrieved the actual installed official10 and GE11 Unix ntdll
binaries and verified their bounded argument by disassembly. No runtime patch.

GE11-6 additionally changes Vulkan/OpenGL overlay handling, Wayland/X11 and
child rendering, controller behavior, DLL selection and several allocation/
syscall paths. Its DXVK/VKD3D updates do not directly address this title's
native Vulkan protection scan. These are whole-runtime comparison variables,
not MCLA performance promises. [GE release notes](https://github.com/GloriousEggroll/proton-ge-custom/releases/tag/GE-Proton11-6).
Official10.0-4b's listed incremental change is Steamworks1.65 support.
[Official release](https://github.com/ValveSoftware/Proton/releases/tag/proton-10.0-4b).

## Installed identities and launch boundary

No installation required. Deck has9.0-4f,10.0-4b,11.0-2c and GE11-6.
The chosen controls are9→10→GE11→9: official10 changes fewer components
than a direct jump to GE11, while GE tests the owner's proposed option.
Use official11 as a further same-container reference only if evidence calls
for it, not an automatic benchmark expansion.

GE's manifest requires tool app4183110/SLR4;9/10 require1628350/Sniper.
Both containers are installed: Sniper3.0.20260805.254768 and
SLR4.0.20260805.254769. Pin VERSIONS.txt as well as the generic entry script.
[Valve runtime mapping](https://github.com/ValveSoftware/steam-runtime),
[GE x64 manifest](https://github.com/GloriousEggroll/proton-ge-custom/blob/GE-Proton11-6/toolmanifest_x86_64.vdf).

GE officially supports non-Steam usage through umu. This bounded SDK-style
diagnostic invokes the correct installed Steam container directly; it is not
a Steam/GamingMode integration PASS. No concrete execution obstacle is inferred
solely from the support boundary. [GE README](https://github.com/GloriousEggroll/proton-ge-custom/blob/GE-Proton11-6/README.md#ge-proton).

Each arm gets a new private prefix and normal Proton `run` initialization.
`runinprefix` skips prefix preparation and must not be used to pretend an
old/empty prefix was upgraded. [GE script](https://github.com/GloriousEggroll/proton-ge-custom/blob/GE-Proton11-6/proton#L2865).
Original owner prefix, normal bundle/current.txt, shortcut, complete profile
and cache remain hash-bound read-only sources. Same exact Type-0 candidate,
batching OFF, fast readback, Desktop Vulkan1280x720, no profiler, no global
DLL, clock, affinity, graphics-driver or synchronization override.

Current private manifest pins scripts/versions, native executable/preloaders,
Unix ntdll and Windows ntdll/kernelbase/winevulkan, plus required containers.
Pause snapshots verify actual mapped identities for all four app-local CRTs,
host/SDK and selected runtime modules. Changes are a complete runtime result;
do not attribute the entire cadence delta to one source fix.

## Startup correction before comparison

The first A9 attempt under runtime-comparison-20260906 failed after3.34s,
before any title was identified. The detached-process guard saw pv-adverb
through its container namespace path and rejected it. Forced cleanup and
failed discovery are recorded; final source/prefix hashes match, and a later
exact private-prefix process inspection returns no children. No gameplay or
cadence evidence exists for this attempt. The unrelated normal Steam helper
was neither owned nor signalled.

Preserve that first helper16065A3F7071B03D684564C6D204FA2ED5961FB23EC7ADE71B75D2D2DAA5A088,
manifest1578497C3D946C5D0F736FF98612A55322269E6DCB05008D8B3816EF9A8694A1
and failed remote root. The v2 correction binds exact installed pv-adverb bytes
through the process executable handle, with unchanged prefix/UID/birth/pidfd
checks. New root runtime-comparison-20260906-v2; no old result overwritten.

V2 reviewed helperDF660B6A7439C31F4D2AD34FA3718E4F905108B993654A8164278678E721BAC3;
manifest7917105140CA3F1488BB83C8A76BA1770323453C6EEE5E15D0296C3FDD8698CB.
Its A9 and B10 controls both completed with exit0/no force/no survivors, exact
loaded modules, and unchanged source/prefix identities; fingerprint4643DC54...
A9 measured257 outputs/20.000004s. Visual validation is pending for these
retained preliminary controls. GE then stopped at6.21s before identifying a
title because its detached Unix-directory Wine loader was not in the process
executable pins. Final source/prefix identity unchanged and independent
exact-prefix inspection found no children. This is a harness startup failure,
not GE gameplay incompatibility. The exact GE script explicitly launches
x86_64-unix/wine-preloader plus wine with WINELOADERNOEXEC=1.

V3 adds those exact installed loader/preloader identities and records the
process identity/path/hash if another unknown executable is encountered.
All four arms restart in a fresh v3 root to retain one uniform comparison
fingerprint; both prior roots remain preserved. Planned helper
D1C94D3CF7FA90202BA0B2C7AF8FB3B66235B7A786A96A6369F95F7569030ED9;
manifest8222680D3B6ABB3422B1EFB32CFBB5122FE17F178B9E370A8F784B5EA0F746C3.
The v3 A9/B10/GE arms completed normally. Its final A9 repeat recorded CAPTURED
and began ordinary title shutdown before the observer's pause-after module
read; ntdll was already unmapping. That repeat is archived as failed observer
evidence and excluded from the comparison. Final source/prefix unchanged;
no private-prefix survivors. Do not call this a title/runtime failure.

Only the final repeat was rerun under reviewed helper
7EEAD33B225EE4DE9DC7EED84C35D8A8D38F0742239B5542394163DBE0284D8C in v4.
It checks loaded modules at gameplay-before-pause and pause-before, avoiding
the exit race without an additional mid-window snapshot. Every baseline field
except observer-helper identity must equal v3; the old helper and three prior
accepted receipts are pinned. V3 fingerprint68B68C5A0921119A6B8909BF83D3C8719E6CFAF8B7ACAA00F21DB2490563FDA0;
v4 fingerprintFDF91C882136343D58184CBADFF40A455C583C85124DF02F79FE9462FBA8E1A6.
The repeat has no end-of-interval loaded-module snapshot. This qualification
must accompany reuse of these controls; artifacts/settings/profile are equal,
but the observer implementations are not identical.

## Accepted comparison

Primary inspected all four saved-gameplay frames and eight PAUSE frames.
Each shows the saved Eclipse/HUD and expected menu. Lighting/body brightness
differs, so scene equivalence is bounded rather than pixel-identical. Draw
density spans5174–5290 packets/output; readback volume36.94–37.17MB/output.

| Runtime / arm | Outputs | Interval seconds | Outputs/s | Arm ms/output | Disarm ms/output | Resolve-copy ms/output |
|---|---:|---:|---:|---:|---:|---:|
| Proton9 A |252|20.000005|12.60|10.674|5.119|4.768|
| Proton10 B |282|20.000005|14.10|6.009|2.846|4.680|
| GE11 C |274|20.000004|13.70|6.271|2.961|4.746|
| Proton9 repeat |262|20.000005|13.10|9.981|4.607|4.717|

All accepted arms exit0 without force, cleanup errors or survivors. Exact
loaded candidate/CRT/runtime identities and final source/prefix preservation
pass. Protection failures and metadata-lock misses are zero. All log the same
RADV VANGOGH, Vulkan1.4.330 and Mesa26.0.0-devel/git-9cc9241790 identity;
Proton9/10 use FSync and GE uses NTSync. No synchronization override was set.

Both newer runtimes exceed both Proton9 controls in this series. Proton10 is
7.6–11.9% above the two controls; GE4.6–8.7%. The protection-cost decrease is
consistent with the already-verified bounded-scan fix, while resolve-copy cost
is nearly unchanged. Timers overlap; do not sum them as exclusive frame costs
or attribute the whole runtime delta to one Wine commit. One paused-scene
series does not establish repeatable gameplay FPS, usable audio, controller
behavior or GamingMode integration. No separate GE advantage is established.

The complete return contains117 bounded regular files/115,043,897bytes,
including the failed v3 repeat. ArchiveSHA
D9783D3C1AD069069DD5FE48812F2DECF8338790F999EE6257CD5ACBF8BD876C;
inventory2E533181A3161B4C336830189B16B699AEBD7018383E79C3016E485F54FF1D35.
Private analyzer2BB872B72973D5BD571C0D2100F9DFA569B6DC796380BADC6DAD25EE271D9D6B
produces analysis2270D8A5F90B196878A81136C3CE82D7062F1C07AC7F016F75DDE7E7F0615028.
Independent review rehashes archive/inventory/all117 files and reproduces
the exact analysis. Private paths: runtime-comparison-return-20260906 and
runtime-comparison-analysis.json under private/evidence/M7-016.

## Decision and next action

Use installed official Proton10 as the next diagnostic baseline: it contains
the relevant fix, retains Sniper/FSync, and no GE-specific benefit was shown.
Keep the normal shortcut, bundle/current.txt and owner prefix unchanged.
The bounded [fixed-runtime residual profile](M7-016-proton10-residual-profile.md)
and subsequent [Type0 control](M7-016-proton10-type0-control.md) are now accepted;
do not optimize the obsolete Proton9 scan again or repeat identical pause tests.
Stock playability, audio and save-successor acceptance remain open. No task
closure, version bump, SDK promotion, normal publication or push.
