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
$PackageListLog = Join-Path $LogRoot "cue4parse_priority_packages.txt"

if ([string]::IsNullOrWhiteSpace($LegacyPak)) { $LegacyPak = $DefaultLegacyPak }
$LegacyPak = [System.IO.Path]::GetFullPath($LegacyPak)

Write-Host ""
Write-Host "============================================================" -ForegroundColor Cyan
Write-Host " DEADBRICK - LOTL editor-safe asset/metadata export" -ForegroundColor Cyan
Write-Host " cooked reference pak -> visuals first, metadata separately" -ForegroundColor Cyan
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
    Get-ChildItem $ToolRoot -File -ErrorAction SilentlyContinue | Where-Object { $_.FullName -ne $Zip } | Remove-Item -Force -ErrorAction SilentlyContinue
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
    Write-Host "Mappings: $($Mappings.FullName)" -ForegroundColor Green
} else {
    Write-Host "No .usmap mapping found. Visual exports will still be attempted independently." -ForegroundColor DarkYellow
    Write-Host "Blueprint defaults that require unversioned property mappings may remain unavailable until a mapping is present." -ForegroundColor DarkYellow
}

$VisualPatterns = @(
    '*SK_*', '*SM_*', '*SkeletalMesh*', '*StaticMesh*',
    '*FirstPerson*', '*First_Person*', '*Hand*', '*Arm*',
    '*Weapon*', '*Gun*', '*Rifle*', '*Pistol*', '*Shotgun*',
    '*Door*', '*Window*', '*Glass*', '*Container*', '*Crate*',
    '*Chest*', '*Locker*', '*Cabinet*', '*Barrel*',
    '*Road*', '*Asphalt*', '*Brick*', '*Concrete*', '*Wood*', '*Metal*'
)

$MetadataPatterns = @(
    '*MainPlayerController*', '*Player*', '*Character*', '*Movement*', '*Locomotion*',
    '*MainGameMode*', '*GameInstance*', '*AIManager*', '*VoxelManager*',
    '*MainVoxel*', '*PhysicsVoxel*', '*PropVoxel*', '*SimulationVoxel*',
    '*ItemManager*', '*SaveManager*', '*AmbienceManager*', '*StructureManager*',
    '*RoadGenerator*', '*VoxelItemComponent*', '*VoxelPhysics*'
)

if (Test-Path $ExportRoot) {
    Get-ChildItem $ExportRoot -Force -ErrorAction SilentlyContinue | Remove-Item -Recurse -Force -ErrorAction SilentlyContinue
}
New-Item -ItemType Directory -Path $ExportRoot -Force | Out-Null
Remove-Item $ExportLog,$PackageListLog -Force -ErrorAction SilentlyContinue

$Common = New-Object System.Collections.Generic.List[string]
$Common.Add('--pak'); $Common.Add($LegacyPak)
$Common.Add('-g'); $Common.Add('GAME_UE5_LATEST')
if ($Mappings) { $Common.Add('-m'); $Common.Add($Mappings.FullName) }

Write-Host ""
Write-Host "[A] Listing candidate package families..." -ForegroundColor Cyan
foreach ($Pattern in ($VisualPatterns + $MetadataPatterns | Select-Object -Unique)) {
    $ListArgs = New-Object System.Collections.Generic.List[string]
    foreach ($Arg in $Common) { $ListArgs.Add($Arg) }
    $ListArgs.Add('-p'); $ListArgs.Add($Pattern)
    $ListArgs.Add('-l')
    "=== $Pattern ===" | Add-Content $PackageListLog
    & $Cue4Parse @ListArgs 2>&1 | Add-Content $PackageListLog
}

Write-Host "Package inventory: $PackageListLog" -ForegroundColor DarkGray
Write-Host ""
Write-Host "[B] Exporting visual families independently..." -ForegroundColor Cyan

$VisualFailures = 0
foreach ($Pattern in $VisualPatterns) {
    Write-Host "  visual $Pattern" -ForegroundColor DarkCyan
    $Args = New-Object System.Collections.Generic.List[string]
    foreach ($Arg in $Common) { $Args.Add($Arg) }
    $Args.Add('-o'); $Args.Add($ExportRoot)
    $Args.Add('--mesh-format'); $Args.Add('Gltf2')
    $Args.Add('--texture-format'); $Args.Add('Png')
    $Args.Add('--lod-format'); $Args.Add('FirstLod')
    $Args.Add('-y')
    $Args.Add('-p'); $Args.Add($Pattern)

    "=== VISUAL $Pattern ===" | Add-Content $ExportLog
    & $Cue4Parse @Args 2>&1 | Add-Content $ExportLog
    if ($LASTEXITCODE -ne 0) { $VisualFailures++ }
}

