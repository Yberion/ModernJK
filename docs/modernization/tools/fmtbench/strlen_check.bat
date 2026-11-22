@echo off
rem {fmt}'s length of a C string with MSVC, every case a fix must keep (plan step 22, before proposing the fix to
rem {fmt}'s authors): for x64 and x86, and for /std:c++14, c++17, c++20, c++23preview, c++23 and c++latest (MSVC has
rem no older mode, so no C++11, which only GCC and clang compile, and whose branch the fix does not change; a mode the
rem compiler does not know, warning D9002, would fall back to C++14: it is reported and skipped),
rem   builtin   strlen_builtin.cpp's four cases: does MSVC accept __builtin_strlen at run time and in a constant
rem             expression (compiled only)
rem   fmt       strlen_fmt.cpp's two constant cases (2: from a literal, 3: from the array of a constexpr object)
rem             against each copy of {fmt}: fmt-ref (build\fmt-ref, 12.2.1 unchanged: the loop), libs (libs\fmt: its
rem             patch, the C library's strlen outside constant evaluation), fmt-a (build\fmt-a: __builtin_strlen,
rem             strlen_a.patch)
rem   run       strlen_fmt.cpp built against each copy and run: the lengths and texts checked, the time of one call on
rem             300 characters, the three copies alternated twice (the times of the x64 build are the measure)
rem A failed case keeps the compiler's output in build\strlen-check\<arch>-<std>-<what>.txt; the summary is also
rem written to build\strlen-check.log. Visual Studio's environment is loaded here; run from cmd or PowerShell.
rem
rem build\fmt-a, made on the development machine from build\fmt-ref (Git Bash, from the repository's root):
rem   cp -r build/fmt-ref build/fmt-a && git apply -p3 --directory=build/fmt-a docs/modernization/tools/fmtbench/strlen_a.patch
rem
rem usage: strlen_check.bat
setlocal EnableDelayedExpansion
set "HERE=%~dp0"
for %%i in ("%HERE%..\..\..\..") do set "REPO=%%~fi"
set "B=%REPO%\build\strlen-check"
set "LOG=%REPO%\build\strlen-check.log"
set "INC_fmt-ref=%REPO%\build\fmt-ref\include"
set "INC_libs=%REPO%\libs\fmt\include"
set "INC_fmt-a=%REPO%\build\fmt-a\include"
for %%v in (fmt-ref libs fmt-a) do if not exist "!INC_%%v!\fmt\base.h" (
	echo !INC_%%v!\fmt\base.h not found
	exit /b 1
)

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

if not exist "%B%" mkdir "%B%"
type nul > "%LOG%"
call :arch x64 vcvars64.bat
call :arch x86 vcvars32.bat
endlocal
exit /b 0

rem :arch <x64|x86> <vcvars script>
:arch
setlocal
call "%VSDIR%\VC\Auxiliary\Build\%~2" > nul
if errorlevel 1 (
	call :out "%~1: %~2 failed"
	endlocal
	exit /b 0
)
for /f "delims=" %%v in ('cl 2^>^&1 ^| findstr /c:"Version"') do call :out "%~1: %%v"
for %%s in (c++14 c++17 c++20 c++23preview c++23 c++latest) do call :mode %~1 %%s
endlocal
exit /b 0

rem :mode <x64|x86> <std>
:mode
cl /nologo /std:%2 /c /DCASE=1 "%HERE%strlen_builtin.cpp" /Fo"%B%\%1-builtin.obj" > "%B%\%1-%2-probe.txt" 2>&1
findstr /c:"D9002" "%B%\%1-%2-probe.txt" > nul
if not errorlevel 1 (
	call :out "%1 %2: unknown to this compiler (D9002), skipped"
	exit /b 0
)
set "line=%1 %2 builtin:"
for %%c in (1 2 3 4) do (
	cl /nologo /std:%2 /c /DCASE=%%c "%HERE%strlen_builtin.cpp" /Fo"%B%\%1-builtin.obj" > "%B%\%1-%2-builtin-%%c.txt" 2>&1
	if errorlevel 1 (set "line=!line! %%c FAIL") else (set "line=!line! %%c ok")
)
call :out "!line!"
for %%v in (fmt-ref libs fmt-a) do (
	set "line=%1 %2 fmt %%v:"
	for %%c in (2 3) do (
		cl /nologo /std:%2 /utf-8 /EHsc /c /DCASE=%%c /I"!INC_%%v!" "%HERE%strlen_fmt.cpp" /Fo"%B%\%1-fmt.obj" > "%B%\%1-%2-fmt-%%v-%%c.txt" 2>&1
		if errorlevel 1 (set "line=!line! %%c FAIL") else (set "line=!line! %%c ok")
	)
	call :out "!line!"
	cl /nologo /std:%2 /utf-8 /EHsc /O2 /DNDEBUG /I"!INC_%%v!" "%HERE%strlen_fmt.cpp" /Fo"%B%\%1-%2-%%v.obj" /Fe"%B%\%1-%2-%%v.exe" > "%B%\%1-%2-run-%%v.txt" 2>&1
	if errorlevel 1 call :out "%1 %2 run %%v: build FAIL"
)
for %%r in (1 2) do for %%v in (fmt-ref libs fmt-a) do if exist "%B%\%1-%2-%%v.exe" (
	for /f "delims=" %%o in ('"%B%\%1-%2-%%v.exe"') do call :out "%1 %2 run %%v round %%r: %%o"
)
exit /b 0

rem :out "<line>": printed and appended to the log
:out
echo %~1
>> "%LOG%" echo %~1
exit /b 0
