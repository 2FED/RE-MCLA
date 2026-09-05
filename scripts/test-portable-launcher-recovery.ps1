[CmdletBinding()]param([string]$LauncherPath)
Set-StrictMode -Version Latest
$ErrorActionPreference='Stop'
$repo=(Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '..')).Path
$private=[IO.Path]::GetFullPath((Join-Path $repo 'private'))
$root=[IO.Path]::GetFullPath((Join-Path $private ('evidence\M7-016\launcher-recovery-test-'+[guid]::NewGuid().ToString('N').Substring(0,8))))
if(-not$root.StartsWith($private+[IO.Path]::DirectorySeparatorChar,[StringComparison]::OrdinalIgnoreCase)){throw 'Recovery fixture escaped private/.'}
$utf8=[Text.UTF8Encoding]::new($false)
. (Join-Path $PSScriptRoot 'Portable-CppRuntime.ps1')
$runtimePolicy=Get-PortableCppRuntimePolicy
$runtimeFiles=@(Get-PortableCppRuntimeFiles $runtimePolicy)
function Hash($Path){$s=[IO.File]::OpenRead($Path);try{$h=[Security.Cryptography.SHA256]::Create();try{([BitConverter]::ToString($h.ComputeHash($s))).Replace('-','')}finally{$h.Dispose()}}finally{$s.Dispose()}}
function Tree($Path){
  if(-not(Test-Path -LiteralPath $Path -PathType Container)){throw "Fixture tree disappeared: $Path"}
  $prefix=[IO.Path]::GetFullPath($Path).TrimEnd('\')+[IO.Path]::DirectorySeparatorChar
  $rows=@(foreach($item in @(Get-ChildItem -LiteralPath $Path -Recurse -Force|Sort-Object FullName)){
    $relative=$item.FullName.Substring($prefix.Length).Replace('\','/')
    if($item.PSIsContainer){"D $relative"}else{"F $relative $($item.Length) $(Hash $item.FullName)"}
  })
  return ($rows-join"`n")
}
function Lock($Root){$files=@('Launch-MCLA.exe','bundle-id.txt','bundle-manifest.json','game-manifest.json','bin/mcla.exe','bin/mcla_crash_handler.exe','bin/mcla.toml','bin/rexgpu-xenos.dll','bin/rexruntime.dll','bin/TracyClient.dll','game/default.xex')+@($runtimeFiles|ForEach-Object{$_.RelativePath});$rows=@($files|Sort-Object|ForEach-Object{"$(Hash (Join-Path $Root $_.Replace('/','\'))) *$_"});[IO.File]::WriteAllText((Join-Path $Root 'bundle-files.sha256'),(($rows-join"`n")+"`n"),$utf8)}
try{
  @('bin','game','user\B13EBABEBABEBABE\545407F8\00000001\mc4.sav','user\B13EBABEBABEBABE\545407F8\Headers\00000001','cache','logs','diagnostics','results','update','metadata')|ForEach-Object{[IO.Directory]::CreateDirectory((Join-Path $root $_))|Out-Null}
  $launcher=if($LauncherPath){[IO.Path]::GetFullPath($LauncherPath)}else{Join-Path $repo 'out\build\win-amd64-release\Launch-MCLA.exe'};$dummy=Join-Path ([Environment]::GetFolderPath([Environment+SpecialFolder]::System)) 'where.exe'
  if(-not(Test-Path -LiteralPath $launcher -PathType Leaf)){throw 'Build the Release portable launcher before its recovery test.'}
  $toolchain=& (Join-Path $PSScriptRoot 'Resolve-Toolchain.ps1')
  $runtimeSource=Resolve-PortableCppRuntimeSource -VisualStudioRoot $toolchain.VisualStudioRoot
  Copy-Item -LiteralPath $launcher -Destination (Join-Path $root 'Launch-MCLA.exe');Copy-Item -LiteralPath $dummy -Destination (Join-Path $root 'bin\mcla.exe')
  foreach($entry in $runtimeFiles){Copy-Item -LiteralPath (Join-Path $runtimeSource.SourceDirectory $entry.Name) -Destination (Join-Path $root $entry.RelativePath.Replace('/','\'))}
  @('bin/mcla_crash_handler.exe','bin/rexgpu-xenos.dll','bin/rexruntime.dll','bin/TracyClient.dll','game/default.xex')|ForEach-Object{[IO.File]::WriteAllText((Join-Path $root $_.Replace('/','\')),"fixture:$_`n",$utf8)}
  [IO.File]::WriteAllText((Join-Path $root 'bin\mcla.toml'),"fullscreen = true`n",$utf8)
  [IO.File]::WriteAllText((Join-Path $root 'user\B13EBABEBABEBABE\545407F8\00000001\mc4.sav\mc4.sav'),"recovery-save`n",$utf8)
  [IO.File]::WriteAllText((Join-Path $root 'user\B13EBABEBABEBABE\545407F8\Headers\00000001\mc4.sav.header'),"recovery-header`n",$utf8)
  $id='mcla-0.9.0.1-win64-RECOVERYTEST0001-RECOVERYTEST0002-RECOVERYTEST0003'
  [IO.File]::WriteAllText((Join-Path $root 'bundle-id.txt'),$id+"`n",$utf8);[IO.File]::WriteAllText((Join-Path $root 'bundle-manifest.json'),"{}`n",$utf8);[IO.File]::WriteAllText((Join-Path $root 'game-manifest.json'),"{}`n",$utf8);Lock $root
  # An interrupted F10 is evidence only. Even its private save fragment must
  # survive both normal-launch validations without becoming the user's save.
  $liveName='live-20260905T151532Z-336-5'
  $livePartial=Join-Path $root ("diagnostics\live\$liveName.partial")
  [IO.Directory]::CreateDirectory((Join-Path $livePartial 'save-private\empty'))|Out-Null
  [IO.File]::WriteAllText((Join-Path $livePartial 'state.json'),'{"unfinished":true}', $utf8)
  [IO.File]::WriteAllText((Join-Path $livePartial 'save-private\fragment.bin'),'not-a-complete-save', $utf8)
  $liveBefore=Tree $livePartial
  $userBefore=Tree (Join-Path $root 'user')
  $sid='session-20260904T122927Z-300';$partial=Join-Path $root ("results\$sid.partial");[IO.Directory]::CreateDirectory($partial)|Out-Null
  $session=[ordered]@{schema='mcla-portable-session-v1';bundle_id=$id;session_id=$sid;started_utc='20260904T122927Z';mode='gameplay';state='running'}
  [IO.File]::WriteAllText((Join-Path $partial 'session.json'),(($session|ConvertTo-Json)+"`n"),$utf8);[IO.File]::WriteAllText((Join-Path $root 'bundle.lock'),"stale`n",$utf8)
  $stdout=Join-Path $root 'launcher-stdout.txt';$stderr=Join-Path $root 'launcher-stderr.txt'
  $p=Start-Process -FilePath (Join-Path $root 'Launch-MCLA.exe') -WorkingDirectory $root -WindowStyle Hidden -RedirectStandardOutput $stdout -RedirectStandardError $stderr -PassThru
  $null=$p.Handle # WinPS 5.1 must retain the handle before WaitForExit.
  if(-not$p.WaitForExit(15000)){$p.Kill();$p.WaitForExit();$detail=((Get-Content -Raw $stdout -ErrorAction SilentlyContinue)+(Get-Content -Raw $stderr -ErrorAction SilentlyContinue));throw "Recovery fixture timed out: $detail"};if($p.ExitCode-eq0){throw 'Dummy title unexpectedly exited zero.'}
  if(Test-Path -LiteralPath (Join-Path $root 'bundle.lock')){throw 'Recovered launcher left bundle.lock.'};if(@(Get-ChildItem (Join-Path $root 'results') -Recurse -Force|Where-Object Name -Like '*.partial').Count){throw 'Recovered launcher left partial state.'}
  $recovered=Get-Content -Raw -LiteralPath (Join-Path $root "results\$sid\result.json")|ConvertFrom-Json
  if($recovered.schema-cne'mcla-portable-interrupted-session-result-v1'-or$recovered.bundle_id-cne$id-or$recovered.session_id-cne$sid-or$recovered.state-cne'interrupted'-or$null-ne$recovered.exit_code-or$recovered.interruption_reason-cne'launcher-terminated-before-session-finalization'-or[int]$recovered.save_snapshot_count-ne1){throw 'Interrupted-session result is invalid.'}
  $next=@(Get-ChildItem (Join-Path $root 'results') -Directory|Where-Object Name -NE $sid);if($next.Count-ne1){throw 'Follow-up session count is invalid.'};$normal=Get-Content -Raw (Join-Path $next[0].FullName 'result.json')|ConvertFrom-Json
  if($normal.schema-cne'mcla-portable-session-result-v1'-or$normal.state-cne'complete'-or[int]$normal.exit_code-eq0){throw 'Follow-up session did not finalize.'}
  if((Tree $livePartial)-cne$liveBefore-or(Tree (Join-Path $root 'user'))-cne$userBefore){throw 'Normal launch mutated unfinished diagnostic evidence or the user profile.'}
  if((Test-Path -LiteralPath (Join-Path $root "diagnostics\live\$liveName"))-or(Test-Path -LiteralPath (Join-Path $root 'diagnostics\latest-live.txt'))){throw 'Normal launch promoted an unfinished diagnostic.'}

  $verifyStdout=Join-Path $root 'verify-stdout.txt';$verifyStderr=Join-Path $root 'verify-stderr.txt'
  $verifyProcess=Start-Process -FilePath (Join-Path $root 'Launch-MCLA.exe') -ArgumentList '--verify-only' -WorkingDirectory $root -WindowStyle Hidden -RedirectStandardOutput $verifyStdout -RedirectStandardError $verifyStderr -PassThru
  $null=$verifyProcess.Handle
  if(-not$verifyProcess.WaitForExit(15000)){$verifyProcess.Kill();$verifyProcess.WaitForExit();throw 'Verify-only fixture timed out with an unfinished diagnostic.'}
  if($null-eq$verifyProcess.ExitCode-or$verifyProcess.ExitCode-ne0){throw "Verify-only fixture exit code was not zero: '$($verifyProcess.ExitCode)'."}
  $receipts=@(Get-ChildItem -LiteralPath (Join-Path $root 'results') -File -Filter 'portable-verification-*.json')
  if($receipts.Count-ne1){throw 'Verify-only did not publish exactly one integrity receipt.'}
  $receipt=Get-Content -Raw -LiteralPath $receipts[0].FullName|ConvertFrom-Json
  if($receipt.schema-cne'mcla-portable-verification-v1'-or$receipt.bundle_id-cne$id-or[int]$receipt.verified_file_count-ne18-or$receipt.decision-cne'portable-bundle-integrity-pass'){throw 'Verify-only integrity receipt is invalid.'}
  if((Tree $livePartial)-cne$liveBefore-or(Tree (Join-Path $root 'user'))-cne$userBefore){throw 'Verify-only mutated unfinished diagnostic evidence or the user profile.'}
  if((Test-Path -LiteralPath (Join-Path $root "diagnostics\live\$liveName"))-or(Test-Path -LiteralPath (Join-Path $root 'diagnostics\latest-live.txt'))){throw 'Verify-only promoted an unfinished diagnostic.'}
  if(Test-Path -LiteralPath (Join-Path $root 'bundle.lock')){throw 'Verify-only left bundle.lock.'}
  [pscustomobject]@{Passed=$true;StaleLockRecovered=$true;InterruptedSessionRecovered=$true;SaveSnapshots=1;FollowUpSessionCompleted=$true;UnfinishedDiagnosticPreserved=$true;VerifyOnlyWithUnfinishedDiagnostic=$true;UserProfileUnchanged=$true}
}finally{if(Test-Path -LiteralPath $root){Remove-Item -LiteralPath $root -Recurse -Force}}
