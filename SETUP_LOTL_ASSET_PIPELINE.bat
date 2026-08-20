@echo off
setlocal
cd /d "%~dp0"

echo.
echo ============================================================
echo  DEADBRICK - Lay of the Land asset pipeline setup
echo ============================================================
echo.

powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0SETUP_LOTL_ASSET_PIPELINE.ps1"
set ERR=%ERRORLEVEL%

echo.
if not "%ERR%"=="0" (
    echo LOTL asset pipeline setup failed with code %ERR%.
    pause
    exit /b %ERR%
)

echo LOTL ActorX pipeline ready.
pause
endlocal
