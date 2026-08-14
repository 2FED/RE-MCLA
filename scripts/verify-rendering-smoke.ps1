[CmdletBinding(DefaultParameterSetName = 'Probe')]
param(
    [Parameter(Mandatory, ParameterSetName = 'Probe')][string]$RuntimeLogPath,
    [Parameter(Mandatory, ParameterSetName = 'Probe')][string]$UserRoot,
    [Parameter(ParameterSetName = 'Probe')][switch]$ProbeOnly,
    [Parameter(ParameterSetName = 'Probe')][switch]$FixtureMode,
    [Parameter(Mandatory, ParameterSetName = 'Result')][string]$ResultPath
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$repo = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '..')).Path
$utf8 = [Text.UTF8Encoding]::new($false)
$gameVerify = Join-Path $PSScriptRoot 'verify-game-manifest.ps1'
$baselineResultRelative = 'private/evidence/M5-002/20260814-093131-ddca5b9d/result.json'
$baselineResultHash = 'A582A41BB254D9187BC6F8147E89E2FF2E92D0EA4B79945760BA6FBCF334BC28'
$xeniaRelative = 'private/tools/xenia-canary/artifacts/screenshots/545407F8/545407F8 - 2026-08-11T00-53-02.png'
$xeniaHash = 'A490553067A8F375191F618C34556B82C9FD244FF295519AB4383AAB40434F8B'
$phases = @('world','sky','street','particle-a','particle-b','particle-c')
$trafficPhases = @(1..30 | ForEach-Object { 'traffic-{0:D2}' -f $_ })

