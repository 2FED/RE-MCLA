[CmdletBinding()]
param(
    [string]$SdkRoot,
    [string]$ImportCoveragePath
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$repoRoot = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '..')).Path
if (-not $SdkRoot) {
    $SdkRoot = Join-Path $repoRoot 'third_party/rexglue-sdk'
}
if (-not $ImportCoveragePath) {
    $ImportCoveragePath = Join-Path $repoRoot 'docs/evidence/M2-013-import-coverage.md'
}

function Read-SdkFile {
    param([Parameter(Mandatory)][string]$RelativePath)
    $path = Join-Path $SdkRoot $RelativePath
    if (-not (Test-Path -LiteralPath $path -PathType Leaf)) {
        throw "Required early-init source was not found: '$path'."
    }
    return Get-Content -LiteralPath $path -Raw
}

if (-not (Test-Path -LiteralPath $ImportCoveragePath -PathType Leaf)) {
    throw "Required import coverage was not found: '$ImportCoveragePath'."
}

$coverage = Get-Content -LiteralPath $ImportCoveragePath -Raw
$imports = @('ExCreateThread', 'KeQueryPerformanceFrequency', 'KeQuerySystemTime')
foreach ($name in $imports) {
    if ($coverage -notmatch [regex]::Escape("| ``$name`` |")) {
        throw "Early-init import '$name' is absent from the accepted M2 coverage."
    }
}

$runtime = Read-SdkFile 'src/system/runtime.cpp'
$runtimeTokens = @(
    'chrono::Clock::set_guest_tick_frequency(50000000);',
    'chrono::Clock::set_guest_system_time_base(chrono::Clock::QueryHostSystemTime());',
    'chrono::Clock::set_guest_time_scalar(1.0);',
    'memory_ = std::make_unique<memory::Memory>();'
)
$previous = -1
foreach ($token in $runtimeTokens) {
    $index = $runtime.IndexOf($token, [System.StringComparison]::Ordinal)
    if ($index -lt 0 -or $index -le $previous) {
        throw "Runtime clock initialization is missing or out of order at '$token'."
    }
    $previous = $index
}

$builder = Read-SdkFile 'src/codegen/builders/system.cpp'
foreach ($token in @(
    'ctx.println("\t{}.u64 = REX_QUERY_TIMEBASE();", ctx.r(ctx.insn.operands[0]));',
    'ctx.println("\t{}.u64 = REX_QUERY_TIMEBASE() >> 32;", ctx.r(ctx.insn.operands[0]));'
)) {
    if (-not $builder.Contains($token)) {
        throw "Generated timebase lowering is missing '$token'."
    }
}

$initTemplate = Read-SdkFile 'resources/templates/codegen/init_h.inja'
if (-not $initTemplate.Contains(
        '#define REX_QUERY_TIMEBASE() rex::chrono::Clock::QueryGuestTickCount()')) {
    throw 'Generated code no longer routes mftb through the scaled guest clock.'
}

$threading = Read-SdkFile 'src/kernel/xboxkrnl/xboxkrnl_threading.cpp'
foreach ($token in @(
    'uint64_t result = chrono::Clock::guest_tick_frequency();',
    'uint64_t time = chrono::Clock::QueryGuestSystemTime();'
)) {
    if (-not $threading.Contains($token)) {
        throw "Xboxkrnl timing export is missing reviewed clock route '$token'."
    }
}

$threadHeader = Read-SdkFile 'include/rex/system/xthread.h'
if ($threadHeader -notmatch 'kGuestThreadStartupDelay\s*\{\s*10\s*\}') {
    throw 'Guest-thread compatibility startup delay is not exactly 10 ms.'
}

$threadSource = Read-SdkFile 'src/system/xthread.cpp'
$executeStart = $threadSource.IndexOf('void XThread::Execute() {', [System.StringComparison]::Ordinal)
$executeEnd = $threadSource.IndexOf('void XThread::EnterCriticalRegion()', $executeStart,
    [System.StringComparison]::Ordinal)
if ($executeStart -lt 0 -or $executeEnd -le $executeStart) {
    throw 'Could not bound XThread::Execute for startup-order review.'
}
$execute = $threadSource.Substring($executeStart, $executeEnd - $executeStart)
$orderedThreadTokens = @(
    'kernel_state_->OnThreadExecute(this);',
    'rex::thread::Sleep(kGuestThreadStartupDelay);',
    'DeliverAPCs();',
    'const StartPlan start_plan = BuildStartPlan(creation_params_);',
    'dispatcher->GetFunction(start_plan.address);',
    'func(*ctx, base);'
)
$previous = -1
foreach ($token in $orderedThreadTokens) {
    $index = $execute.IndexOf($token, [System.StringComparison]::Ordinal)
    if ($index -lt 0 -or $index -le $previous) {
        throw "Guest-thread startup is missing or out of order at '$token'."
    }
    $previous = $index
}

$startPlanStart = $threadSource.IndexOf(
    'XThread::StartPlan XThread::BuildStartPlan(const CreationParams& params) {',
    [System.StringComparison]::Ordinal)
$startPlanEnd = $threadSource.IndexOf('void XThread::Execute()', $startPlanStart,
    [System.StringComparison]::Ordinal)
if ($startPlanStart -lt 0 -or $startPlanEnd -le $startPlanStart) {
    throw 'Could not bound XThread::BuildStartPlan for dispatch review.'
}
$startPlan = $threadSource.Substring($startPlanStart, $startPlanEnd - $startPlanStart)
$xapiPlanPattern = '(?s)if\s*\(params\.xapi_thread_startup\)\s*\{\s*return\s*\{\s*' +
    'params\.xapi_thread_startup,\s*\{params\.start_address,\s*params\.start_context\},\s*' +
    '2,\s*false,\s*\};\s*\}'
$rawPlanPattern = '(?s)return\s*\{\s*params\.start_address,\s*' +
    '\{params\.start_context,\s*0\},\s*1,\s*true,\s*\};'
if ($startPlan -notmatch $xapiPlanPattern) {
    throw 'XAPI guest-thread start plan no longer has the reviewed trampoline/argument/exit contract.'
}
if ($startPlan -notmatch $rawPlanPattern) {
    throw 'Raw guest-thread start plan no longer has the reviewed address/context/exit contract.'
}

$unitCmake = Read-SdkFile 'tests/unit/CMakeLists.txt'
foreach ($testPath in @('core/chrono_test.cpp', 'system/thread_start_test.cpp')) {
    if ($unitCmake -notmatch ('(?m)^\s*' + [regex]::Escape($testPath) + '\s*$')) {
        throw "Early-init regression source '$testPath' is not registered in unit tests."
    }
}
$chronoTest = Read-SdkFile 'tests/unit/core/chrono_test.cpp'
$threadTest = Read-SdkFile 'tests/unit/system/thread_start_test.cpp'
if (-not $chronoTest.Contains('Guest timebase is monotonic at the Xbox 360 50 MHz frequency')) {
    throw 'The 50 MHz monotonic guest-timebase regression case is missing.'
}
foreach ($case in @(
    'Raw guest threads start at the requested routine with context in r3',
    'XAPI guest threads start at the trampoline with routine and context arguments',
    'Guest thread startup retains the compatibility grace period'
)) {
    if (-not $threadTest.Contains($case)) {
        throw "Guest-thread regression case is missing: '$case'."
    }
}

[pscustomobject]@{
    Passed = $true
    ImportsReviewed = $imports.Count
    TimebaseChecks = 7
    ThreadStartChecks = 10
    RegressionCases = 4
}
