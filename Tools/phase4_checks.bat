@echo off
rem P08 NavReadyPCG - every Phase 4 foundation check in one run, so the machine-wide queue is entered only once.
rem F-1 editor build, F-2 game build, F-3 uncooked smoke, F-4 package, F-5 packaged smoke (no GPU), F-6 packaged smoke (GPU).
rem Stops at the first failure and returns its exit code. Results are recorded in Explanation\09_Testing_And_Benchmarks.md.
rem Usage (through the queue): python -I -u tools\ue_queue.py run --project P08 --label "..." -- "<this script>"
setlocal
set "HERE=%~dp0"

call :step F-1 "%HERE%build.bat" NavReadyPCGEditor Development
if %ERRORLEVEL% neq 0 exit /b %ERRORLEVEL%
call :step F-2 "%HERE%build.bat" NavReadyPCG Development
if %ERRORLEVEL% neq 0 exit /b %ERRORLEVEL%
call :step F-3 "%HERE%smoke_game.bat"
if %ERRORLEVEL% neq 0 exit /b %ERRORLEVEL%
call :step F-4 "%HERE%package.bat" Development
if %ERRORLEVEL% neq 0 exit /b %ERRORLEVEL%
call :step F-5 "%HERE%smoke_packaged.bat" nullrhi
if %ERRORLEVEL% neq 0 exit /b %ERRORLEVEL%
call :step F-6 "%HERE%smoke_packaged.bat" gpu
if %ERRORLEVEL% neq 0 exit /b %ERRORLEVEL%
echo [phase4] all checks passed
exit /b 0

:step
echo [phase4] %1 start %DATE% %TIME%
call %2 %3 %4
set "RC=%ERRORLEVEL%"
echo [phase4] %1 end %DATE% %TIME% exit=%RC%
exit /b %RC%
