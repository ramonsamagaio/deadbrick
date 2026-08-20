param(
    [switch]$Force
)

$ErrorActionPreference = "Stop"
$ProgressPreference = "SilentlyContinue"

$ProjectRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$PluginRoot = Join-Path $ProjectRoot "Plugins\UnrealPSKPSA"
$PluginDescriptor = Join-Path $PluginRoot "UnrealPSKPSA.uplugin"
$PrivateRoot = Join-Path $PluginRoot "Source\UnrealPSKPSA\Private"
$PsaFactoryCpp = Join-Path $PrivateRoot "PSAFactory.cpp"
$PskFactoryCpp = Join-Path $PrivateRoot "PSKFactory.cpp"
$BpflCpp = Join-Path $PrivateRoot "BPFL.cpp"
$Marker = Join-Path $ProjectRoot "Saved\LOTL_ACTORX_SETUP.txt"
$PhysXSetup = Join-Path $ProjectRoot "SETUP_PHYSX5_UE58.ps1"
$PhysXBridgeLib = Join-Path $ProjectRoot "ThirdParty\PhysX5\SDK\lib\Win64\DeadbrickPhysXBridge.lib"
$Repository = "https://github.com/djhaled/UnrealPSKPSA.git"
$PinnedCommit = "ec0e0a4624dd5e55fa9dcb235f6846f78bbeedb0"

function Find-Git {
    $Cmd = Get-Command git.exe -ErrorAction SilentlyContinue
    if ($Cmd) { return $Cmd.Source }
    foreach ($Candidate in @("$env:ProgramFiles\Git\cmd\git.exe", "$env:ProgramFiles\Git\bin\git.exe")) {
        if (Test-Path $Candidate) { return $Candidate }
    }
    return $null
}

function Test-PipelineReady {
    foreach ($Path in @($PluginDescriptor, $PsaFactoryCpp, $PskFactoryCpp, $BpflCpp)) {
        if (-not (Test-Path $Path)) { return $false }
    }

    $PsaText = Get-Content $PsaFactoryCpp -Raw
    $PskText = Get-Content $PskFactoryCpp -Raw
    $BpflText = Get-Content $BpflCpp -Raw
    return ($PsaText -match 'DEADBRICK_AUTOMATED_PSA') -and
           ($PskText -match 'DEADBRICK_UE58_COMPAT') -and
           ($BpflText -match 'DEADBRICK_UE58_COMPAT')
}

Write-Host ""
Write-Host "============================================================" -ForegroundColor Cyan
Write-Host " DEADBRICK - Lay of the Land ActorX asset pipeline" -ForegroundColor Cyan
Write-Host " PSK/PSKX skeletal meshes + PSA animations | UE 5.8" -ForegroundColor Cyan
Write-Host "============================================================" -ForegroundColor Cyan
Write-Host ""

# REBUILD_AND_OPEN historically checked only PhysX_64.dll. Existing machines therefore already have
# the NVIDIA SDK but not the new isolation bridge. Ensure the bridge here as well so one normal rebuild
# upgrades old checkouts automatically instead of silently compiling WITH_DEADBRICK_PHYSX5=0.
if (-not (Test-Path $PhysXBridgeLib)) {
    if (-not (Test-Path $PhysXSetup)) { throw "SETUP_PHYSX5_UE58.ps1 is missing; cannot build the PhysX isolation bridge." }
    Write-Host "PhysX isolation bridge is missing. Building it before the ActorX plugin..." -ForegroundColor Cyan
    & powershell.exe -NoProfile -ExecutionPolicy Bypass -File $PhysXSetup
    if ($LASTEXITCODE -ne 0 -or -not (Test-Path $PhysXBridgeLib)) {
        throw "PhysX isolation bridge setup failed with exit code $LASTEXITCODE."
    }
}

if ((Test-PipelineReady) -and -not $Force) {
    Write-Host "LOTL ActorX importer is already patched for UE 5.8 and unattended import." -ForegroundColor Green
    exit 0
}

$Git = Find-Git
if (-not $Git) { throw "Git was not found. Install Git for Windows or add git.exe to PATH." }

if (-not (Test-Path (Join-Path $PluginRoot ".git"))) {
    if (Test-Path $PluginRoot) { Remove-Item $PluginRoot -Recurse -Force }
    New-Item -ItemType Directory -Path (Split-Path -Parent $PluginRoot) -Force | Out-Null
    Write-Host "Cloning UnrealPSKPSA ActorX importer..." -ForegroundColor Cyan
    & $Git clone --depth 1 $Repository $PluginRoot
    if ($LASTEXITCODE -ne 0) { throw "UnrealPSKPSA clone failed with code $LASTEXITCODE." }
}

Write-Host "Pinning ActorX importer revision..." -ForegroundColor Cyan
& $Git -C $PluginRoot fetch --depth 1 origin $PinnedCommit
if ($LASTEXITCODE -ne 0) { throw "Could not fetch pinned UnrealPSKPSA revision." }
& $Git -C $PluginRoot checkout --force $PinnedCommit
if ($LASTEXITCODE -ne 0) { throw "Could not checkout pinned UnrealPSKPSA revision." }

