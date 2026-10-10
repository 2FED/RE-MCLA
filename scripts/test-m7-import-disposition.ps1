[CmdletBinding()]
param([string]$RepoRoot)

# Read-only source contract. This does NOT execute guest code or prove invocation.
# Pins intentionally reject regeneration or SDK edits until reviewed and repinned.
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
if ([string]::IsNullOrWhiteSpace($RepoRoot)) { $RepoRoot = Split-Path -Parent $PSScriptRoot }
$pins = [ordered]@{
    'third_party/rexglue-sdk/src/kernel/xboxkrnl/xboxkrnl_crypt.cpp' = '6EF4032AA9BD9CD10721CA1503C9A0B2C7F7415EC55D5F2DAEF9DBF9C9F9C880'
    'generated/default/mcla_recomp.29.cpp' = '2473F38749D70C6EBD2D5820E6ADFC756E0A2A906747E8D92EC836978F833675'
    'generated/default/mcla_init.cpp' = '35D1C5673F2F080B170DFB6491FF9CE65F7FCF02B86AED04DDA614298303A0CD'
    'generated/default/mcla_register.cpp' = 'DAD90C6C9B3247300EED7BE33F5549FBDC75FEEDC332E386EDACDE24DB256BE3'
    'third_party/rexglue-sdk/src/kernel/xboxkrnl/export_table.inc' = 'EFE1609D2609007E38A905EA8A18CE68228D15610D2BC85D08A6E832FE224950'
    'third_party/rexglue-sdk/src/kernel/xboxkrnl/xboxkrnl_io.cpp' = 'B27DA8CE8C2B8EBC6D4AE06EA0F94C11920A438C2587E5F24FB30FFB9C555BAC'
    'third_party/rexglue-sdk/src/kernel/xboxkrnl/xboxkrnl_rtl.cpp' = 'F6A94F4C7ABDC9C5E39A76E39295049D592815C4FEE11FC97C4ECAEFBC26A4BB'
    'third_party/rexglue-sdk/src/kernel/xboxkrnl/xboxkrnl_ob.cpp' = '57F566F209A75617041B07A10B37CCD3E5A8B5081226053F3316400C497863AE'
}
function Assert-PinnedBytes([byte[]]$Bytes, [string]$Expected) {
    $sha = [Security.Cryptography.SHA256]::Create()
    try { $actual = ([BitConverter]::ToString($sha.ComputeHash($Bytes))).Replace('-', '') }
    finally { $sha.Dispose() }
    if ($actual -cne $Expected) { throw 'Inspected source changed; review the disposition before repinning.' }
}
$texts = @{}
$negativeCount = 0
foreach ($path in $pins.Keys) {
    $bytes = [IO.File]::ReadAllBytes((Join-Path $RepoRoot $path))
    Assert-PinnedBytes $bytes $pins[$path]
    $texts[$path] = [Text.Encoding]::UTF8.GetString($bytes)
    # In-memory drift fixture only: no source/temporary file is written.
    $changed = [byte[]]$bytes.Clone()
    $changed[0] = $changed[0] -bxor 1
    $rejected = $false
    try { Assert-PinnedBytes $changed $pins[$path] } catch { $rejected = $true }
    if (-not $rejected) { throw 'Source drift was accepted.' }
    $negativeCount++
}
$script:factCount = 0
function Assert-Count([string]$Text, [string]$Pattern, [int]$Expected) {
    if ([regex]::Matches($Text, $Pattern).Count -ne $Expected) { throw "Source fact changed: $Pattern" }
    $script:factCount++
}
$crypt = $texts['third_party/rexglue-sdk/src/kernel/xboxkrnl/xboxkrnl_crypt.cpp']
Assert-Count $crypt '(?s)u32 XeKeysConsoleSignatureVerification_entry\(mapped_void hash, mapped_void signature,\s*mapped_void pubkey\) \{\s*REXKRNL_DEBUG\("XeKeysConsoleSignatureVerification - stub"\);\s*return 0;  // Success \(signature valid\)\s*\}' 1
Assert-Count $crypt '(?s)REX_EXPORT\(__imp__XeKeysConsoleSignatureVerification,\s*rex::kernel::xboxkrnl::XeKeysConsoleSignatureVerification_entry\)' 1
Assert-Count $texts['third_party/rexglue-sdk/src/kernel/xboxkrnl/export_table.inc'] 'XE_EXPORT\(xboxkrnl, 0x00000257, XeKeysConsoleSignatureVerification,\s*kFunction\)' 1
Assert-Count $texts['generated/default/mcla_init.cpp'] '\{ 0x827BD8E4, __imp__XeKeysConsoleSignatureVerification \}' 1
Assert-Count $texts['generated/default/mcla_register.cpp'] 'SetFunction\(0x827BD8E4, __imp__XeKeysConsoleSignatureVerification\)' 1
$tu = $texts['generated/default/mcla_recomp.29.cpp']
$wrapper = [regex]::Match($tu, '(?s)DEFINE_REX_FUNC\(sub_8244D9C0\).*?(?=DEFINE_REX_FUNC\(sub_8244DA40\))').Value
Assert-Count $wrapper 'ctx\.r11\.s64 = 0;\s*// stw|ctx\.r11\.s64 = 0;\s*// mr r31,r3' 1
Assert-Count $wrapper 'REX_STORE_U32\(ctx\.r1\.u32 \+ 80, ctx\.r11\.u32\);' 1
Assert-Count $wrapper '(?s)__imp__XeKeysConsoleSignatureVerification\(ctx, base\);\s*// cmpwi r3,0\s*ctx\.cr0\.compare<int32_t>\(ctx\.r3\.s32, 0, ctx\.xer\);\s*// beq 0x8244da28\s*if \(ctx\.cr0\.eq\) goto loc_8244DA28;' 1
Assert-Count $wrapper '(?s)ctx\.r11\.u64 = REX_LOAD_U32\(ctx\.r1\.u32 \+ 80\);.*?ctx\.r3\.s64 = 1;.*?if \(!ctx\.cr6\.eq\) goto loc_8244DA2C;.*?loc_8244DA28:.*?ctx\.r3\.s64 = 0;' 1
Assert-Count $tu '(?s)sub_8244D9C0\(ctx, base\);\s*// cmpwi r3,0\s*ctx\.cr0\.compare<int32_t>\(ctx\.r3\.s32, 0, ctx\.xer\);\s*// beq 0x8244e8e0\s*if \(ctx\.cr0\.eq\) goto loc_8244E8E0;' 1
Assert-Count $tu '(?s)loc_8244E8E0:.*?REX_STORE_U32\(ctx\.r1\.u32 \+ 160, ctx\.r31\.u32\);.*?REX_STORE_U32\(ctx\.r1\.u32 \+ 788, ctx\.r20\.u32\);.*?ctx\.r29\.u64 = ctx\.r29\.u64 \| 15;.*?REX_STORE_U32\(ctx\.r11\.u32 \+ 0, ctx\.r28\.u32\);.*?REX_STORE_U32\(ctx\.r11\.u32 \+ 4, ctx\.r28\.u32\);\s*loc_8244E8F8:' 1
$importCalls = 0; $wrapperCalls = 0
foreach ($file in Get-ChildItem -LiteralPath (Join-Path $RepoRoot 'generated/default') -Filter '*.cpp' -File) {
    $content = [IO.File]::ReadAllText($file.FullName)
    $importCalls += [regex]::Matches($content, '__imp__XeKeysConsoleSignatureVerification\(ctx, base\);').Count
    $wrapperCalls += [regex]::Matches($content, 'sub_8244D9C0\(ctx, base\);').Count
}
if ($importCalls -ne 1 -or $wrapperCalls -ne 1) { throw 'Direct-call inventory changed; re-audit all callers.' }
$modelCases = 0
foreach ($status in @([uint32]0, [uint32]1, [uint32]::MaxValue)) {
    foreach ($output in @([uint32]0, [uint32]1, [uint32]::MaxValue)) {
        $branchResult = 0
        if ($status -ne 0) { $branchResult = 1; if ($output -eq 0) { $branchResult = 0 } }
        if ($branchResult -ne [int](($status -ne 0) -and ($output -ne 0))) { throw 'Branch model failed.' }
        $modelCases++
    }
}
[pscustomobject]@{
    Passed = $true; Decision = 'source-bound-unavailable-verification-fallback'
    SourcePins = $pins.Count; SourceFacts = $script:factCount; DriftNegatives = $negativeCount
    DirectImportCalls = $importCalls; DirectWrapperCalls = $wrapperCalls; ModelCases = $modelCases
    ActualGuestExecution = $false; RuntimeInvocationKnown = $false
    CryptographicVerificationImplemented = $false
}
