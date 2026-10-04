@echo off
cd /d "%~dp0"
call build.cmd
if errorlevel 1 exit /b 1
if not exist test-runtime\Scripts mkdir test-runtime\Scripts
for %%T in (modes_tests fullmodel_tests agility_tests recorder_tests observation_tests camera_context_tests) do (
 cl /nologo /std:c++20 /EHsc /MD /O2 /Iinclude %%T.cpp /Fobuild\%%T.obj /Febuild\%%T.exe /link /DELAYLOAD:UE4SS.dll delayimp.lib build\UE4SS.lib build\buffer.obj build\hook.obj build\trampoline.obj build\hde64.obj
 if errorlevel 1 exit /b 1
 build\%%T.exe
 if errorlevel 1 exit /b 1
)
exit /b 0
