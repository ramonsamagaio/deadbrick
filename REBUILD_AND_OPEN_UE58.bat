@echo off
setlocal

set "PROJECT_DIR=%~dp0"
set "PROJECT=%PROJECT_DIR%DEADBRICK.uproject"
set "ENGINE_DIR=C:\Program Files\Epic Games\UE_5.8"
set "BUILD_BAT=%ENGINE_DIR%\Engine\Build\BatchFiles\Build.bat"
set "EDITOR_EXE=%ENGINE_DIR%\Engine\Binaries\Win64\UnrealEditor.exe"
set "EDITOR_CMD=%ENGINE_DIR%\Engine\Binaries\Win64\UnrealEditor-Cmd.exe"
set "CLEAN_SCRIPT=%PROJECT_DIR%CLEAN_INVALID_REFERENCE_CONTENT.ps1"
set "PHYSX_SETUP=%PROJECT_DIR%SETUP_PHYSX5_UE58.ps1"
set "LOTL_PIPELINE_SETUP=%PROJECT_DIR%SETUP_LOTL_ASSET_PIPELINE.ps1"
set "REFERENCE_EXPORT=%PROJECT_DIR%ReferenceExported"
set "REFERENCE_EXPORT_SCRIPT=%PROJECT_DIR%EXPORT_LOTL_EDITOR_ASSETS.ps1"
set "REFERENCE_IMPORT_SCRIPT=%PROJECT_DIR%Tools\import_lotl_reference.py"
set "LOTL_LEGACY_PAK=%PROJECT_DIR%ReferenceExtracted\Legacy\LayOfTheLand_Legacy.pak"
set "PHYSX_DLL=%PROJECT_DIR%ThirdParty\PhysX5\SDK\bin\Win64\PhysX_64.dll"
set "ACTORX_PLUGIN=%PROJECT_DIR%Plugins\UnrealPSKPSA\UnrealPSKPSA.uplugin"

echo ============================================================
echo DEADBRICK - UE 5.8 + PhysX 5.8 + LOTL clean rebuild
echo ============================================================
echo.

if not exist "%PROJECT%" (
    echo ERROR: DEADBRICK.uproject was not found next to this script.
    pause
    exit /b 1
)

if not exist "%BUILD_BAT%" (
    echo ERROR: Unreal Engine 5.8 was not found at:
    echo %ENGINE_DIR%
    echo.
    echo Edit ENGINE_DIR in this BAT if Unreal is installed elsewhere.
    pause
    exit /b 1
)

tasklist /FI "IMAGENAME eq UnrealEditor.exe" 2>NUL | find /I "UnrealEditor.exe" >NUL
if not errorlevel 1 (
    echo ERROR: UnrealEditor.exe is currently running.
    echo Close Unreal completely, then run this BAT again.
    pause
    exit /b 1
)

rem PhysX is now a required runtime for DEADBRICK voxel collapse. Do not silently compile a Chaos
rem build when the SDK is missing: install the pinned official 5.8 SDK first and fail loudly on error.
if not exist "%PHYSX_DLL%" (
    echo.
    echo [1/5] PhysX 5.8 SDK missing. Bootstrapping the pinned NVIDIA SDK...
    if not exist "%PHYSX_SETUP%" (
        echo ERROR: SETUP_PHYSX5_UE58.ps1 is missing.
        pause
        exit /b 10
    )
    powershell -NoProfile -ExecutionPolicy Bypass -File "%PHYSX_SETUP%"
    if errorlevel 1 (
        echo.
        echo ============================================================
        echo PHYSX 5.8 SETUP FAILED. BUILD ABORTED.
        echo The voxel collapse backend will NOT silently fall back here.
        echo ============================================================
        pause
        exit /b 10
    )
) else (
    echo [1/5] PhysX 5.8 SDK ready.
)

rem The project explicitly enables UnrealPSKPSA because CUE4Parse gives us PSK/PSA for the original
rem LOTL skeletal meshes and animations. Install/patch it before UBT sees the .uproject.
if not exist "%ACTORX_PLUGIN%" (
    echo.
    echo [2/5] LOTL ActorX importer missing. Installing pinned importer...
    if not exist "%LOTL_PIPELINE_SETUP%" (
        echo ERROR: SETUP_LOTL_ASSET_PIPELINE.ps1 is missing.
        pause
        exit /b 11
    )
    powershell -NoProfile -ExecutionPolicy Bypass -File "%LOTL_PIPELINE_SETUP%"
    if errorlevel 1 (
        echo.
        echo ============================================================
        echo LOTL ACTORX PIPELINE SETUP FAILED. BUILD ABORTED.
        echo ============================================================
        pause
        exit /b 11
    )
) else (
    rem Re-run quickly so an existing checkout also receives the unattended-PSA patch.
    powershell -NoProfile -ExecutionPolicy Bypass -File "%LOTL_PIPELINE_SETUP%"
    if errorlevel 1 (
        echo ERROR: existing LOTL ActorX importer could not be validated/patched.
        pause
        exit /b 11
    )
    echo [2/5] LOTL ActorX importer ready.
)

if exist "%CLEAN_SCRIPT%" (
    echo.
    echo Validating project Content before build...
    powershell -NoProfile -ExecutionPolicy Bypass -File "%CLEAN_SCRIPT%"
    if errorlevel 2 (
        echo.
        echo ============================================================
        echo CONTENT VALIDATION FOUND A TRACKED INVALID PACKAGE.
        echo Nothing tracked was deleted. Send Nyra the lines above.
        echo ============================================================
        pause
        exit /b 2
    )
)

