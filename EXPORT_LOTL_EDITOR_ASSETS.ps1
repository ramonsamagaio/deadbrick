param(
    [string]$LegacyPak = "",
    [switch]$ForceDownload
)

$ErrorActionPreference = "Stop"
$ProgressPreference = "SilentlyContinue"

$ProjectRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$ReferenceRoot = Join-Path $ProjectRoot "ReferenceExtracted"
$DefaultLegacyPak = Join-Path $ReferenceRoot "Legacy\LayOfTheLand_Legacy.pak"
$ExportRoot = Join-Path $ProjectRoot "ReferenceExported"
$ToolRoot = Join-Path $ProjectRoot "Tools\cue4parse"
$Cue4Parse = Join-Path $ToolRoot "cue4parse.exe"
$LogRoot = Join-Path $ReferenceRoot "Logs"
$ExportLog = Join-Path $LogRoot "cue4parse_priority_export.txt"

if ([string]::IsNullOrWhiteSpace($LegacyPak)) { $LegacyPak = $DefaultLegacyPak }
$LegacyPak = [System.IO.Path]::GetFullPath($LegacyPak)

Write-Host ""
Write-Host "============================================================" -ForegroundColor Cyan
Write-Host " DEADBRICK - LOTL editor-safe asset export" -ForegroundColor Cyan
Write-Host " cooked reference pak -> GLB/PNG (NO cooked uasset copy)" -ForegroundColor Cyan
Write-Host "============================================================" -ForegroundColor Cyan
Write-Host ""

if (-not (Test-Path $LegacyPak)) {
    Write-Host "Legacy reference pak not found:" -ForegroundColor Red
    Write-Host $LegacyPak -ForegroundColor Yellow
    Write-Host "Run IMPORT_LOTL_REFERENCE_UE58.bat first." -ForegroundColor Yellow
    exit 20
}

New-Item -ItemType Directory -Path $ToolRoot,$ExportRoot,$LogRoot -Force | Out-Null

function Install-Cue4Parse {
    if ((Test-Path $Cue4Parse) -and -not $ForceDownload) { return }

    Write-Host "Downloading current CUE4Parse.CLI Win64 exporter..." -ForegroundColor Cyan
    $Headers = @{ "User-Agent" = "DEADBRICK-reference-exporter" }
    $Release = Invoke-RestMethod -Headers $Headers -Uri "https://api.github.com/repos/joric/CUE4Parse.CLI/releases/latest"
    $Asset = $Release.assets | Where-Object {
        $_.name -match 'CUE4Parse\.CLI-.*-Win64-bin\.zip$'
    } | Select-Object -First 1

    if (-not $Asset) {
        $Asset = $Release.assets | Where-Object { $_.name -match 'Win64.*\.zip$|Windows.*\.zip$' } | Select-Object -First 1
    }
    if (-not $Asset) { throw "Could not locate the Win64 CUE4Parse.CLI release archive." }

    $Zip = Join-Path $ToolRoot $Asset.name
    Invoke-WebRequest -Headers $Headers -Uri $Asset.browser_download_url -OutFile $Zip

    Get-ChildItem $ToolRoot -File -ErrorAction SilentlyContinue | Where-Object { $_.Name -ne $Zip } | Remove-Item -Force -ErrorAction SilentlyContinue
    Expand-Archive -Path $Zip -DestinationPath $ToolRoot -Force
    Remove-Item $Zip -Force -ErrorAction SilentlyContinue

    $Found = Get-ChildItem $ToolRoot -Recurse -Filter "cue4parse.exe" -File | Select-Object -First 1
    if (-not $Found) { throw "cue4parse.exe was not present in the downloaded release." }
    if ($Found.FullName -ne $Cue4Parse) { Copy-Item $Found.FullName $Cue4Parse -Force }
}

Install-Cue4Parse
Write-Host "CUE4Parse: $Cue4Parse" -ForegroundColor DarkGray

$Mappings = Get-ChildItem $ReferenceRoot -Recurse -Filter "*.usmap" -File -ErrorAction SilentlyContinue | Select-Object -First 1
if ($Mappings) {
    Write-Host "Mappings: $($Mappings.FullName)" -ForegroundColor DarkGray
} else {
    Write-Host "No .usmap mapping found. Exporter will use package metadata that can be decoded without mappings." -ForegroundColor DarkYellow
}

