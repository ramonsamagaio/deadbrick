param(
    [switch]$Force
)

$ErrorActionPreference = "Stop"
$ProgressPreference = "SilentlyContinue"

$ProjectRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$PluginRoot = Join-Path $ProjectRoot "Plugins\UnrealPSKPSA"
$PluginDescriptor = Join-Path $PluginRoot "UnrealPSKPSA.uplugin"
$PsaFactoryCpp = Join-Path $PluginRoot "Source\UnrealPSKPSA\Private\PSAFactory.cpp"
$Marker = Join-Path $ProjectRoot "Saved\LOTL_ACTORX_SETUP.txt"
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
    if (-not (Test-Path $PluginDescriptor)) { return $false }
    if (-not (Test-Path $PsaFactoryCpp)) { return $false }
    return (Get-Content $PsaFactoryCpp -Raw) -match 'DEADBRICK_AUTOMATED_PSA'
}

Write-Host ""
Write-Host "============================================================" -ForegroundColor Cyan
Write-Host " DEADBRICK - Lay of the Land ActorX asset pipeline" -ForegroundColor Cyan
Write-Host " PSK/PSKX skeletal meshes + PSA animations" -ForegroundColor Cyan
Write-Host "============================================================" -ForegroundColor Cyan
Write-Host ""

if ((Test-PipelineReady) -and -not $Force) {
    Write-Host "LOTL ActorX importer is already installed and patched for unattended import." -ForegroundColor Green
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

if (-not (Test-Path $PsaFactoryCpp)) { throw "PSAFactory.cpp is missing from the ActorX plugin." }
$Text = Get-Content $PsaFactoryCpp -Raw

# Upstream always opens a modal skeleton picker, even when UAssetImportTask is marked automated.
# DEADBRICK sets SettingsImporter.Skeleton from Python, so skip the window when that skeleton exists.
if ($Text -notmatch 'DEADBRICK_AUTOMATED_PSA') {
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
    if (-not $Text.Contains($Old)) { throw "Unexpected PSAFactory.cpp layout. Refusing to patch an unknown importer revision." }
    $Text = $Text.Replace($Old, $New.TrimEnd())

    $SkeletonLine = "`tUSkeleton* Skeleton = SettingsImporter->Skeleton;"
    $SkeletonGuard = @"
`tif (!SettingsImporter || !SettingsImporter->Skeleton)
`t{
`t`tUE_LOG(LogTemp, Error, TEXT("UnrealPSKPSA: automated PSA import has no skeleton."));
`t`treturn nullptr;
`t}
`tUSkeleton* Skeleton = SettingsImporter->Skeleton;
"@
    if (-not $Text.Contains($SkeletonLine)) { throw "Could not locate PSA skeleton assignment for automation patch." }
    $Text = $Text.Replace($SkeletonLine, $SkeletonGuard.TrimEnd())

    # Upstream logs every animation key at Warning level. Large LOTL animation sets become painfully slow.
    $Text = $Text -replace '\s*UE_LOG\(LogTemp, Warning, TEXT\(" Position %s"\), \*AnimKey\.Position\.ToString\(\)\);', ''
    Set-Content $PsaFactoryCpp -Value $Text -Encoding UTF8
}

if (-not (Test-PipelineReady)) { throw "ActorX plugin automation patch did not validate." }

New-Item -ItemType Directory -Path (Split-Path -Parent $Marker) -Force | Out-Null
@(
    "DEADBRICK LOTL ActorX importer",
    "Prepared: $(Get-Date -Format o)",
    "Repository: $Repository",
    "PinnedCommit: $PinnedCommit",
    "AutomatedPSA: true",
    "Plugin: $PluginRoot"
) | Set-Content $Marker -Encoding UTF8

Write-Host "LOTL ActorX importer ready." -ForegroundColor Green
exit 0
