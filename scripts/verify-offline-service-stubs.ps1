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
foreach ($path in @($SdkRoot, $ImportCoveragePath)) {
    if (-not (Test-Path -LiteralPath $path)) {
        throw "Required offline-service input was not found: '$path'."
    }
}

$entries = @(
    [pscustomobject]@{ Name = 'XamShowFriendsUI'; File = 'src/kernel/xam/xam_ui.cpp'; Signature = 'u32 XamShowFriendsUIOffline_entry() {'; Function = 'XamShowFriendsUIOffline_entry'; Return = 'return X_ERROR_NOT_LOGGED_ON;'; Expectation = 'CHECK(CallOfflineHook(__imp__XamShowFriendsUI) == uint32_t(X_ERROR_NOT_LOGGED_ON));' },
    [pscustomobject]@{ Name = 'XamShowGamerCardUIForXUID'; File = 'src/kernel/xam/xam_ui.cpp'; Signature = 'u32 XamShowGamerCardUIForXUIDOffline_entry() {'; Function = 'XamShowGamerCardUIForXUIDOffline_entry'; Return = 'return X_ERROR_NOT_LOGGED_ON;'; Expectation = 'CHECK(CallOfflineHook(__imp__XamShowGamerCardUIForXUID) == uint32_t(X_ERROR_NOT_LOGGED_ON));' },
    [pscustomobject]@{ Name = 'XamShowPlayerReviewUI'; File = 'src/kernel/xam/xam_ui.cpp'; Signature = 'u32 XamShowPlayerReviewUIOffline_entry() {'; Function = 'XamShowPlayerReviewUIOffline_entry'; Return = 'return X_ERROR_NOT_LOGGED_ON;'; Expectation = 'CHECK(CallOfflineHook(__imp__XamShowPlayerReviewUI) == uint32_t(X_ERROR_NOT_LOGGED_ON));' },
    [pscustomobject]@{ Name = 'NetDll_XNetServerToInAddr'; File = 'src/kernel/xam/xam_net.cpp'; Signature = 'u32 NetDll_XNetServerToInAddrOffline_entry() {'; Function = 'NetDll_XNetServerToInAddrOffline_entry'; Return = 'return kWsaNetworkUnreachable;'; Expectation = 'CHECK(CallOfflineHook(__imp__NetDll_XNetServerToInAddr) == kWsaNetworkUnreachable);' },
    [pscustomobject]@{ Name = 'NetDll_XNetUnregisterInAddr'; File = 'src/kernel/xam/xam_net.cpp'; Signature = 'u32 NetDll_XNetUnregisterInAddrOffline_entry() {'; Function = 'NetDll_XNetUnregisterInAddrOffline_entry'; Return = 'return 0;'; Expectation = 'CHECK(CallOfflineHook(__imp__NetDll_XNetUnregisterInAddr) == 0);' },
    [pscustomobject]@{ Name = 'NetDll_XNetConnect'; File = 'src/kernel/xam/xam_net.cpp'; Signature = 'u32 NetDll_XNetConnectOffline_entry() {'; Function = 'NetDll_XNetConnectOffline_entry'; Return = 'return kWsaNetworkUnreachable;'; Expectation = 'CHECK(CallOfflineHook(__imp__NetDll_XNetConnect) == kWsaNetworkUnreachable);' },
    [pscustomobject]@{ Name = 'NetDll_XNetGetConnectStatus'; File = 'src/kernel/xam/xam_net.cpp'; Signature = 'u32 NetDll_XNetGetConnectStatusOffline_entry() {'; Function = 'NetDll_XNetGetConnectStatusOffline_entry'; Return = 'return kXNetConnectStatusLost;'; Expectation = 'CHECK(CallOfflineHook(__imp__NetDll_XNetGetConnectStatus) == kXNetConnectStatusLost);' },
    [pscustomobject]@{ Name = 'NetDll_XNetQosLookup'; File = 'src/kernel/xam/xam_net.cpp'; Signature = 'u32 NetDll_XNetQosLookupOffline_entry() {'; Function = 'NetDll_XNetQosLookupOffline_entry'; Return = 'return X_ERROR_FUNCTION_FAILED;'; Expectation = 'CHECK(CallOfflineHook(__imp__NetDll_XNetQosLookup) == uint32_t(X_ERROR_FUNCTION_FAILED));' },
    [pscustomobject]@{ Name = 'XNetLogonGetTitleID'; File = 'src/kernel/xam/xam_misc.cpp'; Signature = 'u32 XNetLogonGetTitleIDOffline_entry() {'; Function = 'XNetLogonGetTitleIDOffline_entry'; Return = 'return 0;'; Expectation = 'CHECK(CallOfflineHook(__imp__XNetLogonGetTitleID) == 0);' },
    [pscustomobject]@{ Name = 'XamVoiceHeadsetPresent'; File = 'src/kernel/xam/xam_voice.cpp'; Signature = 'u32 XamVoiceHeadsetPresent_entry(mapped_void voice_ptr) {'; Function = 'XamVoiceHeadsetPresent_entry'; Return = 'return 0;'; Expectation = 'CHECK(CallOfflineHook(__imp__XamVoiceHeadsetPresent) == 0);' }
)

