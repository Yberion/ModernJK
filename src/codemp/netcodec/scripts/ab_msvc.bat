@echo off
rem A/B (Windows VM): bench_codec of a reference against the working tree, MSVC x64 Release, run alternately
rem on logical CPU 1 with high priority; prints each run, then the best of each build against ref with the checks of
rem ab_summary.ps1 (netcodec mismatches, a busy machine, code placement).
rem The reference: ABM_REF_DIR=<a copy of a tree> if set; else extracted here when git is in the PATH
rem (ABM_REF=<commit>, default HEAD); otherwise the copy that ab_msvc_ref.sh [commit] writes on the host into
rem build\ab-msvc, used only if its commit is HEAD (read from .git, no git needed): a stale copy stops the run
rem (ABM_ANY_REF=1 compares it anyway).
rem The reference is built with its own ModernJK sources (the legacy coder, msg.cpp, and the states' layout, q_shared.h):
rem those of its commit, or ABM_REF_DIR's when it holds them, else the working tree's. Each tree is built in its
rem layout (as ab_tree.sh): from its root when it has ProjectOptions.cmake, from netcodec's folder in older trees.
rem ABM_BENCH=client: netcodec_bench_client on the recorded client packets (docs\modernization\data\clientpackets\*.jkcp)
rem instead of bench_codec; the times are then per client packet.
rem Usage (cmd or PowerShell, from anywhere): src\codemp\netcodec\scripts\ab_msvc.bat [rounds]. It loads Visual
rem Studio's x64 environment itself (vcvars64.bat, Visual Studio found by vswhere.exe).
setlocal EnableDelayedExpansion
set "R=%~dp0..\..\..\.."
for %%i in ("%R%") do set "R=%%~fi"
set "ROUNDS=%~1"
if "%ROUNDS%"=="" set "ROUNDS=6"
if not defined ABM_BENCH set "ABM_BENCH=codec"
set "ARGS="%R%\docs\modernization\data\traces\codec_trace.bin" 10"
if "%ABM_BENCH%"=="client" (
	set "ARGS="
	for %%p in ("%R%\docs\modernization\data\clientpackets\*.jkcp") do set ARGS=!ARGS! "%%~fp"
)
set "W=%TEMP%\netcodec-abm"
if exist "%W%" rmdir /s /q "%W%"
mkdir "%W%"

rem Visual Studio's x64 environment: the 64-bit compiler
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
call "%VSDIR%\VC\Auxiliary\Build\vcvars64.bat" > nul || exit /b 1

rem the commit of HEAD: .git\HEAD names a branch ("ref: refs/heads/...", in its own file or in packed-refs) or holds it
set "HEADC="
set "HEADLINE="
for /f "usebackq delims=" %%L in ("%R%\.git\HEAD") do if not defined HEADLINE set "HEADLINE=%%L"
if "!HEADLINE:~0,5!"=="ref: " (
	set "REFNAME=!HEADLINE:~5!"
	set "REFFILE=%R%\.git\!REFNAME:/=\!"
	if exist "!REFFILE!" (
		for /f "usebackq delims=" %%L in ("!REFFILE!") do if not defined HEADC set "HEADC=%%L"
	) else (
		for /f "usebackq tokens=1,2" %%a in ("%R%\.git\packed-refs") do if "%%b"=="!REFNAME!" set "HEADC=%%a"
	)
) else (
	set "HEADC=!HEADLINE!"
)
if not defined HEADC (
	echo cannot read the HEAD commit from %R%\.git
	exit /b 1
)

if defined ABM_REF_DIR (
	set "REFTREE=%ABM_REF_DIR%"
	echo ref = the copy in %ABM_REF_DIR%; cur = the working tree
	goto refready
)
where git >nul 2>&1
if not errorlevel 1 (
	mkdir "%W%\ref"
	if not defined ABM_REF set "ABM_REF=HEAD"
	rem the files an A/B build needs, in the commit's layout (as ab_tree.sh)
	set "PATHS="
	git -c safe.directory=* -C "%R%" cat-file -e !ABM_REF!:ProjectOptions.cmake >nul 2>&1
	if not errorlevel 1 (
		set "PATHS=CMakeLists.txt CMakePresets.json ProjectOptions.cmake Dependencies.cmake cmake src/CMakeLists.txt src/shared src/codemp libs/llvm-libc"
	) else (
		for %%p in (src/codemp src/shared libs/gsl-lite libs/llvm-libc codemp shared lib/gsl-lite lib/llvm-libc) do (
			git -c safe.directory=* -C "%R%" cat-file -e !ABM_REF!:%%p >nul 2>&1 && set "PATHS=!PATHS! %%p"
		)
	)
	git -c safe.directory=* -C "%R%" archive !ABM_REF! !PATHS! | "%SystemRoot%\System32\tar.exe" -x -f - -C "%W%\ref"
	set "REFTREE=%W%\ref"
	echo ref = !ABM_REF! ^(HEAD is !HEADC:~0,9!, extracted with git^); cur = the working tree
) else (
	set "REFTXT=%R%\build\ab-msvc\ref.txt"
	set "REFC="
	set "REFDESC="
	if exist "!REFTXT!" for /f "usebackq tokens=1*" %%a in ("!REFTXT!") do if not defined REFC (
		set "REFC=%%a"
		set "REFDESC=%%b"
	)
	if not defined REFC (
		echo no reference copy: run on the host, in Git Bash: bash src/codemp/netcodec/scripts/ab_msvc_ref.sh
		exit /b 1
	)
	if not "!REFC!"=="!HEADC!" if not defined ABM_ANY_REF (
		echo the reference copy is !REFC:~0,9! but HEAD is !HEADC:~0,9!: run on the host, in Git Bash:
		echo   bash src/codemp/netcodec/scripts/ab_msvc_ref.sh
		echo or set ABM_ANY_REF=1 to compare that older reference anyway
		exit /b 1
	)
	set "REFTREE=%R%\build\ab-msvc\ref"
	echo ref = !REFDESC! ^(the host's copy^); cur = the working tree
)

:refready
echo building the reference and the working tree (bench_%ABM_BENCH%) ...
call :build "!REFTREE!" ref || exit /b 1
call :build "%R%" cur || exit /b 1
for /l %%i in (1,1,%ROUNDS%) do (
	for %%v in (ref cur) do (
		echo %%v %%i
		start "" /b /wait /affinity 0x2 /high "!EXE_%%v!" !ARGS! > "%W%\out-%%v-%%i.txt"
		findstr /c:"mismatches" /c:"  encode" /c:"  decode" /c:"coding time" /c:"netcodec all:" "%W%\out-%%v-%%i.txt"
	)
)
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0ab_summary.ps1" "%W%"
exit /b 0

rem ---- builds the benchmark of a tree (%1) as ref or cur (%2), in the tree's layout; sets EXE_<ref or cur>
:build
set "TREE=%~1"
set "B=%W%\build-%~2"
rem the legacy benchmark's sources: the tree's own when it has them, else the working tree's
set "ENGINE=%R%"
if exist "%TREE%\src\codemp\qcommon\msg.cpp" set "ENGINE=%TREE%"
if exist "%TREE%\codemp\qcommon\msg.cpp" set "ENGINE=%TREE%"
set "COMMON=-G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_C_COMPILER=cl -DCMAKE_CXX_COMPILER=cl -Dnetcodec_TRACE_DIR="%R%\docs\modernization\data\traces""
if exist "%TREE%\ProjectOptions.cmake" (
	cmake -S "%TREE%" -B "%B%" %COMMON% -Dnetcodec_ENGINE_DIR="!ENGINE!" -Dmodernjk_BUILD_MP_CLIENT=OFF -Dmodernjk_BUILD_MP_SERVER=OFF -Dmodernjk_BUILD_MP_GAME=OFF -Dmodernjk_BUILD_SP=OFF -Dmodernjk_BUILD_JK2=OFF -Dmodernjk_BUILD_ASSETS_PK3=OFF -DBUILD_TESTING=OFF -Dmodernjk_BUILD_TOOLS=OFF > "%W%\cfg-%~2.txt" 2>&1
	set "EXE_%~2=%B%\bin\netcodec_bench_%ABM_BENCH%.exe"
) else (
	set "NC=%TREE%\codemp\netcodec"
	if exist "%TREE%\src\codemp\netcodec\CMakeLists.txt" set "NC=%TREE%\src\codemp\netcodec"
	cmake -S "!NC!" -B "%B%" %COMMON% -Dnetcodec_MODERNJK_DIR="!ENGINE!" > "%W%\cfg-%~2.txt" 2>&1
	set "EXE_%~2=%B%\bench\netcodec_bench_%ABM_BENCH%.exe"
)
if errorlevel 1 (type "%W%\cfg-%~2.txt" & exit /b 1)
cmake --build "%B%" --target netcodec_bench_%ABM_BENCH% > "%W%\bld-%~2.txt" 2>&1
if errorlevel 1 (type "%W%\bld-%~2.txt" & exit /b 1)
exit /b 0
