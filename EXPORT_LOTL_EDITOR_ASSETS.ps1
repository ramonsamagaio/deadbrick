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
Write-Host " DEADBRICK - LOTL editor-safe export" -ForegroundColor Cyan
Write-Host " static GLTF + skeletal PSK + animation PSA + metadata JSON" -ForegroundColor Cyan
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
    $Asset = $Release.assets | Where-Object { $_.name -match 'CUE4Parse\.CLI-.*-Win64-bin\.zip$' } | Select-Object -First 1
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

function New-CommonArgs {
    $Args = New-Object System.Collections.Generic.List[string]
    $Args.Add('--pak'); $Args.Add($LegacyPak)
    $Args.Add('-g'); $Args.Add('GAME_UE5_LATEST')
    if ($Mappings) { $Args.Add('-m'); $Args.Add($Mappings.FullName) }
    return $Args
}

function Invoke-PatternExport([string]$Label, [string]$Pattern, [string[]]$ExtraArgs) {
    Write-Host "  $Label $Pattern" -ForegroundColor DarkCyan
    $Args = New-CommonArgs
    $Args.Add('-o'); $Args.Add($ExportRoot)
    foreach ($Arg in $ExtraArgs) { $Args.Add($Arg) }
    $Args.Add('-y')
    $Args.Add('-p'); $Args.Add($Pattern)

    "=== $Label $Pattern ===" | Add-Content $ExportLog
    & $Cue4Parse @Args 2>&1 | Add-Content $ExportLog
    return $LASTEXITCODE
}

Install-Cue4Parse
Write-Host "CUE4Parse: $Cue4Parse" -ForegroundColor DarkGray

$Mappings = Get-ChildItem $ReferenceRoot -Recurse -Filter "*.usmap" -File -ErrorAction SilentlyContinue | Select-Object -First 1
if ($Mappings) {
    Write-Host "Mappings: $($Mappings.FullName)" -ForegroundColor Green
} else {
    Write-Host "No .usmap mapping found. Mesh/texture/animation exports will still be attempted." -ForegroundColor DarkYellow
    Write-Host "Blueprint defaults that require unversioned property mappings may remain unavailable." -ForegroundColor DarkYellow
}

$StaticPatterns = @(
    '*SM_*', '*StaticMesh*',
    '*Door*', '*Window*', '*Glass*', '*Container*', '*Crate*', '*Chest*', '*Locker*', '*Cabinet*', '*Barrel*',
    '*Weapon*', '*Gun*', '*Rifle*', '*Pistol*', '*Shotgun*', '*Ammo*',
    '*Road*', '*Asphalt*', '*Brick*', '*Concrete*', '*Wood*', '*Metal*',
    '*Furniture*', '*Chair*', '*Table*', '*Shelf*', '*Bed*', '*Desk*', '*Prop*'
)

$SkeletalPatterns = @(
    '*SK_*', '*SkeletalMesh*', '*Character*', '*Player*', '*Human*', '*Enemy*', '*Zombie*', '*Creature*',
    '*FirstPerson*', '*First_Person*', '*Hand*', '*Hands*', '*Arm*', '*Arms*'
)

$AnimationPatterns = @(
    '*Anim*', '*Animation*', '*Locomotion*',
    '*Idle*', '*Walk*', '*Run*', '*Sprint*', '*Crouch*', '*Jump*', '*Fall*', '*Land*',
    '*Attack*', '*Melee*', '*Hit*', '*Damage*', '*Death*', '*Die*',
    '*Fire*', '*Shoot*', '*Reload*', '*Equip*', '*Unequip*', '*Aim*', '*Interact*',
    '*FirstPerson*', '*First_Person*'
)

$MetadataPatterns = @(
    '*MainPlayerController*', '*Player*', '*Character*', '*Movement*', '*Locomotion*',
    '*MainGameMode*', '*GameInstance*', '*AIManager*', '*VoxelManager*',
    '*MainVoxel*', '*PhysicsVoxel*', '*PropVoxel*', '*SimulationVoxel*',
    '*ItemManager*', '*SaveManager*', '*AmbienceManager*', '*StructureManager*',
    '*RoadGenerator*', '*VoxelItemComponent*', '*VoxelPhysics*',
    '*BuildingManager*', '*StructureEditor*', '*Craft*', '*Recipe*', '*Inventory*'
)

if (Test-Path $ExportRoot) {
    Get-ChildItem $ExportRoot -Force -ErrorAction SilentlyContinue | Remove-Item -Recurse -Force -ErrorAction SilentlyContinue
}
New-Item -ItemType Directory -Path $ExportRoot -Force | Out-Null
Remove-Item $ExportLog,$PackageListLog -Force -ErrorAction SilentlyContinue

$AllPatterns = @($StaticPatterns + $SkeletalPatterns + $AnimationPatterns + $MetadataPatterns | Select-Object -Unique)
Write-Host ""
Write-Host "[A] Listing candidate package families..." -ForegroundColor Cyan
foreach ($Pattern in $AllPatterns) {
    $Args = New-CommonArgs
    $Args.Add('-p'); $Args.Add($Pattern)
    $Args.Add('-l')
    "=== $Pattern ===" | Add-Content $PackageListLog
    & $Cue4Parse @Args 2>&1 | Add-Content $PackageListLog
}
Write-Host "Package inventory: $PackageListLog" -ForegroundColor DarkGray