$coverage = Get-Content -LiteralPath $ImportCoveragePath -Raw
$testPath = Join-Path $SdkRoot 'tests/unit/kernel/offline_service_test.cpp'
$testCmakePath = Join-Path $SdkRoot 'tests/unit/CMakeLists.txt'
foreach ($path in @($testPath, $testCmakePath)) {
    if (-not (Test-Path -LiteralPath $path -PathType Leaf)) {
        throw "Required offline-service regression file was not found: '$path'."
    }
}
$testSource = Get-Content -LiteralPath $testPath -Raw
$testCmake = Get-Content -LiteralPath $testCmakePath -Raw
if ($testCmake -notmatch '(?m)^\s*kernel/offline_service_test\.cpp\s*$') {
    throw 'The offline-service regression source is not registered in the unit-test target.'
}
if ($testSource -notmatch 'ctx\.r3\.u64\s*=\s*0xDEADBEEF') {
    throw 'The regression test does not seed r3 with a stale caller value.'
}

$reviewed = foreach ($entry in $entries) {
    $sourcePath = Join-Path $SdkRoot $entry.File
    if (-not (Test-Path -LiteralPath $sourcePath -PathType Leaf)) {
        throw "Offline-service source was not found: '$sourcePath'."
    }
    $lines = @(Get-Content -LiteralPath $sourcePath)
    $signatureIndex = [Array]::IndexOf($lines, $entry.Signature)
    if ($signatureIndex -lt 0) {
        throw "Missing explicit implementation signature for '$($entry.Name)'."
    }
    $endIndex = -1
    for ($i = $signatureIndex + 1; $i -lt $lines.Count; $i++) {
        if ($lines[$i] -eq '}') {
            $endIndex = $i
            break
        }
    }
    if ($endIndex -lt 0) {
        throw "Could not bound the implementation for '$($entry.Name)'."
    }
    $body = ($lines[$signatureIndex..$endIndex] -join "`n")
    if ($body -notmatch [regex]::Escape("[OFFLINE] $($entry.Name)")) {
        throw "'$($entry.Name)' does not emit its explicit [OFFLINE] diagnostic."
    }
    if ($body -notmatch [regex]::Escape($entry.Return)) {
        throw "'$($entry.Name)' does not expose the reviewed return contract '$($entry.Return)'."
    }
    $source = $lines -join "`n"
    $registrationPattern = 'REX_EXPORT\(\s*__imp__' + [regex]::Escape($entry.Name) +
        '\s*,\s*rex::kernel::xam::' + [regex]::Escape($entry.Function) + '\s*\)'
    if ($source -notmatch $registrationPattern) {
        throw "'$($entry.Name)' is not bound to its reviewed explicit implementation."
    }
    if ($source -match [regex]::Escape("REX_EXPORT_STUB(__imp__$($entry.Name));")) {
        throw "'$($entry.Name)' regressed to the generic stale-r3 stub."
    }
    if ($coverage -notmatch [regex]::Escape("| ``$($entry.Name)`` |")) {
        throw "'$($entry.Name)' is not present in the accepted M2 import coverage."
    }
    if ($testSource -notmatch [regex]::Escape($entry.Expectation)) {
        throw "'$($entry.Name)' lacks its exact guest-visible regression assertion."
    }
    [pscustomobject]@{
        Import = $entry.Name
        Source = $entry.File
        ReturnContract = $entry.Return
    }
}

[pscustomobject]@{
    Passed = $true
    ImportsReviewed = $reviewed.Count
    GenericStubs = 0
    Assertions = $entries.Count
    Matrix = $reviewed
}
