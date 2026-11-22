@echo off
rem The formatting benchmark of plan step 22 with MSVC: builds fmtbench and the size probe in build\fmtbench-msvc
rem (Visual Studio's x64 environment loaded here), checks that snprintf and {fmt} write the same text, times every
rem case and prints the sizes. With build\fmt-ref, a copy of libs\fmt without its patches, both are timed,
rem alternated, twice. Run from cmd or PowerShell.
rem
rem usage: run.bat [core]
setlocal
set "HERE=%~dp0"
for %%i in ("%HERE%..\..\..\..") do set "REPO=%%~fi"
set "B=%REPO%\build\fmtbench-msvc"
set "REF=%REPO%\build\fmt-ref"
set "CORE_ARG="
if not "%~1"=="" set "CORE_ARG=--core %~1"

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

set "REF_ARG="
if exist "%REF%\src\format.cc" set "REF_ARG=-DFMT_REF=%REF%"
cmake -S "%HERE:~0,-1%" -B "%B%" -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_COMPILER=cl -DREPO="%REPO%" %REF_ARG% > nul || exit /b 1
cmake --build "%B%" > "%B%\build.log" 2>&1
if errorlevel 1 (
	type "%B%\build.log"
	exit /b 1
)
if defined REF_ARG (
	for %%r in (1 2) do (
		echo === round %%r: {fmt} without libs\fmt's patches ^(build\fmt-ref^)
		"%B%\fmtbench_ref.exe" %CORE_ARG%
		echo === round %%r: libs\fmt
		"%B%\fmtbench.exe" %CORE_ARG%
	)
) else (
	"%B%\fmtbench.exe" %CORE_ARG%
)

echo.
echo sizes (bytes)
for %%p in (size_printf size_fmt) do for %%s in ("%B%\%%p.exe") do echo   %%p %%~zs
for /r "%B%" %%o in (format.cc.obj) do if exist "%%o" echo   format.cc %%~zo (libs\fmt\src\format.cc's object, every section)
endlocal
