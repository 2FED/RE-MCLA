# M3-013 post-GPU startup traps

Date: 2026-08-11

Status: accepted

## Scope

M3-013 selects the Xenos plugin on the normal native route and removes every
compile, link, invalid-function, or `PPC_UNIMPLEMENTED` failure reachable in the
bounded startup window. It does not claim frontend usability; M3-014 and M3-015
own the canonical smoke and repeated lifecycle gates.

`MclaApp::OnPreSetup` selects `xenos` only when no plugin was configured and the
process is not running a lifecycle, module-config, VFS, crash, or logging probe.
This keeps focused guest-free probes independent while making a plain normal
launch exercise the staged GPU backend.

## Runtime-guided control-flow repair

The first Xenos-enabled trace loaded `rexgpu-xenosrd.dll`, initialized graphics,
and then failed at `0x8249CBF0`. Five iterations followed the same fail-closed
sequence: retain the exact first invalid target, add one evidence-bounded entry,
run clean non-force codegen, inspect the complete generated body, rebuild, and
repeat the same isolated trace.

| Order | Address | Exclusive end | Classification |
|---:|---|---|---|
| 1 | `0x8249CBF0` | `0x8249CC00` | vtable tail, slot `+80` |
| 2 | `0x8249CC00` | `0x8249CC10` | vtable tail, slot `+68` |
| 3 | `0x823F3C68` | `0x823F3C80` | bounded result leaf with `blr` |
| 4 | `0x823F6EF8` | `0x823F6F1C` | status guard/tail wrapper |
| 5 | `0x822C9DD8` | `0x822C9DE8` | tail wrapper to `0x822C7568` |

No entry is a semantic stub or game patch. The mapping count increases from
30,020 to 30,025. The final clean output remains 67 files and 65 C++ sources,
totals 133,908,410 bytes, and has private manifest SHA-256
`85BAAEE37F082804CBF1B969E95F7185463BAB5A7DE1408E59562D303681BF8D`.

## Clean configuration matrix

Private matrix run: `20260811-152419-96864c05`.

Debug, RelWithDebInfo, and Release each completed a clean 71-action build with
65 generated objects and only their matching runtime, Tracy, and Xenos DLLs.
Executable SHA-256 values were:

- Debug: `2BC09C6AA65ECCB780302F3C3B4FFAD8A82A68B9A5C611D040C4901E693713E3`;
- RelWithDebInfo: `2BF8F52D1E9B0B1E6E1C514C3B78270084639067890CA503B8D895DC8F3210A7`;
- Release: `129B68C14E2E07957AC1FD80F14A6639A65B66DEA43282AABB9D2143BD49144C`.

## Final bounded trace

Private run: `20260811-152722-e7412213`.

The post-clean RelWithDebInfo executable was launched without an explicit
`gpu_plugin` argument. It remained alive for the complete 20-second observation,
then the runner terminated it and confirmed no process survived. Ordered
evidence includes:

1. project-default `xenos` selection;
2. `rexgpu-xenosrd.dll` load;
3. guest module launch;
4. graphics interrupt callback;
5. graphics pipeline creation;
6. audio worker callback.

The log contains zero no-GPU, fatal, invalid/unregistered-function,
`PPC_UNIMPLEMENTED`, structured guest-crash, or post-launch Bink markers. Runtime
log SHA-256:
`8696C16C84064B1EF52C017F127E21B6FBE0BCCA322C53CE3A8542583E4B0628`.

## Gates

- analysis configuration: 25 exact reviewed functions, zero switch/invalid/
  exception/jump overrides;
- trace fixtures: one positive and seven negative cases;
- generated integration: 67 files, 65 C++ sources, 30,025 mappings, zero
  tracked generated files;
- clean native matrix: 3/3 configurations;
- bounded post-clean runtime: full 20 seconds, graphics/audio progress, cleanup
  confirmed, zero reachable trap markers;
- post-clean lifecycle probe: exit 0 with four ordered markers and zero GPU
  selection/load markers;
- complete project PowerShell suite: 27/27 scripts passed;
- ast-grep scan: clean; rule tests: 3/3 passed;
- prerequisite/bootstrap audit: 12/12 checks passed;
- KI-011: closed; no skip-intro condition appeared after module launch.

## Re-evaluation

The current deterministic boundary is continuous GPU/audio execution rather
than a guest trap. M3-014 should reuse the ordered markers and add canonical
title/image identity expectations. M3-015 must then repeat controlled cycles and
resolve or close the cold post-relink exit anomaly tracked by KI-010.
