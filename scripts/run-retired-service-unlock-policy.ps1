[CmdletBinding()]
param()

Set-StrictMode -Version Latest
$ErrorActionPreference='Stop'
$repo=(Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '..')).Path;$verify=Join-Path $PSScriptRoot 'verify-retired-service-unlock-policy.ps1';$policyPath=Join-Path $repo 'config/retired-service-unlock-policy.json'
$prior=[ordered]@{task='M6-008';run_id='20260819-164950-4b322fe1';sha256='7DC8946ABD4B1BB78D52E527655D713EB4FD7037421997F4BA209689BC21903B';decision='explicit-offline-progression-and-retired-service-matrix-pass'}
function Hash([string]$Path){(Get-FileHash -LiteralPath $Path -Algorithm SHA256).Hash}
function TextHash([string]$Path){$text=[IO.File]::ReadAllText($Path).Replace("`r`n","`n");$sha=[Security.Cryptography.SHA256]::Create();try{([BitConverter]::ToString($sha.ComputeHash([Text.Encoding]::UTF8.GetBytes($text)))).Replace('-','')}finally{$sha.Dispose()}}
Write-Host 'M6-009 [1/3]: validating retired-service history and the exact offline-policy prerequisite...' -ForegroundColor Cyan
$source=&$verify -SourceOnly;$priorPath=Join-Path $repo "private/evidence/$($prior.task)/$($prior.run_id)/result.json";if(-not(Test-Path -LiteralPath $priorPath)-or(Hash $priorPath)-cne$prior.sha256){throw 'Accepted M6-008 result drifted.'}
Write-Host 'M6-009 [2/3]: recording the retail-preserving compatibility/cheat boundary...' -ForegroundColor Cyan
$root=Join-Path $repo ('private/evidence/M6-009/'+(Get-Date -Format 'yyyyMMdd-HHmmss')+'-'+[guid]::NewGuid().ToString('N').Substring(0,8));[IO.Directory]::CreateDirectory($root)|Out-Null
$result=[ordered]@{schema='mcla-retired-service-unlock-decision-v1';task='M6-009';decision='retail-preserving-compatibility-with-explicit-cheat-separation';policy_sha256=TextHash $policyPath;prior_evidence=$prior;policy=$source.Policy;source_audit=[ordered]@{runtime_unlock_implementation_present=$false;save_or_source_game_mutation_performed=$false;current_service_entitlement_claimed=$false};scope=[ordered]@{retail_progression_preserved=$true;existing_authentic_entitlements_preserved=$true;compatibility_and_cheats_separated=$true;future_cheat_requires_explicit_opt_in=$true;future_cheat_excluded_from_canonical_evidence=$true;audi_r8_unlocked=$false;retired_service_emulated=$false;fake_live_state_used=$false;save_mutated=$false;source_game_mutated=$false;cheat_implemented=$false}}
$path=Join-Path $root 'result.json';[IO.File]::WriteAllText($path,($result|ConvertTo-Json -Depth 8),[Text.UTF8Encoding]::new($false))
Write-Host 'M6-009 [3/3]: revalidating the persisted decision and evidence boundary...' -ForegroundColor Cyan
$final=&$verify -ResultPath $path;Write-Host "M6-009 PASS: retired-service unlock policy approved without granting or fabricating an entitlement. Result: '$path'." -ForegroundColor Green;$final
