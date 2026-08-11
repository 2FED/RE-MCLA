[CmdletBinding()]
param([string]$RawRoot,[string]$OutputPath)

Set-StrictMode -Version Latest
$ErrorActionPreference='Stop'
$repoRoot=(Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '..')).Path
$evidenceRoot=Join-Path $repoRoot 'docs/evidence'
$generatedRoot=Join-Path $repoRoot 'private/evidence/M2-016/pre-release-tag/generated'
if(-not $RawRoot){$RawRoot=Join-Path $repoRoot 'private/evidence/M2-012'}
if(-not $OutputPath){$OutputPath=Join-Path $evidenceRoot 'M2-012-manual-analysis-config.md'}
$resolvedOutput=[System.IO.Path]::GetFullPath($OutputPath)
$safeRoot=[System.IO.Path]::GetFullPath($evidenceRoot).TrimEnd('\')
if(-not $resolvedOutput.StartsWith("$safeRoot\",[System.StringComparison]::OrdinalIgnoreCase)-or [System.IO.Path]::GetExtension($resolvedOutput)-ne '.md'){throw 'Output must be Markdown below docs/evidence.'}

$expected=@(
    [pscustomobject]@{Name='01-cf02-chunk';Exit=1;Count=6;Entries=1;Config='606C782BD37C3338DFD4E6301CD0200C3C528281FEB91B39FA75C87192F63DC8';Stderr='165C0EB7FF4CB5CEB01F400EE3C1FF3E68ADD47D9081A7C99DBC20647ABEDD49';Note='CF-02 chunk accepted'},
    [pscustomobject]@{Name='02-cf01-tail';Exit=1;Count=5;Entries=2;Config='9A9F949C9DA0FCF319779C58625516152451D5EC4246229F498F47979D571BF7';Stderr='BADB3671FC6E8CB6149C3A60C2B2663A05EE6AE1C01F291D4592CF5C127AA65D';Note='CF-01 accepted'},
    [pscustomobject]@{Name='03-cf03-dispatch';Exit=1;Count=4;Entries=3;Config='CDE5A0F77600EF65AFDFA15DEE4B68BC7C9152D86020F34CBA418A911248B657';Stderr='5EA1E3FA4466A40CEA186385E3EC827A6AE541F2CDDFF351A33B123010D4DC14';Note='CF-03 accepted'},
    [pscustomobject]@{Name='04-cf04-dispatch';Exit=1;Count=3;Entries=4;Config='461CD1C859047E2E26AAE144C7FF4DA7E1AB7D9F5958B734627E2DBE04ED3462';Stderr='7C83A4DDB23EC52589A34E3FAE232450EF40262FDDD680B7E8B071AF1F506EEF';Note='CF-04 accepted'},
    [pscustomobject]@{Name='05-cf05-dispatch';Exit=1;Count=3;Entries=5;Config='F7C8343D3606E5836BCF59504D0A000C99130D3CBC43E9E66F2A56CA23EA91D4';Stderr='AE7FB9BAB27A6474848CE997EF03C56C169CE3BF8A35823A84B9984F1EB4CE10';Note='CF-05 accepted; exposed 0x822C9948'},
    [pscustomobject]@{Name='06-cf05-followup';Exit=1;Count=2;Entries=6;Config='01DD49501E1F8A48E98B3F8E6E6D2E4F37B18C93AED340CA9296B26C29A63852';Stderr='C86312D46968B884164254F0624E9713207189912D17367B1C65FCEC505434CF';Note='follow-up accepted'},
    [pscustomobject]@{Name='07-cf06-leaf';Exit=1;Count=1;Entries=7;Config='6ED3162FC0BC2E5417C7EA2B69A367CABBA21EED00361A121389D5FD352CF21B';Stderr='C1ACCDA1C5F6755915970DF501A66987950646EE19AD902E259C86E2AE9EDA84';Note='CF-06 accepted'},
    [pscustomobject]@{Name='08-cf07-tail-final';Exit=0;Count=0;Entries=8;Config='FEB14690C0795A6748AD76E751B98D58435A5373847E8BE32D5A54CB6CE53FFF';Stderr='E4EDEE04351A483FF11BFD93540F69F29AE1AF7DF19A05C534BA7BD2C644E53B';Note='first success'},
    [pscustomobject]@{Name='09-final-clean-a';Exit=0;Count=0;Entries=8;Config='FEB14690C0795A6748AD76E751B98D58435A5373847E8BE32D5A54CB6CE53FFF';Stderr='81C3D18309BB6B7F841BB1AC93CA7063A0FEF87E2E71B701EA32E8B93B7E0492';Note='clean determinism A'},
    [pscustomobject]@{Name='10-final-clean-b';Exit=0;Count=0;Entries=8;Config='FEB14690C0795A6748AD76E751B98D58435A5373847E8BE32D5A54CB6CE53FFF';Stderr='A90277AA4D1CF3DE672F1F4F091AAC0AFA146D6C97F2726B36F288ED3460737A';Note='clean determinism B'}
)
$rows=@()
foreach($item in $expected){
    $dir=Join-Path $RawRoot $item.Name
    foreach($file in @('run.json','stdout.log','stderr.log','mcla_manifest.toml','mcla_functions.toml')){if(-not(Test-Path -LiteralPath (Join-Path $dir $file)-PathType Leaf)){throw "Missing $($item.Name)/$file."}}
    $meta=Get-Content (Join-Path $dir 'run.json') -Raw|ConvertFrom-Json
    $stderrPath=Join-Path $dir 'stderr.log';$stdoutPath=Join-Path $dir 'stdout.log';$configPath=Join-Path $dir 'mcla_functions.toml';$manifestPath=Join-Path $dir 'mcla_manifest.toml'
    $configHash=(Get-FileHash $configPath -Algorithm SHA256).Hash;$stderrHash=(Get-FileHash $stderrPath -Algorithm SHA256).Hash
    if($meta.schema-ne 1-or $meta.task-ne'M2-012'-or $meta.iteration-ne$item.Name-or $meta.command-ne'rexglue codegen mcla_manifest.toml'-or $meta.exit_code-ne$item.Exit-or $meta.unresolved_count-ne$item.Count-or $configHash-ne$item.Config-or $stderrHash-ne$item.Stderr-or $meta.config_sha256-ne$configHash-or $meta.stderr_sha256-ne$stderrHash-or $meta.manifest_sha256-ne(Get-FileHash $manifestPath -Algorithm SHA256).Hash-or $meta.stdout_sha256-ne(Get-FileHash $stdoutPath -Algorithm SHA256).Hash){throw "Iteration identity mismatch: $($item.Name)."}
    if((Get-Item $stdoutPath).Length-ne 0){throw "Unexpected stdout in $($item.Name)."}
    $entryCount=@(Get-Content $configPath|Where-Object{$_ -match '^"0x[0-9A-F]{8}"'}).Count
    $detailCount=@(Get-Content $stderrPath|Where-Object{$_ -match '^  0x[0-9A-F]{8} from '}).Count
    if($entryCount-ne$item.Entries-or $detailCount-ne$item.Count){throw "Iteration counts changed: $($item.Name)."}
    if ($item.Exit -eq 0) {
        $packWarnings = @(Get-Content $stderrPath | Where-Object { $_ -match '^Unexpected float16_4 pack instruction at ' })
        $unexpectedErrors = @(Get-Content $stderrPath | Where-Object { $_ -match 'ANALYSIS ERRORS|UnresolvedCall' })
        if ($packWarnings.Count -ne 20 -or $unexpectedErrors.Count -ne 0) {
            throw "Successful iteration classification changed: $($item.Name)."
        }
    }
    $rows+="| ``$($item.Name)`` | $($item.Entries) | $($item.Exit) | $($item.Count) | $($item.Note) | ``$($stderrHash.Substring(0,12))...`` |"
}
$newFinding=Get-Content (Join-Path $RawRoot '05-cf05-dispatch/stderr.log')|Where-Object{$_ -ceq '  0x822C9948 from 0x822C9E14: b 0x822C9948 from 0x822C9E14 - target not in any function'}
if(@($newFinding).Count-ne1){throw 'CF-05 follow-up finding is missing.'}

$manifestA=Join-Path $RawRoot '09-final-clean-a/generated-manifest.json';$manifestB=Join-Path $RawRoot '10-final-clean-b/generated-manifest.json'
foreach($path in @($manifestA,$manifestB)){if(-not(Test-Path $path -PathType Leaf)){throw "Missing final generated manifest: $path"}}
$hashA=(Get-FileHash $manifestA -Algorithm SHA256).Hash;$hashB=(Get-FileHash $manifestB -Algorithm SHA256).Hash
if($hashA-ne'F8A97618215848CA3B2279725DE77D8AF556E6B2E02AF1CA66D51A812E60F933'-or$hashA-ne$hashB){throw 'Clean generated manifests are not identical.'}
$generatedManifest=Get-Content $manifestB -Raw|ConvertFrom-Json
if($generatedManifest.file_count-ne64-or$generatedManifest.total_bytes-ne128031984){throw 'Final generated aggregate changed.'}
foreach($entry in @($generatedManifest.files)){$path=Join-Path $generatedRoot $entry.path;if(-not(Test-Path $path -PathType Leaf)-or(Get-Item $path).Length-ne$entry.bytes-or(Get-FileHash $path -Algorithm SHA256).Hash-ne$entry.sha256){throw "Current generated output mismatch: $($entry.path)."}}

$currentConfig=(Get-FileHash (Join-Path $RawRoot '10-final-clean-b/mcla_functions.toml') -Algorithm SHA256).Hash
$currentManifest=(Get-FileHash (Join-Path $RawRoot '10-final-clean-b/mcla_manifest.toml') -Algorithm SHA256).Hash
if($currentConfig-ne'FEB14690C0795A6748AD76E751B98D58435A5373847E8BE32D5A54CB6CE53FFF'-or$currentManifest-ne'3ED7976DCC75085BB235CBA1406F1110C6DF78B7FA6525AD03FBF62D44B3AE90'){throw 'Current manifest/config differs from successful clean inputs.'}
& (Join-Path $PSScriptRoot 'verify-rexglue-manifest.ps1')|Out-Null

$configRows=@(
'| `0x8220BF08` | `0x8220C018` | standalone | computed-dispatch entry; terminates at `bctr` |',
'| `0x8220C018` | `0x8220C0D0` | standalone | second entry in the shared dispatch gap |',
'| `0x822B88C8` | `0x822B88DC` | standalone | bounded tail-branch thunk |',
'| `0x822C98B8` | `0x822C9948` | standalone | first dispatch entry; exact end exposed the next entry |',
'| `0x822C9948` | `0x822C9A2C` | standalone | follow-up dispatch entry verified privately to `bctr` |',
'| `0x823F32E8` | `0x823F3300` | standalone | bounded leaf ending in `blr` |',
'| `0x823FD718` | `0x823FD720` | standalone | two-instruction tail thunk |',
'| `0x824B0DE8` | `0x824B0DF8` | chunk of `0x824B0CC0` | begins exactly at parent PDATA end; chunk hypothesis passed |'
)
$report=@('# M2-012 manual analysis configuration','', 'Date: 2026-08-11','Result: NON-FORCE CODEGEN PASSES WITH DETERMINISTIC OUTPUT','',
'## Final input identity','',"- Manifest SHA-256: ``$currentManifest``","- Reviewed function config SHA-256: ``$currentConfig``","- Clean generated-manifest SHA-256: ``$hashA``",'- ReXGlue: pinned v0.9.0; all nine optimization/exception-generation flags remain false','',
'## Incremental evidence','', '| Iteration | Function entries | Exit | Unresolved | Outcome | Private stderr SHA prefix |','| --- | ---: | ---: | ---: | --- | --- |')+$rows+@('',
'Iteration 05 is intentionally retained as a failed prediction: adding `0x822C98B8` removed its original blocker but exposed the adjacent entry `0x822C9948`. A separate private Ghidra window proved that entry reaches `bctr` at `0x822C9A28`; iteration 06 added the bounded follow-up and restored the monotonic path to zero.','',
'## Accepted manual functions','', '| Address | Exclusive end | Relation | Address rationale |','| --- | --- | --- | --- |')+$configRows+@('',
'## Explicit zero override classes','',
'- Manual switch tables: 0 - M2-009 reported no missing jump table and the three `bctr` bodies are callable dispatch helpers, not unresolved switch sites.','- Invalid-instruction/data regions: 0 - M2-009 reported no invalid-region or unimplemented-instruction finding.','- Exception-handler hints: 0 - all 34 exception-marked PDATA records parsed without diagnostics.','- setjmp/longjmp overrides: 0 - M2-011 found no matching jump-buffer implementation.','',
'## Determinism and gate result','',
'Two runs started with no `generated/default` directory, used the exact same final manifest/config, exited 0, emitted the same 64 files / 128,031,984 bytes, and produced byte-identical generated manifests. Both completed Register, Scan, Discover, GapFill, Merge, Validate, and Write. The only diagnostics were the same 20 classified FLOAT16_4 warnings owned by M2-016; there were zero analysis errors and zero uncatalogued findings.','',
'M2-012 acceptance: PASS. The generated output remains ignored and private.')
[System.IO.File]::WriteAllText($resolvedOutput,(($report-join[Environment]::NewLine)+[Environment]::NewLine),[System.Text.UTF8Encoding]::new($false))
[pscustomobject]@{Exported=$true;Iterations=10;Functions=8;FinalUnresolved=0;GeneratedFiles=64;GeneratedBytes=128031984;Deterministic=$true;OutputPath=$resolvedOutput;OutputSha256=(Get-FileHash $resolvedOutput -Algorithm SHA256).Hash}
