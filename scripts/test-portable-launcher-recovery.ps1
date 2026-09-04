[CmdletBinding()]param()
Set-StrictMode -Version Latest
$ErrorActionPreference='Stop'
$repo=(Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '..')).Path
$private=[IO.Path]::GetFullPath((Join-Path $repo 'private'))
$root=[IO.Path]::GetFullPath((Join-Path $private ('evidence\M7-016\launcher-recovery-test-'+[guid]::NewGuid().ToString('N').Substring(0,8))))
if(-not$root.StartsWith($private+[IO.Path]::DirectorySeparatorChar,[StringComparison]::OrdinalIgnoreCase)){throw 'Recovery fixture escaped private/.'}
$utf8=[Text.UTF8Encoding]::new($false)
function Hash($Path){$s=[IO.File]::OpenRead($Path);try{$h=[Security.Cryptography.SHA256]::Create();try{([BitConverter]::ToString($h.ComputeHash($s))).Replace('-','')}finally{$h.Dispose()}}finally{$s.Dispose()}}
function Lock($Root){$files=@('Launch-MCLA.exe','bundle-id.txt','bundle-manifest.json','game-manifest.json','bin/mcla.exe','bin/mcla_crash_handler.exe','bin/mcla.toml','bin/rexgpu-xenos.dll','bin/rexruntime.dll','bin/TracyClient.dll','game/default.xex');$rows=@($files|Sort-Object|ForEach-Object{"$(Hash (Join-Path $Root $_.Replace('/','\'))) *$_"});[IO.File]::WriteAllText((Join-Path $Root 'bundle-files.sha256'),(($rows-join"`n")+"`n"),$utf8)}
try{
  @('bin','game','user\B13EBABEBABEBABE\545407F8\00000001\mc4.sav','user\B13EBABEBABEBABE\545407F8\Headers\00000001','cache','logs','diagnostics','results','update','metadata')|ForEach-Object{[IO.Directory]::CreateDirectory((Join-Path $root $_))|Out-Null}
  $launcher=Join-Path $repo 'out\build\win-amd64-release\Launch-MCLA.exe';$dummy=Join-Path ([Environment]::GetFolderPath([Environment+SpecialFolder]::System)) 'where.exe'
  if(-not(Test-Path -LiteralPath $launcher -PathType Leaf)){throw 'Build the Release portable launcher before its recovery test.'}
  Copy-Item -LiteralPath $launcher -Destination (Join-Path $root 'Launch-MCLA.exe');Copy-Item -LiteralPath $dummy -Destination (Join-Path $root 'bin\mcla.exe')
  @('bin/mcla_crash_handler.exe','bin/rexgpu-xenos.dll','bin/rexruntime.dll','bin/TracyClient.dll','game/default.xex')|ForEach-Object{[IO.File]::WriteAllText((Join-Path $root $_.Replace('/','\')),"fixture:$_`n",$utf8)}
  [IO.File]::WriteAllText((Join-Path $root 'bin\mcla.toml'),"fullscreen = true`n",$utf8)
  [IO.File]::WriteAllText((Join-Path $root 'user\B13EBABEBABEBABE\545407F8\00000001\mc4.sav\mc4.sav'),"recovery-save`n",$utf8)
  [IO.File]::WriteAllText((Join-Path $root 'user\B13EBABEBABEBABE\545407F8\Headers\00000001\mc4.sav.header'),"recovery-header`n",$utf8)
  $id='mcla-0.9.0.1-win64-RECOVERYTEST0001-RECOVERYTEST0002-RECOVERYTEST0003'
  [IO.File]::WriteAllText((Join-Path $root 'bundle-id.txt'),$id+"`n",$utf8);[IO.File]::WriteAllText((Join-Path $root 'bundle-manifest.json'),"{}`n",$utf8);[IO.File]::WriteAllText((Join-Path $root 'game-manifest.json'),"{}`n",$utf8);Lock $root
  $sid='session-20260904T122927Z-300';$partial=Join-Path $root ("results\$sid.partial");[IO.Directory]::CreateDirectory($partial)|Out-Null
  $session=[ordered]@{schema='mcla-portable-session-v1';bundle_id=$id;session_id=$sid;started_utc='20260904T122927Z';mode='gameplay';state='running'}
  [IO.File]::WriteAllText((Join-Path $partial 'session.json'),(($session|ConvertTo-Json)+"`n"),$utf8);[IO.File]::WriteAllText((Join-Path $root 'bundle.lock'),"stale`n",$utf8)
  $stdout=Join-Path $root 'launcher-stdout.txt';$stderr=Join-Path $root 'launcher-stderr.txt'
  $p=Start-Process -FilePath (Join-Path $root 'Launch-MCLA.exe') -WorkingDirectory $root -WindowStyle Hidden -RedirectStandardOutput $stdout -RedirectStandardError $stderr -PassThru
  if(-not$p.WaitForExit(15000)){$p.Kill($true);$p.WaitForExit();$detail=((Get-Content -Raw $stdout -ErrorAction SilentlyContinue)+(Get-Content -Raw $stderr -ErrorAction SilentlyContinue));throw "Recovery fixture timed out: $detail"};if($p.ExitCode-eq0){throw 'Dummy title unexpectedly exited zero.'}
  if(Test-Path -LiteralPath (Join-Path $root 'bundle.lock')){throw 'Recovered launcher left bundle.lock.'};if(@(Get-ChildItem (Join-Path $root 'results') -Recurse -Force|Where-Object Name -Like '*.partial').Count){throw 'Recovered launcher left partial state.'}
  $recovered=Get-Content -Raw -LiteralPath (Join-Path $root "results\$sid\result.json")|ConvertFrom-Json
  if($recovered.schema-cne'mcla-portable-interrupted-session-result-v1'-or$recovered.bundle_id-cne$id-or$recovered.session_id-cne$sid-or$recovered.state-cne'interrupted'-or$null-ne$recovered.exit_code-or$recovered.interruption_reason-cne'launcher-terminated-before-session-finalization'-or[int]$recovered.save_snapshot_count-ne1){throw 'Interrupted-session result is invalid.'}
  $next=@(Get-ChildItem (Join-Path $root 'results') -Directory|Where-Object Name -NE $sid);if($next.Count-ne1){throw 'Follow-up session count is invalid.'};$normal=Get-Content -Raw (Join-Path $next[0].FullName 'result.json')|ConvertFrom-Json
  if($normal.schema-cne'mcla-portable-session-result-v1'-or$normal.state-cne'complete'-or[int]$normal.exit_code-eq0){throw 'Follow-up session did not finalize.'}
  [pscustomobject]@{Passed=$true;StaleLockRecovered=$true;InterruptedSessionRecovered=$true;SaveSnapshots=1;FollowUpSessionCompleted=$true}
}finally{if(Test-Path -LiteralPath $root){Remove-Item -LiteralPath $root -Recurse -Force}}
