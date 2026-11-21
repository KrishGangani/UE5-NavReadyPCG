@echo off
rem P08 NavReadyPCG - step S1 checks in one queue job: build both targets, package, then the world spike in the packaged
rem build without and with the GPU. Stops at the first failure. Results are recorded in Explanation\09_Testing_And_Benchmarks.md.
setlocal
set "HERE=%~dp0"

call :step S1-build-editor "%HERE%build.bat" NavReadyPCGEditor Development
if %ERRORLEVEL% neq 0 exit /b %ERRORLEVEL%
call :step S1-build-game "%HERE%build.bat" NavReadyPCG Development
if %ERRORLEVEL% neq 0 exit /b %ERRORLEVEL%
call :step S1-package "%HERE%package.bat" Development
if %ERRORLEVEL% neq 0 exit /b %ERRORLEVEL%
call :step S1-spike-nullrhi "%HERE%run_packaged.bat" S1_spike_nullrhi nullrhi "-NRSpike -NRExit"
if %ERRORLEVEL% neq 0 exit /b %ERRORLEVEL%
call :step S1-spike-gpu "%HERE%run_packaged.bat" S1_spike_gpu gpu "-NRSpike -NRExit"
if %ERRORLEVEL% neq 0 exit /b %ERRORLEVEL%
echo [s1] all checks passed
exit /b 0

:step
echo [s1] %1 start %DATE% %TIME%
call %2 %3 %4 %5 %6 %7 %8 %9
set "RC=%ERRORLEVEL%"
echo [s1] %1 end %DATE% %TIME% exit=%RC%
exit /b %RC%
