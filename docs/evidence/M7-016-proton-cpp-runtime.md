# M7-016: Proton timed C++ wait and app-local runtime

2026-09-05. New KI-030: portable C++ runtime dependency mismatch. This is a
correctness/deployment finding, not the established cause of KI-027's FPS.

## Observed failure

The owner reported that the autonomous probe stayed on PRESS START until
it closed. Its exact log shows synthetic START submitted, a 20-second
observation timeout, then the guest acknowledging the input at that timeout.
The same host probe succeeds on Windows. The failed Deck run is retained
with FAILED status and is not gameplay evidence.

A standalone /MD C++23 fixture holds a mutex, starts a contender which waits
20 ms and tries to lock it, then calls predicate `condition_variable::wait_for`
with a 500-ms timeout. It records contender entry, acquisition, observation
and completion after joining the thread. It contains no game, SDK, graphics
or controller code. Compile with the same toolchain/runtime selection as the
Release consumers; a static-CRT fixture would be an invalid discriminator.

| Exact same fixture PE | Observed result |
|---|---|
| Native Windows system C++ runtime | 3/3 acquisitions and notifications around 20--21 ms |
| Proton builtin MSVCP140 | Timeout at 500 ms; contender attempts at 21 ms but acquires only at 500 ms |
| Official app-local MSVC runtime | 3/3 acquisitions/notifications at 21 ms |

The result is reproduced both with a private native-DLL preference and with
**no DLL override**. Loaded module paths confirm the difference. No installer,
registry, system DLL, Steam shortcut or Proton-version change was made.
The reusable fixture is `tests/mcla_cpp_runtime_probe.cpp`, exposed by
`MCLA_BUILD_DIAGNOSTIC_TESTS` as `mcla_cpp_runtime_probe`; its CMake target
explicitly selects `MultiThreadedDLL` on Windows. The primary verified its
exact imports include `SleepConditionVariableSRW`, `_Mtx_lock` and `_Mtx_unlock`.
The first reusable-probe PE is
`90B00F9E1D400E140DC9BCE9BE919746FADBCFDA847EA8B2F3912D4E250E879B`.
After the reviewed module-path truncation guard, final PE
`03A052741F20E1FC8D7679C87F52253C0B4C4CECCBA1B285E61A5D4EFDEDEB18`
again passes Windows3/3, fails builtin Proton, and passes app-local Proton3/3
at21ms with the loaded module paths recorded. No static-CRT substitute.

Local MSVC source shows the timed-wait helper manipulates mutex ownership
fields then passes an internal SRW lock to `SleepConditionVariableSRW`, while
lock/unlock call into the loaded C++ DLL. The observed mismatch is consistent
with incompatible lock implementations/layout across that boundary. This
does not claim a complete disassembly of every Wine implementation or a
universal failure across all Proton/MSVC versions.

Four isolated actual-host runs with app-local runtime reached saved gameplay
and pause, with renderer captures inspected by the primary and normal exit 0.
The fourth uses only the four bin DLLs below, without any DLL override; loaded
paths confirm all four app-local modules and the unchanged Wine UCRT. It
measures 248 outputs/20 seconds (12.40/s), 9,186,967,552 readback bytes and
1,145,808 nested copy microseconds. Original profile/cache/published files stay
unchanged. Its completed 24-file return is locally hash-verified under
`private/evidence/M7-016/readback-minimal-crt-return-20260905`.
No global timeout increase or fabricated input ACK is part of the fix.

## Bounded dependency correction

The source dependency is the installed, signed Microsoft VC143 x64 Redist
directory, whose DLL file version is **14.44.35211.0** (directory name
14.44.35112). The runtime files remain private; only expected identities and
verification code belong in Git. Independent review inspected static/delay
imports and bounded DLL-name strings in all six current published PEs and
the required Microsoft DLLs:

| Location | Required closure |
|---|---|
| Beside Launch-MCLA.exe | msvcp140.dll, vcruntime140.dll, vcruntime140_1.dll |
| bin/ | Those three plus msvcp140_atomic_wait.dll |

The launcher needs VCRUNTIME140_1 transitively through Microsoft's MSVCP140.
Root DLLs do not cover an executable in `bin/`; all seven placements must be
materialized copies and covered by the immutable hash inventory. Additional
dynamic dependencies remain subject to actual-load verification for each
new build. No UCRT, API-set forwarder, debug CRT or Windows system DLL is
copied, and no caller is switched to static CRT as a workaround.

