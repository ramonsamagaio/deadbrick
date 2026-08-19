@echo off
setlocal
cd /d "%~dp0"

echo.
echo DEADBRICK - import LayOfTheLand reference content

echo Close Unreal Editor before continuing.
echo.

if "%~1"=="" (
    powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0IMPORT_LOTL_REFERENCE_UE58.ps1"
) else (
    powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0IMPORT_LOTL_REFERENCE_UE58.ps1" -ReferenceRoot "%~1"
)

set EXITCODE=%ERRORLEVEL%
if not "%EXITCODE%"=="0" (
    echo.
    echo Import finished with error code %EXITCODE%.
    pause
)
exit /b %EXITCODE%
