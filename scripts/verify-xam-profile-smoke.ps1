[CmdletBinding(DefaultParameterSetName = 'Result')]
param(
    [Parameter(Mandatory, ParameterSetName = 'Result')][string]$ResultPath,
    [Parameter(Mandatory, ParameterSetName = 'Probe')][string]$RuntimeLogPath,
    [Parameter(Mandatory, ParameterSetName = 'Probe')][string]$BmpPath,
    [Parameter(Mandatory, ParameterSetName = 'Probe')][switch]$ProbeOnly,
    [Parameter(ParameterSetName = 'Probe')][switch]$ProfileOnly
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$repoRoot = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '..')).Path
$titleVerifier = Join-Path $PSScriptRoot 'verify-render-path-smoke.ps1'
$gameVerifier = Join-Path $PSScriptRoot 'verify-game-manifest.ps1'
$canonicalGame = Join-Path $repoRoot 'private/game'
$canonicalBuild = Join-Path $repoRoot 'out/build/win-amd64-relwithdebinfo'
$artifactNames = @('mcla.exe','rexruntimerd.dll','TracyClientrd.dll','rexgpu-xenosrd.dll')
$utf8 = [Text.UTF8Encoding]::new($false)
$hashPattern = '^[0-9A-F]{64}$'
$requiredSettingIds = '1004000C,1004000D,1004000E'

