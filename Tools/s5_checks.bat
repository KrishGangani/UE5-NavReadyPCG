@echo off
rem P08 NavReadyPCG - final checks in one queue job: build both targets, every NavReadyPCG automation test, package, and one
rem short batch: B0 vs NR, 2 interleaved runs each + priming, pinned, real time at 60 fps on the GPU (about 3 minutes).
setlocal
set "HERE=%~dp0"

call :step S5-build-editor "%HERE%build.bat" NavReadyPCGEditor Development
if %ERRORLEVEL% neq 0 exit /b %ERRORLEVEL%
call :step S5-build-game "%HERE%build.bat" NavReadyPCG Development
if %ERRORLEVEL% neq 0 exit /b %ERRORLEVEL%
call :step S5-unit-tests "%HERE%run_tests.bat" NavReadyPCG
if %ERRORLEVEL% neq 0 exit /b %ERRORLEVEL%
call :step S5-package "%HERE%package.bat" Development
if %ERRORLEVEL% neq 0 exit /b %ERRORLEVEL%
call :step S5-batch python -I -u "%HERE%run_bench.py" --configs "B0,NR" --runs 2 --label final
if %ERRORLEVEL% neq 0 exit /b %ERRORLEVEL%
echo [s5] all checks passed
exit /b 0

rem :step <name> <command and any number of arguments>, collected with shift (%2 ... %9 reach only eight). A comma also splits arguments in cmd, so the config list is quoted.
:step
set "STEPNAME=%~1"
set "STEPCMD="
:collect
shift
if "%~1"=="" goto run
set STEPCMD=%STEPCMD% %1
goto collect
:run
echo [s5] %STEPNAME% start %DATE% %TIME%
echo [s5] %STEPNAME% command:%STEPCMD%
call %STEPCMD%
set "RC=%ERRORLEVEL%"
echo [s5] %STEPNAME% end %DATE% %TIME% exit=%RC%
exit /b %RC%
