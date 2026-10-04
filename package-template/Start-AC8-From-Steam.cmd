@echo off
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0Run-Console.ps1" -Action Session
set "AC8_RESULT=%errorlevel%"
if not "%AC8_RESULT%"=="0" pause
exit /b %AC8_RESULT%
