param(
    [string]$ReferenceRoot = "",
    [switch]$SkipRebuild,
    [switch]$NonInteractive
)

$ErrorActionPreference = "Stop"
$ProgressPreference = "SilentlyContinue"

$ProjectRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
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
Write-Host " DEADBRICK - LayOfTheLand reference pipeline (UE 5.8)" -ForegroundColor Cyan
Write-Host " IoStore -> isolated cooked reference -> GLTF/PSK/PSA/PNG -> Unreal" -ForegroundColor Cyan
Write-Host "============================================================" -ForegroundColor Cyan
Write-Host ""

if (Get-Process UnrealEditor -ErrorAction SilentlyContinue) {
    Write-Host "UnrealEditor.exe is still running. Close it before extracting reference content." -ForegroundColor Red
    exit 10
}

if (-not (Test-Path $UnrealPak)) {
    Write-Host "UnrealPak.exe was not found at: $UnrealPak" -ForegroundColor Red
    exit 11
}

function Resolve-PakDirectory([string]$InputPath) {
    if ([string]::IsNullOrWhiteSpace($InputPath) -or -not (Test-Path $InputPath)) { return $null }
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

if ([string]::IsNullOrWhiteSpace($ReferenceRoot)) {
    $AutoCandidates = @(
        "C:\Program Files (x86)\Steam\steamapps\common\Lay of the Land\LayOfTheLand\Content\Paks",
        "C:\Program Files (x86)\Steam\steamapps\common\Lay of the Land\LayOfTheLand",
        "C:\Program Files\Steam\steamapps\common\Lay of the Land\LayOfTheLand\Content\Paks"
    )

    foreach ($Candidate in $AutoCandidates) {
        $Detected = Resolve-PakDirectory $Candidate
        if ($Detected) {
            $ReferenceRoot = $Detected
            Write-Host "Found LayOfTheLand automatically:" -ForegroundColor Green
            Write-Host $ReferenceRoot -ForegroundColor Green
            Write-Host ""
            break
        }
    }
}

if ([string]::IsNullOrWhiteSpace($ReferenceRoot)) {
    if ($NonInteractive) {
        Write-Host "LayOfTheLand was not found in the standard Steam locations." -ForegroundColor Red
        exit 12
    }
    $ReferenceRoot = Read-Host "Paste or drag the LayOfTheLand game folder, Content folder, or Paks folder here"
}
$ReferenceRoot = $ReferenceRoot.Trim().Trim('"')

$PakDir = Resolve-PakDirectory $ReferenceRoot
if (-not $PakDir) {
    Write-Host "Could not locate the LayOfTheLand Content\Paks directory from:" -ForegroundColor Red
    Write-Host $ReferenceRoot -ForegroundColor Yellow
    exit 12
}

Write-Host "Pak directory: $PakDir" -ForegroundColor Green
New-Item -ItemType Directory -Path $StageRoot,$Stage,$LegacyDir,$LogDir,$ToolsDir -Force | Out-Null

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
Write-Host "[1/6] Converting IoStore/Zen packages to an isolated legacy reference pak..." -ForegroundColor Cyan
& $RetocExe to-legacy $PakDir $LegacyPak 2>&1 | Tee-Object -FilePath $RetocLog | Out-Host
if ($LASTEXITCODE -ne 0 -or -not (Test-Path $LegacyPak)) {
    Write-Host "retoc failed to create the legacy package." -ForegroundColor Red
    Write-Host "Send me: ReferenceExtracted\Logs\retoc_to_legacy.txt" -ForegroundColor Yellow
    exit 20
}

Write-Host ""
Write-Host "[2/6] Listing converted packages..." -ForegroundColor Cyan
& $UnrealPak $LegacyPak -List 2>&1 | Tee-Object -FilePath $AssetListLog | Out-Host

Write-Host ""
Write-Host "[3/6] Extracting an inspection copy outside Content..." -ForegroundColor Cyan
& $UnrealPak $LegacyPak "-Extract=$Stage" 2>&1 | Tee-Object -FilePath $ExtractLog | Out-Host
if ($LASTEXITCODE -ne 0) {
    Write-Host "Primary UnrealPak extract syntax failed; trying alternate syntax..." -ForegroundColor Yellow
    & $UnrealPak $LegacyPak -Extract $Stage 2>&1 | Tee-Object -FilePath $ExtractLog -Append | Out-Host
}

$CookedFiles = @(Get-ChildItem $Stage -Recurse -File -ErrorAction SilentlyContinue)
$Uassets = @($CookedFiles | Where-Object { $_.Extension -ieq '.uasset' })
Write-Host "Inspection extraction contains $($CookedFiles.Count) files and $($Uassets.Count) visible .uasset files." -ForegroundColor Green
if ($Uassets.Count -eq 0) {
    Write-Host "That is NOT treated as failure: CUE4Parse reads the converted pak directly in the next stage." -ForegroundColor DarkYellow
}

Write-Host ""
Write-Host "[4/6] Keeping cooked packages OUTSIDE DEADBRICK Content..." -ForegroundColor Cyan
Write-Host "Cooked game packages are not editor-native source assets." -ForegroundColor Yellow
Write-Host "They stay isolated in ReferenceExtracted and are never mounted as /Game assets." -ForegroundColor Yellow
Write-Host "This prevents PACKAGE_FILE_TAG errors and broken material/texture state." -ForegroundColor Yellow

$MarkerFile = Join-Path $ProjectRoot "Saved\LOTL_REFERENCE_IMPORT.txt"
New-Item -ItemType Directory -Path (Split-Path -Parent $MarkerFile) -Force | Out-Null
@(
    "Extracted: $(Get-Date -Format o)",
    "PakDir: $PakDir",
    "Retoc: $RetocExe",
    "LegacyPak: $LegacyPak",
    "InspectionFiles: $($CookedFiles.Count)",
    "VisibleCookedUassets: $($Uassets.Count)",
    "InstalledIntoContent: False",
    "Stage: $Stage"
) | Set-Content $MarkerFile -Encoding UTF8

Write-Host ""
Write-Host "[5/6] Generating manifest and exporting editor-safe LOTL assets..." -ForegroundColor Cyan
$ManifestScript = Join-Path $ProjectRoot "GENERATE_LOTL_MANIFEST.ps1"
if (Test-Path $ManifestScript) {
    & powershell -NoProfile -ExecutionPolicy Bypass -File $ManifestScript
}

$ExportScript = Join-Path $ProjectRoot "EXPORT_LOTL_EDITOR_ASSETS.ps1"
if (Test-Path $ExportScript) {
    & powershell -NoProfile -ExecutionPolicy Bypass -File $ExportScript -LegacyPak $LegacyPak
    if ($LASTEXITCODE -ne 0) {
        Write-Host "LOTL cooked extraction succeeded, but some editor-safe export families need another pass." -ForegroundColor Yellow
        Write-Host "Recovered families remain usable." -ForegroundColor Yellow
    }
} else {
    Write-Host "EXPORT_LOTL_EDITOR_ASSETS.ps1 not found; skipping editor-safe conversion." -ForegroundColor Yellow
}

Write-Host ""
if ($SkipRebuild) {
    Write-Host "[6/6] Reference preparation complete. Parent rebuild will compile/import next." -ForegroundColor Green
    exit 0
}

Write-Host "[6/6] Rebuilding DEADBRICK, importing GLTF/PSK/PSA/PNG exports, and opening Unreal..." -ForegroundColor Cyan
$Rebuild = Join-Path $ProjectRoot "REBUILD_AND_OPEN_UE58.bat"
if (Test-Path $Rebuild) {
    & $Rebuild
} else {
    Write-Host "REBUILD_AND_OPEN_UE58.bat not found; rebuild manually." -ForegroundColor Yellow
}
