@echo off
setlocal
cd /d "%~dp0"

echo.
echo DEADBRICK - import LayOfTheLand reference content
echo Close Unreal Editor before continuing.
echo.

REM Keep all Steam-path detection inside PowerShell. CMD can misparse
REM parentheses in paths such as C:\Program Files (x86) when expanded
REM inside parenthesized IF blocks.
if "%~1"=="" goto AUTO

powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0IMPORT_LOTL_REFERENCE_UE58.ps1" -ReferenceRoot "%~1"
goto DONE

:AUTO
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0IMPORT_LOTL_REFERENCE_UE58.ps1"

:DONE
set EXITCODE=%ERRORLEVEL%
if "%EXITCODE%"=="0" goto END

echo.
echo Import finished with error code %EXITCODE%.
pause

:END
exit /b %EXITCODE%
