# Default off semaphore callback transport

The in-process transport delivers semaphore metadata to one immutable callback
table. It does not assign credits, read clocks or game memory, or establish
complete operation history. The host collector owns those separate concerns.

Registration is explicit and once-only. Repeated or racing registration cannot
replace the table or re-enable disabled admissions. Callback exceptions are
contained, recorded with a saturating failure counter and disable new admissions.
LastError is restored on every transport mutation path. No observer mutex or
allocation surrounds the original semaphore operation.

Disable stops new creation, destruction and Begin observations; an admitted
callback may still finish. A nonzero End continues through the same table after
Disable or failure. Therefore Disable is not a drain barrier, and callbacks,
context and the registry must remain alive for the process lifetime. The host
must independently validate each returned ticket and freeze under its own lock.

## Standalone verification

Clang 20 C++23 Release with /W4 /WX passes 5 default-off and 49 enabled checks.
Tests cover exact metadata, defined previous-count values only, unsupported
operations, malformed inputs, callback exceptions, registration races and an
in-flight Begin spanning Disable. They also verify LastError preservation.
The initial OFF compilation rejected unused test helpers; marking those helpers
maybe_unused resolved the warning without changing transport behavior.

Independent source review accepts this contract. The transport contains no
global singleton: its integration must supply exactly one process-lifetime
registry and compile the host and SDK with consistent diagnostic definitions.
All native, simulation, render, frame-mapping and FPS proof flags remain false.

| Source | SHA256 |
| --- | --- |
| src/mcla_semaphore_witness_api_win.h | 0E8E50F31740DA93CEA58501ABF241CDDDBCA897537002A9141D869784E9E810 |
| tests/mcla_semaphore_witness_api_win_test.cpp | 1AD3EBFD3B6E2A92BA8C227EC65242FA11D35D336CDED1FB9C74122305295AF5 |

## Separate private SDK smoke

Fresh private/sdk-sem-s contains an exact copy of this API and default-off SDK
hooks. Single-wait and multiple-wait observations finish before the post-wait
title-termination check. Save, Restore and combined waits remain explicitly
unsupported rather than ordinary modeled credits. No default SDK was installed.

A reviewed no-game tool-mode fixture passes 45 checks in
private/sem-sdk-fixture-20261013-1, normal exit0 with no forced cleanup. The actual
failed-release LastError survives a deliberately clobbering callback. DLL symbol
inspection finds one ProcessRegistry export and the fixture imports it. SDK
source-isolated tests pass 401 cases, 3364 assertions and 4 known skips; PPC tests
pass 1462 cases and 5753 assertions. Metadata135, packet100 and provenance30 pass.

The fixture EXE is 4BF898A49D2E38AC6D55EFCE8DDEAE71FC605DF97B947176B5429B9EA1C11ED4;
rexruntime.dll is B5581ADC73B2C5A32041C4AEE027E74FCC357FE8B62C8AE0647E1E401E935FC1.
This private integration is not promoted by the standalone transport commit.
The smoke does not exercise actual title termination, APC, Restore or concurrent
gameplay. Host selection, complete history and frame/update joins remain pending.
M8-001 stays open; accepted l12, original game assets and MAIN are unchanged.
