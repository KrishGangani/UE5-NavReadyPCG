@echo off
rem P08 NavReadyPCG - run the automation tests headless in the editor and exit with their result.
rem Usage: run_tests.bat [Filter]   (default filter: NavReadyPCG, i.e. every test of this project)
rem Exit code 0 = all passed; non-zero = a failure (the engine's automation commandline sets it).
rem Needs the Development editor target (build.bat). Run it through the machine-wide queue, never directly.
setlocal
set "UE_ROOT=C:\Program Files\Epic Games\UE_5.8"
set "PROJECT=%~dp0..\NavReadyPCG\NavReadyPCG.uproject"
set "FILTER=%~1"
if "%FILTER%"=="" set "FILTER=NavReadyPCG"
set "REPORT=%~dp0..\NavReadyPCG\Saved\Automation\Reports"
set "LOG=%~dp0..\NavReadyPCG\Saved\Logs\Tests.log"

"%UE_ROOT%\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "%PROJECT%" -ExecCmds="Automation RunTests %FILTER%;Quit" -unattended -nullrhi -nosound -nosplash -nopause -log -abslog="%LOG%" -ReportExportPath="%REPORT%"
exit /b %ERRORLEVEL%
