@echo off
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0Launch-AC8-via-Steam.ps1"
if errorlevel 1 pause
