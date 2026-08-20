param(
    [switch]$Force
)

$ErrorActionPreference = "Stop"
$ProgressPreference = "SilentlyContinue"

$ProjectRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$ThirdPartyRoot = Join-Path $ProjectRoot "ThirdParty\PhysX5"
$SourceRoot = Join-Path $ThirdPartyRoot "_src"
$SdkRoot = Join-Path $ThirdPartyRoot "SDK"
$IncludeRoot = Join-Path $SdkRoot "include"
$LibRoot = Join-Path $SdkRoot "lib\Win64"
$BinRoot = Join-Path $SdkRoot "bin\Win64"
$Marker = Join-Path $ProjectRoot "Saved\PHYSX5_SETUP.txt"
$PhysXRepository = "https://github.com/NVIDIA-Omniverse/PhysX.git"
$PhysXTag = "110.0-omni-and-physx-5.8.0"
$PresetName = "vc17win64-cpu-only"

# Keep the bootstrap independent from optional Visual Studio components. CMake 3.31.x is new enough
# for PhysX 5.8 but avoids future CMake 4.x policy breakage in NVIDIA's project generator.
$PortableCMakeVersion = "3.31.10"
$PortableToolsRoot = Join-Path $ProjectRoot "Tools\cmake"

$RequiredLibs = @(
    "PhysX_64.lib",
    "PhysXCommon_64.lib",
    "PhysXFoundation_64.lib",
    "PhysXExtensions_static_64.lib"
)
$RequiredDlls = @(
    "PhysX_64.dll",
    "PhysXCommon_64.dll",
    "PhysXFoundation_64.dll"
)

function Test-SdkReady {
    if (-not (Test-Path (Join-Path $IncludeRoot "PxPhysicsAPI.h"))) { return $false }
    foreach ($Name in $RequiredLibs) { if (-not (Test-Path (Join-Path $LibRoot $Name))) { return $false } }
    foreach ($Name in $RequiredDlls) { if (-not (Test-Path (Join-Path $BinRoot $Name))) { return $false } }
    return $true
}

function Find-Executable([string]$Name, [string[]]$Candidates) {
    $Cmd = Get-Command $Name -ErrorAction SilentlyContinue
    if ($Cmd) { return $Cmd.Source }
    foreach ($Candidate in $Candidates) {
        if ($Candidate -and (Test-Path $Candidate)) { return $Candidate }
    }
    return $null
}

function Find-VisualStudioCMake {
    $VsWhereCandidates = @(
        "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe",
        "$env:ProgramFiles\Microsoft Visual Studio\Installer\vswhere.exe"
    ) | Where-Object { $_ -and (Test-Path $_) }

    foreach ($VsWhere in $VsWhereCandidates) {
        $InstallPath = (& $VsWhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath 2>$null | Select-Object -First 1)
        if (-not [string]::IsNullOrWhiteSpace($InstallPath)) {
            $Candidate = Join-Path $InstallPath "Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
            if (Test-Path $Candidate) { return $Candidate }
        }
    }
    return $null
}

function Install-PortableCMake {
    $Existing = Get-ChildItem $PortableToolsRoot -Recurse -Filter "cmake.exe" -File -ErrorAction SilentlyContinue | Select-Object -First 1
    if ($Existing) { return $Existing.FullName }

    Write-Host "CMake was not installed. Downloading portable Kitware CMake $PortableCMakeVersion..." -ForegroundColor Cyan
    New-Item -ItemType Directory -Path $PortableToolsRoot -Force | Out-Null

    $ZipName = "cmake-$PortableCMakeVersion-windows-x86_64.zip"
    $BaseUrl = "https://github.com/Kitware/CMake/releases/download/v$PortableCMakeVersion"
    $ZipPath = Join-Path $PortableToolsRoot $ZipName
    $ShaPath = Join-Path $PortableToolsRoot "cmake-$PortableCMakeVersion-SHA-256.txt"
    $Headers = @{ "User-Agent" = "DEADBRICK-PhysX-bootstrap" }

    Invoke-WebRequest -Headers $Headers -Uri "$BaseUrl/$ZipName" -OutFile $ZipPath
    Invoke-WebRequest -Headers $Headers -Uri "$BaseUrl/cmake-$PortableCMakeVersion-SHA-256.txt" -OutFile $ShaPath

    $ExpectedLine = Get-Content $ShaPath | Where-Object { $_ -match [regex]::Escape($ZipName) } | Select-Object -First 1
    if (-not $ExpectedLine) { throw "Could not verify the downloaded CMake archive: SHA-256 entry is missing." }
    $ExpectedHash = (($ExpectedLine.Trim() -split '\s+')[0]).ToUpperInvariant()
    $ActualHash = (Get-FileHash -Algorithm SHA256 -Path $ZipPath).Hash.ToUpperInvariant()
    if ($ExpectedHash -ne $ActualHash) {
        Remove-Item $ZipPath -Force -ErrorAction SilentlyContinue
        throw "Portable CMake checksum mismatch. Download was discarded."
    }

    Expand-Archive -Path $ZipPath -DestinationPath $PortableToolsRoot -Force
    Remove-Item $ZipPath,$ShaPath -Force -ErrorAction SilentlyContinue

    $Found = Get-ChildItem $PortableToolsRoot -Recurse -Filter "cmake.exe" -File -ErrorAction SilentlyContinue | Select-Object -First 1
    if (-not $Found) { throw "Portable CMake archive was extracted but cmake.exe was not found." }
    Write-Host "Portable CMake ready: $($Found.FullName)" -ForegroundColor Green
    return $Found.FullName
}

