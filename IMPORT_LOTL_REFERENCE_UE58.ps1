param(
    [string]$ReferenceRoot = ""
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
Write-Host " DEADBRICK - LayOfTheLand reference extractor (UE 5.8)" -ForegroundColor Cyan
Write-Host " IoStore -> retoc legacy reference packages (isolated)" -ForegroundColor Cyan
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
Write-Host "[1/5] Converting IoStore/Zen packages to a legacy reference pak..." -ForegroundColor Cyan
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
Write-Host "[3/5] Extracting legacy cooked packages for inspection/export..." -ForegroundColor Cyan
& $UnrealPak $LegacyPak "-Extract=$Stage" 2>&1 | Tee-Object -FilePath $ExtractLog | Out-Host
if ($LASTEXITCODE -ne 0) {
    Write-Host "Primary UnrealPak extract syntax failed; trying alternate syntax..." -ForegroundColor Yellow
    & $UnrealPak $LegacyPak -Extract $Stage 2>&1 | Tee-Object -FilePath $ExtractLog -Append | Out-Host
}

$Uassets = @(Get-ChildItem $Stage -Recurse -Filter "*.uasset" -File -ErrorAction SilentlyContinue)
if ($Uassets.Count -eq 0) {
    Write-Host "Conversion completed but no .uasset packages appeared after extraction." -ForegroundColor Red
    Write-Host "Send me these two small logs:" -ForegroundColor Yellow
    Write-Host "  ReferenceExtracted\Logs\retoc_to_legacy.txt" -ForegroundColor Yellow
    Write-Host "  ReferenceExtracted\Logs\legacy_asset_list.txt" -ForegroundColor Yellow
    exit 21
}

Write-Host "Recovered $($Uassets.Count) cooked .uasset packages." -ForegroundColor Green

Write-Host ""
Write-Host "[4/5] Keeping cooked packages OUTSIDE DEADBRICK Content..." -ForegroundColor Cyan
Write-Host "Cooked game packages are not editor-native source assets." -ForegroundColor Yellow
Write-Host "They stay isolated in ReferenceExtracted until converted/exported to normal editor formats." -ForegroundColor Yellow
Write-Host "This prevents PACKAGE_FILE_TAG errors and broken materials/textures in Unreal Editor." -ForegroundColor Yellow

$MarkerFile = Join-Path $ProjectRoot "Saved\LOTL_REFERENCE_IMPORT.txt"
New-Item -ItemType Directory -Path (Split-Path -Parent $MarkerFile) -Force | Out-Null
@(
    "Extracted: $(Get-Date -Format o)",
    "PakDir: $PakDir",
    "Retoc: $RetocExe",
    "LegacyPak: $LegacyPak",
    "CookedUassetsRecovered: $($Uassets.Count)",
    "InstalledIntoContent: False",
    "Stage: $Stage"
) | Set-Content $MarkerFile

Write-Host ""
Write-Host "[5/5] Generating the precise reference manifest and rebuilding DEADBRICK..." -ForegroundColor Cyan
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
