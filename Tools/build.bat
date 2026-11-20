@echo off
rem P08 NavReadyPCG - build one target from the command line (reproducible; no IDE needed).
rem Usage: build.bat [Target] [Configuration]
rem   Target:        NavReadyPCGEditor (default) | NavReadyPCG
rem   Configuration: Development (default) | Shipping | DebugGame   (Test can't be built by the launcher engine)
rem Run it through the machine-wide queue, never directly: tools\ue_queue.py run ... -- Tools\build.bat
setlocal
set "UE_ROOT=C:\Program Files\Epic Games\UE_5.8"
set "PROJECT=%~dp0..\NavReadyPCG\NavReadyPCG.uproject"
set "TARGET=%~1"
if "%TARGET%"=="" set "TARGET=NavReadyPCGEditor"
set "CONFIG=%~2"
if "%CONFIG%"=="" set "CONFIG=Development"

call "%UE_ROOT%\Engine\Build\BatchFiles\Build.bat" %TARGET% Win64 %CONFIG% -Project="%PROJECT%" -WaitMutex -NoHotReloadFromIDE
exit /b %ERRORLEVEL%
