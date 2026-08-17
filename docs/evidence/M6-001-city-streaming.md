# M6-001 all-regions city-streaming gate

Status: accepted. Physical result
`private/evidence/M6-001/20260817-115619-d269e2a9/result.json`, SHA-256
`519B84FF456BDD3220BFC8BE3DD230CCB209A56CF2B203D51CFF5454729E178F`.

## Contract

The canonical gate uses the completed M5 save in one clean optimized Release
process. Its project-owned route manifest defines eight broad geographic
coverage zones plus a return to the starting area:

1. Hollywood/Sunset start;
2. Beverly Hills/Westwood;
3. Santa Monica/Venice;
4. Hollywood Hills/Valley;
5. Downtown/industrial east;
6. USC/Exposition Park;
7. Crenshaw;
8. southern South Central near the 105 boundary;
9. Hollywood/Sunset return.

At every stopped checkpoint the operator opens the full GPS map and confirms
the prompted region. The host then captures a private 1280x720 guest frame,
binds it to the current successful guest-present sequence, and records numeric
process counters. The nine present sequences must increase and all nine frame
hashes must differ. Raw maps, frames, logs, save data, process identifiers, and
host paths remain private.

The resource samples contain only checkpoint identity, private bytes, working
set, handle/thread counts, and cumulative Win32 process-read bytes. Relative to
the first regional checkpoint, peak private and working-set growth may not
exceed 768 MiB, handles may not grow by more than 256, and host threads may not
grow by more than 32. Cumulative process reads must be monotonic and grow by at
least 1 MiB from the title baseline. These are bounded route-regression limits,
not an indefinite leak-freedom claim.

Acceptance also requires the exact M5-002 archive-streaming prerequisite, the
completed save/header shape and identity, a clean Release build, the four
runtime artifacts, no fatal/assertion/guest-crash/device-loss marker, and
controlled external `WM_CLOSE`. The console-style title has no in-game Exit
command.

## Commands

```powershell
scripts/run-city-streaming-smoke.ps1 -CounterSelfTest
scripts/test-city-streaming-smoke.ps1
scripts/run-city-streaming-smoke.ps1
scripts/verify-city-streaming-smoke.ps1 `
  -ResultPath <private-result.json>
```

## Accepted result

One continuous process reached all nine ordered checkpoints and returned to
Hollywood/Sunset. All nine private GPS captures are distinct. From the first
regional checkpoint to the sampled peak, growth was:

- private bytes: 264,323,072;
- working-set bytes: 183,648,256;
- handles: 7;
- host threads: 2.

Cumulative process reads grew by 4,920,118,313 bytes between the baseline and
the return checkpoint. The final save and header remained byte-identical to
the completed seed. The process emitted the exact nine-frame PASS summary and
exited through external `WM_CLOSE`; the final verifier rehashes the build,
runtime artifacts, prerequisite, save/header, log, samples, captures, and
complete evidence tree.

The successful physical run initially stopped at final verification because
Windows PowerShell 5.1 preserves a top-level `ConvertFrom-Json` array as one
pipeline object in the original expression. The persisted file already held
the correct baseline plus nine samples. The verifier now separates JSON parse
from array assignment, was rerun under Windows PowerShell 5.1, and finalized
the unchanged physical evidence without relaunching the game. The shared
toolchain resolver also gained three bounded retries after a transient empty
`vswhere` result; it still requires the exact Visual C++ x64 component.

The fixture suite passes two positives, fourteen fail-closed negatives, and
twenty-three source-contract checks. The process-counter self-test, ten
consecutive real toolchain resolutions, PowerShell parsers, and whitespace
checks also pass.

## Scope

Acceptance proves broad geographic streaming coverage across eight
project-defined zones plus return-to-start on the current Windows/D3D12 host.
It does not claim every road, event, race, opponent, collectible, campaign
item, first-run route, alternate GPU/backend, or indefinite soak duration.
