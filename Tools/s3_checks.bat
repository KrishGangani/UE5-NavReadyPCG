@echo off
rem P08 NavReadyPCG - step S3 checks in one queue job: build both targets, every NavReadyPCG automation test, package,
rem the statistics self-tests, one B0 batch (priming + 1 run, pinned), one single-cell run, and the probe check on it.
rem Stops at the first failure. Results go to Explanation\09_Testing_And_Benchmarks.md.
setlocal
set "HERE=%~dp0"

call :step S3-build-editor "%HERE%build.bat" NavReadyPCGEditor Development
if %ERRORLEVEL% neq 0 exit /b %ERRORLEVEL%
call :step S3-build-game "%HERE%build.bat" NavReadyPCG Development
if %ERRORLEVEL% neq 0 exit /b %ERRORLEVEL%
call :step S3-unit-tests "%HERE%run_tests.bat" NavReadyPCG
if %ERRORLEVEL% neq 0 exit /b %ERRORLEVEL%
call :step S3-package "%HERE%package.bat" Development
if %ERRORLEVEL% neq 0 exit /b %ERRORLEVEL%
call :step S3-stats-selftest python -I -u "%HERE%csv_stats.py" --selftest
if %ERRORLEVEL% neq 0 exit /b %ERRORLEVEL%
call :step S3-probe-selftest python -I -u "%HERE%check_probe.py" --selftest
if %ERRORLEVEL% neq 0 exit /b %ERRORLEVEL%
call :step S3-b0-batch python -I -u "%HERE%run_bench.py" --configs B0 --runs 1 --label s3-b0
if %ERRORLEVEL% neq 0 exit /b %ERRORLEVEL%
call :step S3-single-cell python -I -u "%HERE%run_bench.py" --configs B0 --runs 1 --cells 1 --no-prime --label s3-probe
if %ERRORLEVEL% neq 0 exit /b %ERRORLEVEL%
call :step S3-probe-check python -I -u "%HERE%check_probe.py" "%HERE%..\Benchmarks\Raw\*_s3-probe_B0_r1.csv"
if %ERRORLEVEL% neq 0 exit /b %ERRORLEVEL%
echo [s3] all checks passed
exit /b 0

rem :step <name> <command and any number of arguments>. The arguments are collected with shift, because %2 ... %9
rem reach only eight of them (S3 code review: the benchmark steps have more, and the last ones were silently dropped).
:step
set "STEPNAME=%~1"
set "STEPCMD="
:collect
shift
if "%~1"=="" goto run
set STEPCMD=%STEPCMD% %1
goto collect
:run
echo [s3] %STEPNAME% start %DATE% %TIME%
echo [s3] %STEPNAME% command:%STEPCMD%
call %STEPCMD%
set "RC=%ERRORLEVEL%"
echo [s3] %STEPNAME% end %DATE% %TIME% exit=%RC%
exit /b %RC%
