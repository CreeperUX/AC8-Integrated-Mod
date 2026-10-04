@echo off
cd /d "%~dp0"
call build.cmd
if errorlevel 1 exit /b 1
if not exist test-runtime\previews mkdir test-runtime\previews
cl /nologo /std:c++20 /EHsc /MD /O2 gpu_hud_test.cpp /Fobuild\gpu_hud_test.obj /Febuild\gpu_hud_test.exe
if errorlevel 1 exit /b 1
build\gpu_hud_test.exe
exit /b %errorlevel%
