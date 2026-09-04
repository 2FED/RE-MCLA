[CmdletBinding()]
param(
    [string]$SdkRoot,
    [string]$ResultPath,
    [switch]$SourceOnly
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$repo = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '..')).Path
if (-not $SdkRoot) { $SdkRoot = Join-Path $repo 'third_party\rexglue-sdk' }
$sdk = [IO.Path]::GetFullPath($SdkRoot)

function Assert-SourceContract {
    $headerPath = Join-Path $sdk 'include\rex\ui\window_sdl.h'
    $sourcePath = Join-Path $sdk 'src\ui\window_sdl.cpp'
    foreach ($path in @($headerPath,$sourcePath)) {
        if (-not (Test-Path -LiteralPath $path -PathType Leaf)) {
            throw "Fullscreen source contract file is missing: '$path'."
        }
    }
    $header = [IO.File]::ReadAllText($headerPath)
    $source = [IO.File]::ReadAllText($sourcePath)
    foreach ($needle in @('suppress_alt_enter_key_up_', 'suppress_double_click_mouse_up_')) {
        if (-not $header.Contains($needle)) { throw "Fullscreen header contract is missing: $needle" }
    }
    foreach ($needle in @(
        'SDL_SCANCODE_RETURN',
        'SDL_SCANCODE_KP_ENTER',
        'SDL_KMOD_ALT',
        '!event.key.repeat',
        'SetFullscreen(fullscreen)',
        'source=alt-enter',
        'event.button.button == SDL_BUTTON_LEFT',
        'event.button.clicks >= 2',
        '(event.button.clicks % 2) == 0',
        'source=left-double-click'
    )) {
        if (-not $source.Contains($needle)) { throw "Fullscreen source contract is missing: $needle" }
    }
    if ([regex]::Matches($source, 'REX_WINDOW_FULLSCREEN_TOGGLE v=1').Count -ne 2) {
        throw 'Fullscreen source must emit one bounded marker for each shortcut.'
    }
    [pscustomobject]@{ Passed=$true; AltEnter=$true; LeftDoubleClick=$true; EventsConsumed=$true }
}

$sourceResult = Assert-SourceContract
if ($SourceOnly -or -not $ResultPath) { return $sourceResult }

$resultFile = (Resolve-Path -LiteralPath $ResultPath).Path
$result = Get-Content -LiteralPath $resultFile -Raw | ConvertFrom-Json
$expected = @(
    'schema','task','decision','run_id','sdk_version','sdk_commit','build_configuration',
    'executable_sha256','initial_windowed','alt_enter_fullscreen','left_double_click_windowed',
    'alt_enter_marker_count','left_double_click_marker_count','controlled_external_close',
    'exit_code','force_cleanup','source_contract'
)
foreach ($name in $expected) {
    if ($result.PSObject.Properties.Name -cnotcontains $name) { throw "Fullscreen result is missing '$name'." }
}
if ((@($result.PSObject.Properties.Name | Sort-Object) -join '|') -cne (@($expected | Sort-Object) -join '|')) {
    throw 'Fullscreen result property topology drifted.'
}
if ($result.schema -cne 'mcla-runtime-fullscreen-toggle-v1' -or $result.task -cne 'M7-016' -or
    $result.decision -cne 'runtime-fullscreen-two-shortcuts-pass' -or
    [string]$result.run_id -cnotmatch '^\d{8}-\d{6}-[0-9a-f]{8}$' -or
    $result.build_configuration -cne 'Release') { throw 'Fullscreen result identity is invalid.' }
if ($result.initial_windowed -ne $true -or $result.alt_enter_fullscreen -ne $true -or
    $result.left_double_click_windowed -ne $true -or $result.alt_enter_marker_count -ne 1 -or
    $result.left_double_click_marker_count -ne 1 -or $result.controlled_external_close -ne $true -or
    $result.exit_code -ne 0 -or $result.force_cleanup -ne $false -or $result.source_contract -ne $true -or
    [string]$result.executable_sha256 -cnotmatch '^[0-9A-F]{64}$' -or
    [string]$result.sdk_commit -cne '492614eec92c31f11d75dd8fa0f09785cbae4a66' -or
    [string]$result.sdk_version -cne '0.10.0.2') {
    throw 'Fullscreen physical toggle contract failed.'
}
[pscustomobject]@{ Passed=$true; Decision=$result.decision; RunId=$result.run_id; ResultPath=$resultFile }
