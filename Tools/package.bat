@echo off
rem P08 NavReadyPCG - package a standalone Win64 build (BuildCookRun), the build every headline benchmark uses.
rem Usage: package.bat [Configuration]   Configuration: Development (default) | Shipping (no CSV profiler; not used)
rem The launcher (installed) engine cannot build the Test configuration (found in UE5-UpdateBudget, Phase 4).
rem Output: ..\Build\Windows\NavReadyPCG.exe   (Build\ is git-ignored)
rem Run it through the machine-wide queue, never directly.
setlocal
set "UE_ROOT=C:\Program Files\Epic Games\UE_5.8"
set "PROJECT=%~dp0..\NavReadyPCG\NavReadyPCG.uproject"
set "ARCHIVE=%~dp0..\Build"
set "CONFIG=%~1"
if "%CONFIG%"=="" set "CONFIG=Development"

call "%UE_ROOT%\Engine\Build\BatchFiles\RunUAT.bat" BuildCookRun -project="%PROJECT%" -noP4 -utf8output -unattended -platform=Win64 -clientconfig=%CONFIG% -build -cook -stage -pak -archive -archivedirectory="%ARCHIVE%"
exit /b %ERRORLEVEL%
