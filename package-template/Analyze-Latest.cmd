@echo off
"%SystemRoot%\System32\WindowsPowerShell\v1.0\powershell.exe" -NoProfile -ExecutionPolicy Bypass -File "%~dp0Run-Console.ps1" -Action Analyze
set "AC8_RESULT=%errorlevel%"
pause
exit /b %AC8_RESULT%
