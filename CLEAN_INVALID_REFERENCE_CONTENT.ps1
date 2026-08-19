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
    $GitFiles = @(& git -C $ProjectRoot ls-files "Content/*.uasset" 2>$null)
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

$Invalid = @()
foreach ($File in Get-ChildItem $ContentRoot -Recurse -Filter "*.uasset" -File -ErrorAction SilentlyContinue) {
    if (Test-UnrealPackageTag $File.FullName) { continue }
    $Relative = $File.FullName.Substring($ProjectRoot.Length).TrimStart('\','/').Replace('\','/')
    $Invalid += [PSCustomObject]@{ File = $File; Relative = $Relative; Tracked = $Tracked.Contains($Relative) }
}

if ($Invalid.Count -eq 0) {
    Write-Host "All project .uasset headers are editor-readable. Nothing quarantined." -ForegroundColor Green
    exit 0
}

$Moved = 0
$TrackedInvalid = 0
foreach ($Entry in $Invalid) {
    if ($Entry.Tracked) {
        $TrackedInvalid++
        Write-Host "TRACKED INVALID PACKAGE (left untouched): $($Entry.Relative)" -ForegroundColor Red
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
    Write-Host "Quarantined cooked/non-editor package: $($Entry.Relative)" -ForegroundColor Yellow
}

Write-Host ""
Write-Host "Quarantined packages: $Moved" -ForegroundColor Green
if ($Moved -gt 0) { Write-Host "Backup: $QuarantineRoot" -ForegroundColor DarkGray }
if ($TrackedInvalid -gt 0) {
    Write-Host "WARNING: $TrackedInvalid tracked package(s) also have invalid headers and were NOT modified." -ForegroundColor Red
    exit 2
}

exit 0
