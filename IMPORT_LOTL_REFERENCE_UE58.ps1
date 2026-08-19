param(
    [string]$ReferenceRoot = ""
)

$ErrorActionPreference = "Stop"
$ProgressPreference = "SilentlyContinue"

$ProjectRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$ProjectContent = Join-Path $ProjectRoot "Content"
$UE58 = "C:\Program Files\Epic Games\UE_5.8"
$UnrealPak = Join-Path $UE58 "Engine\Binaries\Win64\UnrealPak.exe"
$StageRoot = Join-Path $ProjectRoot "ReferenceExtracted"
$Stage = Join-Path $StageRoot "LayOfTheLand"
$LegacyDir = Join-Path $StageRoot "Legacy"
$LegacyPak = Join-Path $LegacyDir "LayOfTheLand_Legacy.pak"
$LogDir = Join-Path $StageRoot "Logs"
$ToolsDir = Join-Path $ProjectRoot "Tools\retoc"
$RetocExe = Join-Path $ToolsDir "retoc.exe"

Write-Host ""
Write-Host "============================================================" -ForegroundColor Cyan
Write-Host " DEADBRICK - LayOfTheLand reference importer (UE 5.8)" -ForegroundColor Cyan
Write-Host " IoStore -> retoc legacy assets -> DEADBRICK Content" -ForegroundColor Cyan
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
    $ReferenceRoot = Read-Host "Paste or drag the LayOfTheLand game folder, Content folder, or Paks folder here"
}
$ReferenceRoot = $ReferenceRoot.Trim().Trim('"')

function Resolve-PakDirectory([string]$InputPath) {
    if (-not (Test-Path $InputPath)) { return $null }
    $Resolved = (Resolve-Path $InputPath).Path

    if ((Split-Path $Resolved -Leaf) -ieq "Paks") {
        if (Get-ChildItem $Resolved -Filter "*.utoc" -File -ErrorAction SilentlyContinue) { return $Resolved }
    }
    if ((Split-Path $Resolved -Leaf) -ieq "Content") {
        $Candidate = Join-Path $Resolved "Paks"
        if (Test-Path $Candidate) { return $Candidate }
    }

    $Candidates = @(
        (Join-Path $Resolved "Content\Paks"),
        (Join-Path $Resolved "LayOfTheLand\Content\Paks"),
        (Join-Path $Resolved "Lay of the Land\LayOfTheLand\Content\Paks")
    )
    foreach ($Candidate in $Candidates) {
        if (Test-Path $Candidate) { return $Candidate }
    }
    return $null
}

$PakDir = Resolve-PakDirectory $ReferenceRoot
if (-not $PakDir) {
    Write-Host "Could not locate the LayOfTheLand Content\Paks directory from:" -ForegroundColor Red
    Write-Host $ReferenceRoot -ForegroundColor Yellow
    exit 12
}

Write-Host "Pak directory: $PakDir" -ForegroundColor Green
New-Item -ItemType Directory -Path $StageRoot,$Stage,$LegacyDir,$LogDir,$ToolsDir,$ProjectContent -Force | Out-Null

# Retoc is specifically designed for Unreal Engine IoStore (.utoc/.ucas) and can convert
# modern Zen packages to legacy .uasset/.uexp packages. UnrealPak alone cannot do that conversion.
function Ensure-Retoc {
    if (Test-Path $RetocExe) { return }

    Write-Host ""
    Write-Host "Downloading retoc (IoStore conversion tool)..." -ForegroundColor Cyan
    $Headers = @{ "User-Agent" = "DEADBRICK-reference-importer" }
    $Release = Invoke-RestMethod -Headers $Headers -Uri "https://api.github.com/repos/trumank/retoc/releases/latest"
    $Asset = $Release.assets | Where-Object {
        $_.name -match 'retoc.*(windows|pc-windows|msvc).*\.zip$'
    } | Select-Object -First 1
    if (-not $Asset) {
        $Asset = $Release.assets | Where-Object { $_.name -match '\.zip$' } | Select-Object -First 1
    }
    if (-not $Asset) {
        throw "Could not find a Windows retoc release archive on GitHub."
    }

    $Zip = Join-Path $ToolsDir $Asset.name
    Invoke-WebRequest -Headers $Headers -Uri $Asset.browser_download_url -OutFile $Zip
    Expand-Archive -Path $Zip -DestinationPath $ToolsDir -Force
    Remove-Item $Zip -Force -ErrorAction SilentlyContinue

    $Found = Get-ChildItem $ToolsDir -Recurse -Filter "retoc.exe" -File | Select-Object -First 1
    if (-not $Found) { throw "retoc.exe was not present in the downloaded release." }
    if ($Found.FullName -ne $RetocExe) { Copy-Item $Found.FullName $RetocExe -Force }
}

Ensure-Retoc
Write-Host "retoc: $RetocExe" -ForegroundColor DarkGray

$RetocLog = Join-Path $LogDir "retoc_to_legacy.txt"
$ExtractLog = Join-Path $LogDir "legacy_extract.txt"
$AssetListLog = Join-Path $LogDir "legacy_asset_list.txt"
Remove-Item $RetocLog,$ExtractLog,$AssetListLog -ErrorAction SilentlyContinue
Remove-Item $LegacyPak -Force -ErrorAction SilentlyContinue
if (Test-Path $Stage) { Remove-Item $Stage -Recurse -Force }
New-Item -ItemType Directory -Path $Stage -Force | Out-Null

