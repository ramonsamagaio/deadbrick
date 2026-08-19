@echo off
setlocal

set "PROJECT_DIR=%~dp0"
set "PROJECT=%PROJECT_DIR%DEADBRICK.uproject"
set "ENGINE_DIR=C:\Program Files\Epic Games\UE_5.8"
set "BUILD_BAT=%ENGINE_DIR%\Engine\Build\BatchFiles\Build.bat"
set "EDITOR_EXE=%ENGINE_DIR%\Engine\Binaries\Win64\UnrealEditor.exe"
set "EDITOR_CMD=%ENGINE_DIR%\Engine\Binaries\Win64\UnrealEditor-Cmd.exe"
set "CLEAN_SCRIPT=%PROJECT_DIR%CLEAN_INVALID_REFERENCE_CONTENT.ps1"
set "REFERENCE_EXPORT=%PROJECT_DIR%ReferenceExported"
set "REFERENCE_IMPORT_SCRIPT=%PROJECT_DIR%Tools\import_lotl_reference.py"

echo ============================================================
echo DEADBRICK - clean rebuild for Unreal Engine 5.8
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

if exist "%CLEAN_SCRIPT%" (
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

echo Removing stale compiled binaries and generated files...
if exist "%PROJECT_DIR%Binaries" rmdir /S /Q "%PROJECT_DIR%Binaries"
if exist "%PROJECT_DIR%Intermediate" rmdir /S /Q "%PROJECT_DIR%Intermediate"
if exist "%PROJECT_DIR%.vs" rmdir /S /Q "%PROJECT_DIR%.vs"

echo.
echo Building DeadbrickEditor Win64 Development...
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
    dir /S /B "%REFERENCE_EXPORT%\*.png" 2>NUL | findstr /R "." >NUL && set "HAS_REFERENCE_EXPORT=1"
)

if "%HAS_REFERENCE_EXPORT%"=="1" if exist "%REFERENCE_IMPORT_SCRIPT%" if exist "%EDITOR_CMD%" (
    echo.
    echo Importing editor-safe Lay of the Land reference exports...
    "%EDITOR_CMD%" "%PROJECT%" -run=pythonscript -script="%REFERENCE_IMPORT_SCRIPT%" -unattended -nop4 -nosplash
    if errorlevel 1 (
        echo WARNING: LOTL editor import returned an error.
        echo The DEADBRICK build succeeded, so the editor will still open.
        echo Check Saved\Logs and Saved\LOTL_EDITOR_IMPORT.txt.
    ) else (
        echo LOTL editor-safe import completed.
    )
)

echo.
echo ============================================================
echo BUILD SUCCEEDED. Opening DEADBRICK with the freshly built DLL.
echo ============================================================
start "" "%EDITOR_EXE%" "%PROJECT%"

endlocal
