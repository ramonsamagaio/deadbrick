$ErrorActionPreference = "Stop"

$ProjectRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$ContentRoot = Join-Path $ProjectRoot "Content"
$Timestamp = Get-Date -Format "yyyyMMdd_HHmmss"
$QuarantineRoot = Join-Path $ProjectRoot "ReferenceExtracted\QuarantinedContent\$Timestamp"

Write-Host ""
Write-Host "============================================================" -ForegroundColor Cyan
Write-Host " DEADBRICK - validate editor Content packages" -ForegroundColor Cyan
Write-Host "============================================================" -ForegroundColor Cyan

if (-not (Test-Path $ContentRoot)) {
    Write-Host "Content folder not found; nothing to validate." -ForegroundColor Yellow
    exit 0
}

$Tracked = New-Object 'System.Collections.Generic.HashSet[string]' ([System.StringComparer]::OrdinalIgnoreCase)
try {
    $GitFiles = @(& git -C $ProjectRoot ls-files "Content/*" 2>$null)
    foreach ($GitFile in $GitFiles) {
        if (-not [string]::IsNullOrWhiteSpace($GitFile)) { [void]$Tracked.Add($GitFile.Replace('\','/')) }
    }
} catch {
    Write-Host "Git file list unavailable; package validation will still run." -ForegroundColor DarkYellow
}

function Test-UnrealPackageTag([string]$Path) {
    try {
        $Stream = [System.IO.File]::Open($Path, [System.IO.FileMode]::Open, [System.IO.FileAccess]::Read, [System.IO.FileShare]::ReadWrite)
        try {
            if ($Stream.Length -lt 4) { return $false }
            $Bytes = New-Object byte[] 4
            [void]$Stream.Read($Bytes, 0, 4)
            # Unreal PACKAGE_FILE_TAG = 0x9E2A83C1, little endian on disk.
            return $Bytes[0] -eq 0xC1 -and $Bytes[1] -eq 0x83 -and $Bytes[2] -eq 0x2A -and $Bytes[3] -eq 0x9E
        } finally {
            $Stream.Dispose()
        }
    } catch {
        return $false
    }
}

$InvalidByPath = @{}
function Add-InvalidPackage([System.IO.FileInfo]$File, [string]$Reason) {
    $Relative = $File.FullName.Substring($ProjectRoot.Length).TrimStart('\','/').Replace('\','/')
    if ($InvalidByPath.ContainsKey($Relative)) { return }
    $InvalidByPath[$Relative] = [PSCustomObject]@{
        File = $File
        Relative = $Relative
        Tracked = $Tracked.Contains($Relative)
        Reason = $Reason
    }
}

# First pass: definitely malformed package headers.
foreach ($File in Get-ChildItem $ContentRoot -Recurse -Filter "*.uasset" -File -ErrorAction SilentlyContinue) {
    if (-not (Test-UnrealPackageTag $File.FullName)) {
        Add-InvalidPackage $File "invalid package tag"
    }
}

# The previous LOTL importer copied cooked packages directly into /Game. Cooked/unversioned packages
# can still have a perfectly valid PACKAGE_FILE_TAG, so a 4-byte header test cannot identify them.
# The current pipeline never installs LOTL cooked packages into Content; therefore untracked files in
# these legacy roots are remnants of that old importer and must be isolated before the editor scans.
$LegacyCookedRoots = @(
    (Join-Path $ContentRoot "UltraDynamicSky"),
    (Join-Path $ContentRoot "Blueprints")
)

foreach ($LegacyRoot in $LegacyCookedRoots) {
    if (-not (Test-Path $LegacyRoot)) { continue }

    $RootRelative = $LegacyRoot.Substring($ProjectRoot.Length).TrimStart('\','/').Replace('\','/')
    $TrackedUnderRoot = @($Tracked | Where-Object { $_.StartsWith($RootRelative + "/", [System.StringComparison]::OrdinalIgnoreCase) })

    if ($TrackedUnderRoot.Count -gt 0) {
        Write-Host "Legacy-looking root contains tracked project files; leaving root untouched: $RootRelative" -ForegroundColor DarkYellow
        continue
    }

    foreach ($File in Get-ChildItem $LegacyRoot -Recurse -Filter "*.uasset" -File -ErrorAction SilentlyContinue) {
        Add-InvalidPackage $File "untracked cooked/unversioned legacy reference package"
    }
}

$Invalid = @($InvalidByPath.Values)
if ($Invalid.Count -eq 0) {
    Write-Host "No invalid or legacy cooked packages found in project Content." -ForegroundColor Green
    exit 0
}

$Moved = 0
$TrackedInvalid = 0
foreach ($Entry in $Invalid) {
    if ($Entry.Tracked) {
        $TrackedInvalid++
        Write-Host "TRACKED INVALID PACKAGE (left untouched): $($Entry.Relative) [$($Entry.Reason)]" -ForegroundColor Red
        continue
    }

    $RelativeInsideContent = $Entry.File.FullName.Substring($ContentRoot.Length).TrimStart('\','/')
    $BaseSource = [System.IO.Path]::Combine($Entry.File.DirectoryName, [System.IO.Path]::GetFileNameWithoutExtension($Entry.File.Name))
    $DestinationBase = Join-Path $QuarantineRoot ([System.IO.Path]::ChangeExtension($RelativeInsideContent, $null))
    New-Item -ItemType Directory -Path (Split-Path -Parent $DestinationBase) -Force | Out-Null

    foreach ($Ext in @('.uasset','.uexp','.ubulk','.uptnl')) {
        $Source = "$BaseSource$Ext"
        if (-not (Test-Path $Source)) { continue }
        $Destination = "$DestinationBase$Ext"
        Move-Item $Source $Destination -Force
    }
    $Moved++
    Write-Host "Quarantined: $($Entry.Relative) [$($Entry.Reason)]" -ForegroundColor Yellow
}

Write-Host ""
Write-Host "Quarantined packages: $Moved" -ForegroundColor Green
if ($Moved -gt 0) { Write-Host "Backup: $QuarantineRoot" -ForegroundColor DarkGray }
if ($TrackedInvalid -gt 0) {
    Write-Host "WARNING: $TrackedInvalid tracked package(s) also look invalid and were NOT modified." -ForegroundColor Red
    exit 2
}

exit 0