function Assert-ExactProperties {
    param([object]$Value,[string[]]$Expected,[string]$Description)
    $actual=@($Value.PSObject.Properties.Name|Sort-Object);$wanted=@($Expected|Sort-Object)
    if($actual.Count-ne$wanted.Count){throw "$Description has missing or unknown properties."}
    for($i=0;$i-lt$wanted.Count;$i++){if($actual[$i]-cne$wanted[$i]){throw "$Description has missing or unknown properties."}}
}
function Assert-JsonTypes {
    param([object]$Value,[string[]]$Booleans=@(),[string[]]$Integers=@(),[string[]]$Strings=@(),[string]$Description)
    foreach($n in $Booleans){if($Value.PSObject.Properties[$n].Value-isnot[bool]){throw "$Description '$n' must be a JSON boolean."}}
    foreach($n in $Integers){$v=$Value.PSObject.Properties[$n].Value;if($v-isnot[int32]-and$v-isnot[int64]-and$v-isnot[uint32]-and$v-isnot[uint64]-and$v-isnot[int16]-and$v-isnot[uint16]-and$v-isnot[byte]-and$v-isnot[sbyte]){throw "$Description '$n' must be a JSON integer."}}
    foreach($n in $Strings){if($Value.PSObject.Properties[$n].Value-isnot[string]){throw "$Description '$n' must be a JSON string."}}
}
function Assert-ContainedNonReparsePath {
    param([string]$Path,[string]$Description)
    $full=[IO.Path]::GetFullPath($Path);$prefix=$repoRoot.TrimEnd('\')+'\'
    if(-not$full.StartsWith($prefix,[StringComparison]::OrdinalIgnoreCase)){throw "$Description must stay inside the repository."}
    $current=$repoRoot
    foreach($part in @($full.Substring($prefix.Length).Split('\')|Where-Object{$_.Length})){
        $current=Join-Path $current $part;if(-not(Test-Path -LiteralPath $current)){throw "$Description component is missing."}
        if((Get-Item $current -Force).Attributes-band[IO.FileAttributes]::ReparsePoint){throw "$Description traverses a reparse point."}
    };$full
}
function Assert-ExactChildren {
    param([string]$Root,[string[]]$Expected,[string]$Description)
    $a=@((Get-ChildItem $Root -Force|Sort-Object Name).Name);$w=@($Expected|Sort-Object)
    if($a.Count-ne$w.Count){throw "$Description has missing or extra children."}
    for($i=0;$i-lt$w.Count;$i++){if($a[$i]-cne$w[$i]){throw "$Description has incorrectly named children."}}
}
function Get-TreeSnapshot {
    param([string]$Root)
    $items=@(Get-ChildItem $Root -Recurse -Force);$entries=@();$files=@($items|Where-Object{-not$_.PSIsContainer}|Sort-Object FullName)
    foreach($i in @((Get-Item $Root -Force))+$items){if($i.Attributes-band[IO.FileAttributes]::ReparsePoint){throw 'Evidence or source tree contains a reparse point.'}}
    foreach($d in @($items|Where-Object PSIsContainer|Sort-Object FullName)){$entries+=[ordered]@{kind='directory';path=$d.FullName.Substring($Root.Length).TrimStart('\').Replace('\','/')}}
    foreach($f in $files){$entries+=[ordered]@{kind='file';path=$f.FullName.Substring($Root.Length).TrimStart('\').Replace('\','/');length=$f.Length;sha256=(Get-FileHash $f.FullName -Algorithm SHA256).Hash}}
    $json=ConvertTo-Json -InputObject @($entries) -Depth 4 -Compress;$sha=[Security.Cryptography.SHA256]::Create()
    try{$hash=-join($sha.ComputeHash($utf8.GetBytes($json))|ForEach-Object{$_.ToString('X2')})}finally{$sha.Dispose()}
    $bytes=0L;foreach($f in $files){$bytes+=[long]$f.Length}
    [pscustomobject]@{Hash=$hash;FileCount=$files.Count;DirectoryCount=@($items|Where-Object PSIsContainer).Count;Bytes=$bytes}
}
function Get-RuntimeLogSet {
    param([string]$CurrentLogPath)
    if((Split-Path -Leaf $CurrentLogPath)-cne'mcla.log'){throw 'Current runtime log must be named mcla.log.'}
    $files=@(Get-ChildItem (Split-Path -Parent $CurrentLogPath) -File -Force -Filter 'mcla*.log')
    if($files.Count-lt1-or$files.Count-gt16){throw 'Runtime log set must contain 1-16 files.'}
    $current=@($files|Where-Object Name -ceq 'mcla.log');if($current.Count-ne1){throw 'Runtime log set requires exactly one mcla.log.'}
    $rot=@();foreach($f in $files){if($f.Attributes-band[IO.FileAttributes]::ReparsePoint){throw 'Runtime log is a reparse point.'};if($f.Name-ceq'mcla.log'){continue};$m=[regex]::Match($f.Name,'^mcla\.([1-9][0-9]*)\.log$');if(-not$m.Success){throw 'Malformed runtime log rotation name.'};$rot+=[pscustomobject]@{N=[int]$m.Groups[1].Value;F=$f}}
    $ids=@($rot|ForEach-Object{$_.N}|Sort-Object);for($i=0;$i-lt$ids.Count;$i++){if($ids[$i]-ne$i+1-or$ids[$i]-gt15){throw 'Runtime log rotations are not contiguous.'}}
    $ordered=@($rot|Sort-Object N -Descending|ForEach-Object{$_.F})+$current;$manifest=@();$parts=@();$bytes=0L;$previous=[datetime]::MinValue
    foreach($f in $ordered){if($f.Length-gt8388608-or$f.LastWriteTimeUtc-lt$previous){throw 'Runtime log size or chronology failed.'};$previous=$f.LastWriteTimeUtc;$bytes+=$f.Length;if($bytes-gt100663296){throw 'Runtime logs exceed 96 MiB.'};$manifest+=[ordered]@{name=$f.Name;bytes=$f.Length;sha256=(Get-FileHash $f.FullName -Algorithm SHA256).Hash};$parts+=[IO.File]::ReadAllText($f.FullName)}
    $json=ConvertTo-Json -InputObject @($manifest) -Depth 3 -Compress;$sha=[Security.Cryptography.SHA256]::Create();try{$hash=-join($sha.ComputeHash($utf8.GetBytes($json))|ForEach-Object{$_.ToString('X2')})}finally{$sha.Dispose()}
    [pscustomobject]@{Files=@($manifest);Count=$manifest.Count;Bytes=$bytes;Hash=$hash;Text=($parts-join[Environment]::NewLine)}
}
function Get-GameIdentity {
    $root=Assert-ContainedNonReparsePath $canonicalGame 'Canonical game tree';$manifest=Assert-ContainedNonReparsePath (Join-Path $repoRoot 'private/game-manifest.json') 'Canonical game manifest';$tree=Get-TreeSnapshot $root;$v=&$gameVerifier -GamePath $root -ManifestPath $manifest -VerifyHashes
    [pscustomobject]@{file_count=$v.FileCount;payload_bytes=$v.PayloadBytes;hashes_verified=$v.HashesVerified;manifest_sha256=(Get-FileHash $manifest -Algorithm SHA256).Hash;tree_sha256=$tree.Hash;tree_file_count=$tree.FileCount;tree_directory_count=$tree.DirectoryCount;tree_bytes=$tree.Bytes}
}
function Get-Artifacts {@($artifactNames|ForEach-Object{$p=Assert-ContainedNonReparsePath (Join-Path $canonicalBuild $_) "Artifact $_";[pscustomobject]@{name=$_;sha256=(Get-FileHash $p -Algorithm SHA256).Hash}})}
function Get-ExactProcesses {$exe=[IO.Path]::GetFullPath((Join-Path $canonicalBuild 'mcla.exe'));@((Get-Process mcla -ErrorAction SilentlyContinue)|Where-Object{try{[string]::Equals($_.Path,$exe,[StringComparison]::OrdinalIgnoreCase)}catch{$false}})}
function Assert-StaticContract {
    $generated=Get-ChildItem (Join-Path $repoRoot 'generated/default') -Filter 'mcla_recomp.*.cpp' -File
    $expected=[ordered]@{XamUserGetSigninState=5;XamUserReadProfileSettings=4;XamUserCheckPrivilege=3;XamUserGetXUID=1;XamUserGetName=1;XamUserGetSigninInfo=1;XamShowSigninUI=1}
    foreach($name in $expected.Keys){$count=0;foreach($f in $generated){$count+=[regex]::Matches([IO.File]::ReadAllText($f.FullName),[regex]::Escape("__imp__$name(ctx, base)")).Count};if($count-ne$expected[$name]){throw "Generated import count for $name changed: expected $($expected[$name]), got $count."}}
    $sdk=[IO.File]::ReadAllText((Join-Path $repoRoot 'third_party/rexglue-sdk/src/kernel/xam/xam_user.cpp'))
    foreach($name in $expected.Keys){if($sdk-notmatch"REX_EXPORT\(__imp__$name,"){throw "$name is not a concrete SDK export."};if($sdk-match"REX_EXPORT_STUB\(__imp__$name\)"){throw "$name regressed to a generic stub."}}
    $profile=[IO.File]::ReadAllText((Join-Path $repoRoot 'third_party/rexglue-sdk/include/rex/system/xam/user_profile.h'))
    if($profile-notmatch'return 1;' -or $sdk-notmatch'if \(user_index == 0\)'){throw 'Static single-local-user contract is missing.'}
    $unit=[IO.File]::ReadAllText((Join-Path $repoRoot 'third_party/rexglue-sdk/tests/unit/kernel/xam_user_profile_test.cpp'))
    $unitCmake=[IO.File]::ReadAllText((Join-Path $repoRoot 'third_party/rexglue-sdk/tests/unit/CMakeLists.txt'))
    foreach($needle in @('Default XAM profile exposes one stable local signed-in user',
            'Default XAM profile supplies MCLA voice settings',
            'CHECK(profile.signin_state() == 1)', 'CHECK(profile.xuid() != 0)',
            'GetInt32Setting(profile, 0x1004000C).value == 0',
            'GetInt32Setting(profile, 0x1004000D).value == 0',
            'GetInt32Setting(profile, 0x1004000E).value == 100')) {
        if(-not$unit.Contains($needle)){throw "Focused profile unit contract is missing '$needle'."}
    }
    if(-not$unitCmake.Contains('kernel/xam_user_profile_test.cpp')){throw 'Focused profile unit test is not registered.'}
}
function Convert-Summary {
    param([System.Text.RegularExpressions.Match]$Match)
    $names=@('signin_state_calls','signin_state_slot0_local','signin_state_other','signin_state_other_mask','xuid_calls','xuid_success','xuid_nonzero','profile_read_calls','profile_read_size','profile_read_fill','profile_read_failure','privilege_calls','privilege_success','privilege_allowed','privilege_251_false','privilege_252_false','name_calls','name_match','signin_info_calls','signin_info_consistent','signin_info_absent','signin_info_absent_mask','signin_ui_calls','signin_ui_ordered','dropped_records')
    $o=[ordered]@{};foreach($n in $names){$o[$n]=if($n.EndsWith('_mask')){[Convert]::ToInt32($Match.Groups[$n].Value,16)}else{[int]$Match.Groups[$n].Value}};[pscustomobject]$o
}
function Get-ProfileProbe {
    param([string]$LogPath,[string]$FramePath,[switch]$SkipTitle)
    $logPathResolved=Assert-ContainedNonReparsePath $LogPath 'Runtime log';$frame=Assert-ContainedNonReparsePath $FramePath 'Capture BMP';$title=$null;if(-not$SkipTitle){$title=&$titleVerifier -ProbeOnly -RuntimeLogPath $logPathResolved -BmpPath $frame};$set=Get-RuntimeLogSet $logPathResolved;$log=$set.Text
    $launch=[regex]::Matches($log,[regex]::Escape('KernelState: Preparing module launch...'));if($launch.Count-ne1){throw 'Profile route requires exactly one module launch marker.'}
    $capture=[regex]::Matches($log,'(?m)^.*MCLA graphics: nontrivial guest frame captured .*$');if($capture.Count-ne1){throw 'Profile route requires exactly one title capture marker.'}
    $config=[regex]::Matches($log,'(?m)^.*XAM_PROFILE_AUDIT_CONFIG v=1 enabled=1 slot=0\r?$');if($config.Count-ne1-or$config[0].Index-le$launch[0].Index){throw 'Profile audit config is missing, duplicated, or before launch.'}
    $num='(?<VAL>[0-9]+)';$hex='(?<HEX>[0-9A-F]{8})'
    $patterns=[ordered]@{
      Signin='(?m)^.*XAM_PROFILE_AUDIT_SIGNIN_STATE v=1 class=(?<class>success|absent|failure) user=(?<user>[0-9]+) state=(?<state>[0-9]+)\r?$'
      Xuid='(?m)^.*XAM_PROFILE_AUDIT_XUID v=1 class=(?<class>success|failure) user=(?<user>[0-9]+) mask=(?<mask>[0-9]+) result=(?<result>[0-9A-F]{8}) nonzero=(?<nonzero>[01])\r?$'
      Read='(?m)^.*XAM_PROFILE_AUDIT_PROFILE_READ v=1 class=(?<class>size|fill|failure) title=(?<title>profile|other) user=(?<user>[0-9]+) xuid_count=(?<xuid_count>[0-9]+) setting_count=(?<setting_count>[0-9]+) setting_ids=(?<setting_ids>none|[0-9A-F]{8}(?:,[0-9A-F]{8})*) buffer_size_in=(?<buffer_size_in>[0-9]+) buffer_size_out=(?<buffer_size_out>[0-9]+) result=(?<result>[0-9A-F]{8}) overlapped=(?<overlapped>[01]) completed=(?<completed>[0-9A-F]{8})\r?$'
      Priv='(?m)^.*XAM_PROFILE_AUDIT_PRIVILEGE v=1 class=(?<class>success|failure) user=(?<user>[0-9]+) mask=(?<mask>[0-9]+) result=(?<result>[0-9A-F]{8}) allowed=(?<allowed>[01])\r?$'
      Name='(?m)^.*XAM_PROFILE_AUDIT_NAME v=1 class=(?<class>success|failure) user=(?<user>[0-9]+) result=(?<result>[0-9A-F]{8}) profile_match=(?<profile_match>[01])\r?$'
      Info='(?m)^.*XAM_PROFILE_AUDIT_SIGNIN_INFO v=1 class=(?<class>success|absent|failure) user=(?<user>[0-9]+) result=(?<result>[0-9A-F]{8}) state_match=(?<state_match>[01]) xuid_match=(?<xuid_match>[01]) name_match=(?<name_match>[01])\r?$'
      UI='(?m)^.*XAM_PROFILE_AUDIT_SIGNIN_UI v=1 class=success signin_changed_seq=(?<signin_changed_seq>[0-9]+) ui_off_seq=(?<ui_off_seq>[0-9]+) ordered=(?<ordered>[01]) result=00000000\r?$'
    }
    $records=@{};foreach($key in $patterns.Keys){$records[$key]=[regex]::Matches($log,$patterns[$key])}
    $summaryNames=@('signin_state_calls','signin_state_slot0_local','signin_state_other','signin_state_other_mask','xuid_calls','xuid_success','xuid_nonzero','profile_read_calls','profile_read_size','profile_read_fill','profile_read_failure','privilege_calls','privilege_success','privilege_allowed','privilege_251_false','privilege_252_false','name_calls','name_match','signin_info_calls','signin_info_consistent','signin_info_absent','signin_info_absent_mask','signin_ui_calls','signin_ui_ordered','dropped_records')
    $tail=($summaryNames|ForEach-Object{if($_.EndsWith('_mask')){"$_=(?<$_>[0-9A-F]+)"}else{"$_=(?<$_>[0-9]+)"}})-join' '
    $summaries=[regex]::Matches($log,"(?m)^.*XAM_PROFILE_AUDIT_SUMMARY v=1 phase=checkpoint status=(?<status>PASS|FAIL) $tail\r?$")
    if($summaries.Count-ne1-or$summaries[0].Index-le$config[0].Index-or$summaries[0].Index-ge$capture[0].Index){throw 'Exactly one checkpoint summary must follow config and precede title capture.'}
    if($summaries[0].Groups['status'].Value-cne'PASS'){throw 'Profile checkpoint summary is not PASS.'};$s=Convert-Summary $summaries[0]
    $allAudit=[regex]::Matches($log,'(?m)^.*XAM_PROFILE_AUDIT_[A-Z_]+.*$');$known=1+1;foreach($key in $records.Keys){$known+=$records[$key].Count};if($allAudit.Count-ne$known){throw 'Unknown or malformed XAM profile audit record exists.'}
    $signinLocal=@($records.Signin|Where-Object{$_.Groups['class'].Value-eq'success'-and[int]$_.Groups['user'].Value-eq0-and[int]$_.Groups['state'].Value-eq1})
    $signinAbsent=@($records.Signin|Where-Object{$_.Groups['class'].Value-eq'absent'-and[int]$_.Groups['user'].Value-ge1-and[int]$_.Groups['user'].Value-le3-and[int]$_.Groups['state'].Value-eq0})
    if($records.Signin.Count-ne4-or$signinLocal.Count-ne1-or$signinAbsent.Count-ne3-or@($signinAbsent|ForEach-Object{[int]$_.Groups['user'].Value}|Sort-Object -Unique).Count-ne3){throw 'Sign-in state records do not prove one local slot and distinct absent slots 1, 2, and 3.'}
    if(@($records.Xuid|Where-Object{$_.Groups['class'].Value-ne'success'-or$_.Groups['result'].Value-ne'00000000'-or[int]$_.Groups['user'].Value-ne0-or[int]$_.Groups['mask'].Value-ne7-or[int]$_.Groups['nonzero'].Value-ne1}).Count){throw 'XUID record violates the exact slot-0 mask-7 contract.'}
    $size=@($records.Read|Where-Object{$_.Groups['class'].Value-eq'size'});$fill=@($records.Read|Where-Object{$_.Groups['class'].Value-eq'fill'})
    if($records.Read.Count-gt0-and($size.Count-lt1-or$fill.Count-lt1-or@($records.Read|Where-Object{$_.Groups['class'].Value-eq'failure'}).Count)){throw 'Reached profile route lacks the exact size/fill pair or contains failure.'}
    foreach($m in $size){if($m.Groups['title'].Value-cne'profile'-or[int]$m.Groups['user'].Value-ne255-or[int]$m.Groups['xuid_count'].Value-ne0-or[int]$m.Groups['setting_count'].Value-ne3-or$m.Groups['setting_ids'].Value-cne$requiredSettingIds-or[int]$m.Groups['buffer_size_in'].Value-ne0-or[int]$m.Groups['buffer_size_out'].Value-ne128-or$m.Groups['result'].Value-cne'0000007A'-or[int]$m.Groups['overlapped'].Value-ne0-or$m.Groups['completed'].Value-cne'0000007A'){throw 'Profile size record differs from the exact three-setting contract.'}}
    foreach($m in $fill){$sync=$m.Groups['result'].Value-ceq'00000000'-and[int]$m.Groups['overlapped'].Value-eq0-and$m.Groups['completed'].Value-ceq'00000000';$async=$m.Groups['result'].Value-ceq'000003E5'-and[int]$m.Groups['overlapped'].Value-eq1-and$m.Groups['completed'].Value-ceq'00000000';if($m.Groups['title'].Value-cne'profile'-or[int]$m.Groups['user'].Value-ne0-or[int]$m.Groups['xuid_count'].Value-ne0-or[int]$m.Groups['setting_count'].Value-ne3-or$m.Groups['setting_ids'].Value-cne$requiredSettingIds-or[int]$m.Groups['buffer_size_in'].Value-ne128-or[int]$m.Groups['buffer_size_out'].Value-ne128-or(-not$sync-and-not$async)){throw 'Profile fill record differs from the exact three-setting contract.'}}
    if(@($records.Priv|Where-Object{$_.Groups['class'].Value-ne'success'-or$_.Groups['result'].Value-ne'00000000'-or[int]$_.Groups['user'].Value-ne0-or[int]$_.Groups['allowed'].Value-ne0-or[int]$_.Groups['mask'].Value-notin@(251,252)}).Count){throw 'Privilege record contains an unexpected mask, failure, or allow.'}
    if(@($records.Name|Where-Object{$_.Groups['class'].Value-ne'success'-or$_.Groups['result'].Value-ne'00000000'-or[int]$_.Groups['user'].Value-ne0-or[int]$_.Groups['profile_match'].Value-ne1}).Count){throw 'Optional name identity record is inconsistent.'}
    $infoSuccess=@($records.Info|Where-Object{$_.Groups['class'].Value-eq'success'})
    $infoAbsent=@($records.Info|Where-Object{$_.Groups['class'].Value-eq'absent'})
    if(@($records.Info|Where-Object{$_.Groups['class'].Value-eq'failure'}).Count-or
       @($infoSuccess|Where-Object{$_.Groups['result'].Value-ne'00000000'-or[int]$_.Groups['user'].Value-ne0-or[int]$_.Groups['state_match'].Value-ne1-or[int]$_.Groups['xuid_match'].Value-ne1-or[int]$_.Groups['name_match'].Value-ne1}).Count-or
       @($infoAbsent|Where-Object{$_.Groups['result'].Value-ne'80070525'-or[int]$_.Groups['user'].Value-lt1-or[int]$_.Groups['user'].Value-gt3-or[int]$_.Groups['state_match'].Value-ne0-or[int]$_.Groups['xuid_match'].Value-ne0-or[int]$_.Groups['name_match'].Value-ne0}).Count){throw 'Signin-info record is neither consistent slot 0 nor expected absent slot 1-3.'}
    if($records.Xuid.Count-lt1-or$records.Name.Count-lt1-or$infoSuccess.Count-lt1-or$infoAbsent.Count-ne3-or@($infoAbsent|ForEach-Object{[int]$_.Groups['user'].Value}|Sort-Object -Unique).Count-ne3){throw 'Calibrated runtime route lacks XUID/name/consistent-info or distinct absent-info records for slots 1, 2, and 3.'}
    if(@($records.UI|Where-Object{[int]$_.Groups['ordered'].Value-ne1-or[int]$_.Groups['signin_changed_seq'].Value-ge[int]$_.Groups['ui_off_seq'].Value}).Count){throw 'Optional SigninUI notification pair is not ordered.'}
    foreach($recordSet in $records.Values){if($recordSet.Count-gt128){throw 'Bounded profile audit record limit was exceeded.'}}
    if($s.signin_state_calls-lt$records.Signin.Count-or$s.signin_state_calls-ne($s.signin_state_slot0_local+$s.signin_state_other)-or
       $s.xuid_calls-lt$records.Xuid.Count-or$s.xuid_success-ne$s.xuid_calls-or$s.xuid_nonzero-ne$s.xuid_success-or
       $s.profile_read_calls-lt$records.Read.Count-or$s.profile_read_calls-ne($s.profile_read_size+$s.profile_read_fill+$s.profile_read_failure)-or
       $s.privilege_calls-lt$records.Priv.Count-or$s.privilege_success-ne$s.privilege_calls-or$s.privilege_allowed-ne0-or
       $s.name_calls-lt$records.Name.Count-or$s.name_match-ne$s.name_calls-or
       $s.signin_info_calls-lt$records.Info.Count-or$s.signin_info_calls-ne($s.signin_info_consistent+$s.signin_info_absent)-or
       $s.signin_ui_calls-lt$records.UI.Count-or$s.signin_ui_ordered-ne$s.signin_ui_calls-or$s.dropped_records-ne0){throw 'Summary counters are internally inconsistent with bounded physical records.'}
    if($infoSuccess.Count-gt$s.signin_info_consistent-or$infoAbsent.Count-gt$s.signin_info_absent){throw 'Signin-info summary is smaller than its bounded records.'}
    if($size.Count-gt$s.profile_read_size-or$fill.Count-gt$s.profile_read_fill-or
       @($records.Priv|Where-Object{[int]$_.Groups['mask'].Value-eq251}).Count-gt$s.privilege_251_false-or
       @($records.Priv|Where-Object{[int]$_.Groups['mask'].Value-eq252}).Count-gt$s.privilege_252_false){throw 'Optional profile or privilege summary is smaller than its bounded records.'}
    $profileOptional=($s.profile_read_calls-eq0-and$s.profile_read_size-eq0-and$s.profile_read_fill-eq0-and$s.profile_read_failure-eq0)-or($s.profile_read_size-ge1-and$s.profile_read_fill-ge1-and$s.profile_read_failure-eq0)
    $privilegeOptional=($s.privilege_calls-eq0-and$s.privilege_251_false-eq0-and$s.privilege_252_false-eq0)-or($s.privilege_251_false-ge1-and$s.privilege_252_false-ge1)
    if(($s.profile_read_calls-gt0-and($size.Count-lt1-or$fill.Count-lt1))-or($s.privilege_calls-gt0-and(@($records.Priv|Where-Object{[int]$_.Groups['mask'].Value-eq251}).Count-lt1-or@($records.Priv|Where-Object{[int]$_.Groups['mask'].Value-eq252}).Count-lt1))){throw 'Reached optional profile or privilege route lacks its bounded exact markers.'}
    if($s.signin_state_slot0_local-lt1-or$s.signin_state_other-lt3-or$s.signin_state_other_mask-ne0xE-or$s.xuid_success-lt1-or$s.name_match-lt1-or$s.signin_info_consistent-lt1-or$s.signin_info_absent-lt3-or$s.signin_info_absent_mask-ne0xE-or-not$profileOptional-or-not$privilegeOptional){throw 'Checkpoint summary lacks the calibrated per-slot sign-in/profile contract.'}
    if($log-match'(?im)^.*(?:REX_EXPORT_STUB.*(?:XamUser|XamShowSignin)|(?:XamUser|XamShowSignin).*unimplemented).*$'){throw 'Runtime contains a profile generic-stub or unimplemented marker.'}
    [pscustomobject]@{LogSet=$set;Title=$title;Summary=$s;ConfigCount=1;SummaryCount=1;SigninRecords=$records.Signin.Count;XuidRecords=$records.Xuid.Count;ReadRecords=$records.Read.Count;PrivilegeRecords=$records.Priv.Count;NameRecords=$records.Name.Count;InfoRecords=$records.Info.Count;UiRecords=$records.UI.Count}
}

if(-not($PSCmdlet.ParameterSetName-eq'Probe'-and$ProfileOnly)){Assert-StaticContract}
if($PSCmdlet.ParameterSetName-eq'Probe'){if(-not$ProbeOnly){throw 'Probe inputs require -ProbeOnly.'};Get-ProfileProbe $RuntimeLogPath $BmpPath -SkipTitle:$ProfileOnly;return}
$resolved=Assert-ContainedNonReparsePath $ResultPath 'Result path';if((Split-Path -Leaf $resolved)-cne'result.json'){throw 'Result must be result.json.'};$json=[IO.File]::ReadAllText($resolved);if($json.Length-gt1048576){throw 'Result exceeds 1 MiB.'};foreach($p in @('(?i)[A-Z]:[\\/]','(?i)\\\\[^"\s]+[\\/]','(?i)(?:^|["\\/])private[\\/]')){if($json-match$p){throw 'Sanitized result contains a private or absolute path.'}};$r=$json|ConvertFrom-Json
$top=@('schema','task','decision','cycle_count','capture_timeout_seconds','first_frame_settle_seconds','post_marker_dwell_milliseconds','exit_timeout_seconds','clean_build','game_identity','artifacts','cycles','all_write_roots_contained','all_prior_cycles_immutable','no_surviving_processes','data_integrity_preserved','all_title_probes_passed','all_profile_probes_passed')
Assert-ExactProperties $r $top 'M4-004 result';Assert-JsonTypes $r -Booleans @('all_write_roots_contained','all_prior_cycles_immutable','no_surviving_processes','data_integrity_preserved','all_title_probes_passed','all_profile_probes_passed') -Integers @('schema','cycle_count','capture_timeout_seconds','first_frame_settle_seconds','post_marker_dwell_milliseconds','exit_timeout_seconds') -Strings @('task','decision') -Description 'M4-004 result'
if($r.schema-ne1-or$r.task-cne'M4-004'-or$r.decision-cne'single-local-offline-profile'-or$r.cycle_count-ne3-or$r.capture_timeout_seconds-ne60-or$r.first_frame_settle_seconds-ne35-or$r.post_marker_dwell_milliseconds-ne2000-or$r.exit_timeout_seconds-ne10){throw 'M4-004 header is noncanonical.'}
Assert-ExactProperties $r.clean_build @('sdk_install_performed','sdk_install_success','sdk_install_exit_code','sdk_install_duration_milliseconds','sdk_install_log_sha256','sdk_profile_test_performed','sdk_profile_test_success','sdk_profile_test_exit_code','sdk_profile_test_duration_milliseconds','sdk_profile_test_cases','sdk_profile_test_assertions','sdk_profile_test_log_sha256','sdk_profile_test_executable_sha256','app_clean_build_performed','app_clean_build_success','app_clean_build_exit_code','app_clean_build_duration_milliseconds','app_clean_build_log_sha256','executable_sha256') 'Clean build';Assert-JsonTypes $r.clean_build -Booleans @('sdk_install_performed','sdk_install_success','sdk_profile_test_performed','sdk_profile_test_success','app_clean_build_performed','app_clean_build_success') -Integers @('sdk_install_exit_code','sdk_install_duration_milliseconds','sdk_profile_test_exit_code','sdk_profile_test_duration_milliseconds','sdk_profile_test_cases','sdk_profile_test_assertions','app_clean_build_exit_code','app_clean_build_duration_milliseconds') -Strings @('sdk_install_log_sha256','sdk_profile_test_log_sha256','sdk_profile_test_executable_sha256','app_clean_build_log_sha256','executable_sha256') -Description 'Clean build'
if(-not$r.clean_build.sdk_install_performed-or-not$r.clean_build.sdk_install_success-or$r.clean_build.sdk_install_exit_code-ne0-or-not$r.clean_build.sdk_profile_test_performed-or-not$r.clean_build.sdk_profile_test_success-or$r.clean_build.sdk_profile_test_exit_code-ne0-or$r.clean_build.sdk_profile_test_cases-ne2-or$r.clean_build.sdk_profile_test_assertions-ne15-or-not$r.clean_build.app_clean_build_performed-or-not$r.clean_build.app_clean_build_success-or$r.clean_build.app_clean_build_exit_code-ne0){throw 'SDK install, focused profile tests, and clean app build must pass.'};foreach($n in 'sdk_install_log_sha256','sdk_profile_test_log_sha256','sdk_profile_test_executable_sha256','app_clean_build_log_sha256','executable_sha256'){if($r.clean_build.$n-notmatch$hashPattern){throw "Invalid clean-build hash '$n'."}}
$identityFields=@('file_count','payload_bytes','hashes_verified','manifest_sha256','tree_sha256','tree_file_count','tree_directory_count','tree_bytes');Assert-ExactProperties $r.game_identity @('before','after') 'Game identity'
foreach($phase in 'before','after'){Assert-ExactProperties $r.game_identity.$phase $identityFields "Game identity $phase";if($r.game_identity.$phase.hashes_verified-ne$r.game_identity.$phase.file_count-or$r.game_identity.$phase.manifest_sha256-notmatch$hashPattern-or$r.game_identity.$phase.tree_sha256-notmatch$hashPattern){throw 'Game identity is invalid.'}}
foreach($f in $identityFields){if($r.game_identity.before.$f-ne$r.game_identity.after.$f){throw "Game identity '$f' drifted."}}
Assert-ExactProperties $r.artifacts @('before','after') 'Artifacts';foreach($phase in 'before','after'){$a=@($r.artifacts.$phase);if($a.Count-ne4){throw 'Four artifacts required.'};for($i=0;$i-lt4;$i++){Assert-ExactProperties $a[$i] @('name','sha256') 'Artifact';if($a[$i].name-cne$artifactNames[$i]-or$a[$i].sha256-notmatch$hashPattern-or$a[$i].sha256-ne$r.artifacts.before[$i].sha256){throw 'Artifact identity drifted.'}}}
$runRoot=Split-Path -Parent $resolved;$expected=[IO.Path]::GetFullPath((Join-Path $repoRoot 'private/evidence/M4-004'));if(-not[string]::Equals((Split-Path -Parent $runRoot),$expected,[StringComparison]::OrdinalIgnoreCase)){throw 'Run root must be an immediate M4-004 evidence child.'};Assert-ExactChildren $runRoot @('result.json','runs','sdk-install.log','sdk-profile-test.log','relwithdebinfo-clean-build.log') 'Run root';$runs=Join-Path $runRoot 'runs';Assert-ExactChildren $runs @('01','02','03') 'Runs root'
$cycleFields=@('index','capture_elapsed_milliseconds','dwell_elapsed_milliseconds','exit_elapsed_milliseconds','exit_code','close_requested','harness_force_cleanup','process_signal_confirmed','process_cleanup_confirmed','prior_cycles_immutable','runtime_logs','runtime_log_file_count','runtime_log_bytes','runtime_log_set_sha256','capture_relative_path','capture_sha256','capture_bytes','logo_edge_correlation_ppm','press_edge_correlation_ppm','resolve_calls','draw_issued','profile_config_count','profile_summary_count','signin_records','xuid_records','profile_read_records','privilege_records','name_records','signin_info_records','signin_ui_records','user_tree_sha256','cache_tree_sha256','cycle_tree_sha256','user_file_count','cache_file_count','user_bytes','cache_bytes')
$cycles=@($r.cycles);if($cycles.Count-ne3){throw 'Exactly three cycles required.'}
for($i=0;$i-lt3;$i++){$n='{0:D2}'-f($i+1);$c=$cycles[$i];Assert-ExactProperties $c $cycleFields "Cycle $n";if($c.index-ne$i+1-or$c.capture_elapsed_milliseconds-lt0-or$c.capture_elapsed_milliseconds-gt60000-or$c.dwell_elapsed_milliseconds-lt2000-or$c.exit_elapsed_milliseconds-lt0-or$c.exit_elapsed_milliseconds-gt10000-or$c.exit_code-ne0-or-not$c.close_requested-or$c.harness_force_cleanup-or-not$c.process_signal_confirmed-or-not$c.process_cleanup_confirmed-or-not$c.prior_cycles_immutable){throw "Cycle $n process contract failed."};$logs=@($c.runtime_logs);if($logs.Count-lt1-or$logs.Count-gt16-or$c.runtime_log_file_count-ne$logs.Count){throw "Cycle $n log manifest cardinality failed."};foreach($l in $logs){Assert-ExactProperties $l @('name','bytes','sha256') 'Runtime log';if($l.sha256-notmatch$hashPattern){throw 'Runtime log hash invalid.'}};$root=Join-Path $runs $n;Assert-ExactChildren $root (@('cache','user')+@($logs.name)) "Cycle $n root";$bmp=Join-Path $root 'user/mcla-first-frame.bmp';if($c.capture_relative_path-cne"runs/$n/user/mcla-first-frame.bmp"-or@(Get-ChildItem $root -Recurse -File -Filter '*.bmp').Count-ne1){throw 'Capture topology is noncanonical.'};$p=Get-ProfileProbe (Join-Path $root 'mcla.log') $bmp;if($c.runtime_log_file_count-ne$p.LogSet.Count-or$c.runtime_log_bytes-ne$p.LogSet.Bytes-or$c.runtime_log_set_sha256-ne$p.LogSet.Hash-or$c.capture_sha256-ne$p.Title.Bmp.Sha256-or$c.capture_bytes-ne$p.Title.Bmp.Bytes-or$c.logo_edge_correlation_ppm-ne$p.Title.Roi.LogoCorrelationPpm-or$c.press_edge_correlation_ppm-ne$p.Title.Roi.PressCorrelationPpm-or$c.resolve_calls-ne$p.Title.Audit.ResolveCalls-or$c.draw_issued-ne$p.Title.Audit.DrawIssued-or$c.profile_config_count-ne1-or$c.profile_summary_count-ne1-or$c.signin_records-ne$p.SigninRecords-or$c.xuid_records-ne$p.XuidRecords-or$c.profile_read_records-ne$p.ReadRecords-or$c.privilege_records-ne$p.PrivilegeRecords-or$c.name_records-ne$p.NameRecords-or$c.signin_info_records-ne$p.InfoRecords-or$c.signin_ui_records-ne$p.UiRecords){throw "Cycle $n aggregate does not match physical evidence."};for($j=0;$j-lt$logs.Count;$j++){foreach($f in 'name','bytes','sha256'){if($logs[$j].$f-ne$p.LogSet.Files[$j].$f){throw 'Runtime log manifest mismatch.'}}};$u=Get-TreeSnapshot(Join-Path $root 'user');$k=Get-TreeSnapshot(Join-Path $root 'cache');$t=Get-TreeSnapshot $root;if($c.user_tree_sha256-ne$u.Hash-or$c.cache_tree_sha256-ne$k.Hash-or$c.cycle_tree_sha256-ne$t.Hash-or$c.user_file_count-ne$u.FileCount-or$c.cache_file_count-ne$k.FileCount-or$c.user_bytes-ne$u.Bytes-or$c.cache_bytes-ne$k.Bytes){throw 'Cycle tree aggregate mismatch.'}}
foreach($f in 'all_write_roots_contained','all_prior_cycles_immutable','no_surviving_processes','data_integrity_preserved','all_title_probes_passed','all_profile_probes_passed'){if(-not$r.$f){throw "Aggregate '$f' is false."}}
$sdkLog=Assert-ContainedNonReparsePath (Join-Path $runRoot 'sdk-install.log') 'SDK install log';$profileTestLog=Assert-ContainedNonReparsePath (Join-Path $runRoot 'sdk-profile-test.log') 'SDK profile test log';$appBuildLog=Assert-ContainedNonReparsePath (Join-Path $runRoot 'relwithdebinfo-clean-build.log') 'App clean-build log';$unitExe=Assert-ContainedNonReparsePath (Join-Path $repoRoot 'third_party/rexglue-sdk/out/win-amd64/RelWithDebInfo/unit_tests.exe') 'SDK profile unit test executable'
if((Get-FileHash $sdkLog -Algorithm SHA256).Hash-ne$r.clean_build.sdk_install_log_sha256-or(Get-FileHash $profileTestLog -Algorithm SHA256).Hash-ne$r.clean_build.sdk_profile_test_log_sha256-or(Get-FileHash $appBuildLog -Algorithm SHA256).Hash-ne$r.clean_build.app_clean_build_log_sha256-or(Get-FileHash $unitExe -Algorithm SHA256).Hash-ne$r.clean_build.sdk_profile_test_executable_sha256-or[IO.File]::ReadAllText($profileTestLog)-notmatch'All tests passed \(15 assertions in 2 test cases\)'){throw 'Clean build or focused profile unit-test evidence is not physically bound.'}
$physicalGame=Get-GameIdentity;foreach($f in $identityFields){if($r.game_identity.after.$f-ne$physicalGame.$f){throw "Physical game identity '$f' differs."}};$physicalArtifacts=Get-Artifacts;for($i=0;$i-lt4;$i++){if($r.artifacts.after[$i].sha256-ne$physicalArtifacts[$i].sha256){throw 'Physical artifact differs.'}};if($r.clean_build.executable_sha256-ne$physicalArtifacts[0].sha256){throw 'Clean-build executable hash is not current.'};if(@(Get-ExactProcesses).Count){throw 'Exact MCLA process survived.'}
[pscustomobject]@{Passed=$true;Decision='single-local-offline-profile';Cycles=3;PhysicalTitleProbes=3;ProfileContractProbes=3;DataIntegrityVerified=$true}
