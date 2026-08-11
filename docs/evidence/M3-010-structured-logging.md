# M3-010 structured subsystem logging

Date: 2026-08-11

Status: accepted

## Scope

M3-010 introduces a stable project-owned category vocabulary without changing
ReXGlue's upstream category names or claiming that later GPU/audio/input paths
are already functional. The required schema-1 categories are:

`app`, `ppc`, `kernel`, `xam`, `vfs`, `gpu`, `audio`, `input`, and `patches`.

Existing project messages now route lifecycle and crash events through `app`,
image/dispatch events through `ppc`, and disc-policy events through `vfs`.
The remaining categories are registered for the subsystems that own later work.

## Filter contract

Every category has an init-only `mcla_log_<category>` setting. Accepted values
are `inherit`, `trace`, `debug`, `info`, `warn`, `error`, `critical`, and `off`.
The default is `inherit`, so the existing global `log_level` behavior is
preserved unless an operator explicitly narrows or expands one category.

The opt-in `mcla_logging_probe` defaults off. When enabled, it emits exactly one
marker through every registered logger:

```text
MCLA_LOG_SCHEMA schema=1 category=<category> event=probe
```

## Automated verification

Source and fixture gates:

- source contract: 9/9 category registrations and overrides;
- source fixtures: one positive and five rejected mutations;
- log fixtures: one positive and four rejected logs;
- complete project PowerShell suite: 24/24 scripts passed;
- ast-grep scan: clean; rule tests: 3/3 passed;
- fresh prerequisite/bootstrap audit: 12/12 checks passed;
- RelWithDebInfo configure/build: 65 generated sources plus the new project
  logging translation unit compiled and linked successfully; executable
  SHA-256 `836717A50EE06C6B2743E1595DC6C46FFAFE2313091FB160E669146DCB523240`;
- post-build lifecycle and crash-report compatibility probes: 2/2 exit code 0.

Rejected mutations cover a missing category, an enabled-by-default probe, a
non-inheriting override default, missing XAM override application, and a generic
project logger regression. Rejected logs cover a wrong category, duplicate
schema markers, an absolute private path, and error-level output.

## Live filter/write matrix

Final private run: `20260811-142820-fe84078e`.

The wrapper ran the lifecycle-only probe nine times. Every run set global
logging to `off`, enabled exactly one required category at `info`, redirected
user/cache/log output below the ignored run root, and required exit code 0.

| Selected category | Exit | Schema markers | Log bytes | SHA-256 |
|---|---:|---:|---:|---|
| app | 0 | 1 | 436 | `871E924576580F7D2E9F00630FFDB89A9D71F2F4416482F2E66425245A6E596D` |
| ppc | 0 | 1 | 99 | `B2904D4818E1F0291F8B51A582CA251D312F776F845F6CB888912F63277D8597` |
| kernel | 0 | 1 | 105 | `8B87B89084AE0CFB420EC2811E0906E39467555F281B278F72E806022E0008EF` |
| xam | 0 | 1 | 99 | `6A0B2F18B094FBBDE85C354DA5AABF451EC830F52E86AE718E38484D155C3271` |
| vfs | 0 | 1 | 99 | `67A68FD7AF42854B41F2D4FEBAF9EE1BD63977F45E1C62804E4665C5F9BD52BD` |
| gpu | 0 | 1 | 99 | `601A8E2A553DC3861E655BEC4D393E1D7B8208018CB07FEB693E72153AB8069C` |
| audio | 0 | 1 | 103 | `F27C4766549BF7692DF97EB05C4058F7A4BBAFD333ECE5A625206953CA7CDD11` |
| input | 0 | 1 | 103 | `1DB46895594FCF77E4B424459BDA905924ABAF8BBDE20FCE61720D4328D45607` |
| patches | 0 | 1 | 106 | `F50E839649C3EE22425859FC0125805875BE320EF4C0F408DE54A153FDC43E7A` |

Every verifier saw only the selected schema category. No other schema marker,
error/critical event, absolute drive path, user/private path fragment, or game
filename was accepted. Raw logs and result JSON remain untracked.

## Re-evaluation

M3-010's acceptance criterion is satisfied: all required categories are
independently filterable and safely written. This task does not add an SDK
defect and therefore required no upstream issue. M3-011 remains conditional on
observing Bink as a startup blocker; M3-012 through M3-015 still own the build
matrix, startup traps/smoke, and repeated-exit stability.
