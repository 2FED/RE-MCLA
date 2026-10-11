#requires -Version 7.0
[CmdletBinding()]
param([Parameter(Mandatory)][string]$BuildDirectory)
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
. (Join-Path $PSScriptRoot 'Mcla-TestBuildSigning.ps1')
. (Join-Path $PSScriptRoot 'Portable-CppRuntime.ps1')
Assert-MclaTestBuildWindows

# PRIMARY-RUN ONLY. Host must implement lifecycle-probe admission with signature,
# host hash and expiry checks intact, but NO MessageBox, network or OS-known-folder
# lease-cache access. Environment redirection cannot redirect SHGetKnownFolderPath.
# This runs no game assets and never invokes New-MclaTestBuildKey.ps1.
# All originals stay read-pinned; all new files are synthetic TEMP test evidence.
function Write-AdmissionNewText([string]$Path, [string]$Text) {
    $stream = [IO.File]::Open($Path, [IO.FileMode]::CreateNew, [IO.FileAccess]::Write, [IO.FileShare]::None)
    try {
        $bytes = [Text.UTF8Encoding]::new($false).GetBytes($Text)
        $stream.Write($bytes, 0, $bytes.Length)
        $stream.Flush($true)
    } finally { $stream.Dispose() }
}
function Copy-AdmissionPinned($Record, [string]$Destination) {
    if ((Get-MclaTestBuildPinnedHash $Record.Stream) -cne $Record.Sha256) { throw 'Pinned original changed before copy.' }
    $output = [IO.File]::Open($Destination, [IO.FileMode]::CreateNew, [IO.FileAccess]::Write, [IO.FileShare]::None)
    try { $Record.Stream.Position = 0; $Record.Stream.CopyTo($output); $output.Flush($true) }
    finally { $output.Dispose(); $Record.Stream.Position = 0 }
    if ((Get-FileHash -LiteralPath $Destination -Algorithm SHA256).Hash -cne $Record.Sha256 -or
        (Get-MclaTestBuildPinnedHash $Record.Stream) -cne $Record.Sha256) { throw 'Source/copy SHA256 mismatch.' }
}
function Assert-AdmissionInventory([string]$Directory, [string[]]$Allowed) {
    $items = @(Get-ChildItem -LiteralPath $Directory -Force)
    foreach ($item in $items) {
        if ($item.PSIsContainer -or ($item.Attributes -band [IO.FileAttributes]::ReparsePoint) -or
            $item.Name -cnotin $Allowed) {
            throw 'Rejected host created an unexpected file/directory (including user/cache/log outputs).'
        }
        $streams = @(Get-Item -LiteralPath $item.FullName -Stream '*' -ErrorAction Stop)
        if ($streams.Count -ne 1 -or $streams[0].Stream -cne ':$DATA') { throw 'Unexpected alternate stream in case.' }
    }
    if ($items.Count -ne $Allowed.Count) { throw 'Case inventory lost an expected file.' }
}
function Invoke-AdmissionCase($Case, [string]$EvidenceDirectory, [string]$WindowsDirectory) {
    $info = [Diagnostics.ProcessStartInfo]::new()
    $info.FileName = Join-Path $Case.Directory 'mcla.exe'
    $info.WorkingDirectory = $Case.Directory
    $info.UseShellExecute = $false
    $info.CreateNoWindow = $true
    $info.WindowStyle = [Diagnostics.ProcessWindowStyle]::Hidden
    $info.RedirectStandardOutput = $true
    $info.RedirectStandardError = $true
    $info.ArgumentList.Add('--mcla_lifecycle_probe=true')
    $info.ArgumentList.Add('--mcla_crash_reporter_dialog=false')
    # No inherited tokens, profile pointers, config variables, debugger/loader
    # overrides, network proxy credentials, application PATH or shell execution.
    $info.Environment.Clear()
    $info.Environment['SystemRoot'] = $WindowsDirectory
    $info.Environment['WINDIR'] = $WindowsDirectory
    $info.Environment['SystemDrive'] = [IO.Path]::GetPathRoot($WindowsDirectory).TrimEnd('\')
    $info.Environment['PATH'] = (Join-Path $WindowsDirectory 'System32') + ';' + $WindowsDirectory
    foreach ($name in @('TEMP','TMP','USERPROFILE','LOCALAPPDATA','APPDATA','PROGRAMDATA','HOME')) {
        $info.Environment[$name] = $Case.Directory
    }
    $info.Environment['HOMEDRIVE'] = [IO.Path]::GetPathRoot($Case.Directory).TrimEnd('\')
    $info.Environment['HOMEPATH'] = $Case.Directory.Substring(2)
    $process = [Diagnostics.Process]::new()
    $process.StartInfo = $info
    $started = $false
    $outTask = $null
    $errTask = $null
    $stdout = ''
    $stderr = ''
    $result = [ordered]@{ case = $Case.Name; pid = 0; exit_code = $null; timeout = $false;
        result = 'FAILED'; expected_stderr = $Case.Expected; error = $null; source_kind = 'isolated copied host; no game assets' }
    try {
        Assert-AdmissionInventory $Case.Directory $Case.Allowed
        if (-not $process.Start()) { throw 'Owned admission probe failed to start.' }
        $started = $true
        $result.pid = $process.Id
        $outTask = $process.StandardOutput.ReadToEndAsync()
        $errTask = $process.StandardError.ReadToEndAsync()
        if (-not $process.WaitForExit(15000)) {
            $result.timeout = $true
            # This Process instance identifies only this exact started child.
            # No process-name lookup, machine-wide kill or unrelated GUI action.
            $process.Kill($true)
            if (-not $process.WaitForExit(5000)) { throw 'Timed-out owned process did not finish after bounded cleanup.' }
        }
        if (-not $outTask.Wait(2000) -or -not $errTask.Wait(2000)) { throw 'Owned output streams did not complete.' }
        $stdout = $outTask.GetAwaiter().GetResult()
        $stderr = $errTask.GetAwaiter().GetResult()
        $result.exit_code = $process.ExitCode
        if ($result.timeout) { throw 'Owned process exceeded15-second deadline; evidence preserved.' }
        if ($process.ExitCode -eq 0) { throw 'Rejected ticket unexpectedly admitted the host.' }
        if ($stderr -notmatch $Case.Expected) { throw 'Nonzero exit did not report the expected ticket admission rejection.' }
        Assert-AdmissionInventory $Case.Directory $Case.Allowed
        $result.result = 'PASS'
    } catch {
        $result.error = $_.Exception.Message
    } finally {
        if ($started -and -not $process.HasExited) {
            try { $process.Kill($true); $null = $process.WaitForExit(5000) }
            catch { $result.error = 'Owned cleanup failed; retained PID identifies the only authorized cleanup target.' }
        }
        if ($null -ne $outTask -and $outTask.IsCompletedSuccessfully) { $stdout = $outTask.GetAwaiter().GetResult() }
        if ($null -ne $errTask -and $errTask.IsCompletedSuccessfully) { $stderr = $errTask.GetAwaiter().GetResult() }
        Write-AdmissionNewText (Join-Path $EvidenceDirectory ($Case.Name + '.stdout.txt')) $stdout
        Write-AdmissionNewText (Join-Path $EvidenceDirectory ($Case.Name + '.stderr.txt')) $stderr
        Write-AdmissionNewText (Join-Path $EvidenceDirectory ($Case.Name + '.json')) ($result | ConvertTo-Json -Depth 5)
        $process.Dispose()
    }
    if ($result.result -cne 'PASS') { throw ('Admission case failed: ' + $Case.Name + '. ' + $result.error) }
    return [pscustomobject]$result
}

$fixture = Join-Path ([IO.Path]::GetTempPath()) ('mcla-host-admission-' + [Guid]::NewGuid().ToString('N'))
$null = [IO.Directory]::CreateDirectory($fixture)
$evidence = Join-Path $fixture 'evidence'
$null = [IO.Directory]::CreateDirectory($evidence)
$pinned = [ordered]@{}
$results = [Collections.Generic.List[object]]::new()
$summary = [ordered]@{ result = 'FAILED'; source_files = @(); cases = @(); originals_unchanged = $false; error = $null }
try {
    # Exact staged build directory, never search parent/children or copy tickets,
    # TOML/settings, launcher state, helpers, game assets or personal data.
    $policy = Get-PortableCppRuntimePolicy
    $names = @('mcla.exe','rexruntime.dll','rexgpu-xenos.dll','TracyClient.dll') + @($policy.files | ForEach-Object { $_.name })
    foreach ($name in $names) {
        $stream = Open-MclaTestBuildPinnedInput (Join-Path $BuildDirectory $name)
        $record = [pscustomobject]@{ Stream = $stream; Sha256 = ''; Name = $name }
        $pinned[$name] = $record
        $record.Sha256 = Get-MclaTestBuildPinnedHash $stream
    }
    foreach ($entry in $policy.files) {
        Assert-PortableCppRuntimeFile -Path (Join-Path $BuildDirectory $entry.name) -Sha256 $entry.sha256 -Version $policy.version
    }
    $summary.source_files = @($pinned.Values | ForEach-Object { [ordered]@{ name = $_.Name; sha256 = $_.Sha256 } })
    $cases = @()
    foreach ($name in @('missing-ticket','invalid-ticket','expired-ticket')) {
        $directory = Join-Path $fixture ($name + '-' + [Guid]::NewGuid().ToString('N'))
        $null = [IO.Directory]::CreateDirectory($directory)
        foreach ($record in $pinned.Values) { Copy-AdmissionPinned $record (Join-Path $directory $record.Name) }
        $allowed = @($names)
        $expected = 'Test-build ticket missing'
        if ($name -cne 'missing-ticket') { $allowed += 'mcla-test-build.ticket' }
        if ($name -ceq 'invalid-ticket') {
            $expected = 'signature|Malformed'
            $invalid = "MCLA-TEST-BUILD-V1`n" + $pinned['mcla.exe'].Sha256 + ' ' +
                [DateTimeOffset]::UtcNow.ToUnixTimeSeconds().ToString([Globalization.CultureInfo]::InvariantCulture) +
                "`n--SIGNATURE--`n" + ('0' * 512) + "`n"
            Write-MclaTestBuildNewEnvelope (Join-Path $directory 'mcla-test-build.ticket') $invalid
        } elseif ($name -ceq 'expired-ticket') {
            $expected = 'BUILD EXPIRED'
            $created = [DateTimeOffset]::UtcNow.AddDays(-15).ToUnixTimeSeconds().ToString([Globalization.CultureInfo]::InvariantCulture)
            # Uses only an already-existing production signing key. Failure to
            # open it stops preparation BEFORE any of the copied hosts launch.
            $null = & (Join-Path $PSScriptRoot 'New-MclaTestBuildTicket.ps1') -HostPath (Join-Path $directory 'mcla.exe') `
                -OutputPath (Join-Path $directory 'mcla-test-build.ticket') -CreatedUtc $created
        }
        $cases += [pscustomobject]@{ Name = $name; Directory = $directory; Allowed = $allowed; Expected = $expected }
    }
    $windowsDirectory = [Environment]::GetFolderPath([Environment+SpecialFolder]::Windows)
    if ([string]::IsNullOrEmpty($windowsDirectory)) { throw 'Cannot resolve Windows system directory.' }
    foreach ($case in $cases) { $results.Add((Invoke-AdmissionCase $case $evidence $windowsDirectory)) }
    $summary.result = 'PASS'
} catch {
    $summary.error = $_.Exception.Message
} finally {
    $unchanged = $true
    foreach ($record in $pinned.Values) {
        try { if ((Get-MclaTestBuildPinnedHash $record.Stream) -cne $record.Sha256) { $unchanged = $false } }
        catch { $unchanged = $false }
        finally { $record.Stream.Dispose() }
    }
    $summary.originals_unchanged = $unchanged
    $summary.cases = $results.ToArray()
    if (-not $unchanged) { $summary.result = 'FAILED'; $summary.error = 'Original source identity changed or could not be verified.' }
    Write-AdmissionNewText (Join-Path $evidence 'summary.json') ($summary | ConvertTo-Json -Depth 8)
    Write-Output ('Evidence preserved: ' + $fixture)
}
if ($summary.result -cne 'PASS') { throw ('Host admission gate FAILED. ' + $summary.error) }
Write-Output 'HOST_ADMISSION_MISSING_INVALID_EXPIRED_PASS; no gameplay claim.'
