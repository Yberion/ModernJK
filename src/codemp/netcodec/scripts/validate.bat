@echo off
rem Validation on Windows (MSVC): configure, build and test the MSVC presets (x64 Release and Debug: the game,
rem netcodec and their tests; x64 with AddressSanitizer: the same with it), the zone files' converter
rem (tools\eal_converter, out of the presets' builds) in Release, then the size benchmark, the codec
rem benchmark (two runs) and the trainers, which must rewrite the committed tables unchanged. Needs the traces of
rem docs\modernization\data\traces (export_rans_trace.py, export_codec_trace.py, export_demo_traces.py, and the
rem client traces of netcodec_bench_client --trace).
rem Run from anywhere (cmd or PowerShell): src\codemp\netcodec\scripts\validate.bat. It loads Visual Studio's x64
rem environment itself (vcvars64.bat, Visual Studio found by vswhere.exe).
rem Output: the console and build\validate-windows.log; the summary (with each step's time) and the benchmark figures
rem come last.
setlocal EnableDelayedExpansion
cd /d "%~dp0..\..\..\.."
set "B=build"
set "T=docs\modernization\data\traces"
set "G=src\codemp\netcodec\src\generated"
if not exist "%B%" mkdir "%B%"
set "LOG=%B%\validate-windows.log"
set "OUT=%TEMP%\modernjk-validate"
if exist "%OUT%" rmdir /s /q "%OUT%"
mkdir "%OUT%"
set FAILED=0
set "SUMMARY=%OUT%\summary.txt"
type nul > "%SUMMARY%"
type nul > "%LOG%"

rem Visual Studio's x64 environment: the 64-bit compiler (the Developer PowerShell's shortcut loads the 32-bit one)
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
set "VSDIR="
if not exist "%VSWHERE%" (
	echo vswhere.exe not found: Visual Studio is not installed
	exit /b 1
)
for /f "usebackq delims=" %%i in (`"%VSWHERE%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "VSDIR=%%i"
if not defined VSDIR (
	echo Visual Studio with the C++ tools not found by vswhere.exe
	exit /b 1
)
call "%VSDIR%\VC\Auxiliary\Build\vcvars64.bat" > nul
if errorlevel 1 (
	echo vcvars64.bat failed
	exit /b 1
)

rem the sources' contents at the start, compared at the end: a file changed during the run makes it a mix (FAIL)
for /f %%h in ('powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0source_hash.ps1" "%CD%"') do set "START_HASH=%%h"

for %%p in (windows-msvc-release windows-msvc-debug windows-msvc-asan) do (
	call :step "%%p: configure" cmake --preset %%p
	call :buildstep "%%p: build" --preset %%p
	call :step "%%p: tests" ctest --preset %%p
)
call :buildstep "windows-msvc-release: zone converter" --preset windows-msvc-release --target eal_converter

set "R=%B%\windows-msvc-release\bin"
call :bench "size benchmark, windows-msvc-release" size "%R%\netcodec_bench_size.exe" "%T%\holdout.bin"
call :bench "size benchmark, windows-msvc-asan" size-asan "%B%\windows-msvc-asan\bin\netcodec_bench_size.exe" "%T%\holdout.bin"
call :bench "codec benchmark, windows-msvc-release, run 1" codec1 "%R%\netcodec_bench_codec.exe" "%T%\codec_trace.bin" 10
call :bench "codec benchmark, windows-msvc-release, run 2" codec2 "%R%\netcodec_bench_codec.exe" "%T%\codec_trace.bin" 10
call :step "trainer windows-msvc-release: train" "%R%\netcodec_train.exe" "%T%\train.bin" "%OUT%\tables.inc"
call :step "trainer windows-msvc-release writes the committed tables" fc.exe /b "%OUT%\tables.inc" "%G%\jka_tables.inc"
call :step "client trainer windows-msvc-release: train" "%R%\netcodec_train_client.exe" "%OUT%\client_tables.inc" "%T%\client_train.bin"
call :step "client trainer windows-msvc-release writes the committed client tables" fc.exe /b "%OUT%\client_tables.inc" "%G%\jka_client_tables.inc"

call :out ""
call :out "=== results"
for %%f in (size size-asan codec1 codec2) do (
	call :out "--- %%f"
	type "%OUT%\%%f.txt" 2>nul
	type "%OUT%\%%f.txt" >> "%LOG%" 2>nul
)
set "END_HASH="
for /f %%h in ('powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0source_hash.ps1" "%CD%"') do set "END_HASH=%%h"
if not "%END_HASH%"=="%START_HASH%" (
	>> "%SUMMARY%" echo FAIL  sources unchanged during the run: a source changed, run again
	set FAILED=1
)
call :out ""
call :out "=== summary"
type "%SUMMARY%"
type "%SUMMARY%" >> "%LOG%"
if %FAILED%==0 (call :out "ALL OK") else (call :out "SOME STEPS FAILED")
echo log: %LOG%
rmdir /s /q "%OUT%"
exit /b %FAILED%

rem ---- the time in seconds, into the variable named by the argument
:now
for /f %%t in ('powershell -NoProfile -Command "[DateTimeOffset]::UtcNow.ToUnixTimeSeconds()"') do set "%~1=%%t"
exit /b 0

rem ---- one step: label, then the command; its output goes to the console and the log
:step
set "NAME=%~1"
shift
set "CMD="
:collect
if "%~1"=="" goto run
set CMD=!CMD! %1
shift
goto collect
:run
call :out ""
call :out "=== %NAME%"
call :now START
call %CMD% > "%OUT%\step.txt" 2>&1
set STEPRC=!ERRORLEVEL!
call :now END
set /a SECS=END-START
type "%OUT%\step.txt"
type "%OUT%\step.txt" >> "%LOG%"
if !STEPRC!==0 (>> "%SUMMARY%" echo ok    !NAME! ^(!SECS! s^)) else (>> "%SUMMARY%" echo FAIL  !NAME! ^(!SECS! s^)& set FAILED=1)
exit /b 0

rem ---- one benchmark: label, output name, then the command; its output is kept for the results
:bench
set "NAME=%~1"
set "FILE=%~2"
shift
shift
set "CMD="
:collectb
if "%~1"=="" goto runb
set CMD=!CMD! %1
shift
goto collectb
:runb
call :out ""
call :out "=== %NAME%"
call %CMD% > "%OUT%\%FILE%.txt" 2>&1
set STEPRC=!ERRORLEVEL!
type "%OUT%\%FILE%.txt" >> "%LOG%"
if !STEPRC!==0 (>> "%SUMMARY%" echo ok    !NAME!) else (>> "%SUMMARY%" echo FAIL  !NAME!& set FAILED=1)
exit /b 0

rem ---- one build: label, then the arguments of cmake --build. A failed build is retried once from clean: on the
rem virtual machine's shared folder MSVC sometimes leaves a corrupt object or PDB (LNK1103); a real error fails twice.
:buildstep
set "NAME=%~1"
shift
set "ARGS="
:collectbuild
if "%~1"=="" goto runbuild
set ARGS=!ARGS! %1
shift
goto collectbuild
:runbuild
call :out ""
call :out "=== %NAME%"
call :now START
cmake --build !ARGS! > "%OUT%\step.txt" 2>&1
set STEPRC=!ERRORLEVEL!
type "%OUT%\step.txt"
type "%OUT%\step.txt" >> "%LOG%"
if not !STEPRC!==0 (
	call :out "=== %NAME%: failed, built again from clean"
	cmake --build !ARGS! --clean-first > "%OUT%\step.txt" 2>&1
	set STEPRC=!ERRORLEVEL!
	type "%OUT%\step.txt"
	type "%OUT%\step.txt" >> "%LOG%"
)
call :now END
set /a SECS=END-START
if !STEPRC!==0 (>> "%SUMMARY%" echo ok    !NAME! ^(!SECS! s^)) else (>> "%SUMMARY%" echo FAIL  !NAME! ^(!SECS! s^)& set FAILED=1)
exit /b 0

rem ---- a line to the console and the log
:out
if "%~1"=="" (echo.& >> "%LOG%" echo.) else (echo %~1& >> "%LOG%" echo %~1)
exit /b 0
