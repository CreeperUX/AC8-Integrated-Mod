@echo off
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0Choose-Features.ps1"
if errorlevel 1 exit /b 1
pause
