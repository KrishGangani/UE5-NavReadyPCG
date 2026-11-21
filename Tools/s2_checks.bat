@echo off
rem P08 NavReadyPCG - step S2 checks in one queue job: build both targets, run the hash unit tests, package, then the
rem determinism runs: seed 1 twice and seed 2 once in the packaged build, seed 1 once uncooked (the editor binary), and
rem compare the per-cell content hashes. Stops at the first failure. Results go to Explanation\09_Testing_And_Benchmarks.md.
setlocal
set "HERE=%~dp0"

call :step S2-build-editor "%HERE%build.bat" NavReadyPCGEditor Development
if %ERRORLEVEL% neq 0 exit /b %ERRORLEVEL%
call :step S2-build-game "%HERE%build.bat" NavReadyPCG Development
if %ERRORLEVEL% neq 0 exit /b %ERRORLEVEL%
call :step S2-unit-tests "%HERE%run_tests.bat" NavReadyPCG.Hash
if %ERRORLEVEL% neq 0 exit /b %ERRORLEVEL%
call :step S2-package "%HERE%package.bat" Development
if %ERRORLEVEL% neq 0 exit /b %ERRORLEVEL%
call :step S2-seed1-a "%HERE%run_game.bat" S2_seed1_a nullrhi "-NRRun -NRSeed=1 -NRRunTag=S2_seed1_a -NRExit"
if %ERRORLEVEL% neq 0 exit /b %ERRORLEVEL%
call :step S2-seed1-b "%HERE%run_game.bat" S2_seed1_b nullrhi "-NRRun -NRSeed=1 -NRRunTag=S2_seed1_b -NRExit"
if %ERRORLEVEL% neq 0 exit /b %ERRORLEVEL%
call :step S2-seed2 "%HERE%run_game.bat" S2_seed2 nullrhi "-NRRun -NRSeed=2 -NRRunTag=S2_seed2 -NRExit"
if %ERRORLEVEL% neq 0 exit /b %ERRORLEVEL%
call :step S2-seed1-gpu "%HERE%run_game.bat" S2_seed1_gpu gpu "-NRRun -NRSeed=1 -NRRunTag=S2_seed1_gpu -NRExit"
if %ERRORLEVEL% neq 0 exit /b %ERRORLEVEL%
call :step S2-seed1-uncooked "%HERE%run_game.bat" S2_seed1_uncooked uncooked "-NRRun -NRSeed=1 -NRRunTag=S2_seed1_uncooked -NRExit"
if %ERRORLEVEL% neq 0 exit /b %ERRORLEVEL%
call :step S2-compare python -I -u "%HERE%compare_hashes.py" "%HERE%..\Build"
if %ERRORLEVEL% neq 0 exit /b %ERRORLEVEL%
echo [s2] all checks passed
exit /b 0

:step
echo [s2] %1 start %DATE% %TIME%
call %2 %3 %4 %5 %6 %7 %8 %9
set "RC=%ERRORLEVEL%"
echo [s2] %1 end %DATE% %TIME% exit=%RC%
exit /b %RC%
