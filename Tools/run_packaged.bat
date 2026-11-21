@echo off
rem P08 NavReadyPCG - run the PACKAGED Development build once, for a functional check (not a timed benchmark run).
rem Usage: run_packaged.bat <LogName> [nullrhi|gpu] ["game args"]
rem   nullrhi: no window, no GPU.   gpu: a 960x540 window on the real GPU.
rem   The game arguments are ONE quoted string, e.g. "-NRSpike -NRExit -NRTimeout=30": batch splits unquoted arguments at
rem   '=', so -Key=Value would otherwise reach the game as two words. %~3 removes the quotes; the game sees the text as is.
rem The log goes to ..\Build\<LogName>.log; the exit code is the game's own (UNRBenchmarkSubsystem returns 1 on a failed check).
rem Run it through the machine-wide queue, never directly.
setlocal
set "EXE=%~dp0..\Build\Windows\NavReadyPCG.exe"
set "LOGNAME=%~1"
set "MODE=%~2"
set "RHI=-nullrhi"
if /I "%MODE%"=="gpu" set "RHI=-windowed -ResX=960 -ResY=540"
set "LOG=%~dp0..\Build\%LOGNAME%.log"

"%EXE%" %RHI% -nosound -unattended -traceautostart=0 -log -abslog="%LOG%" %~3
exit /b %ERRORLEVEL%
