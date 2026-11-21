@echo off
rem P08 NavReadyPCG - run the game once for a functional check (not a timed benchmark run).
rem Usage: run_game.bat <LogName> <nullrhi|gpu|uncooked> ["game args"]
rem   nullrhi:  the PACKAGED build, no window, no GPU.
rem   gpu:      the PACKAGED build, a 960x540 window on the real GPU.
rem   uncooked: the editor binary in -game mode (no cooking; for comparing editor and packaged results).
rem   The game arguments are ONE quoted string, e.g. "-NRRun -NRSeed=2 -NRExit": batch splits unquoted arguments at '=',
rem   so -Key=Value would otherwise reach the game as two words. %~3 removes the quotes; the game sees the text as is.
rem The log goes to ..\Build\<LogName>.log (an older one is deleted first, so a run that never starts can't leave stale
rem results behind); the exit code is the game's own (UNRBenchmarkSubsystem returns 1 on a failed run).
rem Each launch sits on its own line, outside any ( ) block: cmd expands %ERRORLEVEL% in a block when it reads the
rem block, before the game has run, so "exit /b %ERRORLEVEL%" inside one would return the code from before the launch.
rem Run it through the machine-wide queue, never directly.
setlocal
set "UE_ROOT=C:\Program Files\Epic Games\UE_5.8"
set "PROJECT=%~dp0..\NavReadyPCG\NavReadyPCG.uproject"
set "LOGNAME=%~1"
set "MODE=%~2"
set "LOG=%~dp0..\Build\%LOGNAME%.log"
set "COMMON=-nosound -unattended -traceautostart=0 -log -abslog="%LOG%""
if exist "%LOG%" del "%LOG%"

if /I "%MODE%"=="uncooked" goto :uncooked
set "RHI=-nullrhi"
if /I "%MODE%"=="gpu" set "RHI=-windowed -ResX=960 -ResY=540"
"%~dp0..\Build\Windows\NavReadyPCG.exe" %RHI% %COMMON% %~3
exit /b %ERRORLEVEL%

:uncooked
"%UE_ROOT%\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "%PROJECT%" -game -nullrhi -nosplash %COMMON% %~3
exit /b %ERRORLEVEL%
