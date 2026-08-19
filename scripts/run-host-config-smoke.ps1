[CmdletBinding()]
param()

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$repoRoot = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '..')).Path
$toolchain = & (Join-Path $PSScriptRoot 'Resolve-Toolchain.ps1') -ExportPath
$cmake = $toolchain.CMakePath
$runId = (Get-Date -Format 'yyyyMMdd-HHmmss') + '-' + [guid]::NewGuid().ToString('N').Substring(0,8)
$runRoot = Join-Path $repoRoot "private/evidence/M6-011/$runId"
[IO.Directory]::CreateDirectory($runRoot) | Out-Null
$utf8 = [Text.UTF8Encoding]::new($false)
$sdkRoot = Join-Path $repoRoot 'third_party/rexglue-sdk'

Write-Host 'M6-011 [1/4]: clean-building and installing ReXGlue SDK...'
$sdkLog = Join-Path $runRoot 'sdk-install.log'
Push-Location $sdkRoot
try {
    & $cmake --preset win-amd64 *>&1 | Tee-Object -FilePath $sdkLog
    if ($LASTEXITCODE -ne 0) { throw "SDK configure failed. Private run: '$runRoot'." }
    & $cmake --build --preset win-amd64-relwithdebinfo --target install --clean-first --parallel 8 *>&1 | Tee-Object -FilePath $sdkLog -Append
    $sdkExit = $LASTEXITCODE
} finally { Pop-Location }
if ($sdkExit -ne 0) { throw "SDK build/install failed. Private run: '$runRoot'." }

Write-Host 'M6-011 [2/4]: running focused audio/config tests...'
$testLog = Join-Path $runRoot 'focused-tests.log'
Push-Location $sdkRoot
try {
    & $cmake --build --preset win-amd64-relwithdebinfo --target unit_tests --parallel 8 *>&1 | Tee-Object -FilePath $testLog
    if ($LASTEXITCODE -ne 0) { throw "Focused test build failed. Private run: '$runRoot'." }
    & .\out\win-amd64\RelWithDebInfo\unit_tests.exe '[audio][config]' *>&1 | Tee-Object -FilePath $testLog -Append
    $testExit = $LASTEXITCODE
} finally { Pop-Location }
if ($testExit -ne 0) { throw "Focused audio/config tests failed. Private run: '$runRoot'." }
$testText = [IO.File]::ReadAllText($testLog)
if ($testText -notmatch 'All tests passed \(27 assertions in 3 test cases\)') { throw 'Focused test totals drifted.' }

Write-Host 'M6-011 [3/4]: clean-building the MCLA host and staged example...'
$appLog = Join-Path $runRoot 'app-clean-build.log'
Push-Location $repoRoot
try {
    & $cmake --preset win-amd64-relwithdebinfo *>&1 | Tee-Object -FilePath $appLog
    if ($LASTEXITCODE -ne 0) { throw "App configure failed. Private run: '$runRoot'." }
    & $cmake --build --preset win-amd64-relwithdebinfo --target mcla --clean-first --parallel 8 *>&1 | Tee-Object -FilePath $appLog -Append
    $appExit = $LASTEXITCODE
} finally { Pop-Location }
if ($appExit -ne 0) { throw "App clean build failed. Private run: '$runRoot'." }

$template = Join-Path $repoRoot 'config/mcla.toml.example'
$result = [ordered]@{
    schema=1; task='M6-011'; decision='host-config-contract-pass'
    sdk_version='0.9.0.28'; sdk_install_exit_code=$sdkExit
    focused_test_exit_code=$testExit; focused_test_cases=3; focused_test_assertions=27
    app_clean_build_exit_code=$appExit
    template_sha256=(Get-FileHash -LiteralPath $template -Algorithm SHA256).Hash
    sdk_install_log_sha256=(Get-FileHash -LiteralPath $sdkLog -Algorithm SHA256).Hash
    focused_test_log_sha256=(Get-FileHash -LiteralPath $testLog -Algorithm SHA256).Hash
    app_clean_build_log_sha256=(Get-FileHash -LiteralPath $appLog -Algorithm SHA256).Hash
}
$resultPath = Join-Path $runRoot 'result.json'
[IO.File]::WriteAllText($resultPath, (($result | ConvertTo-Json -Depth 5) + [Environment]::NewLine), $utf8)
Write-Host 'M6-011 [4/4]: revalidating the physical result...'
$verification = & (Join-Path $PSScriptRoot 'verify-host-config-smoke.ps1') -ResultPath $resultPath
[pscustomobject]@{ Passed=$verification.Passed; ConfigKeys=$verification.Keys; BadInputCoverage=$verification.BadInputCoverage; ResultPath=$resultPath }