function Resolve-Safe([string]$Path, [string]$Description, [bool]$Directory = $false) {
    $full = [IO.Path]::GetFullPath($(if ([IO.Path]::IsPathRooted($Path)) { $Path } else { Join-Path $repo $Path }))
    $prefix = $repo.TrimEnd('\') + '\'
    if (-not $full.StartsWith($prefix, [StringComparison]::OrdinalIgnoreCase)) { throw "$Description escapes repository." }
    $current = $repo
    foreach ($part in @($full.Substring($prefix.Length).Split('\') | Where-Object Length)) {
        $current = Join-Path $current $part
        if (-not (Test-Path -LiteralPath $current)) { throw "$Description is missing." }
        if ((Get-Item -LiteralPath $current -Force).Attributes -band [IO.FileAttributes]::ReparsePoint) { throw "$Description traverses a reparse point." }
    }
    if ($Directory -and -not (Test-Path -LiteralPath $full -PathType Container)) { throw "$Description is not a directory." }
    if (-not $Directory -and -not (Test-Path -LiteralPath $full -PathType Leaf)) { throw "$Description is not a file." }
    $full
}

function Assert-NoReparse([string]$Root) {
    $pending = [Collections.Generic.Stack[string]]::new(); $pending.Push($Root)
    while ($pending.Count) {
        foreach ($item in @(Get-ChildItem -LiteralPath $pending.Pop() -Force)) {
            if ($item.Attributes -band [IO.FileAttributes]::ReparsePoint) { throw 'Tree contains a reparse point.' }
            if ($item.PSIsContainer) { $pending.Push($item.FullName) }
        }
    }
}

function Get-Tree([string]$Root) {
    $root = Resolve-Safe $Root 'Tree' $true
    Assert-NoReparse $root
    $items = @(Get-ChildItem -LiteralPath $root -Recurse -Force)
    $files = @($items | Where-Object { -not $_.PSIsContainer } | Sort-Object FullName)
    $entries = @()
    foreach ($directory in @($items | Where-Object PSIsContainer | Sort-Object FullName)) { $entries += [ordered]@{ kind='directory'; path=$directory.FullName.Substring($root.Length).TrimStart('\').Replace('\','/') } }
    foreach ($file in $files) { $entries += [ordered]@{ kind='file'; path=$file.FullName.Substring($root.Length).TrimStart('\').Replace('\','/'); length=$file.Length; sha256=(Get-FileHash -LiteralPath $file.FullName -Algorithm SHA256).Hash } }
    $json = ConvertTo-Json -InputObject @($entries) -Compress -Depth 4
    $sha = [Security.Cryptography.SHA256]::Create(); try { $hash = -join ($sha.ComputeHash($utf8.GetBytes($json)) | ForEach-Object { $_.ToString('X2') }) } finally { $sha.Dispose() }
    $bytes = 0L; foreach ($file in $files) { $bytes += $file.Length }
    [pscustomobject]@{ Hash=$hash; FileCount=$files.Count; DirectoryCount=@($items | Where-Object PSIsContainer).Count; Bytes=$bytes }
}

function Get-GameIdentity {
    $root = Resolve-Safe 'private/game' 'Canonical game' $true
    $tree = Get-Tree $root
    $verified = & $gameVerify -GamePath $root -VerifyHashes
    [ordered]@{ file_count=$verified.FileCount; payload_bytes=$verified.PayloadBytes; manifest_sha256=(Get-FileHash -LiteralPath $verified.ManifestPath -Algorithm SHA256).Hash; tree_sha256=$tree.Hash; tree_file_count=$tree.FileCount; tree_directory_count=$tree.DirectoryCount; tree_bytes=$tree.Bytes }
}

function Get-Artifacts {
    $build = Resolve-Safe 'out/build/win-amd64-relwithdebinfo' 'Canonical build' $true
    @('mcla.exe','rexruntimerd.dll','TracyClientrd.dll','rexgpu-xenosrd.dll') | ForEach-Object { [ordered]@{ name=$_; sha256=(Get-FileHash -LiteralPath (Resolve-Safe (Join-Path $build $_) "Artifact $_") -Algorithm SHA256).Hash } }
}

function Get-LogSet([string]$Current) {
    $current = Resolve-Safe $Current 'Runtime log'
    if ((Split-Path $current -Leaf) -cne 'mcla.log') { throw 'Current runtime log must be mcla.log.' }
    $files = @(Get-ChildItem -LiteralPath (Split-Path $current) -File -Filter 'mcla*.log')
    $rotated = @(); $now = $null
    foreach ($file in $files) {
        if ($file.Name -ceq 'mcla.log') { $now = $file; continue }
        $match = [regex]::Match($file.Name, '^mcla\.([1-9][0-9]*)\.log$')
        if (-not $match.Success) { throw 'Malformed runtime-log rotation.' }
        $rotated += [pscustomobject]@{ Index=[int]$match.Groups[1].Value; File=$file }
    }
    if (-not $now -or $files.Count -gt 24) { throw 'Runtime-log topology is invalid.' }
    $indices = @($rotated | ForEach-Object Index | Sort-Object)
    for ($index=0; $index -lt $indices.Count; $index++) { if ($indices[$index] -ne $index + 1) { throw 'Runtime-log rotations are not contiguous.' } }
    $ordered = @($rotated | Sort-Object Index -Descending | ForEach-Object File) + $now
    $lines = [Collections.Generic.List[string]]::new(); $manifest=@(); $bytes=0L
    foreach ($file in $ordered) {
        $bytes += $file.Length
        if ($bytes -gt 268435456) { throw 'Runtime logs exceed 256 MiB.' }
        foreach ($line in [IO.File]::ReadLines($file.FullName)) { $lines.Add($line) }
        $manifest += [ordered]@{ name=$file.Name; bytes=$file.Length; sha256=(Get-FileHash -LiteralPath $file.FullName -Algorithm SHA256).Hash }
    }
    $json = ConvertTo-Json -InputObject @($manifest) -Compress -Depth 3
    $sha=[Security.Cryptography.SHA256]::Create(); try { $hash=-join($sha.ComputeHash($utf8.GetBytes($json))|ForEach-Object{$_.ToString('X2')}) } finally { $sha.Dispose() }
    [pscustomobject]@{ Lines=$lines; Text=$lines -join "`n"; Files=@($manifest); Count=$manifest.Count; Bytes=$bytes; Hash=$hash }
}

function Get-OnlyLine([Collections.Generic.List[string]]$Lines, [string]$Pattern, [string]$Description) {
    $hits=@()
    for($i=0;$i-lt$Lines.Count;$i++){ if($Lines[$i] -match $Pattern){ $hits += [pscustomobject]@{ Index=$i; Line=$Lines[$i]; Match=$Matches } } }
    if($hits.Count-ne 1){throw "$Description count is $($hits.Count), expected 1."}
    $hits[0]
}

function Convert-Fields([string]$Line) {
    $fields=[ordered]@{}
    foreach($match in [regex]::Matches($Line,'(?<key>[a-z0-9_]+)=(?<value>[^\s]+)')){$key=$match.Groups['key'].Value;if($fields.Contains($key)){throw "Duplicate summary field '$key'."};$fields[$key]=$match.Groups['value'].Value}
    $fields
}

function Assert-FieldSchema($Fields,[string[]]$Expected,[string]$Description){if(($Fields.Keys-join',')-cne($Expected-join',')){throw "$Description field schema changed."}}

function Get-Bmp([string]$Path) {
    $path=Resolve-Safe $Path 'Rendering BMP'
    $bytes=[IO.File]::ReadAllBytes($path)
    if($bytes.Length-ne 3686454 -or $bytes[0]-ne 0x42 -or $bytes[1]-ne 0x4D){throw 'Rendering BMP format/size changed.'}
    $width=[BitConverter]::ToInt32($bytes,18);$height=[BitConverter]::ToInt32($bytes,22);$bpp=[BitConverter]::ToInt16($bytes,28);$offset=[BitConverter]::ToInt32($bytes,10)
    if($width-ne1280-or$height-ne720-or$bpp-ne32-or$offset-ne54){throw 'Rendering BMP dimensions/layout changed.'}
    [pscustomobject]@{ Path=$path; Bytes=$bytes.Length; Sha256=(Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash; Data=$bytes }
}

function Get-DifferentPixelCount($A,$B,[int]$X,[int]$Y,[int]$Width,[int]$Height,[int]$Threshold=12) {
    $count=0
    for($py=$Y;$py-lt($Y+$Height);$py+=2){$diskY=719-$py;for($px=$X;$px-lt($X+$Width);$px+=2){$index=54+(($diskY*1280+$px)*4);$delta=[Math]::Abs([int]$A.Data[$index]-[int]$B.Data[$index])+[Math]::Abs([int]$A.Data[$index+1]-[int]$B.Data[$index+1])+[Math]::Abs([int]$A.Data[$index+2]-[int]$B.Data[$index+2]);if($delta-gt$Threshold){$count++}}}
    $count
}

function Get-Probe([string]$Log,[string]$User,[bool]$Fixture) {
    $logSet=Get-LogSet $Log;$lines=$logSet.Lines;$text=$logSet.Text
    $launch=Get-OnlyLine $lines 'KernelState: Preparing module launch' 'Module launch marker'
    $config=Get-OnlyLine $lines 'MCLA_RENDER_SMOKE_CONFIG v=1 slot=0 gameplay_wait_seconds=45 traffic_samples=30 traffic_interval_ms=1000 camera_hold_ms=1200 particle_hold_ms=15000 dismiss_pulses=6 frames=36$' 'Render config'
    $summary=Get-OnlyLine $lines 'MCLA_RENDER_SMOKE_SUMMARY v=1 status=PASS frames=36 frontend_input_records=28 render_input_records=8 external_close_required=1$' 'Render summary'
    $xconfig=Get-OnlyLine $lines 'XENOS_AUDIT_CONFIG v=1 backend=d3d12 rt_path=host bindless=[01] scale_x=1 scale_y=1 native_2x_supported=[01] gamma_rt_unorm16=[01] depth_f24_ps=[01] depth_f24_round=[01] direct_host_resolve=1$' 'Xenos config'
    $startPatterns=@(
        'MCLA_FRONTEND_SMOKE_INPUT v=1 side=source sequence=1 buttons=0010$',
        'MCLA_FRONTEND_SMOKE_INPUT v=1 side=guest sequence=1 buttons=0010$',
        'MCLA_FRONTEND_SMOKE_INPUT v=1 side=source sequence=1 buttons=0000$',
        'MCLA_FRONTEND_SMOKE_INPUT v=1 side=guest sequence=1 buttons=0000$')
    $start=@();foreach($pattern in $startPatterns){$start+=Get-OnlyLine $lines $pattern 'Title Start input'}
    $dismiss=@{};foreach($sequence in 2..7){$dismiss[$sequence]=@();foreach($pattern in @("MCLA_FRONTEND_SMOKE_INPUT v=1 side=source sequence=$sequence buttons=1000$","MCLA_FRONTEND_SMOKE_INPUT v=1 side=guest sequence=$sequence buttons=1000$","MCLA_FRONTEND_SMOKE_INPUT v=1 side=source sequence=$sequence buttons=0000$","MCLA_FRONTEND_SMOKE_INPUT v=1 side=guest sequence=$sequence buttons=0000$")){$dismiss[$sequence]+=Get-OnlyLine $lines $pattern "Dismiss input $sequence"}}
    $frame=@{};foreach($phase in $phases){$frame[$phase]=Get-OnlyLine $lines "MCLA_RENDER_SMOKE_FRAME v=1 phase=$([regex]::Escape($phase)) width=1280 height=720 status=PASS$" "Frame $phase"}
    $trafficFrame=@{};foreach($phase in $trafficPhases){$trafficFrame[$phase]=Get-OnlyLine $lines "MCLA_RENDER_SMOKE_FRAME v=1 phase=$phase width=1280 height=720 status=PASS$" "Frame $phase"}
    $renderPatterns=@(
        'MCLA_RENDER_SMOKE_INPUT v=1 side=source sequence=1 buttons=0000 lt=0 rt=0 lx=0 ly=0 rx=0 ry=32767$',
        'MCLA_RENDER_SMOKE_INPUT v=1 side=guest sequence=1 buttons=0000 lt=0 rt=0 lx=0 ly=0 rx=0 ry=32767$',
        'MCLA_RENDER_SMOKE_INPUT v=1 side=source sequence=1 buttons=0000 lt=0 rt=0 lx=0 ly=0 rx=0 ry=0$',
        'MCLA_RENDER_SMOKE_INPUT v=1 side=guest sequence=1 buttons=0000 lt=0 rt=0 lx=0 ly=0 rx=0 ry=0$',
        'MCLA_RENDER_SMOKE_INPUT v=1 side=source sequence=2 buttons=0000 lt=255 rt=255 lx=0 ly=0 rx=0 ry=0$',
        'MCLA_RENDER_SMOKE_INPUT v=1 side=guest sequence=2 buttons=0000 lt=255 rt=255 lx=0 ly=0 rx=0 ry=0$',
        'MCLA_RENDER_SMOKE_INPUT v=1 side=source sequence=2 buttons=0000 lt=0 rt=0 lx=0 ly=0 rx=0 ry=0$',
        'MCLA_RENDER_SMOKE_INPUT v=1 side=guest sequence=2 buttons=0000 lt=0 rt=0 lx=0 ly=0 rx=0 ry=0$')
    $render=@();foreach($pattern in $renderPatterns){$render+=Get-OnlyLine $lines $pattern 'Render input'}
    $pipeline=Get-OnlyLine $lines 'XENOS_AUDIT_PIPELINE_SUMMARY v=1 phase=checkpoint ' 'Pipeline summary'
    $cp=Get-OnlyLine $lines 'XENOS_AUDIT_CP_SUMMARY v=1 phase=checkpoint ' 'Command processor summary'
    $rt=Get-OnlyLine $lines 'XENOS_AUDIT_RT_SUMMARY v=1 phase=checkpoint ' 'Render target summary'
    $resolve=Get-OnlyLine $lines 'XENOS_AUDIT_RESOLVE_SUMMARY v=1 phase=checkpoint ' 'Resolve summary'
    $closing=Get-OnlyLine $lines 'Window closing, shutting down\.\.\.$' 'WM_CLOSE marker'
    $complete=Get-OnlyLine $lines 'Execution complete$' 'Execution-complete marker'
    $hard=Get-OnlyLine $lines 'Title terminated; hard-exiting process\.$' 'Hard-exit marker'
    $trafficOrder=@();for($sample=1;$sample-le30;$sample++){if($sample%5-eq0){$trafficOrder+=@($dismiss[1+$sample/5])};$trafficOrder+=$trafficFrame[('traffic-{0:D2}'-f$sample)]}
    $ordered=@($launch,$config)+$start+@($frame.world)+$trafficOrder+$render[0..1]+@($frame.sky)+$render[2..3]+@($frame.street)+$render[4..5]+@($frame.'particle-a',$frame.'particle-b',$frame.'particle-c')+$render[6..7]+@($summary,$pipeline,$cp,$rt,$resolve,$closing,$complete,$hard)
    for($i=1;$i-lt$ordered.Count;$i++){if($ordered[$i-1].Index-ge$ordered[$i].Index){throw "Render chronology failed at index $i."}}
    if($xconfig.Index-ge$config.Index){throw 'Xenos config must precede rendering route.'}
    $pf=Convert-Fields $pipeline.Line;$cf=Convert-Fields $cp.Line;$rf=Convert-Fields $rt.Line;$vf=Convert-Fields $resolve.Line
    Assert-FieldSchema $pf @('v','phase','shader_entries','translate_vs_ok','translate_ps_ok','translate_fail','pso_entries','pso_attempt','pso_ok','pso_fail','shader_records','shader_overflow','pso_records','pso_overflow') 'Pipeline summary'
    Assert-FieldSchema $cf @('v','phase','draw_issued','draw_indexed','draw_nonindexed','pso_pending_skip','pso_failed_skip','depth_test','depth_write','stencil','depth_bound','depth_without_bound','bind_records','bind_overflow','msaa1','msaa2','msaa4','gamma_table_dispatch','gamma_pwl_dispatch','gamma_identity_dispatch','gamma_nonidentity_dispatch','gamma_table_writes','gamma_pwl_writes','gamma_uploads','gamma_records','gamma_overflow','refresh_fail') 'Command processor summary'
    Assert-FieldSchema $rf @('v','phase','create_attempt','create_ok','create_fail','records','overflow','host_depth_store','ownership_draws','ownership_modes','ownership_overflow') 'Render target summary'
    Assert-FieldSchema $vf @('v','phase','calls','info_ok','info_fail','zero_area','shader_known','shader_unknown','direct_preflight','direct_preflight_dump_ok','direct_reject','fallback_dump_ok','fallback_dump_fail','copy_dispatch','final_ok','final_fail','modes','overflow','true_direct_dispatch') 'Resolve summary'
    foreach($required in @(@($pf,'translate_fail','0'),@($pf,'pso_fail','0'),@($pf,'pso_overflow','0'),@($cf,'pso_failed_skip','0'),@($cf,'bind_overflow','0'),@($cf,'gamma_overflow','0'),@($cf,'refresh_fail','0'),@($rf,'create_fail','0'),@($rf,'overflow','0'),@($rf,'ownership_overflow','0'),@($vf,'info_fail','0'),@($vf,'shader_unknown','0'),@($vf,'fallback_dump_fail','0'),@($vf,'final_fail','0'),@($vf,'overflow','0'),@($vf,'true_direct_dispatch','0'))){if(-not$required[0].Contains($required[1])-or$required[0][$required[1]]-cne$required[2]){throw "GPU failure field $($required[1]) changed."}}
    if([int64]$pf.pso_ok-lt300-or[int64]$cf.draw_issued-lt100000-or[int64]$cf.depth_test-lt1-or[int64]$cf.depth_write-lt1-or[int64]$cf.msaa2-lt1-or[int64]$cf.msaa4-lt1-or[int64]$cf.gamma_nonidentity_dispatch-lt1-or[int64]$rf.create_ok-lt20-or[int64]$rf.ownership_draws-lt1-or[int64]$vf.calls-lt10000-or[int64]$vf.copy_dispatch-lt10000){throw 'Gameplay GPU coverage is below the rendering floor.'}
    if([int64]$pf.pso_attempt-ne([int64]$pf.pso_ok+[int64]$pf.pso_fail)-or[int64]$cf.draw_issued-ne([int64]$cf.draw_indexed+[int64]$cf.draw_nonindexed)-or[int64]$cf.draw_issued-ne([int64]$cf.msaa1+[int64]$cf.msaa2+[int64]$cf.msaa4)-or([int64]$cf.gamma_table_dispatch+[int64]$cf.gamma_pwl_dispatch)-ne([int64]$cf.gamma_identity_dispatch+[int64]$cf.gamma_nonidentity_dispatch)-or[int64]$rf.create_attempt-ne([int64]$rf.create_ok+[int64]$rf.create_fail)-or[int64]$vf.calls-ne([int64]$vf.info_ok+[int64]$vf.info_fail)-or[int64]$vf.calls-ne([int64]$vf.final_ok+[int64]$vf.final_fail)-or[int64]$vf.direct_preflight-ne([int64]$vf.direct_preflight_dump_ok+[int64]$vf.direct_reject)){throw 'Gameplay GPU summary arithmetic is inconsistent.'}
    if([int64]$pf.shader_records-ne256-or[int64]$pf.shader_overflow-lt1-or[int64]$pf.shader_overflow-gt2048){throw 'Bounded gameplay shader-record saturation changed.'}
    $firstSummary=@($pipeline.Index,$cp.Index,$rt.Index,$resolve.Index|Measure-Object -Minimum).Minimum
    for($i=$firstSummary+1;$i-lt$lines.Count;$i++){if($lines[$i]-match'XENOS_AUDIT_(?:CONFIG|RT|BIND|OWNERSHIP|RESOLVE|SHADER|PSO|DRAW|GAMMA) v='){throw 'Xenos detail/config marker appeared after the frozen summary boundary.'}}
    if($text-match'(?i)(REX_GUEST_CRASH|GUEST_CRASH_REPORT|PPC_UNIMPLEMENTED|\[fatal\]|assertion failed|D3D12.*device (?:lost|removed)|XENOS_AUDIT_FAILURE)'){throw 'Fatal, unsupported, or device-loss marker found.'}
    if($Fixture){return [pscustomobject]@{Passed=$true;LogSet=$logSet}}
    $user=Resolve-Safe $User 'Rendering user root' $true;Assert-NoReparse $user
    $bmps=@{};foreach($phase in $phases){$bmps[$phase]=Get-Bmp (Join-Path $user "mcla-render-$phase.bmp")}
    $trafficBmps=@{};foreach($phase in $trafficPhases){$trafficBmps[$phase]=Get-Bmp (Join-Path $user "mcla-render-$phase.bmp")}
    if(@(Get-ChildItem -LiteralPath $user -Recurse -File -Filter 'mcla-render-*.bmp').Count-ne36){throw 'Rendering capture topology changed.'}
    if(@($bmps.Values|ForEach-Object Sha256|Select-Object -Unique).Count-ne6){throw 'Rendering captures are not temporally distinct.'}
    $skyDifference=Get-DifferentPixelCount $bmps.world $bmps.sky 0 0 1280 720
    $particleAB=Get-DifferentPixelCount $bmps.'particle-a' $bmps.'particle-b' 300 360 700 360
    $particleBC=Get-DifferentPixelCount $bmps.'particle-b' $bmps.'particle-c' 300 360 700 360
    $trafficDifference=-1;$trafficSelected=$null
    foreach($phase in $trafficPhases){$difference=Get-DifferentPixelCount $bmps.world $trafficBmps[$phase] 0 240 1280 180;if($difference-gt$trafficDifference){$trafficDifference=$difference;$trafficSelected=$phase}}
    if($skyDifference-lt5000-or$particleAB-lt1000-or$particleBC-lt1000-or$trafficDifference-lt500){throw 'Camera/traffic/particle temporal image evidence is below floor.'}
    [pscustomobject]@{Passed=$true;LogSet=$logSet;Bmps=$bmps;TrafficBmps=$trafficBmps;TrafficSelected=$trafficSelected;TrafficDifference=$trafficDifference;SkyDifference=$skyDifference;ParticleAB=$particleAB;ParticleBC=$particleBC;PsoOk=[int64]$pf.pso_ok;Draws=[int64]$cf.draw_issued;Resolves=[int64]$vf.calls;ShaderOverflow=[int64]$pf.shader_overflow}
}

if($PSCmdlet.ParameterSetName-eq'Probe'){
    if(-not$ProbeOnly){throw 'Probe mode requires -ProbeOnly.'}
    Get-Probe $RuntimeLogPath $UserRoot $FixtureMode.IsPresent
    return
}

$result=Resolve-Safe $ResultPath 'M5-003 result';$raw=[IO.File]::ReadAllText($result)
if($raw-match'(?i)([A-Z]:[\\/]|\\\\[^"\s]+[\\/]|(?:^|["\\/])private[\\/])'){throw 'Result contains a private or absolute path.'}
$record=$raw|ConvertFrom-Json
$properties=@('schema','task','decision','sdk_version','route_id','baseline','build','seed','game_identity','artifacts','cycle','visual_review','no_surviving_processes','data_integrity_preserved')
if(($record.PSObject.Properties.Name-join',')-cne($properties-join',')-or$record.schema-ne1-or$record.task-cne'M5-003'-or$record.decision-cne'rendering-categories-pass'-or$record.sdk_version-cne'0.9.0.18'-or$record.route_id-cne'pinned-save-sunset-strip-rendering-v1'-or$record.no_surviving_processes-ne$true-or$record.data_integrity_preserved-ne$true){throw 'M5-003 result identity/scope failed.'}
$evidenceRoot=Resolve-Safe 'private/evidence/M5-003' 'M5-003 evidence root' $true;$resultRoot=Split-Path -Parent $result
if((Split-Path $result -Leaf)-cne'result.json'-or(Split-Path $resultRoot -Parent)-cne$evidenceRoot-or(Split-Path $resultRoot -Leaf)-notmatch'^[0-9]{8}-[0-9]{6}-[0-9a-f]{8}$'){throw 'M5-003 result location is not canonical.'}
$rootChildren=@(Get-ChildItem -LiteralPath $resultRoot -Force|Sort-Object Name|ForEach-Object Name);if(($rootChildren-join',')-cne'candidate.json,relwithdebinfo-clean-build.log,result.json,runs'){throw 'M5-003 result-root topology changed.'}
$baselineProperties=@('m5_002_result_sha256','xenia_free_roam_sha256');$buildProperties=@('app_build_log_sha256','executable_sha256');$seedProperties=@('before_sha256','after_sha256','file_count')
if(($record.baseline.PSObject.Properties.Name-join',')-cne($baselineProperties-join',')-or($record.build.PSObject.Properties.Name-join',')-cne($buildProperties-join',')-or($record.seed.PSObject.Properties.Name-join',')-cne($seedProperties-join',')-or($record.game_identity.PSObject.Properties.Name-join',')-cne'before,after'-or($record.artifacts.PSObject.Properties.Name-join',')-cne'before,after'){throw 'M5-003 nested result schema changed.'}
$baseline=Resolve-Safe $baselineResultRelative 'M5-002 baseline';$xenia=Resolve-Safe $xeniaRelative 'Xenia rendering reference'
if($record.baseline.m5_002_result_sha256-cne$baselineResultHash-or(Get-FileHash $baseline -Algorithm SHA256).Hash-cne$baselineResultHash-or$record.baseline.xenia_free_roam_sha256-cne$xeniaHash-or(Get-FileHash $xenia -Algorithm SHA256).Hash-cne$xeniaHash){throw 'Rendering baseline identity changed.'}
$root=$resultRoot;$buildLog=Resolve-Safe (Join-Path $root 'relwithdebinfo-clean-build.log') 'Build log'
if((Get-FileHash $buildLog -Algorithm SHA256).Hash-cne$record.build.app_build_log_sha256){throw 'Build log hash mismatch.'}
$exe=Resolve-Safe 'out/build/win-amd64-relwithdebinfo/mcla.exe' 'Canonical executable';if((Get-FileHash $exe -Algorithm SHA256).Hash-cne$record.build.executable_sha256){throw 'Executable hash mismatch.'}
$seed=Get-Tree (Resolve-Safe 'private/baseline/M4-011/post-oobe-profile' 'Pinned seed' $true);if($record.seed.before_sha256-cne$seed.Hash-or$record.seed.after_sha256-cne$seed.Hash-or$record.seed.file_count-ne2){throw 'Pinned seed changed.'}
$game=Get-GameIdentity;if(($record.game_identity.before|ConvertTo-Json -Compress)-cne($game|ConvertTo-Json -Compress)-or($record.game_identity.after|ConvertTo-Json -Compress)-cne($game|ConvertTo-Json -Compress)){throw 'Source-game identity changed.'}
$artifacts=@(Get-Artifacts);if(($record.artifacts.before|ConvertTo-Json -Compress)-cne($artifacts|ConvertTo-Json -Compress)-or($record.artifacts.after|ConvertTo-Json -Compress)-cne($artifacts|ConvertTo-Json -Compress)){throw 'Runtime artifacts changed.'}
if($record.cycle.relative_root-cne'runs/01'-or$record.cycle.exit_code-ne0-or$record.cycle.close_requested-ne$true-or$record.cycle.harness_force_cleanup-ne$false){throw 'Cycle lifecycle failed.'}
$cycle=Resolve-Safe (Join-Path $root $record.cycle.relative_root) 'M5-003 cycle' $true;$probe=Get-Probe (Join-Path $cycle 'mcla.log') (Join-Path $cycle 'user') $false
$cycleProperties=@('relative_root','exit_code','close_requested','harness_force_cleanup','runtime_logs','runtime_log_set_sha256','runtime_log_file_count','runtime_log_bytes','captures','traffic_captures','traffic_selected_name','traffic_difference_pixels','contact_sheet_sha256','sky_difference_pixels','particle_ab_difference_pixels','particle_bc_difference_pixels','pso_ok','draws','resolves','shader_record_overflow','tree_sha256','tree_file_count','tree_directory_count','tree_bytes')
if(($record.cycle.PSObject.Properties.Name-join',')-cne($cycleProperties-join',')){throw 'M5-003 cycle schema changed.'};$cycleTree=Get-Tree $cycle;if($record.cycle.tree_sha256-cne$cycleTree.Hash-or$record.cycle.tree_file_count-ne$cycleTree.FileCount-or$record.cycle.tree_directory_count-ne$cycleTree.DirectoryCount-or$record.cycle.tree_bytes-ne$cycleTree.Bytes){throw 'M5-003 cycle tree changed.'}
if($record.cycle.runtime_log_set_sha256-cne$probe.LogSet.Hash-or$record.cycle.runtime_log_file_count-ne$probe.LogSet.Count-or$record.cycle.runtime_log_bytes-ne$probe.LogSet.Bytes-or($record.cycle.runtime_logs|ConvertTo-Json -Compress)-cne($probe.LogSet.Files|ConvertTo-Json -Compress)-or$record.cycle.traffic_selected_name-cne$probe.TrafficSelected-or$record.cycle.traffic_difference_pixels-ne$probe.TrafficDifference-or$record.cycle.sky_difference_pixels-ne$probe.SkyDifference-or$record.cycle.particle_ab_difference_pixels-ne$probe.ParticleAB-or$record.cycle.particle_bc_difference_pixels-ne$probe.ParticleBC-or$record.cycle.pso_ok-ne$probe.PsoOk-or$record.cycle.draws-ne$probe.Draws-or$record.cycle.resolves-ne$probe.Resolves-or$record.cycle.shader_record_overflow-ne$probe.ShaderOverflow){throw 'Cycle metrics do not match physical evidence.'}
foreach($phase in $phases){if($record.cycle.captures.$phase.sha256-cne$probe.Bmps[$phase].Sha256-or$record.cycle.captures.$phase.bytes-ne$probe.Bmps[$phase].Bytes){throw 'Capture/result mismatch.'}}
if($record.cycle.traffic_captures.Count-ne30){throw 'Traffic capture manifest count changed.'};for($i=0;$i-lt30;$i++){$phase=$trafficPhases[$i];$entry=$record.cycle.traffic_captures[$i];if($entry.name-cne$phase-or$entry.sha256-cne$probe.TrafficBmps[$phase].Sha256-or$entry.bytes-ne$probe.TrafficBmps[$phase].Bytes){throw 'Traffic capture/result mismatch.'}}
$contact=Resolve-Safe (Join-Path $cycle 'user/rendering-contact-sheet.png') 'Contact sheet';if((Get-FileHash $contact -Algorithm SHA256).Hash-cne$record.cycle.contact_sheet_sha256){throw 'Contact sheet hash mismatch.'}
$contactBytes=[IO.File]::ReadAllBytes($contact);if($contactBytes.Length-lt24-or$contactBytes[0]-ne137-or$contactBytes[1]-ne80-or[Net.IPAddress]::NetworkToHostOrder([BitConverter]::ToInt32($contactBytes,16))-ne1280-or[Net.IPAddress]::NetworkToHostOrder([BitConverter]::ToInt32($contactBytes,20))-ne450){throw 'Contact sheet PNG layout changed.'}
$visualProperties=@('status','reviewer','categories','known_minor_issue')
if(($record.visual_review.PSObject.Properties.Name-join',')-cne($visualProperties-join',')-or$record.visual_review.status-cne'pass'-or$record.visual_review.reviewer-cne'owner'-or$record.visual_review.known_minor_issue-cne'KI-013-green-vehicle-shadow-nonblocking'){throw 'Owner visual review is missing.'}
$categoryProperties=@('road','buildings','player_vehicle','traffic','night_sky','shadows','particles','hud');if(($record.visual_review.categories.PSObject.Properties.Name-join',')-cne($categoryProperties-join',')){throw 'Visual category schema changed.'}
foreach($category in @('road','buildings','player_vehicle','traffic','night_sky','shadows','particles','hud')){if($record.visual_review.categories.$category-ne$true){throw "Visual category '$category' was not approved."}}
if(@((Get-Process mcla -ErrorAction SilentlyContinue)|Where-Object{try{[string]::Equals($_.Path,$exe,[StringComparison]::OrdinalIgnoreCase)}catch{$false}}).Count){throw 'Canonical MCLA process still exists.'}
[pscustomobject]@{Passed=$true;Decision=$record.decision;RoadVerified=$true;BuildingsVerified=$true;VehicleVerified=$true;TrafficVerified=$true;SkyVerified=$true;ShadowsVerified=$true;ParticlesVerified=$true;HudVerified=$true;OwnerVisualPass=$true;DataIntegrityVerified=$true}