# Deliberately narrow first pass. These patterns target gameplay-facing meshes/materials that
# DEADBRICK can bind immediately instead of dumping thousands of irrelevant cooked assets.
$Patterns = @(
    '*FirstPerson*',
    '*First_Person*',
    '*Player*',
    '*Character*',
    '*Human*',
    '*Hand*',
    '*Arm*',
    '*Weapon*',
    '*Gun*',
    '*Rifle*',
    '*Pistol*',
    '*Shotgun*',
    '*Door*',
    '*Window*',
    '*Glass*',
    '*Container*',
    '*Crate*',
    '*Chest*',
    '*Locker*',
    '*Cabinet*',
    '*Barrel*',
    '*Road*',
    '*Asphalt*',
    '*Brick*',
    '*Concrete*',
    '*Wood*',
    '*Metal*',
    '*Soil*',
    '*Dirt*'
)

if (Test-Path $ExportRoot) {
    Get-ChildItem $ExportRoot -Force -ErrorAction SilentlyContinue | Remove-Item -Recurse -Force -ErrorAction SilentlyContinue
}
New-Item -ItemType Directory -Path $ExportRoot -Force | Out-Null
Remove-Item $ExportLog -Force -ErrorAction SilentlyContinue

$Args = New-Object System.Collections.Generic.List[string]
$Args.Add('--pak'); $Args.Add($LegacyPak)
$Args.Add('-o'); $Args.Add($ExportRoot)
$Args.Add('-g'); $Args.Add('GAME_UE5_LATEST')
$Args.Add('--mesh-format'); $Args.Add('Gltf2')
$Args.Add('--texture-format'); $Args.Add('Png')
$Args.Add('--lod-format'); $Args.Add('FirstLod')
$Args.Add('-y')
if ($Mappings) {
    $Args.Add('-m'); $Args.Add($Mappings.FullName)
}
foreach ($Pattern in $Patterns) {
    $Args.Add('-p'); $Args.Add($Pattern)
}

Write-Host ""
Write-Host "Exporting priority LOTL meshes/material textures..." -ForegroundColor Cyan
& $Cue4Parse @Args 2>&1 | Tee-Object -FilePath $ExportLog | Out-Host
$ExitCode = $LASTEXITCODE

$Glb = @(Get-ChildItem $ExportRoot -Recurse -Include '*.glb','*.gltf' -File -ErrorAction SilentlyContinue)
$Textures = @(Get-ChildItem $ExportRoot -Recurse -Include '*.png','*.tga','*.jpg','*.jpeg' -File -ErrorAction SilentlyContinue)
$ActorX = @(Get-ChildItem $ExportRoot -Recurse -Include '*.psk','*.pskx','*.psa' -File -ErrorAction SilentlyContinue)

$Marker = Join-Path $ProjectRoot "Saved\LOTL_REFERENCE_EXPORT.txt"
New-Item -ItemType Directory -Path (Split-Path -Parent $Marker) -Force | Out-Null
@(
    "Exported: $(Get-Date -Format o)",
    "LegacyPak: $LegacyPak",
    "Cue4Parse: $Cue4Parse",
    "ExitCode: $ExitCode",
    "GLTFMeshes: $($Glb.Count)",
    "Textures: $($Textures.Count)",
    "ActorXFiles: $($ActorX.Count)",
    "ExportRoot: $ExportRoot",
    "Log: $ExportLog"
) | Set-Content $Marker

Write-Host ""
Write-Host "Editor-safe export result:" -ForegroundColor Cyan
Write-Host "  GLB/GLTF meshes : $($Glb.Count)" -ForegroundColor Green
Write-Host "  textures        : $($Textures.Count)" -ForegroundColor Green
if ($ActorX.Count -gt 0) { Write-Host "  ActorX anim/mesh: $($ActorX.Count)" -ForegroundColor DarkGray }

if ($ExitCode -ne 0 -and $Glb.Count -eq 0 -and $Textures.Count -eq 0) {
    Write-Host "CUE4Parse could not decode the priority assets. Send ReferenceExtracted\Logs\cue4parse_priority_export.txt." -ForegroundColor Red
    exit 30
}

if ($Glb.Count -eq 0 -and $Textures.Count -eq 0) {
    Write-Host "No priority GLB/PNG assets were produced yet. The package list/mappings need another pass." -ForegroundColor Yellow
    exit 31
}

Write-Host ""
Write-Host "ReferenceExported is ready for Unreal's normal importer." -ForegroundColor Green
exit 0
