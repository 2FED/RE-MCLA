[CmdletBinding()]
param([switch]$ValidateOnly)
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$repo = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$build = Join-Path $repo 'out\build\win-amd64-release'
$exe = Join-Path $build 'mcla.exe'
$helper = Join-Path $build 'mcla_crash_handler.exe'
$game = Join-Path $repo 'private\game'
$utf8 = [Text.UTF8Encoding]::new($false)

function Wait-Exit([Diagnostics.Process]$Process,[int]$Seconds,[string]$Label){
    if($Process.WaitForExit($Seconds*1000)){return}
    Stop-Process -Id $Process.Id -Force -ErrorAction SilentlyContinue
    $null=$Process.WaitForExit(5000)
    throw "$Label timed out after $Seconds seconds."
}
function Wait-Latest([string]$Path,[int]$Seconds,[string]$Label){
    $deadline=[DateTime]::UtcNow.AddSeconds($Seconds)
    while([DateTime]::UtcNow-lt$deadline){
        if(Test-Path -LiteralPath $Path -PathType Leaf){
            $name=(Get-Content -LiteralPath $Path -Raw).Trim()
            if($name){return $name}
        }
        Start-Sleep -Milliseconds 100
    }
    throw "$Label did not publish a latest-package pointer within $Seconds seconds."
}

& (Join-Path $PSScriptRoot 'test-diagnostics-smoke.ps1') | Out-Null
if($ValidateOnly){[pscustomobject]@{Passed=$true;Decision='diagnostic-contract-validation-pass';ValidateOnly=$true};return}

Write-Host 'M6-014 DIAGNOSTICS [1/4]: clean-building the Release title and crash helper...' -ForegroundColor Cyan
& (Join-Path $PSScriptRoot 'Resolve-Toolchain.ps1') -ExportPath | Out-Null
& cmake --preset win-amd64-release | Out-Host;if($LASTEXITCODE-ne0){throw 'Release configure failed.'}
& cmake --build --preset win-amd64-release --clean-first --parallel 8 | Out-Host;if($LASTEXITCODE-ne0){throw 'Release build failed.'}
foreach($path in @($exe,$helper)){if(-not(Test-Path -LiteralPath $path -PathType Leaf)){throw "Required diagnostic binary is missing: '$path'."}}
if(-not(Test-Path -LiteralPath (Join-Path $game 'default.xex') -PathType Leaf)){throw 'Prepared private game root is missing.'}

$runId=(Get-Date).ToUniversalTime().ToString('yyyyMMdd-HHmmss')+'-'+[guid]::NewGuid().ToString('N').Substring(0,8)
$root=Join-Path $repo "private\evidence\M6-014\diagnostics\$runId"
New-Item -ItemType Directory -Force -Path $root | Out-Null

Write-Host 'M6-014 DIAGNOSTICS [2/4]: capturing one guest-free live snapshot...' -ForegroundColor Cyan
$liveUser=Join-Path $root 'live-user';$liveLog=Join-Path $root 'live.log'
$liveArgs=@('--mcla_diagnostics_snapshot_probe=true',('--user_data_root="{0}"'-f$liveUser),('--log_file="{0}"'-f$liveLog),'--log_level=info')
$liveProcess=Start-Process -FilePath $exe -ArgumentList $liveArgs -PassThru -WindowStyle Hidden
Wait-Exit $liveProcess 60 'Live diagnostic probe'
if($liveProcess.ExitCode-ne0){throw "Live diagnostic probe exited with $($liveProcess.ExitCode)."}
$liveName=Wait-Latest (Join-Path $liveUser 'diagnostics\latest-live.txt') 10 'Live diagnostic probe'
$livePackage=Join-Path $liveUser "diagnostics\live\$liveName"

Write-Host 'M6-014 DIAGNOSTICS [3/4]: raising one deliberate native crash and collecting out-of-process...' -ForegroundColor Cyan
$crashUser=Join-Path $root 'crash-user';$crashLog=Join-Path $root 'crash.log'
$crashArgs=@('--mcla_native_crash_post_setup_probe=true','--mcla_crash_reporter_dialog=false',('--game_data_root="{0}"'-f$game),('--user_data_root="{0}"'-f$crashUser),('--log_file="{0}"'-f$crashLog),'--log_level=info')
$crashProcess=Start-Process -FilePath $exe -ArgumentList $crashArgs -PassThru -WindowStyle Hidden
Wait-Exit $crashProcess 60 'Native crash probe'
if($crashProcess.ExitCode-eq0){throw 'Deliberate native crash unexpectedly exited successfully.'}
$crashName=Wait-Latest (Join-Path $crashUser 'diagnostics\latest-crash.txt') 30 'Native crash helper'
$crashPackage=Join-Path $crashUser "diagnostics\crash\$crashName"

Write-Host 'M6-014 DIAGNOSTICS [4/4]: verifying dump flags, manifests, privacy, and atomic packages...' -ForegroundColor Cyan
$verified=& (Join-Path $PSScriptRoot 'verify-diagnostics-smoke.ps1') -LivePackage $livePackage -CrashPackage $crashPackage
$result=[ordered]@{schema='mcla-diagnostics-smoke-result-v1';task='M6-014';decision=$verified.Decision;run_id=$runId;mcla_version=(Get-Content -LiteralPath (Join-Path $repo 'VERSION') -Raw).Trim();live_package=$livePackage;crash_package=$crashPackage;live_dump_bytes=$verified.LiveDumpBytes;crash_dump_bytes=$verified.CrashDumpBytes;live_dump_flags=$verified.LiveDumpFlags;crash_dump_flags=$verified.CrashDumpFlags;automatic_upload=$false;physical_f10_required=$true}
$resultPath=Join-Path $root 'result.json';[IO.File]::WriteAllText($resultPath,(($result|ConvertTo-Json -Depth 8)+[Environment]::NewLine),$utf8)
Write-Host "M6-014 DIAGNOSTICS PASS: '$resultPath'." -ForegroundColor Green
[pscustomobject]@{Passed=$true;Decision=$verified.Decision;RunId=$runId;ResultPath=$resultPath;LivePackage=$livePackage;CrashPackage=$crashPackage}
