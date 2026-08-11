[CmdletBinding()]
param([string]$SdkRoot = 'third_party/rexglue-sdk')

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$repoRoot = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '..')).Path
$candidate = if ([System.IO.Path]::IsPathRooted($SdkRoot)) { $SdkRoot } else { Join-Path $repoRoot $SdkRoot }
$sdkRootPath = (Resolve-Path -LiteralPath $candidate).Path
$repoPrefix = $repoRoot.TrimEnd('\') + '\'
if (-not $sdkRootPath.StartsWith($repoPrefix, [System.StringComparison]::OrdinalIgnoreCase)) {
    throw "SDK root must stay inside the repository: '$sdkRootPath'."
}

function Read-ContractFile {
    param([Parameter(Mandatory)][string]$RelativePath)
    $path = Join-Path $sdkRootPath $RelativePath
    if (-not (Test-Path -LiteralPath $path -PathType Leaf)) {
        throw "Crash-report contract file was not found: '$path'."
    }
    return Get-Content -LiteralPath $path -Raw
}

function Assert-Pattern {
    param(
        [Parameter(Mandatory)][string]$Text,
        [Parameter(Mandatory)][string]$Pattern,
        [Parameter(Mandatory)][string]$Description
    )
    if (-not [regex]::IsMatch($Text, $Pattern, [System.Text.RegularExpressions.RegexOptions]::Singleline)) {
        throw "Crash-report contract is missing $Description."
    }
}

$context = Read-ContractFile 'include/rex/ppc/context.h'
$hook = Read-ContractFile 'include/rex/hook.h'
$header = Read-ContractFile 'include/rex/system/crash_report.h'
$implementation = Read-ContractFile 'src/system/crash_report.cpp'
$xthread = Read-ContractFile 'src/system/xthread.cpp'
$functionGraph = Read-ContractFile 'src/codegen/function_graph.cpp'
$codegenTemplate = Read-ContractFile 'resources/templates/codegen/init_h.inja'
$systemCmake = Read-ContractFile 'src/system/CMakeLists.txt'
$unitCmake = Read-ContractFile 'tests/unit/CMakeLists.txt'

Assert-Pattern $context 'uint32_t guest_pc = 0;.*uint32_t current_function = 0;.*const char\* last_import = nullptr;' 'PPC crash breadcrumbs'
Assert-Pattern $context 'class GuestFunctionScope.*std::uncaught_exceptions\(\).*context_\.guest_pc = previous_pc_.*context_\.current_function = previous_function_' 'exception-aware guest function scope'
Assert-Pattern $hook '#define REX_HOOK_RAW\(name\).*RecordGuestImport\(ctx, #name\).*name##_raw_impl' 'raw-hook import breadcrumb'
if ([regex]::Matches($hook, 'RecordGuestImport\(ctx, #subroutine\)').Count -lt 5) {
    throw 'Crash-report contract does not instrument typed hooks and all stub forms.'
}
Assert-Pattern $functionGraph 'REX_FUNC_PROLOGUE\(0x\{:08X\}\).*SetGuestProgramCounter\(ctx, 0x\{:08X\}\)' 'generated function/basic-block breadcrumbs'
Assert-Pattern $codegenTemplate '#define REX_GUEST_FUNCTION_SCOPE\(\.\.\.\).*GuestFunctionScope.*#define REX_FUNC_PROLOGUE\(\.\.\.\).*REX_GUEST_FUNCTION_SCOPE\(__VA_ARGS__\).*REX_UNIMPLEMENTED.*SetGuestProgramCounter\(ctx, addr\).*throw std::runtime_error' 'compatible generated prologue and exact unimplemented-PC breadcrumb'
Assert-Pattern $xthread 'try\s*\{\s*Execute\(\);\s*\}\s*catch \(const std::exception& error\).*CaptureGuestCrashReport\(error\.what\(\), thread_state_\.get\(\), 1\).*catch \(\.\.\.\).*CaptureGuestCrashReport\("unknown guest exception", thread_state_\.get\(\), 1\)' 'XThread C++ exception boundary'
Assert-Pattern $header 'kMaxHostFrames = 16' '16-frame host-stack bound'
Assert-Pattern $header 'bool guest_memory_included = false;' 'default guest-memory exclusion'
foreach ($field in @('REX_GUEST_CRASH schema=', 'guest_pc=', 'ppc_function=sub_', 'thread_id=',
        'last_import=', 'host_stack_count=', 'guest_memory_included=')) {
    if (-not $implementation.Contains($field)) {
        throw "Crash-report formatter is missing field '$field'."
    }
}
if (-not $systemCmake.Contains('crash_report.cpp') -or
    -not $unitCmake.Contains('system/crash_report_test.cpp')) {
    throw 'Crash-report implementation or focused unit test is not registered with CMake.'
}

[pscustomobject]@{
    Passed = $true
    RequiredFields = 7
    MaxHostFrames = 16
    GuestMemoryIncludedByDefault = $false
    ExceptionBoundaries = 2
    ImportBreadcrumbForms = [regex]::Matches($hook, 'RecordGuestImport\(ctx, #(subroutine|name)\)').Count
}
