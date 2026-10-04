@echo off
rem Called by Steam launch options; do not execute the original command twice.
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0Start-ModSession.ps1"
set "AC8_RESULT=%errorlevel%"
if not "%AC8_RESULT%"=="0" pause
exit /b %AC8_RESULT%
