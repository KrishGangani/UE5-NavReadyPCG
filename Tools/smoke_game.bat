@echo off
rem P08 NavReadyPCG - smoke test of the UNCOOKED game: run the project in -game mode (no window, no GPU), load the
rem default map, print three engine console variables, quit. Printing pcg.* and ai.navigation.* values proves that the
rem PCG plugin and the navigation system are loaded (a console variable only exists once its module has registered it).
rem Usage: smoke_game.bat [extra args...]
rem Run it through the machine-wide queue, never directly.
setlocal
set "UE_ROOT=C:\Program Files\Epic Games\UE_5.8"
set "PROJECT=%~dp0..\NavReadyPCG\NavReadyPCG.uproject"
set "LOG=%~dp0..\NavReadyPCG\Saved\Logs\SmokeGame.log"

"%UE_ROOT%\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "%PROJECT%" -game -nullrhi -nosound -unattended -nosplash -traceautostart=0 -ExecCmds="pcg.FrameTime,pcg.RuntimeGeneration.NumGeneratingComponents,ai.navigation.MaxDirtyAreasCountWhileSuspended,quit" -log -abslog="%LOG%" %*
exit /b %ERRORLEVEL%
