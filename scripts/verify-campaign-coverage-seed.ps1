[CmdletBinding()]
param([string]$MatrixPath=(Join-Path $PSScriptRoot '../config/campaign-coverage-seed.json'))
Set-StrictMode -Version Latest
$ErrorActionPreference='Stop'
$repo=(Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '..')).Path
$matrix=Get-Content -LiteralPath $MatrixPath -Raw|ConvertFrom-Json
function Exact($Value,[string[]]$Fields){if((@($Value.PSObject.Properties.Name|Sort-Object)-join'|') -cne (@($Fields|Sort-Object)-join'|')){throw 'Coverage schema topology drifted.'}}
Exact $matrix @('schema','task','catalog_complete','mandatory_node_count','dependency_graph_complete','full_campaign_pass','entries')
if($matrix.schema -cne 'mcla-campaign-coverage-seed-v1' -or $matrix.task -cne 'M7-001' -or $null -ne $matrix.mandatory_node_count){throw 'Coverage seed identity/count invalid.'}
foreach($name in @('catalog_complete','dependency_graph_complete','full_campaign_pass')){if($matrix.$name -isnot [bool] -or $matrix.$name){throw 'Preparation inventory cannot claim full campaign/catalog completion.'}}
$ids=[Collections.Generic.HashSet[string]]::new([StringComparer]::Ordinal)
$issues=[IO.File]::ReadAllText((Join-Path $repo 'docs/known-issues.md'))
if($matrix.entries -isnot [array] -or $matrix.entries.Count -lt 1){throw 'Coverage seed needs an explicit entry array.'}
foreach($entry in $matrix.entries){
    Exact $entry @('id','label','kind','evidence_level','evidence','observed','unknown','known_issues','counts_toward_full_campaign')
    if($entry.id -cnotmatch '^[a-z][a-z0-9-]+$' -or -not $ids.Add($entry.id)){throw 'Coverage IDs must be unique local identifiers.'}
    if($entry.kind -cnotin @('campaign-route','aggregate-session','recovery-route','support-route','noncampaign-route','proposed-route') -or
       $entry.evidence_level -cnotin @('historical-accepted','owner-attested-categories','implementation-with-historical-evidence','unverified-definition')){throw 'Coverage evidence classification invalid.'}
    if($entry.counts_toward_full_campaign -isnot [bool] -or $entry.counts_toward_full_campaign){throw 'Historical scenarios cannot automatically credit a clean campaign.'}
    if($entry.kind -ceq 'proposed-route' -and $entry.evidence_level -cne 'unverified-definition'){throw 'Proposed routes must not acquire physical evidence by schema edit.'}
    foreach($field in @('evidence','observed','unknown','known_issues')){if($entry.$field -isnot [array]){throw 'Coverage list must be an array.'}}
    # This historical definition has no native calibration evidence. Changing
    # two mutable classification fields must not promote it to a physical pass.
    if(($entry.id -ceq 'trevor-proposed-route' -or $entry.evidence -ccontains 'config/first-race-route.json') -and
       ($entry.kind -cne 'proposed-route' -or $entry.evidence_level -cne 'unverified-definition')){throw 'Uncalibrated first-race provenance must remain an unverified proposed route.'}
    foreach($field in @('evidence','observed','unknown')){if($entry.$field.Count -lt 1){throw 'Coverage must state evidence, observations and remaining gaps.'};foreach($value in $entry.$field){if($value -isnot [string] -or [string]::IsNullOrWhiteSpace($value)){throw 'Coverage text must be nonempty.'}}}
    foreach($relative in $entry.evidence){
        if($relative -cnotmatch '^(README-AI|docs/[a-zA-Z0-9_/-]+\.md|config/[a-zA-Z0-9_/-]+\.json)$' -or $relative -match '\.\.') {throw 'Coverage evidence path must be a safe public repository reference.'}
        if(-not (Test-Path -LiteralPath (Join-Path $repo $relative) -PathType Leaf)){throw 'Coverage evidence reference is missing.'}
    }
    foreach($issue in $entry.known_issues){if($issue -cnotmatch '^KI-\d{3}$' -or -not $issues.Contains('| '+$issue+' |')){throw 'Coverage references an unknown issue.'}}
}
[pscustomobject]@{Passed=$true;Entries=$matrix.entries.Count;FullCampaignPass=$false;CatalogComplete=$false;Scope='source inventory only; no gameplay credit'}
