@echo off
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0Run-Console.ps1" -Action Start
set "AC8_RESULT=%errorlevel%"
if not "%AC8_RESULT%"=="0" pause
exit /b %AC8_RESULT%
