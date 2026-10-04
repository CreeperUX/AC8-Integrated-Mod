@echo off
powershell.exe -NoProfile -STA -WindowStyle Hidden -ExecutionPolicy Bypass -File "%~dp0Launcher-GUI.ps1"
exit /b %errorlevel%
