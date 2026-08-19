param(
    [string]$ReferenceRoot = ""
)

$ErrorActionPreference = "Stop"
$ProjectRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$ProjectContent = Join-Path $ProjectRoot "Content"
$UE58 = "C:\Program Files\Epic Games\UE_5.8"
$UnrealPak = Join-Path $UE58 "Engine\Binaries\Win64\UnrealPak.exe"
$Stage = Join-Path $ProjectRoot "ReferenceExtracted\LayOfTheLand"
$LogDir = Join-Path $ProjectRoot "ReferenceExtracted\Logs"

Write-Host ""
Write-Host "============================================================" -ForegroundColor Cyan
Write-Host " DEADBRICK - LayOfTheLand cooked-content importer (UE 5.8)" -ForegroundColor Cyan
Write-Host "============================================================" -ForegroundColor Cyan
Write-Host ""

if (Get-Process UnrealEditor -ErrorAction SilentlyContinue) {
    Write-Host "Close Unreal Editor before importing reference content." -ForegroundColor Red
    exit 10
}

if (-not (Test-Path $UnrealPak)) {
    Write-Host "UnrealPak.exe was not found at: $UnrealPak" -ForegroundColor Red
    exit 11
}

if ([string]::IsNullOrWhiteSpace($ReferenceRoot)) {
    $ReferenceRoot = Read-Host "Paste or drag the Steam game folder, LayOfTheLand, Content, or Paks folder here, then press Enter"
}
$ReferenceRoot = $ReferenceRoot.Trim().Trim('"').TrimEnd('\')

if (-not (Test-Path $ReferenceRoot)) {
    Write-Host "The supplied path does not exist: $ReferenceRoot" -ForegroundColor Red
    exit 12
}

$ResolvedInput = (Resolve-Path $ReferenceRoot).Path.TrimEnd('\')
$PakDir = $null

# Accept any of these inputs:
#   ...\steamapps\common\Lay of the Land
#   ...\steamapps\common\Lay of the Land\LayOfTheLand
#   ...\LayOfTheLand\Content
#   ...\LayOfTheLand\Content\Paks
$Candidates = @()

if ((Split-Path $ResolvedInput -Leaf) -ieq "Paks") {
    $Candidates += $ResolvedInput
}

$Candidates += (Join-Path $ResolvedInput "Paks")
$Candidates += (Join-Path $ResolvedInput "Content\Paks")
$Candidates += (Join-Path $ResolvedInput "LayOfTheLand\Content\Paks")

foreach ($Candidate in $Candidates) {
    if (Test-Path $Candidate -PathType Container) {
        $HasContainers = (Get-ChildItem $Candidate -File -ErrorAction SilentlyContinue | Where-Object {
            $_.Extension -in @('.pak', '.utoc', '.ucas')
        } | Select-Object -First 1) -ne $null

        if ($HasContainers) {
            $PakDir = (Resolve-Path $Candidate).Path.TrimEnd('\')
            break
        }
    }
}

if (-not $PakDir) {
    Write-Host "Could not locate a valid Content\Paks folder from: $ResolvedInput" -ForegroundColor Red
    Write-Host "You may drag any of these: game folder, LayOfTheLand folder, Content folder, or Paks folder." -ForegroundColor Yellow
    Write-Host "Expected to find .pak/.utoc/.ucas files inside the detected Paks directory." -ForegroundColor Yellow
    exit 12
}

# Normalize ReferenceRoot to the actual LayOfTheLand folder.
$ContentDir = Split-Path $PakDir -Parent
$ReferenceRoot = Split-Path $ContentDir -Parent

Write-Host "Reference build: $ReferenceRoot" -ForegroundColor Green
Write-Host "Pak directory:   $PakDir" -ForegroundColor Green

if (Test-Path $Stage) { Remove-Item $Stage -Recurse -Force }
New-Item -ItemType Directory -Path $Stage -Force | Out-Null
New-Item -ItemType Directory -Path $LogDir -Force | Out-Null
New-Item -ItemType Directory -Path $ProjectContent -Force | Out-Null

$ListLog = Join-Path $LogDir "container_list.txt"
$ExtractLog = Join-Path $LogDir "extract_log.txt"
$DescribeLog = Join-Path $LogDir "iostore_describe.txt"
Remove-Item $ListLog,$ExtractLog,$DescribeLog -ErrorAction SilentlyContinue

Write-Host ""
Write-Host "[1/4] Inspecting IoStore containers..." -ForegroundColor Cyan
$Utocs = Get-ChildItem $PakDir -Filter "*.utoc" -File | Where-Object { $_.Name -ne "global.utoc" }
foreach ($Container in $Utocs) {
    "===== $($Container.Name) =====" | Out-File $ListLog -Append -Encoding utf8
    & $UnrealPak $Container.FullName -List 2>&1 | Tee-Object -FilePath $ListLog -Append | Out-Host
}

$GlobalUcas = Join-Path $PakDir "global.ucas"
if (Test-Path $GlobalUcas) {
    Write-Host "Describing global IoStore package graph..." -ForegroundColor DarkGray
    & $UnrealPak IoStore "-Describe=$GlobalUcas" "-DumpToFile=$DescribeLog" 2>&1 | Out-File $ExtractLog -Append -Encoding utf8
}

Write-Host ""
Write-Host "[2/4] Extracting cooked content with UnrealPak..." -ForegroundColor Cyan

function Invoke-Extract([string]$ContainerPath) {
    Write-Host "Extracting $(Split-Path $ContainerPath -Leaf)..." -ForegroundColor Gray
    & $UnrealPak $ContainerPath "-Extract=$Stage" 2>&1 | Tee-Object -FilePath $ExtractLog -Append | Out-Host
    if ($LASTEXITCODE -ne 0) {
        Write-Host "First extract syntax returned $LASTEXITCODE; trying alternate UE syntax..." -ForegroundColor Yellow
        & $UnrealPak $ContainerPath -Extract $Stage 2>&1 | Tee-Object -FilePath $ExtractLog -Append | Out-Host
    }
}

foreach ($Container in $Utocs) {
    Invoke-Extract $Container.FullName
}

# Pak files can also contain loose cooked/staged files; extract them too.
$Paks = Get-ChildItem $PakDir -Filter "*.pak" -File
foreach ($Pak in $Paks) {
    Invoke-Extract $Pak.FullName
}

Write-Host ""
Write-Host "[3/4] Copying cooked Unreal assets into DEADBRICK Content with original /Game paths..." -ForegroundColor Cyan

$Extensions = @(".uasset", ".uexp", ".ubulk", ".uptnl")
$Copied = 0
$TopLevelEntries = New-Object 'System.Collections.Generic.HashSet[string]'

$CookedFiles = Get-ChildItem $Stage -Recurse -File | Where-Object { $Extensions -contains $_.Extension.ToLowerInvariant() }
foreach ($File in $CookedFiles) {
    $Full = $File.FullName
    $Marker = "\Content\"
    $Index = $Full.IndexOf($Marker, [System.StringComparison]::OrdinalIgnoreCase)
    if ($Index -lt 0) { continue }

    $Relative = $Full.Substring($Index + $Marker.Length)
    if ([string]::IsNullOrWhiteSpace($Relative)) { continue }

    $Destination = Join-Path $ProjectContent $Relative
    $DestinationDir = Split-Path -Parent $Destination
    New-Item -ItemType Directory -Path $DestinationDir -Force | Out-Null
    Copy-Item $File.FullName $Destination -Force
    $Copied++

    $FirstSegment = ($Relative -split '[\\/]')[0]
    if (-not [string]::IsNullOrWhiteSpace($FirstSegment)) {
        [void]$TopLevelEntries.Add($FirstSegment)
    }
}

# Keep imported cooked reference content local without polluting normal Git history.
$GitExclude = Join-Path $ProjectRoot ".git\info\exclude"
if (Test-Path (Split-Path -Parent $GitExclude)) {
    foreach ($Entry in $TopLevelEntries) {
        $Pattern = "/Content/$Entry/"
        $Existing = if (Test-Path $GitExclude) { Get-Content $GitExclude } else { @() }
        if ($Existing -notcontains $Pattern) {
            Add-Content -Path $GitExclude -Value $Pattern
        }
    }
}

$MarkerFile = Join-Path $ProjectRoot "Saved\LOTL_REFERENCE_IMPORT.txt"
New-Item -ItemType Directory -Path (Split-Path -Parent $MarkerFile) -Force | Out-Null
@(
    "Imported: $(Get-Date -Format o)",
    "ReferenceRoot: $ReferenceRoot",
    "PakDir: $PakDir",
    "CookedFilesCopied: $Copied",
    "ExtractStage: $Stage",
    "DescribeLog: $DescribeLog"
) | Set-Content $MarkerFile

Write-Host ""
if ($Copied -eq 0) {
    Write-Host "No .uasset/.uexp/.ubulk files were extracted." -ForegroundColor Red
    Write-Host "The build may require a different container extraction path or may be protected." -ForegroundColor Yellow
    Write-Host "Send me ReferenceExtracted\Logs\extract_log.txt and iostore_describe.txt." -ForegroundColor Yellow
    exit 20
}

Write-Host "$Copied cooked asset files copied into DEADBRICK while preserving their original Content paths." -ForegroundColor Green
Write-Host "The runtime now auto-searches those assets for player/enemy skins and compatible animations." -ForegroundColor Green

Write-Host ""
Write-Host "[4/4] Rebuilding DEADBRICK against UE 5.8..." -ForegroundColor Cyan
$Rebuild = Join-Path $ProjectRoot "REBUILD_AND_OPEN_UE58.bat"
if (Test-Path $Rebuild) {
    & $Rebuild
} else {
    Write-Host "REBUILD_AND_OPEN_UE58.bat not found. Rebuild the project manually." -ForegroundColor Yellow
}