Write-Host ""
Write-Host "[1/5] Converting IoStore/Zen packages to legacy .uasset/.uexp..." -ForegroundColor Cyan
& $RetocExe to-legacy $PakDir $LegacyPak 2>&1 | Tee-Object -FilePath $RetocLog | Out-Host
if ($LASTEXITCODE -ne 0 -or -not (Test-Path $LegacyPak)) {
    Write-Host "retoc failed to create the legacy package." -ForegroundColor Red
    Write-Host "Send me: ReferenceExtracted\Logs\retoc_to_legacy.txt" -ForegroundColor Yellow
    exit 20
}

Write-Host ""
Write-Host "[2/5] Listing converted packages..." -ForegroundColor Cyan
& $UnrealPak $LegacyPak -List 2>&1 | Tee-Object -FilePath $AssetListLog | Out-Host

Write-Host ""
Write-Host "[3/5] Extracting legacy Unreal packages..." -ForegroundColor Cyan
& $UnrealPak $LegacyPak "-Extract=$Stage" 2>&1 | Tee-Object -FilePath $ExtractLog | Out-Host
if ($LASTEXITCODE -ne 0) {
    Write-Host "Primary UnrealPak extract syntax failed; trying alternate syntax..." -ForegroundColor Yellow
    & $UnrealPak $LegacyPak -Extract $Stage 2>&1 | Tee-Object -FilePath $ExtractLog -Append | Out-Host
}

$Uassets = Get-ChildItem $Stage -Recurse -Filter "*.uasset" -File -ErrorAction SilentlyContinue
if (-not $Uassets -or $Uassets.Count -eq 0) {
    Write-Host "Conversion completed but no .uasset packages appeared after extraction." -ForegroundColor Red
    Write-Host "Send me these two small logs:" -ForegroundColor Yellow
    Write-Host "  ReferenceExtracted\Logs\retoc_to_legacy.txt" -ForegroundColor Yellow
    Write-Host "  ReferenceExtracted\Logs\legacy_asset_list.txt" -ForegroundColor Yellow
    exit 21
}

Write-Host "Recovered $($Uassets.Count) .uasset packages." -ForegroundColor Green

Write-Host ""
Write-Host "[4/5] Installing cooked reference assets locally into DEADBRICK Content..." -ForegroundColor Cyan
$Extensions = @(".uasset", ".uexp", ".ubulk", ".uptnl")
$Copied = 0
$TopLevelEntries = New-Object 'System.Collections.Generic.HashSet[string]'
$CookedFiles = Get-ChildItem $Stage -Recurse -File | Where-Object { $Extensions -contains $_.Extension.ToLowerInvariant() }

foreach ($File in $CookedFiles) {
    $Full = $File.FullName
    $Normalized = $Full.Replace('/', '\')
    $Marker = "\Content\"
    $Index = $Normalized.IndexOf($Marker, [System.StringComparison]::OrdinalIgnoreCase)

    if ($Index -ge 0) {
        $Relative = $Normalized.Substring($Index + $Marker.Length)
    } else {
        # retoc's legacy pak can mount directly at a game-relative path. Strip common mount prefixes.
        $Relative = $Normalized.Substring($Stage.Length).TrimStart('\')
        $Relative = $Relative -replace '^\.\.\\\.\.\\\.\.\\', ''
        $Relative = $Relative -replace '^LayOfTheLand\\Content\\', ''
        $Relative = $Relative -replace '^Game\\Content\\', ''
    }

    if ([string]::IsNullOrWhiteSpace($Relative)) { continue }
    if ($Relative -match '^(Engine\\|Plugins\\)') { continue }

    $Destination = Join-Path $ProjectContent $Relative
    $DestinationDir = Split-Path -Parent $Destination
    New-Item -ItemType Directory -Path $DestinationDir -Force | Out-Null
    Copy-Item $File.FullName $Destination -Force
    $Copied++

    $FirstSegment = ($Relative -split '[\\/]')[0]
    if (-not [string]::IsNullOrWhiteSpace($FirstSegment)) { [void]$TopLevelEntries.Add($FirstSegment) }
}

# Keep the large extracted/reference payload local while source code and manifests remain versioned.
$GitExclude = Join-Path $ProjectRoot ".git\info\exclude"
if (Test-Path (Split-Path -Parent $GitExclude)) {
    $Existing = if (Test-Path $GitExclude) { @(Get-Content $GitExclude) } else { @() }
    foreach ($Entry in $TopLevelEntries) {
        $Pattern = "/Content/$Entry/"
        if ($Existing -notcontains $Pattern) {
            Add-Content -Path $GitExclude -Value $Pattern
            $Existing += $Pattern
        }
    }
}

$MarkerFile = Join-Path $ProjectRoot "Saved\LOTL_REFERENCE_IMPORT.txt"
New-Item -ItemType Directory -Path (Split-Path -Parent $MarkerFile) -Force | Out-Null
@(
    "Imported: $(Get-Date -Format o)",
    "PakDir: $PakDir",
    "Retoc: $RetocExe",
    "LegacyPak: $LegacyPak",
    "UassetsRecovered: $($Uassets.Count)",
    "CookedFilesInstalled: $Copied",
    "Stage: $Stage"
) | Set-Content $MarkerFile

Write-Host "Installed $Copied cooked package files locally." -ForegroundColor Green

Write-Host ""
Write-Host "[5/5] Generating the precise asset manifest and rebuilding DEADBRICK..." -ForegroundColor Cyan
$ManifestScript = Join-Path $ProjectRoot "GENERATE_LOTL_MANIFEST.ps1"
if (Test-Path $ManifestScript) {
    & powershell -NoProfile -ExecutionPolicy Bypass -File $ManifestScript
}

$Rebuild = Join-Path $ProjectRoot "REBUILD_AND_OPEN_UE58.bat"
if (Test-Path $Rebuild) {
    & $Rebuild
} else {
    Write-Host "REBUILD_AND_OPEN_UE58.bat not found; rebuild manually." -ForegroundColor Yellow
}
