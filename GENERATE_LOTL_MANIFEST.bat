@echo off
setlocal
cd /d "%~dp0"

echo.
echo DEADBRICK - generate LayOfTheLand asset manifest
echo.

powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0GENERATE_LOTL_MANIFEST.ps1"
set EXITCODE=%ERRORLEVEL%

if not "%EXITCODE%"=="0" (
    echo.
    echo Manifest generation failed with error code %EXITCODE%.
)

echo.
pause
exit /b %EXITCODE%
