@echo off
rem P08 NavReadyPCG - smoke test of the PACKAGED Development build (run package.bat first).
rem Usage: smoke_packaged.bat [nullrhi|gpu] [extra args...]
rem   nullrhi (default): no window, no GPU.   gpu: a 960x540 window on the real GPU (checks which adapter is used).
rem Loads the default map, prints three engine console variables (proves the PCG plugin and the navigation system are
rem in the packaged build), then quits. The log is written next to this script's project: ..\Build\Smoke_<mode>.log
rem Run it through the machine-wide queue, never directly.
setlocal
set "EXE=%~dp0..\Build\Windows\NavReadyPCG.exe"
set "MODE=%~1"
if "%MODE%"=="" set "MODE=nullrhi"
set "RHI=-nullrhi"
if /I "%MODE%"=="gpu" set "RHI=-windowed -ResX=960 -ResY=540"
set "LOG=%~dp0..\Build\Smoke_%MODE%.log"

"%EXE%" %RHI% -nosound -unattended -traceautostart=0 -ExecCmds="pcg.FrameTime,pcg.RuntimeGeneration.NumGeneratingComponents,ai.navigation.MaxDirtyAreasCountWhileSuspended,quit" -log -abslog="%LOG%" %2 %3 %4 %5 %6 %7 %8 %9
exit /b %ERRORLEVEL%