set "HAS_REFERENCE_EXPORT=0"
set "HAS_LOTL_PSK=0"
set "HAS_LOTL_PSA=0"
if exist "%REFERENCE_EXPORT%" (
    dir /S /B "%REFERENCE_EXPORT%\*.glb" 2>NUL | findstr /R "." >NUL && set "HAS_REFERENCE_EXPORT=1"
    dir /S /B "%REFERENCE_EXPORT%\*.gltf" 2>NUL | findstr /R "." >NUL && set "HAS_REFERENCE_EXPORT=1"
    dir /S /B "%REFERENCE_EXPORT%\*.psk" 2>NUL | findstr /R "." >NUL && set "HAS_REFERENCE_EXPORT=1" && set "HAS_LOTL_PSK=1"
    dir /S /B "%REFERENCE_EXPORT%\*.pskx" 2>NUL | findstr /R "." >NUL && set "HAS_REFERENCE_EXPORT=1"
    dir /S /B "%REFERENCE_EXPORT%\*.psa" 2>NUL | findstr /R "." >NUL && set "HAS_REFERENCE_EXPORT=1" && set "HAS_LOTL_PSA=1"
    dir /S /B "%REFERENCE_EXPORT%\*.png" 2>NUL | findstr /R "." >NUL && set "HAS_REFERENCE_EXPORT=1"
)

rem Older DEADBRICK exports only contained GLTF/textures. If the local LOTL pak exists but PSK or PSA
rem is absent, regenerate now so player/enemy skins and original animation sequences actually enter UE.
if exist "%LOTL_LEGACY_PAK%" if exist "%REFERENCE_EXPORT_SCRIPT%" (
    if "%HAS_LOTL_PSK%"=="0" goto :export_lotl
    if "%HAS_LOTL_PSA%"=="0" goto :export_lotl
    goto :export_done

    :export_lotl
    echo.
    echo [3/5] Exporting full LOTL asset families: GLTF + PSK + PSA + textures + metadata...
    powershell -NoProfile -ExecutionPolicy Bypass -File "%REFERENCE_EXPORT_SCRIPT%" -LegacyPak "%LOTL_LEGACY_PAK%"
    if errorlevel 1 (
        echo WARNING: LOTL export reported undecodable families.
        echo Check ReferenceExtracted\Logs\cue4parse_priority_export.txt.
        echo Build continues so recovered families can still be imported.
    )
    :export_done
) else (
    echo.
    echo [3/5] No local LOTL legacy pak found. Existing exported assets, if any, will be used.
)

echo.
echo [4/5] Removing stale compiled binaries and generated files...
if exist "%PROJECT_DIR%Binaries" rmdir /S /Q "%PROJECT_DIR%Binaries"
if exist "%PROJECT_DIR%Intermediate" rmdir /S /Q "%PROJECT_DIR%Intermediate"
if exist "%PROJECT_DIR%.vs" rmdir /S /Q "%PROJECT_DIR%.vs"

echo Building DeadbrickEditor Win64 Development with PhysX 5.8...
call "%BUILD_BAT%" DeadbrickEditor Win64 Development -Project="%PROJECT%" -WaitMutex -NoHotReloadFromIDE

if errorlevel 1 (
    echo.
    echo ============================================================
    echo BUILD FAILED.
    echo Send Nyra the first ERROR lines shown above.
    echo ============================================================
    pause
    exit /b 1
)

set "HAS_REFERENCE_EXPORT=0"
if exist "%REFERENCE_EXPORT%" (
    dir /S /B "%REFERENCE_EXPORT%\*.glb" 2>NUL | findstr /R "." >NUL && set "HAS_REFERENCE_EXPORT=1"
    dir /S /B "%REFERENCE_EXPORT%\*.gltf" 2>NUL | findstr /R "." >NUL && set "HAS_REFERENCE_EXPORT=1"
    dir /S /B "%REFERENCE_EXPORT%\*.psk" 2>NUL | findstr /R "." >NUL && set "HAS_REFERENCE_EXPORT=1"
    dir /S /B "%REFERENCE_EXPORT%\*.pskx" 2>NUL | findstr /R "." >NUL && set "HAS_REFERENCE_EXPORT=1"
    dir /S /B "%REFERENCE_EXPORT%\*.psa" 2>NUL | findstr /R "." >NUL && set "HAS_REFERENCE_EXPORT=1"
    dir /S /B "%REFERENCE_EXPORT%\*.png" 2>NUL | findstr /R "." >NUL && set "HAS_REFERENCE_EXPORT=1"
)

if "%HAS_REFERENCE_EXPORT%"=="1" if exist "%REFERENCE_IMPORT_SCRIPT%" if exist "%EDITOR_CMD%" (
    echo.
    echo [5/5] Importing Lay of the Land static meshes, skeletal meshes, animations and textures...
    "%EDITOR_CMD%" "%PROJECT%" -run=pythonscript -script="%REFERENCE_IMPORT_SCRIPT%" -unattended -nop4 -nosplash
    if errorlevel 1 (
        echo WARNING: LOTL editor import returned an error.
        echo The DEADBRICK C++ build succeeded, so the editor will still open.
        echo Check Saved\Logs and Saved\LOTL_EDITOR_IMPORT.txt.
    ) else (
        echo LOTL editor-safe import completed.
    )
) else (
    echo.
    echo [5/5] WARNING: no editor-safe LOTL visual export is available.
    echo PhysX DEADBRICK will open, but reference visuals cannot be bound until the local LOTL export exists.
)

echo.
echo ============================================================
echo BUILD SUCCEEDED.
echo PhysX 5.8 voxel backend compiled. Opening fresh DEADBRICK DLL.
echo ============================================================
start "" "%EDITOR_EXE%" "%PROJECT%"

endlocal
