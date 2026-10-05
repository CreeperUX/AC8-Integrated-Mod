@echo off
setlocal
if exist "%~dp0mod-disabled.flag" goto original
if exist "%~dp0..\AC8-Managed-Packages.json" if exist "%~dp0..\mod-disabled.flag" goto original
"%SystemRoot%\System32\WindowsPowerShell\v1.0\powershell.exe" -NoProfile -ExecutionPolicy Bypass -File "%~dp0Run-Console.ps1" -Action Session
set "AC8_RESULT=%errorlevel%"
if not "%AC8_RESULT%"=="0" pause
exit /b %AC8_RESULT%
:original
"%SystemRoot%\System32\WindowsPowerShell\v1.0\powershell.exe" -NoProfile -ExecutionPolicy Bypass -File "%~dp0Mod-Mode.ps1" -Action CheckOriginal
if errorlevel 1 goto refused
if "%~1"=="" goto refused
set "EOS_USE_ANTICHEATCLIENTNULL="
%*
exit /b %errorlevel%
:refused
echo Original launch refused. Close the game, complete cleanup, then start from Steam.
pause
exit /b 1
