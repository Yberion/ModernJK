# Determinism test of the game's player movement (pmove_det.c): bg_pmove.c and its files built as the game module
# (_GAME) and as cgame (_CGAME), with ModernJK's own compiler settings for the modules (not netcodec's: the point is
# how the shipped game code behaves). Its output is compared between builds and compilers (validate.sh, validate.bat).
set(netcodec_MODERNJK_DIR "${PROJECT_SOURCE_DIR}/../.." CACHE PATH "ModernJK source tree, for the legacy benchmark")
set(netcodec_root "${netcodec_MODERNJK_DIR}")
set(netcodec_pmove_sources
	"${netcodec_root}/codemp/game/bg_g2_utils.c"
	"${netcodec_root}/codemp/game/bg_misc.c"
	"${netcodec_root}/codemp/game/bg_panimate.c"
	"${netcodec_root}/codemp/game/bg_pmove.c"
	"${netcodec_root}/codemp/game/bg_saber.c"
	"${netcodec_root}/codemp/game/bg_saberLoad.c"
	"${netcodec_root}/codemp/game/bg_saga.c"
	"${netcodec_root}/codemp/game/bg_slidemove.c"
	"${netcodec_root}/codemp/game/bg_vehicleLoad.c"
	"${netcodec_root}/codemp/game/bg_weapons.c"
	"${netcodec_root}/codemp/qcommon/q_shared.c"
	"${netcodec_root}/shared/qcommon/q_math.c"
	"${netcodec_root}/shared/qcommon/q_libm.cpp"
	"${netcodec_root}/shared/qcommon/q_rand.c"
	"${netcodec_root}/shared/qcommon/q_string.c"
	"${netcodec_root}/shared/qcommon/q_color.c"
	pmove_det.c)
# the math functions' entry point on LLVM libc, as ModernJK's qlibm target builds it (top-level CMakeLists.txt)
set_source_files_properties("${netcodec_root}/shared/qcommon/q_libm.cpp" PROPERTIES
	INCLUDE_DIRECTORIES "${netcodec_root}/lib/llvm-libc"
	COMPILE_OPTIONS "$<IF:$<CXX_COMPILER_ID:MSVC>,/std:c++20,-std=c++20>")

foreach(side game cgame)
	set(t netcodec_pmove_${side})
	add_executable(${t} ${netcodec_pmove_sources})
	string(TOUPPER "_${side}" def)
	target_compile_definitions(${t} PRIVATE ${def})
	# ModernJK's SharedDefines (top-level CMakeLists.txt)
	target_compile_definitions(${t} PRIVATE $<IF:$<CONFIG:Debug>,_DEBUG,FINAL_BUILD>)
	if(NOT WIN32)
		target_compile_definitions(${t} PRIVATE "ARCH_STRING=\"${CMAKE_SYSTEM_PROCESSOR}\"")
	endif()
	if(MSVC)
		target_compile_definitions(${t} PRIVATE NOMINMAX _CRT_NONSTDC_NO_DEPRECATE)
	endif()
	target_include_directories(${t} PRIVATE "${netcodec_root}/codemp" "${netcodec_root}/shared" include)
	# ModernJK's settings for C (top-level CMakeLists.txt): signed char, no contraction, -O3 in Release
	if(MSVC)
		target_compile_options(${t} PRIVATE /W0)   # /W0, not /w: CMake maps /W0 to the project's warning level (/w comes after MSBuild's default /W1: D9025)
		target_compile_definitions(${t} PRIVATE _CRT_SECURE_NO_WARNINGS)
	else()
		target_compile_options(${t} PRIVATE -w -fsigned-char -ffp-contract=off $<$<CONFIG:Release>:-O3>)
		target_link_libraries(${t} PRIVATE m)
	endif()
	if(MINGW)
		target_link_options(${t} PRIVATE -static)
	endif()
	add_test(NAME netcodec.pmove_${side} COMMAND ${t} 20000)
endforeach()

