@echo off
setlocal
cd /d "%~dp0"

echo.
echo DEADBRICK - import LayOfTheLand reference content
echo Close Unreal Editor before continuing.
echo.

set "AUTO_LOTL=C:\Program Files (x86)\Steam\steamapps\common\Lay of the Land\LayOfTheLand\Content\Paks"

if not "%~1"=="" (
    powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0IMPORT_LOTL_REFERENCE_UE58.ps1" -ReferenceRoot "%~1"
) else if exist "%AUTO_LOTL%\pakchunk0-Windows.utoc" (
    echo Found Steam reference automatically:
    echo %AUTO_LOTL%
    echo.
    powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0IMPORT_LOTL_REFERENCE_UE58.ps1" -ReferenceRoot "%AUTO_LOTL%"
) else (
    powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0IMPORT_LOTL_REFERENCE_UE58.ps1"
)

set EXITCODE=%ERRORLEVEL%
if not "%EXITCODE%"=="0" (
    echo.
    echo Import finished with error code %EXITCODE%.
    pause
)
exit /b %EXITCODE%
