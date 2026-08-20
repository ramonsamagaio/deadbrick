@echo off
setlocal
cd /d "%~dp0"

echo.
echo ============================================================
echo  DEADBRICK - NVIDIA PhysX 5.8 setup
echo ============================================================
echo.

powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0SETUP_PHYSX5_UE58.ps1"
set ERR=%ERRORLEVEL%

echo.
if not "%ERR%"=="0" (
    echo PhysX setup failed with code %ERR%.
    pause
    exit /b %ERR%
)

echo PhysX 5.8 setup complete.
pause
endlocal
