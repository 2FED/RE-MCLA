[CmdletBinding()]
param()

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$repo = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '..')).Path
$verifier = Join-Path $PSScriptRoot 'verify-fullscreen-toggle-smoke.ps1'
$source = & $verifier -SourceOnly
if (-not $source.Passed) { throw 'Canonical fullscreen source contract failed.' }

$temporary = Join-Path ([IO.Path]::GetTempPath()) ('mcla-fullscreen-test-' + [guid]::NewGuid().ToString('N'))
$sdk = Join-Path $temporary 'sdk'
$header = Join-Path $sdk 'include\rex\ui\window_sdl.h'
$implementation = Join-Path $sdk 'src\ui\window_sdl.cpp'
[IO.Directory]::CreateDirectory((Split-Path -Parent $header)) | Out-Null
[IO.Directory]::CreateDirectory((Split-Path -Parent $implementation)) | Out-Null
$canonicalHeader = [IO.File]::ReadAllText((Join-Path $repo 'third_party\rexglue-sdk\include\rex\ui\window_sdl.h'))
$canonicalSource = [IO.File]::ReadAllText((Join-Path $repo 'third_party\rexglue-sdk\src\ui\window_sdl.cpp'))
$mutations = @(
    { param($text) $text.Replace('SDL_SCANCODE_KP_ENTER', 'SDL_SCANCODE_UNKNOWN') },
    { param($text) $text.Replace('SDL_KMOD_ALT', 'SDL_KMOD_CTRL') },
    { param($text) $text.Replace('!event.key.repeat', 'event.key.repeat') },
    { param($text) $text.Replace('source=alt-enter', 'source=keyboard') },
    { param($text) $text.Replace('event.button.button == SDL_BUTTON_LEFT', 'event.button.button == SDL_BUTTON_RIGHT') },
    { param($text) $text.Replace('event.button.clicks >= 2', 'event.button.clicks >= 3') },
    { param($text) $text.Replace('(event.button.clicks % 2) == 0', '(event.button.clicks % 2) != 0') },
    { param($text) $text.Replace('source=left-double-click', 'source=mouse') }
)
$rejected = 0
try {
    foreach ($mutation in $mutations) {
        [IO.File]::WriteAllText($header, $canonicalHeader)
        [IO.File]::WriteAllText($implementation, (& $mutation $canonicalSource))
        try { & $verifier -SdkRoot $sdk -SourceOnly | Out-Null }
        catch { $rejected++; continue }
        throw 'Fullscreen verifier accepted a mutated source contract.'
    }
} finally {
    if (Test-Path -LiteralPath $temporary) { Remove-Item -LiteralPath $temporary -Recurse -Force }
}
if ($rejected -ne $mutations.Count) { throw 'Fullscreen negative fixture count drifted.' }
Write-Host "Fullscreen toggle source PASS: 1 positive / $rejected fail-closed mutations." -ForegroundColor Green
[pscustomobject]@{ Passed=$true; Positive=1; Negative=$rejected }