Write-Host ""
Write-Host "[C] Exporting gameplay/default-object metadata independently..." -ForegroundColor Cyan
$MetadataFailures = 0
foreach ($Pattern in $MetadataPatterns) {
    Write-Host "  metadata $Pattern" -ForegroundColor DarkCyan
    $Args = New-Object System.Collections.Generic.List[string]
    foreach ($Arg in $Common) { $Args.Add($Arg) }
    $Args.Add('-o'); $Args.Add($ExportRoot)
    $Args.Add('-f'); $Args.Add('json')
    $Args.Add('-y')
    $Args.Add('-p'); $Args.Add($Pattern)

    "=== METADATA $Pattern ===" | Add-Content $ExportLog
    & $Cue4Parse @Args 2>&1 | Add-Content $ExportLog
    if ($LASTEXITCODE -ne 0) { $MetadataFailures++ }
}

$Glb = @(Get-ChildItem $ExportRoot -Recurse -Include '*.glb','*.gltf' -File -ErrorAction SilentlyContinue)
$Textures = @(Get-ChildItem $ExportRoot -Recurse -Include '*.png','*.tga','*.jpg','*.jpeg' -File -ErrorAction SilentlyContinue)
$ActorX = @(Get-ChildItem $ExportRoot -Recurse -Include '*.psk','*.pskx','*.psa' -File -ErrorAction SilentlyContinue)
$Json = @(Get-ChildItem $ExportRoot -Recurse -Filter '*.json' -File -ErrorAction SilentlyContinue)

$Marker = Join-Path $ProjectRoot "Saved\LOTL_REFERENCE_EXPORT.txt"
New-Item -ItemType Directory -Path (Split-Path -Parent $Marker) -Force | Out-Null
@(
    "Exported: $(Get-Date -Format o)",
    "LegacyPak: $LegacyPak",
    "Cue4Parse: $Cue4Parse",
    "Mappings: $(if ($Mappings) { $Mappings.FullName } else { '<none>' })",
    "VisualPatternFailures: $VisualFailures",
    "MetadataPatternFailures: $MetadataFailures",
    "GLTFMeshes: $($Glb.Count)",
    "Textures: $($Textures.Count)",
    "ActorXFiles: $($ActorX.Count)",
    "GameplayMetadataJson: $($Json.Count)",
    "ExportRoot: $ExportRoot",
    "Log: $ExportLog",
    "PackageList: $PackageListLog"
) | Set-Content $Marker

Write-Host ""
Write-Host "LOTL export result:" -ForegroundColor Cyan
Write-Host "  GLB/GLTF meshes       : $($Glb.Count)" -ForegroundColor Green
Write-Host "  textures              : $($Textures.Count)" -ForegroundColor Green
Write-Host "  gameplay JSON         : $($Json.Count)" -ForegroundColor Green
Write-Host "  visual pattern errors : $VisualFailures" -ForegroundColor DarkYellow
Write-Host "  metadata errors       : $MetadataFailures" -ForegroundColor DarkYellow

if ($Glb.Count -eq 0 -and $Textures.Count -eq 0 -and $Json.Count -eq 0) {
    Write-Host "No editor-safe LOTL data was decoded. Check the package inventory and export log." -ForegroundColor Red
    Write-Host $PackageListLog -ForegroundColor Yellow
    Write-Host $ExportLog -ForegroundColor Yellow
    exit 30
}

if ($Glb.Count -eq 0 -and $Textures.Count -eq 0) {
    Write-Host "Gameplay metadata was recovered, but no visual references were decoded." -ForegroundColor Yellow
} else {
    Write-Host "Visual references are ready for Unreal's normal importer." -ForegroundColor Green
}

if ($Json.Count -eq 0) {
    Write-Host "No Blueprint/default metadata decoded. If the packages are unversioned, a .usmap mapping is required for exact property recovery." -ForegroundColor Yellow
}

exit 0
