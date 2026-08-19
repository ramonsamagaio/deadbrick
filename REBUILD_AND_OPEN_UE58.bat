@echo off
setlocal

set "PROJECT_DIR=%~dp0"
set "PROJECT=%PROJECT_DIR%DEADBRICK.uproject"
set "ENGINE_DIR=C:\Program Files\Epic Games\UE_5.8"
set "BUILD_BAT=%ENGINE_DIR%\Engine\Build\BatchFiles\Build.bat"
set "EDITOR_EXE=%ENGINE_DIR%\Engine\Binaries\Win64\UnrealEditor.exe"

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

echo.
echo ============================================================
echo BUILD SUCCEEDED. Opening DEADBRICK with the freshly built DLL.
echo ============================================================
start "" "%EDITOR_EXE%" "%PROJECT%"

endlocal