function Find-BuiltFile([string]$Name) {
    $Matches = @(Get-ChildItem $SourceRoot -Recurse -File -Filter $Name -ErrorAction SilentlyContinue)
    if ($Matches.Count -eq 0) { return $null }

    $Checked = @($Matches | Where-Object { $_.FullName -match '[\\/]checked[\\/]' })
    if ($Checked.Count -gt 0) { return $Checked[0].FullName }
    return $Matches[0].FullName
}

Write-Host ""
Write-Host "============================================================" -ForegroundColor Cyan
Write-Host " DEADBRICK - NVIDIA PhysX 5.8 bootstrap" -ForegroundColor Cyan
Write-Host " pinned tag: $PhysXTag" -ForegroundColor Cyan
Write-Host "============================================================" -ForegroundColor Cyan
Write-Host ""

if ((Test-SdkReady) -and -not $Force) {
    Write-Host "PhysX 5.8 SDK is already normalized for DEADBRICK." -ForegroundColor Green
    exit 0
}

$Git = Find-Executable "git.exe" @(
    "$env:ProgramFiles\Git\cmd\git.exe",
    "$env:ProgramFiles\Git\bin\git.exe",
    "$env:LOCALAPPDATA\GitHubDesktop\app-*\resources\app\git\cmd\git.exe"
)

$CMake = Find-Executable "cmake.exe" @(
    "$env:ProgramFiles\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe",
    "$env:ProgramFiles\Microsoft Visual Studio\2022\Professional\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe",
    "$env:ProgramFiles\Microsoft Visual Studio\2022\Enterprise\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe",
    "$env:ProgramFiles\Microsoft Visual Studio\2022\BuildTools\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
)
if (-not $CMake) { $CMake = Find-VisualStudioCMake }
if (-not $CMake) { $CMake = Install-PortableCMake }

$UE58Python = Join-Path "C:\Program Files\Epic Games\UE_5.8" "Engine\Binaries\ThirdParty\Python3\Win64\python.exe"
$Python = Find-Executable "python.exe" @(
    $UE58Python,
    "$env:LOCALAPPDATA\Programs\Python\Python314\python.exe",
    "$env:LOCALAPPDATA\Programs\Python\Python313\python.exe",
    "$env:LOCALAPPDATA\Programs\Python\Python312\python.exe",
    "$env:LOCALAPPDATA\Programs\Python\Python311\python.exe"
)

if (-not $Git) { throw "Git was not found. Git for Windows or GitHub Desktop is required." }
if (-not $CMake) { throw "CMake bootstrap failed unexpectedly." }
if (-not $Python) { throw "Python was not found. UE 5.8's bundled Python or a normal Python installation is required." }

Write-Host "Git   : $Git" -ForegroundColor DarkGray
Write-Host "CMake : $CMake" -ForegroundColor DarkGray
Write-Host "Python: $Python" -ForegroundColor DarkGray

$env:PATH = "$(Split-Path -Parent $CMake);$(Split-Path -Parent $Python);$env:PATH"
New-Item -ItemType Directory -Path $ThirdPartyRoot -Force | Out-Null