Microsoft documents app-local deployment and the need to redeploy those
libraries when updating them. This immutable-bundle workflow owns servicing;
it is not an installer-managed runtime. See [local deployment](https://learn.microsoft.com/en-us/cpp/windows/deployment-in-visual-cpp?view=msvc-170)
and [dependency/version guidance](https://learn.microsoft.com/en-us/cpp/windows/determining-which-dlls-to-redistribute?view=msvc-170).
Public redistribution remains subject to the applicable terms and
[Visual Studio redistribution list](https://learn.microsoft.com/en-us/visualstudio/releases/2022/redistribution);
this private control makes no general licensing eligibility claim.

New bundle schema v2 requires a fixed app-local C++ policy/version and
all seven pinned DLL placements. Legacy v1 stays readable only as a returned
profile source; do not make missing CRT optional in new publications.
The launcher verifies required paths/hashes without changing inherited DLL
preferences. Root DLLs load **before** launcher verification, so that check
is corruption detection, not a pre-execution security boundary.

## Scope and remaining gates

The defect explains the synthetic input observation timeout. It may also
affect other timed STL waits, such as diagnostic idle completion. In contrast,
the known GPU-ring and audio-worker waits use Win32 wait APIs, not this STL
helper. Do not call this the primary FPS/audio root cause. Successful
app-local readback-cost arms still produce only about 12--14.5 outputs/s.

Primary and independent review accepted the exact seven-path topology and
legacy return handling. Both PowerShell 7 and Windows PowerShell 5.1 pass:
bundle 3 positive/34 negative/45 source contracts; CRT 3 positive/8 negative;
lineage 48 checks; performance analyzer 15 positive/56 negative; session
1 positive/10 negative; and all eight native launcher recovery checks.
The legacy fixture was corrected to construct a v2 publication candidate,
not to weaken v1 rejection at the publication boundary. Runtime fixtures
validate signed source identity, PE architecture and staged materialization.

Reviewed source 0.17.0.0 clean-builds a 32-immutable-file isolated candidate:
`mcla-0.17.0.0-win64-AD4D0F5815126536-2FC8B65D466D35F4-58B3385C89D9E499`.
Windows relocation, native launcher integrity, guest-free live diagnostics,
normal session completion and full owner-profile selection pass; receipt
`private/evidence/M7-016/bundle-20260905-203701-1cc3a1a9/result.json`.
This candidate is outside the Syncthing publication root. Actual published
bundle/current.txt remain 0.16.0.1.

## Exact candidate Deck proof, completed overnight September 5--6

The binaries-only candidate was copied to a private directory outside the
entire Syncthing root. Existing prepared game files were hash-checked and
linked; the existing Deck profile was copied locally into the isolated test.
No profile was uploaded from Windows, Steam shortcut changed, original game
replaced or experimental save promoted. No DLL override was used.

- Exact `Launch-MCLA.exe --verify-only`: exit0, all32 immutable files.
- Exact `--diagnostics-probe`: exit0, atomic session
  `session-20260905T204922Z-220`, one full-profile snapshot, zero watcher errors.
- New host AD4D0F58... autonomously reaches actual saved Eclipse/world/HUD and
  pause; primary inspected both renderer captures. Normal exit0 after154s,
  no forced cleanup, original profile/cache/published hashes unchanged.
- Packaged TOML wins over command-line logging/fullscreen: this control is
  fullscreen, not the earlier matched windowed A-B-A. Its harness looked in
  the wrong log path and retained a failed receipt; separate hash-bound
  post-run validation found the CAPTURED marker in `logs/mcla.log` and intact
  samples. This is a harness-path correction, not fabricated gameplay.
  Cadence remains12.15outputs/s; no comparative performance gain is claimed.
- An isolated copy of the new bin closure, without TOML/dialog, deliberately
  crashes after runtime setup. The out-of-process helper completes its private
  minidump. Existing diagnostic verifier accepts actual live48,687-byte and
  crash93,479-byte dumps, both flags0x1020, privacy metadata and all artifacts.
- The session verifier accepts the returned session against the exact local
  manifest and unchanged returned profile; all32 hashes were verified on Deck.

Complete private return62files/38,995,747bytes is locally hash-verified under
`private/evidence/M7-016/cpp-candidate-return-20260905`; compressed hash
`BDC9F53F592514E0081D9A07F946448D620AAC596E0FABA2B21BD4405E4BC728`.
Original failed harness receipt remains unmodified. These controls prove the
candidate dependency/diagnostic boundary, not Gaming Mode/controller/audio
parity, evolved profile reload or usable stock performance. Publication is
still separate; no further owner-driven unchanged run is requested.
