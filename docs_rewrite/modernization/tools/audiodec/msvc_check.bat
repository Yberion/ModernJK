@echo off
rem The decided audio decoders with MSVC on the Windows VM (surveys/audio-decoders.md, section 8): libogg, libvorbis
rem and libFLAC built from the sources the survey downloaded, the decided decoders and their references built with
rem Visual Studio 2026, normally and with AddressSanitizer (which stops at the first fault with MSVC), then run on the
rem corpora; the lists are compared with the other compilers' afterwards (crosscheck.py).
rem usage: Z:\ModernJK\docs_rewrite\modernization\tools\audiodec\msvc_check.bat   (cmd or PowerShell; logs in <decoders>\work\check\)
setlocal
set R=Z:\ModernJK
set D=%R%\build-netcodec\audiolibs\decoders
set T=%R%\docs_rewrite\modernization\tools\audiodec
set W=%D%\work\check
set M=%D%\msvc
set G=-G "Visual Studio 18 2026" -A x64
if not exist "%W%" mkdir "%W%"

call :libs "%M%\build" "%M%\prefix" "/DWIN32 /D_WINDOWS" msvc || goto :end
call :libs "%M%\build-asan" "%M%\prefix-asan" "/DWIN32 /D_WINDOWS /fsanitize=address" msvc-asan || goto :end
call :bench "%M%\bench" "%M%\prefix" OFF msvc || goto :end
call :bench "%M%\bench-asan" "%M%\prefix-asan" ON msvc-asan || goto :end
call :run
goto :end

:libs
rem %1 build folder, %2 install folder, %3 C flags, %4 log tag
call :lib "%~1\ogg" "%~2" "%~3" %4-ogg "%D%\src\libogg-1.3.6" "-DINSTALL_DOCS=OFF" || exit /b 1
call :lib "%~1\vorbis" "%~2" "%~3" %4-vorbis "%D%\src\libvorbis-1.3.7" "" || exit /b 1
call :lib "%~1\flac" "%~2" "%~3" %4-flac "%D%\src\flac-1.5.0" "-DBUILD_CXXLIBS=OFF -DBUILD_PROGRAMS=OFF -DBUILD_EXAMPLES=OFF -DBUILD_TESTING=OFF -DBUILD_DOCS=OFF -DINSTALL_MANPAGES=OFF -DWITH_OGG=ON -DENABLE_MULTITHREADING=OFF" || exit /b 1
exit /b 0

:lib
rem %1 build folder, %2 install folder, %3 C flags, %4 log name, %5 source, %6 more options (one string)
cmake -S "%~5" -B "%~1" %G% "-DCMAKE_C_FLAGS=%~3" -DBUILD_SHARED_LIBS=OFF "-DCMAKE_INSTALL_PREFIX=%~2" "-DCMAKE_PREFIX_PATH=%~2" -DCMAKE_POLICY_VERSION_MINIMUM=3.5 %~6 > "%W%\%~4.log" 2>&1 || goto :libfail
cmake --build "%~1" --config Release >> "%W%\%~4.log" 2>&1 || goto :libfail
cmake --install "%~1" --config Release >> "%W%\%~4.log" 2>&1 || goto :libfail
exit /b 0
:libfail
echo FAIL: %~4 (see %W%\%~4.log)
exit /b 1

:bench
rem %1 build folder, %2 the libraries' install folder, %3 ASAN, %4 log tag
if exist "%~1" rd /s /q "%~1"
cmake -S "%T%" -B "%~1" %G% "-DDEC=%D%" "-DREPO=%R%" "-DLIBS_PREFIX=%~2" -DASAN=%3 -DCHOSEN_ONLY=ON > "%W%\%~4-bench.log" 2>&1 || goto :benchfail
cmake --build "%~1" --config Release >> "%W%\%~4-bench.log" 2>&1 || goto :benchfail
for /f %%n in ('findstr /r /c:": warning C[0-9]" "%W%\%~4-bench.log" ^| find /c /v ""') do echo %~4: lines with a compiler warning in the benchmark's build: %%n (MSBuild prints each twice)
exit /b 0
:benchfail
echo FAIL: %~4-bench (see %W%\%~4-bench.log)
exit /b 1

:run
set O=%W%\msvc
if not exist "%O%" mkdir "%O%"
set S=%O%\summary.txt
type nul > "%S%"
set E=%M%\bench\Release
call :one bench_mp3code game_mp3
call :one bench_drmp3 game_mp3 --compare
call :one bench_enginewav game_wav
call :one bench_drwav game_wav --compare
call :one bench_libflac sample_flac
call :one bench_vorbisfile sample_vorbis
call :one bench_stbvorbis sample_vorbis --compare
set E=%M%\bench-asan\Release
call :mutate bench_drmp3 robust_mp3 3
call :mutate bench_drwav robust_wav 9
call :mutate bench_libflac robust_flac 3
call :mutate bench_stbvorbis robust_vorbis 3
>> "%S%" echo == done
echo msvc: done, %S%
exit /b 0

:one
rem %1 executable, %2 corpus, %3 --compare or nothing
>> "%S%" echo == %1 %2
"%E%\%1.exe" "%D%\data\%2.bin" --runs 1 --list "%O%\%1.tsv" %3 >> "%S%" 2>&1
>> "%S%" echo exit %errorlevel%
exit /b 0

:mutate
rem %1 executable, %2 corpus, %3 damaged copies of each file
>> "%S%" echo == asan %1 %2 x%3
"%E%\%1.exe" "%D%\data\%2.bin" --mutate %3 > "%O%\asan_%1.txt" 2>&1
>> "%S%" echo exit %errorlevel%
findstr /c:"decoder " /c:"ERROR: AddressSanitizer" "%O%\asan_%1.txt" >> "%S%"
exit /b 0

:end
endlocal
