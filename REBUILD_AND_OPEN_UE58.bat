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
set "REFERENCE_PREP_SCRIPT=%PROJECT_DIR%IMPORT_LOTL_REFERENCE_UE58.ps1"
set "REFERENCE_EXPORT=%PROJECT_DIR%ReferenceExported"
set "REFERENCE_EXPORT_SCRIPT=%PROJECT_DIR%EXPORT_LOTL_EDITOR_ASSETS.ps1"
set "REFERENCE_IMPORT_SCRIPT=%PROJECT_DIR%Tools\import_lotl_reference.py"
set "LOTL_LEGACY_PAK=%PROJECT_DIR%ReferenceExtracted\Legacy\LayOfTheLand_Legacy.pak"
set "PHYSX_DLL=%PROJECT_DIR%ThirdParty\PhysX5\SDK\bin\Win64\PhysX_64.dll"
set "PHYSX_BRIDGE=%PROJECT_DIR%ThirdParty\PhysX5\SDK\lib\Win64\DeadbrickPhysXBridge.lib"
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

rem PhysX 5.8 is isolated behind a plain-C bridge because UE 5.8's Chaos compatibility headers still
rem reserve legacy physx::PxQuat/PxTransform names. Both the NVIDIA runtime and the bridge are required.
set "NEED_PHYSX_SETUP=0"
if not exist "%PHYSX_DLL%" set "NEED_PHYSX_SETUP=1"
if not exist "%PHYSX_BRIDGE%" set "NEED_PHYSX_SETUP=1"

if "%NEED_PHYSX_SETUP%"=="1" (
    echo.
    echo [1/6] PhysX 5.8 SDK or isolation bridge missing. Preparing native backend...
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
    echo [1/6] PhysX 5.8 SDK + isolation bridge ready.
)

rem CUE4Parse exports LOTL skeletal assets as PSK/PSA. Install and patch the ActorX importer before
rem UBT reads the project, otherwise the recovered character meshes and animations cannot enter UE.
if not exist "%ACTORX_PLUGIN%" (
    echo.
    echo [2/6] LOTL ActorX importer missing. Installing pinned importer...
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
    powershell -NoProfile -ExecutionPolicy Bypass -File "%LOTL_PIPELINE_SETUP%"
    if errorlevel 1 (
        echo ERROR: existing LOTL ActorX importer could not be validated/patched.
        pause
        exit /b 11
    )
    echo [2/6] LOTL ActorX importer ready.
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

rem A fresh checkout may have none of the 2 GB local reference data. The preparation script searches
rem the standard Steam LOTL locations itself, converts IoStore to an isolated legacy pak, and exports
rem editor-safe assets without recursively launching this rebuild.
if not exist "%LOTL_LEGACY_PAK%" if exist "%REFERENCE_PREP_SCRIPT%" (
    echo.
    echo [3/6] Local LOTL legacy pak missing. Auto-detecting the Steam install and preparing reference data...
    powershell -NoProfile -ExecutionPolicy Bypass -File "%REFERENCE_PREP_SCRIPT%" -SkipRebuild -NonInteractive
    if errorlevel 1 (
        echo WARNING: automatic LOTL preparation did not complete.
        echo If Lay of the Land is installed somewhere non-standard, run IMPORT_LOTL_REFERENCE_UE58.bat once and point it at Content\Paks.
        echo The PhysX build can continue, but unavailable LOTL assets obviously cannot be bound.
    )
) else (
    echo [3/6] Local LOTL legacy reference pak ready.
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

rem Older DEADBRICK exports only contained GLTF/textures. Regenerate if PSK or PSA is missing so
rem player/enemy skins, first-person arms and original animation sequences actually enter UE.
set "NEED_LOTL_EXPORT=0"
if "%HAS_REFERENCE_EXPORT%"=="0" set "NEED_LOTL_EXPORT=1"
if "%HAS_LOTL_PSK%"=="0" set "NEED_LOTL_EXPORT=1"
if "%HAS_LOTL_PSA%"=="0" set "NEED_LOTL_EXPORT=1"

if "%NEED_LOTL_EXPORT%"=="1" if exist "%LOTL_LEGACY_PAK%" if exist "%REFERENCE_EXPORT_SCRIPT%" (
    echo.
    echo [4/6] Exporting full LOTL families: GLTF + PSK + PSA + textures + metadata...
    powershell -NoProfile -ExecutionPolicy Bypass -File "%REFERENCE_EXPORT_SCRIPT%" -LegacyPak "%LOTL_LEGACY_PAK%"
    if errorlevel 1 (
        echo WARNING: LOTL export reported undecodable families.
        echo Check ReferenceExtracted\Logs\cue4parse_priority_export.txt.
        echo Recovered families remain usable and will still be imported.
    )
) else (
    if "%NEED_LOTL_EXPORT%"=="0" (
        echo [4/6] Full LOTL export already contains skeletal meshes and animations.
    ) else (
        echo [4/6] No local LOTL legacy pak is available; existing exports, if any, will be used.
    )
)

echo.
echo [5/6] Removing stale compiled binaries and generated files...
if exist "%PROJECT_DIR%Binaries" rmdir /S /Q "%PROJECT_DIR%Binaries"
if exist "%PROJECT_DIR%Intermediate" rmdir /S /Q "%PROJECT_DIR%Intermediate"
if exist "%PROJECT_DIR%.vs" rmdir /S /Q "%PROJECT_DIR%.vs"

echo Building DeadbrickEditor Win64 Development with isolated PhysX 5.8...
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
    echo [6/6] Importing Lay of the Land static meshes, skeletal meshes, animations and textures...
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
    echo [6/6] WARNING: no editor-safe LOTL visual export is available.
    echo PhysX DEADBRICK will open, but reference visuals cannot bind until local LOTL data is prepared.
)

echo.
echo ============================================================
echo BUILD SUCCEEDED.
echo PhysX 5.8 voxel backend compiled through isolated bridge. Opening fresh DEADBRICK DLL.
echo ============================================================
start "" "%EDITOR_EXE%" "%PROJECT%"

endlocal