if (-not (Test-Path (Join-Path $SourceRoot ".git"))) {
    if (Test-Path $SourceRoot) { Remove-Item $SourceRoot -Recurse -Force }
    Write-Host "Cloning official NVIDIA PhysX 5.8 source..." -ForegroundColor Cyan
    & $Git clone --depth 1 --branch $PhysXTag $PhysXRepository $SourceRoot
    if ($LASTEXITCODE -ne 0) { throw "PhysX clone failed with exit code $LASTEXITCODE." }
} else {
    Write-Host "Refreshing pinned PhysX source..." -ForegroundColor Cyan
    & $Git -C $SourceRoot fetch --depth 1 origin "refs/tags/$PhysXTag`:refs/tags/$PhysXTag"
    & $Git -C $SourceRoot checkout --force $PhysXTag
    if ($LASTEXITCODE -ne 0) { throw "Could not checkout pinned PhysX tag $PhysXTag." }
}

$Preset = Join-Path $SourceRoot "physx\buildtools\presets\public\$PresetName.xml"
if (-not (Test-Path $Preset)) { throw "PhysX preset not found: $Preset" }

# Unreal Engine uses the dynamic MSVC CRT. The official CPU-only preset defaults to static CRT,
# so normalize these switches before compiling to avoid allocator/CRT boundary problems in UE.
$PresetText = Get-Content $Preset -Raw
$PresetText = $PresetText -replace '(name="PX_BUILDSNIPPETS"\s+value=")True(")', '${1}False$2'
$PresetText = $PresetText -replace '(name="PX_BUILDPVDRUNTIME"\s+value=")True(")', '${1}False$2'
$PresetText = $PresetText -replace '(name="NV_USE_STATIC_WINCRT"\s+value=")True(")', '${1}False$2'
$PresetText = $PresetText -replace '(name="NV_USE_DEBUG_WINCRT"\s+value=")True(")', '${1}False$2'
Set-Content -Path $Preset -Value $PresetText -Encoding UTF8

$PhysXDir = Join-Path $SourceRoot "physx"
$Generate = Join-Path $PhysXDir "generate_projects.bat"
if (-not (Test-Path $Generate)) { throw "PhysX generate_projects.bat was not found." }

Write-Host "Generating VS2022 Win64 CPU-only PhysX projects..." -ForegroundColor Cyan
Push-Location $PhysXDir
try {
    & $Generate $PresetName
    if ($LASTEXITCODE -ne 0) { throw "PhysX project generation failed with exit code $LASTEXITCODE." }
} finally {
    Pop-Location
}

$CompilerDir = Join-Path $PhysXDir "compiler\$PresetName"
if (-not (Test-Path $CompilerDir)) { throw "Generated PhysX compiler directory was not found: $CompilerDir" }

Write-Host "Building PhysX 5.8 CHECKED with VS2022..." -ForegroundColor Cyan
& $CMake --build $CompilerDir --config checked --parallel
if ($LASTEXITCODE -ne 0) { throw "PhysX CHECKED build failed with exit code $LASTEXITCODE." }

New-Item -ItemType Directory -Path $IncludeRoot,$LibRoot,$BinRoot,(Split-Path -Parent $Marker) -Force | Out-Null
if (Test-Path $IncludeRoot) { Remove-Item $IncludeRoot -Recurse -Force }
New-Item -ItemType Directory -Path $IncludeRoot -Force | Out-Null
Copy-Item (Join-Path $PhysXDir "include\*") $IncludeRoot -Recurse -Force

Write-Host "Normalizing PhysX libraries and runtime DLLs..." -ForegroundColor Cyan
foreach ($Name in $RequiredLibs) {
    $Found = Find-BuiltFile $Name
    if (-not $Found) { throw "Required PhysX library was not produced: $Name" }
    Copy-Item $Found (Join-Path $LibRoot $Name) -Force
}
foreach ($Name in $RequiredDlls) {
    $Found = Find-BuiltFile $Name
    if (-not $Found) { throw "Required PhysX runtime DLL was not produced: $Name" }
    Copy-Item $Found (Join-Path $BinRoot $Name) -Force
}

if (-not (Test-SdkReady)) { throw "PhysX build finished but normalized SDK validation failed." }

@(
    "DEADBRICK PhysX 5.8 setup",
    "Prepared: $(Get-Date -Format o)",
    "Repository: $PhysXRepository",
    "Tag: $PhysXTag",
    "Preset: $PresetName",
    "Configuration: checked",
    "CRT: dynamic MSVC",
    "CMake: $CMake",
    "Python: $Python",
    "SDK: $SdkRoot"
) | Set-Content $Marker -Encoding UTF8

Write-Host ""
Write-Host "PhysX 5.8 is ready for DEADBRICK." -ForegroundColor Green
Write-Host "  headers: $IncludeRoot" -ForegroundColor DarkGray
Write-Host "  libs   : $LibRoot" -ForegroundColor DarkGray
Write-Host "  dlls   : $BinRoot" -ForegroundColor DarkGray
exit 0
