@echo off
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0Launch-Offline.ps1" -CleanupOnly
set "AC8_RESULT=%errorlevel%"
pause
exit /b %AC8_RESULT%