foreach ($Path in @($PsaFactoryCpp, $PskFactoryCpp, $BpflCpp)) {
    if (-not (Test-Path $Path)) { throw "ActorX plugin source is missing: $Path" }
}

# Upstream always opens a modal skeleton picker, even when UAssetImportTask is marked automated.
# DEADBRICK sets SettingsImporter.Skeleton from Python, so skip the window when that skeleton exists.
$PsaText = Get-Content $PsaFactoryCpp -Raw
if ($PsaText -notmatch 'DEADBRICK_AUTOMATED_PSA') {
    $Old = "`tbool ks = IsAutomatedImport();"
    $New = @"
`t// DEADBRICK_AUTOMATED_PSA: unattended CUE4Parse PSA import supplies the skeleton on the factory.
`tconst bool bAutomatedImport = IsAutomatedImport();
`tif (bAutomatedImport && SettingsImporter && SettingsImporter->Skeleton)
`t{
`t`tSettingsImporter->bInitialized = true;
`t`tbImport = true;
`t`tbImportAll = true;
`t}
"@
    if (-not $PsaText.Contains($Old)) { throw "Unexpected PSAFactory.cpp layout. Refusing to patch an unknown importer revision." }
    $PsaText = $PsaText.Replace($Old, $New.TrimEnd())

    $SkeletonLine = "`tUSkeleton* Skeleton = SettingsImporter->Skeleton;"
    $SkeletonGuard = @"
`tif (!SettingsImporter || !SettingsImporter->Skeleton)
`t{
`t`tUE_LOG(LogTemp, Error, TEXT("UnrealPSKPSA: automated PSA import has no skeleton."));
`t`treturn nullptr;
`t}
`tUSkeleton* Skeleton = SettingsImporter->Skeleton;
"@
    if (-not $PsaText.Contains($SkeletonLine)) { throw "Could not locate PSA skeleton assignment for automation patch." }
    $PsaText = $PsaText.Replace($SkeletonLine, $SkeletonGuard.TrimEnd())

    # Upstream logs every animation key at Warning level. Large LOTL animation sets become painfully slow.
    $PsaText = $PsaText -replace '\s*UE_LOG\(LogTemp, Warning, TEXT\(" Position %s"\), \*AnimKey\.Position\.ToString\(\)\);', ''
    Set-Content $PsaFactoryCpp -Value $PsaText -Encoding UTF8
}

# UE 5.8 compatibility. Epic moved FStaticMeshComponentLODInfo to its own public header,
# UBodySetup requires its concrete header, CreatePackage is now the one-argument API, and
# the old renderer line accidentally used comparison instead of assignment.
$BpflText = Get-Content $BpflCpp -Raw
if ($BpflText -notmatch 'DEADBRICK_UE58_COMPAT') {
    if ($BpflText -notmatch '#include\s+"StaticMeshComponentLODInfo\.h"') {
        $BpflText = $BpflText.Replace(
            '#include "Components/StaticMeshComponent.h"',
            "#include `"Components/StaticMeshComponent.h`"`r`n#include `"StaticMeshComponentLODInfo.h`"`r`n#include `"PhysicsEngine/BodySetup.h`"")
    }

    $BpflText = $BpflText -replace 'Settings->Reflections\s*==\s*EReflectionMethod::None\s*;', 'Settings->Reflections = EReflectionMethod::None;'
    $BpflText = $BpflText -replace 'CreatePackage\(nullptr\s*,\s*\*PathForTextures\)', 'CreatePackage(*PathForTextures)'
    $BpflText = "// DEADBRICK_UE58_COMPAT: patched automatically for Unreal Engine 5.8.`r`n" + $BpflText
    Set-Content $BpflCpp -Value $BpflText -Encoding UTF8
}

# UE 5.8 removed these two legacy FSkeletalMeshImportData flags. Their old values here were both false,
# so removing the assignments preserves the intended behavior while using the current importer structure.
$PskText = Get-Content $PskFactoryCpp -Raw
if ($PskText -notmatch 'DEADBRICK_UE58_COMPAT') {
    $PskText = $PskText -replace '(?m)^\s*SkeletalMeshImportData\.bDiffPose\s*=\s*false;\s*\r?\n', ''
    $PskText = $PskText -replace '(?m)^\s*SkeletalMeshImportData\.bUseT0AsRefPose\s*=\s*false;\s*\r?\n', ''
    $PskText = "// DEADBRICK_UE58_COMPAT: removed legacy FSkeletalMeshImportData fields for Unreal Engine 5.8.`r`n" + $PskText
    Set-Content $PskFactoryCpp -Value $PskText -Encoding UTF8
}

if (-not (Test-PipelineReady)) { throw "ActorX UE 5.8 compatibility/automation patch did not validate." }

New-Item -ItemType Directory -Path (Split-Path -Parent $Marker) -Force | Out-Null
@(
    "DEADBRICK LOTL ActorX importer",
    "Prepared: $(Get-Date -Format o)",
    "Repository: $Repository",
    "PinnedCommit: $PinnedCommit",
    "AutomatedPSA: true",
    "UE58Compat: true",
    "Plugin: $PluginRoot"
) | Set-Content $Marker -Encoding UTF8

Write-Host "LOTL ActorX importer ready for Unreal Engine 5.8." -ForegroundColor Green
exit 0
