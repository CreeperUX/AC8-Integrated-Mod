@echo off
cd /d "%~dp0"
if not defined VSCMD_ARG_TGT_ARCH (
 for /f "usebackq delims=" %%I in (`"%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "AC8_VS_PATH=%%I"
 if not defined AC8_VS_PATH exit /b 1
)
if not defined VSCMD_ARG_TGT_ARCH call "%AC8_VS_PATH%\VC\Auxiliary\Build\vcvars64.bat"
if errorlevel 1 exit /b 1
if /i not "%VSCMD_ARG_TGT_ARCH%"=="x64" exit /b 1
if exist ..\scripts\generate-hud-theme.py (
 python -X utf8 ..\scripts\generate-hud-theme.py --check
 if errorlevel 1 exit /b 1
)
if not exist build mkdir build
lib /nologo /def:src\ue4ss_lua.def /out:build\UE4SS.lib /machine:x64
if errorlevel 1 exit /b 1
cl /nologo /std:c++20 /EHsc /MD /O2 /LD /Iinclude src\mouse_aim.cpp src\vendor\minhook\src\buffer.c src\vendor\minhook\src\hook.c src\vendor\minhook\src\trampoline.c src\vendor\minhook\src\hde\hde64.c /Fobuild\ /Febuild\ac8_mouse_aim_010.dll /link build\UE4SS.lib
exit /b %errorlevel%
