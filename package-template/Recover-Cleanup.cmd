@echo off
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0Recover-Cleanup.ps1" -Interactive
set "AC8_RESULT=%errorlevel%"
pause
exit /b %AC8_RESULT%