$StaticFailures = 0
Write-Host ""
Write-Host "[B] Exporting static props/buildings/weapons as GLTF..." -ForegroundColor Cyan
foreach ($Pattern in $StaticPatterns) {
    $Exit = Invoke-PatternExport 'STATIC' $Pattern @('--mesh-format','Gltf2','--texture-format','Png','--lod-format','FirstLod')
    if ($Exit -ne 0) { $StaticFailures++ }
}

$SkeletalFailures = 0
Write-Host ""
Write-Host "[C] Exporting skeletal characters/arms as ActorX PSK..." -ForegroundColor Cyan
foreach ($Pattern in $SkeletalPatterns) {
    $Exit = Invoke-PatternExport 'SKELETAL' $Pattern @('--mesh-format','ActorX','--anim-format','ActorX','--texture-format','Png','--lod-format','FirstLod')
    if ($Exit -ne 0) { $SkeletalFailures++ }
}

$AnimationFailures = 0
Write-Host ""
Write-Host "[D] Exporting animation families as ActorX PSA..." -ForegroundColor Cyan
foreach ($Pattern in $AnimationPatterns) {
    $Exit = Invoke-PatternExport 'ANIMATION' $Pattern @('--mesh-format','ActorX','--anim-format','ActorX','--texture-format','Png','--lod-format','FirstLod')
    if ($Exit -ne 0) { $AnimationFailures++ }
}

$MetadataFailures = 0
Write-Host ""
Write-Host "[E] Exporting gameplay/default-object metadata independently..." -ForegroundColor Cyan
foreach ($Pattern in $MetadataPatterns) {
    $Exit = Invoke-PatternExport 'METADATA' $Pattern @('-f','json')
    if ($Exit -ne 0) { $MetadataFailures++ }
}

$Glb = @(Get-ChildItem $ExportRoot -Recurse -Include '*.glb','*.gltf' -File -ErrorAction SilentlyContinue)
$Psk = @(Get-ChildItem $ExportRoot -Recurse -Include '*.psk','*.pskx' -File -ErrorAction SilentlyContinue)
$Psa = @(Get-ChildItem $ExportRoot -Recurse -Filter '*.psa' -File -ErrorAction SilentlyContinue)
$Textures = @(Get-ChildItem $ExportRoot -Recurse -Include '*.png','*.tga','*.jpg','*.jpeg' -File -ErrorAction SilentlyContinue)
$Json = @(Get-ChildItem $ExportRoot -Recurse -Filter '*.json' -File -ErrorAction SilentlyContinue)

$Marker = Join-Path $ProjectRoot "Saved\LOTL_REFERENCE_EXPORT.txt"
New-Item -ItemType Directory -Path (Split-Path -Parent $Marker) -Force | Out-Null
@(
    "Exported: $(Get-Date -Format o)",
    "LegacyPak: $LegacyPak",
    "Cue4Parse: $Cue4Parse",
    "Mappings: $(if ($Mappings) { $Mappings.FullName } else { '<none>' })",
    "StaticPatternFailures: $StaticFailures",
    "SkeletalPatternFailures: $SkeletalFailures",
    "AnimationPatternFailures: $AnimationFailures",
    "MetadataPatternFailures: $MetadataFailures",
    "GLTFStaticMeshes: $($Glb.Count)",
    "ActorXSkeletalMeshes: $($Psk.Count)",
    "ActorXAnimations: $($Psa.Count)",
    "Textures: $($Textures.Count)",
    "GameplayMetadataJson: $($Json.Count)",
    "ExportRoot: $ExportRoot",
    "Log: $ExportLog",
    "PackageList: $PackageListLog"
) | Set-Content $Marker -Encoding UTF8

Write-Host ""
Write-Host "LOTL export result:" -ForegroundColor Cyan
Write-Host "  static GLB/GLTF : $($Glb.Count)" -ForegroundColor Green
Write-Host "  skeletal PSK    : $($Psk.Count)" -ForegroundColor Green
Write-Host "  animation PSA   : $($Psa.Count)" -ForegroundColor Green
Write-Host "  textures        : $($Textures.Count)" -ForegroundColor Green
Write-Host "  gameplay JSON   : $($Json.Count)" -ForegroundColor Green
Write-Host "  pattern errors  : static=$StaticFailures skeletal=$SkeletalFailures anim=$AnimationFailures metadata=$MetadataFailures" -ForegroundColor DarkYellow

if ($Glb.Count -eq 0 -and $Psk.Count -eq 0 -and $Psa.Count -eq 0 -and $Textures.Count -eq 0 -and $Json.Count -eq 0) {
    Write-Host "No editor-safe LOTL data was decoded. Check package inventory/export logs." -ForegroundColor Red
    Write-Host $PackageListLog -ForegroundColor Yellow
    Write-Host $ExportLog -ForegroundColor Yellow
    exit 30
}

if ($Psk.Count -eq 0) {
    Write-Host "WARNING: no ActorX skeletal mesh was decoded, so character/arm skins cannot be recreated yet." -ForegroundColor Yellow
}
if ($Psa.Count -eq 0) {
    Write-Host "WARNING: no ActorX animation was decoded, so original locomotion/combat animations cannot be recreated yet." -ForegroundColor Yellow
}
if ($Json.Count -eq 0) {
    Write-Host "No Blueprint/default metadata decoded. Unversioned properties may require a .usmap mapping." -ForegroundColor Yellow
}

exit 0
