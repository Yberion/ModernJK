# Determinism test of the game's player movement (pmove_det.cpp): bg_pmove.cpp and its files built as the game module
# (_GAME) and as cgame (_CGAME), with ModernJK's own compiler settings for the modules (not netcodec's: the point is
# how the shipped game code behaves). Its output is compared between builds and compilers (validate.sh, validate.bat).
set(netcodec_MODERNJK_DIR "${PROJECT_SOURCE_DIR}/../../.." CACHE PATH "ModernJK source tree, for the legacy benchmark")
set(netcodec_root "${netcodec_MODERNJK_DIR}")
set(netcodec_pmove_sources
	"${netcodec_root}/src/codemp/game/bg_g2_utils.cpp"
	"${netcodec_root}/src/codemp/game/bg_misc.cpp"
	"${netcodec_root}/src/codemp/game/bg_panimate.cpp"
	"${netcodec_root}/src/codemp/game/bg_pmove.cpp"
	"${netcodec_root}/src/codemp/game/bg_saber.cpp"
	"${netcodec_root}/src/codemp/game/bg_saberLoad.cpp"
	"${netcodec_root}/src/codemp/game/bg_saga.cpp"
	"${netcodec_root}/src/codemp/game/bg_slidemove.cpp"
	"${netcodec_root}/src/codemp/game/bg_vehicleLoad.cpp"
	"${netcodec_root}/src/codemp/game/bg_weapons.cpp"
	"${netcodec_root}/src/codemp/qcommon/q_shared.cpp"
	"${netcodec_root}/src/shared/qcommon/q_math.cpp"
	"${netcodec_root}/src/shared/qcommon/q_libm.cpp"
	"${netcodec_root}/src/shared/qcommon/q_rand.cpp"
	"${netcodec_root}/src/shared/qcommon/q_string.cpp"
	"${netcodec_root}/src/shared/qcommon/q_color.cpp"
	pmove_det.cpp)
# the math functions' entry point on LLVM libc, as ModernJK's qlibm target builds it (top-level CMakeLists.txt)
set_source_files_properties("${netcodec_root}/src/shared/qcommon/q_libm.cpp" PROPERTIES
	INCLUDE_DIRECTORIES "${netcodec_root}/libs/llvm-libc")

foreach(side game cgame)
	set(t netcodec_pmove_${side})
	add_executable(${t} ${netcodec_pmove_sources})
	# ModernJK's C++20 without compiler extensions (top-level CMakeLists.txt; with MSVC, /permissive-)
	set_target_properties(${t} PROPERTIES CXX_STANDARD 20 CXX_STANDARD_REQUIRED ON CXX_EXTENSIONS OFF)
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
	target_include_directories(${t} PRIVATE "${netcodec_root}/src/codemp" "${netcodec_root}/src/shared" include)
	# ModernJK's settings for the modules (top-level CMakeLists.txt): signed char, no contraction, -O3 in Release
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

